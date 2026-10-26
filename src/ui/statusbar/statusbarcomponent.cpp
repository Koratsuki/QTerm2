#include "ui/statusbar/statusbarcomponent.h"

namespace qterm2 {

StatusBarComponent::StatusBarComponent(QObject *parent)
    : QObject(parent)
{
}

void StatusBarComponent::setConfiguration(const QVariantMap &config)
{
    m_config = config;
}

QVariantMap StatusBarComponent::configuration() const
{
    return m_config;
}

} // namespace qterm2
