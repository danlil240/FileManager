#include "actionloggerworker.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>

ActionLoggerWorker::ActionLoggerWorker(const QString& logPath, QObject* parent)
    : QObject(parent), m_file(logPath) {
  QFileInfo info(logPath);
  QDir().mkpath(info.absolutePath());
  m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void ActionLoggerWorker::writeLine(const QString& line) {
  QMutexLocker locker(&m_writeMutex);
  if (!m_file.isOpen()) {
    return;
  }
  QTextStream stream(&m_file);
  stream << line << "\n";
  stream.flush();
}
