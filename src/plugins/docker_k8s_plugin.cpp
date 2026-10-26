#include "plugins/docker_k8s_plugin.h"
#include "core/commandregistry.h"

namespace qterm2 {

DockerK8sPlugin::DockerK8sPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginMetadata DockerK8sPlugin::metadata() const
{
    PluginMetadata m;
    m.id = QStringLiteral("org.qterm2.docker-k8s");
    m.name = QStringLiteral("Docker/Kubernetes Integration");
    m.version = QStringLiteral("1.0.0");
    m.vendor = QStringLiteral("QTerm2");
    m.description = QStringLiteral("Attach to Docker containers and Kubernetes pods");
    m.apiVersion = 2;
    m.permissions = {QStringLiteral("containers.read"), QStringLiteral("containers.exec"), QStringLiteral("commands.register")};
    return m;
}

bool DockerK8sPlugin::initialize(PluginHost *host, QString *errorMessage)
{
    m_host = host;
    
    if (!host->hasPermission(m.id, PluginPermission::CommandsRegister)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Permission denied: commands.register");
        return false;
    }
    
    if (host->commands()) {
        qterm2::Command cmd;
        cmd.id = QStringLiteral("qterm2.docker.attach");
        cmd.title = QStringLiteral("Docker: Attach to container");
        cmd.category = QStringLiteral("Containers");
        cmd.action = [] {};
        host->commands()->registerCommand(cmd);
        
        qterm2::Command cmd2;
        cmd2.id = QStringLiteral("qterm2.k8s.exec");
        cmd2.title = QStringLiteral("Kubernetes: Exec into pod");
        cmd2.category = QStringLiteral("Containers");
        cmd2.action = [] {};
        host->commands()->registerCommand(cmd2);
    }
    
    return true;
}

void DockerK8sPlugin::shutdown()
{
}

} // namespace qterm2
