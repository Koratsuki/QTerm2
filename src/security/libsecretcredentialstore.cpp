#include "security/libsecretcredentialstore.h"

#ifdef QTERM2_HAVE_LIBSECRET

#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")

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

} // namespace

LibSecretCredentialStore::LibSecretCredentialStore(QObject *parent)
    : CredentialStore(parent)
{
}

bool LibSecretCredentialStore::isAvailable() const
{
    return true;
}

QString LibSecretCredentialStore::backendName() const
{
    return QStringLiteral("Secret Service (libsecret)");
}

bool LibSecretCredentialStore::getSecret(const QString &service,
                                         const QString &account,
                                         QString &secret,
                                         QString *errorMessage)
{
    GError *error = nullptr;
    gchar *result = secret_password_lookup_sync(credentialSchema(),
                                                nullptr,
                                                &error,
                                                "service", service.toUtf8().constData(),
                                                "account", account.toUtf8().constData(),
                                                nullptr);
    if (error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error->message);
        g_error_free(error);
        return false;
    }
    if (!result) {
        secret.clear();
        return true;
    }
    secret = QString::fromUtf8(result);
    secret_password_free(result);
    return true;
}

bool LibSecretCredentialStore::setSecret(const QString &service,
                                         const QString &account,
                                         const QString &secret,
                                         QString *errorMessage)
{
    GError *error = nullptr;
    const bool ok = secret_password_store_sync(credentialSchema(),
                                               SECRET_COLLECTION_DEFAULT,
                                               qPrintable(QStringLiteral("%1 (%2)").arg(service, account)),
                                               secret.toUtf8().constData(),
                                               nullptr,
                                               &error,
                                               "service", service.toUtf8().constData(),
                                               "account", account.toUtf8().constData(),
                                               nullptr);
    if (!ok || error) {
        if (errorMessage) {
            *errorMessage = error ? QString::fromUtf8(error->message)
                                  : QStringLiteral("Unknown error storing secret");
        }
        if (error)
            g_error_free(error);
        return false;
    }
    return true;
}

bool LibSecretCredentialStore::clearSecret(const QString &service,
                                           const QString &account,
                                           QString *errorMessage)
{
    GError *error = nullptr;
    const bool ok = secret_password_clear_sync(credentialSchema(),
                                                nullptr,
                                                &error,
                                                "service", service.toUtf8().constData(),
                                                "account", account.toUtf8().constData(),
                                                nullptr);
    if (error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error->message);
        g_error_free(error);
        return false;
    }
    return ok;
}

} // namespace qterm2

#endif
