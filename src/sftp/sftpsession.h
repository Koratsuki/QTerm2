#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include "sftp/sftpservice.h"

namespace qterm2 {

class SftpSession : public QObject
{
    Q_OBJECT
public:
    explicit SftpSession(QObject *parent = nullptr);
    ~SftpSession() override;

    bool open(const QString &host, int port, const QString &user);
    void close();
    bool isOpen() const;

    QVector<RemoteFile> list(const QString &path);
    bool get(const QString &remotePath, const QString &localPath);
    bool put(const QString &localPath, const QString &remotePath);
    bool mkdir(const QString &path);
    bool rm(const QString &path);
    bool rmdir(const QString &path);
    bool rename(const QString &oldPath, const QString &newPath);
    bool chmod(const QString &path, int mode);
    QByteArray readFile(const QString &path);
    bool writeFile(const QString &path, const QByteArray &data);

signals:
    void opened();
    void closed();
    void errorOccurred(const QString &message);

private:
    bool m_open = false;
    SftpService m_service;
};

} // namespace qterm2
