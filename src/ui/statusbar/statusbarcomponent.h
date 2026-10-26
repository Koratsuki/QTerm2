#pragma once

#include <QObject>
#include <QString>
#include <QWidget>
#include <QVariantMap>

namespace qterm2 {

class StatusBarComponent : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarComponent(QObject *parent = nullptr);
    virtual ~StatusBarComponent() = default;

    virtual QString componentId() const = 0;
    virtual QString displayName() const = 0;
    virtual QWidget *createWidget(QWidget *parent) = 0;
    virtual bool isEnabled() const { return true; }
    virtual int priority() const { return 0; }

    void setConfiguration(const QVariantMap &config);
    QVariantMap configuration() const;

signals:
    void needsUpdate();

protected:
    QVariantMap m_config;
};

} // namespace qterm2
