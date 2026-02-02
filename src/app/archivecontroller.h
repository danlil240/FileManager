#pragma once

#include <QObject>
#include <QStringList>

class QProcess;

class ArchiveController : public QObject {
  Q_OBJECT
public:
  explicit ArchiveController(QObject* parent = nullptr);

  void compressZip(const QStringList& sources, const QString& destZip, const QString& workingDir);
  void extractArchive(const QString& archivePath, const QString& destDir);
  void cancel();

signals:
  void taskStarted(const QString& label);
  void taskOutput(const QString& text);
  void taskFinished(bool success, const QString& error);

private:
  void startProcess(QProcess* process, const QString& label, const QString& program,
                    const QStringList& args, const QString& workingDir);

  QProcess* m_process = nullptr;
};
