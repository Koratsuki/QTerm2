#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "domain/session.h"
#include "storage/sessionrepository.h"

namespace qterm2 {

/**
 * In-memory session catalog used by the UI.
 *
 * Persistence is delegated to SessionRepository.  The store never touches
 * SQLite directly and never serializes sessions to JSON at runtime.
 */
class SessionStore : public QObject
{
    Q_OBJECT
public:
    explicit SessionStore(SessionRepository *repository, QObject *parent = nullptr);

    const QVector<Session> &sessions() const { return m_sessions; }
    bool load();
    QString lastError() const { return m_lastError; }

    int indexOf(const QString &id) const;
    Session sessionById(const QString &id) const;
    bool contains(const QString &id) const { return indexOf(id) >= 0; }

    bool addSession(Session session);
    bool updateSession(Session session);
    bool removeSession(const QString &id);
    bool markUsed(const QString &id, const QDateTime &when = QDateTime::currentDateTimeUtc());

    QStringList groups() const;
    QString suggestGroup() const;

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    void setError(const QString &message);

    SessionRepository *m_repository = nullptr;
    QVector<Session> m_sessions;
    QString m_lastError;
};

} // namespace qterm2
