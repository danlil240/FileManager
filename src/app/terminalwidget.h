#pragma once

#include <QWidget>

class QTimer;

class TerminalWidget : public QWidget {
  Q_OBJECT
public:
  explicit TerminalWidget(QWidget* parent = nullptr);
  ~TerminalWidget() override;

  void setWorkingDirectory(const QString& path);
  void setSyncEnabled(bool enabled);
  bool syncEnabled() const;
  void focusTerminal();

private:
  void spawnShell(const QString& dir);
  void sendCommand(const QString& command);
  QString shellQuote(const QString& text) const;

  bool m_syncEnabled = true;
  bool m_started = false;
  QString m_currentDir;

#ifdef HAS_VTE
  QWidget* m_container = nullptr;
  void* m_terminal = nullptr;
  void* m_window = nullptr;
  QTimer* m_glibPump = nullptr;
#endif
};
