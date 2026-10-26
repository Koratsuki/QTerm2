#pragma once

#include <QObject>
#include <QList>
#include <QStringList>
#include <QWidget>
#include "ui/statusbar/statusbarcomponent.h"
#include "domain/session.h"

namespace qterm2 {

class StatusBarManager : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarManager(QObject *parent = nullptr);

    void registerComponent(StatusBarComponent *component);
    void unregisterComponent(const QString &componentId);
    StatusBarComponent *component(const QString &componentId) const;
    QList<StatusBarComponent *> components() const;

    QStringList enabledComponents() const;
    void setEnabledComponents(const QStringList &components);

    void buildStatusBar(QWidget *statusBar);
    void updateComponents(const Session &session);

signals:
    void configurationChanged();

private:
    QHash<QString, StatusBarComponent *> m_components;
    QStringList m_enabledComponents;
};

} // namespace qterm2
