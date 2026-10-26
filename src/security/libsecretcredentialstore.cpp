#include "security/libsecretcredentialstore.h"

#ifdef QTERM2_HAVE_LIBSECRET

#include <libsecret/secret.h>

namespace qterm2 {

namespace {

const SecretSchema *credentialSchema()
{
    static const SecretSchema schema = {
        "org.qterm2.Credential",
        SECRET_SCHEMA_NONE,
        {
            { "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SecretSchemaAttributeType(0) },
        },
    };
    return &schema;
}

QString errorText(GError *error)
{
    if (!error)
        return QStringLiteral("Error desconocido de Secret Service.");
    const QString message = QString::fromUtf8(error->message);
    g_error_free(error);
    return message;
}

} // namespace

LibSecretCredentialStore::LibSecretCredentialStore() = default;
LibSecretCredentialStore::~LibSecretCredentialStore() = default;

bool LibSecretCredentialStore::isAvailable() const
{
    return secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, nullptr) != nullptr;
}

QString LibSecretCredentialStore::availabilityHint() const
{
    return QStringLiteral(
        "Secret Service no responde. Comprueba que gnome-keyring o KWallet "
        "estén desbloqueados y que el bus D-Bus de sesión esté disponible.");
}

bool LibSecretCredentialStore::store(const CredentialKey &key, const QString &secret,
                                     QString *errorMessage)
{
    GError *error = nullptr;
    const QByteArray service = key.service.toUtf8();
    const QByteArray account = key.account.toUtf8();
    const QByteArray label = key.label.toUtf8();
    const QByteArray password = secret.toUtf8();
    const gboolean ok = secret_password_store_sync(
        credentialSchema(), SECRET_COLLECTION_DEFAULT, label.constData(),
        password.constData(), nullptr, &error,
        "service", service.constData(), "account", account.constData(), nullptr);
    if (!ok) {
        if (errorMessage)
            *errorMessage = errorText(error);
        return false;
    }
    return true;
}

bool LibSecretCredentialStore::lookup(const CredentialKey &key, QString &secret,
                                      QString *errorMessage) const
{
    GError *error = nullptr;
    const QByteArray service = key.service.toUtf8();
    const QByteArray account = key.account.toUtf8();
    gchar *password = secret_password_lookup_sync(
        credentialSchema(), nullptr, &error,
        "service", service.constData(), "account", account.constData(), nullptr);
    if (error) {
        if (errorMessage)
            *errorMessage = errorText(error);
        return false;
    }
    if (!password) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Credencial no encontrada.");
        return false;
    }
    secret = QString::fromUtf8(password);
    secret_password_free(password);
    return true;
}

bool LibSecretCredentialStore::remove(const CredentialKey &key, QString *errorMessage)
{
    GError *error = nullptr;
    const QByteArray service = key.service.toUtf8();
    const QByteArray account = key.account.toUtf8();
    secret_password_clear_sync(credentialSchema(), nullptr, &error,
                               "service", service.constData(),
                               "account", account.constData(), nullptr);
    if (error) {
        if (errorMessage)
            *errorMessage = errorText(error);
        return false;
    }
    return true;
}

} // namespace qterm2

#endif // QTERM2_HAVE_LIBSECRET
