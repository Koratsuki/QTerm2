#pragma once

#include <QProcessEnvironment>
#include <QWidget>
#include "terminal/terminalemulator.h"
#include <QRect>
#include <QVector>
#include <QPoint>
#include <QString>
#include <QFont>
#include <QTimer>

namespace qterm2 { class TerminalEmulator; }
#include "ui/theme.h"

namespace qterm2 {
class TerminalSession;
}

/**
 * Terminal view: renders a TerminalEmulator and translates keyboard, mouse,
 * clipboard and IME events into bytes for a TerminalSession.
 *
 * The widget owns the emulator and a transport, but neither owns the other:
 * future transports (libssh channels, plugin backends) only have to implement
 * TerminalSession, and the parser can be tested without a widget.
 */
class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    // ---- process control -------------------------------------------------
    bool startShell(const QString &shell, const QString &workDir = QString());
    bool startProgram(const QString &program, const QStringList &args,
                      const QString &workDir = QString(),
                      const QProcessEnvironment &extraEnv = QProcessEnvironment());
    bool isRunning() const;
    void sendText(const QString &text);

    /** Replaces the transport (takes ownership).  Used by future backends. */
    void setTransport(qterm2::TerminalSession *transport);

    // ---- appearance ------------------------------------------------------
    void setTerminalColors(const Theme::TerminalColors &colors);
    const Theme::TerminalColors &terminalColors() const { return m_colors; }
    void setFontFamily(const QString &family);
    QString fontFamily() const { return m_font.family(); }
    void setFontSize(int pointSize);
    int fontPointSize() const { return m_font.pointSize(); }
    void setScrollbackLines(int lines);
    void setCursorStyle(TerminalEmulator::CursorStyle style);
    TerminalEmulator::CursorStyle cursorStyle() const { return m_cursorStyle; }
    void setCursorBlinking(bool blink);
    bool cursorBlinking() const { return m_cursorBlinkEnabled; }

    /** Stable names used by settings and the preferences dialog. */
    static QString cursorStyleName(TerminalEmulator::CursorStyle style);
    static TerminalEmulator::CursorStyle cursorStyleFromName(const QString &name);

    TerminalEmulator *emulator() const { return m_emulator; }

    // ---- text / clipboard ------------------------------------------------
    QString title() const { return m_title; }
    QString screenText() const;
    QString selectedText() const;
    void copySelection();
    void pasteClipboard();
    void selectAll();
    void clearSelection();
    void clearScrollback();
    void resetTerminal();
    void scrollToBottom();
    /** Scrolls so `absLine` (history + screen) is visible, roughly centered. */
    void scrollToLine(int absLine);

    // ---- search ----------------------------------------------------------
    void setSearchText(const QString &text);
    bool findNext(bool forward);
    void clearSearch();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void titleChanged(const QString &title);
    void terminalFinished();
    void bellRang();
    void searchResult(bool found);
    void selectionChanged(bool hasSelection);
    void workingDirectoryChanged(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    // ---- view geometry ---------------------------------------------------
    void updateTerminalSize();
    int historyRows() const;
    int totalLines() const;
    QString plainLine(int absoluteLine) const;
    const TermLine *lineForView(int row) const;
    int absoluteLineForView(int row) const;

    // ---- painting helpers ------------------------------------------------
    QColor colorFor(int c, bool foreground) const;
    bool isSelected(int absLine, int col) const;
    void searchRangesForLine(int absLine, QVector<QPair<int, int>> &ranges) const;
    void clampSelection();
    QPoint cellFromPixel(const QPoint &pos) const;
    void setDirty() { update(); }

    // ---- input -----------------------------------------------------------
    void sendMouseEvent(int button, int col, int row, bool press,
                        bool motion, Qt::KeyboardModifiers mods);

    // ---- state -----------------------------------------------------------
    TerminalEmulator *m_emulator = nullptr;
    qterm2::TerminalSession *m_transport = nullptr;

    QFont m_font;
    QFont m_boldFont, m_italicFont, m_boldItalicFont;
    Theme::TerminalColors m_colors;
    int m_cellW = 8;
    int m_cellH = 16;
    int m_ascent = 12;

    // ---- selection / scrollback (display state) --------------------------
    int m_scrollOffset = 0;
    bool m_hasSelection = false;
    QPoint m_selAnchor;
    QPoint m_selEnd;
    bool m_mouseSelecting = false;
    QPoint m_lastMouseCell;

    // ---- search ----------------------------------------------------------
    QString m_searchText;
    int m_searchAbsLine = -1;
    int m_searchCol = -1;

    // ---- view-only state -------------------------------------------------
    QString m_title;
    bool m_cursorBlinkEnabled = true;   // user preference (master switch)
    bool m_cursorBlinkRequested = true; // DECSCUSR request from the running app
    bool m_cursorBlinkOn = true;
    /** User-configured style, restored when the application sends DECSCUSR 0. */
    TerminalEmulator::CursorStyle m_preferredCursorStyle = TerminalEmulator::CursorStyle::Block;
    TerminalEmulator::CursorStyle m_cursorStyle = TerminalEmulator::CursorStyle::Block;

    void applyCursorBlink();

    QTimer m_blinkTimer;
};
