#include "ssh/libssh/libsshsession.h"

namespace qterm2 {

LibsshTerminalSession::LibsshTerminalSession(QObject *parent)
    : QObject(parent)
{
}

LibsshTerminalSession::~LibsshTerminalSession()
{
    disconnect();
}

bool LibsshTerminalSession::connect(const QString &host, int port, const QString &user, const QString &authMethod)
{
    Q_UNUSED(host); Q_UNUSED(port); Q_UNUSED(user); Q_UNUSED(authMethod);
    m_connected = true;
    emit connected();
    return true;
}

void LibsshTerminalSession::disconnect()
{
    if (m_connected) {
        m_connected = false;
        emit disconnected(0);
    }
}

bool LibsshTerminalSession::isConnected() const
{
    return m_connected;
}

void LibsshTerminalSession::sendData(const QByteArray &data)
{
    Q_UNUSED(data);
}

void LibsshTerminalSession::resize(int rows, int cols)
{
    Q_UNUSED(rows); Q_UNUSED(cols);
}

} // namespace qterm2
