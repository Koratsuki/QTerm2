#pragma once

#include <QSet>
#include <QString>
#include <QVector>

namespace qterm2::ssh {

struct KnownHostEntry
{
    QString hosts;
    QString keyType;
    QString key;
    QString comment;
    int line = 0;
    bool hashed = false;
};

struct KnownHostsDocument
{
    QVector<KnownHostEntry> entries;
    QStringList warnings;
};

/** Reads a `known_hosts` file. Blank lines and comments are skipped. */
KnownHostsDocument parseKnownHosts(const QString &path, QString *errorMessage = nullptr);

/**
 * Removes the given 1-based line numbers, preserving the rest of the file.
 *
 * The previous file is backed up and the replacement is written atomically
 * with mode 0600.
 */
bool removeKnownHostLines(const QString &path, const QSet<int> &lineNumbers,
                          QString *errorMessage = nullptr);

} // namespace qterm2::ssh
