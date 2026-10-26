#include "tunnels/tunnelmanager.h"
#include <QUuid>
#include <QTimer>

namespace qterm2 {

TunnelManager::TunnelManager(QObject *parent)
    : QObject(parent)
{
}

void TunnelManager::addTunnel(const Tunnel &tunnel)
{
    m_tunnels.append(tunnel);
}

void TunnelManager::startTunnel(const QString &id)
{
    if (auto *t = tunnel(id)) {
        t->state = TunnelState::Running;
        emit tunnelStateChanged(id, t->state);
    }
}

void TunnelManager::stopTunnel(const QString &id)
{
    if (auto *t = tunnel(id)) {
        t->state = TunnelState::Stopped;
        if (m_reconnectTimers.contains(id)) {
            m_reconnectTimers[id]->stop();
        }
        emit tunnelStateChanged(id, t->state);
    }
}

void TunnelManager::restartTunnel(const QString &id)
{
    stopTunnel(id);
    startTunnel(id);
}

Tunnel *TunnelManager::tunnel(const QString &id)
{
    for (auto &t : m_tunnels)
        if (t.id == id) return &t;
    return nullptr;
}

void TunnelManager::onReconnectTimer()
{
    // Reconnection logic with exponential backoff
}

} // namespace qterm2
