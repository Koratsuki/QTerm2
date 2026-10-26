#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>

namespace qterm2 {

enum class ShellIntegrationState {
    Unknown,
    Prompt,
    Command,
    Output
};

struct CommandBlock {
    QString command;
    QString output;
    QString directory;
    QDateTime startTime;
    QDateTime endTime;
    int exitCode = -1;
    int startLine = 0;
    int endLine = 0;
};

class ShellIntegration : public QObject
{
    Q_OBJECT
public:
    explicit ShellIntegration(QObject *parent = nullptr);

    void handleOsc133(const QByteArray &arg);
    ShellIntegrationState state() const { return m_state; }
    QVector<CommandBlock> commandHistory() const { return m_history; }
    CommandBlock currentCommand() const { return m_current; }

signals:
    void commandStarted(const QString &command);
    void commandFinished(const QString &command, int exitCode);
    void promptShown();
    void workingDirectoryChanged(const QString &path);

private:
    ShellIntegrationState m_state = ShellIntegrationState::Unknown;
    QVector<CommandBlock> m_history;
    CommandBlock m_current;
    QString m_pendingCommand;
};

} // namespace qterm2
