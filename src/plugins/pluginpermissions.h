#pragma once

#include "plugins/pluginapi.h"
#include <QHash>
#include <QString>
#include <QStringList>

namespace qterm2 {

class PluginPermissionManager
{
public:
    PluginPermissionManager();
    void grant(const QString &pluginId, PluginPermission permission);
    void revoke(const QString &pluginId, PluginPermission permission);
    bool hasPermission(const QString &pluginId, PluginPermission permission) const;
    QStringList permissionsFor(const QString &pluginId) const;
    void loadFromSettings(SettingsStore *settings);
    void saveToSettings(SettingsStore *settings) const;

private:
    QHash<QString, QStringList> m_granted;
};

} // namespace qterm2
