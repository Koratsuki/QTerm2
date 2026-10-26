#include "ssh/keymanager.h"

#include "core/pathutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace qterm2::ssh {

namespace KeyManager {

QString sshDirectory()
{
    return QDir::homePath() + QStringLiteral("/.ssh");
}

QString expandPath(const QString &path)
{
    return qterm2::expandHome(path);
}

bool ensureSshDirectory(QString *errorMessage)
{
    const QString dir = sshDirectory();
    if (!QDir().mkpath(dir)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo crear %1").arg(dir);
        return false;
    }
    QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return true;
}

QStringList listPrivateKeys()
{
    QStringList result;
    QDir dir(sshDirectory());
    if (!dir.exists())
        return result;

    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();
        if (name.endsWith(QStringLiteral(".pub")) || name == QLatin1String("known_hosts")
            || name == QLatin1String("known_hosts.old") || name == QLatin1String("authorized_keys")
            || name == QLatin1String("config") || name == QLatin1String("agent")
            || name.startsWith(QLatin1Char('.')))
            continue;

        // A private key has a matching .pub (or is a PEM file).  Avoid offering
        // random scripts or certificates stored in ~/.ssh.
        const bool hasPub = QFileInfo::exists(fi.absoluteFilePath() + QStringLiteral(".pub"));
        const bool looksLikeKey = hasPub || name.endsWith(QStringLiteral(".pem"))
                                  || name.startsWith(QStringLiteral("id_"));
        if (looksLikeKey)
            result.append(fi.absoluteFilePath());
    }
    result.sort();
    return result;
}

QString publicKeyFor(const QString &privateKeyPath)
{
    const QString key = expandPath(privateKeyPath);
    if (key.isEmpty())
        return QString();
    QFile pub(key + QStringLiteral(".pub"));
    if (pub.open(QIODevice::ReadOnly))
        return QString::fromUtf8(pub.readAll()).trimmed();
    return QString();
}

bool generateEd25519(const QString &privateKeyPath, const QString &comment,
                     const QString &passphrase, QString *errorMessage)
{
    const QString path = expandPath(privateKeyPath);
    if (path.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Ruta de llave vacía.");
        return false;
    }
    if (QFileInfo::exists(path) || QFileInfo::exists(path + QStringLiteral(".pub"))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Ya existe una llave (o su .pub) en:\n%1").arg(path);
        return false;
    }
    const QString parentDir = QFileInfo(path).absolutePath();
    if (QDir::cleanPath(parentDir) == QDir::cleanPath(sshDirectory())) {
        if (!ensureSshDirectory(errorMessage))
            return false;
    }
    if (!QDir().mkpath(parentDir)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo crear la carpeta destino.");
        return false;
    }

    QProcess proc;
    proc.setProgram(QStringLiteral("ssh-keygen"));
    proc.setArguments({ QStringLiteral("-t"), QStringLiteral("ed25519"),
                        QStringLiteral("-a"), QStringLiteral("100"),
                        QStringLiteral("-f"), path,
                        QStringLiteral("-C"), comment,
                        QStringLiteral("-N"), passphrase });
    proc.start();
    if (!proc.waitForStarted(5000)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo ejecutar ssh-keygen.");
        return false;
    }
    proc.waitForFinished(30000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            QString out = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (out.isEmpty())
                out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            *errorMessage = out.isEmpty()
                                ? QStringLiteral("ssh-keygen falló (código %1).").arg(proc.exitCode())
                                : out;
        }
        return false;
    }

    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    QFile::setPermissions(path + QStringLiteral(".pub"),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    return true;
}

QString suggestedKeyPath(const QString &user, const QString &host)
{
    QString base = QStringLiteral("id_ed25519_qterm2");
    if (!host.trimmed().isEmpty()) {
        QString clean = host.trimmed();
        clean.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
        base += QLatin1Char('_') + clean;
    }
    Q_UNUSED(user)
    return sshDirectory() + QLatin1Char('/') + base;
}

} // namespace KeyManager

} // namespace qterm2::ssh
