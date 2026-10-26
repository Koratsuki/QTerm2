#include "ui/terminalpane.h"

#include "core/processspec.h"
#include "sessions/sessionlauncher.h"
#include "terminal/terminalwidget.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

TerminalPane::TerminalPane(const Session &session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_root = new QSplitter(Qt::Horizontal, this);
    m_root->setChildrenCollapsible(false);
    m_root->setHandleWidth(1);
    m_root->setOpaqueResize(true);
    layout->addWidget(m_root);
}

TerminalPane::~TerminalPane()
{
    // TerminalWidget owns its PTY; deleting the pane kills every child shell.
    m_terminals.clear();
}

TerminalWidget *TerminalPane::createTerminal()
{
    auto *term = new TerminalWidget(this);
    term->setTerminalColors(m_colors);
    if (!m_fontFamily.isEmpty())
        term->setFontFamily(m_fontFamily);
    term->setFontSize(m_fontSize);
    term->setScrollbackLines(m_scrollback);
    term->setCursorBlinking(m_cursorBlink);
    term->setCursorStyle(TerminalWidget::cursorStyleFromName(m_cursorStyle));
    return term;
}

void TerminalPane::attachTerminal(TerminalWidget *terminal)
{
    m_terminals.append(terminal);

    connect(terminal, &TerminalWidget::titleChanged, this, [this] {
        emit titleChanged();
    });
    connect(terminal, &TerminalWidget::terminalFinished, this, [this, terminal] {
        emit terminalFinished(terminal);
    });
    connect(terminal, &TerminalWidget::bellRang, this, [this, terminal] {
        emit bellRang(terminal);
    });
    terminal->installEventFilter(this);
}

bool TerminalPane::launch(TerminalWidget *terminal, const Session &session,
                          QString *errorMessage)
{
    const qterm2::ProcessSpec spec = qterm2::sessions::SessionLauncher::specFor(session);
    if (!spec.ok) {
        if (errorMessage)
            *errorMessage = spec.error;
        return false;
    }
    QString dir = spec.workDir;
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        dir = QDir::homePath();
    if (!terminal->startProgram(spec.program, spec.args, dir, spec.environment)) {
        if (errorMessage)
            *errorMessage = tr("No se pudo iniciar %1.").arg(spec.program);
        return false;
    }
    return true;
}

bool TerminalPane::start(QString *errorMessage)
{
    const qterm2::ProcessSpec spec =
        qterm2::sessions::SessionLauncher::specFor(m_session);
    if (!spec.ok) {
        if (errorMessage)
            *errorMessage = spec.error;
        return false;
    }

    TerminalWidget *term = createTerminal();
    if (!launch(term, m_session, errorMessage)) {
        delete term;
        return false;
    }

    attachTerminal(term);
    m_root->addWidget(term);
    term->show();
    setActive(term);
    term->setFocus(Qt::OtherFocusReason);
    return true;
}

bool TerminalPane::startCommand(const QString &program, const QStringList &args,
                                const QString &workDir, QString *errorMessage)
{
    TerminalWidget *term = createTerminal();
    QString dir = workDir;
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        dir = QDir::homePath();
    if (!term->startProgram(program, args, dir)) {
        delete term;
        if (errorMessage)
            *errorMessage = tr("No se pudo iniciar %1.").arg(program);
        return false;
    }
    attachTerminal(term);
    m_root->addWidget(term);
    term->show();
    setActive(term);
    term->setFocus(Qt::OtherFocusReason);
    return true;
}

bool TerminalPane::restartActive(QString *errorMessage)
{
    return restartTerminal(activeTerminal(), errorMessage);
}

bool TerminalPane::restartTerminal(TerminalWidget *terminal, QString *errorMessage)
{
    if (!terminal || !m_terminals.contains(terminal)) {
        if (errorMessage)
            *errorMessage = tr("La terminal ya no existe.");
        return false;
    }
    terminal->resetTerminal();
    return launch(terminal, m_session, errorMessage);
}

TerminalWidget *TerminalPane::activeTerminal() const
{
    if (m_active && m_terminals.contains(m_active))
        return m_active;
    return m_terminals.isEmpty() ? nullptr : m_terminals.first();
}

void TerminalPane::setActive(TerminalWidget *terminal)
{
    if (!terminal || m_active == terminal)
        return;
    m_active = terminal;
    emit activeTerminalChanged(terminal);
}

void TerminalPane::activateTerminal(TerminalWidget *terminal)
{
    if (!terminal)
        return;
    setActive(terminal);
    terminal->setFocus();
}

void TerminalPane::commitSplit(TerminalWidget *terminal, Qt::Orientation orientation)
{
    TerminalWidget *current = activeTerminal();
    if (!current) {
        m_root->addWidget(terminal);
        return;
    }

    QSplitter *parent = qobject_cast<QSplitter *>(current->parentWidget());
    if (parent && parent->orientation() == orientation) {
        const int index = parent->indexOf(current);
        parent->insertWidget(index + 1, terminal);
        const int count = parent->count();
        QList<int> sizes;
        for (int i = 0; i < count; ++i)
            sizes.append(1);
        parent->setSizes(sizes);
    } else if (parent) {
        auto *nested = new QSplitter(orientation, parent);
        nested->setChildrenCollapsible(false);
        nested->setHandleWidth(1);
        nested->setOpaqueResize(true);
        const int index = parent->indexOf(current);
        nested->addWidget(current);
        nested->addWidget(terminal);
        parent->insertWidget(index, nested);
        nested->setSizes({ 1, 1 });
    } else {
        m_root->addWidget(terminal);
    }
    terminal->show();
}

bool TerminalPane::prepareSplit(QString *errorMessage)
{
    const qterm2::ProcessSpec spec =
        qterm2::sessions::SessionLauncher::specFor(m_session);
    if (!spec.ok) {
        if (errorMessage)
            *errorMessage = spec.error;
        return false;
    }
    return true;
}

bool TerminalPane::split(Qt::Orientation orientation, QString *errorMessage)
{
    if (!prepareSplit(errorMessage))
        return false;

    TerminalWidget *term = createTerminal();
    if (!launch(term, m_session, errorMessage)) {
        delete term;
        return false;
    }

    attachTerminal(term);
    commitSplit(term, orientation);
    setActive(term);
    term->setFocus(Qt::OtherFocusReason);
    return true;
}

void TerminalPane::removeTerminal(TerminalWidget *terminal)
{
    if (!terminal)
        return;

    m_terminals.removeAll(terminal);
    QSplitter *parent = qobject_cast<QSplitter *>(terminal->parentWidget());
    terminal->setParent(nullptr);
    terminal->deleteLater();

    // Collapse splitters that now contain a single pane, and prune empty ones.
    while (parent && parent != m_root && parent->count() <= 1) {
        QSplitter *grand = qobject_cast<QSplitter *>(parent->parentWidget());
        if (parent->count() == 0) {
            parent->deleteLater();
            parent = grand;
            continue;
        }
        QWidget *child = parent->widget(0);
        const int index = grand ? grand->indexOf(parent) : -1;
        if (grand && index >= 0)
            grand->insertWidget(index, child);
        parent->deleteLater();
        parent = grand;
    }
    if (parent && parent->count() == 0 && parent != m_root)
        parent->deleteLater();

    if (m_terminals.isEmpty()) {
        m_active = nullptr;
        emit activeTerminalChanged(nullptr);
        emit lastPaneClosed();
        return;
    }

    if (!m_terminals.contains(m_active))
        m_active = nullptr;
    TerminalWidget *next = activeTerminal();
    m_active = next;
    if (next) {
        next->setFocus(Qt::OtherFocusReason);
        emit activeTerminalChanged(next);
    }
}

bool TerminalPane::closeActivePane()
{
    TerminalWidget *term = activeTerminal();
    if (!term)
        return false;
    removeTerminal(term);
    return !m_terminals.isEmpty();
}

void TerminalPane::focusActive()
{
    if (TerminalWidget *term = activeTerminal())
        term->setFocus(Qt::OtherFocusReason);
}

bool TerminalPane::anyRunning() const
{
    for (TerminalWidget *term : m_terminals) {
        if (term->isRunning())
            return true;
    }
    return false;
}

void TerminalPane::applyAppearance(const Theme::TerminalColors &colors,
                                   const QString &fontFamily, int fontSize,
                                   int scrollback, bool cursorBlink,
                                   const QString &cursorStyle)
{
    // A session profile overrides only the fields it defines; everything else
    // follows the global preference passed by MainWindow.
    const TerminalProfile &profile = m_session.terminalProfile;
    const bool useProfile = profile.enabled;
    const QString effectiveFamily =
        (useProfile && !profile.fontFamily.trimmed().isEmpty()) ? profile.fontFamily
                                                               : fontFamily;
    const int effectiveSize =
        (useProfile && profile.fontSize > 0) ? profile.fontSize : fontSize;
    const int effectiveScrollback =
        (useProfile && profile.scrollbackLines > 0) ? profile.scrollbackLines
                                                    : scrollback;
    const bool effectiveBlink = useProfile ? profile.cursorBlink : cursorBlink;
    const QString effectiveStyle =
        (useProfile && !profile.cursorStyle.trimmed().isEmpty()) ? profile.cursorStyle
                                                                 : cursorStyle;

    m_colors = colors;
    m_fontFamily = effectiveFamily;
    m_fontSize = effectiveSize;
    m_scrollback = effectiveScrollback;
    m_cursorBlink = effectiveBlink;
    m_cursorStyle = effectiveStyle;

    for (TerminalWidget *term : m_terminals) {
        term->setTerminalColors(colors);
        if (!effectiveFamily.isEmpty())
            term->setFontFamily(effectiveFamily);
        term->setFontSize(effectiveSize);
        term->setScrollbackLines(effectiveScrollback);
        term->setCursorBlinking(effectiveBlink);
        term->setCursorStyle(TerminalWidget::cursorStyleFromName(effectiveStyle));
    }
}

bool TerminalPane::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn) {
        auto *term = qobject_cast<TerminalWidget *>(watched);
        if (term && m_terminals.contains(term))
            setActive(term);
    }
    return QWidget::eventFilter(watched, event);
}
