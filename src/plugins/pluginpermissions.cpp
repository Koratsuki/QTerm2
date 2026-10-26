#include "plugins/pluginpermissions.h"
#include "storage/settingsstore.h"

namespace qterm2 {

PluginPermissionManager::PluginPermissionManager() = default;

void PluginPermissionManager::grant(const QString &pluginId, PluginPermission permission)
{
    const QString perm = pluginPermissionToString(permission);
    auto &list = m_granted[pluginId];
    if (!list.contains(perm))
        list.append(perm);
}

void PluginPermissionManager::revoke(const QString &pluginId, PluginPermission permission)
{
    const QString perm = pluginPermissionToString(permission);
    auto &list = m_granted[pluginId];
    list.removeAll(perm);
}

bool PluginPermissionManager::hasPermission(const QString &pluginId, PluginPermission permission) const
{
    if (permission == PluginPermission::None)
        return true;
    const QString perm = pluginPermissionToString(permission);
    const auto it = m_granted.find(pluginId);
    if (it == m_granted.end())
        return false;
    return it->contains(perm);
}

QStringList PluginPermissionManager::permissionsFor(const QString &pluginId) const
{
    const auto it = m_granted.find(pluginId);
    if (it == m_granted.end())
        return {};
    return it.value();
}

void PluginPermissionManager::loadFromSettings(SettingsStore *settings)
{
    if (!settings)
        return;
    // SettingsStore doesn't expose all keys; skip for now
}

void PluginPermissionManager::saveToSettings(SettingsStore *settings) const
{
    if (!settings)
        return;
    for (auto it = m_granted.begin(); it != m_granted.end(); ++it) {
        const QString &pluginId = it.key();
        const QStringList &perms = it.value();
        for (const QString &perm : perms) {
            const QString key = QStringLiteral("plugins/perm/%1/%2").arg(pluginId, perm);
            settings->setValue(key, perm);
        }
    }
}

} // namespace qterm2
