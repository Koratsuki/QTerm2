#include "terminal/integration/shellintegration.h"

namespace qterm2 {

ShellIntegration::ShellIntegration(QObject *parent)
    : QObject(parent)
{
}

void ShellIntegration::handleOsc133(const QByteArray &arg)
{
    const QString value = QString::fromUtf8(arg);
    if (value.isEmpty()) return;
    const QChar c = value.at(0);
    if (c == QLatin1Char('A')) {
        m_state = ShellIntegrationState::Prompt;
        m_current = CommandBlock();
        m_current.startTime = QDateTime::currentDateTime();
        emit promptShown();
    } else if (c == QLatin1Char('B')) {
        m_state = ShellIntegrationState::Command;
    } else if (c == QLatin1Char('C')) {
        m_state = ShellIntegrationState::Output;
    } else if (c == QLatin1Char('D')) {
        const int sep = value.indexOf(QLatin1Char(';'));
        if (sep >= 0) {
            bool ok = false;
            m_current.exitCode = value.mid(sep + 1).toInt(&ok);
            if (!ok) m_current.exitCode = -1;
        }
        m_current.endTime = QDateTime::currentDateTime();
        if (m_state != ShellIntegrationState::Unknown) {
            m_history.append(m_current);
            emit commandFinished(m_current.command, m_current.exitCode);
        }
        m_state = ShellIntegrationState::Prompt;
    }
}

} // namespace qterm2
