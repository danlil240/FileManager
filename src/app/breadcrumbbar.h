#pragma once

#include <QWidget>

class BreadcrumbBar : public QWidget {
  Q_OBJECT
public:
  explicit BreadcrumbBar(QWidget* parent = nullptr);

  void setPath(const QString& path);

signals:
  void pathClicked(const QString& path);

private:
  void rebuild();

  QString m_path;
};
