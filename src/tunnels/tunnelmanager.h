#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QHash>

namespace qterm2 {

enum class TunnelType {
    Local,
    Remote,
    Dynamic
};

enum class TunnelState {
    Stopped,
    Starting,
    Running,
    Failed,
    Reconnecting
};

struct Tunnel {
    QString id;
    QString name;
    TunnelType type;
    TunnelState state;
    QString localHost;
    int localPort = 0;
    QString remoteHost;
    int remotePort = 0;
    QString sessionId;
    int reconnectAttempts = 0;
    int maxReconnectAttempts = 10;
};

class TunnelManager : public QObject
{
    Q_OBJECT
public:
    explicit TunnelManager(QObject *parent = nullptr);

    void addTunnel(const Tunnel &tunnel);
    void startTunnel(const QString &id);
    void stopTunnel(const QString &id);
    void restartTunnel(const QString &id);
    QVector<Tunnel> tunnels() const { return m_tunnels; }
    Tunnel *tunnel(const QString &id);

signals:
    void tunnelStateChanged(const QString &id, TunnelState state);
    void tunnelError(const QString &id, const QString &error);

private slots:
    void onReconnectTimer();

private:
    QVector<Tunnel> m_tunnels;
    QHash<QString, QTimer *> m_reconnectTimers;
};

} // namespace qterm2
