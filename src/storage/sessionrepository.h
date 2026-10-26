#pragma once

#include "domain/session.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace qterm2 {

/**
 * Storage-neutral session catalog.
 *
 * UI and session orchestration depend on this interface, not on SQLite.  That
 * keeps the SQL schema replaceable and lets tests use an in-memory
 * implementation without touching the user's real database.
 */
class SessionRepository
{
public:
    virtual ~SessionRepository() = default;

    virtual bool loadAll(QVector<Session> &sessions, QString *errorMessage = nullptr) = 0;
    virtual bool upsert(Session &session, QString *errorMessage = nullptr) = 0;
    virtual bool remove(const QString &id, QString *errorMessage = nullptr) = 0;
    virtual bool markUsed(const QString &id, const QDateTime &when,
                          QString *errorMessage = nullptr) = 0;
    virtual int count(QString *errorMessage = nullptr) const = 0;
};

} // namespace qterm2
