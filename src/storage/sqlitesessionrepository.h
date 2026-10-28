#pragma once

#include "storage/database.h"
#include "storage/sessionrepository.h"

namespace qterm2 {

class SqliteSessionRepository final : public SessionRepository
{
public:
    explicit SqliteSessionRepository(Database *database);

    bool loadAll(QVector<Session> &sessions, QString *errorMessage = nullptr) override;
    bool upsert(Session &session, QString *errorMessage = nullptr) override;
    bool remove(const QString &id, QString *errorMessage = nullptr) override;
    bool markUsed(const QString &id, const QDateTime &when,
                  QString *errorMessage = nullptr) override;
    bool clearRecentSessions(QString *errorMessage = nullptr) override;
    int count(QString *errorMessage = nullptr) const override;

private:
    bool ensureGroup(const QString &name, QString *errorMessage);
    Database *m_database = nullptr;
};

} // namespace qterm2
