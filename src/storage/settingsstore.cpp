#include "storage/settingsstore.h"

#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <QSqlError>
#include <QSqlQuery>

namespace qterm2 {

namespace {

QByteArray serialize(const QVariant &value)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << value;
    return data;
}

QVariant deserialize(const QByteArray &data)
{
    QVariant value;
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_0);
    stream >> value;
    return value;
}

QString queryError(const QSqlQuery &query)
{
    return query.lastError().text();
}

} // namespace

SettingsStore::SettingsStore(Database *database)
    : m_database(database)
{
}

bool SettingsStore::initialize(QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    return true;
}

QVariant SettingsStore::value(const QString &key, const QVariant &defaultValue) const
{
    if (!m_database || !m_database->isOpen())
        return defaultValue;
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next())
        return defaultValue;
    const QVariant stored = deserialize(query.value(0).toByteArray());
    return stored.isValid() ? stored : defaultValue;
}

bool SettingsStore::contains(const QString &key) const
{
    if (!m_database || !m_database->isOpen())
        return false;
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("SELECT 1 FROM settings WHERE key = ?"));
    query.addBindValue(key);
    return query.exec() && query.next();
}

bool SettingsStore::setValue(const QString &key, const QVariant &value,
                             QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value, updated_at) VALUES(?, ?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value, "
        "updated_at=excluded.updated_at"));
    query.addBindValue(key);
    query.addBindValue(serialize(value));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SettingsStore::remove(const QString &key, QString *errorMessage)
{
    if (!m_database || !m_database->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = queryError(query);
        return false;
    }
    return true;
}

bool SettingsStore::importLegacySettings(QSettings &legacy, QString *errorMessage)
{
    static const QString importedKey =
        QStringLiteral("migration/legacySettingsImported");
    if (value(importedKey).toBool())
        return true;

    static const QStringList keys = {
        QStringLiteral("terminal/fontFamily"),
        QStringLiteral("terminal/fontSize"),
        QStringLiteral("terminal/scrollback"),
        QStringLiteral("terminal/cursorBlink"),
        QStringLiteral("terminal/shell"),
        QStringLiteral("ui/sidebarVisible"),
        QStringLiteral("ui/sidebarWidth"),
        QStringLiteral("ui/darkTheme"),
        QStringLiteral("ui/confirmClose"),
        QStringLiteral("ui/geometry"),
        QStringLiteral("startup/openLocalTab"),
    };

    for (const QString &key : keys) {
        if (contains(key) || !legacy.contains(key))
            continue;
        if (!setValue(key, legacy.value(key), errorMessage))
            return false;
    }
    return setValue(importedKey, true, errorMessage);
}

} // namespace qterm2
