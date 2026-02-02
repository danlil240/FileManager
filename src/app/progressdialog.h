#pragma once

#include <QProgressDialog>

class ProgressDialog : public QProgressDialog {
  Q_OBJECT
public:
  explicit ProgressDialog(QWidget* parent = nullptr);

public slots:
  void start(const QString& label, qint64 totalBytes);
  void updateProgress(qint64 doneBytes, qint64 totalBytes, const QString& currentItem);
};
