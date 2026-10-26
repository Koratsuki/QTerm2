#pragma once

#include "plugins/pluginapi.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QPluginLoader;

namespace qterm2 {

/**
 * Loads QTerm2 plugins from the user and application plugin directories.
 *
 * Loading is deliberately conservative: wrong IID, incompatible API version
 * or a failing initialize() disables only that plugin.
 */
class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(PluginHost *host, QObject *parent = nullptr);
    ~PluginManager() override;

    QStringList searchPaths() const;
    bool loadAll(QString *errorMessage = nullptr);
    void unloadAll();

    int loadedCount() const { return int(m_plugins.size()); }
    QStringList loadedPluginIds() const;
    QStringList errors() const { return m_errors; }

signals:
    void changed();

private:
    struct LoadedPlugin
    {
        QString id;
        QString path;
        QObject *instance = nullptr;
        Plugin *plugin = nullptr;
        QPluginLoader *loader = nullptr;
    };

    void loadFile(const QString &path);

    PluginHost *m_host = nullptr;
    QVector<LoadedPlugin> m_plugins;
    QStringList m_errors;
};

} // namespace qterm2
