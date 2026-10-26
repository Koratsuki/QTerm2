#include "terminal/history/commandhistory.h"

namespace qterm2 {

CommandHistory::CommandHistory(QObject *parent)
    : QObject(parent)
{
}

void CommandHistory::addEntry(const QString &command, const QString &sessionId, const QString &directory)
{
    if (command.trimmed().isEmpty())
        return;
    CommandHistoryEntry entry;
    entry.command = command.trimmed();
    entry.sessionId = sessionId;
    entry.directory = directory;
    entry.timestamp = QDateTime::currentDateTime();
    entry.exitCode = -1;
    m_entries.append(entry);
    emit entryAdded(entry);
}

QVector<CommandHistoryEntry> CommandHistory::entries(const QString &sessionId) const
{
    if (sessionId.isEmpty())
        return m_entries;
    QVector<CommandHistoryEntry> result;
    for (const auto &e : m_entries)
        if (e.sessionId == sessionId)
            result.append(e);
    return result;
}

QStringList CommandHistory::search(const QString &query, const QString &sessionId) const
{
    QStringList result;
    const QString q = query.toLower();
    for (const auto &e : m_entries) {
        if (!sessionId.isEmpty() && e.sessionId != sessionId)
            continue;
        if (e.command.toLower().contains(q))
            result.append(e.command);
    }
    result.removeDuplicates();
    return result;
}

void CommandHistory::clear(const QString &sessionId)
{
    if (sessionId.isEmpty()) {
        m_entries.clear();
        return;
    }
    QVector<CommandHistoryEntry> filtered;
    for (const auto &e : m_entries)
        if (e.sessionId != sessionId)
            filtered.append(e);
    m_entries = filtered;
}

} // namespace qterm2
