#pragma once

#include "plugins/pluginapi.h"
#include <QObject>

namespace qterm2 {

/**
 * Docker/K8s integration plugin (API v2)
 * Provides commands to attach to containers/pods via docker exec/kubectl exec
 */
class DockerK8sPlugin : public QObject, public PluginV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QTerm2Plugin_iid FILE "docker_k8s.json")
    Q_INTERFACES(qterm2::Plugin)

public:
    explicit DockerK8sPlugin(QObject *parent = nullptr);
    PluginMetadata metadata() const override;
    bool initialize(PluginHost *host, QString *errorMessage) override;
    void shutdown() override;

    QStringList providedThemes() const override { return {}; }
    QStringList providedPanels() const override { return {"containers"}; }
    QStringList providedBackends() const override { return {"docker", "kubernetes"}; }

private:
    PluginHost *m_host = nullptr;
};

} // namespace qterm2
