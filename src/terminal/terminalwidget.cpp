#include "terminal/terminalwidget.h"
#include "terminal/terminalemulator.h"


#include "terminal/ptyterminalsession.h"
#include "terminal/terminalsession.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUrl>
#include <QWheelEvent>

#include <QtMath>

#include <algorithm>

// --------------------------------------------------------------- utilities

namespace {

QFont pickDefaultFont()
{
    QStringList preferred = {
        QStringLiteral("JetBrains Mono NL"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Hack Nerd Font Mono"),
        QStringLiteral("Hack Nerd Font"),
        QStringLiteral("FiraCode Nerd Font Mono"),
        QStringLiteral("Fira Code"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Source Code Pro"),
        QStringLiteral("IBM Plex Mono"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Monospace"),
    };
    const QStringList installed = QFontDatabase::families();
    QFont f;
    for (const QString &name : preferred) {
        if (installed.contains(name, Qt::CaseInsensitive)) {
            f = QFont(name);
            break;
        }
    }
    if (f.family().isEmpty())
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::TypeWriter, QFont::PreferQuality);
    f.setFixedPitch(true);
    f.setPointSize(11);
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

bool isUrlChar(QChar c)
{
    if (c.isSpace() || c.isNull())
        return false;
    static const QString bad = QStringLiteral("<>\"'`()[]{}|\\^");
    return !bad.contains(c);
}

/** Token under (or next to) a column, clamped to the actual line length. */
QString tokenAt(const QString &line, int col)
{
    if (line.isEmpty())
        return QString();
    int start = qBound(0, col, line.size() - 1);
    int end = start;
    while (start > 0 && isUrlChar(line.at(start - 1)))
        --start;
    while (end + 1 < line.size() && isUrlChar(line.at(end + 1)))
        ++end;
    return line.mid(start, end - start + 1);
}

} // namespace

// ------------------------------------------------------------- construction

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
    , m_font(pickDefaultFont())
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);
    setFont(m_font);

    m_boldFont = m_font;
    m_boldFont.setBold(true);
    m_italicFont = m_font;
    m_italicFont.setItalic(true);
    m_boldItalicFont = m_font;
    m_boldItalicFont.setBold(true);
    m_boldItalicFont.setItalic(true);

    m_colors = Theme::TerminalColors::forMode(Theme::Mode::Dark);

    QFontMetricsF fm(m_font);
    m_cellW = (std::max)(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH = (std::max)(1, qCeil(fm.height()));
    m_ascent = qRound(fm.ascent());

    m_blinkTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] {
        m_cursorBlinkOn = !m_cursorBlinkOn;
        update();
    });

    m_emulator = new TerminalEmulator(this);
    connect(m_emulator, &TerminalEmulator::updated, this, [this] {
        if (m_blinkTimer.isActive())
            m_blinkTimer.start();
        update();
    });
    connect(m_emulator, &TerminalEmulator::screenReset, this, [this] {
        m_scrollOffset = 0;
        m_hasSelection = false;
        m_searchAbsLine = -1;
        m_searchCol = -1;
        emit selectionChanged(false);
    });
    connect(m_emulator, &TerminalEmulator::titleChanged, this,
            [this](const QString &title) {
                m_title = title;
                emit titleChanged(title);
            });
    connect(m_emulator, &TerminalEmulator::bellRang,
            this, &TerminalWidget::bellRang);
    connect(m_emulator, &TerminalEmulator::workingDirectoryChanged,
            this, &TerminalWidget::workingDirectoryChanged);
    connect(m_emulator, &TerminalEmulator::response, this,
            [this](const QByteArray &data) {
                if (m_transport)
                    m_transport->write(data);
            });
    connect(m_emulator, &TerminalEmulator::cursorStyleRequested, this,
            [this](TerminalEmulator::CursorStyle style, bool blink, bool isDefault) {
                // The running application may choose the shape, but DECSCUSR 0
                // returns to the style configured by the user.  The user's
                // "blinking cursor" preference remains the master switch.
                m_cursorStyle = isDefault ? m_preferredCursorStyle : style;
                m_cursorBlinkRequested = blink;
                applyCursorBlink();
            });

    setCursorBlinking(true);
    setTransport(new qterm2::PtyTerminalSession(this));
}

TerminalWidget::~TerminalWidget()
{
    if (m_transport)
        m_transport->terminate();
}

void TerminalWidget::setTransport(qterm2::TerminalSession *transport)
{
    if (m_transport == transport)
        return;
    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->disconnect(m_emulator);
        m_transport->terminate();
        m_transport->deleteLater();
    }
    m_transport = transport;
    if (!m_transport)
        return;
    m_transport->setParent(this);
    connect(m_transport, &qterm2::TerminalSession::dataReceived,
            m_emulator, &TerminalEmulator::feed);
    connect(m_transport, &qterm2::TerminalSession::finished, this, [this] {
        emit terminalFinished();
        update();
    });
}

// ---------------------------------------------------------- process control

bool TerminalWidget::isRunning() const
{
    return m_transport && m_transport->isRunning();
}

bool TerminalWidget::startShell(const QString &shell, const QString &workDir)
{
    if (!m_transport)
        return false;
    return m_transport->startShell(shell, m_emulator->cols(),
                                   m_emulator->rows(), workDir);
}

bool TerminalWidget::startProgram(const QString &program, const QStringList &args,
                                  const QString &workDir,
                                  const QProcessEnvironment &extraEnv)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = extraEnv.keys();
    for (const QString &k : keys)
        env.insert(k, extraEnv.value(k));
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("QTerm2"));
    env.insert(QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("1.0"));
    const QString locale = env.value(QStringLiteral("LANG"));
    if (!locale.contains(QStringLiteral("UTF-8"), Qt::CaseInsensitive)
        && !locale.contains(QStringLiteral("utf8"), Qt::CaseInsensitive))
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));

    if (!m_transport)
        return false;
    return m_transport->start(program, args, m_emulator->cols(),
                              m_emulator->rows(), workDir, env);
}

void TerminalWidget::sendText(const QString &text)
{
    if (m_transport)
        m_transport->write(text.toUtf8());
}

// --------------------------------------------------------------- geometry

void TerminalWidget::updateTerminalSize()
{
    QFontMetricsF fm(m_font);
    m_cellW = (std::max)(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH = (std::max)(1, qCeil(fm.height()));
    m_ascent = qRound(fm.ascent());
    const int rows = qMax(2, height() / m_cellH);
    const int cols = qMax(2, width() / m_cellW);
    m_emulator->resize(rows, cols);
    m_scrollOffset = qBound(0, m_scrollOffset, historyRows());
    if (m_transport)
        m_transport->resize(cols, rows);
}

// -------------------------------------------------------------------- theming

QColor TerminalWidget::colorFor(int c, bool foreground) const
{
    if (c < 0)
        return foreground ? m_colors.foreground : m_colors.background;

    static const int ramp[6] = { 0, 0x5f, 0x87, 0xaf, 0xd7, 0xff };
    if (c & 0x1000000)
        return QColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    if (c < 16)
        return m_colors.palette[c];
    if (c < 232) {
        const int v = c - 16;
        return QColor(ramp[v / 36], ramp[(v / 6) % 6], ramp[v % 6]);
    }
    const int g = 8 + (c - 232) * 10;
    return QColor(g, g, g);
}

// --------------------------------------------------------------- view helpers

int TerminalWidget::historyRows() const
{
    return m_emulator->historyRows();
}

int TerminalWidget::totalLines() const
{
    return m_emulator->totalLines();
}

QString TerminalWidget::plainLine(int absoluteLine) const
{
    return m_emulator->plainLine(absoluteLine);
}

const TermLine *TerminalWidget::lineForView(int row) const
{
    const int abs = absoluteLineForView(row);
    if (abs < 0)
        return nullptr;
    const int hist = historyRows();
    if (abs < hist)
        return &m_emulator->historyBuffer().at(abs);
    const int idx = abs - hist;
    const TermBuffer &buffer = m_emulator->activeBuffer();
    if (idx >= 0 && idx < buffer.size())
        return &buffer.at(idx);
    return nullptr;
}

int TerminalWidget::absoluteLineForView(int row) const
{
    const int abs = historyRows() - m_scrollOffset + row;
    return (abs >= 0 && abs < totalLines()) ? abs : -1;
}

void TerminalWidget::searchRangesForLine(int absLine, QVector<QPair<int, int>> &ranges) const
{
    ranges.clear();
    if (m_searchText.isEmpty())
        return;
    const QString text = plainLine(absLine);
    if (text.isEmpty())
        return;

    int from = 0;
    for (;;) {
        const int idx = text.indexOf(m_searchText, from, Qt::CaseInsensitive);
        if (idx < 0)
            break;
        ranges.append(qMakePair(idx, idx + m_searchText.size() - 1));
        from = idx + qMax(1, m_searchText.size());
    }
}

void TerminalWidget::clampSelection()
{
    const int last = qMax(0, totalLines() - 1);
    m_selAnchor.setY(qBound(0, m_selAnchor.y(), last));
    m_selEnd.setY(qBound(0, m_selEnd.y(), last));
    m_selAnchor.setX(qBound(0, m_selAnchor.x(), m_emulator->cols() - 1));
    m_selEnd.setX(qBound(0, m_selEnd.x(), m_emulator->cols() - 1));
    if (m_selAnchor.y() > m_selEnd.y()
        || (m_selAnchor.y() == m_selEnd.y() && m_selAnchor.x() > m_selEnd.x())) {
        std::swap(m_selAnchor, m_selEnd);
    }
}

bool TerminalWidget::isSelected(int absLine, int col) const
{
    if (!m_hasSelection)
        return false;
    const QPoint a = (m_selAnchor.y() < m_selEnd.y()
                      || (m_selAnchor.y() == m_selEnd.y() && m_selAnchor.x() <= m_selEnd.x()))
                         ? m_selAnchor : m_selEnd;
    const QPoint b = (a == m_selAnchor) ? m_selEnd : m_selAnchor;

    if (absLine < a.y() || absLine > b.y())
        return false;
    if (absLine == a.y() && absLine == b.y())
        return col >= a.x() && col <= b.x();
    if (absLine == a.y())
        return col >= a.x();
    if (absLine == b.y())
        return col <= b.x();
    return true;
}

// -------------------------------------------------------------------- painting

void TerminalWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), m_colors.background);
    p.setFont(m_font);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const bool focused = hasFocus();
    const QColor selectionBg = m_colors.selection;
    const QColor selectionFg = (m_colors.selection.lightnessF() > 0.55)
                                   ? QColor(0x10, 0x12, 0x16)
                                   : Qt::white;

    QVector<QPair<int, int>> ranges;
    for (int row = 0; row < m_emulator->rows(); ++row) {
        const int abs = absoluteLineForView(row);
        const TermLine *line = lineForView(row);
        const int y = row * m_cellH;
        if (!line)
            continue;

        if (!m_searchText.isEmpty()) {
            searchRangesForLine(abs, ranges);
        } else {
            ranges.clear();
        }

        for (int col = 0; col < m_emulator->cols() && col < line->size(); ++col) {
            const TermCell &cell = line->at(col);
            if (cell.ch.unicode() == 0)
                continue; // trailing half of a wide glyph

            int fg = cell.fg;
            int bg = cell.bg;
            quint8 attrs = cell.attrs;
            if (attrs & AttrInverse)
                std::swap(fg, bg);

            const bool selected = isSelected(abs, col);

            bool inSearch = false;
            for (const auto &r : ranges) {
                if (col >= r.first && col <= r.second) {
                    inSearch = true;
                    break;
                }
            }

            const int cells = cell.wide ? 2 : 1;
            const QRect cellRect(col * m_cellW, y, m_cellW * cells, m_cellH);

            if (selected)
                p.fillRect(cellRect, selectionBg);
            else if (bg != -1)
                p.fillRect(cellRect, colorFor(bg, false));
            else if (inSearch)
                p.fillRect(cellRect, QColor(255, 200, 40, 55));

            if ((attrs & AttrInvisible) || cell.ch == QLatin1Char(' '))
                continue;
            if (cell.ch.unicode() < 32)
                continue;

            QColor fgColor = selected ? selectionFg : colorFor(fg, true);
            if (attrs & AttrFaint)
                fgColor.setAlpha(150);

            QFont font = m_font;
            if ((attrs & AttrBold) && (attrs & AttrItalic))
                font = m_boldItalicFont;
            else if (attrs & AttrBold)
                font = m_boldFont;
            else if (attrs & AttrItalic)
                font = m_italicFont;
            p.setFont(font);
            p.setPen(fgColor);
            p.drawText(col * m_cellW, y + m_ascent, QString(cell.ch));
            p.setFont(m_font);

            if (attrs & AttrUnderline)
                p.fillRect(QRect(col * m_cellW, y + m_cellH - 2, m_cellW * cells, 1), fgColor);
            if (attrs & AttrStrike)
                p.fillRect(QRect(col * m_cellW, y + m_cellH / 2, m_cellW * cells, 1), fgColor);
        }
    }

    // ---- cursor ----------------------------------------------------------
    if (m_emulator->cursorVisible() && m_scrollOffset == 0 && m_cursorBlinkOn) {
        const int cx = qBound(0, m_emulator->cursorX(), m_emulator->cols() - 1);
        const int cy = qBound(0, m_emulator->cursorY(), m_emulator->rows() - 1);
        const int x = cx * m_cellW;
        const int y = cy * m_cellH;
        const QRect r(x, y, m_cellW, m_cellH);
        const QColor cursorColor = m_colors.cursor;

        switch (m_cursorStyle) {
        case TerminalEmulator::CursorStyle::Block:
            if (focused) {
                p.fillRect(r, cursorColor);
                const TermLine *line = lineForView(cy);
                if (line && cx < line->size()) {
                    const QChar ch = line->at(cx).ch;
                    if (ch.unicode() != u' ' && ch.unicode() != 0) {
                        p.setPen(m_colors.background);
                        p.setFont(m_font);
                        p.drawText(x, y + m_ascent, QString(ch));
                    }
                }
            } else {
                p.setPen(QPen(cursorColor, 1));
                p.drawRect(r.adjusted(0, 0, -1, -1));
            }
            break;

        case TerminalEmulator::CursorStyle::IBeam: {
            // U+258F LEFT ONE EIGHTH BLOCK: a solid bar that fills the cell
            // height and sits on the left edge of the cell.  ASCII "|" is
            // centred inside its advance width, which makes the cursor look
            // like it is floating in the middle of the character.
            p.save();
            p.setClipRect(r);
            // The glyph is only ~2px wide; subpixel antialiasing would split
            // it into coloured fringes instead of a solid bar.
            QFont cursorFont = m_font;
            cursorFont.setStyleStrategy(QFont::NoAntialias);
            p.setFont(cursorFont);
            p.setPen(cursorColor);
            p.drawText(QPointF(x, y + m_ascent), QString(QChar(0x258F)));
            p.restore();
            break;
        }

        case TerminalEmulator::CursorStyle::Underscore: {
            const int thick = focused ? 3 : 1;
            p.fillRect(QRect(x, y + m_cellH - thick, m_cellW, thick), cursorColor);
            break;
        }
        }
    }
}

// ----------------------------------------------------------------------- input

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl  = mods.testFlag(Qt::ControlModifier);
    const bool alt   = mods.testFlag(Qt::AltModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);

    // Let window-level shortcuts (tab cycling, sidebar, splits) through.
    if (ctrl && event->key() == Qt::Key_Tab)
        { event->ignore(); return; }

    if (ctrl && shift && event->key() == Qt::Key_C) {
        if (m_hasSelection) {
            copySelection();
            clearSelection();
            return;
        }
    }
    if ((ctrl && shift && event->key() == Qt::Key_V)
        || (shift && event->key() == Qt::Key_Insert)) {
        pasteClipboard();
        return;
    }
    if ((ctrl && event->key() == Qt::Key_Insert) && m_hasSelection) {
        copySelection();
        return;
    }
    if (ctrl && shift && event->key() == Qt::Key_A) {
        selectAll();
        return;
    }

    // Shift+PageUp / Shift+PageDown scroll the scrollback, as in xterm/iTerm2.
    if (shift && (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown)) {
        const int dir = event->key() == Qt::Key_PageUp ? 1 : -1;
        m_scrollOffset = qBound(0, m_scrollOffset + dir * qMax(1, m_emulator->rows() - 1), historyRows());
        update();
        return;
    }
    if (shift && (event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
        && historyRows() > 0) {
        m_scrollOffset = event->key() == Qt::Key_Home ? historyRows() : 0;
        update();
        return;
    }

    // SGR mouse coordinates are relative to the visible screen.
    const int extra = ctrl ? (shift ? 6 : 5) : (shift ? 2 : (alt ? 3 : 1));
    const QString cur = m_emulator->applicationCursorKeys() ? QStringLiteral("\x1bO") : QStringLiteral("\x1b[");

    QString seq;
    switch (event->key()) {
    case Qt::Key_Return: case Qt::Key_Enter: seq = QStringLiteral("\r"); break;
    case Qt::Key_Backspace: seq = ctrl ? QStringLiteral("\x08") : QStringLiteral("\x7f"); break;
    case Qt::Key_Tab:
        if (!ctrl)
            seq = shift ? QStringLiteral("\x1b[Z") : QStringLiteral("\t");
        break;
    case Qt::Key_Escape: seq = QStringLiteral("\x1b"); break;
    case Qt::Key_Up:    seq = (extra == 1) ? cur + QLatin1Char('A') : QStringLiteral("\x1b[1;%1A").arg(extra); break;
    case Qt::Key_Down:  seq = (extra == 1) ? cur + QLatin1Char('B') : QStringLiteral("\x1b[1;%1B").arg(extra); break;
    case Qt::Key_Right: seq = (extra == 1) ? cur + QLatin1Char('C') : QStringLiteral("\x1b[1;%1C").arg(extra); break;
    case Qt::Key_Left:  seq = (extra == 1) ? cur + QLatin1Char('D') : QStringLiteral("\x1b[1;%1D").arg(extra); break;
    case Qt::Key_Home:  seq = (extra == 1) ? cur + QLatin1Char('H') : QStringLiteral("\x1b[1;%1H").arg(extra); break;
    case Qt::Key_End:   seq = (extra == 1) ? cur + QLatin1Char('F') : QStringLiteral("\x1b[1;%1F").arg(extra); break;
    case Qt::Key_Insert: seq = QStringLiteral("\x1b[2~"); break;
    case Qt::Key_Delete: seq = QStringLiteral("\x1b[3~"); break;
    case Qt::Key_PageUp:   seq = QStringLiteral("\x1b[5~"); break;
    case Qt::Key_PageDown: seq = QStringLiteral("\x1b[6~"); break;
    case Qt::Key_F1:  seq = QStringLiteral("\x1bOP"); break;
    case Qt::Key_F2:  seq = QStringLiteral("\x1bOQ"); break;
    case Qt::Key_F3:  seq = QStringLiteral("\x1bOR"); break;
    case Qt::Key_F4:  seq = QStringLiteral("\x1bOS"); break;
    case Qt::Key_F5:  seq = QStringLiteral("\x1b[15~"); break;
    case Qt::Key_F6:  seq = QStringLiteral("\x1b[17~"); break;
    case Qt::Key_F7:  seq = QStringLiteral("\x1b[18~"); break;
    case Qt::Key_F8:  seq = QStringLiteral("\x1b[19~"); break;
    case Qt::Key_F9:  seq = QStringLiteral("\x1b[20~"); break;
    case Qt::Key_F10: seq = QStringLiteral("\x1b[21~"); break;
    case Qt::Key_F11: seq = QStringLiteral("\x1b[23~"); break;
    case Qt::Key_F12: seq = QStringLiteral("\x1b[24~"); break;
    default: break;
    }

    if (seq.isEmpty() && !event->text().isEmpty()) {
        const QString t = event->text();
        const ushort u = t.at(0).unicode();
        if (ctrl && u >= u'a' && u <= u'z') {
            seq = QChar(u - u'a' + 1);
        } else if (ctrl && u >= u'A' && u <= u'Z') {
            seq = QChar(u - u'A' + 1);
        } else if (ctrl && t == QStringLiteral("[")) {
            seq = QChar(0x1b);
        } else if (ctrl && t == QStringLiteral("\\")) {
            seq = QChar(0x1c);
        } else if (ctrl && t == QStringLiteral("]")) {
            seq = QChar(0x1d);
        } else if (ctrl && t == QStringLiteral(" ")) {
            seq = QChar(0x00);
        } else {
            if (alt)
                seq += QLatin1Char('\x1b');
            seq += t;
        }
    }

    if (!seq.isEmpty()) {
        m_scrollOffset = 0;
        sendText(seq);
        event->accept();
    }
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty()) {
        m_scrollOffset = 0;
        sendText(event->commitString());
    }
    event->accept();
}

// ----------------------------------------------------------------------- mouse

QPoint TerminalWidget::cellFromPixel(const QPoint &pos) const
{
    const int x = qBound(0, pos.x() / qMax(1, m_cellW), m_emulator->cols() - 1);
    const int row = qBound(0, pos.y() / qMax(1, m_cellH), m_emulator->rows() - 1);
    const int abs = qBound(0, historyRows() - m_scrollOffset + row,
                           qMax(0, totalLines() - 1));
    return QPoint(x, abs);
}

void TerminalWidget::sendMouseEvent(int button, int col, int row, bool press,
                                    bool motion, Qt::KeyboardModifiers mods)
{
    if (!m_transport)
        return;

    // `row` arrives as an absolute scrollback line (see cellFromPixel); mouse
    // protocols expect a row relative to the visible screen.
    const int screenRow = qBound(0, row - historyRows() + m_scrollOffset,
                                 qMax(0, m_emulator->rows() - 1));

    int b = button;
    if (motion)
        b += 32;
    // X10/legacy encoding reports every release as button 3.
    if (!m_emulator->mouseSgr() && !press && !motion)
        b = 3;
    if (mods.testFlag(Qt::ShiftModifier))   b += 4;
    if (mods.testFlag(Qt::AltModifier))     b += 8;
    if (mods.testFlag(Qt::ControlModifier)) b += 16;

    if (m_emulator->mouseSgr()) {
        m_transport->write(QStringLiteral("\x1b[<%1;%2;%3%4")
                         .arg(b).arg(col + 1).arg(screenRow + 1)
                         .arg(press ? QLatin1Char('M') : QLatin1Char('m'))
                         .toUtf8());
    } else {
        const char cb = char(32 + qBound(0, b, 223));
        const char cx = char(32 + qBound(1, col + 1, 255));
        const char cy = char(32 + qBound(1, screenRow + 1, 255));
        const char seq[6] = { 0x1b, '[', 'M', cb, cx, cy };
        m_transport->write(QByteArray(seq, 6));
    }
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    const QPoint cell = cellFromPixel(event->position().toPoint());

    // Ctrl+click opens URLs, like iTerm2's semantic click.
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        const QString line = plainLine(cell.y());
        const QString token = tokenAt(line, cell.x());
        if (token.startsWith(QStringLiteral("http://"))
            || token.startsWith(QStringLiteral("https://"))
            || token.startsWith(QStringLiteral("www."))) {
            const QString url = token.startsWith(QStringLiteral("www."))
                                    ? QStringLiteral("https://") + token : token;
            QDesktopServices::openUrl(QUrl(url));
            return;
        }
    }

    if (event->button() == Qt::MiddleButton) {
        pasteClipboard();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_emulator->mouseMode() && !event->modifiers().testFlag(Qt::ShiftModifier)) {
            sendMouseEvent(0, cell.x(), cell.y(), true, false, event->modifiers());
            return;
        }
        m_hasSelection = true;
        m_selAnchor = m_selEnd = cell;
        m_mouseSelecting = true;
        m_lastMouseCell = cell;
        emit selectionChanged(true);
        update();
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint cell = cellFromPixel(event->position().toPoint());

    if (m_mouseSelecting && (event->buttons() & Qt::LeftButton)) {
        if (cell != m_selEnd) {
            m_selEnd = cell;
            clampSelection();
            update();
        }
        return;
    }

    if (m_emulator->mouseMode() && event->buttons() != Qt::NoButton) {
        if (m_emulator->mouseMode() == 1002 || m_emulator->mouseMode() == 1003) {
            if (cell != m_lastMouseCell) {
                int btn = 0;
                if (event->buttons() & Qt::LeftButton)       btn = 0;
                else if (event->buttons() & Qt::MiddleButton) btn = 1;
                else if (event->buttons() & Qt::RightButton)  btn = 2;
                sendMouseEvent(btn, cell.x(), cell.y(), true, true, event->modifiers());
                m_lastMouseCell = cell;
            }
        }
        return;
    }

    if (m_emulator->mouseMode() == 1003 && event->buttons() == Qt::NoButton) {
        if (cell != m_lastMouseCell) {
            sendMouseEvent(3, cell.x(), cell.y(), true, true, event->modifiers());
            m_lastMouseCell = cell;
        }
    }

    // Ctrl+hover shows a pointing hand over links.
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const QString line = plainLine(cell.y());
        const QString token = tokenAt(line, cell.x());
        QWidget::setCursor((token.startsWith(QStringLiteral("http"))
                            || token.startsWith(QStringLiteral("www.")))
                               ? Qt::PointingHandCursor : Qt::IBeamCursor);
    } else {
        QWidget::setCursor(Qt::IBeamCursor);
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_emulator->mouseMode() && !event->modifiers().testFlag(Qt::ShiftModifier)) {
            const QPoint cell = cellFromPixel(event->position().toPoint());
            sendMouseEvent(0, cell.x(), cell.y(), false, false, event->modifiers());
            return;
        }
        m_mouseSelecting = false;
        if (m_hasSelection) {
            const QString text = selectedText();
            if (text.isEmpty()) {
                clearSelection();
            } else {
                QApplication::clipboard()->setText(text, QClipboard::Selection);
            }
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_emulator->mouseMode()) {
            const QPoint cell = cellFromPixel(event->position().toPoint());
            sendMouseEvent(2, cell.x(), cell.y(), true, false, event->modifiers());
            sendMouseEvent(2, cell.x(), cell.y(), false, false, event->modifiers());
        }
    }
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_emulator->mouseMode())
        return;

    const QPoint cell = cellFromPixel(event->position().toPoint());
    const QString line = plainLine(cell.y());
    if (line.isEmpty())
        return;

    auto isWord = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-')
               || c == QLatin1Char('.') || c == QLatin1Char('/') || c == QLatin1Char(':')
               || c == QLatin1Char('@') || c == QLatin1Char('%') || c == QLatin1Char('+');
    };

    int start = qBound(0, cell.x(), line.size() - 1);
    int end = start;
    if (!isWord(line.at(start))) {
        clearSelection();
        return;
    }
    while (start > 0 && isWord(line.at(start - 1)))
        --start;
    while (end + 1 < line.size() && isWord(line.at(end + 1)))
        ++end;

    m_hasSelection = true;
    m_selAnchor = QPoint(start, cell.y());
    m_selEnd = QPoint(end, cell.y());
    emit selectionChanged(true);
    QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
    update();
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() == 0)
        return;

    const int steps = qMax(1, qAbs(event->angleDelta().y()) / 40);
    const int direction = event->angleDelta().y() > 0 ? 1 : -1;

    if (m_emulator->mouseMode() && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        const QPoint cell = cellFromPixel(event->position().toPoint());
        const int button = direction > 0 ? 64 : 65;
        sendMouseEvent(button, cell.x(), cell.y(), true, false, event->modifiers());
        // X10 encoding has no release event for the wheel.
        event->accept();
        return;
    }

    m_scrollOffset = qBound(0, m_scrollOffset + direction * steps * 3, historyRows());
    update();
    event->accept();
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (width() <= 0 || height() <= 0)
        return;
    updateTerminalSize();
}

void TerminalWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    if (m_blinkTimer.isActive())
        m_blinkTimer.start();
    if (m_emulator->focusReporting() && m_transport)
        m_transport->write(QByteArrayLiteral("\x1b[I"));
    update();
}

void TerminalWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    if (m_emulator->focusReporting() && m_transport)
        m_transport->write(QByteArrayLiteral("\x1b[O"));
    update();
}

// ------------------------------------------------------------- public helpers

QString TerminalWidget::screenText() const
{
    return m_emulator->screenText();
}

QString TerminalWidget::selectedText() const
{
    if (!m_hasSelection)
        return QString();

    QPoint a = (m_selAnchor.y() < m_selEnd.y()
                || (m_selAnchor.y() == m_selEnd.y() && m_selAnchor.x() <= m_selEnd.x()))
                   ? m_selAnchor : m_selEnd;
    QPoint b = (a == m_selAnchor) ? m_selEnd : m_selAnchor;
    a.setY(qBound(0, a.y(), qMax(0, totalLines() - 1)));
    b.setY(qBound(0, b.y(), qMax(0, totalLines() - 1)));

    QString out;
    for (int abs = a.y(); abs <= b.y(); ++abs) {
        const QString line = plainLine(abs);
        const int from = (abs == a.y()) ? qBound(0, a.x(), m_emulator->cols() - 1) : 0;
        const int to = (abs == b.y()) ? qBound(0, b.x(), m_emulator->cols() - 1) : m_emulator->cols() - 1;
        QString chunk;
        for (int c = from; c <= to && c < line.size(); ++c)
            chunk += (line.at(c).unicode() == 0) ? QLatin1Char(' ') : line.at(c);
        while (chunk.endsWith(QLatin1Char(' ')))
            chunk.chop(1);
        out += chunk;
        if (abs != b.y())
            out += QLatin1Char('\n');
    }
    return out;
}

void TerminalWidget::copySelection()
{
    const QString text = selectedText();
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void TerminalWidget::pasteClipboard()
{
    QString text = QApplication::clipboard()->text(QClipboard::Clipboard);
    if (text.isEmpty())
        return;
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    if (m_emulator->bracketedPaste())
        text = QStringLiteral("\x1b[200~") + text + QStringLiteral("\x1b[201~");
    m_scrollOffset = 0;
    sendText(text);
}

void TerminalWidget::selectAll()
{
    if (totalLines() == 0)
        return;
    m_hasSelection = true;
    m_selAnchor = QPoint(0, 0);
    m_selEnd = QPoint(m_emulator->cols() - 1, totalLines() - 1);
    emit selectionChanged(true);
    update();
}

void TerminalWidget::clearSelection()
{
    if (!m_hasSelection)
        return;
    m_hasSelection = false;
    m_mouseSelecting = false;
    emit selectionChanged(false);
    update();
}

void TerminalWidget::clearScrollback()
{
    m_emulator->clearHistory();
    m_scrollOffset = 0;
    update();
}

void TerminalWidget::resetTerminal()
{
    m_emulator->reset();
    m_scrollOffset = 0;
    m_hasSelection = false;
    m_searchAbsLine = -1;
    m_searchCol = -1;
    update();
}

void TerminalWidget::scrollToBottom()
{
    m_scrollOffset = 0;
    update();
}

void TerminalWidget::scrollToLine(int absLine)
{
    const int rows = qMax(1, m_emulator->rows());
    const int history = historyRows();
    // abs = history - offset + screenRow; put absLine at the middle row.
    const int offset = history - absLine + rows / 2;
    m_scrollOffset = qBound(0, offset, history);
    update();
}

QSize TerminalWidget::minimumSizeHint() const
{
    return QSize(m_cellW * 24, m_cellH * 6);
}

QSize TerminalWidget::sizeHint() const
{
    return QSize(m_cellW * 100, m_cellH * 30);
}

// ------------------------------------------------------------------- settings

void TerminalWidget::setTerminalColors(const Theme::TerminalColors &colors)
{
    m_colors = colors;
    update();
}

void TerminalWidget::setFontFamily(const QString &family)
{
    if (family.isEmpty() || family == m_font.family())
        return;
    m_font.setFamily(family);
    m_font.setStyleHint(QFont::TypeWriter);
    m_font.setFixedPitch(true);
    m_boldFont = m_font; m_boldFont.setBold(true);
    m_italicFont = m_font; m_italicFont.setItalic(true);
    m_boldItalicFont = m_font; m_boldItalicFont.setBold(true); m_boldItalicFont.setItalic(true);
    setFont(m_font);
    updateTerminalSize();
    update();
}

void TerminalWidget::setFontSize(int pointSize)
{
    pointSize = qBound(6, pointSize, 48);
    if (m_font.pointSize() == pointSize)
        return;
    m_font.setPointSize(pointSize);
    m_boldFont = m_font; m_boldFont.setBold(true);
    m_italicFont = m_font; m_italicFont.setItalic(true);
    m_boldItalicFont = m_font; m_boldItalicFont.setBold(true); m_boldItalicFont.setItalic(true);
    setFont(m_font);
    updateTerminalSize();
    update();
}

void TerminalWidget::setScrollbackLines(int lines)
{
    m_emulator->setMaxHistory(lines);
    m_scrollOffset = qBound(0, m_scrollOffset, historyRows());
}

void TerminalWidget::setCursorStyle(TerminalEmulator::CursorStyle style)
{
    m_preferredCursorStyle = style;
    if (m_cursorStyle == style)
        return;
    m_cursorStyle = style;
    update();
}

void TerminalWidget::setCursorBlinking(bool blink)
{
    m_cursorBlinkEnabled = blink;
    applyCursorBlink();
}

void TerminalWidget::applyCursorBlink()
{
    const bool blink = m_cursorBlinkEnabled && m_cursorBlinkRequested;
    if (blink) {
        const int flash = QApplication::cursorFlashTime();
        m_blinkTimer.start(flash > 0 ? qMax(200, flash / 2) : 500);
    } else {
        m_blinkTimer.stop();
    }
    m_cursorBlinkOn = true;
    update();
}

QString TerminalWidget::cursorStyleName(TerminalEmulator::CursorStyle style)
{
    switch (style) {
    case TerminalEmulator::CursorStyle::IBeam:
        return QStringLiteral("ibeam");
    case TerminalEmulator::CursorStyle::Underscore:
        return QStringLiteral("underscore");
    case TerminalEmulator::CursorStyle::Block:
        break;
    }
    return QStringLiteral("block");
}

TerminalEmulator::CursorStyle TerminalWidget::cursorStyleFromName(const QString &name)
{
    const QString value = name.trimmed().toLower();
    if (value == QStringLiteral("ibeam") || value == QStringLiteral("i-beam")
        || value == QStringLiteral("bar") || value == QStringLiteral("line")) {
        return TerminalEmulator::CursorStyle::IBeam;
    }
    if (value == QStringLiteral("underscore")
        || value == QStringLiteral("underline")) {
        return TerminalEmulator::CursorStyle::Underscore;
    }
    return TerminalEmulator::CursorStyle::Block;
}

// --------------------------------------------------------------------- search

void TerminalWidget::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    m_searchAbsLine = -1;
    m_searchCol = -1;
    update();
}

bool TerminalWidget::findNext(bool forward)
{
    if (m_searchText.isEmpty())
        return false;

    const int total = totalLines();
    if (total <= 0)
        return false;

    const bool havePos = (m_searchAbsLine >= 0);
    const int hist = historyRows();
    const int cursorAbs = hist + m_emulator->cursorY();

    int startLine;
    int startCol;
    if (havePos) {
        startLine = m_searchAbsLine;
        startCol = forward ? m_searchCol + 1 : m_searchCol - 1;
    } else {
        startLine = (m_scrollOffset > 0) ? hist - m_scrollOffset : cursorAbs;
        startCol = (m_scrollOffset > 0) ? 0 : m_emulator->cursorX();
    }
    startLine = qBound(0, startLine, total - 1);

    int foundLine = -1;
    int foundCol = -1;

    for (int i = 0; i < total; ++i) {
        const int abs = forward ? (startLine + i) % total
                                : (startLine - i % total + total) % total;
        const QString text = plainLine(abs);
        if (text.isEmpty())
            continue;

        if (forward) {
            const int from = (i == 0) ? qBound(0, startCol, text.size()) : 0;
            const int idx = text.indexOf(m_searchText, from, Qt::CaseInsensitive);
            if (idx >= 0) {
                foundLine = abs;
                foundCol = idx;
                break;
            }
        } else {
            const int limit = (i == 0) ? qBound(-1, startCol, text.size() - 1)
                                       : text.size() - 1;
            int searchFrom = 0;
            for (;;) {
                const int idx = text.indexOf(m_searchText, searchFrom, Qt::CaseInsensitive);
                if (idx < 0)
                    break;
                if (idx <= limit)
                    foundLine = abs, foundCol = idx;
                searchFrom = idx + qMax(1, m_searchText.size());
            }
            if (foundLine >= 0)
                break;
        }
    }

    if (foundLine < 0) {
        emit searchResult(false);
        return false;
    }

    m_searchAbsLine = foundLine;
    m_searchCol = foundCol;

    const int firstVisible = hist - m_scrollOffset;
    const int lastVisible = firstVisible + m_emulator->rows() - 1;
    if (foundLine < firstVisible || foundLine > lastVisible)
        m_scrollOffset = qBound(0, hist + m_emulator->rows() / 3 - foundLine, historyRows());

    emit searchResult(true);
    update();
    return true;
}

void TerminalWidget::clearSearch()
{
    m_searchText.clear();
    m_searchAbsLine = -1;
    m_searchCol = -1;
    update();
}