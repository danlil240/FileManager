#include "breadcrumbbar.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

BreadcrumbBar::BreadcrumbBar(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
}

void BreadcrumbBar::setPath(const QString& path) {
  if (m_path == path) {
    return;
  }
  m_path = QDir::cleanPath(path);
  rebuild();
}

void BreadcrumbBar::rebuild() {
  auto* layout = qobject_cast<QHBoxLayout*>(this->layout());
  while (QLayoutItem* item = layout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  const QString homePath = QDir::homePath();
  QString current;

  auto addSegment = [this, layout](const QString& label, const QString& path, bool addSeparator) {
    auto* button = new QToolButton(this);
    button->setText(label);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setProperty("path", path);

    connect(button, &QToolButton::clicked, this, [this, button]() {
      emit pathClicked(button->property("path").toString());
    });

    layout->addWidget(button);
    if (addSeparator) {
      auto* sep = new QLabel("/", this);
      sep->setStyleSheet("color: #808080;");
      layout->addWidget(sep);
    }
  };

  if (m_path == homePath || m_path.startsWith(homePath + "/")) {
    current = homePath;
    QString remainder = m_path.mid(homePath.length());
    QStringList parts = remainder.split("/", Qt::SkipEmptyParts);
    addSegment("~", homePath, !parts.isEmpty());

    for (int i = 0; i < parts.size(); ++i) {
      current = QDir(current).filePath(parts.at(i));
      addSegment(parts.at(i), current, i < parts.size() - 1);
    }
  } else if (m_path.startsWith("/")) {
    current = "/";
    QStringList parts = m_path.split("/", Qt::SkipEmptyParts);
    addSegment("/", "/", !parts.isEmpty());

    for (int i = 0; i < parts.size(); ++i) {
      current = QDir(current).filePath(parts.at(i));
      addSegment(parts.at(i), current, i < parts.size() - 1);
    }
  } else {
    current = QDir::currentPath();
    QStringList parts = m_path.split("/", Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
      current = QDir(current).filePath(parts.at(i));
      addSegment(parts.at(i), current, i < parts.size() - 1);
    }
  }
  layout->addStretch();
}
