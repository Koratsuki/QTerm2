#pragma once

#include "core/apppaths.h"

#include <QSqlDatabase>
#include <QString>

namespace qterm2 {

/**
 * Owns the single SQLite connection used by the application.
 *
 * SQLite stores metadata and non-secret configuration only.  Credentials are
 * handled by security/credentialstore.h and never reach this class.
 */
class Database
{
public:
    explicit Database(const AppPaths &paths);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(QString *errorMessage = nullptr);
    bool isOpen() const;

    QSqlDatabase handle() const;
    QString filePath() const { return m_path; }

    /** Current schema version, or -1 when the database is not open. */
    int schemaVersion() const;

private:
    bool migrate(QString *errorMessage);
    bool exec(const QString &sql, QString *errorMessage);

    AppPaths m_paths;
    QString m_path;
    QString m_connectionName;
    bool m_open = false;
};

} // namespace qterm2
