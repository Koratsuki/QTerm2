#include "app/applicationcontext.h"

#include "plugins/pluginmanager.h"
#include "security/credentialstore.h"
#include "security/libsecretcredentialstore.h"
#include "sessions/sessionstore.h"
#include "sessions/snippetstore.h"
#include "storage/database.h"
#include "storage/legacymigration.h"
#include "storage/settingsstore.h"
#include "storage/sqlitesessionrepository.h"
#include "storage/sqlitesnippetrepository.h"

namespace qterm2 {

ApplicationContext::ApplicationContext(const AppPaths &paths, QObject *parent)
    : QObject(parent)
    , m_paths(paths)
{
}

ApplicationContext::~ApplicationContext()
{
    delete m_snippets;
    m_snippets = nullptr;
    delete m_sessions;
    m_sessions = nullptr;
}

bool ApplicationContext::initialize(QString *errorMessage)
{
    QString error;
    if (!m_paths.ensure(&error)) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }

    m_database = std::make_unique<Database>(m_paths);
    if (!m_database->open(&error)) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }

    m_settings = std::make_unique<SettingsStore>(m_database.get());
    if (!m_settings->initialize(&error)) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }

    m_repository = std::make_unique<SqliteSessionRepository>(m_database.get());
    m_repository->clearRecentSessions();

    const Status migration =
        importLegacyData(m_paths, *m_repository, *m_settings);
    if (!migration.ok)
        m_warnings << migration.message;

    m_sessions = new SessionStore(m_repository.get(), this);
    if (!m_sessions->load())
        m_warnings << m_sessions->lastError();

    m_snippetRepository = std::make_unique<SqliteSnippetRepository>(m_database.get());
    m_snippets = new SnippetStore(m_snippetRepository.get(), this);
    if (!m_snippets->load())
        m_warnings << m_snippets->lastError();

#ifdef QTERM2_HAVE_LIBSECRET
    m_credentials = std::make_unique<LibSecretCredentialStore>();
#else
    m_credentials = std::make_unique<UnavailableCredentialStore>();
#endif

    m_commands = new CommandRegistry(this);
    m_pluginPerms = std::make_unique<PluginPermissionManager>();
    if (m_settings)
        m_pluginPerms->loadFromSettings(m_settings.get());
    m_plugins = new PluginManager(this, this);
    QString pluginError;
    m_plugins->loadAll(&pluginError);
    if (!pluginError.isEmpty())
        m_warnings << pluginError;

    return true;
}

int ApplicationContext::apiVersion() const
{
    return kPluginApiVersion;
}

bool ApplicationContext::hasPermission(const QString &pluginId, PluginPermission permission) const
{
    if (!m_pluginPerms)
        return false;
    return m_pluginPerms->hasPermission(pluginId, permission);
}

void ApplicationContext::requestPermission(const QString &pluginId, PluginPermission permission)
{
    if (m_pluginPerms)
        m_pluginPerms->grant(pluginId, permission);
}

} // namespace qterm2
