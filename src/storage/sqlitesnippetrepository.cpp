#include "storage/sqlitesnippetrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace qterm2 {

namespace {

QString queryError(const QSqlQuery &query)
{
    return query.lastError().text();
}

QString iso(const QDateTime &dateTime)
{
    return dateTime.isValid()
               ? dateTime.toUTC().toString(Qt::ISODateWithMs)
               : QString();
}

QDateTime fromIso(const QVariant &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QString sqlText(const QString &value)
{
    return value.isNull() ? QString::fromLatin1("") : value;
}

} // namespace

SqliteSnippetRepository::SqliteSnippetRepository(Database *database)
    : m_database(database)
{
}

bool SqliteSnippetRepository::loadAll(QVector<Snippet> &snippets,
                                      QString *errorMessage)
{
    snippets.clear();
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QSqlQuery query(m_database->handle());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, command, description, created_at, updated_at"
            " FROM snippets ORDER BY name COLLATE NOCASE"))) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }

    while (query.next()) {
        Snippet snippet;
        snippet.id = query.value(0).toString();
        snippet.name = query.value(1).toString();
        snippet.command = query.value(2).toString();
        snippet.description = query.value(3).toString();
        snippet.createdAt = fromIso(query.value(4));
        snippet.updatedAt = fromIso(query.value(5));
        snippets.append(snippet);
    }
    return true;
}

bool SqliteSnippetRepository::upsert(Snippet &snippet, QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (snippet.id.isEmpty())
        snippet.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!snippet.createdAt.isValid())
        snippet.createdAt = now;
    snippet.updatedAt = now;

    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO snippets (id, name, command, description, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(id) DO UPDATE SET"
        " name=excluded.name, command=excluded.command,"
        " description=excluded.description, updated_at=excluded.updated_at"));
    query.addBindValue(snippet.id);
    query.addBindValue(sqlText(snippet.name));
    query.addBindValue(sqlText(snippet.command));
    query.addBindValue(sqlText(snippet.description));
    query.addBindValue(sqlText(iso(snippet.createdAt)));
    query.addBindValue(sqlText(iso(snippet.updatedAt)));

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SqliteSnippetRepository::remove(const QString &id, QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("DELETE FROM snippets WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

} // namespace qterm2
