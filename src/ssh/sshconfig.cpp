#include "ssh/sshconfig.h"

#include "core/pathutils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace qterm2::ssh {

namespace {

constexpr int kMaxIncludeDepth = 8;

QString stripComment(const QString &line)
{
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"'))
            quoted = !quoted;
        else if (ch == QLatin1Char('#') && !quoted)
            return line.left(i);
    }
    return line;
}

QStringList splitTokens(const QString &line)
{
    QStringList tokens;
    QString current;
    bool quoted = false;
    bool hasToken = false;
    for (const QChar ch : line) {
        if (ch == QLatin1Char('"')) {
            quoted = !quoted;
            hasToken = true;
            continue;
        }
        if (!quoted && ch.isSpace()) {
            if (hasToken) {
                tokens.append(current);
                current.clear();
                hasToken = false;
            }
            continue;
        }
        current.append(ch);
        hasToken = true;
    }
    if (hasToken)
        tokens.append(current);
    return tokens;
}

bool hasWildcard(const QString &pattern)
{
    return pattern.contains(QLatin1Char('*')) || pattern.contains(QLatin1Char('?'))
           || pattern.startsWith(QLatin1Char('!'));
}

QString expandConfigPath(const QString &value, const QString &baseDir)
{
    QString path = expandHome(value.trimmed());
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        return path;
    if (!QDir::isAbsolutePath(path))
        path = baseDir + QLatin1Char('/') + path;
    return QDir::cleanPath(path);
}

void appendInclude(const QString &value, const QString &baseDir,
                   QStringList &paths)
{
    const QString expanded = expandConfigPath(value, baseDir);
    const QString dirPath = QFileInfo(expanded).path();
    const QString pattern = QFileInfo(expanded).fileName();
    const QDir dir(dirPath);
    const QStringList matches =
        dir.entryList({ pattern }, QDir::Files | QDir::Readable, QDir::Name);
    for (const QString &name : matches)
        paths.append(dir.filePath(name));
}

struct ParseState
{
    SshConfigEntry current;
    bool hasHost = false;
};

void applyOption(SshConfigEntry &entry, const QString &key, const QString &value)
{
    if (key == QLatin1String("hostname")) {
        entry.hostName = value;
    } else if (key == QLatin1String("user")) {
        entry.user = value;
    } else if (key == QLatin1String("port")) {
        bool ok = false;
        const int port = value.toInt(&ok);
        if (ok && port > 0 && port <= 65535)
            entry.port = port;
    } else if (key == QLatin1String("identityfile")) {
        entry.identityFile = expandHome(value);
    } else if (key == QLatin1String("proxyjump")) {
        entry.proxyJump = (value.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
                              ? QString()
                              : value;
    }
}

void finishEntry(ParseState &state, QVector<SshConfigEntry> &entries,
                 const QString &path)
{
    if (!state.hasHost)
        return;

    SshConfigEntry entry = state.current;
    entry.sourcePath = path;
    entry.wildcard = false;
    entry.alias.clear();

    for (const QString &pattern : entry.patterns) {
        if (hasWildcard(pattern)) {
            entry.wildcard = true;
            continue;
        }
        if (entry.alias.isEmpty())
            entry.alias = pattern;
    }

    if (!entry.wildcard && !entry.alias.isEmpty()) {
        if (entry.hostName.contains(QLatin1String("%h")))
            entry.hostName.replace(QLatin1String("%h"), entry.alias);
        entries.append(entry);
    }
}

void parseFile(const QString &path, ParseState &state,
               QVector<SshConfigEntry> &entries, QStringList &warnings,
               int depth, QSet<QString> &visited);

void parseInclude(const QString &value, const QString &baseDir,
                  QVector<SshConfigEntry> &entries, QStringList &warnings,
                  int depth, QSet<QString> &visited)
{
    QStringList includePaths;
    appendInclude(value, baseDir, includePaths);
    if (includePaths.isEmpty()) {
        warnings.append(QStringLiteral("%1: Include sin coincidencias: %2")
                            .arg(baseDir, value));
        return;
    }
    for (const QString &include : includePaths) {
        // Included files get their own Host scope; this keeps a Host block in
        // the parent file from being closed or duplicated by the include.
        ParseState includeState;
        parseFile(include, includeState, entries, warnings, depth + 1, visited);
    }
}

void parseFile(const QString &path, ParseState &state,
               QVector<SshConfigEntry> &entries, QStringList &warnings,
               int depth, QSet<QString> &visited)
{
    const QString clean = QDir::cleanPath(path);
    if (depth > kMaxIncludeDepth) {
        warnings.append(QStringLiteral("%1: Include demasiado profundo").arg(clean));
        return;
    }
    if (visited.contains(clean)) {
        warnings.append(QStringLiteral("%1: Include circular").arg(clean));
        return;
    }
    visited.insert(clean);

    QFile file(clean);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        warnings.append(QStringLiteral("No se pudo leer %1").arg(clean));
        return;
    }

    const QString baseDir = QFileInfo(clean).absolutePath();
    int lineNumber = 0;
    while (!file.atEnd()) {
        ++lineNumber;
        const QString raw = QString::fromUtf8(file.readLine());
        const QStringList tokens = splitTokens(stripComment(raw));
        if (tokens.isEmpty())
            continue;

        const QString key = tokens.first().toLower();
        const QString value = tokens.mid(1).join(QLatin1Char(' '));

        if (key == QLatin1String("host")) {
            finishEntry(state, entries, clean);
            state.current = SshConfigEntry();
            state.current.line = lineNumber;
            state.current.patterns = tokens.mid(1);
            state.hasHost = !state.current.patterns.isEmpty();
            continue;
        }

        if (key == QLatin1String("match")) {
            finishEntry(state, entries, clean);
            state.current = SshConfigEntry();
            state.hasHost = false;
            continue;
        }

        if (key == QLatin1String("include")) {
            parseInclude(value, baseDir, entries, warnings, depth, visited);
            continue;
        }

        if (state.hasHost)
            applyOption(state.current, key, value);
    }

    finishEntry(state, entries, clean);
    visited.remove(clean);
}

QString configValue(const QString &value)
{
    if (value.contains(QLatin1Char(' ')) || value.contains(QLatin1Char('\t')))
        return QLatin1Char('"') + value + QLatin1Char('"');
    return value;
}

QString aliasCandidate(const Session &session)
{
    QString alias = session.name.trimmed();
    if (alias.isEmpty())
        alias = session.host;
    alias.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
    alias.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")));
    if (alias.isEmpty())
        alias = QStringLiteral("qterm2-session");
    return alias;
}

QString uniqueAlias(const QString &base, QSet<QString> &used)
{
    QString alias = base;
    int suffix = 2;
    while (used.contains(alias.toLower()))
        alias = base + QLatin1Char('-') + QString::number(suffix++);
    used.insert(alias.toLower());
    return alias;
}

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

QString forwardFlag(PortForward::Type type)
{
    switch (type) {
    case PortForward::Type::Remote:  return QStringLiteral("RemoteForward");
    case PortForward::Type::Dynamic: return QStringLiteral("DynamicForward");
    case PortForward::Type::Local:   break;
    }
    return QStringLiteral("LocalForward");
}

} // namespace

QString managedBlockBegin()
{
    return QStringLiteral("# >>> QTerm2 managed block >>>");
}

QString managedBlockEnd()
{
    return QStringLiteral("# <<< QTerm2 managed block <<<");
}

SshConfigDocument parseSshConfig(const QString &path, QString *errorMessage)
{
    SshConfigDocument document;
    const QString expanded = expandHome(path);
    if (!QFileInfo::exists(expanded)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No existe el archivo %1").arg(expanded);
        return document;
    }

    ParseState state;
    QSet<QString> visited;
    parseFile(expanded, state, document.entries, document.warnings, 0, visited);

    std::sort(document.entries.begin(), document.entries.end(),
              [](const SshConfigEntry &a, const SshConfigEntry &b) {
                  return a.alias.localeAwareCompare(b.alias) < 0;
              });
    return document;
}

QVector<Session> sessionsFromSshConfig(const SshConfigDocument &document,
                                       const QString &group)
{
    QVector<Session> sessions;
    for (const SshConfigEntry &entry : document.entries) {
        Session session;
        session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session.name = entry.alias;
        session.group = group;
        session.type = Session::Type::Ssh;
        session.host = entry.effectiveHost();
        session.port = entry.port > 0 ? entry.port : 22;
        session.user = entry.user;
        session.privateKey = entry.identityFile;
        session.auth = entry.identityFile.isEmpty() ? Session::Auth::Agent
                                                     : Session::Auth::PublicKey;
        session.proxyJump = entry.proxyJump;
        session.keepAlive = true;
        session.notes = QStringLiteral("Importado desde %1:%2")
                            .arg(entry.sourcePath)
                            .arg(entry.line);
        session.touchCreated();
        sessions.append(session);
    }
    return sessions;
}

QString renderSshConfig(const QVector<Session> &sessions)
{
    QStringList lines;
    QSet<QString> used;
    int rendered = 0;

    for (const Session &session : sessions) {
        if (session.isLocal() || session.host.trimmed().isEmpty())
            continue;

        const QString alias = uniqueAlias(aliasCandidate(session), used);
        if (rendered > 0)
            lines.append(QString());
        ++rendered;

        lines.append(QStringLiteral("Host %1").arg(alias));
        lines.append(QStringLiteral("    HostName %1")
                         .arg(configValue(session.host.trimmed())));
        if (!session.user.trimmed().isEmpty())
            lines.append(QStringLiteral("    User %1")
                             .arg(configValue(session.user.trimmed())));
        if (session.port > 0 && session.port != 22)
            lines.append(QStringLiteral("    Port %1").arg(session.port));
        if (!session.privateKey.trimmed().isEmpty())
            lines.append(QStringLiteral("    IdentityFile %1")
                             .arg(configValue(session.privateKey.trimmed())));
        if (!session.proxyJump.trimmed().isEmpty())
            lines.append(QStringLiteral("    ProxyJump %1")
                             .arg(configValue(session.proxyJump.trimmed())));
        if (session.keepAlive) {
            lines.append(QStringLiteral("    ServerAliveInterval 30"));
            lines.append(QStringLiteral("    ServerAliveCountMax 3"));
        }
        for (const PortForward &forward : session.portForwards) {
            if (!forward.enabled || forward.listenPort <= 0)
                continue;
            const QString listen = forward.listenHost.trimmed().isEmpty()
                                       ? QStringLiteral("127.0.0.1")
                                       : forward.listenHost.trimmed();
            if (forward.type == PortForward::Type::Dynamic) {
                lines.append(QStringLiteral("    DynamicForward %1:%2")
                                 .arg(listen)
                                 .arg(forward.listenPort));
            } else if (forward.targetPort > 0
                       && !forward.targetHost.trimmed().isEmpty()) {
                lines.append(QStringLiteral("    %1 %2:%3:%4:%5")
                                 .arg(forwardFlag(forward.type), listen,
                                      QString::number(forward.listenPort),
                                      forward.targetHost.trimmed(),
                                      QString::number(forward.targetPort)));
            }
        }
    }

    if (rendered == 0)
        return QString();
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

bool writeManagedSshConfig(const QString &path, const QString &block,
                           QString *errorMessage)
{
    const QString expanded = expandHome(path);
    const QFileInfo info(expanded);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo crear %1").arg(dir.absolutePath());
        return false;
    }

    QString content;
    if (info.exists()) {
        QFile existing(expanded);
        if (!existing.open(QIODevice::ReadOnly)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("No se pudo leer %1").arg(expanded);
            return false;
        }
        content = QString::fromUtf8(existing.readAll());
        existing.close();

        const QString backup = backupPathFor(expanded);
        if (!QFile::copy(expanded, backup)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("No se pudo crear la copia %1").arg(backup);
            return false;
        }
    }

    QStringList keptLines;
    const QStringList existingLines = content.split(QLatin1Char('\n'));
    bool inside = false;
    for (const QString &line : existingLines) {
        if (line.trimmed() == managedBlockBegin()) {
            inside = true;
            continue;
        }
        if (line.trimmed() == managedBlockEnd()) {
            inside = false;
            continue;
        }
        if (!inside)
            keptLines.append(line);
    }
    while (!keptLines.isEmpty() && keptLines.last().trimmed().isEmpty())
        keptLines.removeLast();

    QStringList output = keptLines;
    if (!output.isEmpty())
        output.append(QString());
    output.append(managedBlockBegin());
    output.append(block.trimmed());
    output.append(managedBlockEnd());
    output.append(QString());

    QSaveFile file(expanded);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo escribir %1").arg(expanded);
        return false;
    }
    file.write(output.join(QLatin1Char('\n')).toUtf8());
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo guardar %1").arg(expanded);
        return false;
    }

    QFile::setPermissions(expanded, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

} // namespace qterm2::ssh
