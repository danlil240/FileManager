#pragma once

#include <QMainWindow>
#include <QStringList>

#include "fileview.h"

class ArchiveController;
class FileOpsController;
class FilePane;
class PlacesModel;
class ProgressDialog;
class TerminalWidget;

class QListView;
class QDockWidget;
class QSplitter;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

private slots:
  void openFile(const QString& path);
  void onPaneFocused(FilePane* pane);
  void onPanePathChanged(const QString& path);
  void showContextMenu(const QPoint& pos, const QList<QUrl>& urls);
  void handleDrop(const QList<QUrl>& urls, const QString& destDir, Qt::DropAction action);

  void copySelection();
  void cutSelection();
  void pasteClipboard();
  void renameSelection();
  void deleteSelection();
  void createFolder();
  void createFile();
  void compressSelection();
  void extractSelection();
  void sortByName();
  void sortBySize();
  void sortByDate();

  void toggleSplitView(bool enabled);
  void toggleTerminal(bool enabled);
  void toggleViewMode();
  void focusTerminal();
  void setTerminalSync(bool enabled);

private:
  void buildToolbar();
  void updateActivePane(FilePane* pane);
  void showError(const QString& title, const QString& message);
  void startExtractQueue();

  FilePane* m_leftPane = nullptr;
  FilePane* m_rightPane = nullptr;
  FilePane* m_activePane = nullptr;
  QSplitter* m_splitter = nullptr;

  PlacesModel* m_placesModel = nullptr;
  QListView* m_placesView = nullptr;

  FileOpsController* m_fileOps = nullptr;
  ArchiveController* m_archive = nullptr;
  ProgressDialog* m_progress = nullptr;

  QDockWidget* m_terminalDock = nullptr;
  TerminalWidget* m_terminal = nullptr;

  QAction* m_actionSplitView = nullptr;
  QAction* m_actionTerminal = nullptr;
  QAction* m_actionSyncTerminal = nullptr;
  QAction* m_actionViewMode = nullptr;
  QAction* m_actionFocusTerminal = nullptr;

  QStringList m_clipboardPaths;
  bool m_clipboardCut = false;
  bool m_clipboardPendingClear = false;

  QStringList m_extractQueue;
  QString m_extractDestination;
};
