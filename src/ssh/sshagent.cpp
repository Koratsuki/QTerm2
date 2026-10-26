#include "ssh/sshagent.h"

#include "core/pathutils.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace qterm2::ssh {

QString agentSocketPath()
{
    const QByteArray value = qgetenv("SSH_AUTH_SOCK");
    if (value.isEmpty())
        return QString();
    return expandHome(QString::fromLocal8Bit(value));
}

QVector<AgentIdentity> parseAgentIdentities(const QString &output)
{
    QVector<AgentIdentity> identities;
    const QRegularExpression modern(
        QStringLiteral("^(\\d+)\\s+(\\S+)\\s+(.*?)\\s*\\(([^()]*)\\)\\s*$"));

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;

        const QRegularExpressionMatch match = modern.match(line);
        if (match.hasMatch()) {
            AgentIdentity identity;
            identity.bits = match.captured(1);
            identity.fingerprint = match.captured(2);
            identity.comment = match.captured(3).trimmed();
            identity.keyType = match.captured(4).trimmed();
            identities.append(identity);
            continue;
        }

        // Fallback for unusual formats: keep the whole line as fingerprint.
        AgentIdentity identity;
        identity.fingerprint = line;
        identities.append(identity);
    }
    return identities;
}

AgentStatus queryAgent()
{
    AgentStatus status;
    status.socketPath = agentSocketPath();
    if (status.socketPath.isEmpty()) {
        status.error = QStringLiteral(
            "SSH_AUTH_SOCK no está definida. Arranca un agente (por ejemplo con "
            "«eval $(ssh-agent)») o configura uno en tu entorno de escritorio.");
        return status;
    }
    if (!QFileInfo::exists(status.socketPath)) {
        status.error = QStringLiteral(
            "SSH_AUTH_SOCK apunta a %1, pero ese socket no existe. El agente "
            "puede haberse detenido.")
                           .arg(status.socketPath);
        return status;
    }

    QProcess process;
    process.setProgram(QStringLiteral("ssh-add"));
    process.setArguments({ QStringLiteral("-l") });
    process.start();
    if (!process.waitForStarted(5000)) {
        status.error = QStringLiteral(
            "No se pudo ejecutar ssh-add. Instala el paquete openssh-client.");
        return status;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        status.error = QStringLiteral("ssh-add no respondió a tiempo.");
        return status;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    status.identities = parseAgentIdentities(output);
    status.available = true;

    if (process.exitStatus() != QProcess::NormalExit) {
        status.available = false;
        status.error = QStringLiteral("ssh-add terminó de forma anómala.");
        return status;
    }

    // Exit code 0: identities listed. 1: agent reachable but empty. 2: cannot
    // connect to the agent.  ssh-add writes the explanation to stderr.
    if (process.exitCode() == 2) {
        status.available = false;
        const QString stderrText =
            QString::fromUtf8(process.readAllStandardError()).trimmed();
        status.error = stderrText.isEmpty()
                           ? QStringLiteral("No se pudo conectar con el agente SSH.")
                           : stderrText;
        status.identities.clear();
    }
    return status;
}

} // namespace qterm2::ssh
