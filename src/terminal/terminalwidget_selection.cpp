#include "terminal/terminalwidget.h"

#include "terminal/terminalemulator.h"
#include "terminal/terminalsession.h"

#include <QApplication>
#include <QClipboard>
#include <QPoint>
#include <QString>
#include <QtGlobal>

#include <algorithm>

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
    QPoint a = m_selAnchor;
    QPoint b = m_selEnd;
    if (a.y() > b.y() || (a.y() == b.y() && a.x() > b.x()))
        std::swap(a, b);
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
