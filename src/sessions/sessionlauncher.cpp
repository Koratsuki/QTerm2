#include "sessions/sessionlauncher.h"

#include "core/pathutils.h"
#include "ssh/sshcommandbuilder.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace qterm2::sessions {

ProcessSpec SessionLauncher::specFor(const Session &session)
{
    if (!session.isLocal())
        return ssh::buildConnectSpec(session);

    ProcessSpec spec;
    QString shell = expandHome(session.shell);
    if (shell.isEmpty())
        shell = defaultShellPath();
    spec.program = shell;
    spec.workDir = expandHome(session.startDir);

    if (!session.initialCommand.trimmed().isEmpty()) {
        spec.args << QStringLiteral("-c")
                  << QStringLiteral("%1\nexec %2 -l")
                         .arg(session.initialCommand, shellQuote(shell));
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("QTerm2"));
    env.insert(QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("1.0"));
    const QString locale = env.value(QStringLiteral("LANG"));
    if (!locale.contains(QStringLiteral("UTF-8"), Qt::CaseInsensitive)
        && !locale.contains(QStringLiteral("utf8"), Qt::CaseInsensitive)) {
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    }
    spec.environment = env;

    spec.ok = true;
    return spec;
}

QString SessionLauncher::commandLineFor(const Session &session)
{
    return specFor(session).commandLine();
}

} // namespace qterm2::sessions
