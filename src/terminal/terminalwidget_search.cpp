#include "terminal/terminalwidget.h"

#include "terminal/terminalemulator.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>

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
