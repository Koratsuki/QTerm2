#include "terminal/broadcast/broadcastmanager.h"

namespace qterm2 {

BroadcastManager::BroadcastManager(QObject *parent)
    : QObject(parent)
{
}

void BroadcastManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void BroadcastManager::addTarget(QObject *target)
{
    if (target)
        m_targets.insert(target);
}

void BroadcastManager::removeTarget(QObject *target)
{
    m_targets.remove(target);
}

void BroadcastManager::clearTargets()
{
    m_targets.clear();
}

void BroadcastManager::broadcastInput(const QByteArray &data)
{
    if (!m_enabled)
        return;
    for (QObject *target : m_targets)
        emit inputBroadcasted(data, target);
}

} // namespace qterm2
