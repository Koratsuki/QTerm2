#include "domain/session.h"

#include "core/pathutils.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QUuid>

using qterm2::expandHome;

QString shellQuote(const QString &value)
{
    if (value.isEmpty())
        return QStringLiteral("''");
    bool simple = true;
    for (QChar c : value) {
        if (!(c.isLetterOrNumber() || QStringLiteral("@%_+=:,./-").contains(c))) {
            simple = false;
            break;
        }
    }
    if (simple)
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString commandLineFor(const QString &program, const QStringList &args)
{
    QStringList parts;
    parts << shellQuote(program);
    for (const QString &a : args)
        parts << shellQuote(a);
    return parts.join(QLatin1Char(' '));
}

QString defaultShellPath()
{
    const QString envShell = QString::fromLocal8Bit(qgetenv("SHELL"));
    if (!envShell.isEmpty() && QFileInfo(envShell).isExecutable())
        return envShell;
    for (const QString &candidate : { QStringLiteral("/bin/zsh"),
                                      QStringLiteral("/bin/bash"),
                                      QStringLiteral("/bin/sh") }) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QStringLiteral("/bin/sh");
}

namespace {

QString sshDestination(const QString &user, const QString &host)
{
    QString h = host;
    if (h.contains(QLatin1Char(':')) && !h.startsWith(QLatin1Char('[')))
        h = QLatin1Char('[') + h + QLatin1Char(']');
    return user.isEmpty() ? h : user + QLatin1Char('@') + h;
}

QDateTime fromIso(const QJsonValue &value)
{
    const QString text = value.toString();
    if (text.isEmpty())
        return QDateTime();
    return QDateTime::fromString(text, Qt::ISODate);
}

QString toIso(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toString(Qt::ISODate) : QString();
}

} // namespace

// ------------------------------------------------------------- port forwarding

QJsonObject PortForward::toJson() const
{
    QJsonObject obj;
    switch (type) {
    case Type::Local:   obj[QStringLiteral("type")] = QStringLiteral("local"); break;
    case Type::Remote:  obj[QStringLiteral("type")] = QStringLiteral("remote"); break;
    case Type::Dynamic: obj[QStringLiteral("type")] = QStringLiteral("dynamic"); break;
    }
    obj[QStringLiteral("listenHost")] = listenHost;
    obj[QStringLiteral("listenPort")] = listenPort;
    obj[QStringLiteral("targetHost")] = targetHost;
    obj[QStringLiteral("targetPort")] = targetPort;
    obj[QStringLiteral("enabled")] = enabled;
    return obj;
}

PortForward PortForward::fromJson(const QJsonObject &obj)
{
    PortForward forward;
    const QString type = obj.value(QStringLiteral("type")).toString();
    forward.type = type == QLatin1String("remote") ? Type::Remote
                 : type == QLatin1String("dynamic") ? Type::Dynamic
                                                    : Type::Local;
    forward.listenHost =
        obj.value(QStringLiteral("listenHost")).toString(QStringLiteral("127.0.0.1"));
    forward.listenPort = obj.value(QStringLiteral("listenPort")).toInt(0);
    forward.targetHost = obj.value(QStringLiteral("targetHost")).toString();
    forward.targetPort = obj.value(QStringLiteral("targetPort")).toInt(0);
    forward.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    return forward;
}

QString PortForward::typeName(Type type)
{
    switch (type) {
    case Type::Local:   return QStringLiteral("Local (-L)");
    case Type::Remote:  return QStringLiteral("Remoto (-R)");
    case Type::Dynamic: return QStringLiteral("SOCKS5 (-D)");
    }
    return QString();
}

// ------------------------------------------------------------ terminal profile

QJsonObject TerminalProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("enabled")] = enabled;
    obj[QStringLiteral("fontFamily")] = fontFamily;
    obj[QStringLiteral("fontSize")] = fontSize;
    obj[QStringLiteral("scrollbackLines")] = scrollbackLines;
    obj[QStringLiteral("cursorStyle")] = cursorStyle;
    obj[QStringLiteral("cursorBlink")] = cursorBlink;
    return obj;
}

TerminalProfile TerminalProfile::fromJson(const QJsonObject &obj)
{
    TerminalProfile profile;
    profile.enabled = obj.value(QStringLiteral("enabled")).toBool(false);
    profile.fontFamily = obj.value(QStringLiteral("fontFamily")).toString();
    profile.fontSize = obj.value(QStringLiteral("fontSize")).toInt(0);
    profile.scrollbackLines =
        obj.value(QStringLiteral("scrollbackLines")).toInt(0);
    profile.cursorStyle = obj.value(QStringLiteral("cursorStyle")).toString();
    profile.cursorBlink = obj.value(QStringLiteral("cursorBlink")).toBool(true);
    return profile;
}

// -------------------------------------------------------------------- session

QString Session::destination() const
{
    return sshDestination(user, host);
}

QString Session::displayTarget() const
{
    if (isLocal())
        return shell.isEmpty() ? defaultShellPath() : shell;
    return QStringLiteral("%1@%2:%3")
        .arg(user.isEmpty() ? QStringLiteral("(usuario)") : user, host)
        .arg(port);
}

QString Session::publicKeyPath() const
{
    const QString key = expandHome(privateKey);
    if (key.isEmpty())
        return QString();
    if (QFileInfo::exists(key + QStringLiteral(".pub")))
        return key + QStringLiteral(".pub");
    return key;
}

void Session::touchCreated()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!createdAt.isValid())
        createdAt = now;
    updatedAt = now;
}

void Session::touchUpdated()
{
    updatedAt = QDateTime::currentDateTimeUtc();
}

QJsonObject Session::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("group")] = group;
    obj[QStringLiteral("type")] = type == Type::Ssh ? QStringLiteral("ssh")
                                 : type == Type::Telnet ? QStringLiteral("telnet")
                                                        : QStringLiteral("local");
    obj[QStringLiteral("host")] = host;
    obj[QStringLiteral("port")] = port;
    obj[QStringLiteral("user")] = user;
    QString authValue = QStringLiteral("publickey");
    if (auth == Auth::Agent) authValue = QStringLiteral("agent");
    else if (auth == Auth::Password) authValue = QStringLiteral("password");
    obj[QStringLiteral("auth")] = authValue;
    obj[QStringLiteral("privateKey")] = privateKey;
    obj[QStringLiteral("proxyJump")] = proxyJump;
    obj[QStringLiteral("extraArgs")] = extraArgs;
    obj[QStringLiteral("startDir")] = startDir;
    obj[QStringLiteral("initialCommand")] = initialCommand;
    obj[QStringLiteral("shell")] = shell;
    obj[QStringLiteral("x11Forwarding")] = x11Forwarding;
    obj[QStringLiteral("forwardAgent")] = forwardAgent;
    obj[QStringLiteral("keepAlive")] = keepAlive;
    obj[QStringLiteral("autoReconnect")] = autoReconnect;
    obj[QStringLiteral("connectTimeout")] = connectTimeout;
    obj[QStringLiteral("hostKeyPolicy")] =
        hostKeyPolicy == HostKeyPolicy::Strict ? QStringLiteral("strict")
        : hostKeyPolicy == HostKeyPolicy::AcceptNew ? QStringLiteral("accept-new")
                                                    : QStringLiteral("ask");
    QJsonArray forwards;
    for (const PortForward &forward : portForwards)
        forwards.append(forward.toJson());
    obj[QStringLiteral("portForwards")] = forwards;
    obj[QStringLiteral("terminalProfile")] = terminalProfile.toJson();
    obj[QStringLiteral("favorite")] = favorite;
    obj[QStringLiteral("lastUsedAt")] = toIso(lastUsedAt);
    obj[QStringLiteral("createdAt")] = toIso(createdAt);
    obj[QStringLiteral("updatedAt")] = toIso(updatedAt);
    obj[QStringLiteral("colorTag")] = colorTag;
    obj[QStringLiteral("notes")] = notes;
    return obj;
}

Session Session::fromJson(const QJsonObject &obj)
{
    Session s;
    s.id = obj.value(QStringLiteral("id")).toString();
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = obj.value(QStringLiteral("name")).toString();
    s.group = obj.value(QStringLiteral("group")).toString(QStringLiteral("Servidores"));
    const QString typeStr = obj.value(QStringLiteral("type")).toString();
    s.type = typeStr == QStringLiteral("local") ? Type::Local
           : typeStr == QStringLiteral("telnet") ? Type::Telnet
                                                : Type::Ssh;
    s.host = obj.value(QStringLiteral("host")).toString();
    s.port = obj.value(QStringLiteral("port")).toInt(22);
    s.user = obj.value(QStringLiteral("user")).toString();
    const QString auth = obj.value(QStringLiteral("auth")).toString();
    s.auth = auth == QStringLiteral("agent") ? Auth::Agent
             : auth == QStringLiteral("password") ? Auth::Password : Auth::PublicKey;
    s.privateKey = obj.value(QStringLiteral("privateKey")).toString();
    s.proxyJump = obj.value(QStringLiteral("proxyJump")).toString();
    s.extraArgs = obj.value(QStringLiteral("extraArgs")).toString();
    s.startDir = obj.value(QStringLiteral("startDir")).toString();
    s.initialCommand = obj.value(QStringLiteral("initialCommand")).toString();
    s.shell = obj.value(QStringLiteral("shell")).toString();
    s.x11Forwarding = obj.value(QStringLiteral("x11Forwarding")).toBool(false);
    s.forwardAgent = obj.value(QStringLiteral("forwardAgent")).toBool(false);
    s.keepAlive = obj.value(QStringLiteral("keepAlive")).toBool(true);
    s.autoReconnect = obj.value(QStringLiteral("autoReconnect")).toBool(false);
    s.connectTimeout = obj.value(QStringLiteral("connectTimeout")).toInt(10);
    const QString policy = obj.value(QStringLiteral("hostKeyPolicy")).toString();
    s.hostKeyPolicy = policy == QLatin1String("strict") ? HostKeyPolicy::Strict
                    : policy == QLatin1String("accept-new") ? HostKeyPolicy::AcceptNew
                                                            : HostKeyPolicy::Ask;
    for (const QJsonValue &value : obj.value(QStringLiteral("portForwards")).toArray()) {
        if (value.isObject())
            s.portForwards.append(PortForward::fromJson(value.toObject()));
    }
    s.terminalProfile = TerminalProfile::fromJson(
        obj.value(QStringLiteral("terminalProfile")).toObject());
    s.favorite = obj.value(QStringLiteral("favorite")).toBool(false);
    s.lastUsedAt = fromIso(obj.value(QStringLiteral("lastUsedAt")));
    s.createdAt = fromIso(obj.value(QStringLiteral("createdAt")));
    s.updatedAt = fromIso(obj.value(QStringLiteral("updatedAt")));
    s.colorTag = obj.value(QStringLiteral("colorTag")).toString(QStringLiteral("blue"));
    s.notes = obj.value(QStringLiteral("notes")).toString();
    if (s.name.trimmed().isEmpty())
        s.name = s.host.isEmpty() ? QStringLiteral("Sesión") : s.host;
    if (s.type == Type::Local && s.shell.isEmpty())
        s.shell = defaultShellPath();
    return s;
}

Session Session::makeLocal(const QString &name)
{
    Session s;
    s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = name;
    s.group = QStringLiteral("Locales");
    s.type = Type::Local;
    s.shell = defaultShellPath();
    s.startDir = QDir::homePath();
    s.colorTag = QStringLiteral("green");
    s.keepAlive = false;
    s.touchCreated();
    return s;
}

QString Session::colorForTag(const QString &tag)
{
    if (tag == QLatin1String("green"))  return QStringLiteral("#34d399");
    if (tag == QLatin1String("purple")) return QStringLiteral("#a78bfa");
    if (tag == QLatin1String("orange")) return QStringLiteral("#fb923c");
    if (tag == QLatin1String("red"))    return QStringLiteral("#f87171");
    if (tag == QLatin1String("gray"))   return QStringLiteral("#94a3b8");
    if (tag == QLatin1String("yellow")) return QStringLiteral("#fbbf24");
    return QStringLiteral("#60a5fa");
}

QStringList Session::colorTags()
{
    return { QStringLiteral("blue"), QStringLiteral("green"), QStringLiteral("purple"),
             QStringLiteral("orange"), QStringLiteral("red"), QStringLiteral("yellow"),
             QStringLiteral("gray") };
}

QString Session::authName(Auth auth)
{
    switch (auth) {
    case Auth::PublicKey: return QStringLiteral("Llave pública");
    case Auth::Agent:     return QStringLiteral("Agente SSH");
    case Auth::Password:  return QStringLiteral("Contraseña");
    }
    return QString();
}

QString Session::typeName(Type type)
{
    switch (type) {
    case Type::Local: return QStringLiteral("Local");
    case Type::Ssh:   return QStringLiteral("SSH");
    case Type::Telnet: return QStringLiteral("Telnet");
    }
    return QStringLiteral("SSH");
}

QString Session::hostKeyPolicyName(HostKeyPolicy policy)
{
    switch (policy) {
    case HostKeyPolicy::Ask:       return QStringLiteral("Preguntar");
    case HostKeyPolicy::Strict:    return QStringLiteral("Estricta");
    case HostKeyPolicy::AcceptNew: return QStringLiteral("Aceptar nuevas");
    }
    return QString();
}
