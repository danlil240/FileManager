#include "app/mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("dolphin-lite");
  app.setApplicationDisplayName("Dolphin Lite");
  app.setOrganizationName("dolphin-lite");

  app.setWindowIcon(QIcon(":/icons/app.svg"));
  QApplication::setStyle("Fusion");

  QPalette palette;
  palette.setColor(QPalette::Window, QColor(247, 248, 250));
  palette.setColor(QPalette::WindowText, QColor(30, 30, 30));
  palette.setColor(QPalette::Base, QColor(255, 255, 255));
  palette.setColor(QPalette::AlternateBase, QColor(240, 242, 245));
  palette.setColor(QPalette::Button, QColor(235, 238, 242));
  palette.setColor(QPalette::ButtonText, QColor(30, 30, 30));
  palette.setColor(QPalette::Highlight, QColor(62, 143, 224));
  palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  app.setPalette(palette);

  MainWindow window;
  window.show();

  return app.exec();
}
