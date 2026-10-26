#pragma once

#include "core/apppaths.h"
#include "core/commandregistry.h"
#include "plugins/pluginapi.h"
#include "plugins/pluginpermissions.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace qterm2 {

class CredentialStore;
class Database;
class PluginManager;
class SessionRepository;
class SessionStore;
class SettingsStore;
class SnippetRepository;
class SnippetStore;

/**
 * Composition root for the application.
 *
 * Widgets receive this object (or narrow services from it) instead of
 * constructing databases, settings and plugin loaders themselves.  Tests can
 * build a context rooted at a temporary directory.
 */
class ApplicationContext : public QObject, public PluginHost
{
    Q_OBJECT
public:
    explicit ApplicationContext(const AppPaths &paths = AppPaths::system(),
                                QObject *parent = nullptr);
    ~ApplicationContext() override;

    ApplicationContext(const ApplicationContext &) = delete;
    ApplicationContext &operator=(const ApplicationContext &) = delete;

    bool initialize(QString *errorMessage = nullptr);

    const AppPaths &paths() const { return m_paths; }
    Database *database() const { return m_database.get(); }
    SettingsStore *settings() const { return m_settings.get(); }
    SessionRepository *sessionRepository() const { return m_repository.get(); }
    SessionStore *sessionStore() const { return m_sessions; }
    SnippetRepository *snippetRepository() const { return m_snippetRepository.get(); }
    SnippetStore *snippetStore() const { return m_snippets; }
    CredentialStore *credentials() const { return m_credentials.get(); }
    CommandRegistry *commands() const override { return m_commands; }
    SessionRepository *sessions() const override { return m_repository.get(); }
    PluginManager *plugins() const { return m_plugins; }
    PluginPermissionManager *pluginPermissions() const { return m_pluginPerms.get(); }
    int apiVersion() const override;
    bool hasPermission(const QString &pluginId, PluginPermission permission) const override;
    void requestPermission(const QString &pluginId, PluginPermission permission) override;
    QStringList warnings() const { return m_warnings; }

private:
    AppPaths m_paths;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<SettingsStore> m_settings;
    std::unique_ptr<SessionRepository> m_repository;
    std::unique_ptr<CredentialStore> m_credentials;
    SessionStore *m_sessions = nullptr;
    std::unique_ptr<SnippetRepository> m_snippetRepository;
    SnippetStore *m_snippets = nullptr;
    CommandRegistry *m_commands = nullptr;
    PluginManager *m_plugins = nullptr;
    std::unique_ptr<PluginPermissionManager> m_pluginPerms;
    QStringList m_warnings;
};

} // namespace qterm2
