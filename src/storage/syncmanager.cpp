#include "storage/syncmanager.h"
#include "storage/sessionrepository.h"
#include "security/crypto.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace qterm2 {

namespace {

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

// Encrypted exports are private on disk: the database they mirror is 0600 too.
constexpr QFileDevice::Permissions kOwnerOnly =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner;

} // namespace

SyncManager::SyncManager(SessionRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

bool SyncManager::exportEncrypted(const QString &filePath, const QString &password, QString *errorMessage)
{
    if (!m_repo) {
        setError(errorMessage, QStringLiteral("No hay repositorio de sesiones."));
        return false;
    }
    if (filePath.isEmpty()) {
        setError(errorMessage, QStringLiteral("Falta la ruta del archivo de exportación."));
        return false;
    }

    QVector<Session> sessions;
    if (!m_repo->loadAll(sessions, errorMessage))
        return false;

    QJsonArray array;
    for (const Session &s : sessions) {
        array.append(s.toJson());
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("sessions")] = array;
    root[QStringLiteral("encrypted")] = true;

    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QByteArray encrypted;
    if (!Crypto::encrypt(json, password, encrypted, errorMessage))
        return false;

    // QSaveFile writes to a temporary file and renames it into place, so a
    // crash or a full disk never leaves a truncated export behind.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage,
                 QStringLiteral("No se pudo escribir %1: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }
    file.setPermissions(kOwnerOnly);
    if (file.write(encrypted) != encrypted.size()) {
        const QString reason = file.errorString();
        file.cancelWriting();
        setError(errorMessage,
                 QStringLiteral("No se pudo escribir %1: %2").arg(filePath, reason));
        return false;
    }
    if (!file.commit()) {
        setError(errorMessage,
                 QStringLiteral("No se pudo guardar %1: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    emit syncCompleted(true, QStringLiteral("Export completed"));
    return true;
}

bool SyncManager::importEncrypted(const QString &filePath, const QString &password, QString *errorMessage)
{
    if (!m_repo) {
        setError(errorMessage, QStringLiteral("No hay repositorio de sesiones."));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("No se pudo leer %1: %2")
                     .arg(filePath, file.errorString()));
        return false;
    }

    const QByteArray encrypted = file.readAll();
    file.close();

    QByteArray json;
    if (!Crypto::decrypt(encrypted, password, json, errorMessage))
        return false;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorMessage,
                 QStringLiteral("El archivo descifrado no contiene JSON válido."));
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt(0) != 1) {
        setError(errorMessage,
                 QStringLiteral("El archivo usa una versión de exportación no soportada."));
        return false;
    }
    if (!root.value(QStringLiteral("sessions")).isArray()) {
        setError(errorMessage,
                 QStringLiteral("El archivo no contiene una lista de sesiones."));
        return false;
    }

    const QJsonArray array = root.value(QStringLiteral("sessions")).toArray();

    // Parse and sanitise everything *before* writing: a malformed entry must
    // not leave a partially imported catalogue behind.
    QVector<Session> sessions;
    sessions.reserve(array.size());
    for (const auto &val : array) {
        if (!val.isObject()) {
            setError(errorMessage,
                     QStringLiteral("El archivo contiene una sesión con formato inválido."));
            return false;
        }
        sessions.append(Session::fromJson(val.toObject()));
    }

    if (!m_repo->beginTransaction(errorMessage))
        return false;
    for (Session &s : sessions) {
        if (!m_repo->upsert(s, errorMessage)) {
            m_repo->rollbackTransaction();
            return false;
        }
    }
    if (!m_repo->commitTransaction(errorMessage))
        return false;

    emit syncCompleted(true, QStringLiteral("Import completed"));
    return true;
}

bool SyncManager::isEncrypted(const QString &filePath) const
{
    // Sniff the real header instead of trusting the extension: any file can be
    // renamed, and only the magic proves the payload is one of our exports.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray magic = file.read(4);
    return magic == QByteArrayLiteral("Q2SE");
}

} // namespace qterm2
