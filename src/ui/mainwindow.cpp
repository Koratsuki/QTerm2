#include "ui/mainwindow.h"

using qterm2::SessionStore;

#include "app/applicationcontext.h"
#include "sessions/sessionlauncher.h"
#include "sessions/sessionstore.h"
#include "sessions/snippetstore.h"
#include "ssh/keymanager.h"
#include "ssh/sshcommandbuilder.h"
#include "storage/settingsstore.h"
#include "terminal/broadcast/broadcastmanager.h"
#include "terminal/terminalwidget.h"
#include "tunnels/tunnelmanager.h"
#include "ui/networkdiscoverydialog.h"
#include "ui/sftp/sftpdialog.h"
#include "ui/tunnels/tunneldialog.h"
#include "ui/commandpalettedialog.h"
#include "ui/globalsearchdialog.h"
#include "ui/preferencesdialog.h"
#include "ui/sessiondialog.h"
#include "ui/sidebar.h"
#include "ui/snippetdialog.h"
#include "ui/sshintegration.h"
#include "ui/terminalpane.h"
#include "ui/uihelpers.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

using qterm2::uihelpers::dotIcon;

namespace {

QToolButton *toolbarButton(QAction *action, const QString &label, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setText(label);
    button->setToolTip(action->text());
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    return button;
}

} // namespace

// ---------------------------------------------------------------- lifecycle

MainWindow::MainWindow(qterm2::ApplicationContext *context, QWidget *parent)
    : QMainWindow(parent)
    , m_context(context)
    , m_broadcast(new qterm2::BroadcastManager(this))
    , m_tunnels(new qterm2::TunnelManager(this))
    , m_store(context ? context->sessionStore() : nullptr)
    , m_settings(context ? context->settings() : nullptr)
{
    loadSettings();

    Theme::setCurrentMode(m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light);

    buildUi();
    buildActions();
    buildMenus();
    buildToolbar();
    buildStatusBar();
    registerCommands();

    connect(m_sidebar, &Sidebar::connectRequested, this, &MainWindow::connectSession);
    connect(m_sidebar, &Sidebar::editRequested, this, &MainWindow::promptEditSession);
    connect(m_sidebar, &Sidebar::deleteRequested, this, &MainWindow::deleteSession);
    connect(m_sidebar, &Sidebar::duplicateRequested, this, &MainWindow::duplicateSession);
    connect(m_sidebar, &Sidebar::favoriteToggleRequested, this, &MainWindow::toggleFavorite);
    connect(m_sidebar, &Sidebar::installKeyRequested, this, &MainWindow::installPublicKey);
    connect(m_sidebar, &Sidebar::copyCommandRequested, this, &MainWindow::copyCommand);
    connect(m_sidebar, &Sidebar::newSshRequested, this, &MainWindow::promptNewSession);
    connect(m_sidebar, &Sidebar::newLocalRequested, this, &MainWindow::newLocalTab);
    connect(m_sidebar, &Sidebar::hideRequested, this, [this] { setSidebarVisible(false); });

    connect(m_store, &SessionStore::changed, this, &MainWindow::updateSessionsMenu);

    const QByteArray geometry = m_settings->value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1240, 800);

    setSidebarVisible(m_sidebarVisible);
    applyTerminalAppearance();
    updateSessionsMenu();

    if (m_tunnels && m_settings)
        m_tunnels->loadFromSettings(m_settings);

    if (m_startWithLocalTab)
        newLocalTab();
    else
        updateWelcomeState();

    updateZoomHint();
}

MainWindow::~MainWindow() = default;

void MainWindow::loadSettings()
{
    if (!m_settings)
        return;
    m_fontFamily = m_settings->value(QStringLiteral("terminal/fontFamily")).toString();
    m_fontSize = qBound(6, m_settings->value(QStringLiteral("terminal/fontSize"), 11).toInt(), 40);
    m_scrollback = qMax(0, m_settings->value(QStringLiteral("terminal/scrollback"), 10000).toInt());
    m_cursorStyle = TerminalWidget::cursorStyleName(TerminalWidget::cursorStyleFromName(
        m_settings->value(QStringLiteral("terminal/cursorStyle"),
                          QStringLiteral("block")).toString()));
    m_cursorBlink = m_settings->value(QStringLiteral("terminal/cursorBlink"), true).toBool();
    m_localShell = m_settings->value(QStringLiteral("terminal/shell"), defaultShellPath()).toString();
    if (m_localShell.isEmpty())
        m_localShell = defaultShellPath();
    m_sidebarVisible = m_settings->value(QStringLiteral("ui/sidebarVisible"), true).toBool();
    m_sidebarWidth = qBound(210, m_settings->value(QStringLiteral("ui/sidebarWidth"), 272).toInt(), 480);
    m_darkTheme = m_settings->value(QStringLiteral("ui/darkTheme"), true).toBool();
    m_confirmClose = m_settings->value(QStringLiteral("ui/confirmClose"), true).toBool();
    m_startWithLocalTab = m_settings->value(QStringLiteral("startup/openLocalTab"), true).toBool();
}

void MainWindow::saveSettings()
{
    if (!m_settings)
        return;
    m_settings->setValue(QStringLiteral("terminal/fontFamily"), m_fontFamily);
    m_settings->setValue(QStringLiteral("terminal/fontSize"), m_fontSize);
    m_settings->setValue(QStringLiteral("terminal/scrollback"), m_scrollback);
    m_settings->setValue(QStringLiteral("terminal/cursorStyle"), m_cursorStyle);
    m_settings->setValue(QStringLiteral("terminal/cursorBlink"), m_cursorBlink);
    m_settings->setValue(QStringLiteral("terminal/shell"), m_localShell);
    m_settings->setValue(QStringLiteral("ui/sidebarVisible"), m_sidebarVisible);
    m_settings->setValue(QStringLiteral("ui/sidebarWidth"), m_sidebarWidth);
    m_settings->setValue(QStringLiteral("ui/darkTheme"), m_darkTheme);
    m_settings->setValue(QStringLiteral("ui/geometry"), saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_confirmClose) {
        int running = 0;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
            if (pane && pane->anyRunning())
                ++running;
        }
        if (running > 0) {
            const auto answer = QMessageBox::question(
                this, tr("Close QTerm2"),
                tr("There is %1 active sessions. ¿Close anyway?").arg(running),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
    }
    saveSettings();
    // save session state
    if (m_settings) {
        QStringList ids;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *pane = qobject_cast<TerminalPane*>(m_tabs->widget(i))) {
                if (auto *term = pane->activeTerminal()) {
                    if (!term->property("sessionId").toString().isEmpty())
                        ids << term->property("sessionId").toString();
                }
            }
        }
        m_settings->setValue(QStringLiteral("ui/restoredSessionIds"), ids);
    }
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------- UI

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("QTerm2"));
    setMinimumSize(760, 480);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    m_sidebar = new Sidebar(m_store, m_splitter);
    m_splitter->addWidget(m_sidebar);

    auto *right = new QWidget(m_splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // ---- search bar ------------------------------------------------------
    m_searchBar = new QFrame(right);
    m_searchBar->setObjectName(QStringLiteral("SearchBar"));
    auto *searchLayout = new QHBoxLayout(m_searchBar);
    searchLayout->setContentsMargins(10, 7, 10, 7);
    searchLayout->setSpacing(7);

    m_searchEdit = new QLineEdit(m_searchBar);
    m_searchEdit->setPlaceholderText(tr("Buscar en la terminal…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(430);
    searchLayout->addWidget(m_searchEdit);

    m_searchStatus = new QLabel(m_searchBar);
    m_searchStatus->setObjectName(QStringLiteral("Hint"));
    searchLayout->addWidget(m_searchStatus);
    searchLayout->addStretch(1);

    auto *prevButton = new QToolButton(m_searchBar);
    prevButton->setText(QStringLiteral("\u2191"));
    prevButton->setToolTip(tr("Coincidencia anterior (Shift+F3)"));
    auto *nextButton = new QToolButton(m_searchBar);
    nextButton->setText(QStringLiteral("\u2193"));
    nextButton->setToolTip(tr("Coincidencia siguiente (F3)"));
    auto *closeButton = new QToolButton(m_searchBar);
    closeButton->setText(QStringLiteral("\u2715"));
    closeButton->setToolTip(tr("Cerrar búsqueda (Esc)"));
    searchLayout->addWidget(prevButton);
    searchLayout->addWidget(nextButton);
    searchLayout->addWidget(closeButton);
    m_searchBar->hide();
    rightLayout->addWidget(m_searchBar);

    // ---- tabs / welcome --------------------------------------------------
    m_stack = new QStackedWidget(right);
    m_tabs = new QTabWidget(m_stack);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setIconSize(QSize(10, 10));
    m_stack->addWidget(m_tabs);

    m_welcome = buildWelcomePage();
    m_stack->addWidget(m_welcome);
    rightLayout->addWidget(m_stack, 1);

    auto *corner = new QToolButton(m_tabs);
    corner->setText(QStringLiteral("+"));
    corner->setToolTip(tr("Nueva pestaña"));
    corner->setPopupMode(QToolButton::InstantPopup);
    auto *cornerMenu = new QMenu(corner);
    cornerMenu->addAction(tr("Nueva terminal local"), this, &MainWindow::newLocalTab);
    auto *connMenu = cornerMenu->addMenu(tr("Nueva conexión"));
    connMenu->addAction(tr("SSH"), this, &MainWindow::promptNewSession);
    connMenu->addAction(tr("Telnet"), this, [this]() {
        Session s = Session::makeLocal();
        s.type = Session::Type::Telnet;
        s.name = tr("Telnet");
        s.port = 23;
        SessionDialog dlg(s, m_store ? m_store->groups() : QStringList(), this);
        if (dlg.exec() == QDialog::Accepted && m_store) {
            Session result = dlg.resultSession();
            if (m_store->addSession(result))
                connectSession(result.id);
        }
    });
    corner->setMenu(cornerMenu);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        auto *tray = new QSystemTrayIcon(this);
        tray->setIcon(windowIcon());
        auto *trayMenu = new QMenu(this);
        trayMenu->addAction(tr("Show"), this, &QWidget::showNormal);
        trayMenu->addAction(tr("Hide"), this, &QWidget::hide);
        trayMenu->addSeparator();
        trayMenu->addAction(tr("Quit"), this, &QApplication::quit);
        tray->setContextMenu(trayMenu);
        tray->show();
    }

    m_splitter->addWidget(right);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ m_sidebarWidth, 960 });
    setCentralWidget(m_splitter);

    // ---- signals ---------------------------------------------------------
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int index = m_tabs->tabBar()->tabAt(pos);
        if (index < 0)
            return;
        QMenu menu(this);
        menu.addAction(tr("Dividir a la derecha"), this, [this]() { splitActive(Qt::Horizontal); });
        menu.addAction(tr("Dividir abajo"), this, [this]() { splitActive(Qt::Vertical); });
        menu.addSeparator();
        menu.addAction(tr("Duplicar pestaña"), this, [this, index]() {
            auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(index));
            if (!pane)
                return;
            const Session s = pane->session();
            if (m_store && m_store->contains(s.id))
                connectSession(s.id);
            else
                addSessionTab(s);
        });
        menu.addAction(tr("Renombrar sesión…"), this, [this, index]() {
            auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(index));
            if (!pane)
                return;
            const Session s = pane->session();
            if (m_store && m_store->contains(s.id))
                promptEditSession(s.id);
            else
                statusBar()->showMessage(tr("Solo se pueden renombrar sesiones guardadas."), 3000);
        });
        menu.addSeparator();
        menu.addAction(tr("Reconnect"), this, [this, index]() {
            if (auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(index))) {
                QString error;
                if (!pane->restartActive(&error)) {
                    statusBar()->showMessage(
                        tr("Reconexión fallida: %1").arg(error), 5000);
                } else {
                    statusBar()->showMessage(
                        tr("Reconectando a %1…").arg(pane->session().displayTarget()),
                        3000);
                }
            }
        });
        menu.addAction(tr("Desconectar"), this, [this, index]() { closeTabAt(index, false); });
        menu.addSeparator();

        const bool hasOther = m_tabs->count() > 1;
        const bool hasRight = index < m_tabs->count() - 1;
        QAction *closeOthers = menu.addAction(tr("Cerrar otras pestañas"), this, [this, index]() {
            const bool prevConfirm = m_confirmClose;
            m_confirmClose = false;
            for (int i = m_tabs->count() - 1; i >= 0; --i) {
                if (i != index)
                    closeTabAt(i, true);
            }
            m_confirmClose = prevConfirm;
        });
        closeOthers->setEnabled(hasOther);
        QAction *closeRight = menu.addAction(tr("Cerrar pestañas a la derecha"), this, [this, index]() {
            const bool prevConfirm = m_confirmClose;
            m_confirmClose = false;
            for (int i = m_tabs->count() - 1; i > index; --i)
                closeTabAt(i, true);
            m_confirmClose = prevConfirm;
        });
        closeRight->setEnabled(hasRight);
        menu.addSeparator();
        menu.addAction(tr("Cerrar"), this, [this, index]() { closeTabAt(index, true); });
        menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
    });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        closeTabAt(index, true);
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        if (m_searchBar->isVisible())
            hideSearch();
        if (TerminalPane *pane = currentPane())
            pane->focusActive();
        updateWindowTitle();
        updateStatus();
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (TerminalWidget *term = activeTerminal()) {
            term->setSearchText(text);
            if (text.isEmpty())
                term->clearSearch();
        }
        m_searchStatus->clear();
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this] {
        findInTerminal(true);
    });
    connect(prevButton, &QToolButton::clicked, this, [this] { findInTerminal(false); });
    connect(nextButton, &QToolButton::clicked, this, [this] { findInTerminal(true); });
    connect(closeButton, &QToolButton::clicked, this, &MainWindow::hideSearch);

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_searchBar);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &MainWindow::hideSearch);

    auto *nextShortcut = new QShortcut(QKeySequence(Qt::Key_F3), m_searchBar);
    nextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextShortcut, &QShortcut::activated, this, [this] { findInTerminal(true); });
    auto *prevShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), m_searchBar);
    prevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(prevShortcut, &QShortcut::activated, this, [this] { findInTerminal(false); });

    connect(m_splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (m_sidebar && m_sidebar->isVisible() && m_sidebar->width() > 100)
            m_sidebarWidth = m_sidebar->width();
    });
}

QWidget *MainWindow::buildWelcomePage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("Welcome"));
    page->setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 40);
    layout->setSpacing(12);
    layout->addStretch(2);

    auto *logo = new QLabel(QStringLiteral(">_"), page);
    QFont logoFont(QStringLiteral("Monospace"));
    logoFont.setPointSize(36);
    logoFont.setBold(true);
    logo->setFont(logoFont);
    logo->setStyleSheet(QStringLiteral("color: #60a5fa;"));
    logo->setAlignment(Qt::AlignHCenter);
    layout->addWidget(logo);

    auto *title = new QLabel(QStringLiteral("QTerm2"), page);
    QFont titleFont = title->font();
    titleFont.setPointSize(32);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Terminal xterm-256color · SSH session manager · Tabs and panes"), page);
    subtitle->setObjectName(QStringLiteral("Hint"));
    subtitle->setAlignment(Qt::AlignHCenter);
    layout->addWidget(subtitle);

    layout->addSpacing(18);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch(1);
    auto *localButton = new QPushButton(tr("Nueva terminal local"), page);
    localButton->setDefault(true);
    auto *sshButton = new QPushButton(tr("Nueva conexión SSH"), page);
    auto *sessionsButton = new QPushButton(tr("Mostrar sesiones"), page);
    buttons->addWidget(localButton);
    buttons->addWidget(sshButton);
    buttons->addWidget(sessionsButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    layout->addSpacing(24);
    auto *hints = new QLabel(tr(
        "Ctrl+B  panel lateral      Ctrl+Shift+D  dividir a la derecha      "
        "Ctrl+Shift+E  dividir abajo\n"
        "Ctrl+Shift+F  buscar      Ctrl+T  nueva terminal      Ctrl+W  cerrar pestaña"), page);
    hints->setObjectName(QStringLiteral("Hint"));
    hints->setAlignment(Qt::AlignHCenter);
    QFont hintFont = hints->font();
    hintFont.setFamily(QStringLiteral("Monospace"));
    hintFont.setPointSize(9);
    hints->setFont(hintFont);
    layout->addWidget(hints);

    layout->addStretch(3);

    connect(localButton, &QPushButton::clicked, this, &MainWindow::newLocalTab);
    connect(sshButton, &QPushButton::clicked, this, &MainWindow::promptNewSession);
    connect(sessionsButton, &QPushButton::clicked, this, [this] { setSidebarVisible(true); });

    return page;
}

void MainWindow::buildActions()
{
    auto add = [this](const QString &key, const QString &text,
                      const QList<QKeySequence> &shortcuts,
                      std::function<void()> fn, bool checkable = false,
                      bool checked = false) -> QAction * {
        auto *action = new QAction(text, this);
        if (!shortcuts.isEmpty())
            action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WindowShortcut);
        action->setCheckable(checkable);
        action->setChecked(checked);
        connect(action, &QAction::triggered, this, [fn] { fn(); });
        m_actions.insert(key, action);
        return action;
    };

    add(QStringLiteral("newLocal"), tr("Nueva terminal local"),
        { QKeySequence(QStringLiteral("Ctrl+T")) }, [this] { newLocalTab(); });
    add(QStringLiteral("newSsh"), tr("Nueva conexión SSH…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+N")) }, [this] { promptNewSession(); });
    add(QStringLiteral("newTelnet"), tr("Nueva conexión Telnet…"), {}, [this] {
        Session s = Session::makeLocal();
        s.type = Session::Type::Telnet;
        s.name = tr("Telnet");
        s.port = 23;
        SessionDialog dlg(s, m_store ? m_store->groups() : QStringList(), this);
        if (dlg.exec() == QDialog::Accepted && m_store) {
            Session result = dlg.resultSession();
            if (m_store->addSession(result))
                connectSession(result.id);
        }
    });
    add(QStringLiteral("importSshConfig"), tr("Importar desde ~/.ssh/config…"), {},
        [this] { importSshConfig(); });
    add(QStringLiteral("exportSshConfig"), tr("Exportar a ~/.ssh/config…"), {},
        [this] { exportSshConfig(); });
    add(QStringLiteral("knownHosts"), tr("Ver known_hosts…"), {},
        [this] { showKnownHosts(); });
    add(QStringLiteral("sshAgent"), tr("Agente SSH…"), {},
        [this] { showSshAgent(); });
    add(QStringLiteral("snippets"), tr("Snippets…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+S")) },
        [this] { showSnippets(); });
    add(QStringLiteral("tunnels"), tr("Túneles SSH…"), {},
        [this] { showTunnels(); });
    add(QStringLiteral("sftp"), tr("Explorador SFTP…"), {},
        [this] { showSftp(); });
    add(QStringLiteral("networkDiscovery"), tr("Descubrimiento de red…"), {},
        [this] { showNetworkDiscovery(); });
    add(QStringLiteral("closeTab"), tr("Cerrar pestaña"),
        { QKeySequence(QStringLiteral("Ctrl+W")) }, [this] { closeTabAt(m_tabs->currentIndex(), true); });
    add(QStringLiteral("closePane"), tr("Cerrar panel dividido"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+W")) }, [this] { closeCurrentPane(); });

    add(QStringLiteral("copy"), tr("Copiar"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+C")) }, [this] {
        if (TerminalWidget *term = activeTerminal()) {
            if (!term->selectedText().isEmpty()) {
                term->copySelection();
                statusBar()->showMessage(tr("Texto copiado"), 1800);
            }
        }
    });
    add(QStringLiteral("paste"), tr("Pegar"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+V")) }, [this] {
        if (TerminalWidget *term = activeTerminal())
            term->pasteClipboard();
    });
    add(QStringLiteral("selectAll"), tr("Select All"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+A")) }, [this] {
        if (TerminalWidget *term = activeTerminal())
            term->selectAll();
    });

    add(QStringLiteral("search"), tr("Buscar en la terminal…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+F")) }, [this] {
        if (m_searchBar->isVisible())
            hideSearch();
        else
            showSearch();
    });
    add(QStringLiteral("globalSearch"), tr("Find in all tabs…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+G")) },
        [this] { showGlobalSearch(); });
    add(QStringLiteral("broadcast"), tr("Broadcast input"),
        {}, [this] { setBroadcastEnabled(!m_broadcast || !m_broadcast->isEnabled() ? true : false); },
        true, false);
    // Keep action state in sync through toggled.
    if (QAction *bc = m_actions.value(QStringLiteral("broadcast"))) {
        disconnect(bc, &QAction::triggered, nullptr, nullptr);
        connect(bc, &QAction::toggled, this, [this](bool on) { setBroadcastEnabled(on); });
    }
    add(QStringLiteral("clear"), tr("Clear history"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+K")) }, [this] {
        if (TerminalWidget *term = activeTerminal()) {
            term->clearScrollback();
            statusBar()->showMessage(tr("Historial borrado"), 1800);
        }
    });
    add(QStringLiteral("reset"), tr("Reset terminal"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+R")) }, [this] {
        if (TerminalWidget *term = activeTerminal())
            term->resetTerminal();
    });

    add(QStringLiteral("splitRight"), tr("Dividir a la derecha"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+D")) },
        [this] { splitActive(Qt::Horizontal); });
    add(QStringLiteral("splitDown"), tr("Dividir abajo"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+E")) },
        [this] { splitActive(Qt::Vertical); });

    add(QStringLiteral("zoomIn"), tr("Zoom in"),
        { QKeySequence::ZoomIn, QKeySequence(QStringLiteral("Ctrl+=")) },
        [this] { adjustFontSize(1); });
    add(QStringLiteral("zoomOut"), tr("Zoom out"),
        { QKeySequence::ZoomOut, QKeySequence(QStringLiteral("Ctrl+-")) },
        [this] { adjustFontSize(-1); });
    add(QStringLiteral("zoomReset"), tr("Tamaño normal"),
        { QKeySequence(QStringLiteral("Ctrl+0")) }, [this] { resetFontSize(); });

    m_toggleSidebarAction = add(QStringLiteral("toggleSidebar"),
                                tr("Sesiones"),
                                { QKeySequence(QStringLiteral("Ctrl+B")) },
                                [this] { setSidebarVisible(!m_sidebarVisible); },
                                true, m_sidebarVisible);
    m_toggleSidebarAction->setObjectName(QStringLiteral("ToggleSidebarAction"));
    m_toggleSidebarAction->setToolTip(tr("Mostrar u ocultar el panel de sesiones (Ctrl+B)"));

    m_darkThemeAction = add(QStringLiteral("darkTheme"), tr("Tema oscuro"), {},
                            [this] { applyTheme(); }, true, m_darkTheme);
    // The theme action is wired through toggled below so that its state is the
    // source of truth.
    connect(m_darkThemeAction, &QAction::toggled, this, [this](bool on) {
        m_darkTheme = on;
        applyTheme();
    });

    add(QStringLiteral("fullscreen"), tr("Pantalla completa"),
        { QKeySequence(Qt::Key_F11) }, [this] {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });
    add(QStringLiteral("commandPalette"), tr("Command Palette…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+P")),
          QKeySequence(QStringLiteral("Ctrl+K")) },
        [this] { showCommandPalette(); });
    add(QStringLiteral("prefs"), tr("Preferencias…"),
        { QKeySequence(QStringLiteral("Ctrl+,")) }, [this] { showPreferences(); });
    add(QStringLiteral("quit"), tr("Quit"), { QKeySequence::Quit },
        [this] { close(); });

    add(QStringLiteral("nextTab"), tr("Pestaña siguiente"),
        { QKeySequence(QStringLiteral("Ctrl+Tab")) }, [this] { nextTab(1); });
    add(QStringLiteral("prevTab"), tr("Pestaña anterior"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+Tab")) }, [this] { nextTab(-1); });

    add(QStringLiteral("about"), tr("Acerca de QTerm2"), {}, [this] { showAbout(); });
    add(QStringLiteral("shortcuts"), tr("Atajos de teclado"), {}, [this] { showShortcuts(); });

    // Ctrl+1..9 selects a tab directly.
    for (int i = 1; i <= 9; ++i) {
        auto *action = new QAction(this);
        action->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i)));
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, this, [this, i] {
            if (i < 9) {
                if (i <= m_tabs->count())
                    m_tabs->setCurrentIndex(i - 1);
            } else if (m_tabs->count() > 0) {
                m_tabs->setCurrentIndex(m_tabs->count() - 1);
            }
        });
        addAction(action);
    }

    for (QAction *action : m_actions)
        addAction(action);
}

void MainWindow::buildMenus()
{
    menuBar()->clear();
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actions.value(QStringLiteral("closePane")));
    fileMenu->addAction(m_actions.value(QStringLiteral("closeTab")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("quit")));

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actions.value(QStringLiteral("copy")));
    editMenu->addAction(m_actions.value(QStringLiteral("paste")));
    editMenu->addAction(m_actions.value(QStringLiteral("selectAll")));
    editMenu->addSeparator();
    editMenu->addAction(m_actions.value(QStringLiteral("search")));
    editMenu->addAction(m_actions.value(QStringLiteral("globalSearch")));
    editMenu->addSeparator();
    editMenu->addAction(m_actions.value(QStringLiteral("commandPalette")));
    auto *langMenu = editMenu->addMenu(tr("Language"));
    const QString currentLang = m_settings
        ? m_settings->value(QStringLiteral("ui/language"),
                            QStringLiteral("en")).toString()
        : QStringLiteral("en");
    auto *langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);
    auto *english = langMenu->addAction(QStringLiteral("English"));
    auto *spanish = langMenu->addAction(QStringLiteral("Español"));
    english->setCheckable(true);
    spanish->setCheckable(true);
    langGroup->addAction(english);
    langGroup->addAction(spanish);
    (currentLang == QLatin1String("es") ? spanish : english)->setChecked(true);
    const auto chooseLanguage = [this](const QString &lang) {
        const QString current = m_settings
            ? m_settings->value(QStringLiteral("ui/language"),
                                QStringLiteral("en")).toString()
            : QStringLiteral("en");
        if (current == lang)
            return;
        if (m_settings)
            m_settings->setValue(QStringLiteral("ui/language"), lang);
        statusBar()->showMessage(
            tr("Language saved (%1). Restart to apply.").arg(lang), 3000);
    };
    connect(english, &QAction::triggered, this,
            [chooseLanguage] { chooseLanguage(QStringLiteral("en")); });
    connect(spanish, &QAction::triggered, this,
            [chooseLanguage] { chooseLanguage(QStringLiteral("es")); });
    editMenu->addAction(m_actions.value(QStringLiteral("prefs")));

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addAction(m_darkThemeAction);
    viewMenu->addAction(m_actions.value(QStringLiteral("broadcast")));
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions.value(QStringLiteral("nextTab")));
    viewMenu->addAction(m_actions.value(QStringLiteral("prevTab")));
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions.value(QStringLiteral("fullscreen")));

    m_sessionsMenu = menuBar()->addMenu(tr("&Sessions"));
    updateSessionsMenu();

    auto *terminalMenu = menuBar()->addMenu(tr("&Terminal"));
    terminalMenu->addAction(m_actions.value(QStringLiteral("splitRight")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("splitDown")));
    terminalMenu->addSeparator();
    terminalMenu->addAction(m_actions.value(QStringLiteral("clear")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("reset")));
    terminalMenu->addSeparator();
    terminalMenu->addAction(m_actions.value(QStringLiteral("tunnels")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("sftp")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("networkDiscovery")));
    terminalMenu->addSeparator();
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomIn")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomOut")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomReset")));

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actions.value(QStringLiteral("shortcuts")));
    helpMenu->addAction(m_actions.value(QStringLiteral("about")));
}

void MainWindow::buildToolbar()
{
    auto *toolbar = addToolBar(tr("Principal"));
    toolbar->setObjectName(QStringLiteral("MainToolBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    toolbar->addWidget(toolbarButton(m_toggleSidebarAction, QStringLiteral("\u2630"), toolbar));
    toolbar->addSeparator();
    toolbar->addWidget(toolbarButton(m_actions.value(QStringLiteral("splitRight")),
                                     QStringLiteral("\u25eb"), toolbar));
    toolbar->addWidget(toolbarButton(m_actions.value(QStringLiteral("splitDown")),
                                     QStringLiteral("\u2b12"), toolbar));
    toolbar->addSeparator();
    toolbar->addWidget(toolbarButton(m_actions.value(QStringLiteral("search")),
                                     QStringLiteral("\u2315"), toolbar));

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
}

void MainWindow::buildStatusBar()
{
    m_statusLeft = new QLabel(this);
    m_statusLeft->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusRight = new QLabel(this);
    m_statusRight->setObjectName(QStringLiteral("Hint"));
    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusRight);
    statusBar()->setSizeGripEnabled(true);

    m_statusBarManager = new qterm2::StatusBarManager(this);
    m_statusBarManager->buildStatusBar(statusBar());
}

// -------------------------------------------------------------- appearance

void MainWindow::applyTheme()
{
    Theme::setCurrentMode(m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light);
    if (m_darkThemeAction && m_darkThemeAction->isChecked() != m_darkTheme)
        m_darkThemeAction->setChecked(m_darkTheme);
    applyTerminalAppearance();
    if (m_sidebar)
        m_sidebar->repaintItems();
    m_settings->setValue(QStringLiteral("ui/darkTheme"), m_darkTheme);
    updateStatus();
}

void MainWindow::applyTerminalAppearance()
{
    const Theme::TerminalColors colors =
        Theme::TerminalColors::forMode(m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light);
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
        if (pane)
            pane->applyAppearance(colors, m_fontFamily, m_fontSize, m_scrollback,
                                  m_cursorBlink, m_cursorStyle);
    }
    updateZoomHint();
}

void MainWindow::updateZoomHint()
{
    if (m_statusRight) {
        m_statusRight->setText(tr("xterm-256color · UTF-8 · %1 pt · Ctrl+B panel")
                                   .arg(m_fontSize));
    }
}

void MainWindow::adjustFontSize(int delta)
{
    const int value = qBound(6, m_fontSize + delta, 40);
    if (value == m_fontSize)
        return;
    m_fontSize = value;
    m_settings->setValue(QStringLiteral("terminal/fontSize"), m_fontSize);
    applyTerminalAppearance();
    statusBar()->showMessage(tr("Tamaño de fuente: %1 pt").arg(m_fontSize), 1400);
}

void MainWindow::resetFontSize()
{
    m_fontSize = 11;
    m_settings->setValue(QStringLiteral("terminal/fontSize"), m_fontSize);
    applyTerminalAppearance();
    statusBar()->showMessage(tr("Tamaño de fuente restaurado"), 1400);
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(m_fontFamily, m_fontSize, m_scrollback, m_cursorStyle,
                             m_cursorBlink, m_localShell, m_darkTheme,
                             m_settings->value(QStringLiteral("ui/language"),
                                               QStringLiteral("en")).toString(),
                             this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_fontFamily = dialog.fontFamily();
    m_fontSize = dialog.fontSize();
    m_scrollback = dialog.scrollback();
    m_cursorStyle = dialog.cursorStyle();
    m_cursorBlink = dialog.blink();
    m_localShell = dialog.shell().isEmpty() ? defaultShellPath() : dialog.shell();
    m_darkTheme = dialog.darkTheme();
    const QString lang = dialog.language();
    if (!lang.isEmpty()) {
        m_settings->setValue(QStringLiteral("ui/language"), lang);
        // Language switching requires restart in this setup; notify user
        statusBar()->showMessage(tr("Language saved (%1). Restart to apply.").arg(lang), 3000);
    }

    saveSettings();
    applyTheme();
    statusBar()->showMessage(tr("Preferencias guardadas"), 2200);
}

// --------------------------------------------------------------- navigation

TerminalPane *MainWindow::currentPane() const
{
    return qobject_cast<TerminalPane *>(m_tabs->currentWidget());
}

TerminalWidget *MainWindow::activeTerminal() const
{
    TerminalPane *pane = currentPane();
    return pane ? pane->activeTerminal() : nullptr;
}

void MainWindow::nextTab(int delta)
{
    if (m_tabs->count() < 2)
        return;
    int index = m_tabs->currentIndex() + delta;
    if (index < 0)
        index = m_tabs->count() - 1;
    if (index >= m_tabs->count())
        index = 0;
    m_tabs->setCurrentIndex(index);
}

void MainWindow::closeTabAt(int index, bool confirm)
{
    if (index < 0 || index >= m_tabs->count())
        return;
    auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(index));
    if (!pane)
        return;

    if (confirm && pane->anyRunning()) {
        const auto answer = QMessageBox::question(
            this, tr("Cerrar sesión"),
            tr("«%1» todavía tiene un proceso en ejecución.\n¿Cerrar de todos modos?")
                .arg(pane->sessionName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    m_tabs->removeTab(index);
    pane->deleteLater();
    updateWelcomeState();
    updateWindowTitle();
    updateStatus();
}

void MainWindow::closeCurrentPane()
{
    TerminalPane *pane = currentPane();
    if (!pane)
        return;

    if (pane->terminals().size() > 1) {
        if (pane->anyRunning() && activeTerminal() && activeTerminal()->isRunning()) {
            const auto answer = QMessageBox::question(
                this, tr("Cerrar panel dividido"),
                tr("Este panel tiene un proceso en ejecución.\n¿Cerrarlo?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
        pane->closeActivePane();
        updateStatus();
    } else {
        closeTabAt(m_tabs->currentIndex(), true);
    }
}

void MainWindow::splitActive(Qt::Orientation orientation)
{
    TerminalPane *pane = currentPane();
    if (!pane)
        return;
    QString error;
    if (!pane->split(orientation, &error)) {
        QMessageBox::warning(this, tr("No se pudo dividir el panel"), error);
        return;
    }
    updateStatus();
    statusBar()->showMessage(tr("Panel dividido"), 1600);
}



// ------------------------------------------------------------------- state

void MainWindow::setSidebarVisible(bool visible)
{
    if (!m_sidebar)
        return;
    m_sidebarVisible = visible;
    m_sidebar->setVisible(visible);

    if (m_toggleSidebarAction) {
        QSignalBlocker blocker(m_toggleSidebarAction);
        m_toggleSidebarAction->setChecked(visible);
    }
    if (visible) {
        const QList<int> sizes = m_splitter->sizes();
        if (sizes.isEmpty() || sizes.value(0) < 120) {
            const int width = qMax(210, m_sidebarWidth);
            m_splitter->setSizes({ width, qMax(320, this->width() - width) });
        }
    }
    m_settings->setValue(QStringLiteral("ui/sidebarVisible"), visible);
}

void MainWindow::updateWelcomeState()
{
    if (!m_stack || !m_tabs)
        return;
    const bool empty = m_tabs->count() == 0;
    m_stack->setCurrentWidget(empty ? m_welcome : static_cast<QWidget *>(m_tabs));
    updateWindowTitle();
    updateStatus();
}

void MainWindow::updateWindowTitle()
{
    TerminalPane *pane = currentPane();
    if (!pane) {
        setWindowTitle(QStringLiteral("QTerm2"));
        return;
    }
    setWindowTitle(QStringLiteral("%1 — QTerm2").arg(pane->sessionName()));
}

void MainWindow::updateStatus()
{
    TerminalPane *pane = currentPane();
    if (!pane) {
        m_statusLeft->setText(tr("Sin sesiones activas · Ctrl+T abre una terminal"));
        return;
    }
    const Session &s = pane->session();
    QString left = s.name + QStringLiteral("  ·  ")
                   + (s.isLocal() ? tr("Terminal local") : s.displayTarget());
    if (!s.isLocal()) {
        left += QStringLiteral("  ·  ") + Session::authName(s.auth);
    }
    if (TerminalWidget *term = pane->activeTerminal()) {
        const QString title = term->title();
        if (!title.isEmpty() && title != s.name)
            left += QStringLiteral("  —  ") + title;
        if (!term->isRunning())
            left += QStringLiteral("  ·  ") + tr("proceso finalizado");
    }
    if (pane->terminals().size() > 1)
        left += tr("  ·  %1 paneles").arg(pane->terminals().size());
    m_statusLeft->setText(left);
    if (m_statusBarManager)
        m_statusBarManager->updateComponents(pane->session());
    updateZoomHint();
}

void MainWindow::setBroadcastEnabled(bool enabled)
{
    if (m_broadcast)
        m_broadcast->setEnabled(enabled);
    if (QAction *action = m_actions.value(QStringLiteral("broadcast"))) {
        if (action->isChecked() != enabled)
            action->setChecked(enabled);
    }
    statusBar()->showMessage(
        enabled ? tr("Broadcast activado: la entrada se envía a todas las terminales")
                : tr("Broadcast desactivado"),
        2500);
}

void MainWindow::connectTerminalBroadcast(TerminalWidget *terminal)
{
    if (!terminal)
        return;
    connect(terminal, &TerminalWidget::userInput, this, [this, terminal](const QByteArray &data) {
        if (m_broadcastingInput || !m_broadcast || !m_broadcast->isEnabled())
            return;
        m_broadcastingInput = true;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
            if (!pane)
                continue;
            for (TerminalWidget *other : pane->terminals()) {
                if (other == terminal || !other)
                    continue;
                // Write directly to avoid re-emitting userInput.
                if (other->isRunning())
                    other->sendText(QString::fromUtf8(data));
            }
        }
        m_broadcastingInput = false;
    }, Qt::UniqueConnection);
}

void MainWindow::showTunnels()
{
    if (!m_tunnels)
        return;
    TunnelDialog dialog(m_tunnels, m_store, this);
    dialog.exec();
}

void MainWindow::showSftp()
{
    SftpDialog dialog(m_store, this);
    dialog.exec();
}

void MainWindow::showNetworkDiscovery()
{
    NetworkDiscoveryDialog dialog(this);
    connect(&dialog, &NetworkDiscoveryDialog::createSessionRequested, this, [this](const QString &hostOrAddress) {
        if (!m_store)
            return;
        Session base;
        base.type = Session::Type::Ssh;
        base.group = m_store->suggestGroup();
        base.port = 22;
        base.host = hostOrAddress.trimmed();
        base.name = base.host;
        base.keepAlive = true;
        base.connectTimeout = 10;
        base.hostKeyPolicy = Session::HostKeyPolicy::Ask;
        base.auth = Session::Auth::PublicKey;
        base.shell = m_localShell;

        const QStringList keys = qterm2::ssh::KeyManager::listPrivateKeys();
        for (const QString &preferred : { QStringLiteral("id_ed25519"), QStringLiteral("id_rsa"),
                                          QStringLiteral("id_ecdsa") }) {
            for (const QString &key : keys) {
                if (QFileInfo(key).fileName() == preferred) { base.privateKey = key; break; }
            }
            if (!base.privateKey.isEmpty()) break;
        }
        if (base.privateKey.isEmpty() && !keys.isEmpty())
            base.privateKey = keys.first();

        SessionDialog dlg(base, m_store->groups(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        Session session = dlg.resultSession();
        if (!m_store->addSession(session)) {
            QMessageBox::warning(this, tr("No se pudo guardar la sesión"), m_store->lastError());
            return;
        }
        m_sidebar->selectSession(session.id);
        addSessionTab(session);
    });
    dialog.exec();
}
