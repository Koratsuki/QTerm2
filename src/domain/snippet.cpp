#include "domain/snippet.h"

#include <QProcessEnvironment>

namespace qterm2 {

namespace {

QString currentUser()
{
    const QString fromEnv = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("USER"));
    return fromEnv.isEmpty() ? QStringLiteral("root") : fromEnv;
}

} // namespace

QString expandSnippet(const QString &text, const Session &session)
{
    // Protect escaped dollar signs before replacing placeholders, so `$${host}`
    // stays a literal `${host}`.
    const QString sentinel = QStringLiteral("\x01");
    QString result = text;
    result.replace(QStringLiteral("$$"), sentinel);

    const QString host =
        session.host.trimmed().isEmpty() ? QStringLiteral("localhost") : session.host;
    const QString user =
        session.user.trimmed().isEmpty() ? currentUser() : session.user;
    const QString port = QString::number(session.port > 0 ? session.port : 22);

    result.replace(QStringLiteral("${host}"), host);
    result.replace(QStringLiteral("${user}"), user);
    result.replace(QStringLiteral("${port}"), port);
    result.replace(QStringLiteral("${name}"), session.name);
    result.replace(QStringLiteral("${group}"), session.group);

    result.replace(sentinel, QStringLiteral("$"));
    return result;
}

} // namespace qterm2
