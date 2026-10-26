#pragma once

#include <QHash>
#include <QString>

namespace qterm2 {

struct CredentialKey
{
    QString service;
    QString account;
    QString label;

    bool isValid() const { return !service.isEmpty() && !account.isEmpty(); }

    static CredentialKey sessionPassword(const QString &sessionId);
    static CredentialKey keyPassphrase(const QString &privateKeyPath);
};

/**
 * Credential storage boundary.
 *
 * QTerm2 never stores passwords, passphrases or tokens in SQLite, JSON or
 * settings files.  When the user explicitly asks to remember a secret, it is
 * written to the platform Secret Service.  Password authentication itself is
 * delegated to OpenSSH, which prompts interactively inside the PTY.
 */
class CredentialStore
{
public:
    virtual ~CredentialStore() = default;

    virtual QString backendName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual QString availabilityHint() const = 0;

    virtual bool store(const CredentialKey &key, const QString &secret,
                       QString *errorMessage = nullptr) = 0;
    virtual bool lookup(const CredentialKey &key, QString &secret,
                        QString *errorMessage = nullptr) const = 0;
    virtual bool remove(const CredentialKey &key,
                        QString *errorMessage = nullptr) = 0;
};

/** Backend used when no Secret Service implementation is compiled in. */
class UnavailableCredentialStore final : public CredentialStore
{
public:
    QString backendName() const override { return QStringLiteral("none"); }
    bool isAvailable() const override { return false; }
    QString availabilityHint() const override;
    bool store(const CredentialKey &, const QString &,
               QString *errorMessage = nullptr) override;
    bool lookup(const CredentialKey &, QString &,
                QString *errorMessage = nullptr) const override;
    bool remove(const CredentialKey &, QString *errorMessage = nullptr) override;
};

/**
 * In-memory backend for automated tests only.
 *
 * It is intentionally not selectable from the application configuration.
 */
class MemoryCredentialStore final : public CredentialStore
{
public:
    QString backendName() const override { return QStringLiteral("memory-test"); }
    bool isAvailable() const override { return true; }
    QString availabilityHint() const override { return QString(); }
    bool store(const CredentialKey &key, const QString &secret,
               QString *errorMessage = nullptr) override;
    bool lookup(const CredentialKey &key, QString &secret,
                QString *errorMessage = nullptr) const override;
    bool remove(const CredentialKey &key,
                QString *errorMessage = nullptr) override;

private:
    QString mapKey(const CredentialKey &key) const;
    QHash<QString, QString> m_secrets;
};

} // namespace qterm2
