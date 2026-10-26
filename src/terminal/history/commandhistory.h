#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>

namespace qterm2 {

struct CommandHistoryEntry {
    QString command;
    QString sessionId;
    QDateTime timestamp;
    QString directory;
    int exitCode = -1;
};

class CommandHistory : public QObject
{
    Q_OBJECT
public:
    explicit CommandHistory(QObject *parent = nullptr);

    void addEntry(const QString &command, const QString &sessionId, const QString &directory = QString());
    QVector<CommandHistoryEntry> entries(const QString &sessionId = QString()) const;
    QStringList search(const QString &query, const QString &sessionId = QString()) const;
    void clear(const QString &sessionId = QString());

signals:
    void entryAdded(const CommandHistoryEntry &entry);

private:
    QVector<CommandHistoryEntry> m_entries;
};

} // namespace qterm2
