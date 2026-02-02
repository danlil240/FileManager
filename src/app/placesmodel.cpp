#include "placesmodel.h"

#include <QDir>
#include <QIcon>
#include <QStandardPaths>

PlacesModel::PlacesModel(QObject* parent) : QStandardItemModel(parent) {
  refresh();
}

void PlacesModel::refresh() {
  clear();
  addPlace("Home", QDir::homePath(), "user-home");

  const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  if (!desktop.isEmpty()) {
    addPlace("Desktop", desktop, "user-desktop");
  }

  const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  if (!downloads.isEmpty()) {
    addPlace("Downloads", downloads, "folder-download");
  }

  addPlace("Root", "/", "drive-harddisk");
}

void PlacesModel::addPlace(const QString& name, const QString& path, const QString& iconName) {
  auto* item = new QStandardItem(QIcon::fromTheme(iconName), name);
  item->setData(path, PathRole);
  item->setEditable(false);
  appendRow(item);
}
