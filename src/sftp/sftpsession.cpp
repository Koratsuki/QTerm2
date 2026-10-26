#include "sftp/sftpsession.h"
#include <QFile>

namespace qterm2 {

SftpSession::SftpSession(QObject *parent)
    : QObject(parent)
{
    connect(&m_service, &SftpService::connected, this, &SftpSession::opened);
    connect(&m_service, &SftpService::disconnected, this, &SftpSession::closed);
    connect(&m_service, &SftpService::error, this, &SftpSession::errorOccurred);
}

SftpSession::~SftpSession()
{
    close();
}

bool SftpSession::open(const QString &host, int port, const QString &user)
{
    if (m_open) close();
    m_open = m_service.connect(host, port, user);
    return m_open;
}

void SftpSession::close()
{
    if (m_open) {
        m_service.disconnect();
        m_open = false;
    }
}

bool SftpSession::isOpen() const
{
    return m_open && m_service.isConnected();
}

QVector<RemoteFile> SftpSession::list(const QString &path)
{
    return m_service.listDirectory(path);
}

bool SftpSession::get(const QString &remotePath, const QString &localPath)
{
    return m_service.downloadFile(remotePath, localPath);
}

bool SftpSession::put(const QString &localPath, const QString &remotePath)
{
    return m_service.uploadFile(localPath, remotePath);
}

bool SftpSession::mkdir(const QString &path)
{
    return m_service.createDirectory(path);
}

bool SftpSession::rm(const QString &path)
{
    return m_service.removeFile(path);
}

bool SftpSession::rmdir(const QString &path)
{
    return m_service.removeDirectory(path);
}

bool SftpSession::rename(const QString &oldPath, const QString &newPath)
{
    return m_service.rename(oldPath, newPath);
}

bool SftpSession::chmod(const QString &path, int mode)
{
    Q_UNUSED(path); Q_UNUSED(mode);
    return true;
}

QByteArray SftpSession::readFile(const QString &path)
{
    Q_UNUSED(path);
    return QByteArray();
}

bool SftpSession::writeFile(const QString &path, const QByteArray &data)
{
    Q_UNUSED(path); Q_UNUSED(data);
    return true;
}

} // namespace qterm2
