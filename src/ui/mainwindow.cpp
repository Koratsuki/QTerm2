#include "ui/mainwindow.h"

using qterm2::SessionStore;

#include "app/applicationcontext.h"
#include "sessions/sessionlauncher.h"
#include "sessions/sessionstore.h"
#include "sessions/snippetstore.h"
#include "ssh/keymanager.h"
#include "ssh/knownhosts.h"
#include "ssh/sshagent.h"
#include "ssh/sshcommandbuilder.h"
#include "ssh/sshconfig.h"
#include "storage/settingsstore.h"
#include "terminal/terminalwidget.h"
#include "ui/commandpalettedialog.h"
#include "ui/globalsearchdialog.h"
#include "ui/knownhostsdialog.h"
#include "ui/sessiondialog.h"
#include "ui/sidebar.h"
#include "ui/snippetdialog.h"
#include "ui/sshagentdialog.h"
#include "ui/sshconfigdialog.h"
#include "ui/terminalpane.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

constexpr int kMaxReconnectAttempts = 5;
constexpr int kReconnectStableMs = 30000;

QIcon dotIcon(const QString &colorTag, int size = 12)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Session::colorForTag(colorTag)));
    p.drawEllipse(QRectF(0, 0, size, size).adjusted(1, 1, -1, -1));
    p.end();
    return QIcon(pm);
}

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

class PreferencesDialog : public QDialog
{
public:
    PreferencesDialog(const QString &fontFamily, int fontSize, int scrollback,
                      const QString &cursorStyle, bool blink,
                      const QString &shell, bool dark, QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("Preferencias de QTerm2"));
        setMinimumWidth(520);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setSpacing(10);

        m_font = new QFontComboBox(this);
        m_font->setFontFilters(QFontComboBox::MonospacedFonts);
        const QString family = fontFamily.isEmpty()
                                   ? QFontDatabase::systemFont(QFontDatabase::FixedFont).family()
                                   : fontFamily;
        m_font->setCurrentFont(QFont(family));
        form->addRow(tr("Fuente"), m_font);

        m_size = new QSpinBox(this);
        m_size->setRange(6, 40);
        m_size->setValue(fontSize);
        m_size->setSuffix(tr(" pt"));
        form->addRow(tr("Tamaño"), m_size);

        m_scrollback = new QSpinBox(this);
        m_scrollback->setRange(0, 200000);
        m_scrollback->setSingleStep(1000);
        m_scrollback->setSuffix(tr(" líneas"));
        m_scrollback->setValue(scrollback);
        form->addRow(tr("Historial"), m_scrollback);

        m_cursor = new QComboBox(this);
        m_cursor->addItem(tr("Bloque"), QStringLiteral("block"));
        m_cursor->addItem(tr("I-Beam (barra vertical)"), QStringLiteral("ibeam"));
        m_cursor->addItem(tr("Subrayado"), QStringLiteral("underscore"));
        const QString style = TerminalWidget::cursorStyleName(
            TerminalWidget::cursorStyleFromName(cursorStyle));
        m_cursor->setCurrentIndex(qMax(0, m_cursor->findData(style)));
        form->addRow(tr("Forma del cursor"), m_cursor);

        m_blink = new QCheckBox(tr("Cursor parpadeante"), this);
        m_blink->setChecked(blink);
        m_blink->setToolTip(tr("Desactívalo para mantener el cursor siempre visible."));
        form->addRow(QString(), m_blink);

        m_shell = new QLineEdit(shell, this);
        auto *browse = new QPushButton(tr("Examinar…"), this);
        auto *shellRow = new QWidget(this);
        auto *shellLayout = new QHBoxLayout(shellRow);
        shellLayout->setContentsMargins(0, 0, 0, 0);
        shellLayout->setSpacing(6);
        shellLayout->addWidget(m_shell, 1);
        shellLayout->addWidget(browse);
        form->addRow(tr("Shell local"), shellRow);

        m_theme = new QComboBox(this);
        m_theme->addItem(tr("Oscuro"), true);
        m_theme->addItem(tr("Claro"), false);
        m_theme->setCurrentIndex(dark ? 0 : 1);
        form->addRow(tr("Tema"), m_theme);

        layout->addLayout(form);

        auto *hint = new QLabel(tr("Los cambios se aplican a todas las terminales abiertas. "
                                   "Las sesiones SSH activas no se reconectan."), this);
        hint->setWordWrap(true);
        hint->setObjectName(QStringLiteral("Hint"));
        layout->addWidget(hint);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
        auto *save = buttons->addButton(tr("Guardar"), QDialogButtonBox::AcceptRole);
        save->setDefault(true);
        layout->addWidget(buttons);

        connect(browse, &QPushButton::clicked, this, [this] {
            const QString file = QFileDialog::getOpenFileName(
                this, tr("Seleccionar shell"), QStringLiteral("/bin"));
            if (!file.isEmpty())
                m_shell->setText(file);
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
    }

    QString fontFamily() const { return m_font->currentFont().family(); }
    int fontSize() const { return m_size->value(); }
    int scrollback() const { return m_scrollback->value(); }
    QString cursorStyle() const { return m_cursor->currentData().toString(); }
    bool blink() const { return m_blink->isChecked(); }
    QString shell() const { return m_shell->text().trimmed(); }
    bool darkTheme() const { return m_theme->currentData().toBool(); }

private:
    QFontComboBox *m_font = nullptr;
    QSpinBox *m_size = nullptr;
    QSpinBox *m_scrollback = nullptr;
    QComboBox *m_cursor = nullptr;
    QCheckBox *m_blink = nullptr;
    QLineEdit *m_shell = nullptr;
    QComboBox *m_theme = nullptr;
};

} // namespace

// ---------------------------------------------------------------- lifecycle

MainWindow::MainWindow(qterm2::ApplicationContext *context, QWidget *parent)
    : QMainWindow(parent)
    , m_context(context)
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
                this, tr("Cerrar QTerm2"),
                tr("Hay %1 sesiones activas. ¿Cerrar de todos modos?").arg(running),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
    }
    saveSettings();
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
    cornerMenu->addAction(tr("Nueva conexión SSH…"), this, &MainWindow::promptNewSession);
    corner->setMenu(cornerMenu);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);

    m_splitter->addWidget(right);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ m_sidebarWidth, 960 });
    setCentralWidget(m_splitter);

    // ---- signals ---------------------------------------------------------
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
        tr("Terminal xterm-256color · Gestor de sesiones SSH · Pestañas y paneles"), page);
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
    add(QStringLiteral("closeTab"), tr("Cerrar pestaña"),
        { QKeySequence(QStringLiteral("Ctrl+W")) }, [this] { closeTabAt(m_tabs->currentIndex(), true); });
    add(QStringLiteral("closePane"), tr("Cerrar panel"),
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
    add(QStringLiteral("selectAll"), tr("Seleccionar todo"),
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
    add(QStringLiteral("globalSearch"), tr("Buscar en todas las pestañas…"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+G")) },
        [this] { showGlobalSearch(); });
    add(QStringLiteral("clear"), tr("Limpiar historial"),
        { QKeySequence(QStringLiteral("Ctrl+Shift+K")) }, [this] {
        if (TerminalWidget *term = activeTerminal()) {
            term->clearScrollback();
            statusBar()->showMessage(tr("Historial borrado"), 1800);
        }
    });
    add(QStringLiteral("reset"), tr("Reiniciar terminal"),
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

    add(QStringLiteral("zoomIn"), tr("Aumentar tamaño"),
        { QKeySequence::ZoomIn, QKeySequence(QStringLiteral("Ctrl+=")) },
        [this] { adjustFontSize(1); });
    add(QStringLiteral("zoomOut"), tr("Reducir tamaño"),
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
        { QKeySequence(QStringLiteral("Ctrl+Shift+P")) },
        [this] { showCommandPalette(); });
    add(QStringLiteral("prefs"), tr("Preferencias…"),
        { QKeySequence(QStringLiteral("Ctrl+,")) }, [this] { showPreferences(); });
    add(QStringLiteral("quit"), tr("Salir"), { QKeySequence::Quit },
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
    auto *fileMenu = menuBar()->addMenu(tr("&Archivo"));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("closePane")));
    fileMenu->addAction(m_actions.value(QStringLiteral("closeTab")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("quit")));

    auto *editMenu = menuBar()->addMenu(tr("&Editar"));
    editMenu->addAction(m_actions.value(QStringLiteral("copy")));
    editMenu->addAction(m_actions.value(QStringLiteral("paste")));
    editMenu->addAction(m_actions.value(QStringLiteral("selectAll")));
    editMenu->addSeparator();
    editMenu->addAction(m_actions.value(QStringLiteral("search")));
    editMenu->addAction(m_actions.value(QStringLiteral("globalSearch")));
    editMenu->addSeparator();
    editMenu->addAction(m_actions.value(QStringLiteral("commandPalette")));
    editMenu->addAction(m_actions.value(QStringLiteral("prefs")));

    m_sessionsMenu = menuBar()->addMenu(tr("&Sesiones"));
    updateSessionsMenu();

    auto *terminalMenu = menuBar()->addMenu(tr("&Terminal"));
    terminalMenu->addAction(m_actions.value(QStringLiteral("splitRight")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("splitDown")));
    terminalMenu->addSeparator();
    terminalMenu->addAction(m_actions.value(QStringLiteral("clear")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("reset")));
    terminalMenu->addSeparator();
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomIn")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomOut")));
    terminalMenu->addAction(m_actions.value(QStringLiteral("zoomReset")));

    auto *viewMenu = menuBar()->addMenu(tr("&Ver"));
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addAction(m_darkThemeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions.value(QStringLiteral("nextTab")));
    viewMenu->addAction(m_actions.value(QStringLiteral("prevTab")));
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions.value(QStringLiteral("fullscreen")));

    auto *toolsMenu = menuBar()->addMenu(tr("&Herramientas"));
    toolsMenu->addAction(tr("Túneles..."));
    connect(toolsMenu->actions().last(), &QAction::triggered, this, &MainWindow::showTunnels);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Plugins..."));
    connect(toolsMenu->actions().last(), &QAction::triggered, this, &MainWindow::showPlugins);

    auto *helpMenu = menuBar()->addMenu(tr("A&yuda"));
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
    toolbar->addWidget(toolbarButton(m_actions.value(QStringLiteral("newLocal")),
                                     QStringLiteral("+"), toolbar));
    toolbar->addWidget(toolbarButton(m_actions.value(QStringLiteral("newSsh")),
                                     QStringLiteral("SSH"), toolbar));
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
    m_tunnelManager = new qterm2::TunnelManager(this);
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
                             m_cursorBlink, m_localShell, m_darkTheme, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_fontFamily = dialog.fontFamily();
    m_fontSize = dialog.fontSize();
    m_scrollback = dialog.scrollback();
    m_cursorStyle = dialog.cursorStyle();
    m_cursorBlink = dialog.blink();
    m_localShell = dialog.shell().isEmpty() ? defaultShellPath() : dialog.shell();
    m_darkTheme = dialog.darkTheme();

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
                this, tr("Cerrar panel"),
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

// ---------------------------------------------------------------- sessions

void MainWindow::newLocalTab()
{
    Session local = Session::makeLocal();
    local.name = tr("Terminal local");
    local.shell = m_localShell;
    local.startDir = QDir::homePath();
    local.colorTag = QStringLiteral("green");
    addSessionTab(local);
}

TerminalPane *MainWindow::addSessionTab(const Session &session)
{
    auto *pane = new TerminalPane(session, m_tabs);
    pane->applyAppearance(Theme::TerminalColors::forMode(
                              m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light),
                          m_fontFamily, m_fontSize, m_scrollback, m_cursorBlink,
                          m_cursorStyle);
    QString error;
    if (!pane->start(&error)) {
        delete pane;
        QMessageBox::warning(this, tr("No se pudo abrir la sesión"), error);
        return nullptr;
    }

    const int index = m_tabs->addTab(pane, session.name);
    m_tabs->setTabIcon(index, dotIcon(session.colorTag));
    wirePane(pane);
    refreshTabInfo(pane);
    m_tabs->setCurrentIndex(index);
    updateWelcomeState();
    updateStatus();

    statusBar()->showMessage(session.isLocal()
                                 ? tr("Terminal local iniciada")
                                 : tr("Conectando a %1…").arg(session.displayTarget()),
                             3500);
    return pane;
}

TerminalPane *MainWindow::addCommandTab(const QString &title, const Session &session,
                                        const QString &program, const QStringList &args)
{
    Session display = session;
    display.name = title;
    auto *pane = new TerminalPane(display, m_tabs);
    pane->applyAppearance(Theme::TerminalColors::forMode(
                              m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light),
                          m_fontFamily, m_fontSize, m_scrollback, m_cursorBlink,
                          m_cursorStyle);
    QString error;
    if (!pane->startCommand(program, args, QDir::homePath(), &error)) {
        delete pane;
        QMessageBox::warning(this, tr("No se pudo ejecutar el comando"), error);
        return nullptr;
    }
    const int index = m_tabs->addTab(pane, title);
    m_tabs->setTabIcon(index, dotIcon(session.colorTag));
    wirePane(pane);
    refreshTabInfo(pane);
    m_tabs->setCurrentIndex(index);
    updateWelcomeState();
    return pane;
}

void MainWindow::wirePane(TerminalPane *pane)
{
    connect(pane, &TerminalPane::activeTerminalChanged, this, [this, pane](TerminalWidget *) {
        if (currentPane() == pane)
            updateStatus();
    });
    connect(pane, &TerminalPane::titleChanged, this, [this, pane] {
        refreshTabInfo(pane);
        if (currentPane() == pane)
            updateStatus();
    });
    connect(pane, &TerminalPane::terminalFinished, this, [this, pane](TerminalWidget *terminal) {
        if (currentPane() == pane) {
            statusBar()->showMessage(tr("El proceso terminó. La pestaña sigue abierta."), 4000);
            updateStatus();
        }
        refreshTabInfo(pane);
        if (!m_closing && pane->session().autoReconnect && !pane->session().isLocal())
            scheduleReconnect(pane, terminal);
    });
    connect(pane, &TerminalPane::bellRang, this, [this](TerminalWidget *) {
        QApplication::beep();
    });
    connect(pane, &TerminalPane::lastPaneClosed, this, [this, pane] {
        const int index = m_tabs->indexOf(pane);
        if (index >= 0)
            closeTabAt(index, false);
    });
}

void MainWindow::scheduleReconnect(TerminalPane *pane, TerminalWidget *terminal)
{
    if (!pane || !terminal || m_closing || m_tabs->indexOf(pane) < 0)
        return;
    if (pane->session().isLocal() || !pane->session().autoReconnect)
        return;

    int attempt = m_reconnectAttempts.value(terminal, 0);
    if (attempt >= kMaxReconnectAttempts) {
        m_reconnectAttempts.remove(terminal);
        if (QTimer *timer = m_reconnectTimers.take(terminal))
            timer->deleteLater();
        statusBar()->showMessage(
            tr("No se pudo reconectar a %1 tras %2 intentos.")
                .arg(pane->session().displayTarget())
                .arg(kMaxReconnectAttempts),
            8000);
        return;
    }

    ++attempt;
    m_reconnectAttempts.insert(terminal, attempt);
    const int delaySeconds = qMin(30, 1 << (attempt - 1));

    QTimer *timer = m_reconnectTimers.value(terminal);
    if (!timer) {
        timer = new QTimer(terminal);
        timer->setSingleShot(true);
        m_reconnectTimers.insert(terminal, timer);
        connect(timer, &QTimer::timeout, this, [this, pane, terminal] {
            if (m_closing || m_tabs->indexOf(pane) < 0)
                return;

            QString error;
            if (!pane->restartTerminal(terminal, &error)) {
                statusBar()->showMessage(tr("Reconexión fallida: %1").arg(error), 5000);
                scheduleReconnect(pane, terminal);
                return;
            }

            statusBar()->showMessage(
                tr("Reconectando a %1 (intento %2/%3)…")
                    .arg(pane->session().displayTarget())
                    .arg(m_reconnectAttempts.value(terminal))
                    .arg(kMaxReconnectAttempts),
                5000);

            // Only a connection that stays up counts as success.
            QTimer::singleShot(kReconnectStableMs, terminal, [this, terminal] {
                if (terminal->isRunning())
                    m_reconnectAttempts.remove(terminal);
            });
        });
        connect(terminal, &QObject::destroyed, this, [this, terminal] {
            m_reconnectAttempts.remove(terminal);
            m_reconnectTimers.remove(terminal);
        });
    }

    timer->start(delaySeconds * 1000);
    statusBar()->showMessage(
        tr("Conexión perdida. Reintentando en %1 s (intento %2/%3)…")
            .arg(delaySeconds)
            .arg(attempt)
            .arg(kMaxReconnectAttempts),
        6000);
}

void MainWindow::refreshTabInfo(TerminalPane *pane)
{
    const int index = m_tabs->indexOf(pane);
    if (index < 0)
        return;
    const Session &s = pane->session();
    m_tabs->setTabText(index, s.name);
    m_tabs->setTabIcon(index, dotIcon(s.colorTag));

    QStringList tips;
    tips << s.name;
    tips << (s.isLocal() ? tr("Terminal local: %1").arg(s.shell) : s.displayTarget());
    if (!s.isLocal())
        tips << tr("Autenticación: %1").arg(Session::authName(s.auth));
    tips << tr("Paneles: %1").arg(pane->terminals().size());
    if (TerminalWidget *term = pane->activeTerminal()) {
        if (!term->title().isEmpty())
            tips << term->title();
    }
    m_tabs->setTabToolTip(index, tips.join(QLatin1Char('\n')));
    updateWindowTitle();
}

void MainWindow::updateSessionsMenu()
{
    if (!m_sessionsMenu)
        return;
    m_sessionsMenu->clear();
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("newSsh")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("newLocal")));
    m_sessionsMenu->addSeparator();
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("importSshConfig")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("exportSshConfig")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("knownHosts")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("sshAgent")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("snippets")));
    m_sessionsMenu->addSeparator();

    if (!m_store || m_store->sessions().isEmpty()) {
        QAction *empty = m_sessionsMenu->addAction(tr("No hay sesiones guardadas"));
        empty->setEnabled(false);
        return;
    }

    QStringList groups = m_store->groups();
    if (groups.isEmpty())
        groups << QStringLiteral("Servidores");
    for (const QString &group : groups) {
        QMenu *sub = m_sessionsMenu->addMenu(group);
        QVector<Session> list;
        for (const Session &s : m_store->sessions()) {
            const QString g = s.group.trimmed().isEmpty() ? QStringLiteral("Servidores") : s.group.trimmed();
            if (g == group)
                list.append(s);
        }
        std::sort(list.begin(), list.end(), [](const Session &a, const Session &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        for (const Session &s : list) {
            QAction *action = sub->addAction(dotIcon(s.colorTag), s.name);
            const QString id = s.id;
            connect(action, &QAction::triggered, this, [this, id] { connectSession(id); });
        }
    }

    m_sessionsMenu->addSeparator();
    m_sessionsMenu->addAction(tr("Editar sesiones en el panel"), this, [this] {
        setSidebarVisible(true);
    });
}

void MainWindow::connectSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    addSessionTab(session);
    m_sidebar->selectSession(id);
    m_store->markUsed(id);
}

void MainWindow::promptNewSession()
{
    Session base;
    base.type = Session::Type::Ssh;
    base.group = m_store->suggestGroup();
    base.port = 22;
    base.keepAlive = true;
    base.connectTimeout = 10;
    base.hostKeyPolicy = Session::HostKeyPolicy::Ask;
    base.auth = Session::Auth::PublicKey;
    base.shell = m_localShell;

    const QStringList keys = qterm2::ssh::KeyManager::listPrivateKeys();
    for (const QString &preferred : { QStringLiteral("id_ed25519"), QStringLiteral("id_rsa"),
                                      QStringLiteral("id_ecdsa") }) {
        for (const QString &key : keys) {
            if (QFileInfo(key).fileName() == preferred) {
                base.privateKey = key;
                break;
            }
        }
        if (!base.privateKey.isEmpty())
            break;
    }
    if (base.privateKey.isEmpty() && !keys.isEmpty())
        base.privateKey = keys.first();

    SessionDialog dialog(base, m_store->groups(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session session = dialog.resultSession();
    if (!m_store->addSession(session)) {
        QMessageBox::warning(this, tr("No se pudo guardar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(session.id);
    addSessionTab(session);
}

void MainWindow::promptEditSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session current = m_store->sessionById(id);
    SessionDialog dialog(current, m_store->groups(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session updated = dialog.resultSession();
    if (!m_store->updateSession(updated)) {
        QMessageBox::warning(this, tr("No se pudo actualizar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(updated.id);

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
        if (pane && pane->session().id == updated.id) {
            pane->updateSession(updated);
            refreshTabInfo(pane);
        }
    }
    // Re-apply so an edited terminal profile takes effect without reconnecting.
    applyTerminalAppearance();
    statusBar()->showMessage(tr("Sesión actualizada"), 2000);
}

void MainWindow::duplicateSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    Session copy = m_store->sessionById(id);
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name = tr("%1 (copia)").arg(copy.name);
    copy.createdAt = QDateTime();
    if (!m_store->addSession(copy)) {
        QMessageBox::warning(this, tr("No se pudo duplicar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(copy.id);
}

void MainWindow::toggleFavorite(const QString &id)
{
    if (!m_store->contains(id))
        return;
    Session session = m_store->sessionById(id);
    session.favorite = !session.favorite;
    if (!m_store->updateSession(session)) {
        QMessageBox::warning(this, tr("No se pudo actualizar el favorito"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(id);
    statusBar()->showMessage(session.favorite
                                 ? tr("«%1» añadida a favoritos").arg(session.name)
                                 : tr("«%1» quitada de favoritos").arg(session.name),
                             2000);
}

void MainWindow::deleteSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    const auto answer = QMessageBox::question(
        this, tr("Eliminar sesión"),
        tr("¿Eliminar «%1» del panel?\nLas terminales ya abiertas no se cerrarán.")
            .arg(session.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    if (!m_store->removeSession(id)) {
        QMessageBox::warning(this, tr("No se pudo eliminar la sesión"),
                             m_store->lastError());
        return;
    }
    // Clear from recents
    statusBar()->showMessage(tr("Sesión eliminada"), 2000);
}

void MainWindow::installPublicKey(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    if (session.isLocal()) {
        QMessageBox::information(this, tr("Sesión local"),
                                 tr("Esta acción solo aplica a servidores SSH."));
        return;
    }

    const qterm2::ProcessSpec spec = qterm2::ssh::buildInstallKeySpec(session);
    if (!spec.ok) {
        QMessageBox::warning(this, tr("No se puede instalar la llave"), spec.error);
        return;
    }

    Session display = Session::makeLocal(tr("Instalar llave · %1").arg(session.name));
    display.colorTag = QStringLiteral("orange");
    TerminalPane *pane = addCommandTab(display.name, display, spec.program, spec.args);
    if (pane) {
        statusBar()->showMessage(
            tr("ssh-copy-id: escribe la contraseña del servidor si se solicita."), 6000);
    }
}

void MainWindow::copyCommand(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const QString command =
        qterm2::sessions::SessionLauncher::commandLineFor(m_store->sessionById(id));
    if (command.isEmpty()) {
        QMessageBox::warning(this, tr("Comando no disponible"),
                             tr("Revisa la configuración de la sesión (host y llave)."));
        return;
    }
    QApplication::clipboard()->setText(command);
    statusBar()->showMessage(tr("Comando SSH copiado al portapapeles"), 2500);
}

void MainWindow::importSshConfig()
{
    const QString defaultPath = QDir::home().filePath(QStringLiteral(".ssh/config"));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Importar configuración SSH"), defaultPath,
        tr("Configuración SSH (*);;Todos los archivos (*)"));
    if (path.isEmpty())
        return;

    QString error;
    const qterm2::ssh::SshConfigDocument document =
        qterm2::ssh::parseSshConfig(path, &error);
    if (document.entries.isEmpty()) {
        QMessageBox::information(
            this, tr("Sin hosts importables"),
            error.isEmpty()
                ? tr("No se encontraron bloques «Host» concretos en %1.").arg(path)
                : error);
        return;
    }

    QSet<QString> existing;
    for (const Session &session : m_store->sessions()) {
        if (!session.isLocal())
            existing.insert(session.host.trimmed().toLower());
    }

    SshConfigDialog dialog(document.entries, existing, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    qterm2::ssh::SshConfigDocument selectedDocument;
    selectedDocument.entries = dialog.selectedEntries();
    if (selectedDocument.entries.isEmpty()) {
        QMessageBox::information(this, tr("Sin selección"),
                                 tr("No se seleccionó ningún host para importar."));
        return;
    }

    int added = 0;
    for (const Session &session :
         qterm2::ssh::sessionsFromSshConfig(selectedDocument)) {
        const QString host = session.host.trimmed().toLower();
        if (existing.contains(host))
            continue;
        if (m_store->addSession(session)) {
            ++added;
            existing.insert(host);
        }
    }

    if (added == 0) {
        QMessageBox::information(this, tr("Nada importado"),
                                 tr("Todas las entradas seleccionadas ya existían."));
        return;
    }

    QString message = tr("Se importaron %1 sesiones desde %2.").arg(added).arg(path);
    if (!document.warnings.isEmpty())
        message += tr("\n\nAvisos: %1").arg(document.warnings.join(QLatin1Char('\n')));
    QMessageBox::information(this, tr("Importación completada"), message);
}

void MainWindow::exportSshConfig()
{
    const QString block = qterm2::ssh::renderSshConfig(m_store->sessions());
    if (block.isEmpty()) {
        QMessageBox::information(this, tr("Nada que exportar"),
                                 tr("No hay sesiones SSH con servidor para exportar."));
        return;
    }

    int count = 0;
    for (const Session &session : m_store->sessions()) {
        if (!session.isLocal() && !session.host.trimmed().isEmpty())
            ++count;
    }

    const QString defaultPath = QDir::home().filePath(QStringLiteral(".ssh/config"));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exportar configuración SSH"), defaultPath,
        tr("Configuración SSH (*);;Todos los archivos (*)"));
    if (path.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, tr("Exportar configuración SSH"),
        tr("Se escribirán %1 sesiones dentro del bloque gestionado por QTerm2 "
           "en:\n%2\n\nEl contenido que ya exista fuera del bloque se conserva y "
           "se crea una copia de seguridad del archivo.\n\n¿Continuar?")
            .arg(count)
            .arg(path),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!qterm2::ssh::writeManagedSshConfig(path, block, &error)) {
        QMessageBox::warning(this, tr("No se pudo exportar"), error);
        return;
    }

    QMessageBox::information(
        this, tr("Exportación completada"),
        tr("Se exportaron %1 sesiones a %2.\n\nLos cambios se aplican a nuevas "
           "conexiones de OpenSSH.")
            .arg(count)
            .arg(path));
}

void MainWindow::showKnownHosts()
{
    const QString path = QDir::home().filePath(QStringLiteral(".ssh/known_hosts"));
    if (!QFileInfo::exists(path)) {
        QMessageBox::information(
            this, tr("known_hosts no encontrado"),
            tr("Todavía no existe %1. Se creará la primera vez que conectes a "
               "un servidor con OpenSSH.")
                .arg(path));
        return;
    }

    KnownHostsDialog dialog(path, this);
    dialog.exec();
}

void MainWindow::showSshAgent()
{
    SshAgentDialog dialog(qterm2::ssh::queryAgent(), this);
    dialog.exec();
}

void MainWindow::showSnippets()
{
    if (!m_context || !m_context->snippetStore())
        return;

    const Session context = currentPane() ? currentPane()->session()
                                          : Session::makeLocal();
    SnippetDialog dialog(m_context->snippetStore(), context, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString command = dialog.commandToSend();
    if (command.isEmpty())
        return;

    TerminalWidget *term = activeTerminal();
    if (!term || !term->isRunning()) {
        statusBar()->showMessage(
            tr("Abre una terminal activa para insertar el snippet"), 3000);
        return;
    }
    term->sendText(command);
    term->setFocus();
    statusBar()->showMessage(tr("Snippet insertado en la terminal"), 2000);
}

void MainWindow::showCommandPalette()
{
    if (!m_context || !m_context->commands())
        return;

    QVector<qterm2::Command> commands;
    for (const qterm2::Command &command : m_context->commands()->commands()) {
        if (command.enabled)
            commands.append(command);
    }
    if (commands.isEmpty()) {
        statusBar()->showMessage(tr("No hay comandos disponibles"), 2500);
        return;
    }

    CommandPaletteDialog dialog(commands, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString id = dialog.selectedCommandId();
    if (id.isEmpty())
        return;
    const qterm2::Command command = m_context->commands()->command(id);
    if (command.action)
        command.action();
}

void MainWindow::showGlobalSearch()
{
    struct Target
    {
        int tabIndex = -1;
        TerminalWidget *terminal = nullptr;
    };

    QVector<Target> targets;
    QVector<GlobalSearchTarget> snapshots;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
        if (!pane)
            continue;
        const QList<TerminalWidget *> terminals = pane->terminals();
        for (int j = 0; j < terminals.size(); ++j) {
            TerminalWidget *terminal = terminals.at(j);
            QString label = m_tabs->tabText(i);
            if (terminals.size() > 1)
                label += tr(" · panel %1").arg(j + 1);
            targets.append({ i, terminal });
            snapshots.append({ label, terminal->screenText() });
        }
    }

    if (snapshots.isEmpty()) {
        statusBar()->showMessage(tr("No hay terminales abiertas"), 2500);
        return;
    }

    GlobalSearchDialog dialog(snapshots, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const int index = dialog.selectedTargetIndex();
    if (index < 0 || index >= targets.size())
        return;

    const Target &target = targets.at(index);
    m_tabs->setCurrentIndex(target.tabIndex);
    if (auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(target.tabIndex)))
        pane->activateTerminal(target.terminal);
    if (!dialog.query().isEmpty())
        target.terminal->setSearchText(dialog.query());
    target.terminal->scrollToLine(dialog.selectedLine());
    target.terminal->setFocus();
    statusBar()->showMessage(
        tr("Resultado en %1").arg(m_tabs->tabText(target.tabIndex)), 2500);
}

void MainWindow::registerCommands()
{
    if (!m_context || !m_context->commands())
        return;
    qterm2::CommandRegistry *registry = m_context->commands();

    for (const QString &id : m_registeredCommandIds)
        registry->unregisterCommand(id);
    m_registeredCommandIds.clear();

    const QHash<QString, QString> categories = {
        { QStringLiteral("newLocal"), tr("Archivo") },
        { QStringLiteral("newSsh"), tr("Archivo") },
        { QStringLiteral("closeTab"), tr("Archivo") },
        { QStringLiteral("closePane"), tr("Archivo") },
        { QStringLiteral("quit"), tr("Archivo") },
        { QStringLiteral("importSshConfig"), tr("Sesiones") },
        { QStringLiteral("exportSshConfig"), tr("Sesiones") },
        { QStringLiteral("knownHosts"), tr("Sesiones") },
        { QStringLiteral("sshAgent"), tr("Sesiones") },
        { QStringLiteral("snippets"), tr("Sesiones") },
        { QStringLiteral("copy"), tr("Editar") },
        { QStringLiteral("paste"), tr("Editar") },
        { QStringLiteral("selectAll"), tr("Editar") },
        { QStringLiteral("search"), tr("Editar") },
        { QStringLiteral("globalSearch"), tr("Editar") },
        { QStringLiteral("clear"), tr("Terminal") },
        { QStringLiteral("reset"), tr("Terminal") },
        { QStringLiteral("splitRight"), tr("Terminal") },
        { QStringLiteral("splitDown"), tr("Terminal") },
        { QStringLiteral("zoomIn"), tr("Terminal") },
        { QStringLiteral("zoomOut"), tr("Terminal") },
        { QStringLiteral("zoomReset"), tr("Terminal") },
        { QStringLiteral("toggleSidebar"), tr("Ver") },
        { QStringLiteral("darkTheme"), tr("Ver") },
        { QStringLiteral("fullscreen"), tr("Ver") },
        { QStringLiteral("nextTab"), tr("Ventana") },
        { QStringLiteral("prevTab"), tr("Ventana") },
        { QStringLiteral("commandPalette"), tr("Aplicación") },
        { QStringLiteral("prefs"), tr("Aplicación") },
        { QStringLiteral("about"), tr("Ayuda") },
        { QStringLiteral("shortcuts"), tr("Ayuda") },
    };

    for (auto it = m_actions.cbegin(); it != m_actions.cend(); ++it) {
        QAction *action = it.value();
        qterm2::Command command;
        command.id = QStringLiteral("qterm2.") + it.key();
        command.title = action->text();
        command.category = categories.value(it.key(), tr("General"));
        command.shortcut = action->shortcuts().value(0);
        command.action = [action] {
            if (action && action->isEnabled())
                action->trigger();
        };
        if (registry->registerCommand(command))
            m_registeredCommandIds.append(command.id);
    }
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

// ------------------------------------------------------------------ search

void MainWindow::showSearch()
{
    if (!currentPane())
        return;
    m_searchBar->show();
    if (TerminalWidget *term = activeTerminal()) {
        const QString selection = term->selectedText();
        if (!selection.isEmpty() && !selection.contains(QLatin1Char('\n'))
            && selection.size() < 120)
            m_searchEdit->setText(selection);
    }
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    m_searchEdit->selectAll();
}

void MainWindow::hideSearch()
{
    if (m_searchBar->isHidden())
        return;
    m_searchBar->hide();
    m_searchStatus->clear();
    if (TerminalWidget *term = activeTerminal())
        term->clearSearch();
    m_searchEdit->clear();
    if (TerminalPane *pane = currentPane())
        pane->focusActive();
}

bool MainWindow::findInTerminal(bool forward)
{
    TerminalWidget *term = activeTerminal();
    if (!term || m_searchEdit->text().isEmpty())
        return false;
    term->setSearchText(m_searchEdit->text());
    const bool found = term->findNext(forward);
    m_searchStatus->setText(found ? tr("Coincidencia encontrada") : tr("Sin coincidencias"));
    m_searchStatus->setStyleSheet(found ? QString()
                                        : QStringLiteral("color: #f87171;"));
    return found;
}

// -------------------------------------------------------------------- help

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("Acerca de QTerm2"),
        tr("<h3>QTerm2 v1.0</h3>"
           "<p>Un terminal para Linux inspirado en iTerm2, con gestor de "
           "sesiones SSH integrado.</p>"
           "<p>Emulador xterm-256color con color de 24 bits, pestañas, paneles "
           "divididos, historial desplazable, búsqueda y llaves OpenSSH.</p>"
           "<p>Qt %1 · C++17 · OpenSSH</p>")
            .arg(QString::fromLatin1(qVersion())));
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("Atajos de teclado"),
        tr("<b>Pestañas y paneles</b><br>"
           "Ctrl+T · nueva terminal local<br>"
           "Ctrl+Shift+N · nueva conexión SSH<br>"
           "Ctrl+W · cerrar pestaña<br>"
           "Ctrl+Shift+W · cerrar panel dividido<br>"
           "Ctrl+Shift+D · dividir a la derecha<br>"
           "Ctrl+Shift+E · dividir abajo<br>"
           "Ctrl+Tab / Ctrl+Shift+Tab · cambiar pestaña<br>"
           "Ctrl+1…9 · ir a la pestaña<br><br>"
           "<b>Panel y terminal</b><br>"
           "Ctrl+B · mostrar/ocultar panel lateral<br>"
           "Ctrl+Shift+F · buscar<br>"
           "F3 / Shift+F3 · siguiente/anterior coincidencia<br>"
           "Ctrl+Shift+C / Ctrl+Shift+V · copiar/pegar<br>"
           "Ctrl+Shift+A · seleccionar todo<br>"
           "Ctrl+Shift+K · limpiar historial<br>"
           "Ctrl++ / Ctrl+- / Ctrl+0 · zoom<br>"
           "Ctrl+Shift+rueda o Shift+PageUp · historial<br>"
           "Ctrl+clic · abrir enlaces<br>"
           "F11 · pantalla completa"));
}


void MainWindow::showTunnels()
{
    qterm2::TunnelDialog dlg(m_tunnelManager, this);
    dlg.exec();
}


void MainWindow::showPlugins()
{
    QMessageBox::information(this, tr("Plugins"),
        tr("<h3>Plugins (API v2)</h3>"
           "<p>Los plugins se cargan automáticamente desde:</p>"
           "<ul>"
           "<li><b>~/.local/lib/qterm2/plugins/</b></li>"
           "<li><b>./plugins/</b> (junto al binario)</li>"
           "</ul>"
           "<p>Formato: archivo .so con interfaz qterm2::Plugin (IID org.qterm2.QTerm2Plugin/1.0)</p>"
           "<p>Capacidades: comandos, paneles, temas, backends. Sistema de permisos integrado.</p>"
           "<p>Plugin Docker/Kubernetes disponible con comandos en la paleta de comandos (Ctrl+Shift+P).</p>"));
}
