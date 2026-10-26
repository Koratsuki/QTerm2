#include "ssh/sshcommandbuilder.h"

#include "core/pathutils.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

namespace qterm2::ssh {

namespace {

QString sshDestination(const QString &user, const QString &host)
{
    QString h = host.trimmed();
    if (h.contains(QLatin1Char(':')) && !h.startsWith(QLatin1Char('[')))
        h = QLatin1Char('[') + h + QLatin1Char(']');
    return user.isEmpty() ? h : user.trimmed() + QLatin1Char('@') + h;
}

QString hostPort(const QString &host, int port)
{
    QString h = host.trimmed();
    if (h.contains(QLatin1Char(':')) && !h.startsWith(QLatin1Char('[')))
        h = QLatin1Char('[') + h + QLatin1Char(']');
    return QStringLiteral("%1:%2").arg(h).arg(port);
}

QString forwardSpec(const PortForward &forward)
{
    const QString listen = forward.listenHost.trimmed().isEmpty()
                               ? QString::number(forward.listenPort)
                               : hostPort(forward.listenHost, forward.listenPort);
    return listen + QLatin1Char(':')
           + hostPort(forward.targetHost, forward.targetPort);
}

void appendHostKeyPolicy(QStringList &args, Session::HostKeyPolicy policy)
{
    switch (policy) {
    case Session::HostKeyPolicy::Strict:
        args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=yes");
        break;
    case Session::HostKeyPolicy::AcceptNew:
        args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
        break;
    case Session::HostKeyPolicy::Ask:
        break;
    }
}

} // namespace

QString expandPath(const QString &path)
{
    return qterm2::expandHome(path);
}

ProcessSpec buildConnectSpec(const Session &session)
{
    ProcessSpec spec;
    spec.workDir = qterm2::expandHome(session.startDir);

    if (session.host.trimmed().isEmpty())
        return ProcessSpec::failure(QStringLiteral("El servidor no tiene un nombre de host."));
    if (session.port < 1 || session.port > 65535)
        return ProcessSpec::failure(QStringLiteral("El puerto debe estar entre 1 y 65535."));

    spec.program = QStringLiteral("ssh");
    if (session.port != 22)
        spec.args << QStringLiteral("-p") << QString::number(session.port);

    if (session.keepAlive) {
        spec.args << QStringLiteral("-o") << QStringLiteral("ServerAliveInterval=30")
                  << QStringLiteral("-o") << QStringLiteral("ServerAliveCountMax=3")
                  << QStringLiteral("-o") << QStringLiteral("TCPKeepAlive=yes");
    }
    if (session.connectTimeout > 0) {
        spec.args << QStringLiteral("-o")
                  << QStringLiteral("ConnectTimeout=%1").arg(session.connectTimeout);
    }
    appendHostKeyPolicy(spec.args, session.hostKeyPolicy);

    switch (session.auth) {
    case Session::Auth::PublicKey: {
        const QString key = expandPath(session.privateKey);
        if (key.isEmpty())
            return ProcessSpec::failure(
                QStringLiteral("No se eligió ninguna llave privada para la autenticación."));
        if (!QFileInfo::exists(key))
            return ProcessSpec::failure(
                QStringLiteral("La llave privada no existe:\n%1").arg(key));
        spec.args << QStringLiteral("-i") << key
                  << QStringLiteral("-o") << QStringLiteral("IdentitiesOnly=yes")
                  << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=publickey");
        break;
    }
    case Session::Auth::Agent:
        spec.args << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=publickey");
        break;
    case Session::Auth::Password:
        spec.args << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no")
                  << QStringLiteral("-o")
                  << QStringLiteral("PreferredAuthentications=password,keyboard-interactive")
                  << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=3");
        break;
    }

    if (!session.proxyJump.trimmed().isEmpty())
        spec.args << QStringLiteral("-J") << session.proxyJump.trimmed();
    if (session.x11Forwarding)
        spec.args << QStringLiteral("-X");
    if (session.forwardAgent)
        spec.args << QStringLiteral("-A");

    for (const PortForward &forward : session.portForwards) {
        if (!forward.enabled || forward.listenPort < 1)
            continue;
        switch (forward.type) {
        case PortForward::Type::Local:
            if (forward.targetHost.trimmed().isEmpty() || forward.targetPort < 1)
                continue;
            spec.args << QStringLiteral("-L") << forwardSpec(forward);
            break;
        case PortForward::Type::Remote:
            if (forward.targetHost.trimmed().isEmpty() || forward.targetPort < 1)
                continue;
            spec.args << QStringLiteral("-R") << forwardSpec(forward);
            break;
        case PortForward::Type::Dynamic:
            spec.args << QStringLiteral("-D")
                      << (forward.listenHost.trimmed().isEmpty()
                              ? QString::number(forward.listenPort)
                              : hostPort(forward.listenHost, forward.listenPort));
            break;
        }
    }

    const QString extra = session.extraArgs.trimmed();
    if (!extra.isEmpty())
        spec.args << QProcess::splitCommand(extra);

    const QStringList remoteCommand = QProcess::splitCommand(session.initialCommand);
    if (!remoteCommand.isEmpty())
        spec.args << QStringLiteral("-t");

    spec.args << sshDestination(session.user, session.host);
    spec.args << remoteCommand;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("QTerm2"));
    env.insert(QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("1.0"));
    spec.environment = env;

    spec.ok = true;
    return spec;
}

ProcessSpec buildInstallKeySpec(const Session &session)
{
    if (session.isLocal())
        return ProcessSpec::failure(
            QStringLiteral("Esta acción solo aplica a sesiones SSH."));

    const QString pub = session.publicKeyPath();
    if (pub.isEmpty() || !QFileInfo::exists(pub)) {
        return ProcessSpec::failure(
            QStringLiteral("No se encontró la llave pública.\n"
                           "Genera una llave primero en el editor de la sesión."));
    }

    ProcessSpec spec;
    spec.program = QStringLiteral("ssh-copy-id");
    spec.args << QStringLiteral("-i") << pub;
    if (session.port != 22)
        spec.args << QStringLiteral("-p") << QString::number(session.port);
    if (!session.proxyJump.trimmed().isEmpty()) {
        spec.args << QStringLiteral("-o")
                  << QStringLiteral("ProxyJump=%1").arg(session.proxyJump.trimmed());
    }
    if (!session.privateKey.trimmed().isEmpty()
        && session.auth == Session::Auth::PublicKey) {
        spec.args << QStringLiteral("-o")
                  << QStringLiteral("IdentityFile=%1").arg(expandPath(session.privateKey));
    }
    spec.args << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=20")
              << sshDestination(session.user, session.host);

    spec.environment = QProcessEnvironment::systemEnvironment();
    spec.ok = true;
    return spec;
}

} // namespace qterm2::ssh
