#pragma once

#include <QString>

namespace qterm2 {

/**
 * Filesystem layout used by QTerm2.
 *
 * The application deliberately separates:
 *   - configuration (human-editable, non-secret): ~/.config/QTerm2
 *   - durable data (SQLite database):              ~/.local/share/QTerm2
 *   - disposable cache:                             ~/.cache/QTerm2
 *
 * Secrets never live in any of these directories.  They belong to the
 * platform Secret Service (see security/credentialstore.h).
 */
struct AppPaths
{
    QString configDir;
    QString dataDir;
    QString cacheDir;
    QString databaseFile;

    static AppPaths system();
    static AppPaths forRoot(const QString &root);

    bool ensure(QString *errorMessage = nullptr) const;
};

} // namespace qterm2
