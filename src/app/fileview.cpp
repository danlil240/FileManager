#include "fileview.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QListView>
#include <QMimeData>
#include <QTreeView>
#include <QVBoxLayout>

FileView::FileView(Mode mode, QWidget* parent) : QWidget(parent) {
  if (mode == Mode::Details) {
    auto* tree = new QTreeView(this);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->setAllColumnsShowFocus(true);
    tree->header()->setStretchLastSection(true);
    tree->setSortingEnabled(true);
    m_view = tree;
  } else {
    auto* list = new QListView(this);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setIconSize(QSize(64, 64));
    list->setUniformItemSizes(true);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view = list;
  }

  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->setAcceptDrops(true);
  m_view->viewport()->setAcceptDrops(true);
  m_view->setDragEnabled(true);
  m_view->setDropIndicatorShown(true);
  m_view->setDefaultDropAction(Qt::CopyAction);
  m_view->setDragDropMode(QAbstractItemView::DragDrop);
  m_view->viewport()->installEventFilter(this);

  connect(m_view, &QAbstractItemView::doubleClicked, this, &FileView::activated);
  connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    emit contextMenuRequested(m_view->viewport()->mapToGlobal(pos), m_view->indexAt(pos));
  });

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_view);
}

void FileView::setModel(QAbstractItemModel* model) {
  m_view->setModel(model);
}

void FileView::setRootIndex(const QModelIndex& index) {
  m_view->setRootIndex(index);
}

QList<QModelIndex> FileView::selectedIndexes() const {
  if (!m_view->selectionModel()) {
    return {};
  }
  return m_view->selectionModel()->selectedRows(0);
}

QModelIndex FileView::indexAt(const QPoint& pos) const {
  return m_view->indexAt(pos);
}

bool FileView::eventFilter(QObject* obj, QEvent* event) {
  if (obj != m_view->viewport()) {
    return QWidget::eventFilter(obj, event);
  }

  switch (event->type()) {
    case QEvent::MouseButtonPress:
      emit viewFocused();
      break;
    case QEvent::DragEnter: {
      auto* dragEvent = static_cast<QDragEnterEvent*>(event);
      if (dragEvent->mimeData()->hasUrls()) {
        dragEvent->acceptProposedAction();
        return true;
      }
      break;
    }
    case QEvent::DragMove: {
      auto* dragMoveEvent = static_cast<QDragMoveEvent*>(event);
      if (dragMoveEvent->mimeData()->hasUrls()) {
        dragMoveEvent->acceptProposedAction();
        return true;
      }
      break;
    }
    case QEvent::Drop: {
      auto* dropEvent = static_cast<QDropEvent*>(event);
      if (dropEvent->mimeData()->hasUrls()) {
        const QList<QUrl> urls = dropEvent->mimeData()->urls();
        const QModelIndex destIndex = m_view->indexAt(dropEvent->position().toPoint());
        emit dropUrlsRequested(urls, destIndex, dropEvent->dropAction());
        dropEvent->acceptProposedAction();
        return true;
      }
      break;
    }
    default:
      break;
  }

  return QWidget::eventFilter(obj, event);
}
