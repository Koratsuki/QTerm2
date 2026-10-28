#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct LaunchSpec
{
    QString program;
    QStringList args;
    QString workDir;
    bool ok = false;
    QString error;
};

/** One SSH port forwarding rule. */
struct PortForward
{
    enum class Type { Local, Remote, Dynamic };

    Type type = Type::Local;
    QString listenHost = QStringLiteral("127.0.0.1");
    int listenPort = 0;
    QString targetHost;
    int targetPort = 0;
    bool enabled = true;

    QJsonObject toJson() const;
    static PortForward fromJson(const QJsonObject &obj);
    static QString typeName(Type type);
};

/**
 * Per-session terminal overrides.
 *
 * When `enabled` is false the global preferences are used for every field.
 * Empty strings and zero values also fall back to the global setting, so a
 * profile can override only the fields the user cares about.
 */
struct TerminalProfile
{
    bool enabled = false;
    QString fontFamily;
    int fontSize = 0;
    int scrollbackLines = 0;
    QString cursorStyle;          // block | ibeam | underscore
    bool cursorBlink = true;

    QJsonObject toJson() const;
    static TerminalProfile fromJson(const QJsonObject &obj);
    bool isEmpty() const { return !enabled; }
};

/**
 * Persisted server/session metadata.
 *
 * This is a value object: it contains no connection secrets.  Passwords and
 * passphrases are never written here; optional secrets are addressed by a
 * deterministic credential key handled by the Secret Service backend.
 */
struct Session
{
    enum class Type { Local, Ssh, Telnet };
    enum class Auth { PublicKey, Agent, Password };

    /** How OpenSSH should treat an unknown host key. */
    enum class HostKeyPolicy { Ask, Strict, AcceptNew };

    QString id;
    QString name;
    QString group = QStringLiteral("Servidores");
    Type type = Type::Ssh;
    QString host;
    int port = 22;
    QString user;
    Auth auth = Auth::PublicKey;
    QString privateKey;
    QString proxyJump;
    QString extraArgs;
    QString startDir;
    QString initialCommand;
    QString shell;
    bool x11Forwarding = false;
    bool forwardAgent = false;
    bool keepAlive = true;
    bool autoReconnect = false;
    int connectTimeout = 10;
    HostKeyPolicy hostKeyPolicy = HostKeyPolicy::Ask;
    QVector<PortForward> portForwards;
    TerminalProfile terminalProfile;
    bool favorite = false;
    QDateTime lastUsedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
    QString colorTag = QStringLiteral("blue");
    QString notes;

    bool isLocal() const { return type == Type::Local; }
    QString destination() const;
    QString displayTarget() const;
    QString publicKeyPath() const;

    /** New sessions get timestamps; called by the repository on insert. */
    void touchCreated();
    void touchUpdated();

    /** Legacy JSON transport, still used for the one-time migration. */
    QJsonObject toJson() const;
    static Session fromJson(const QJsonObject &obj);

    static Session makeLocal(const QString &name = QStringLiteral("Mi equipo"));
    static QString colorForTag(const QString &tag);
    static QStringList colorTags();
    static QString authName(Auth auth);
    static QString typeName(Type type);
    static QString hostKeyPolicyName(HostKeyPolicy policy);
};

QString shellQuote(const QString &value);
QString commandLineFor(const QString &program, const QStringList &args);
QString defaultShellPath();
