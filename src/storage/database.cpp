#include "storage/database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace qterm2 {

namespace {
constexpr int kSchemaVersion = 4;

QString sqlError(const QSqlQuery &query)
{
    return query.lastError().text();
}
} // namespace

Database::Database(const AppPaths &paths)
    : m_paths(paths)
    , m_path(paths.databaseFile)
{
}

Database::~Database()
{
    if (m_open) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isValid())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::open(QString *errorMessage)
{
    if (m_open)
        return true;
    if (!m_paths.ensure(errorMessage))
        return false;

    m_connectionName =
        QStringLiteral("qterm2-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir la base de datos %1: %2")
                                .arg(m_path, db.lastError().text());
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    m_open = true;
    if (!exec(QStringLiteral("PRAGMA foreign_keys = ON"), errorMessage)
        || !exec(QStringLiteral("PRAGMA journal_mode = WAL"), errorMessage)
        || !exec(QStringLiteral("PRAGMA synchronous = NORMAL"), errorMessage)
        || !exec(QStringLiteral("PRAGMA busy_timeout = 5000"), errorMessage)
        || !migrate(errorMessage)) {
        db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        m_open = false;
        return false;
    }
    return true;
}

bool Database::isOpen() const
{
    return m_open;
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int Database::schemaVersion() const
{
    if (!m_open)
        return -1;
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool Database::exec(const QString &sql, QString *errorMessage)
{
    QSqlQuery query(handle());
    if (query.exec(sql))
        return true;
    if (errorMessage)
        *errorMessage = sqlError(query);
    return false;
}

bool Database::migrate(QString *errorMessage)
{
    QSqlDatabase db = handle();
    if (!db.transaction()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        if (errorMessage)
            *errorMessage = sqlError(query);
        db.rollback();
        return false;
    }
    int version = query.value(0).toInt();

    if (version > kSchemaVersion) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("La base de datos usa el esquema %1, más reciente que "
                               "el soportado por esta versión de QTerm2 (%2).")
                    .arg(version)
                    .arg(kSchemaVersion);
        }
        db.rollback();
        return false;
    }

    // Migrations are applied in order, one block per schema version.  Never
    // modify a block that has already been released: append a new one and
    // bump kSchemaVersion instead.
    if (version < 1) {
        const QStringList statements = {
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS groups ("
                " id TEXT PRIMARY KEY,"
                " name TEXT NOT NULL UNIQUE,"
                " parent_id TEXT REFERENCES groups(id) ON DELETE SET NULL,"
                " sort_order INTEGER NOT NULL DEFAULT 0,"
                " created_at TEXT NOT NULL"
                ")"),
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS sessions ("
                " id TEXT PRIMARY KEY,"
                " name TEXT NOT NULL,"
                " group_name TEXT NOT NULL DEFAULT 'Servidores',"
                " type TEXT NOT NULL DEFAULT 'ssh',"
                " host TEXT NOT NULL DEFAULT '',"
                " port INTEGER NOT NULL DEFAULT 22,"
                " username TEXT NOT NULL DEFAULT '',"
                " auth_method TEXT NOT NULL DEFAULT 'publickey',"
                " private_key TEXT NOT NULL DEFAULT '',"
                " proxy_jump TEXT NOT NULL DEFAULT '',"
                " extra_args TEXT NOT NULL DEFAULT '',"
                " start_dir TEXT NOT NULL DEFAULT '',"
                " initial_command TEXT NOT NULL DEFAULT '',"
                " shell TEXT NOT NULL DEFAULT '',"
                " x11_forwarding INTEGER NOT NULL DEFAULT 0,"
                " forward_agent INTEGER NOT NULL DEFAULT 0,"
                " keep_alive INTEGER NOT NULL DEFAULT 1,"
                " connect_timeout INTEGER NOT NULL DEFAULT 10,"
                " host_key_policy TEXT NOT NULL DEFAULT 'ask',"
                " port_forwards TEXT NOT NULL DEFAULT '[]',"
                " favorite INTEGER NOT NULL DEFAULT 0,"
                " last_used_at TEXT,"
                " created_at TEXT NOT NULL,"
                " updated_at TEXT NOT NULL,"
                " color_tag TEXT NOT NULL DEFAULT 'blue',"
                " notes TEXT NOT NULL DEFAULT ''"
                ")"),
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_sessions_group"
                " ON sessions(group_name)"),
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS settings ("
                " key TEXT PRIMARY KEY,"
                " value BLOB NOT NULL,"
                " updated_at TEXT NOT NULL"
                ")"),
        };
        for (const QString &statement : statements) {
            if (!query.exec(statement)) {
                if (errorMessage)
                    *errorMessage = sqlError(query);
                db.rollback();
                return false;
            }
        }
        version = 1;
    }

    if (version < 2) {
        if (!query.exec(QStringLiteral(
                "ALTER TABLE sessions ADD COLUMN auto_reconnect INTEGER NOT NULL DEFAULT 0"))) {
            if (errorMessage)
                *errorMessage = sqlError(query);
            db.rollback();
            return false;
        }
        version = 2;
    }

    if (version < 3) {
        if (!query.exec(QStringLiteral(
                "ALTER TABLE sessions ADD COLUMN terminal_profile TEXT NOT NULL "
                "DEFAULT '{}'"))) {
            if (errorMessage)
                *errorMessage = sqlError(query);
            db.rollback();
            return false;
        }
        version = 3;
    }

    if (version < 4) {
        if (!query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS snippets ("
                " id TEXT PRIMARY KEY,"
                " name TEXT NOT NULL,"
                " command TEXT NOT NULL,"
                " description TEXT NOT NULL DEFAULT '',"
                " created_at TEXT NOT NULL,"
                " updated_at TEXT NOT NULL"
                ")"))) {
            if (errorMessage)
                *errorMessage = sqlError(query);
            db.rollback();
            return false;
        }
        version = 4;
    }

    if (version != kSchemaVersion) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo migrar el esquema de la base de "
                                           "datos: versión %1, esperada %2.")
                                .arg(version)
                                .arg(kSchemaVersion);
        }
        db.rollback();
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
        if (errorMessage)
            *errorMessage = sqlError(query);
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }
    return true;
}

} // namespace qterm2
