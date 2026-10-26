#include "terminal/terminalemulator.h"


#include <QChar>
#include <QUrl>

#include <algorithm>
#include <utility>

// --------------------------------------------------------------- utilities

namespace {

qint64 historyLineBytes(const TermLine &line)
{
    // Approximate the retained heap plus the QVector object itself.  It is a
    // safety bound, not an exact allocator measurement.
    return qint64(sizeof(TermLine)) + qint64(line.size()) * qint64(sizeof(TermCell));
}

int glyphWidth(uint cp)
{
    if (cp == 0)
        return 0;
    if (cp < 32)
        return 0;
    const QChar ch(ushort(cp & 0xFFFF));
    const QChar::Category cat = ch.category();
    if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining
        || cat == QChar::Mark_Enclosing || cp == 0x200B || cp == 0xFEFF)
        return 0;

    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF)
        || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
        || (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0xFE30 && cp <= 0xFE6F)
        || (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6)
        || (cp >= 0x1F300 && cp <= 0x1FAFF) || (cp >= 0x20000 && cp <= 0x3FFFD))
        return 2;
    return 1;
}

} // namespace

TerminalEmulator::TerminalEmulator(QObject *parent)
    : QObject(parent)
{
    allocateScreen(m_rows, m_cols);
    m_bottomMargin = m_rows - 1;
    for (int c = 8; c < m_cols; c += 8)
        m_tabStops.insert(c);
}

void TerminalEmulator::resize(int rows, int cols)
{
    resizeScreen(rows, cols);
}

void TerminalEmulator::setMaxHistory(int lines)
{
    m_maxHistory = qMax(0, lines);
    trimHistory();
}

void TerminalEmulator::setMaxHistoryBytes(int bytes)
{
    m_maxHistoryBytes = qMax(0, bytes);
    trimHistory();
}

void TerminalEmulator::pushHistory(TermLine line)
{
    m_historyBytes += historyLineBytes(line);
    m_history.append(std::move(line));
    trimHistory();
}

void TerminalEmulator::trimHistory()
{
    while (!m_history.isEmpty()
           && (m_history.size() > m_maxHistory || m_historyBytes > m_maxHistoryBytes)) {
        m_historyBytes -= historyLineBytes(m_history.first());
        m_history.removeFirst();
    }
    if (m_history.isEmpty())
        m_historyBytes = 0;
}

void TerminalEmulator::clearHistoryInternal()
{
    m_history.clear();
    m_historyBytes = 0;
}

void TerminalEmulator::allocateScreen(int rows, int cols)
{
    auto make = [rows, cols]() {
        TermBuffer b;
        b.reserve(rows);
        for (int r = 0; r < rows; ++r)
            b.append(TermLine(cols));
        return b;
    };
    m_main = make();
    m_alt = make();
    m_rows = rows;
    m_cols = cols;
    m_tabStops.clear();
    for (int c = 8; c < cols; c += 8)
        m_tabStops.insert(c);
    m_topMargin = 0;
    m_bottomMargin = rows - 1;
    m_curX = m_curY = 0;
}

void TerminalEmulator::reset()
{
    clearHistoryInternal();
    allocateScreen(m_rows, m_cols);
    m_useAlt = false;
    m_curX = m_curY = 0;
    m_fg = m_bg = -1;
    m_attrs = 0;
    m_wrapPending = false;
    m_appCursorKeys = false;
    m_originMode = false;
    m_insertMode = false;
    m_cursorVisible = true;
    m_autowrap = true;
    m_bracketedPaste = false;
    m_focusReporting = false;
    m_mouseMode = 0;
    m_mouseSgr = false;
    m_utf8.clear();
    m_state = Ground;
    m_csi.clear();
    m_osc.clear();

    emit screenReset();
    emit updated();
}

void TerminalEmulator::resizeScreen(int rows, int cols)
{
    rows = qMax(2, rows);
    cols = qMax(2, cols);
    if (rows == m_rows && cols == m_cols)
        return;

    const int oldRows = m_rows;

    auto resizeColumns = [cols](TermBuffer &b) {
        for (TermLine &line : b)
            line.resize(cols);
    };

    resizeColumns(m_main);
    resizeColumns(m_alt);

    if (rows < oldRows) {
        // Keep the cursor visible when the window shrinks: drop lines from the
        // top into the scrollback (main screen) or discard them (alt screen).
        while (m_main.size() > rows) {
            if (m_curY >= rows && !m_useAlt) {
                pushHistory(m_main.takeFirst());
                --m_curY;
            } else {
                m_main.removeLast();
            }
        }
        while (m_alt.size() > rows) {
            if (m_curY >= rows && m_useAlt) {
                m_alt.removeFirst();
                --m_curY;
            } else {
                m_alt.removeLast();
            }
        }
    } else if (rows > oldRows) {
        while (m_main.size() < rows)
            m_main.append(TermLine(cols));
        while (m_alt.size() < rows)
            m_alt.append(TermLine(cols));
    }

    m_rows = rows;
    m_cols = cols;
    m_topMargin = 0;
    m_bottomMargin = rows - 1;
    m_tabStops.clear();
    for (int c = 8; c < cols; c += 8)
        m_tabStops.insert(c);

    m_curX = qBound(0, m_curX, cols - 1);
    m_curY = qBound(0, m_curY, rows - 1);
    m_wrapPending = false;
}

void TerminalEmulator::setCursor(int x, int y)
{
    m_curX = qBound(0, x, m_cols - 1);
    int minY = m_originMode ? m_topMargin : 0;
    int maxY = m_originMode ? m_bottomMargin : m_rows - 1;
    m_curY = qBound(minY, y, maxY);
    m_wrapPending = false;
}

void TerminalEmulator::putChar(uint ucs)
{
    if (ucs == 0)
        return;
    if (ucs > 0xFFFF)
        ucs = 0xFFFD; // astral plane: not representable in a single cell

    const int width = glyphWidth(ucs);
    if (width == 0) {
        // Combining mark: merge it into the previous cell when possible.
        if (m_curX > 0) {
            TermLine &line = screen()[qBound(0, m_curY, m_rows - 1)];
            if (m_curX - 1 < line.size())
                line[m_curX - 1].ch = QChar(ushort(ucs));
        }
        return;
    }

    if (m_wrapPending && m_autowrap) {
        m_curX = 0;
        lineFeed();
        m_wrapPending = false;
    }

    if (width == 2 && m_curX + 1 >= m_cols) {
        if (m_autowrap) {
            m_curX = 0;
            lineFeed();
        } else {
            m_curX = qMax(0, m_cols - 2);
        }
    }

    m_curX = qBound(0, m_curX, m_cols - 1);

    TermCell cell;
    cell.ch = QChar(ushort(ucs));
    cell.fg = m_fg;
    cell.bg = m_bg;
    cell.attrs = m_attrs;
    cell.wide = (width == 2);

    TermBuffer &b = screen();
    if (m_curY >= 0 && m_curY < b.size()) {
        TermLine &line = b[m_curY];
        if (m_insertMode) {
            for (int c = m_cols - 1; c >= m_curX + width; --c)
                line[c] = line[c - width];
        }
        line[m_curX] = cell;
        if (width == 2 && m_curX + 1 < m_cols) {
            TermCell trailing;
            trailing.ch = QChar(0);
            trailing.fg = m_fg;
            trailing.bg = m_bg;
            trailing.attrs = m_attrs;
            line[m_curX + 1] = trailing;
        }
    }

    m_lastChar = cell.ch;
    m_curX += width;
    if (m_curX >= m_cols) {
        if (m_autowrap) {
            m_curX = m_cols - 1;
            m_wrapPending = true;
        } else {
            m_curX = m_cols - 1;
        }
    }
}

void TerminalEmulator::lineFeed()
{
    if (m_curY == m_bottomMargin)
        scrollUp(1);
    else if (m_curY < m_rows - 1)
        ++m_curY;
    m_wrapPending = false;
}

void TerminalEmulator::reverseIndex()
{
    if (m_curY == m_topMargin)
        scrollDown(1);
    else if (m_curY > 0)
        --m_curY;
    m_wrapPending = false;
}

void TerminalEmulator::carriageReturn()
{
    m_curX = 0;
    m_wrapPending = false;
}

void TerminalEmulator::backspace()
{
    if (m_wrapPending) {
        m_wrapPending = false;
        m_curX = m_cols - 1;
        return;
    }
    if (m_curX > 0)
        --m_curX;
}

void TerminalEmulator::tab()
{
    int next = m_cols - 1;
    for (int c = m_curX + 1; c < m_cols; ++c) {
        if (m_tabStops.contains(c)) {
            next = c;
            break;
        }
    }
    m_curX = qBound(0, next, m_cols - 1);
    m_wrapPending = false;
}

void TerminalEmulator::scrollUp(int n)
{
    TermBuffer &b = screen();
    if (b.isEmpty())
        return;
    n = qBound(1, n, m_bottomMargin - m_topMargin + 1);
    for (int i = 0; i < n; ++i) {
        if (!m_useAlt && m_topMargin == 0 && m_bottomMargin == m_rows - 1) {
            pushHistory(b.takeFirst());
            b.append(TermLine(m_cols));
        } else {
            for (int r = m_topMargin; r < m_bottomMargin; ++r)
                b[r] = b[r + 1];
            b[m_bottomMargin] = TermLine(m_cols);
        }
    }
}

void TerminalEmulator::scrollDown(int n)
{
    TermBuffer &b = screen();
    if (b.isEmpty())
        return;
    n = qBound(1, n, m_bottomMargin - m_topMargin + 1);
    for (int i = 0; i < n; ++i) {
        for (int r = m_bottomMargin; r > m_topMargin; --r)
            b[r] = b[r - 1];
        b[m_topMargin] = TermLine(m_cols);
    }
}

void TerminalEmulator::eraseInLine(int mode)
{
    TermBuffer &b = screen();
    if (m_curY < 0 || m_curY >= b.size())
        return;
    TermLine &line = b[m_curY];
    TermCell blank;
    if (mode == 0) {
        for (int c = m_curX; c < m_cols; ++c)
            line[c] = blank;
    } else if (mode == 1) {
        for (int c = 0; c <= m_curX && c < m_cols; ++c)
            line[c] = blank;
    } else {
        for (int c = 0; c < m_cols; ++c)
            line[c] = blank;
    }
}

void TerminalEmulator::eraseInDisplay(int mode)
{
    TermBuffer &b = screen();
    if (mode == 0) {
        eraseInLine(0);
        for (int r = m_curY + 1; r < m_rows; ++r)
            b[r] = TermLine(m_cols);
    } else if (mode == 1) {
        eraseInLine(1);
        for (int r = 0; r < m_curY; ++r)
            b[r] = TermLine(m_cols);
    } else if (mode == 2 || mode == 3) {
        for (int r = 0; r < m_rows; ++r)
            b[r] = TermLine(m_cols);
        if (mode == 3)
            clearHistoryInternal();
    }
}

void TerminalEmulator::insertLines(int n)
{
    if (m_curY < m_topMargin || m_curY > m_bottomMargin)
        return;
    TermBuffer &b = screen();
    n = qBound(1, n, m_bottomMargin - m_curY + 1);
    for (int i = 0; i < n; ++i) {
        for (int r = m_bottomMargin; r > m_curY; --r)
            b[r] = b[r - 1];
        b[m_curY] = TermLine(m_cols);
    }
}

void TerminalEmulator::deleteLines(int n)
{
    if (m_curY < m_topMargin || m_curY > m_bottomMargin)
        return;
    TermBuffer &b = screen();
    n = qBound(1, n, m_bottomMargin - m_curY + 1);
    for (int i = 0; i < n; ++i) {
        for (int r = m_curY; r < m_bottomMargin; ++r)
            b[r] = b[r + 1];
        b[m_bottomMargin] = TermLine(m_cols);
    }
}

void TerminalEmulator::insertChars(int n)
{
    TermBuffer &b = screen();
    if (m_curY < 0 || m_curY >= b.size())
        return;
    TermLine &line = b[m_curY];
    n = qBound(1, n, m_cols - m_curX);
    for (int c = m_cols - 1; c >= m_curX + n; --c)
        line[c] = line[c - n];
    for (int c = m_curX; c < m_curX + n && c < m_cols; ++c)
        line[c] = TermCell();
}

void TerminalEmulator::deleteChars(int n)
{
    TermBuffer &b = screen();
    if (m_curY < 0 || m_curY >= b.size())
        return;
    TermLine &line = b[m_curY];
    n = qBound(1, n, m_cols - m_curX);
    for (int c = m_curX; c < m_cols - n; ++c)
        line[c] = line[c + n];
    for (int c = qMax(m_curX, m_cols - n); c < m_cols; ++c)
        line[c] = TermCell();
}

void TerminalEmulator::eraseChars(int n)
{
    TermBuffer &b = screen();
    if (m_curY < 0 || m_curY >= b.size())
        return;
    TermLine &line = b[m_curY];
    n = qBound(1, n, m_cols - m_curX);
    for (int c = m_curX; c < m_curX + n && c < m_cols; ++c)
        line[c] = TermCell();
}

void TerminalEmulator::saveCursor()
{
    m_savedX = m_curX;
    m_savedY = m_curY;
    m_savedFg = m_fg;
    m_savedBg = m_bg;
    m_savedAttrs = m_attrs;
    m_savedWrap = m_wrapPending;
}

void TerminalEmulator::restoreCursor()
{
    m_curX = qBound(0, m_savedX, m_cols - 1);
    m_curY = qBound(0, m_savedY, m_rows - 1);
    m_fg = m_savedFg;
    m_bg = m_savedBg;
    m_attrs = m_savedAttrs;
    m_wrapPending = m_savedWrap;
}

void TerminalEmulator::setAltScreen(bool alt)
{
    if (alt == m_useAlt)
        return;
    m_useAlt = alt;
    if (alt) {
        for (int r = 0; r < m_alt.size(); ++r)
            m_alt[r] = TermLine(m_cols);
    }
    m_curX = m_curY = 0;
    m_topMargin = 0;
    m_bottomMargin = m_rows - 1;
    m_wrapPending = false;
    emit screenReset();
    emit updated();
}

void TerminalEmulator::applySgr(const QVector<int> &params)
{
    if (params.isEmpty()) {
        m_fg = m_bg = -1;
        m_attrs = 0;
        return;
    }
    for (int i = 0; i < params.size(); ++i) {
        const int p = params[i];
        switch (p) {
        case 0:  m_fg = m_bg = -1; m_attrs = 0; break;
        case 1:  m_attrs |= AttrBold; break;
        case 2:  m_attrs |= AttrFaint; break;
        case 3:  m_attrs |= AttrItalic; break;
        case 4:  m_attrs |= AttrUnderline; break;
        case 5: case 6: m_attrs |= AttrBlink; break;
        case 7:  m_attrs |= AttrInverse; break;
        case 8:  m_attrs |= AttrInvisible; break;
        case 9:  m_attrs |= AttrStrike; break;
        case 21: case 22: m_attrs &= ~(AttrBold | AttrFaint); break;
        case 23: m_attrs &= ~AttrItalic; break;
        case 24: m_attrs &= ~AttrUnderline; break;
        case 25: m_attrs &= ~AttrBlink; break;
        case 27: m_attrs &= ~AttrInverse; break;
        case 28: m_attrs &= ~AttrInvisible; break;
        case 29: m_attrs &= ~AttrStrike; break;
        case 39: m_fg = -1; break;
        case 49: m_bg = -1; break;
        default:
            if (p >= 30 && p <= 37) m_fg = p - 30;
            else if (p >= 40 && p <= 47) m_bg = p - 40;
            else if (p >= 90 && p <= 97) m_fg = p - 90 + 8;
            else if (p >= 100 && p <= 107) m_bg = p - 100 + 8;
            else if (p == 38 || p == 48) {
                const bool fg = (p == 38);
                int value = -1;
                if (i + 1 < params.size() && params[i + 1] == 5 && i + 2 < params.size()) {
                    value = params[i + 2] & 0xFF;
                    i += 2;
                } else if (i + 1 < params.size() && params[i + 1] == 2 && i + 4 < params.size()) {
                    const int r = params[i + 2] & 0xFF;
                    const int g = params[i + 3] & 0xFF;
                    const int b = params[i + 4] & 0xFF;
                    value = 0x1000000 | (r << 16) | (g << 8) | b;
                    i += 4;
                }
                if (value >= 0) {
                    if (fg) m_fg = value;
                    else    m_bg = value;
                }
            }
            break;
        }
    }
}

void TerminalEmulator::setCursorStyleCode(int code)
{
    bool blink = true;
    const bool isDefault = (code == 0);
    switch (code) {
    case 2: m_cursorStyle = CursorStyle::Block; blink = false; break;
    case 3: m_cursorStyle = CursorStyle::Underscore; break;
    case 4: m_cursorStyle = CursorStyle::Underscore; blink = false; break;
    case 5: m_cursorStyle = CursorStyle::IBeam; break;
    case 6: m_cursorStyle = CursorStyle::IBeam; blink = false; break;
    case 1:
    default:
        m_cursorStyle = CursorStyle::Block;
        break;
    }
    emit cursorStyleRequested(m_cursorStyle, blink, isDefault);
    emit updated();
}

void TerminalEmulator::reportDeviceAttributes()
{
    // VT220 with ANSI colour, sixel and UTF-8, plus the xterm "version".
    emit response(QByteArrayLiteral("\x1b[?62;22;29;4;6c"));
}

void TerminalEmulator::handleC0(unsigned char c)
{
    switch (c) {
    case 0x07: emit bellRang(); break;
    case 0x08: backspace(); break;
    case 0x09: tab(); break;
    case 0x0a: case 0x0b: case 0x0c: lineFeed(); break;
    case 0x0d: carriageReturn(); break;
    case 0x0e: case 0x0f: break;
    default: break;
    }
}

void TerminalEmulator::handlePrintable(unsigned char c)
{
    if (c < 0x80) {
        m_utf8.clear();
        putChar(c);
        return;
    }

    if (m_utf8.isEmpty()) {
        if ((c & 0xE0) == 0xC0)      m_utf8.append(char(c));
        else if ((c & 0xF0) == 0xE0) m_utf8.append(char(c));
        else if ((c & 0xF8) == 0xF0) m_utf8.append(char(c));
        else { putChar(0xFFFD); return; }
        return;
    }

    m_utf8.append(char(c));

    const unsigned char b0 = static_cast<unsigned char>(m_utf8.at(0));
    int need = 0;
    if ((b0 & 0xE0) == 0xC0) need = 2;
    else if ((b0 & 0xF0) == 0xE0) need = 3;
    else if ((b0 & 0xF8) == 0xF0) need = 4;
    if (need == 0) {
        m_utf8.clear();
        putChar(0xFFFD);
        return;
    }
    if (m_utf8.size() < need)
        return;

    uint cp = b0 & (need == 2 ? 0x1F : (need == 3 ? 0x0F : 0x07));
    for (int i = 1; i < need; ++i) {
        const unsigned char bn = static_cast<unsigned char>(m_utf8.at(i));
        if ((bn & 0xC0) != 0x80) {
            cp = 0xFFFD;
            break;
        }
        cp = (cp << 6) | (bn & 0x3F);
    }
    m_utf8.clear();
    if (cp >= 0xD800 && cp <= 0xDFFF)
        cp = 0xFFFD;
    putChar(cp);
}

void TerminalEmulator::handleEsc(unsigned char c)
{
    switch (c) {
    case '7': saveCursor(); break;
    case '8': restoreCursor(); break;
    case 'D': lineFeed(); break;                    // IND
    case 'E': carriageReturn(); lineFeed(); break;  // NEL
    case 'M': reverseIndex(); break;                // RI
    case 'H': m_tabStops.insert(m_curX); break;     // HTS
    case 'c': reset(); break;                 // RIS
    case '=': case '>': break;                      // keypad modes
    case 'n': case 'o': break;
    default: break;
    }
}

void TerminalEmulator::handleCsi(char final, const QByteArray &raw)
{
    QByteArray body = raw;
    char priv = 0;
    if (!body.isEmpty() && (body[0] == '?' || body[0] == '>' || body[0] == '<' || body[0] == '=')) {
        priv = body[0];
        body = body.mid(1);
    }

    QByteArray inter;
    while (!body.isEmpty() && body.back() >= 0x20 && body.back() <= 0x2F) {
        inter.prepend(body.back());
        body.chop(1);
    }

    QVector<int> params;
    {
        int cur = -1;
        for (char ch : body) {
            if (ch >= '0' && ch <= '9') {
                if (cur < 0) cur = 0;
                cur = cur * 10 + (ch - '0');
                if (cur > 65535) cur = 65535;
            } else if (ch == ';' || ch == ':') {
                params.append(cur < 0 ? 0 : cur);
                cur = -1;
            }
        }
        if (cur >= 0 || !body.isEmpty())
            params.append(cur < 0 ? 0 : cur);
    }

    auto p = [&](int idx, int def) {
        return (idx < params.size() && params[idx] > 0) ? params[idx] : def;
    };
    auto p0 = [&](int idx, int def) {
        return (idx < params.size() && params[idx] >= 0) ? params[idx] : def;
    };

    switch (final) {
    case 'A': setCursor(m_curX, m_curY - p(0, 1)); break;
    case 'B': setCursor(m_curX, m_curY + p(0, 1)); break;
    case 'C': setCursor(m_curX + p(0, 1), m_curY); break;
    case 'D': setCursor(m_curX - p(0, 1), m_curY); break;
    case 'E': setCursor(0, m_curY + p(0, 1)); break;
    case 'F': setCursor(0, m_curY - p(0, 1)); break;
    case 'G': case '`': setCursor(p(0, 1) - 1, m_curY); break;
    case 'd': setCursor(m_curX, p(0, 1) - 1); break;
    case 'a': setCursor(m_curX + p(0, 1), m_curY); break;
    case 'e': setCursor(m_curX, m_curY + p(0, 1)); break;
    case 'H': case 'f':
        setCursor(p(1, 1) - 1, p(0, 1) - 1 + (m_originMode ? m_topMargin : 0));
        break;
    case 'J': eraseInDisplay(p0(0, 0)); break;
    case 'K': eraseInLine(p0(0, 0)); break;
    case 'L': insertLines(p(0, 1)); break;
    case 'M': deleteLines(p(0, 1)); break;
    case '@': insertChars(p(0, 1)); break;
    case 'P': deleteChars(p(0, 1)); break;
    case 'X': eraseChars(p(0, 1)); break;
    case 'S': scrollUp(p(0, 1)); break;
    case 'T': scrollDown(p(0, 1)); break;
    case 'I': { // CHT
        const int n = p(0, 1);
        for (int i = 0; i < n; ++i) tab();
        break;
    }
    case 'Z': { // CBT
        const int n = p(0, 1);
        for (int i = 0; i < n; ++i) {
            int prev = 0;
            for (int c = m_curX - 1; c > 0; --c) {
                if (m_tabStops.contains(c)) { prev = c; break; }
            }
            m_curX = prev;
        }
        break;
    }
    case 'b': {
        const int repeat = p(0, 1);
        for (int i = 0; i < repeat; ++i)
            putChar(m_lastChar.unicode());
        break;
    }
    case 'c':
        if (priv == '>')
            emit response(QByteArrayLiteral("\x1b[>1;10;0c")); // secondary DA
        else
            reportDeviceAttributes();
        break;
    case 'n':
        if (p0(0, 0) == 5)
            emit response(QByteArrayLiteral("\x1b[0n"));
        else if (p0(0, 0) == 6)
            emit response(QStringLiteral("\x1b[%1;%2R")
                              .arg(m_curY + 1).arg(m_curX + 1).toUtf8());
        break;
    case 's': saveCursor(); break;
    case 'u': restoreCursor(); break;
    case 'g':
        if (p0(0, 0) == 0) m_tabStops.remove(m_curX);
        else if (p0(0, 0) == 3) m_tabStops.clear();
        break;
    case 'q':
        if (inter == " ") setCursorStyleCode(p0(0, 0));
        break;
    case 'p':
        if (inter == "!" && p0(0, 0) == 0) { // DECSTR soft reset
            m_fg = m_bg = -1;
            m_attrs = 0;
            m_originMode = false;
            m_insertMode = false;
            m_autowrap = true;
            m_topMargin = 0;
            m_bottomMargin = m_rows - 1;
            m_cursorVisible = true;
            emit updated();
        }
        break;
    case 'r':
        if (priv == 0) {
            const int top = p(0, 1) - 1;
            const int bot = p(1, m_rows) - 1;
            if (top < bot && bot < m_rows) {
                m_topMargin = top;
                m_bottomMargin = bot;
                setCursor(0, m_originMode ? m_topMargin : 0);
            }
        }
        break;
    case 'h':
    case 'l': {
        const bool set = (final == 'h');
        for (int v : params) {
            if (priv == '?') {
                switch (v) {
                case 1:  m_appCursorKeys = set; break;
                case 6:
                    m_originMode = set;
                    setCursor(0, set ? m_topMargin : 0);
                    break;
                case 7:  m_autowrap = set; m_wrapPending = false; break;
                case 12:
                    emit cursorStyleRequested(m_cursorStyle, set, false);
                    break;
                case 25: m_cursorVisible = set; emit updated(); break;
                case 47: case 1047:
                case 1049:
                    if (set) {
                        if (v == 1049) saveCursor();
                        setAltScreen(true);
                    } else {
                        setAltScreen(false);
                        if (v == 1049) restoreCursor();
                    }
                    break;
                case 1000: case 1002: case 1003:
                    m_mouseMode = set ? v : 0;
                    break;
                case 1004: m_focusReporting = set; break;
                case 1006: m_mouseSgr = set; break;
                case 2004: m_bracketedPaste = set; break;
                default: break;
                }
            } else {
                if (v == 4) m_insertMode = set;
            }
        }
        break;
    }
    case 'm': applySgr(priv == '?' ? QVector<int>() : params); break;
    case 't': break; // window manipulation is ignored on purpose
    case 'x': break;
    default: break;
    }
}

void TerminalEmulator::handleOsc(const QByteArray &raw)
{
    const int sep = raw.indexOf(';');
    const QByteArray cmd = sep < 0 ? raw : raw.left(sep);
    const QByteArray arg = sep < 0 ? QByteArray() : raw.mid(sep + 1);
    bool ok = false;
    const int n = cmd.toInt(&ok);
    if (!ok)
        return;

    if (n == 0 || n == 1 || n == 2) {
        QString t = QString::fromUtf8(arg);
        t.remove(QLatin1Char('\a'));
        if (t != m_title) {
            m_title = t;
            emit titleChanged(t);
        }
    } else if (n == 7) {
        // OSC 7 ; file://host/path — used by zsh/bash integration for cwd.
        const QString value = QString::fromUtf8(arg);
        if (value.startsWith(QStringLiteral("file://"))) {
            const QUrl url(value);
            if (url.isLocalFile())
                emit workingDirectoryChanged(url.toLocalFile());
        }
    } else if (n == 133) {
        // OSC 133 Shell Integration (iTerm2 compatible)
        emit shellIntegrationEvent(QString::fromUtf8(arg));
    }
    // OSC 52 (clipboard write) is intentionally ignored: a remote program
    // must never be able to overwrite the local clipboard silently.
}

void TerminalEmulator::feed(const QByteArray &data)
{
    for (int i = 0; i < data.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(data.at(i));

        switch (m_state) {
        case Ground:
            if (c == 0x1b) { m_state = Esc; break; }
            if (c == 0x9b) { m_state = Csi; m_csi.clear(); break; }
            if (c < 0x20 || c == 0x7f) handleC0(c);
            else handlePrintable(c);
            break;

        case Esc:
            if (c == '[')      { m_state = Csi; m_csi.clear(); }
            else if (c == ']') { m_state = Osc; m_osc.clear(); }
            else if (c == 'P' || c == 'X' || c == '^' || c == '_') { m_state = StrSkip; }
            else { handleEsc(c); m_state = Ground; }
            break;

        case Csi:
            if ((c >= 0x30 && c <= 0x3f) || (c >= 0x20 && c <= 0x2f)) {
                if (m_csi.size() < 256) m_csi.append(char(c));
            } else if (c >= 0x40 && c <= 0x7e) {
                handleCsi(char(c), m_csi);
                m_state = Ground;
            } else {
                m_state = Ground;
            }
            break;

        case Osc:
            if (c == 0x07) { handleOsc(m_osc); m_state = Ground; }
            else if (c == 0x1b) m_state = StrSkipEsc;
            else if (m_osc.size() < 4096) m_osc.append(char(c));
            else m_state = Ground;
            break;

        case StrSkip:
            if (c == 0x1b) m_state = StrSkipEsc;
            break;

        case StrSkipEsc:
            if (c == '\\') m_state = Ground;
            else if (c == 0x1b) m_state = StrSkip;
            else m_state = Ground;
            break;
        }
    }
    emit updated();
}

QString TerminalEmulator::plainLine(int absLine) const
{
    const TermLine *line = nullptr;
    const int hist = historyRows();
    if (absLine < 0)
        return QString();
    if (absLine < hist)
        line = &m_history.at(absLine);
    else if (absLine - hist < screen().size())
        line = &screen().at(absLine - hist);
    if (!line)
        return QString();

    QString s;
    s.reserve(line->size());
    for (const TermCell &c : *line)
        s += (c.ch.unicode() == 0) ? QLatin1Char(' ') : c.ch;
    return s;
}

QString TerminalEmulator::screenText() const
{
    auto lineToString = [](const TermLine &line) {
        QString s;
        s.reserve(line.size());
        for (const TermCell &c : line)
            s += (c.ch.unicode() == 0) ? QString() : QString(c.ch);
        while (s.endsWith(QLatin1Char(' ')))
            s.chop(1);
        return s;
    };

    QString out;
    for (const TermLine &line : m_history)
        out += lineToString(line) + QLatin1Char('\n');
    for (const TermLine &line : screen())
        out += lineToString(line) + QLatin1Char('\n');
    return out;
}