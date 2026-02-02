#include "fileopscontroller.h"
#include "actionlogger.h"

#include <QAtomicInteger>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QThread>

namespace {
constexpr qint64 kChunkSize = 4 * 1024 * 1024;

struct Task {
  enum class Type { Copy, Move, Delete, Rename, Mkdir, CreateFile };
  Type type;
  QStringList sources;
  QString destinationDir;
  QString newName;
};

class FileOpWorker : public QObject {
  Q_OBJECT
public:
  explicit FileOpWorker(const Task& task) : m_task(task) {}

public slots:
  void cancel() { m_cancel.storeRelease(true); }

signals:
  void started(const QString& label, qint64 totalBytes);
  void progress(qint64 doneBytes, qint64 totalBytes, const QString& currentItem);
  void finished(bool success, const QString& error);

public slots:
  void process() {
    bool ok = false;
    QString error;

    switch (m_task.type) {
      case Task::Type::Copy:
        ok = copyEntries(error);
        break;
      case Task::Type::Move:
        ok = moveEntries(error);
        break;
      case Task::Type::Delete:
        ok = deleteEntries(error);
        break;
      case Task::Type::Rename:
        ok = renameEntry(error);
        break;
      case Task::Type::Mkdir:
        ok = createFolder(error);
        break;
      case Task::Type::CreateFile:
        ok = createFile(error);
        break;
    }

    emit finished(ok, error);
  }

private:
  Task m_task;
  QAtomicInteger<bool> m_cancel = false;
  qint64 m_totalBytes = 0;
  qint64 m_doneBytes = 0;

  bool isCancelled() const { return m_cancel.loadAcquire(); }

  qint64 computeTotalBytes(const QStringList& sources) {
    qint64 total = 0;
    for (const QString& src : sources) {
      QFileInfo info(src);
      if (!info.exists()) {
        continue;
      }
      if (info.isFile() || info.isSymLink()) {
        total += info.size();
        continue;
      }
      if (info.isDir()) {
        QDirIterator it(src, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
          it.next();
          total += it.fileInfo().size();
        }
      }
    }
    return total;
  }

  bool ensureDir(const QString& path, QString& error) {
    QDir dir(path);
    if (dir.exists()) {
      return true;
    }
    if (!dir.mkpath(".")) {
      error = QString("Failed to create directory: %1").arg(path);
      return false;
    }
    return true;
  }

  bool copyFileStream(const QString& src, const QString& dst, QString& error) {
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
      error = QString("Failed to open source file: %1").arg(src);
      return false;
    }

    QFileInfo dstInfo(dst);
    if (!ensureDir(dstInfo.absolutePath(), error)) {
      return false;
    }

    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      error = QString("Failed to open destination file: %1").arg(dst);
      return false;
    }

    while (!in.atEnd()) {
      if (isCancelled()) {
        error = "Operation cancelled";
        return false;
      }
      QByteArray chunk = in.read(kChunkSize);
      if (chunk.isEmpty() && !in.atEnd()) {
        error = QString("Failed to read from file: %1").arg(src);
        return false;
      }
      if (out.write(chunk) != chunk.size()) {
        error = QString("Failed to write to file: %1").arg(dst);
        return false;
      }
      m_doneBytes += chunk.size();
      emit progress(m_doneBytes, m_totalBytes, src);
    }

    out.setPermissions(in.permissions());
    return true;
  }

  bool copyEntry(const QString& src, const QString& dst, QString& error) {
    QFileInfo info(src);
    if (info.isFile() || info.isSymLink()) {
      return copyFileStream(src, dst, error);
    }
    if (!info.isDir()) {
      error = QString("Unsupported entry: %1").arg(src);
      return false;
    }

    if (!ensureDir(dst, error)) {
      return false;
    }

    QDirIterator it(src, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      if (isCancelled()) {
        error = "Operation cancelled";
        return false;
      }
      it.next();
      const QString relPath = QDir(src).relativeFilePath(it.filePath());
      const QString dstPath = QDir(dst).filePath(relPath);
      if (it.fileInfo().isDir()) {
        if (!ensureDir(dstPath, error)) {
          return false;
        }
      } else {
        if (!copyFileStream(it.filePath(), dstPath, error)) {
          return false;
        }
      }
    }
    return true;
  }

  bool copyEntries(QString& error) {
    if (m_task.sources.isEmpty()) {
      error = "No sources specified";
      return false;
    }

    m_totalBytes = computeTotalBytes(m_task.sources);
    emit started("Copying files...", m_totalBytes);

    for (const QString& src : m_task.sources) {
      if (isCancelled()) {
        error = "Operation cancelled";
        return false;
      }
      const QString dst = QDir(m_task.destinationDir).filePath(QFileInfo(src).fileName());
      if (!copyEntry(src, dst, error)) {
        return false;
      }
    }

    return true;
  }

  bool moveEntries(QString& error) {
    if (m_task.sources.isEmpty()) {
      error = "No sources specified";
      return false;
    }

    m_totalBytes = computeTotalBytes(m_task.sources);
    emit started("Moving files...", m_totalBytes);

    for (const QString& src : m_task.sources) {
      if (isCancelled()) {
        error = "Operation cancelled";
        return false;
      }
      const QString dst = QDir(m_task.destinationDir).filePath(QFileInfo(src).fileName());
      QFileInfo info(src);

      const QStorageInfo srcStorage(info.absolutePath());
      const QStorageInfo dstStorage(m_task.destinationDir);
      const bool sameDevice = srcStorage.isValid() && dstStorage.isValid() &&
                              srcStorage.device() == dstStorage.device();

      if (sameDevice && QFile::rename(src, dst)) {
        continue;
      }

      if (!copyEntry(src, dst, error)) {
        return false;
      }

      if (!removePath(src, error)) {
        return false;
      }
    }

    return true;
  }

  bool removePath(const QString& path, QString& error) {
    QFileInfo info(path);
    if (info.isDir()) {
      QDir dir(path);
      if (!dir.removeRecursively()) {
        error = QString("Failed to remove directory: %1").arg(path);
        return false;
      }
      return true;
    }
    if (!QFile::remove(path)) {
      error = QString("Failed to remove file: %1").arg(path);
      return false;
    }
    return true;
  }

  bool deleteEntries(QString& error) {
    emit started("Deleting files...", 0);
    for (const QString& src : m_task.sources) {
      if (isCancelled()) {
        error = "Operation cancelled";
        return false;
      }
      if (!removePath(src, error)) {
        return false;
      }
    }
    return true;
  }

  bool renameEntry(QString& error) {
    if (m_task.sources.size() != 1) {
      error = "Rename requires exactly one source";
      return false;
    }
    QFileInfo info(m_task.sources.first());
    const QString dst = QDir(info.absolutePath()).filePath(m_task.newName);
    emit started("Renaming...", 0);
    if (!QFile::rename(info.absoluteFilePath(), dst)) {
      error = QString("Failed to rename %1").arg(info.fileName());
      return false;
    }
    return true;
  }

  bool createFolder(QString& error) {
    if (m_task.sources.size() != 1) {
      error = "Create folder requires one parent directory";
      return false;
    }
    emit started("Creating folder...", 0);
    const QString parentDir = m_task.sources.first();
    QDir dir(parentDir);
    if (!dir.mkdir(m_task.newName)) {
      error = QString("Failed to create folder: %1").arg(m_task.newName);
      return false;
    }
    return true;
  }

  bool createFile(QString& error) {
    if (m_task.sources.size() != 1) {
      error = "Create file requires one parent directory";
      return false;
    }
    emit started("Creating file...", 0);
    const QString parentDir = m_task.sources.first();
    const QString path = QDir(parentDir).filePath(m_task.newName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      error = QString("Failed to create file: %1").arg(path);
      return false;
    }
    return true;
  }
};

}  // namespace

FileOpsController::FileOpsController(QObject* parent) : QObject(parent) {}

void FileOpsController::copy(const QStringList& sources, const QString& destinationDir) {
  Task task{Task::Type::Copy, sources, destinationDir, {}};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::progress, this, &FileOpsController::operationProgress);
  connect(worker, &FileOpWorker::finished, this, [this, sources, destinationDir, worker, thread](bool success, const QString& error) {
    ActionLogger::instance()->logAction("copy", sources, destinationDir, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::move(const QStringList& sources, const QString& destinationDir) {
  Task task{Task::Type::Move, sources, destinationDir, {}};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::progress, this, &FileOpsController::operationProgress);
  connect(worker, &FileOpWorker::finished, this, [this, sources, destinationDir, worker, thread](bool success, const QString& error) {
    ActionLogger::instance()->logAction("move", sources, destinationDir, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::remove(const QStringList& sources) {
  Task task{Task::Type::Delete, sources, {}, {}};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::progress, this, &FileOpsController::operationProgress);
  connect(worker, &FileOpWorker::finished, this, [this, sources, worker, thread](bool success, const QString& error) {
    ActionLogger::instance()->logAction("delete", sources, {}, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::rename(const QString& sourcePath, const QString& newName) {
  Task task{Task::Type::Rename, {sourcePath}, {}, newName};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::progress, this, &FileOpsController::operationProgress);
  connect(worker, &FileOpWorker::finished, this, [this, sourcePath, newName, worker, thread](bool success, const QString& error) {
    const QString destination = QFileInfo(sourcePath).absolutePath() + "/" + newName;
    ActionLogger::instance()->logAction("rename", {sourcePath}, destination, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::createFolder(const QString& parentDir, const QString& name) {
  Task task{Task::Type::Mkdir, {parentDir}, {}, name};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::finished, this, [this, parentDir, name, worker, thread](bool success, const QString& error) {
    const QString destination = QDir(parentDir).filePath(name);
    ActionLogger::instance()->logAction("mkdir", {parentDir}, destination, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::createFile(const QString& parentDir, const QString& name) {
  Task task{Task::Type::CreateFile, {parentDir}, {}, name};

  auto* worker = new FileOpWorker(task);
  auto* thread = new QThread();
  worker->moveToThread(thread);
  m_currentWorker = worker;

  connect(thread, &QThread::started, worker, &FileOpWorker::process);
  connect(worker, &FileOpWorker::started, this, &FileOpsController::operationStarted);
  connect(worker, &FileOpWorker::finished, this, [this, parentDir, name, worker, thread](bool success, const QString& error) {
    const QString destination = QDir(parentDir).filePath(name);
    ActionLogger::instance()->logAction("create_file", {parentDir}, destination, success, error);
    emit operationFinished(success, error);
    worker->deleteLater();
    thread->quit();
    if (m_currentWorker == worker) {
      m_currentWorker = nullptr;
    }
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  thread->start();
}

void FileOpsController::cancelCurrentOperation() {
  if (m_currentWorker) {
    QMetaObject::invokeMethod(m_currentWorker, "cancel", Qt::QueuedConnection);
  }
}

#include "fileopscontroller.moc"
