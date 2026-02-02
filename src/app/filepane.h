#pragma once

#include <QWidget>
#include <QSortFilterProxyModel>

#include "fileview.h"

class BreadcrumbBar;
class QFileSystemModel;
class QLineEdit;
class QStackedWidget;

class FilePane : public QWidget {
  Q_OBJECT
public:
  explicit FilePane(QWidget* parent = nullptr);

  QString currentPath() const;
  void navigateTo(const QString& path, bool addHistory = true);
  void back();
  void forward();
  void up();

  void setViewMode(FileView::Mode mode);
  FileView::Mode viewMode() const;

  QList<QUrl> selectedUrls() const;
  QStringList selectedPaths() const;
  bool hasSelection() const;

  void setActive(bool active);
  FileView* activeView() const;

signals:
  void currentPathChanged(const QString& path);
  void openFileRequested(const QString& path);
  void contextMenuRequested(const QPoint& globalPos, const QList<QUrl>& urls);
  void dropRequested(const QList<QUrl>& urls, const QString& destDir, Qt::DropAction action);
  void paneFocused(FilePane* pane);

private:
  void handleActivated(const QModelIndex& index);
  void handleDrop(const QList<QUrl>& urls, const QModelIndex& destIndex, Qt::DropAction action);
  QString destPathForIndex(const QModelIndex& index) const;

  QFileSystemModel* m_model = nullptr;
  QSortFilterProxyModel* m_proxyModel = nullptr;
  FileView* m_detailsView = nullptr;
  FileView* m_iconView = nullptr;
  FileView* m_activeView = nullptr;
  QStackedWidget* m_viewStack = nullptr;
  BreadcrumbBar* m_breadcrumb = nullptr;
  QLineEdit* m_filterEdit = nullptr;

  QString m_currentPath;
  QStringList m_backStack;
  QStringList m_forwardStack;
};
