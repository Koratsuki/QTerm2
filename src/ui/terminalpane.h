#pragma once

#include <QWidget>
#include <QList>
#include <QMap>

#include "domain/session.h"
#include "ui/theme.h"

class QSplitter;
class TerminalWidget;
class PaneWrapper;

/**
 * One tab. Contains one or more terminal panes arranged in a binary split
 * tree, very much like iTerm2's split panes.
 */
class TerminalPane : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalPane(const Session &session, QWidget *parent = nullptr);
    ~TerminalPane() override;

    bool start(QString *errorMessage = nullptr);
    bool startCommand(const QString &program, const QStringList &args,
                      const QString &workDir = QString(),
                      QString *errorMessage = nullptr);
    /** Clears the active terminal and starts the session again (reconnect). */
    bool restartActive(QString *errorMessage = nullptr);
    /** Reconnects one specific terminal (used by auto-reconnect). */
    bool restartTerminal(TerminalWidget *terminal, QString *errorMessage = nullptr);

    TerminalWidget *activeTerminal() const;
    QList<TerminalWidget *> terminals() const { return m_terminals; }
    bool split(Qt::Orientation orientation, QString *errorMessage = nullptr);
    bool closeActivePane(); // false when no panes remain
    void focusActive();
    /** Makes `terminal` the active pane and gives it keyboard focus. */
    void activateTerminal(TerminalWidget *terminal);

    const Session &session() const { return m_session; }
    void updateSession(const Session &session) { m_session = session; emit titleChanged(); }
    QString sessionName() const { return m_session.name; }
    bool anyRunning() const;

    void applyAppearance(const Theme::TerminalColors &colors,
                         const QString &fontFamily, int fontSize,
                         int scrollback, bool cursorBlink,
                         const QString &cursorStyle = QStringLiteral("block"));

signals:
    void titleChanged();
    void lastPaneClosed();
    void activeTerminalChanged(TerminalWidget *terminal);
    void terminalFinished(TerminalWidget *terminal);
    void bellRang(TerminalWidget *terminal);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    TerminalWidget *createTerminal();
    bool launch(TerminalWidget *terminal, const Session &session, QString *errorMessage);
    bool prepareSplit(QString *errorMessage);
    void commitSplit(TerminalWidget *terminal, Qt::Orientation orientation);
    void attachTerminal(TerminalWidget *terminal);
    void removeTerminal(TerminalWidget *terminal);
    void collapseEmptySplitters();
    void setActive(TerminalWidget *terminal);
    void updateHeaders();

    Session m_session;
    QSplitter *m_root = nullptr;
    QList<TerminalWidget *> m_terminals;
    QMap<TerminalWidget *, PaneWrapper *> m_wrappers;
    TerminalWidget *m_active = nullptr;

    Theme::TerminalColors m_colors = Theme::TerminalColors::dark();
    QString m_fontFamily;
    int m_fontSize = 11;
    int m_scrollback = 10000;
    bool m_cursorBlink = true;
    QString m_cursorStyle = QStringLiteral("block");
};
