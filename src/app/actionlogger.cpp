#include "actionlogger.h"
#include "actionloggerworker.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ActionLogger* ActionLogger::instance() {
  static ActionLogger logger;
  return &logger;
}

ActionLogger::ActionLogger() {
  const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  const QString logPath = cacheDir + "/actions.log";

  m_worker = new ActionLoggerWorker(logPath);
  m_worker->moveToThread(&m_thread);
  m_thread.start();
}

ActionLogger::~ActionLogger() {
  m_thread.quit();
  m_thread.wait();
  delete m_worker;
  m_worker = nullptr;
}

void ActionLogger::logAction(const QString& action,
                             const QStringList& sources,
                             const QString& destination,
                             bool success,
                             const QString& error) {
  QJsonObject obj;
  obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  obj["action"] = action;

  QJsonArray srcArray;
  for (const QString& src : sources) {
    srcArray.append(src);
  }
  obj["sources"] = srcArray;

  if (!destination.isEmpty()) {
    obj["destination"] = destination;
  }
  obj["success"] = success;
  if (!error.isEmpty()) {
    obj["error"] = error;
  }

  const QString line = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  QMetaObject::invokeMethod(m_worker, "writeLine", Qt::QueuedConnection, Q_ARG(QString, line));
}

void ActionLogger::logTerminalSync(const QString& path) {
  logAction("terminal_sync", {path}, {}, true, {});
}
