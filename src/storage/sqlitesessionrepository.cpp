#include "storage/sqlitesessionrepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace qterm2 {

namespace {

QString authToString(Session::Auth auth)
{
    switch (auth) {
    case Session::Auth::PublicKey: return QStringLiteral("publickey");
    case Session::Auth::Agent:     return QStringLiteral("agent");
    case Session::Auth::Password:  return QStringLiteral("password");
    }
    return QStringLiteral("publickey");
}

Session::Auth authFromString(const QString &value)
{
    if (value == QLatin1String("agent"))
        return Session::Auth::Agent;
    if (value == QLatin1String("password"))
        return Session::Auth::Password;
    return Session::Auth::PublicKey;
}

QString policyToString(Session::HostKeyPolicy policy)
{
    switch (policy) {
    case Session::HostKeyPolicy::Ask:       return QStringLiteral("ask");
    case Session::HostKeyPolicy::Strict:    return QStringLiteral("strict");
    case Session::HostKeyPolicy::AcceptNew: return QStringLiteral("accept-new");
    }
    return QStringLiteral("ask");
}

Session::HostKeyPolicy policyFromString(const QString &value)
{
    if (value == QLatin1String("strict"))
        return Session::HostKeyPolicy::Strict;
    if (value == QLatin1String("accept-new"))
        return Session::HostKeyPolicy::AcceptNew;
    return Session::HostKeyPolicy::Ask;
}

QString forwardsToJson(const QVector<PortForward> &forwards)
{
    QJsonArray array;
    for (const PortForward &forward : forwards)
        array.append(forward.toJson());
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<PortForward> forwardsFromJson(const QString &json)
{
    QVector<PortForward> result;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue &value : document.array()) {
        if (value.isObject())
            result.append(PortForward::fromJson(value.toObject()));
    }
    return result;
}

QString profileToJson(const TerminalProfile &profile)
{
    return QString::fromUtf8(
        QJsonDocument(profile.toJson()).toJson(QJsonDocument::Compact));
}

TerminalProfile profileFromJson(const QString &json)
{
    return TerminalProfile::fromJson(
        QJsonDocument::fromJson(json.toUtf8()).object());
}

QString iso(const QDateTime &dateTime)
{
    return dateTime.isValid()
               ? dateTime.toUTC().toString(Qt::ISODateWithMs)
               : QString();
}

QDateTime dateTime(const QVariant &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QString queryError(const QSqlQuery &query)
{
    return query.lastError().text();
}

/**
 * QSqlQuery binds a null QString as SQL NULL.  Every TEXT column in the
 * schema is NOT NULL, so normalise null strings (the default state of a
 * freshly constructed QString) to an empty, non-null string.
 */
QString sqlText(const QString &value)
{
    return value.isNull() ? QString::fromLatin1("") : value;
}

Session sessionFromQuery(const QSqlQuery &query)
{
    Session s;
    int i = 0;
    s.id = query.value(i++).toString();
    s.name = query.value(i++).toString();
    s.group = query.value(i++).toString();
    s.type = query.value(i++) == QLatin1String("local") ? Session::Type::Local
                                                        : Session::Type::Ssh;
    s.host = query.value(i++).toString();
    s.port = query.value(i++).toInt();
    s.user = query.value(i++).toString();
    s.auth = authFromString(query.value(i++).toString());
    s.privateKey = query.value(i++).toString();
    s.proxyJump = query.value(i++).toString();
    s.extraArgs = query.value(i++).toString();
    s.startDir = query.value(i++).toString();
    s.initialCommand = query.value(i++).toString();
    s.shell = query.value(i++).toString();
    s.x11Forwarding = query.value(i++).toBool();
    s.forwardAgent = query.value(i++).toBool();
    s.keepAlive = query.value(i++).toBool();
    s.connectTimeout = query.value(i++).toInt();
    s.hostKeyPolicy = policyFromString(query.value(i++).toString());
    s.portForwards = forwardsFromJson(query.value(i++).toString());
    s.favorite = query.value(i++).toBool();
    s.lastUsedAt = dateTime(query.value(i++));
    s.createdAt = dateTime(query.value(i++));
    s.updatedAt = dateTime(query.value(i++));
    s.colorTag = query.value(i++).toString();
    s.notes = query.value(i++).toString();
    s.autoReconnect = query.value(i++).toBool();
    s.terminalProfile = profileFromJson(query.value(i++).toString());
    return s;
}

QStringList sessionColumns()
{
    return {
        QStringLiteral("id"),           QStringLiteral("name"),
        QStringLiteral("group_name"),   QStringLiteral("type"),
        QStringLiteral("host"),         QStringLiteral("port"),
        QStringLiteral("username"),     QStringLiteral("auth_method"),
        QStringLiteral("private_key"),  QStringLiteral("proxy_jump"),
        QStringLiteral("extra_args"),   QStringLiteral("start_dir"),
        QStringLiteral("initial_command"), QStringLiteral("shell"),
        QStringLiteral("x11_forwarding"), QStringLiteral("forward_agent"),
        QStringLiteral("keep_alive"),   QStringLiteral("connect_timeout"),
        QStringLiteral("host_key_policy"), QStringLiteral("port_forwards"),
        QStringLiteral("favorite"),     QStringLiteral("last_used_at"),
        QStringLiteral("created_at"),   QStringLiteral("updated_at"),
        QStringLiteral("color_tag"),    QStringLiteral("notes"),
        QStringLiteral("auto_reconnect"), QStringLiteral("terminal_profile"),
    };
}

} // namespace

SqliteSessionRepository::SqliteSessionRepository(Database *database)
    : m_database(database)
{
}

bool SqliteSessionRepository::loadAll(QVector<Session> &sessions, QString *errorMessage)
{
    sessions.clear();
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QSqlQuery query(m_database->handle());
    const QString sql =
        QStringLiteral("SELECT %1 FROM sessions "
                       "ORDER BY favorite DESC, group_name COLLATE NOCASE, "
                       "name COLLATE NOCASE")
            .arg(sessionColumns().join(QStringLiteral(", ")));
    if (!query.exec(sql)) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    while (query.next())
        sessions.append(sessionFromQuery(query));
    return true;
}

bool SqliteSessionRepository::ensureGroup(const QString &name, QString *errorMessage)
{
    const QString clean = name.trimmed().isEmpty() ? QStringLiteral("Servidores")
                                                   : name.trimmed();
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO groups(id, name, created_at) VALUES(?, ?, ?)"));
    query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.addBindValue(clean);
    query.addBindValue(iso(QDateTime::currentDateTimeUtc()));
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SqliteSessionRepository::upsert(Session &session, QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    if (session.id.trimmed().isEmpty())
        session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (session.name.trimmed().isEmpty())
        session.name = session.host.trimmed().isEmpty() ? QStringLiteral("Sesión")
                                                        : session.host.trimmed();
    if (session.createdAt.isValid())
        session.updatedAt = QDateTime::currentDateTimeUtc();
    else
        session.touchCreated();

    if (!ensureGroup(session.group, errorMessage))
        return false;

    const QString columns = sessionColumns().join(QStringLiteral(", "));
    QStringList placeholders;
    for (int i = 0; i < sessionColumns().size(); ++i)
        placeholders << QStringLiteral("?");
    const QString sql =
        QStringLiteral("INSERT INTO sessions(%1) VALUES(%2) "
                       "ON CONFLICT(id) DO UPDATE SET "
                       "name=excluded.name, group_name=excluded.group_name, "
                       "type=excluded.type, host=excluded.host, port=excluded.port, "
                       "username=excluded.username, auth_method=excluded.auth_method, "
                       "private_key=excluded.private_key, proxy_jump=excluded.proxy_jump, "
                       "extra_args=excluded.extra_args, start_dir=excluded.start_dir, "
                       "initial_command=excluded.initial_command, shell=excluded.shell, "
                       "x11_forwarding=excluded.x11_forwarding, "
                       "forward_agent=excluded.forward_agent, "
                       "keep_alive=excluded.keep_alive, "
                       "connect_timeout=excluded.connect_timeout, "
                       "host_key_policy=excluded.host_key_policy, "
                       "port_forwards=excluded.port_forwards, favorite=excluded.favorite, "
                       "last_used_at=excluded.last_used_at, updated_at=excluded.updated_at, "
                       "color_tag=excluded.color_tag, notes=excluded.notes, "
                       "auto_reconnect=excluded.auto_reconnect, "
                       "terminal_profile=excluded.terminal_profile")
            .arg(columns, placeholders.join(QStringLiteral(", ")));

    QSqlQuery query(m_database->handle());
    if (!query.prepare(sql)) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    query.addBindValue(sqlText(session.id));
    query.addBindValue(sqlText(session.name));
    query.addBindValue(sqlText(session.group.trimmed().isEmpty() ? QStringLiteral("Servidores")
                                                         : session.group.trimmed()));
    query.addBindValue(session.type == Session::Type::Local ? QStringLiteral("local")
                                                            : QStringLiteral("ssh"));
    query.addBindValue(sqlText(session.host));
    query.addBindValue(session.port);
    query.addBindValue(sqlText(session.user));
    query.addBindValue(authToString(session.auth));
    query.addBindValue(sqlText(session.privateKey));
    query.addBindValue(sqlText(session.proxyJump));
    query.addBindValue(sqlText(session.extraArgs));
    query.addBindValue(sqlText(session.startDir));
    query.addBindValue(sqlText(session.initialCommand));
    query.addBindValue(sqlText(session.shell));
    query.addBindValue(session.x11Forwarding ? 1 : 0);
    query.addBindValue(session.forwardAgent ? 1 : 0);
    query.addBindValue(session.keepAlive ? 1 : 0);
    query.addBindValue(session.connectTimeout);
    query.addBindValue(policyToString(session.hostKeyPolicy));
    query.addBindValue(forwardsToJson(session.portForwards));
    query.addBindValue(session.favorite ? 1 : 0);
    query.addBindValue(sqlText(iso(session.lastUsedAt)));
    query.addBindValue(sqlText(iso(session.createdAt)));
    query.addBindValue(sqlText(iso(session.updatedAt)));
    query.addBindValue(sqlText(session.colorTag));
    query.addBindValue(sqlText(session.notes));
    query.addBindValue(session.autoReconnect ? 1 : 0);
    query.addBindValue(profileToJson(session.terminalProfile));

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SqliteSessionRepository::remove(const QString &id, QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("DELETE FROM sessions WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SqliteSessionRepository::markUsed(const QString &id, const QDateTime &when,
                                       QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("UPDATE sessions SET last_used_at = ? WHERE id = ?"));
    query.addBindValue(iso(when));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

int SqliteSessionRepository::count(QString *errorMessage) const
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return -1;
    }
    QSqlQuery query(m_database->handle());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM sessions")) || !query.next()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace qterm2
