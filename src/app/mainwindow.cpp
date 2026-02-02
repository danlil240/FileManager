#include "mainwindow.h"

#include "actionlogger.h"
#include "archivecontroller.h"
#include "fileopscontroller.h"
#include "filepane.h"
#include "placesmodel.h"
#include "progressdialog.h"
#include "terminalwidget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Dolphin Lite");
  resize(1280, 800);

  m_fileOps = new FileOpsController(this);
  m_archive = new ArchiveController(this);
  m_progress = new ProgressDialog(this);

  connect(m_progress, &QProgressDialog::canceled, m_fileOps, &FileOpsController::cancelCurrentOperation);
  connect(m_progress, &QProgressDialog::canceled, m_archive, &ArchiveController::cancel);

  connect(m_fileOps, &FileOpsController::operationStarted, m_progress, &ProgressDialog::start);
  connect(m_fileOps, &FileOpsController::operationProgress, m_progress, &ProgressDialog::updateProgress);
  connect(m_fileOps, &FileOpsController::operationFinished, this, [this](bool success, const QString& error) {
    if (!success && !error.isEmpty()) {
      showError("File Operation Failed", error);
    }
    if (m_clipboardPendingClear && success) {
      m_clipboardPaths.clear();
      m_clipboardCut = false;
      m_clipboardPendingClear = false;
    }
  });

  connect(m_archive, &ArchiveController::taskStarted, m_progress, [this](const QString& label) {
    m_progress->start(label, 0);
  });
  connect(m_archive, &ArchiveController::taskOutput, this, [this](const QString& text) {
    statusBar()->showMessage(text.trimmed(), 2000);
  });
  connect(m_archive, &ArchiveController::taskFinished, this, [this](bool success, const QString& error) {
    if (!success && !error.isEmpty()) {
      showError("Archive Operation Failed", error);
    }
    if (!m_extractQueue.isEmpty()) {
      startExtractQueue();
    }
  });

  m_leftPane = new FilePane(this);
  m_rightPane = new FilePane(this);
  m_rightPane->hide();

  connect(m_leftPane, &FilePane::paneFocused, this, &MainWindow::onPaneFocused);
  connect(m_rightPane, &FilePane::paneFocused, this, &MainWindow::onPaneFocused);

  connect(m_leftPane, &FilePane::currentPathChanged, this, [this](const QString& path) { onPanePathChanged(path); });
  connect(m_rightPane, &FilePane::currentPathChanged, this, [this](const QString& path) { onPanePathChanged(path); });

  connect(m_leftPane, &FilePane::openFileRequested, this, &MainWindow::openFile);
  connect(m_rightPane, &FilePane::openFileRequested, this, &MainWindow::openFile);

  connect(m_leftPane, &FilePane::contextMenuRequested, this, &MainWindow::showContextMenu);
  connect(m_rightPane, &FilePane::contextMenuRequested, this, &MainWindow::showContextMenu);

  connect(m_leftPane, &FilePane::dropRequested, this, &MainWindow::handleDrop);
  connect(m_rightPane, &FilePane::dropRequested, this, &MainWindow::handleDrop);

  updateActivePane(m_leftPane);

  m_placesModel = new PlacesModel(this);
  m_placesView = new QListView(this);
  m_placesView->setModel(m_placesModel);
  m_placesView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_placesView->setFixedWidth(180);

  connect(m_placesView, &QListView::clicked, this, [this](const QModelIndex& index) {
    const QString path = index.data(PlacesModel::PathRole).toString();
    if (!path.isEmpty()) {
      m_activePane->navigateTo(path);
    }
  });

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->addWidget(m_leftPane);
  m_splitter->addWidget(m_rightPane);
  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 1);

  auto* central = new QWidget(this);
  auto* layout = new QHBoxLayout(central);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  layout->addWidget(m_placesView);
  layout->addWidget(m_splitter, 1);
  setCentralWidget(central);

  m_terminal = new TerminalWidget(this);
  m_terminalDock = new QDockWidget("Terminal", this);
  m_terminalDock->setWidget(m_terminal);
  m_terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea);
  addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
  m_terminalDock->hide();

  buildToolbar();

  statusBar()->showMessage("Ready", 2000);
}

void MainWindow::buildToolbar() {
  auto* toolbar = addToolBar("Main");
  toolbar->setMovable(false);

  auto* actionBack = toolbar->addAction(QIcon::fromTheme("go-previous"), "Back");
  actionBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
  connect(actionBack, &QAction::triggered, this, [this]() { m_activePane->back(); });

  auto* actionForward = toolbar->addAction(QIcon::fromTheme("go-next"), "Forward");
  actionForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
  connect(actionForward, &QAction::triggered, this, [this]() { m_activePane->forward(); });

  auto* actionUp = toolbar->addAction(QIcon::fromTheme("go-up"), "Up");
  actionUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
  connect(actionUp, &QAction::triggered, this, [this]() { m_activePane->up(); });

  toolbar->addSeparator();

  m_actionViewMode = toolbar->addAction(QIcon::fromTheme("view-grid"), "Toggle View");
  connect(m_actionViewMode, &QAction::triggered, this, &MainWindow::toggleViewMode);

  m_actionSplitView = toolbar->addAction(QIcon::fromTheme("view-split-left-right"), "Split View");
  m_actionSplitView->setCheckable(true);
  m_actionSplitView->setShortcut(QKeySequence(Qt::Key_F3));
  connect(m_actionSplitView, &QAction::toggled, this, &MainWindow::toggleSplitView);

  m_actionTerminal = toolbar->addAction(QIcon::fromTheme("utilities-terminal"), "Terminal");
  m_actionTerminal->setCheckable(true);
  connect(m_actionTerminal, &QAction::toggled, this, &MainWindow::toggleTerminal);

  m_actionSyncTerminal = toolbar->addAction(QIcon::fromTheme("view-refresh"), "Sync Terminal");
  m_actionSyncTerminal->setCheckable(true);
  m_actionSyncTerminal->setChecked(true);
  connect(m_actionSyncTerminal, &QAction::toggled, this, &MainWindow::setTerminalSync);

  m_actionFocusTerminal = toolbar->addAction("Focus Terminal");
  m_actionFocusTerminal->setShortcut(QKeySequence(Qt::Key_F4));
  connect(m_actionFocusTerminal, &QAction::triggered, this, &MainWindow::focusTerminal);

  toolbar->addSeparator();

  auto* actionCopy = toolbar->addAction(QIcon::fromTheme("edit-copy"), "Copy");
  actionCopy->setShortcut(QKeySequence::Copy);
  connect(actionCopy, &QAction::triggered, this, &MainWindow::copySelection);

  auto* actionCut = toolbar->addAction(QIcon::fromTheme("edit-cut"), "Cut");
  actionCut->setShortcut(QKeySequence::Cut);
  connect(actionCut, &QAction::triggered, this, &MainWindow::cutSelection);

  auto* actionPaste = toolbar->addAction(QIcon::fromTheme("edit-paste"), "Paste");
  actionPaste->setShortcut(QKeySequence::Paste);
  connect(actionPaste, &QAction::triggered, this, &MainWindow::pasteClipboard);

  auto* actionRename = toolbar->addAction(QIcon::fromTheme("edit-rename"), "Rename");
  actionRename->setShortcut(QKeySequence(Qt::Key_F2));
  connect(actionRename, &QAction::triggered, this, &MainWindow::renameSelection);

  auto* actionDelete = toolbar->addAction(QIcon::fromTheme("edit-delete"), "Delete");
  actionDelete->setShortcut(QKeySequence::Delete);
  connect(actionDelete, &QAction::triggered, this, &MainWindow::deleteSelection);

  toolbar->addSeparator();

  auto* actionMkdir = toolbar->addAction(QIcon::fromTheme("folder-new"), "New Folder");
  connect(actionMkdir, &QAction::triggered, this, &MainWindow::createFolder);

  auto* actionNewFile = toolbar->addAction(QIcon::fromTheme("document-new"), "New File");
  connect(actionNewFile, &QAction::triggered, this, &MainWindow::createFile);

  auto* actionCompress = toolbar->addAction(QIcon::fromTheme("package-x-generic"), "Compress");
  connect(actionCompress, &QAction::triggered, this, &MainWindow::compressSelection);

  auto* actionExtract = toolbar->addAction(QIcon::fromTheme("archive-extract"), "Extract");
  connect(actionExtract, &QAction::triggered, this, &MainWindow::extractSelection);

  auto* sortMenu = new QMenu("Sort", this);
  auto* sortGroup = new QActionGroup(this);
  auto* sortName = sortMenu->addAction("By Name");
  auto* sortSize = sortMenu->addAction("By Size");
  auto* sortDate = sortMenu->addAction("By Date");
  sortGroup->addAction(sortName);
  sortGroup->addAction(sortSize);
  sortGroup->addAction(sortDate);
  connect(sortName, &QAction::triggered, this, &MainWindow::sortByName);
  connect(sortSize, &QAction::triggered, this, &MainWindow::sortBySize);
  connect(sortDate, &QAction::triggered, this, &MainWindow::sortByDate);

  auto* sortButton = new QToolButton(this);
  sortButton->setText("Sort");
  sortButton->setMenu(sortMenu);
  sortButton->setPopupMode(QToolButton::InstantPopup);
  toolbar->addWidget(sortButton);
}

void MainWindow::openFile(const QString& path) {
  const bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  ActionLogger::instance()->logAction("open", {path}, {}, success, success ? "" : "Open failed");
}

void MainWindow::onPaneFocused(FilePane* pane) {
  updateActivePane(pane);
}

void MainWindow::onPanePathChanged(const QString& path) {
  if (m_activePane && m_activePane->currentPath() == path) {
    setWindowTitle(QString("Dolphin Lite - %1").arg(path));
    if (m_actionSyncTerminal && m_actionSyncTerminal->isChecked() && m_terminal) {
      m_terminal->setWorkingDirectory(path);
    }
  }
  ActionLogger::instance()->logAction("navigate", {path});
}

void MainWindow::showContextMenu(const QPoint& pos, const QList<QUrl>& urls) {
  QMenu menu(this);
  const QStringList paths = [urls]() {
    QStringList list;
    for (const QUrl& url : urls) {
      if (url.isLocalFile()) {
        list << url.toLocalFile();
      }
    }
    return list;
  }();

  QAction* openAct = menu.addAction("Open");
  openAct->setEnabled(!paths.isEmpty());
  connect(openAct, &QAction::triggered, this, [this, paths]() {
    if (paths.isEmpty()) {
      return;
    }
    QFileInfo info(paths.first());
    if (info.isDir()) {
      m_activePane->navigateTo(info.absoluteFilePath());
    } else {
      openFile(paths.first());
    }
  });

  menu.addSeparator();

  QAction* copyAct = menu.addAction("Copy");
  copyAct->setShortcut(QKeySequence::Copy);
  connect(copyAct, &QAction::triggered, this, &MainWindow::copySelection);

  QAction* cutAct = menu.addAction("Cut");
  cutAct->setShortcut(QKeySequence::Cut);
  connect(cutAct, &QAction::triggered, this, &MainWindow::cutSelection);

  QAction* pasteAct = menu.addAction("Paste");
  pasteAct->setShortcut(QKeySequence::Paste);
  pasteAct->setEnabled(!m_clipboardPaths.isEmpty());
  connect(pasteAct, &QAction::triggered, this, &MainWindow::pasteClipboard);

  menu.addSeparator();

  QAction* renameAct = menu.addAction("Rename");
  connect(renameAct, &QAction::triggered, this, &MainWindow::renameSelection);

  QAction* deleteAct = menu.addAction("Delete");
  connect(deleteAct, &QAction::triggered, this, &MainWindow::deleteSelection);

  menu.addSeparator();

  QAction* newFolderAct = menu.addAction("New Folder");
  connect(newFolderAct, &QAction::triggered, this, &MainWindow::createFolder);

  QAction* newFileAct = menu.addAction("New File");
  connect(newFileAct, &QAction::triggered, this, &MainWindow::createFile);

  menu.addSeparator();

  QAction* compressAct = menu.addAction("Compress to .zip");
  connect(compressAct, &QAction::triggered, this, &MainWindow::compressSelection);

  QAction* extractAct = menu.addAction("Extract");
  connect(extractAct, &QAction::triggered, this, &MainWindow::extractSelection);

  menu.exec(pos);
}

void MainWindow::handleDrop(const QList<QUrl>& urls, const QString& destDir, Qt::DropAction action) {
  QStringList sources;
  for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
      sources << url.toLocalFile();
    }
  }

  if (sources.isEmpty() || destDir.isEmpty()) {
    return;
  }

  if (action == Qt::MoveAction) {
    m_fileOps->move(sources, destDir);
  } else {
    m_fileOps->copy(sources, destDir);
  }
}

void MainWindow::copySelection() {
  m_clipboardPaths = m_activePane->selectedPaths();
  m_clipboardCut = false;
}

void MainWindow::cutSelection() {
  m_clipboardPaths = m_activePane->selectedPaths();
  m_clipboardCut = true;
}

void MainWindow::pasteClipboard() {
  if (m_clipboardPaths.isEmpty()) {
    return;
  }

  const QString destDir = m_activePane->currentPath();
  if (m_clipboardCut) {
    m_clipboardPendingClear = true;
    m_fileOps->move(m_clipboardPaths, destDir);
  } else {
    m_fileOps->copy(m_clipboardPaths, destDir);
  }
}

void MainWindow::renameSelection() {
  const QStringList paths = m_activePane->selectedPaths();
  if (paths.size() != 1) {
    return;
  }

  QFileInfo info(paths.first());
  bool ok = false;
  const QString newName = QInputDialog::getText(this, "Rename", "New name:", QLineEdit::Normal, info.fileName(), &ok);
  if (!ok || newName.trimmed().isEmpty()) {
    return;
  }

  m_fileOps->rename(info.absoluteFilePath(), newName.trimmed());
}

void MainWindow::deleteSelection() {
  const QStringList paths = m_activePane->selectedPaths();
  if (paths.isEmpty()) {
    return;
  }

  const auto reply = QMessageBox::question(this, "Delete", "Delete selected items?",
                                           QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  m_fileOps->remove(paths);
}

void MainWindow::createFolder() {
  bool ok = false;
  const QString name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "New Folder", &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  m_fileOps->createFolder(m_activePane->currentPath(), name.trimmed());
}

void MainWindow::createFile() {
  bool ok = false;
  const QString name = QInputDialog::getText(this, "New File", "File name:", QLineEdit::Normal, "New File.txt", &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  m_fileOps->createFile(m_activePane->currentPath(), name.trimmed());
}

void MainWindow::compressSelection() {
  const QStringList paths = m_activePane->selectedPaths();
  if (paths.isEmpty()) {
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(this, "Compress", "Archive name:", QLineEdit::Normal, "archive.zip", &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  const QString destZip = QDir(m_activePane->currentPath()).filePath(name.trimmed());
  m_archive->compressZip(paths, destZip, m_activePane->currentPath());
}

void MainWindow::extractSelection() {
  const QStringList paths = m_activePane->selectedPaths();
  if (paths.isEmpty()) {
    return;
  }

  m_extractQueue.clear();
  for (const QString& path : paths) {
    m_extractQueue << path;
  }
  m_extractDestination = m_activePane->currentPath();
  startExtractQueue();
}

void MainWindow::startExtractQueue() {
  if (m_extractQueue.isEmpty()) {
    return;
  }

  const QString nextArchive = m_extractQueue.takeFirst();
  m_archive->extractArchive(nextArchive, m_extractDestination);
}

void MainWindow::sortByName() {
  if (m_activePane && m_activePane->activeView()->view()->model()) {
    m_activePane->activeView()->view()->model()->sort(0, Qt::AscendingOrder);
  }
}

void MainWindow::sortBySize() {
  if (m_activePane && m_activePane->activeView()->view()->model()) {
    m_activePane->activeView()->view()->model()->sort(1, Qt::DescendingOrder);
  }
}

void MainWindow::sortByDate() {
  if (m_activePane && m_activePane->activeView()->view()->model()) {
    m_activePane->activeView()->view()->model()->sort(3, Qt::DescendingOrder);
  }
}

void MainWindow::toggleSplitView(bool enabled) {
  m_rightPane->setVisible(enabled);
  if (enabled) {
    m_splitter->setSizes({1, 1});
  }
}

void MainWindow::toggleTerminal(bool enabled) {
  m_terminalDock->setVisible(enabled);
  if (enabled && m_actionSyncTerminal->isChecked()) {
    m_terminal->setWorkingDirectory(m_activePane->currentPath());
  }
}

void MainWindow::toggleViewMode() {
  if (!m_activePane) {
    return;
  }
  if (m_activePane->viewMode() == FileView::Mode::Details) {
    m_activePane->setViewMode(FileView::Mode::Icons);
  } else {
    m_activePane->setViewMode(FileView::Mode::Details);
  }
}

void MainWindow::focusTerminal() {
  if (!m_terminalDock->isVisible()) {
    m_terminalDock->show();
  }
  m_terminal->focusTerminal();
}

void MainWindow::setTerminalSync(bool enabled) {
  m_terminal->setSyncEnabled(enabled);
  if (enabled) {
    m_terminal->setWorkingDirectory(m_activePane->currentPath());
  }
}

void MainWindow::updateActivePane(FilePane* pane) {
  if (!pane) {
    return;
  }

  if (m_activePane) {
    m_activePane->setActive(false);
  }
  m_activePane = pane;
  m_activePane->setActive(true);

  if (m_actionSyncTerminal && m_actionSyncTerminal->isChecked() && m_terminal) {
    m_terminal->setWorkingDirectory(m_activePane->currentPath());
  }
}

void MainWindow::showError(const QString& title, const QString& message) {
  QMessageBox::critical(this, title, message);
}
