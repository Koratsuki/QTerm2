#include "plugins/pluginpermissions.h"
#include "storage/settingsstore.h"

#include <QStringList>

namespace qterm2 {

PluginPermissionManager::PluginPermissionManager() = default;

void PluginPermissionManager::grant(const QString &pluginId, PluginPermission permission)
{
    if (permission == PluginPermission::None)
        return;
    const QString perm = pluginPermissionToString(permission);
    auto &list = m_granted[pluginId];
    if (!list.contains(perm))
        list.append(perm);
}

void PluginPermissionManager::revoke(const QString &pluginId, PluginPermission permission)
{
    if (permission == PluginPermission::None)
        return;
    const QString perm = pluginPermissionToString(permission);
    auto &list = m_granted[pluginId];
    list.removeAll(perm);
    if (list.isEmpty())
        m_granted.remove(pluginId);
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
    m_granted.clear();
    if (!settings)
        return;
    // Permissions are stored as one string-list value per plugin so they can be
    // loaded back without enumerating setting keys.  (Legacy per-permission keys
    // written by earlier builds are not enumerated by SettingsStore.)
    const QStringList knownIds = settings->value(
        QStringLiteral("plugins/permission.ids")).toStringList();
    for (const QString &pluginId : knownIds) {
        if (pluginId.trimmed().isEmpty())
            continue;
        const QStringList perms = settings->value(
            QStringLiteral("plugins/perms/%1").arg(pluginId)).toStringList();
        QStringList filtered;
        for (const QString &perm : perms) {
            if (!perm.trimmed().isEmpty() && !filtered.contains(perm))
                filtered.append(perm);
        }
        if (!filtered.isEmpty())
            m_granted.insert(pluginId, filtered);
    }
}

void PluginPermissionManager::saveToSettings(SettingsStore *settings) const
{
    if (!settings)
        return;
    const QStringList ids = m_granted.keys();
    settings->setValue(QStringLiteral("plugins/permission.ids"), ids);
    for (auto it = m_granted.begin(); it != m_granted.end(); ++it)
        settings->setValue(QStringLiteral("plugins/perms/%1").arg(it.key()),
                           it.value());
}

} // namespace qterm2
