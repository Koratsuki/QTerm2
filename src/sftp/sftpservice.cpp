#include "sftp/sftpservice.h"
#include <QDebug>

namespace qterm2 {

SftpService::SftpService(QObject *parent)
    : QObject(parent)
{
}

bool SftpService::connect(const QString &host, int port, const QString &user)
{
    Q_UNUSED(host); Q_UNUSED(port); Q_UNUSED(user);
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
    Q_UNUSED(path);
    return QVector<RemoteFile>();
}

bool SftpService::downloadFile(const QString &remotePath, const QString &localPath)
{
    Q_UNUSED(remotePath); Q_UNUSED(localPath);
    emit transferProgress(100, 100);
    return true;
}

bool SftpService::uploadFile(const QString &localPath, const QString &remotePath)
{
    Q_UNUSED(localPath); Q_UNUSED(remotePath);
    emit transferProgress(100, 100);
    return true;
}

bool SftpService::removeFile(const QString &path)
{
    Q_UNUSED(path);
    return true;
}

bool SftpService::removeDirectory(const QString &path)
{
    Q_UNUSED(path);
    return true;
}

bool SftpService::createDirectory(const QString &path)
{
    Q_UNUSED(path);
    return true;
}

bool SftpService::rename(const QString &oldPath, const QString &newPath)
{
    Q_UNUSED(oldPath); Q_UNUSED(newPath);
    return true;
}

} // namespace qterm2
