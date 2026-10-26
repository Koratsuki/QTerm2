#include "storage/syncmanager.h"
#include "storage/sessionrepository.h"
#include "security/crypto.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace qterm2 {

SyncManager::SyncManager(SessionRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

bool SyncManager::exportEncrypted(const QString &filePath, const QString &password, QString *errorMessage)
{
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
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write file");
        return false;
    }
    
    file.write(encrypted);
    file.close();
    
    emit syncCompleted(true, QStringLiteral("Export completed"));
    return true;
}

bool SyncManager::importEncrypted(const QString &filePath, const QString &password, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read file");
        return false;
    }
    
    QByteArray encrypted = file.readAll();
    file.close();
    
    QByteArray json;
    if (!Crypto::decrypt(encrypted, password, json, errorMessage))
        return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid format");
        return false;
    }
    
    QJsonArray array = doc.object()[QStringLiteral("sessions")].toArray();
    for (const auto &val : array) {
        if (val.isObject()) {
            Session s = Session::fromJson(val.toObject());
            m_repo->upsert(s, errorMessage);
        }
    }
    
    emit syncCompleted(true, QStringLiteral("Import completed"));
    return true;
}

bool SyncManager::isEncrypted(const QString &filePath) const
{
    return filePath.endsWith(QStringLiteral(".qterm2enc"));
}

} // namespace qterm2
