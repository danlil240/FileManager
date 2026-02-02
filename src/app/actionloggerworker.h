#pragma once

#include <QObject>
#include <QFile>
#include <QMutex>

class ActionLoggerWorker : public QObject {
  Q_OBJECT
public:
  explicit ActionLoggerWorker(const QString& logPath, QObject* parent = nullptr);

public slots:
  void writeLine(const QString& line);

private:
  QFile m_file;
  QMutex m_writeMutex;
};
