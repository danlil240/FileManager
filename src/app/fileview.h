#pragma once

#include <QWidget>
#include <QAbstractItemView>
#include <QModelIndex>
#include <QUrl>

class FileView : public QWidget {
  Q_OBJECT
public:
  enum class Mode { Details, Icons };

  explicit FileView(Mode mode, QWidget* parent = nullptr);

  void setModel(QAbstractItemModel* model);
  void setRootIndex(const QModelIndex& index);
  QList<QModelIndex> selectedIndexes() const;
  QModelIndex indexAt(const QPoint& pos) const;
  QAbstractItemView* view() const { return m_view; }

signals:
  void activated(const QModelIndex& index);
  void contextMenuRequested(const QPoint& globalPos, const QModelIndex& index);
  void dropUrlsRequested(const QList<QUrl>& urls, const QModelIndex& destIndex, Qt::DropAction action);
  void viewFocused();

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

private:
  QAbstractItemView* m_view = nullptr;
};
