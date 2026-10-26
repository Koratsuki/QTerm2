#include "ssh/knownhosts.h"

#include "core/pathutils.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace qterm2::ssh {

namespace {

QString backupPathFor(const QString &path)
{
    const QString stamp = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString candidate = path + QStringLiteral(".qterm2.bak-") + stamp;
    int suffix = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = path + QStringLiteral(".qterm2.bak-") + stamp
                    + QLatin1Char('-') + QString::number(suffix++);
    }
    return candidate;
}

} // namespace

KnownHostsDocument parseKnownHosts(const QString &path, QString *errorMessage)
{
    KnownHostsDocument document;
    const QString expanded = expandHome(path);
    if (!QFileInfo::exists(expanded)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No existe el archivo %1").arg(expanded);
        return document;
    }

    QFile file(expanded);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo leer %1").arg(expanded);
        return document;
    }

    int lineNumber = 0;
    while (!file.atEnd()) {
        ++lineNumber;
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                              Qt::SkipEmptyParts);
        if (fields.size() < 3) {
            document.warnings.append(QStringLiteral("Línea %1: formato desconocido")
                                         .arg(lineNumber));
            continue;
        }

        KnownHostEntry entry;
        entry.hosts = fields.at(0);
        entry.keyType = fields.at(1);
        entry.key = fields.at(2);
        entry.comment = fields.mid(3).join(QLatin1Char(' '));
        entry.line = lineNumber;
        entry.hashed = entry.hosts.startsWith(QLatin1String("|1|"));
        document.entries.append(entry);
    }
    return document;
}

bool removeKnownHostLines(const QString &path, const QSet<int> &lineNumbers,
                          QString *errorMessage)
{
    const QString expanded = expandHome(path);
    QFile file(expanded);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo leer %1").arg(expanded);
        return false;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    file.close();

    const QString backup = backupPathFor(expanded);
    if (!QFile::copy(expanded, backup)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo crear la copia %1").arg(backup);
        return false;
    }

    QStringList kept;
    kept.reserve(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        if (!lineNumbers.contains(i + 1))
            kept.append(lines.at(i));
    }

    QSaveFile output(expanded);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo escribir %1").arg(expanded);
        return false;
    }
    output.write(kept.join(QLatin1Char('\n')).toUtf8());
    if (!output.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo guardar %1").arg(expanded);
        return false;
    }

    QFile::setPermissions(expanded, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

} // namespace qterm2::ssh
