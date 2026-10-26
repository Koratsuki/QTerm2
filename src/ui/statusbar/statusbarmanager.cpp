#include "ui/statusbar/statusbarmanager.h"
#include "ui/statusbar/components/basiccomponents.h"
#include "ui/statusbar/components/systemcomponents.h"
#include "domain/session.h"
#include <QStatusBar>
#include <QSettings>

namespace qterm2 {

StatusBarManager::StatusBarManager(QObject *parent)
    : QObject(parent)
{
    m_enabledComponents = {QStringLiteral("hostname"), QStringLiteral("uptime"), QStringLiteral("cpugraph"), QStringLiteral("memgraph"), QStringLiteral("git"), QStringLiteral("clock")};

    registerComponent(new HostnameComponent(this));
    registerComponent(new GitBranchComponent(this));
    registerComponent(new ClockComponent(this));
    registerComponent(new CPUComponent(this));
    registerComponent(new MemoryComponent(this));
    registerComponent(new UptimeComponent(this));
    registerComponent(new CPUGraphComponent(this));
    registerComponent(new MemoryGraphComponent(this));
}

void StatusBarManager::registerComponent(StatusBarComponent *component)
{
    if (component)
        m_components[component->componentId()] = component;
}

void StatusBarManager::unregisterComponent(const QString &componentId)
{
        m_components.remove(componentId);
}

StatusBarComponent *StatusBarManager::component(const QString &componentId) const
{
    return m_components.value(componentId);
}

QList<StatusBarComponent *> StatusBarManager::components() const
{
    return m_components.values();
}

QStringList StatusBarManager::enabledComponents() const
{
    return m_enabledComponents;
}

void StatusBarManager::setEnabledComponents(const QStringList &components)
{
    m_enabledComponents = components;
    emit configurationChanged();
}

void StatusBarManager::buildStatusBar(QWidget *statusBar)
{
    auto *sb = qobject_cast<QStatusBar *>(statusBar);
    if (!sb)
        return;
    for (const QString &id : m_enabledComponents) {
        if (auto *comp = m_components.value(id)) {
            if (auto *w = comp->createWidget(sb)) {
                sb->addPermanentWidget(w);
            }
        }
    }
}

void StatusBarManager::updateComponents(const Session &session)
{
    if (auto *hc = qobject_cast<HostnameComponent *>(m_components.value(QStringLiteral("hostname"))))
        hc->update(session.host, session.user);
}

} // namespace qterm2
