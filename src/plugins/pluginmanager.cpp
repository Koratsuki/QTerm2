#include "plugins/pluginmanager.h"

#include "core/commandregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>

namespace qterm2 {

PluginManager::PluginManager(PluginHost *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
}

QStringList PluginManager::searchPaths() const
{
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();

    paths << QDir::cleanPath(appDir + QStringLiteral("/plugins"));
    paths << QDir::cleanPath(appDir + QStringLiteral("/../lib/qterm2/plugins"));
    paths << QDir::homePath() + QStringLiteral("/.local/lib/qterm2/plugins");

    QStringList result;
    for (const QString &path : paths) {
        if (!result.contains(path))
            result.append(path);
    }
    return result;
}

bool PluginManager::loadAll(QString *errorMessage)
{
    unloadAll();
    m_errors.clear();

    for (const QString &dirPath : searchPaths()) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QFileInfoList entries =
            dir.entryInfoList(QStringList() << QStringLiteral("*.so")
                                            << QStringLiteral("*.dylib"),
                              QDir::Files | QDir::Readable);
        for (const QFileInfo &entry : entries)
            loadFile(entry.absoluteFilePath());
    }

    if (!m_errors.isEmpty() && errorMessage)
        *errorMessage = m_errors.join(QLatin1Char('\n'));
    emit changed();
    return true;
}

void PluginManager::loadFile(const QString &path)
{
    auto *loader = new QPluginLoader(path, this);
    QObject *instance = loader->instance();
    if (!instance) {
        m_errors << QStringLiteral("%1: %2").arg(path, loader->errorString());
        delete loader;
        return;
    }

    auto *plugin = qobject_cast<Plugin *>(instance);
    if (!plugin) {
        m_errors << QStringLiteral("%1: no implementa la interfaz QTerm2Plugin").arg(path);
        delete loader;
        return;
    }

    const PluginMetadata metadata = plugin->metadata();
    if (metadata.id.trimmed().isEmpty()) {
        m_errors << QStringLiteral("%1: metadatos sin id").arg(path);
        delete loader;
        return;
    }
    if (metadata.apiVersion != kPluginApiVersion) {
        m_errors << QStringLiteral("%1: API %2 no soportada (se esperaba %3)")
                        .arg(path)
                        .arg(metadata.apiVersion)
                        .arg(kPluginApiVersion);
        delete loader;
        return;
    }
    for (const LoadedPlugin &loaded : m_plugins) {
        if (loaded.id == metadata.id) {
            m_errors << QStringLiteral("%1: plugin duplicado %2").arg(path, metadata.id);
            delete loader;
            return;
        }
    }

    QString error;
    if (!plugin->initialize(m_host, &error)) {
        m_errors << QStringLiteral("%1: %2")
                        .arg(path, error.isEmpty() ? QStringLiteral("initialize() falló")
                                                   : error);
        plugin->shutdown();
        delete loader;
        return;
    }

    LoadedPlugin loaded;
    loaded.id = metadata.id;
    loaded.path = path;
    loaded.instance = instance;
    loaded.plugin = plugin;
    loaded.loader = loader;
    m_plugins.append(loaded);
}

void PluginManager::unloadAll()
{
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it) {
        if (it->plugin)
            it->plugin->shutdown();
        if (it->loader) {
            it->loader->unload();
            delete it->loader;
        }
    }
    m_plugins.clear();
    emit changed();
}

QStringList PluginManager::loadedPluginIds() const
{
    QStringList ids;
    for (const LoadedPlugin &loaded : m_plugins)
        ids << loaded.id;
    return ids;
}

} // namespace qterm2
