#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

namespace qterm2 {

class LibsshTerminalSession : public QObject
{
    Q_OBJECT
public:
    explicit LibsshTerminalSession(QObject *parent = nullptr);
    ~LibsshTerminalSession() override;

    bool connect(const QString &host, int port, const QString &user, const QString &authMethod);
    void disconnect();
    bool isConnected() const;

    void sendData(const QByteArray &data);
    void resize(int rows, int cols);

signals:
    void dataReceived(const QByteArray &data);
    void connected();
    void disconnected(int exitCode);
    void error(const QString &message);

private:
    bool m_connected = false;
};

} // namespace qterm2
