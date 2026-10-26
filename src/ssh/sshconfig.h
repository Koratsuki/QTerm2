#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "domain/session.h"

namespace qterm2::ssh {

/** One concrete `Host` block parsed from an OpenSSH client config. */
struct SshConfigEntry
{
    QStringList patterns;
    QString alias;
    QString hostName;
    QString user;
    int port = 0;
    QString identityFile;
    QString proxyJump;
    QString sourcePath;
    int line = 0;
    bool wildcard = false;

    QString effectiveHost() const
    {
        return hostName.isEmpty() ? alias : hostName;
    }
};

struct SshConfigDocument
{
    QVector<SshConfigEntry> entries;
    QStringList warnings;
};

/**
 * Parses an OpenSSH client config, following `Include` directives.
 *
 * Only concrete `Host` blocks (no `*`, `?` or `!` patterns) are returned.
 * Unknown keywords are ignored; malformed lines are reported as warnings.
 */
SshConfigDocument parseSshConfig(const QString &path, QString *errorMessage = nullptr);

/** Converts parsed hosts into SSH sessions ready to be persisted. */
QVector<Session> sessionsFromSshConfig(const SshConfigDocument &document,
                                       const QString &group = QStringLiteral("SSH config"));

/** Renders SSH sessions as a managed config block (without markers). */
QString renderSshConfig(const QVector<Session> &sessions);

/**
 * Replaces the QTerm2 managed block in `path` with `block`.
 *
 * Existing content outside the markers is preserved, the previous file is
 * backed up and the result is written atomically with mode 0600.
 */
bool writeManagedSshConfig(const QString &path, const QString &block,
                           QString *errorMessage = nullptr);

QString managedBlockBegin();
QString managedBlockEnd();

} // namespace qterm2::ssh
