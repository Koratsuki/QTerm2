#pragma once

#include "core/apppaths.h"
#include "core/status.h"
#include "storage/sessionrepository.h"
#include "storage/settingsstore.h"

namespace qterm2 {

/**
 * One-time import of the pre-SQLite JSON/QSettings state.
 *
 * The source files are renamed after a successful migration so a database
 * reset cannot silently duplicate servers.
 */
Status importLegacyData(const AppPaths &paths, SessionRepository &sessions,
                        SettingsStore &settings);

} // namespace qterm2
