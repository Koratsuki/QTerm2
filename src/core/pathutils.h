#pragma once

#include <QString>

namespace qterm2 {

/** Expands a leading "~" or "~/" using the current user's home directory. */
QString expandHome(const QString &path);

} // namespace qterm2
