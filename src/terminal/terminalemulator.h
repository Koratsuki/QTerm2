#pragma once

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

/**
 * Pure terminal screen model and VT/xterm parser.
 *
 * The emulator knows nothing about widgets, fonts, the clipboard or
 * processes.  It turns bytes coming from any transport (local PTY, SSH
 * channel, plugin backend) into cells, cursor state and mode flags, and
 * reports side effects with signals.  Rendering and input live in
 * TerminalView / TerminalWidget.
 */

/** One character cell. Lines are replaced, never mutated in place. */
struct TermCell
{
    QChar   ch    = u' ';   // 0 means "second half of a wide glyph"
    int     fg    = -1;     // -1 default, 0..255 palette, 0x1000000|RGB truecolor
    int     bg    = -1;
    quint8  attrs = 0;
    bool    wide  = false;  // leading cell of a double-width glyph
};

enum TermAttr : quint8 {
    AttrBold      = 1 << 0,
    AttrFaint     = 1 << 1,
    AttrItalic    = 1 << 2,
    AttrUnderline = 1 << 3,
    AttrBlink     = 1 << 4,
    AttrInverse   = 1 << 5,
    AttrInvisible = 1 << 6,
    AttrStrike    = 1 << 7,
};

using TermLine = QVector<TermCell>;
using TermBuffer = QVector<TermLine>;

class TerminalEmulator : public QObject
{
    Q_OBJECT
public:
    enum class CursorStyle { Block, IBeam, Underscore };
    Q_ENUM(CursorStyle)

    explicit TerminalEmulator(QObject *parent = nullptr);

    // ---- geometry --------------------------------------------------------
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    void resize(int rows, int cols);
    void reset();

    // ---- data path -------------------------------------------------------
    void feed(const QByteArray &data);

    // ---- screen access (read-only for renderers) -------------------------
    const TermBuffer &mainBuffer() const { return m_main; }
    const TermBuffer &altBuffer() const { return m_alt; }
    const TermBuffer &historyBuffer() const { return m_history; }
    const TermBuffer &activeBuffer() const { return m_useAlt ? m_alt : m_main; }
    bool useAltScreen() const { return m_useAlt; }
    int historyRows() const { return m_useAlt ? 0 : m_history.size(); }
    int totalLines() const { return historyRows() + activeBuffer().size(); }
    QString plainLine(int absoluteLine) const;
    QString screenText() const;

    int cursorX() const { return m_curX; }
    int cursorY() const { return m_curY; }
    bool cursorVisible() const { return m_cursorVisible; }
    CursorStyle cursorStyle() const { return m_cursorStyle; }

    // ---- modes -----------------------------------------------------------
    bool applicationCursorKeys() const { return m_appCursorKeys; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    bool focusReporting() const { return m_focusReporting; }
    int mouseMode() const { return m_mouseMode; }
    bool mouseSgr() const { return m_mouseSgr; }

    // ---- scrollback ------------------------------------------------------
    int maxHistory() const { return m_maxHistory; }
    void setMaxHistory(int lines);
    int maxHistoryBytes() const { return m_maxHistoryBytes; }
    void setMaxHistoryBytes(int bytes);
    qint64 historyBytes() const { return m_historyBytes; }
    void clearHistory() { clearHistoryInternal(); }

    QString title() const { return m_title; }

signals:
    /** The visible screen changed and should be repainted. */
    void updated();
    /** A reset or alternate-screen switch invalidated view state. */
    void screenReset();
    void titleChanged(const QString &title);
    void bellRang();
    void workingDirectoryChanged(const QString &path);
    void shellIntegrationEvent(const QString &event);
    /** Terminal query (DA, CPR, ...) that must be written back to the peer. */
    void response(const QByteArray &data);
    /**
     * DECSCUSR asked for a cursor style; blink is a view preference.
     * `isDefault` is true for DECSCUSR 0, where the view should fall back to
     * the user's configured style instead of forcing a block cursor.
     */
    void cursorStyleRequested(TerminalEmulator::CursorStyle style, bool blink,
                              bool isDefault);

private:
    TermBuffer &screen() { return m_useAlt ? m_alt : m_main; }
    const TermBuffer &screen() const { return m_useAlt ? m_alt : m_main; }

    void pushHistory(TermLine line);
    void trimHistory();
    void clearHistoryInternal();

    void allocateScreen(int rows, int cols);
    void resizeScreen(int rows, int cols);

    void putChar(uint ucs);
    void lineFeed();
    void reverseIndex();
    void carriageReturn();
    void backspace();
    void tab();
    void scrollUp(int n);
    void scrollDown(int n);
    void eraseInDisplay(int mode);
    void eraseInLine(int mode);
    void insertLines(int n);
    void deleteLines(int n);
    void insertChars(int n);
    void deleteChars(int n);
    void eraseChars(int n);
    void setCursor(int x, int y);
    void applySgr(const QVector<int> &params);
    void setAltScreen(bool alt);
    void saveCursor();
    void restoreCursor();
    void setCursorStyleCode(int code);
    void reportDeviceAttributes();

    void handleC0(unsigned char c);
    void handlePrintable(unsigned char c);
    void handleEsc(unsigned char c);
    void handleCsi(char final, const QByteArray &raw);
    void handleOsc(const QByteArray &raw);

    int m_rows = 24;
    int m_cols = 80;

    TermBuffer m_main;
    TermBuffer m_alt;
    TermBuffer m_history;
    int m_maxHistory = 10000;
    int m_maxHistoryBytes = 64 * 1024 * 1024;
    qint64 m_historyBytes = 0;
    bool m_useAlt = false;

    int m_curX = 0;
    int m_curY = 0;
    int m_fg = -1;
    int m_bg = -1;
    quint8 m_attrs = 0;
    QChar m_lastChar = u' ';
    bool m_wrapPending = false;
    bool m_autowrap = true;
    bool m_originMode = false;
    bool m_insertMode = false;
    bool m_cursorVisible = true;
    bool m_appCursorKeys = false;
    bool m_bracketedPaste = false;
        bool m_focusReporting = false;
    int m_mouseMode = 0;          // 0, 1000, 1002, 1003
    bool m_mouseSgr = false;
    int m_topMargin = 0;
    int m_bottomMargin = 23;
    QSet<int> m_tabStops;

    int m_savedX = 0, m_savedY = 0;
    int m_savedFg = -1, m_savedBg = -1;
    quint8 m_savedAttrs = 0;
    bool m_savedWrap = false;

    enum State { Ground, Esc, Csi, Osc, StrSkip, StrSkipEsc };
    State m_state = Ground;
    QByteArray m_csi;
    QByteArray m_osc;
    QByteArray m_utf8;
    QString m_title;
    CursorStyle m_cursorStyle = CursorStyle::Block;
};
