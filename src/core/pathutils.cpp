#include "core/pathutils.h"

#include <QDir>

namespace qterm2 {

QString expandHome(const QString &path)
{
    if (path == QLatin1String("~"))
        return QDir::homePath();
    if (path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

} // namespace qterm2
