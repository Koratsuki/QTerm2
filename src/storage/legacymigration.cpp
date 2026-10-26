#include "storage/legacymigration.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace qterm2 {

namespace {

QString legacyJsonPath(const AppPaths &paths)
{
    return paths.configDir + QStringLiteral("/sessions.json");
}

QString legacySettingsPath(const AppPaths &paths)
{
    return paths.configDir + QStringLiteral("/QTerm2.conf");
}

void backup(QString &path)
{
    if (!QFileInfo::exists(path))
        return;
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString target = path + QStringLiteral(".migrated-") + stamp;
    if (!QFile::rename(path, target)) {
        // A read-only home should not prevent the application from starting.
        // Leaving the file in place is safe: the migration flag prevents a
        // second import.
    }
}

} // namespace

Status importLegacyData(const AppPaths &paths, SessionRepository &sessions,
                        SettingsStore &settings)
{
    static const QString importedKey = QStringLiteral("migration/legacyDataImported");
    if (settings.value(importedKey).toBool())
        return Status::success();

    if (QFileInfo::exists(legacySettingsPath(paths))) {
        QSettings legacy(legacySettingsPath(paths), QSettings::IniFormat);
        QString error;
        if (!settings.importLegacySettings(legacy, &error))
            return Status::failure(error);
    }

    QString jsonPath = legacyJsonPath(paths);
    if (QFileInfo::exists(jsonPath)) {
        const int existing = sessions.count();
        if (existing > 0) {
            backup(jsonPath);
        } else {
            QFile file(jsonPath);
            if (!file.open(QIODevice::ReadOnly))
                return Status::failure(QStringLiteral("No se pudo leer %1").arg(jsonPath));
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(file.readAll(), &parseError);
            file.close();
            if (parseError.error != QJsonParseError::NoError
                || !document.isObject()) {
                return Status::failure(
                    QStringLiteral("El archivo heredado %1 no es JSON válido: %2")
                        .arg(jsonPath, parseError.errorString()));
            }
            for (const QJsonValue &value :
                 document.object().value(QStringLiteral("sessions")).toArray()) {
                if (!value.isObject())
                    continue;
                Session session = Session::fromJson(value.toObject());
                if (!session.createdAt.isValid())
                    session.touchCreated();
                QString error;
                if (!sessions.upsert(session, &error)) {
                    return Status::failure(
                        QStringLiteral("No se pudo importar una sesión: %1").arg(error));
                }
            }
            backup(jsonPath);
        }
    }

    QString error;
    if (!settings.setValue(importedKey, true, &error))
        return Status::failure(error);
    return Status::success();
}

} // namespace qterm2
