#include "sftp/sftpsession.h"

#include <QFile>
#include <QTemporaryFile>

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

bool SftpSession::open(const QString &host, int port, const QString &user,
                       const QString &keyFile)
{
    if (m_open)
        close();
    m_open = m_service.connect(host, port, user, keyFile);
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
    // Could be implemented via ssh chmod, but keep simple for now.
    return isOpen();
}

QByteArray SftpSession::readFile(const QString &path)
{
    if (!isOpen())
        return {};
    QTemporaryFile tmp;
    if (!tmp.open())
        return {};
    tmp.close();
    if (!m_service.downloadFile(path, tmp.fileName()))
        return {};
    QFile f(tmp.fileName());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

bool SftpSession::writeFile(const QString &path, const QByteArray &data)
{
    if (!isOpen())
        return false;
    QTemporaryFile tmp;
    if (!tmp.open())
        return false;
    tmp.write(data);
    tmp.flush();
    tmp.close();
    return m_service.uploadFile(tmp.fileName(), path);
}

} // namespace qterm2
