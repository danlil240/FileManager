#pragma once

#include <QStandardItemModel>

class PlacesModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit PlacesModel(QObject* parent = nullptr);

  enum Roles { PathRole = Qt::UserRole + 1 };

  void refresh();

private:
  void addPlace(const QString& name, const QString& path, const QString& iconName);
};
