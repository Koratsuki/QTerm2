#pragma once

#include "ui/statusbar/statusbarcomponent.h"
#include <QLabel>

namespace qterm2 {

class HostnameComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit HostnameComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("hostname"); }
    QString displayName() const override { return QStringLiteral("Hostname"); }
    QWidget *createWidget(QWidget *parent) override;
    void update(const QString &host, const QString &user);

private:
    QLabel *m_label = nullptr;
};

class CPUComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit CPUComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("cpu"); }
    QString displayName() const override { return QStringLiteral("CPU"); }
    QWidget *createWidget(QWidget *parent) override;
};

class MemoryComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit MemoryComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("memory"); }
    QString displayName() const override { return QStringLiteral("Memory"); }
    QWidget *createWidget(QWidget *parent) override;
};

class GitBranchComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit GitBranchComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("git"); }
    QString displayName() const override { return QStringLiteral("Git Branch"); }
    QWidget *createWidget(QWidget *parent) override;
    void update(const QString &branch);

private:
    QLabel *m_label = nullptr;
};

class ClockComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit ClockComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("clock"); }
    QString displayName() const override { return QStringLiteral("Clock"); }
    QWidget *createWidget(QWidget *parent) override;

private:
    QLabel *m_label = nullptr;
    QTimer *m_timer = nullptr;
};

} // namespace qterm2
