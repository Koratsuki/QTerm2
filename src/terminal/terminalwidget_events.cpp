#include "terminal/terminalwidget.h"

#include "terminal/terminalemulator.h"
#include "terminal/terminalsession.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QFontMetrics>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

namespace {

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
