#pragma once

#include <QMainWindow>
#include <QList>
#include <QHash>

#include "domain/session.h"
#include "ui/theme.h"
#include "ui/statusbar/statusbarmanager.h"
#include "tunnels/tunnelmanager.h"
#include "ui/tunnels/tunneldialog.h"

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QWidget;
class Sidebar;
class TerminalPane;
class TerminalWidget;

namespace qterm2 {
class ApplicationContext;
class SessionStore;
class SettingsStore;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(qterm2::ApplicationContext *context, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // ---- construction ----------------------------------------------------
    void loadSettings();
    void saveSettings();
    void buildUi();
    void buildActions();
    void buildMenus();
    void buildToolbar();
    void buildStatusBar();
    QWidget *buildWelcomePage();

    // ---- sessions --------------------------------------------------------
    void newLocalTab();
    void promptNewSession();
    void promptEditSession(const QString &id);
    void connectSession(const QString &id);
    void duplicateSession(const QString &id);
    void toggleFavorite(const QString &id);
    void deleteSession(const QString &id);
    void installPublicKey(const QString &id);
    void copyCommand(const QString &id);
    void importSshConfig();
    void exportSshConfig();
    void showKnownHosts();
    void showSshAgent();
    void showSnippets();
    void showCommandPalette();
    void showGlobalSearch();
    void updateSessionsMenu();
    void registerCommands();

    TerminalPane *addSessionTab(const Session &session);
    TerminalPane *addCommandTab(const QString &title, const Session &session,
                                const QString &program, const QStringList &args);
    void wirePane(TerminalPane *pane);
    void scheduleReconnect(TerminalPane *pane, TerminalWidget *terminal);
    void refreshTabInfo(TerminalPane *pane);

    // ---- tabs / panes ----------------------------------------------------
    TerminalPane *currentPane() const;
    TerminalWidget *activeTerminal() const;
    void closeTabAt(int index, bool confirm);
    void closeCurrentPane();
    void splitActive(Qt::Orientation orientation);
    void nextTab(int delta);

    // ---- ui state --------------------------------------------------------
    void setSidebarVisible(bool visible);
    void applyTheme();
    void applyTerminalAppearance();
    void updateWelcomeState();
    void updateWindowTitle();
    void updateStatus();
    void showTunnels();
    void showPlugins();
    void updateZoomHint();
    void adjustFontSize(int delta);
    void resetFontSize();
    void showPreferences();
    void showAbout();
    void showShortcuts();

    // ---- search ----------------------------------------------------------
    void showSearch();
    void hideSearch();
    bool findInTerminal(bool forward);

    qterm2::ApplicationContext *m_context = nullptr;
    qterm2::SessionStore *m_store = nullptr;
    qterm2::SettingsStore *m_settings = nullptr;

    Sidebar *m_sidebar = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_welcome = nullptr;

    QWidget *m_searchBar = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_searchStatus = nullptr;

    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;
    qterm2::StatusBarManager *m_statusBarManager = nullptr;
    qterm2::TunnelManager *m_tunnelManager = nullptr;
    QMenu *m_sessionsMenu = nullptr;
    QAction *m_toggleSidebarAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QHash<QString, QAction *> m_actions;
    QStringList m_registeredCommandIds;
    QHash<TerminalWidget *, int> m_reconnectAttempts;
    QHash<TerminalWidget *, QTimer *> m_reconnectTimers;

    QString m_fontFamily;
    int m_fontSize = 11;
    int m_scrollback = 10000;
    QString m_cursorStyle = QStringLiteral("block");
    bool m_cursorBlink = true;
    QString m_localShell;
    bool m_sidebarVisible = true;
    int m_sidebarWidth = 272;
    bool m_darkTheme = true;
    bool m_confirmClose = true;
    bool m_startWithLocalTab = true;
    bool m_closing = false;
};
