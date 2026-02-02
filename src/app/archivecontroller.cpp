#include "archivecontroller.h"
#include "actionlogger.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

ArchiveController::ArchiveController(QObject* parent) : QObject(parent) {}

void ArchiveController::compressZip(const QStringList& sources, const QString& destZip, const QString& workingDir) {
  if (sources.isEmpty()) {
    emit taskFinished(false, "No sources to compress");
    return;
  }

  QStringList args;
  args << "-r" << destZip;
  for (const QString& src : sources) {
    args << QFileInfo(src).fileName();
  }

  auto* process = new QProcess(this);
  startProcess(process, "Compressing to zip...", "zip", args, workingDir);

  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this, sources, destZip, process](int exitCode, QProcess::ExitStatus status) {
            const bool success = (status == QProcess::NormalExit && exitCode == 0);
            const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
            ActionLogger::instance()->logAction("compress", sources, destZip, success, error);
            emit taskFinished(success, error);
            process->deleteLater();
          });
}

void ArchiveController::extractArchive(const QString& archivePath, const QString& destDir) {
  QFileInfo info(archivePath);
  if (!info.exists()) {
    emit taskFinished(false, "Archive does not exist");
    return;
  }

  QDir().mkpath(destDir);

  const QString lower = info.fileName().toLower();
  QString program;
  QStringList args;

  if (lower.endsWith(".zip")) {
    program = "unzip";
    args << "-o" << archivePath << "-d" << destDir;
  } else if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz") || lower.endsWith(".tar.xz") || lower.endsWith(".tar")) {
    program = "tar";
    args << "-xf" << archivePath << "-C" << destDir;
  } else {
    emit taskFinished(false, "Unsupported archive format");
    return;
  }

  auto* process = new QProcess(this);
  startProcess(process, "Extracting archive...", program, args, destDir);

  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this, archivePath, destDir, process](int exitCode, QProcess::ExitStatus status) {
            const bool success = (status == QProcess::NormalExit && exitCode == 0);
            const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
            ActionLogger::instance()->logAction("extract", {archivePath}, destDir, success, error);
            emit taskFinished(success, error);
            process->deleteLater();
          });
}

void ArchiveController::cancel() {
  if (m_process) {
    m_process->kill();
  }
}

void ArchiveController::startProcess(QProcess* process, const QString& label, const QString& program,
                                     const QStringList& args, const QString& workingDir) {
  if (m_process) {
    m_process->kill();
    m_process = nullptr;
  }

  m_process = process;
  process->setProgram(program);
  process->setArguments(args);
  process->setWorkingDirectory(workingDir);

  connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
    emit taskOutput(QString::fromUtf8(process->readAllStandardOutput()));
  });
  connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
    emit taskOutput(QString::fromUtf8(process->readAllStandardError()));
  });
  connect(process, &QProcess::started, this, [this, label]() { emit taskStarted(label); });
  connect(process, &QProcess::finished, this, [this]() { m_process = nullptr; });

  process->start();
}
