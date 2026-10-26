#pragma once

#include <QString>
#include <QStringList>
#include <QtPlugin>

namespace qterm2 {

class CommandRegistry;
class SessionRepository;
class SettingsStore;

struct PluginMetadata
{
    QString id;
    QString name;
    QString version;
    QString vendor;
    QString description;
    int apiVersion = 1;
    QStringList permissions;
};

/**
 * Permission requested by a plugin. Plugins must declare permissions in metadata
 * and the host will validate them against an allowlist.
 */
enum class PluginPermission {
    None,
    SessionsRead,
    SessionsWrite,
    CommandsRegister,
    SettingsRead,
    SettingsWrite,
    NetworkLocal,
    ContainerRead,
    ContainerExec,
    FileSystemRead,
    FileSystemWrite
};

QString pluginPermissionToString(PluginPermission permission);
PluginPermission pluginPermissionFromString(const QString &value);

/**
 * Narrow host surface exposed to plugins.
 *
 * Plugins register commands and read session metadata; they do not receive
 * QWidget instances or OpenSSH implementation details. New capabilities are
 * added in later API versions, never by widening this interface in place.
 */
class PluginHost
{
public:
    virtual ~PluginHost() = default;
    virtual int apiVersion() const = 0;
    virtual CommandRegistry *commands() const = 0;
    virtual SessionRepository *sessions() const = 0;
    virtual SettingsStore *settings() const = 0;
    virtual bool hasPermission(const QString &pluginId, PluginPermission permission) const = 0;
    virtual void requestPermission(const QString &pluginId, PluginPermission permission) = 0;
};

/**
 * Stable plugin contract.
 *
 * Build plugins against the QTerm2 development headers and IID below. A
 * plugin whose apiVersion is not supported is rejected before initialize().
 */
class Plugin
{
public:
    virtual ~Plugin() = default;
    virtual PluginMetadata metadata() const = 0;
    virtual bool initialize(PluginHost *host, QString *errorMessage) = 0;
    virtual void shutdown() = 0;
};

class PluginV2 : public Plugin
{
public:
    virtual ~PluginV2() = default;
    /** Optional: called when a new theme is requested */
    virtual QStringList providedThemes() const { return {}; }
    /** Optional: called when a new panel type is registered */
    virtual QStringList providedPanels() const { return {}; }
    /** Optional: called for custom backends */
    virtual QStringList providedBackends() const { return {}; }
};

constexpr int kPluginApiVersion = 2;
constexpr int kPluginApiVersionMin = 1;

} // namespace qterm2

#define QTerm2Plugin_iid "org.qterm2.QTerm2Plugin/1.0"
Q_DECLARE_INTERFACE(qterm2::Plugin, QTerm2Plugin_iid)
