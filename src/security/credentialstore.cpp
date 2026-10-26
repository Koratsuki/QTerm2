#include "security/credentialstore.h"

#include <QFileInfo>
#include <QHash>

namespace qterm2 {

CredentialKey CredentialKey::sessionPassword(const QString &sessionId)
{
    CredentialKey key;
    key.service = QStringLiteral("org.qterm2.QTerm2");
    key.account = QStringLiteral("session/%1/password").arg(sessionId);
    key.label = QStringLiteral("QTerm2: contraseña de sesión %1").arg(sessionId);
    return key;
}

CredentialKey CredentialKey::keyPassphrase(const QString &privateKeyPath)
{
    CredentialKey key;
    key.service = QStringLiteral("org.qterm2.QTerm2");
    key.account = QStringLiteral("key/%1/passphrase")
                      .arg(QFileInfo(privateKeyPath).absoluteFilePath());
    key.label = QStringLiteral("QTerm2: frase de paso de %1")
                    .arg(QFileInfo(privateKeyPath).fileName());
    return key;
}

QString UnavailableCredentialStore::availabilityHint() const
{
    return QStringLiteral(
        "No hay un backend Secret Service disponible. QTerm2 no guardará "
        "contraseñas ni frases de paso; OpenSSH las pedirá en la terminal "
        "cuando sean necesarias. Instala libsecret y un agente de Secret "
        "Service (GNOME Keyring o KWallet con el puente correspondiente).");
}

bool UnavailableCredentialStore::store(const CredentialKey &, const QString &,
                                       QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = availabilityHint();
    return false;
}

bool UnavailableCredentialStore::lookup(const CredentialKey &, QString &,
                                        QString *errorMessage) const
{
    if (errorMessage)
        *errorMessage = availabilityHint();
    return false;
}

bool UnavailableCredentialStore::remove(const CredentialKey &, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = availabilityHint();
    return false;
}

QString MemoryCredentialStore::mapKey(const CredentialKey &key) const
{
    return key.service + QLatin1Char('\n') + key.account;
}

bool MemoryCredentialStore::store(const CredentialKey &key, const QString &secret,
                                  QString *errorMessage)
{
    if (!key.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Clave de credencial inválida.");
        return false;
    }
    m_secrets.insert(mapKey(key), secret);
    return true;
}

bool MemoryCredentialStore::lookup(const CredentialKey &key, QString &secret,
                                   QString *errorMessage) const
{
    const auto it = m_secrets.constFind(mapKey(key));
    if (it == m_secrets.constEnd()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Credencial no encontrada.");
        return false;
    }
    secret = it.value();
    return true;
}

bool MemoryCredentialStore::remove(const CredentialKey &key, QString *errorMessage)
{
    Q_UNUSED(errorMessage)
    m_secrets.remove(mapKey(key));
    return true;
}

} // namespace qterm2
