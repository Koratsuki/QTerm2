#include "sftp/sftpservice.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace qterm2 {

SftpService::SftpService(QObject *parent)
    : QObject(parent)
{
}

QStringList SftpService::sshBaseArgs() const
{
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10")
         << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    if (m_port != 22)
        args << QStringLiteral("-p") << QString::number(m_port);
    if (!m_keyFile.isEmpty())
        args << QStringLiteral("-i") << m_keyFile;
    return args;
}

QString SftpService::remoteSpec(const QString &path) const
{
    QString dest = m_user.isEmpty() ? m_host : (m_user + QLatin1Char('@') + m_host);
    return dest + QLatin1Char(':') + path;
}

bool SftpService::runSsh(const QStringList &remoteCmd, QString *stdOut) const
{
    QProcess proc;
    QStringList args = sshBaseArgs();
    QString dest = m_user.isEmpty() ? m_host : (m_user + QLatin1Char('@') + m_host);
    args << QStringLiteral("--") << dest << remoteCmd;
    proc.start(QStringLiteral("ssh"), args);
    if (!proc.waitForFinished(30000)) {
        m_lastError = tr("Timeout ejecutando comando remoto.");
        return false;
    }
    if (stdOut)
        *stdOut = QString::fromUtf8(proc.readAllStandardOutput());
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (m_lastError.isEmpty())
            m_lastError = tr("El comando remoto falló (%1).").arg(proc.exitCode());
        return false;
    }
    m_lastError.clear();
    return true;
}

bool SftpService::connect(const QString &host, int port, const QString &user,
                          const QString &keyFile)
{
    m_host = host;
    m_port = port;
    m_user = user;
    m_keyFile = keyFile;
    m_lastError.clear();
    // Probe connectivity
    QString out;
    if (!runSsh({QStringLiteral("echo"), QStringLiteral("ok")}, &out) || !out.contains(QLatin1String("ok"))) {
        m_connected = false;
        if (m_lastError.isEmpty())
            m_lastError = tr("No se pudo conectar por SSH/SFTP.");
        emit error(m_lastError);
        return false;
    }
    m_connected = true;
    emit connected();
    return true;
}

void SftpService::disconnect()
{
    m_connected = false;
    emit disconnected();
}

bool SftpService::isConnected() const
{
    return m_connected;
}

QVector<RemoteFile> SftpService::listDirectory(const QString &path)
{
    QVector<RemoteFile> result;
    if (!m_connected)
        return result;
    QString out;
    // Use ls -la --full-time where possible; fall back to ls -la
    QStringList cmd = {QStringLiteral("ls"), QStringLiteral("-la"), QStringLiteral("--full-time"), path};
    if (!runSsh(cmd, &out)) {
        m_lastError.clear();
        cmd = {QStringLiteral("ls"), QStringLiteral("-la"), path};
        if (!runSsh(cmd, &out))
            return result;
    }
    const QStringList lines = out.split(QLatin1Char('\n'));
    QRegularExpression re(QStringLiteral(R"(^([\-dlbcps\?])([rwxstST\-]{9})\s+\S+\s+(\S+)\s+(\S+)\s+(\d+)\s+(.+\S)\s+(.+)$)"));
    for (const QString &line : lines) {
        const QRegularExpressionMatch m = re.match(line.trimmed());
        if (!m.hasMatch())
            continue;
        RemoteFile f;
        f.permissions = m.captured(1) + m.captured(2);
        f.owner = m.captured(3);
        f.group = m.captured(4);
        f.size = m.captured(5).toLongLong();
        f.name = m.captured(7);
        if (f.name == QLatin1String(".") || f.name == QLatin1String(".."))
            continue;
        // symlink target
        if (f.permissions.startsWith(QLatin1Char('l'))) {
            f.type = FileType::Symlink;
            const int arrow = f.name.indexOf(QStringLiteral(" -> "));
            if (arrow > 0)
                f.name = f.name.left(arrow);
        } else if (f.permissions.startsWith(QLatin1Char('d'))) {
            f.type = FileType::Directory;
        } else {
            f.type = FileType::File;
        }
        f.path = path.endsWith(QLatin1Char('/')) ? path + f.name : path + QLatin1Char('/') + f.name;
        result.append(f);
    }
    return result;
}

bool SftpService::downloadFile(const QString &remotePath, const QString &localPath)
{
    if (!m_connected) { m_lastError = tr("No conectado."); return false; }
    QProcess proc;
    QStringList args = sshBaseArgs();
    args << QStringLiteral("-q") << remoteSpec(remotePath) << localPath;
    proc.start(QStringLiteral("scp"), args);
    if (!proc.waitForFinished(300000)) {
        m_lastError = tr("Timeout descargando archivo.");
        emit error(m_lastError);
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        emit error(m_lastError);
        return false;
    }
    emit transferProgress(QFileInfo(localPath).size(), QFileInfo(localPath).size());
    return true;
}

bool SftpService::uploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected) { m_lastError = tr("No conectado."); return false; }
    QProcess proc;
    QStringList args = sshBaseArgs();
    args << QStringLiteral("-q") << localPath << remoteSpec(remotePath);
    proc.start(QStringLiteral("scp"), args);
    if (!proc.waitForFinished(300000)) {
        m_lastError = tr("Timeout subiendo archivo.");
        emit error(m_lastError);
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        emit error(m_lastError);
        return false;
    }
    emit transferProgress(QFileInfo(localPath).size(), QFileInfo(localPath).size());
    return true;
}

bool SftpService::removeFile(const QString &path)
{
    return runSsh({QStringLiteral("rm"), QStringLiteral("-f"), QStringLiteral("--"), path});
}

bool SftpService::removeDirectory(const QString &path)
{
    return runSsh({QStringLiteral("rm"), QStringLiteral("-rf"), QStringLiteral("--"), path});
}

bool SftpService::createDirectory(const QString &path)
{
    return runSsh({QStringLiteral("mkdir"), QStringLiteral("-p"), QStringLiteral("--"), path});
}

bool SftpService::rename(const QString &oldPath, const QString &newPath)
{
    return runSsh({QStringLiteral("mv"), QStringLiteral("--"), oldPath, newPath});
}

} // namespace qterm2
