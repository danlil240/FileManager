#include "terminalwidget.h"
#include "actionlogger.h"

#include <QDir>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>
#include <QTimer>
#include <QWindow>

#ifdef HAS_VTE
#undef signals
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <vte/vte.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#define signals Q_SIGNALS
#endif

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef HAS_VTE
    static bool gtkInitialized = false;
    if (!gtkInitialized)
    {
        gtkInitialized = gtk_init_check(nullptr, nullptr);
    }

    if (gtkInitialized)
    {
        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
        gtk_widget_set_size_request(window, 640, 240);

        VteTerminal *terminal = VTE_TERMINAL(vte_terminal_new());
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(terminal));
        gtk_widget_show_all(window);

        gtk_widget_realize(window);
        GdkWindow *gdkWindow = gtk_widget_get_window(window);

#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
        {
            WId winId = gdk_x11_window_get_xid(gdkWindow);
            QWindow *foreign = QWindow::fromWinId(winId);
            m_container = QWidget::createWindowContainer(foreign, this);
            m_container->setFocusPolicy(Qt::StrongFocus);
            layout->addWidget(m_container);

            m_terminal = terminal;
            m_window = window;

            spawnShell(QDir::homePath());
        }
        else
#endif
        {
            auto *label = new QLabel("Embedded terminal requires X11 display.", this);
            label->setAlignment(Qt::AlignCenter);
            layout->addWidget(label);
            gtk_widget_destroy(window);
            m_terminal = nullptr;
            m_window = nullptr;
            return;
        }

        auto *copyShortcut = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
        connect(copyShortcut, &QShortcut::activated, this,
            [this]()
            {
                if (m_terminal)
                {
                    vte_terminal_copy_clipboard_format(VTE_TERMINAL(m_terminal), VTE_FORMAT_TEXT);
                }
            });

        auto *pasteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+V"), this);
        connect(pasteShortcut, &QShortcut::activated, this,
            [this]()
            {
                if (m_terminal)
                {
                    vte_terminal_paste_clipboard(VTE_TERMINAL(m_terminal));
                }
            });

        m_glibPump = new QTimer(this);
        connect(m_glibPump, &QTimer::timeout, this, []() { g_main_context_iteration(nullptr, false); });
        m_glibPump->start(10);
    }
    else
    {
        auto *label = new QLabel("Embedded terminal unavailable (VTE init failed).", this);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
    }
#endif
}

TerminalWidget::~TerminalWidget()
{
#ifdef HAS_VTE
    if (m_window)
    {
        gtk_widget_destroy(GTK_WIDGET(m_window));
    }
#endif
}

void TerminalWidget::setWorkingDirectory(const QString &path)
{
    if (!m_syncEnabled || path.isEmpty())
    {
        return;
    }

    if (!m_started)
    {
        spawnShell(path);
    }
    else
    {
        const QString cmd = QString("cd %1\n").arg(shellQuote(path));
        sendCommand(cmd);
    }

    m_currentDir = path;
    ActionLogger::instance()->logTerminalSync(path);
}

void TerminalWidget::setSyncEnabled(bool enabled)
{
    m_syncEnabled = enabled;
}

bool TerminalWidget::syncEnabled() const
{
    return m_syncEnabled;
}

void TerminalWidget::focusTerminal()
{
#ifdef HAS_VTE
    if (m_container)
    {
        m_container->setFocus(Qt::OtherFocusReason);
    }
    if (m_terminal)
    {
        gtk_widget_grab_focus(GTK_WIDGET(m_terminal));
    }
#endif
}

void TerminalWidget::spawnShell(const QString &dir)
{
#ifdef HAS_VTE
    if (!m_terminal)
    {
        return;
    }
    const QString shell = qEnvironmentVariable("SHELL", "/bin/bash");
    QByteArray shellBytes = shell.toUtf8();
    char *argv[] = {shellBytes.data(), nullptr};
    QByteArray dirBytes = dir.toUtf8();

    vte_terminal_spawn_async(VTE_TERMINAL(m_terminal), VTE_PTY_DEFAULT, dirBytes.constData(), argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr,
        nullptr, -1, nullptr, nullptr, nullptr);
    m_started = true;
#else
    Q_UNUSED(dir);
#endif
}

void TerminalWidget::sendCommand(const QString &command)
{
#ifdef HAS_VTE
    if (!m_terminal)
    {
        return;
    }
    QByteArray data = command.toUtf8();
    vte_terminal_feed_child(VTE_TERMINAL(m_terminal), data.constData(), data.size());
#else
    Q_UNUSED(command);
#endif
}

QString TerminalWidget::shellQuote(const QString &text) const
{
    QString escaped = text;
    escaped.replace("'", "'\\''");
    return QString("'%1'").arg(escaped);
}
