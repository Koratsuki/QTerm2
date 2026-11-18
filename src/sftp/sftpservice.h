#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>

namespace qterm2 {

enum class FileType {
    File,
    Directory,
    Symlink,
    Unknown
};

struct RemoteFile {
    QString name;
    QString path;
    FileType type = FileType::Unknown;
    qint64 size = 0;
    QDateTime modified;
    QString permissions;
    QString owner;
    QString group;
};

class SftpService : public QObject
{
    Q_OBJECT
public:
    explicit SftpService(QObject *parent = nullptr);

    bool connect(const QString &host, int port, const QString &user,
                 const QString &keyFile = QString());
    void disconnect();
    bool isConnected() const;

    QVector<RemoteFile> listDirectory(const QString &path);
    bool downloadFile(const QString &remotePath, const QString &localPath);
    bool uploadFile(const QString &localPath, const QString &remotePath);
    bool removeFile(const QString &path);
    bool removeDirectory(const QString &path);
    bool createDirectory(const QString &path);
    bool rename(const QString &oldPath, const QString &newPath);

    QString lastError() const { return m_lastError; }

signals:
    void connected();
    void disconnected();
    void transferProgress(qint64 bytes, qint64 total);
    void error(const QString &message);

private:
    QStringList sshBaseArgs() const;
    QString remoteSpec(const QString &path) const;
    bool runSsh(const QStringList &remoteCmd, QString *stdOut = nullptr) const;

    bool m_connected = false;
    QString m_host;
    int m_port = 22;
    QString m_user;
    QString m_keyFile;
    mutable QString m_lastError;
};

} // namespace qterm2
