#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace qterm2::ssh {

/** One key currently loaded in the running SSH agent. */
struct AgentIdentity
{
    QString bits;
    QString fingerprint;
    QString comment;
    QString keyType;
};

struct AgentStatus
{
    bool available = false;      // ssh-add could talk to an agent
    QString socketPath;          // SSH_AUTH_SOCK
    QString error;
    QVector<AgentIdentity> identities;
};

/** Returns the agent socket from SSH_AUTH_SOCK, or an empty string. */
QString agentSocketPath();

/**
 * Parses the output of `ssh-add -l`.
 *
 * Handles both the modern SHA256 fingerprints and legacy MD5 ones:
 * `256 SHA256:abc... user@host (ED25519)`.
 */
QVector<AgentIdentity> parseAgentIdentities(const QString &output);

/**
 * Queries the running agent through `ssh-add -l`.
 *
 * This never prompts and never reads private key material. When the agent is
 * unreachable, `status.error` explains why in user-facing Spanish.
 */
AgentStatus queryAgent();

} // namespace qterm2::ssh
