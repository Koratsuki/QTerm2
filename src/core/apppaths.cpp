#include "core/apppaths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace qterm2 {

AppPaths AppPaths::system()
{
    AppPaths paths;
    const QString configBase =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString dataBase =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString cacheBase =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);

    paths.configDir = configBase + QStringLiteral("/QTerm2");
    paths.dataDir = dataBase + QStringLiteral("/QTerm2");
    paths.cacheDir = cacheBase + QStringLiteral("/QTerm2");
    paths.databaseFile = paths.dataDir + QStringLiteral("/qterm2.db");
    return paths;
}

AppPaths AppPaths::forRoot(const QString &root)
{
    const QString clean = QDir::cleanPath(root);
    AppPaths paths;
    paths.configDir = clean + QStringLiteral("/config");
    paths.dataDir = clean + QStringLiteral("/data");
    paths.cacheDir = clean + QStringLiteral("/cache");
    paths.databaseFile = paths.dataDir + QStringLiteral("/qterm2.db");
    return paths;
}

bool AppPaths::ensure(QString *errorMessage) const
{
    for (const QString &dir : { configDir, dataDir, cacheDir }) {
        if (!QDir().mkpath(dir)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("No se pudo crear el directorio %1").arg(dir);
            return false;
        }
        // The database holds hosts, usernames and private-key paths.  Keep the
        // whole tree owner-only instead of inheriting a permissive umask.
        QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                       | QFileDevice::ExeOwner);
    }
    return true;
}

} // namespace qterm2
