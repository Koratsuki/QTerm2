#pragma once

#include "plugins/pluginmanager.h"
#include "plugins/pluginpermissions.h"

namespace qterm2 {

class PluginManagerV2 : public PluginManager
{
public:
    explicit PluginManagerV2(PluginHost *host, PluginPermissionManager *permMgr, QObject *parent = nullptr);
    
    QStringList availableThemes() const;
    QStringList availablePanels() const;
    QStringList availableBackends() const;

private:
    PluginPermissionManager *m_permMgr = nullptr;
};

} // namespace qterm2
