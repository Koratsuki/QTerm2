#include "ui/statusbar/components/basiccomponents.h"
#include <QTimer>
#include <QDateTime>
#include <QHBoxLayout>

namespace qterm2 {

HostnameComponent::HostnameComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *HostnameComponent::createWidget(QWidget *parent)
{
    m_label = new QLabel(parent);
    m_label->setObjectName(QStringLiteral("StatusBarLabel"));
    return m_label;
}

void HostnameComponent::update(const QString &host, const QString &user)
{
    if (m_label) {
        if (!host.isEmpty())
            m_label->setText(QStringLiteral("%1@%2").arg(user, host));
        else if (!user.isEmpty())
            m_label->setText(user);
        else
            m_label->setText(QString());
    }
}

CPUComponent::CPUComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *CPUComponent::createWidget(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("StatusBarLabel"));
    label->setText(QStringLiteral("CPU: --%"));
    return label;
}

MemoryComponent::MemoryComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *MemoryComponent::createWidget(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("StatusBarLabel"));
    label->setText(QStringLiteral("MEM: --"));
    return label;
}

GitBranchComponent::GitBranchComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *GitBranchComponent::createWidget(QWidget *parent)
{
    m_label = new QLabel(parent);
    m_label->setObjectName(QStringLiteral("StatusBarLabel"));
    return m_label;
}

void GitBranchComponent::update(const QString &branch)
{
    if (m_label) {
        if (!branch.isEmpty())
            m_label->setText(QStringLiteral("git:%1").arg(branch));
        else
            m_label->setText(QString());
    }
}

ClockComponent::ClockComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *ClockComponent::createWidget(QWidget *parent)
{
    m_label = new QLabel(parent);
    m_label->setObjectName(QStringLiteral("StatusBarLabel"));
    m_timer = new QTimer(m_label);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_label)
            m_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    });
    m_timer->start(1000);
    if (m_label)
        m_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    return m_label;
}

} // namespace qterm2
