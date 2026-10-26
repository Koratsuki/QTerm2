#include "sessions/sessionstore.h"

namespace qterm2 {

SessionStore::SessionStore(SessionRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

bool SessionStore::load()
{
    m_sessions.clear();
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de sesiones configurado."));
        return false;
    }
    QString error;
    if (!m_repository->loadAll(m_sessions, &error)) {
        setError(error);
        emit changed();
        return false;
    }
    m_lastError.clear();
    emit changed();
    return true;
}

void SessionStore::setError(const QString &message)
{
    m_lastError = message;
    emit errorOccurred(message);
}

int SessionStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).id == id)
            return i;
    }
    return -1;
}

Session SessionStore::sessionById(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 ? m_sessions.at(index) : Session();
}

bool SessionStore::addSession(Session session)
{
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de sesiones configurado."));
        return false;
    }
    QString error;
    if (!m_repository->upsert(session, &error)) {
        setError(error);
        return false;
    }
    const int existing = indexOf(session.id);
    if (existing >= 0)
        m_sessions[existing] = session;
    else
        m_sessions.append(session);
    m_lastError.clear();
    emit changed();
    return true;
}

bool SessionStore::updateSession(Session session)
{
    if (!contains(session.id))
        return addSession(session);
    return addSession(session);
}

bool SessionStore::removeSession(const QString &id)
{
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de sesiones configurado."));
        return false;
    }
    QString error;
    if (!m_repository->remove(id, &error)) {
        setError(error);
        return false;
    }
    const int index = indexOf(id);
    if (index >= 0)
        m_sessions.removeAt(index);
    m_lastError.clear();
    emit changed();
    return true;
}

bool SessionStore::markUsed(const QString &id, const QDateTime &when)
{
    if (!m_repository)
        return false;
    QString error;
    if (!m_repository->markUsed(id, when, &error)) {
        setError(error);
        return false;
    }
    const int index = indexOf(id);
    if (index >= 0)
        m_sessions[index].lastUsedAt = when;
    emit changed();
    return true;
}

QStringList SessionStore::groups() const
{
    QStringList result;
    for (const Session &session : m_sessions) {
        const QString group = session.group.trimmed();
        if (!group.isEmpty() && !result.contains(group))
            result.append(group);
    }
    return result;
}

QString SessionStore::suggestGroup() const
{
    const QStringList available = groups();
    if (available.contains(QStringLiteral("Servidores")))
        return QStringLiteral("Servidores");
    for (const QString &name : available) {
        if (name != QLatin1String("Locales"))
            return name;
    }
    return QStringLiteral("Servidores");
}

} // namespace qterm2
