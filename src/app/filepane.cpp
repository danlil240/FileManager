#include "filepane.h"

#include "breadcrumbbar.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QUrl>

FilePane::FilePane(QWidget* parent) : QWidget(parent) {
  m_model = new QFileSystemModel(this);
  m_model->setRootPath("/");
  m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden | QDir::System);
  m_model->setReadOnly(true);

  m_proxyModel = new QSortFilterProxyModel(this);
  m_proxyModel->setSourceModel(m_model);
  m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_proxyModel->setFilterKeyColumn(0);

  m_breadcrumb = new BreadcrumbBar(this);
  connect(m_breadcrumb, &BreadcrumbBar::pathClicked, this, [this](const QString& path) {
    navigateTo(path);
  });

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText("Filter current folder...");
  connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
    m_proxyModel->setFilterFixedString(text);
  });

  m_detailsView = new FileView(FileView::Mode::Details, this);
  m_iconView = new FileView(FileView::Mode::Icons, this);

  m_detailsView->setModel(m_proxyModel);
  m_iconView->setModel(m_proxyModel);

  connect(m_detailsView, &FileView::activated, this, &FilePane::handleActivated);
  connect(m_iconView, &FileView::activated, this, &FilePane::handleActivated);

  connect(m_detailsView, &FileView::contextMenuRequested, this, [this](const QPoint& pos, const QModelIndex&) {
    emit contextMenuRequested(pos, selectedUrls());
  });
  connect(m_iconView, &FileView::contextMenuRequested, this, [this](const QPoint& pos, const QModelIndex&) {
    emit contextMenuRequested(pos, selectedUrls());
  });

  connect(m_detailsView, &FileView::dropUrlsRequested, this, &FilePane::handleDrop);
  connect(m_iconView, &FileView::dropUrlsRequested, this, &FilePane::handleDrop);

  connect(m_detailsView, &FileView::viewFocused, this, [this]() { emit paneFocused(this); });
  connect(m_iconView, &FileView::viewFocused, this, [this]() { emit paneFocused(this); });

  m_viewStack = new QStackedWidget(this);
  m_viewStack->addWidget(m_detailsView);
  m_viewStack->addWidget(m_iconView);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(m_breadcrumb);
  layout->addWidget(m_filterEdit);
  layout->addWidget(m_viewStack, 1);

  m_activeView = m_detailsView;
  m_viewStack->setCurrentWidget(m_detailsView);

  navigateTo(QDir::homePath(), false);
}

QString FilePane::currentPath() const {
  return m_currentPath;
}

void FilePane::navigateTo(const QString& path, bool addHistory) {
  const QString cleanPath = QDir::cleanPath(path);
  QModelIndex sourceIndex = m_model->index(cleanPath);
  if (!sourceIndex.isValid()) {
    return;
  }

  if (addHistory && !m_currentPath.isEmpty()) {
    m_backStack.append(m_currentPath);
    m_forwardStack.clear();
  }

  m_currentPath = cleanPath;
  QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
  m_detailsView->setRootIndex(proxyIndex);
  m_iconView->setRootIndex(proxyIndex);
  m_breadcrumb->setPath(cleanPath);
  emit currentPathChanged(cleanPath);
}

void FilePane::back() {
  if (m_backStack.isEmpty()) {
    return;
  }
  const QString path = m_backStack.takeLast();
  if (!m_currentPath.isEmpty()) {
    m_forwardStack.append(m_currentPath);
  }
  navigateTo(path, false);
}

void FilePane::forward() {
  if (m_forwardStack.isEmpty()) {
    return;
  }
  const QString path = m_forwardStack.takeLast();
  if (!m_currentPath.isEmpty()) {
    m_backStack.append(m_currentPath);
  }
  navigateTo(path, false);
}

void FilePane::up() {
  QDir dir(m_currentPath);
  if (dir.cdUp()) {
    navigateTo(dir.absolutePath());
  }
}

void FilePane::setViewMode(FileView::Mode mode) {
  if (mode == FileView::Mode::Details) {
    m_viewStack->setCurrentWidget(m_detailsView);
    m_activeView = m_detailsView;
  } else {
    m_viewStack->setCurrentWidget(m_iconView);
    m_activeView = m_iconView;
  }
}

FileView::Mode FilePane::viewMode() const {
  return (m_activeView == m_iconView) ? FileView::Mode::Icons : FileView::Mode::Details;
}

QList<QUrl> FilePane::selectedUrls() const {
  QList<QUrl> urls;
  const QList<QModelIndex> indices = m_activeView->selectedIndexes();
  for (const QModelIndex& proxyIndex : indices) {
    const QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
    const QString path = m_model->filePath(srcIndex);
    urls.append(QUrl::fromLocalFile(path));
  }
  return urls;
}

QStringList FilePane::selectedPaths() const {
  QStringList paths;
  for (const QUrl& url : selectedUrls()) {
    paths.append(url.toLocalFile());
  }
  return paths;
}

bool FilePane::hasSelection() const {
  return !selectedUrls().isEmpty();
}

void FilePane::setActive(bool active) {
  if (active) {
    setStyleSheet("QWidget { border: 1px solid #3e8fe0; border-radius: 4px; }");
  } else {
    setStyleSheet("");
  }
}

FileView* FilePane::activeView() const {
  return m_activeView;
}

void FilePane::handleActivated(const QModelIndex& index) {
  const QModelIndex srcIndex = m_proxyModel->mapToSource(index);
  const QFileInfo info = m_model->fileInfo(srcIndex);
  if (info.isDir()) {
    navigateTo(info.absoluteFilePath());
  } else {
    emit openFileRequested(info.absoluteFilePath());
  }
}

void FilePane::handleDrop(const QList<QUrl>& urls, const QModelIndex& destIndex, Qt::DropAction action) {
  const QString destDir = destPathForIndex(destIndex);
  emit dropRequested(urls, destDir, action);
}

QString FilePane::destPathForIndex(const QModelIndex& index) const {
  if (!index.isValid()) {
    return m_currentPath;
  }
  const QModelIndex srcIndex = m_proxyModel->mapToSource(index);
  QFileInfo info = m_model->fileInfo(srcIndex);
  if (info.isDir()) {
    return info.absoluteFilePath();
  }
  return info.absolutePath();
}
