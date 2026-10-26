#include "plugins/pluginapi.h"

namespace qterm2 {

QString pluginPermissionToString(PluginPermission permission)
{
    switch (permission) {
    case PluginPermission::None: return QStringLiteral("none");
    case PluginPermission::SessionsRead: return QStringLiteral("sessions.read");
    case PluginPermission::SessionsWrite: return QStringLiteral("sessions.write");
    case PluginPermission::CommandsRegister: return QStringLiteral("commands.register");
    case PluginPermission::SettingsRead: return QStringLiteral("settings.read");
    case PluginPermission::SettingsWrite: return QStringLiteral("settings.write");
    case PluginPermission::NetworkLocal: return QStringLiteral("network.local");
    case PluginPermission::ContainerRead: return QStringLiteral("containers.read");
    case PluginPermission::ContainerExec: return QStringLiteral("containers.exec");
    case PluginPermission::FileSystemRead: return QStringLiteral("fs.read");
    case PluginPermission::FileSystemWrite: return QStringLiteral("fs.write");
    }
    return QStringLiteral("none");
}

PluginPermission pluginPermissionFromString(const QString &value)
{
    if (value == QLatin1String("sessions.read")) return PluginPermission::SessionsRead;
    if (value == QLatin1String("sessions.write")) return PluginPermission::SessionsWrite;
    if (value == QLatin1String("commands.register")) return PluginPermission::CommandsRegister;
    if (value == QLatin1String("settings.read")) return PluginPermission::SettingsRead;
    if (value == QLatin1String("settings.write")) return PluginPermission::SettingsWrite;
    if (value == QLatin1String("network.local")) return PluginPermission::NetworkLocal;
    if (value == QLatin1String("containers.read")) return PluginPermission::ContainerRead;
    if (value == QLatin1String("containers.exec")) return PluginPermission::ContainerExec;
    if (value == QLatin1String("fs.read")) return PluginPermission::FileSystemRead;
    if (value == QLatin1String("fs.write")) return PluginPermission::FileSystemWrite;
    return PluginPermission::None;
}

} // namespace qterm2
