#pragma once

#include <QDialog>

#include "ssh/sshagent.h"

class QLabel;
class QPushButton;
class QTableWidget;

/**
 * Read-only view of the keys loaded in the running SSH agent.
 *
 * QTerm2 never adds or removes identities automatically: `ssh-add` handles
 * passphrases and hardware tokens, and the dialog only reports what the agent
 * exposes through `ssh-add -l`.
 */
class SshAgentDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SshAgentDialog(const qterm2::ssh::AgentStatus &status,
                            QWidget *parent = nullptr);

private:
    void applyStatus(const qterm2::ssh::AgentStatus &status);
    void refresh();

    qterm2::ssh::AgentStatus m_status;
    QLabel *m_socket = nullptr;
    QLabel *m_state = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_refresh = nullptr;
};
