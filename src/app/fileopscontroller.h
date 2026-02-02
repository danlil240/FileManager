#pragma once

#include <QObject>
#include <QStringList>

class FileOpsController : public QObject {
  Q_OBJECT
public:
  explicit FileOpsController(QObject* parent = nullptr);

  void copy(const QStringList& sources, const QString& destinationDir);
  void move(const QStringList& sources, const QString& destinationDir);
  void remove(const QStringList& sources);
  void rename(const QString& sourcePath, const QString& newName);
  void createFolder(const QString& parentDir, const QString& name);
  void createFile(const QString& parentDir, const QString& name);
  void cancelCurrentOperation();

signals:
  void operationStarted(const QString& label, qint64 totalBytes);
  void operationProgress(qint64 doneBytes, qint64 totalBytes, const QString& currentItem);
  void operationFinished(bool success, const QString& error);

private:
  QObject* m_currentWorker = nullptr;
};
