#pragma once

#include "storage/database.h"

#include <QSettings>
#include <QString>
#include <QVariant>

namespace qterm2 {

/**
 * Typed, non-secret application settings stored in SQLite.
 *
 * Values are serialized as QVariant blobs to keep the API small.  Keeping the
 * old QSettings file out of the hot path lets QTerm2 use one transactional
 * store for configuration and server metadata.
 */
class SettingsStore
{
public:
    explicit SettingsStore(Database *database);

    bool initialize(QString *errorMessage = nullptr);
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    bool contains(const QString &key) const;
    bool setValue(const QString &key, const QVariant &value,
                  QString *errorMessage = nullptr);
    bool remove(const QString &key, QString *errorMessage = nullptr);

    /** One-time import from the pre-SQLite QSettings file. */
    bool importLegacySettings(QSettings &legacy, QString *errorMessage = nullptr);

private:
    Database *m_database = nullptr;
};

} // namespace qterm2
