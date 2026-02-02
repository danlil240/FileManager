#include "progressdialog.h"

ProgressDialog::ProgressDialog(QWidget* parent)
    : QProgressDialog(parent) {
  setMinimumDuration(200);
  setAutoClose(true);
  setAutoReset(true);
  setWindowTitle("File Operation");
  setCancelButtonText("Cancel");
}

void ProgressDialog::start(const QString& label, qint64 totalBytes) {
  setLabelText(label);
  if (totalBytes <= 0) {
    setRange(0, 0);
  } else {
    setRange(0, 1000);
  }
  show();
}

void ProgressDialog::updateProgress(qint64 doneBytes, qint64 totalBytes, const QString& currentItem) {
  if (totalBytes <= 0) {
    setRange(0, 0);
    setLabelText(currentItem);
    return;
  }

  const int value = static_cast<int>((doneBytes * 1000) / totalBytes);
  setRange(0, 1000);
  setValue(value);
  setLabelText(currentItem);
}
