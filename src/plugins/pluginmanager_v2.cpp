#include "plugins/pluginmanager_v2.h"

namespace qterm2 {

PluginManagerV2::PluginManagerV2(PluginHost *host, PluginPermissionManager *permMgr, QObject *parent)
    : PluginManager(host, parent)
    , m_permMgr(permMgr)
{
}

QStringList PluginManagerV2::availableThemes() const
{
    QStringList themes;
    // Would iterate loaded plugins and collect from PluginV2
    return themes;
}

QStringList PluginManagerV2::availablePanels() const
{
    QStringList panels;
    return panels;
}

QStringList PluginManagerV2::availableBackends() const
{
    QStringList backends;
    return backends;
}

} // namespace qterm2
