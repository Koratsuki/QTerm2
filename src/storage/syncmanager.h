#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include "domain/session.h"

namespace qterm2 {

class SessionRepository;
class Crypto;

class SyncManager : public QObject
{
    Q_OBJECT
public:
    explicit SyncManager(SessionRepository *repo, QObject *parent = nullptr);
    
    bool exportEncrypted(const QString &filePath, const QString &password, QString *errorMessage = nullptr);
    bool importEncrypted(const QString &filePath, const QString &password, QString *errorMessage = nullptr);
    bool isEncrypted(const QString &filePath) const;

signals:
    void syncCompleted(bool success, const QString &message);

private:
    SessionRepository *m_repo = nullptr;
};

} // namespace qterm2
