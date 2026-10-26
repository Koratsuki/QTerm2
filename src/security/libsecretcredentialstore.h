#pragma once

#include "security/credentialstore.h"

#ifdef QTERM2_HAVE_LIBSECRET

namespace qterm2 {

/** Secret Service backend implemented through libsecret. */
class LibSecretCredentialStore final : public CredentialStore
{
public:
    LibSecretCredentialStore();
    ~LibSecretCredentialStore() override;

    QString backendName() const override { return QStringLiteral("libsecret"); }
    bool isAvailable() const override;
    QString availabilityHint() const override;

    bool store(const CredentialKey &key, const QString &secret,
               QString *errorMessage) override;
    bool lookup(const CredentialKey &key, QString &secret,
                QString *errorMessage) const override;
    bool remove(const CredentialKey &key, QString *errorMessage) override;
};

} // namespace qterm2

#endif // QTERM2_HAVE_LIBSECRET
