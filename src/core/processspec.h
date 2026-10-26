#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace qterm2 {

/**
 * A fully resolved process launch request.
 *
 * The terminal engine only understands ProcessSpec; it never needs to know
 * whether the process is a local shell, OpenSSH, an SFTP helper or a future
 * plugin-managed backend.
 */
struct ProcessSpec
{
    QString program;
    QStringList args;
    QString workDir;
    QProcessEnvironment environment;

    bool ok = false;
    QString error;

    static ProcessSpec failure(const QString &message)
    {
        ProcessSpec spec;
        spec.error = message;
        return spec;
    }

    QString commandLine() const;
};

} // namespace qterm2
