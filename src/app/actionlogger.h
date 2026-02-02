#pragma once

#include <QObject>
#include <QThread>
#include <QStringList>

class ActionLoggerWorker;

class ActionLogger : public QObject {
  Q_OBJECT
public:
  static ActionLogger* instance();

  void logAction(const QString& action,
                 const QStringList& sources = {},
                 const QString& destination = {},
                 bool success = true,
                 const QString& error = {});

  void logTerminalSync(const QString& path);

private:
  ActionLogger();
  ~ActionLogger() override;

  QThread m_thread;
  ActionLoggerWorker* m_worker = nullptr;
};
