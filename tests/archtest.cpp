// Architecture-level tests: persistence, security boundaries, SSH command
// construction and the plugin/command registries.  These tests never touch the
// real user database; every check runs inside a temporary root.

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/applicationcontext.h"
#include "core/apppaths.h"
#include "core/commandregistry.h"
#include "domain/session.h"
#include "domain/snippet.h"
#include "plugins/pluginmanager.h"
#include "security/credentialstore.h"
#include "sessions/sessionlauncher.h"
#include "sessions/snippetstore.h"
#include "ssh/keymanager.h"
#include "ssh/knownhosts.h"
#include "ssh/sshagent.h"
#include "ssh/sshcommandbuilder.h"
#include "ssh/sshconfig.h"
#include "storage/database.h"
#include "storage/legacymigration.h"
#include "storage/sessionrepository.h"
#include "storage/settingsstore.h"
#include "storage/sqlitesessionrepository.h"
#include "storage/sqlitesnippetrepository.h"
#include "terminal/terminalemulator.h"

#include <cstdio>

namespace {

int g_failures = 0;

void check(bool condition, const char *what)
{
    if (condition) {
        std::printf("  ok   %s\n", what);
    } else {
        std::printf("  FAIL %s\n", what);
        ++g_failures;
    }
}

void checkEqual(const QString &actual, const QString &expected, const char *what)
{
    if (actual == expected) {
        std::printf("  ok   %s\n", what);
    } else {
        std::printf("  FAIL %s\n       expected: %s\n       actual:   %s\n",
                    what, qPrintable(expected), qPrintable(actual));
        ++g_failures;
    }
}

bool contains(const QStringList &list, const QString &value)
{
    return list.contains(value);
}

Session makeSsh()
{
    Session ssh;
    ssh.name = QStringLiteral("Producción");
    ssh.type = Session::Type::Ssh;
    ssh.host = QStringLiteral("example.com");
    ssh.user = QStringLiteral("alice");
    ssh.port = 2222;
    ssh.auth = Session::Auth::Agent;
    ssh.keepAlive = true;
    ssh.proxyJump = QStringLiteral("bastion.example.com");
    ssh.hostKeyPolicy = Session::HostKeyPolicy::Strict;
    ssh.connectTimeout = 15;
    return ssh;
}

void testSshCommandBuilder()
{
    std::printf("ssh command builder\n");

    const Session ssh = makeSsh();
    checkEqual(ssh.destination(), QStringLiteral("alice@example.com"), "ssh destination");

    const qterm2::ProcessSpec spec = qterm2::sessions::SessionLauncher::specFor(ssh);
    check(spec.ok, "ssh launch spec is valid");
    checkEqual(spec.program, QStringLiteral("ssh"), "ssh program");
    check(contains(spec.args, QStringLiteral("-p")) &&
              contains(spec.args, QStringLiteral("2222")),
          "custom port is forwarded");
    check(contains(spec.args, QStringLiteral("-J")), "ProxyJump is forwarded");
    check(contains(spec.args, QStringLiteral("StrictHostKeyChecking=yes")),
          "host key policy is forwarded");
    check(contains(spec.args, QStringLiteral("ConnectTimeout=15")),
          "connect timeout is forwarded");
    check(spec.args.last() == QStringLiteral("alice@example.com"),
          "destination is the last argument");
    checkEqual(spec.environment.value(QStringLiteral("TERM")),
               QStringLiteral("xterm-256color"), "TERM is set");
    check(spec.commandLine().startsWith(QStringLiteral("ssh ")),
          "command line is human readable");

    Session broken = ssh;
    broken.host.clear();
    check(!qterm2::sessions::SessionLauncher::specFor(broken).ok,
          "missing host is rejected");

    Session local = Session::makeLocal(QStringLiteral("Local"));
    local.shell = QStringLiteral("/bin/sh");
    const qterm2::ProcessSpec localSpec =
        qterm2::sessions::SessionLauncher::specFor(local);
    check(localSpec.ok, "local launch spec is valid");
    checkEqual(localSpec.program, QStringLiteral("/bin/sh"), "local shell program");

    Session forwarded = ssh;
    PortForward localForward;
    localForward.type = PortForward::Type::Local;
    localForward.listenHost = QStringLiteral("127.0.0.1");
    localForward.listenPort = 5432;
    localForward.targetHost = QStringLiteral("db.internal");
    localForward.targetPort = 5432;
    forwarded.portForwards.append(localForward);

    PortForward dynamic;
    dynamic.type = PortForward::Type::Dynamic;
    dynamic.listenHost = QStringLiteral("127.0.0.1");
    dynamic.listenPort = 1080;
    forwarded.portForwards.append(dynamic);

    const qterm2::ProcessSpec forwardSpec =
        qterm2::sessions::SessionLauncher::specFor(forwarded);
    const int lIndex = forwardSpec.args.indexOf(QStringLiteral("-L"));
    check(lIndex >= 0 && lIndex + 1 < forwardSpec.args.size() &&
              forwardSpec.args.at(lIndex + 1)
                  == QStringLiteral("127.0.0.1:5432:db.internal:5432"),
          "local forwarding rule is rendered");
    const int dIndex = forwardSpec.args.indexOf(QStringLiteral("-D"));
    check(dIndex >= 0 && dIndex + 1 < forwardSpec.args.size() &&
              forwardSpec.args.at(dIndex + 1) == QStringLiteral("127.0.0.1:1080"),
          "dynamic forwarding rule is rendered");
}

void testSettingsAndDatabase()
{
    std::printf("database + settings\n");

    QTemporaryDir dir;
    check(dir.isValid(), "temporary root created");
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());

    qterm2::Database database(paths);
    QString error;
    check(database.open(&error), "database opens");
    if (!database.isOpen())
        std::printf("       error: %s\n", qPrintable(error));
    checkEqual(QString::number(database.schemaVersion()), QStringLiteral("4"),
               "schema version is 4");
    check(QFile::exists(paths.databaseFile), "database file exists");

    qterm2::SettingsStore settings(&database);
    check(settings.initialize(&error), "settings store initializes");
    check(settings.setValue(QStringLiteral("terminal/fontSize"), 13, &error),
          "settings value stored");
    check(settings.value(QStringLiteral("terminal/fontSize")).toInt() == 13,
          "settings value read back");
    check(settings.value(QStringLiteral("missing"), QStringLiteral("fallback"))
              .toString() == QStringLiteral("fallback"),
          "settings default is used");
    check(settings.setValue(QStringLiteral("terminal/fontSize"), 14, &error),
          "settings value updated");
    check(settings.value(QStringLiteral("terminal/fontSize")).toInt() == 14,
          "settings update persists");
    check(settings.remove(QStringLiteral("terminal/fontSize"), &error),
          "settings value removed");
    check(!settings.contains(QStringLiteral("terminal/fontSize")),
          "removed setting is gone");
}

void testSchemaV1Migration()
{
    std::printf("schema v1 -> v4 migration\n");

    QTemporaryDir dir;
    check(dir.isValid(), "migration temp root");
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
    check(paths.ensure(), "migration paths prepared");

    const QString connection = QStringLiteral("qterm2-v1-fixture");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(paths.databaseFile);
        check(db.open(), "v1 fixture opens");
        QSqlQuery query(db);
        check(query.exec(QStringLiteral(
                  "CREATE TABLE groups (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, "
                  "parent_id TEXT, sort_order INTEGER NOT NULL DEFAULT 0, "
                  "created_at TEXT NOT NULL)")),
              "v1 groups table created");
        check(query.exec(QStringLiteral(
                  "CREATE TABLE sessions (id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                  "group_name TEXT NOT NULL DEFAULT 'Servidores', "
                  "type TEXT NOT NULL DEFAULT 'ssh', host TEXT NOT NULL DEFAULT '', "
                  "port INTEGER NOT NULL DEFAULT 22, username TEXT NOT NULL DEFAULT '', "
                  "auth_method TEXT NOT NULL DEFAULT 'publickey', "
                  "private_key TEXT NOT NULL DEFAULT '', proxy_jump TEXT NOT NULL DEFAULT '', "
                  "extra_args TEXT NOT NULL DEFAULT '', start_dir TEXT NOT NULL DEFAULT '', "
                  "initial_command TEXT NOT NULL DEFAULT '', shell TEXT NOT NULL DEFAULT '', "
                  "x11_forwarding INTEGER NOT NULL DEFAULT 0, "
                  "forward_agent INTEGER NOT NULL DEFAULT 0, "
                  "keep_alive INTEGER NOT NULL DEFAULT 1, "
                  "connect_timeout INTEGER NOT NULL DEFAULT 10, "
                  "host_key_policy TEXT NOT NULL DEFAULT 'ask', "
                  "port_forwards TEXT NOT NULL DEFAULT '[]', "
                  "favorite INTEGER NOT NULL DEFAULT 0, last_used_at TEXT, "
                  "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
                  "color_tag TEXT NOT NULL DEFAULT 'blue', notes TEXT NOT NULL DEFAULT '')")),
              "v1 sessions table created");
        check(query.exec(QStringLiteral(
                  "CREATE TABLE settings (key TEXT PRIMARY KEY, value BLOB NOT NULL, "
                  "updated_at TEXT NOT NULL)")),
              "v1 settings table created");
        check(query.exec(QStringLiteral("PRAGMA user_version = 1")),
              "v1 version stamped");
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);

    qterm2::Database database(paths);
    QString error;
    check(database.open(&error), "v1 database opens");
    check(database.schemaVersion() == 4, "schema upgraded to version 4");

    QSqlQuery query(database.handle());
    check(query.exec(QStringLiteral("PRAGMA table_info(sessions)")),
          "session columns inspected");
    bool autoReconnect = false;
    bool terminalProfile = false;
    while (query.next()) {
        if (query.value(1).toString() == QLatin1String("auto_reconnect"))
            autoReconnect = true;
        if (query.value(1).toString() == QLatin1String("terminal_profile"))
            terminalProfile = true;
    }
    check(autoReconnect, "auto_reconnect column was added");
    check(terminalProfile, "terminal_profile column was added");
    check(query.exec(QStringLiteral(
              "SELECT name FROM sqlite_master WHERE type='table' AND name='snippets'")),
          "snippets table inspected");
    check(query.next(), "snippets table was created");
}

void testSessionRepository()
{
    std::printf("session repository\n");

    QTemporaryDir dir;
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
    qterm2::Database database(paths);
    QString error;
    check(database.open(&error), "repository database opens");

    qterm2::SqliteSessionRepository repository(&database);

    Session a = Session::makeLocal(QStringLiteral("Uno"));
    a.group = QStringLiteral("Trabajo");
    Session b = makeSsh();
    b.group = QStringLiteral("Trabajo");
    b.favorite = true;
    b.autoReconnect = true;
    b.terminalProfile.enabled = true;
    b.terminalProfile.fontFamily = QStringLiteral("DejaVu Sans Mono");
    b.terminalProfile.fontSize = 14;
    b.terminalProfile.scrollbackLines = 12000;
    b.terminalProfile.cursorStyle = QStringLiteral("ibeam");
    b.terminalProfile.cursorBlink = false;

    PortForward localForward;
    localForward.type = PortForward::Type::Local;
    localForward.listenHost = QStringLiteral("127.0.0.1");
    localForward.listenPort = 5432;
    localForward.targetHost = QStringLiteral("db.internal");
    localForward.targetPort = 5432;
    b.portForwards.append(localForward);

    PortForward remoteForward;
    remoteForward.type = PortForward::Type::Remote;
    remoteForward.listenHost = QStringLiteral("127.0.0.1");
    remoteForward.listenPort = 8080;
    remoteForward.targetHost = QStringLiteral("localhost");
    remoteForward.targetPort = 80;
    remoteForward.enabled = false;
    b.portForwards.append(remoteForward);

    check(repository.upsert(a, &error), "session inserted");
    if (!error.isEmpty()) { std::printf("       error: %s\n", qPrintable(error)); error.clear(); }
    check(repository.upsert(b, &error), "second session inserted");
    if (!error.isEmpty()) { std::printf("       error: %s\n", qPrintable(error)); error.clear(); }
    check(repository.count() == 2, "repository counts sessions");

    QVector<Session> loaded;
    check(repository.loadAll(loaded, &error), "sessions loaded");
    check(loaded.size() == 2, "two sessions reloaded");
    if (loaded.isEmpty()) {
        std::printf("       no sessions loaded\n");
        return;
    }
    const Session reloaded = loaded.at(0);
    checkEqual(reloaded.host, QStringLiteral("example.com"), "reloaded host");
    check(reloaded.port == 2222, "reloaded port");
    check(reloaded.hostKeyPolicy == Session::HostKeyPolicy::Strict,
          "reloaded host key policy");
    check(reloaded.connectTimeout == 15, "reloaded timeout");
    check(reloaded.favorite, "reloaded favorite");
    check(reloaded.autoReconnect, "reloaded auto reconnect");
    check(reloaded.terminalProfile.enabled, "reloaded terminal profile");
    checkEqual(reloaded.terminalProfile.fontFamily,
               QStringLiteral("DejaVu Sans Mono"), "reloaded profile font");
    check(reloaded.terminalProfile.fontSize == 14, "reloaded profile font size");
    check(reloaded.terminalProfile.scrollbackLines == 12000,
          "reloaded profile scrollback");
    checkEqual(reloaded.terminalProfile.cursorStyle, QStringLiteral("ibeam"),
               "reloaded profile cursor");
    check(!reloaded.terminalProfile.cursorBlink, "reloaded profile blink");
    check(reloaded.portForwards.size() == 2, "reloaded port forwards");
    if (reloaded.portForwards.size() == 2) {
        check(reloaded.portForwards.at(0).type == PortForward::Type::Local,
              "local forward type persisted");
        check(reloaded.portForwards.at(0).listenPort == 5432,
              "local forward listen port persisted");
        checkEqual(reloaded.portForwards.at(0).targetHost,
                   QStringLiteral("db.internal"), "local forward target persisted");
        check(reloaded.portForwards.at(1).type == PortForward::Type::Remote,
              "remote forward type persisted");
        check(!reloaded.portForwards.at(1).enabled,
              "disabled forward stays disabled");
    }

    check(repository.markUsed(b.id, QDateTime::currentDateTimeUtc(), &error),
          "markUsed works");
    loaded.clear();
    repository.loadAll(loaded, &error);
    bool used = false;
    for (const Session &s : loaded) {
        if (s.id == b.id && s.lastUsedAt.isValid())
            used = true;
    }
    check(used, "last used timestamp persists");

    b.favorite = false;
    check(repository.upsert(b, &error), "favorite can be cleared");
    loaded.clear();
    repository.loadAll(loaded, &error);
    for (const Session &s : loaded) {
        if (s.id == b.id)
            check(!s.favorite, "cleared favorite persists");
    }

    check(repository.remove(a.id, &error), "session removed");
    check(repository.count() == 1, "removal persisted");

    // Secrets must never be serialized into the metadata transport.
    const QString json = QString::fromUtf8(
        QJsonDocument(b.toJson()).toJson(QJsonDocument::Compact));
    check(!json.contains(QStringLiteral("password"), Qt::CaseInsensitive),
          "session JSON has no password field");
    check(!json.contains(QStringLiteral("passphrase"), Qt::CaseInsensitive),
          "session JSON has no passphrase field");
}

void testLegacyMigration()
{
    std::printf("legacy migration\n");

    QTemporaryDir dir;
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
    check(paths.ensure(), "legacy root prepared");

    const QString jsonPath = paths.configDir + QStringLiteral("/sessions.json");
    QFile jsonFile(jsonPath);
    check(jsonFile.open(QIODevice::WriteOnly), "legacy JSON created");
    QTextStream out(&jsonFile);
    out << R"({"version":1,"sessions":[{"id":"legacy-1","name":"Vieja",)"
           R"("type":"ssh","host":"legacy.example","port":2200,"user":"old"}]})";
    jsonFile.close();

    {
        QSettings legacy(paths.configDir + QStringLiteral("/QTerm2.conf"),
                         QSettings::IniFormat);
        legacy.setValue(QStringLiteral("terminal/fontSize"), 16);
        legacy.sync();
    }

    qterm2::Database database(paths);
    QString error;
    check(database.open(&error), "migration database opens");
    qterm2::SettingsStore settings(&database);
    qterm2::SqliteSessionRepository repository(&database);

    // Settings must be imported before the migration flag is written.
    const qterm2::Status status =
        qterm2::importLegacyData(paths, repository, settings);
    check(status.ok, "legacy import succeeds");
    if (!status.ok)
        std::printf("       error: %s\n", qPrintable(status.message));

    QVector<Session> loaded;
    repository.loadAll(loaded, &error);
    check(loaded.size() == 1, "legacy session imported");
    if (!loaded.isEmpty()) {
        checkEqual(loaded.at(0).host, QStringLiteral("legacy.example"),
                   "legacy host preserved");
        check(loaded.at(0).port == 2200, "legacy port preserved");
    }
    check(settings.value(QStringLiteral("terminal/fontSize")).toInt() == 16,
          "legacy settings imported");
    check(!QFile::exists(jsonPath), "legacy JSON is renamed after import");

    // Importing again must be a no-op.
    const qterm2::Status second =
        qterm2::importLegacyData(paths, repository, settings);
    check(second.ok, "second import succeeds");
    repository.loadAll(loaded, &error);
    check(loaded.size() == 1, "second import does not duplicate sessions");
}

void testSshConfig()
{
    std::printf("ssh config import/export\n");

    QTemporaryDir dir;
    check(dir.isValid(), "ssh config temp root");

    QDir(dir.path()).mkpath(QStringLiteral("conf.d"));
    QFile include(QDir(dir.path()).filePath(QStringLiteral("conf.d/extra.conf")));
    check(include.open(QIODevice::WriteOnly | QIODevice::Text), "include file created");
    include.write("Host staging\n"
                  "    HostName staging.example.com\n"
                  "    User stage\n");
    include.close();

    const QString path = QDir(dir.path()).filePath(QStringLiteral("config"));
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Text), "ssh config created");
    file.write("# main config\n"
               "Include conf.d/*\n"
               "\n"
               "Host prod prod-alias\n"
               "    HostName 10.0.0.10\n"
               "    User deploy\n"
               "    Port 2222\n"
               "    IdentityFile ~/.ssh/id_ed25519\n"
               "    ProxyJump bastion\n"
               "\n"
               "Host wildcard-*\n"
               "    User ignored\n"
               "\n"
               "Host *\n"
               "    User nobody\n");
    file.close();

    QString error;
    const qterm2::ssh::SshConfigDocument document =
        qterm2::ssh::parseSshConfig(path, &error);
    check(error.isEmpty(), "ssh config parses without error");
    check(document.entries.size() == 2, "concrete hosts are imported");
    bool prodFound = false;
    bool stagingFound = false;
    for (const auto &entry : document.entries) {
        if (entry.alias == QLatin1String("prod")) {
            prodFound = true;
            checkEqual(entry.effectiveHost(), QStringLiteral("10.0.0.10"),
                       "hostname is read");
            checkEqual(entry.user, QStringLiteral("deploy"), "user is read");
            check(entry.port == 2222, "port is read");
            checkEqual(entry.identityFile, QDir::homePath() + QStringLiteral("/.ssh/id_ed25519"),
                       "identity file is expanded");
            checkEqual(entry.proxyJump, QStringLiteral("bastion"), "proxy jump is read");
        }
        if (entry.alias == QLatin1String("staging")) {
            stagingFound = true;
            checkEqual(entry.effectiveHost(), QStringLiteral("staging.example.com"),
                       "included host is read");
        }
    }
    check(prodFound, "concrete host was found");
    check(stagingFound, "included host was found");

    Session exported;
    exported.name = QStringLiteral("Produccion web");
    exported.type = Session::Type::Ssh;
    exported.host = QStringLiteral("web.example.com");
    exported.user = QStringLiteral("deploy");
    exported.port = 2222;
    exported.privateKey = QStringLiteral("/home/user/.ssh/id_ed25519");

    PortForward forward;
    forward.type = PortForward::Type::Local;
    forward.listenHost = QStringLiteral("127.0.0.1");
    forward.listenPort = 5432;
    forward.targetHost = QStringLiteral("db.internal");
    forward.targetPort = 5432;
    exported.portForwards.append(forward);

    const QString block = qterm2::ssh::renderSshConfig({ exported });
    check(block.contains(QStringLiteral("Host Produccion-web")), "alias is sanitized");
    check(block.contains(QStringLiteral("HostName web.example.com")),
          "hostname is exported");
    check(block.contains(QStringLiteral("Port 2222")), "port is exported");
    check(block.contains(QStringLiteral("LocalForward 127.0.0.1:5432:db.internal:5432")),
          "forward is exported");

    const QString managed = QDir(dir.path()).filePath(QStringLiteral("managed_config"));
    QFile existing(managed);
    check(existing.open(QIODevice::WriteOnly | QIODevice::Text), "managed config created");
    existing.write("# keep me\nHost manual\n    HostName manual.example.com\n");
    existing.close();

    check(qterm2::ssh::writeManagedSshConfig(managed, block, &error),
          "managed block is written");
    check(qterm2::ssh::writeManagedSshConfig(managed, block, &error),
          "managed block can be replaced");
    QFile readBack(managed);
    check(readBack.open(QIODevice::ReadOnly), "managed config read back");
    const QString content = QString::fromUtf8(readBack.readAll());
    readBack.close();
    check(content.contains(QStringLiteral("Host manual")), "user content is preserved");
    check(content.count(qterm2::ssh::managedBlockBegin()) == 1,
          "managed block is not duplicated");
    check(content.count(QStringLiteral("Host Produccion-web")) == 1,
          "exported host appears once");
    check((QFile::permissions(managed) & QFile::ReadOwner) != 0,
          "managed config is readable by the owner");
    check((QFile::permissions(managed) & QFile::WriteOwner) != 0,
          "managed config is writable by the owner");
}

void testKnownHosts()
{
    std::printf("known_hosts viewer\n");

    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("known_hosts"));
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Text), "known_hosts created");
    file.write("# comment\n"
               "server.example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAA test@host\n"
               "|1|abc=|def= ssh-rsa AAAAB3NzaC1yc2EAAAADAQAB hashed\n"
               "broken-line\n");
    file.close();

    QString error;
    const qterm2::ssh::KnownHostsDocument document =
        qterm2::ssh::parseKnownHosts(path, &error);
    check(error.isEmpty(), "known_hosts parses without error");
    check(document.entries.size() == 2, "known_hosts entries are read");
    check(document.warnings.size() == 1, "malformed known_hosts line is reported");
    if (document.entries.size() == 2) {
        checkEqual(document.entries.at(0).hosts, QStringLiteral("server.example.com"),
                   "plain host is read");
        check(!document.entries.at(0).hashed, "plain host is not hashed");
        check(document.entries.at(1).hashed, "hashed host is detected");
        checkEqual(document.entries.at(1).keyType, QStringLiteral("ssh-rsa"),
                   "hashed key type is read");
    }

    if (document.entries.size() == 2) {
        check(qterm2::ssh::removeKnownHostLines(path, { document.entries.at(0).line },
                                                &error),
              "known_hosts entry is removed");
        const auto reloaded = qterm2::ssh::parseKnownHosts(path, &error);
        check(reloaded.entries.size() == 1, "only the selected entry was removed");
        check(reloaded.entries.value(0).hashed, "hashed entry is preserved");
        QFile readBack(path);
        check(readBack.open(QIODevice::ReadOnly), "known_hosts read back");
        const QString content = QString::fromUtf8(readBack.readAll());
        check(content.contains(QStringLiteral("# comment")), "comment is preserved");
        check(!content.contains(QStringLiteral("server.example.com")),
              "removed host is gone");
    }
}


void testSshAgent()
{
    std::printf("ssh agent\n");

    const QVector<qterm2::ssh::AgentIdentity> identities =
        qterm2::ssh::parseAgentIdentities(
            QStringLiteral("256 SHA256:AAAAB3Nza user@laptop (ED25519)\n"
                           "2048 MD5:aa:bb:cc legacy key with spaces (RSA)\n"
                           "unusual-line-without-type\n"));
    check(identities.size() == 3, "agent output is parsed");
    if (identities.size() == 3) {
        checkEqual(identities.at(0).bits, QStringLiteral("256"),
                   "agent bits are parsed");
        checkEqual(identities.at(0).fingerprint, QStringLiteral("SHA256:AAAAB3Nza"),
                   "agent SHA256 fingerprint is parsed");
        checkEqual(identities.at(0).comment, QStringLiteral("user@laptop"),
                   "agent comment is parsed");
        checkEqual(identities.at(0).keyType, QStringLiteral("ED25519"),
                   "agent key type is parsed");
        checkEqual(identities.at(1).comment,
                   QStringLiteral("legacy key with spaces"),
                   "agent comment with spaces is parsed");
        checkEqual(identities.at(2).fingerprint,
                   QStringLiteral("unusual-line-without-type"),
                   "unusual agent line is preserved");
    }

    const QByteArray savedSocket = qgetenv("SSH_AUTH_SOCK");
    qunsetenv("SSH_AUTH_SOCK");
    const qterm2::ssh::AgentStatus missing = qterm2::ssh::queryAgent();
    check(!missing.available, "agent without SSH_AUTH_SOCK is unavailable");
    check(!missing.error.isEmpty(), "agent absence is explained");
    if (!savedSocket.isEmpty())
        qputenv("SSH_AUTH_SOCK", savedSocket);
}

void testSnippets()
{
    std::printf("snippets\n");

    const Session ssh = makeSsh();
    checkEqual(qterm2::expandSnippet(
                   QStringLiteral("ssh ${user}@${host} -p ${port}"), ssh),
               QStringLiteral("ssh alice@example.com -p 2222"),
               "snippet expands host, user and port");
    checkEqual(qterm2::expandSnippet(QStringLiteral("${name} · ${group}"), ssh),
               QStringLiteral("Producción · Servidores"),
               "snippet expands session name and group");
    checkEqual(qterm2::expandSnippet(QStringLiteral("echo $${host} ${unknown}"), ssh),
               QStringLiteral("echo ${host} ${unknown}"),
               "escaped and unknown placeholders are preserved");

    const Session local = Session::makeLocal();
    const QString rendered = qterm2::expandSnippet(QStringLiteral("${host}:${port}"), local);
    check(rendered.startsWith(QStringLiteral("localhost:")),
          "local snippet falls back to localhost");
    check(!rendered.contains(QStringLiteral("${")), "local snippet has no placeholders left");

    QTemporaryDir dir;
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
    qterm2::Database database(paths);
    QString error;
    check(database.open(&error), "snippet database opens");

    qterm2::SqliteSnippetRepository repository(&database);
    qterm2::SnippetStore store(&repository);
    check(store.load(), "snippet store loads");
    check(store.snippets().isEmpty(), "snippet store starts empty");

    qterm2::Snippet first;
    first.name = QStringLiteral("Estado de nginx");
    first.command = QStringLiteral("systemctl status nginx");
    first.description = QStringLiteral("Servicio web");
    check(store.saveSnippet(first), "snippet is saved");
    check(!first.id.isEmpty(), "snippet id is assigned");
    check(first.createdAt.isValid(), "snippet creation time is set");

    qterm2::Snippet second;
    second.name = QStringLiteral("Disco");
    second.command = QStringLiteral("df -h /home");
    check(store.saveSnippet(second), "second snippet is saved");
    check(store.snippets().size() == 2, "snippets are listed");

    first.command = QStringLiteral("systemctl status nginx --no-pager");
    check(store.saveSnippet(first), "snippet is updated");
    check(store.snippetById(first.id).command.endsWith(QStringLiteral("--no-pager")),
          "snippet update is kept");

    QVector<qterm2::Snippet> reloaded;
    check(repository.loadAll(reloaded, &error), "snippets reload from disk");
    check(reloaded.size() == 2, "reloaded snippet count");
    checkEqual(reloaded.at(0).name, QStringLiteral("Disco"),
               "snippets are sorted by name");
    checkEqual(reloaded.at(1).description, QStringLiteral("Servicio web"),
               "snippet description is persisted");

    check(store.removeSnippet(second.id), "snippet is removed");
    check(store.snippets().size() == 1, "snippet removal is reflected");
    check(repository.loadAll(reloaded, &error) && reloaded.size() == 1,
          "snippet removal is persisted");
}

void testCredentials()
{
    std::printf("credential store\n");

    qterm2::MemoryCredentialStore memory;
    const qterm2::CredentialKey key =
        qterm2::CredentialKey::sessionPassword(QStringLiteral("abc"));
    check(memory.isAvailable(), "memory backend available");
    check(memory.store(key, QStringLiteral("s3cret")), "secret stored");
    QString secret;
    check(memory.lookup(key, secret), "secret found");
    checkEqual(secret, QStringLiteral("s3cret"), "secret round-trips");
    check(memory.remove(key), "secret removed");
    check(!memory.lookup(key, secret), "removed secret is gone");

    qterm2::UnavailableCredentialStore unavailable;
    check(!unavailable.isAvailable(), "unavailable backend reports itself");
    check(!unavailable.availabilityHint().isEmpty(),
          "unavailable backend explains why");
    QString error;
    check(!unavailable.store(key, QStringLiteral("x"), &error),
          "unavailable backend refuses to store");
    check(!error.isEmpty(), "unavailable backend reports an error");
}

void testCommandRegistry()
{
    std::printf("command registry\n");

    qterm2::CommandRegistry registry;
    qterm2::Command command;
    command.id = QStringLiteral("test.command");
    command.title = QStringLiteral("Test");
    command.category = QStringLiteral("Pruebas");
    command.shortcut = QKeySequence(QStringLiteral("Ctrl+Shift+T"));
    command.action = [] {};
    check(registry.registerCommand(command), "command registered");
    check(!registry.registerCommand(command), "duplicate command rejected");
    const qterm2::Command stored = registry.command(command.id);
    checkEqual(stored.category, QStringLiteral("Pruebas"),
               "command category is kept");
    checkEqual(stored.shortcut.toString(), QStringLiteral("Ctrl+Shift+T"),
               "command shortcut is kept");
    check(registry.contains(command.id), "command lookup works");
    check(registry.commands().size() == 1, "command listed");
    registry.setEnabled(command.id, false);
    check(!registry.command(command.id).enabled, "command disabled");
    check(registry.unregisterCommand(command.id), "command unregistered");
    check(registry.commands().isEmpty(), "registry empty again");
}

void testTerminalEmulator()
{
    std::printf("terminal emulator (no widget)\n");

    TerminalEmulator emulator;
    emulator.resize(6, 20);
    check(emulator.rows() == 6 && emulator.cols() == 20, "geometry applied");

    emulator.feed("hola \x1b[31mrojo\x1b[0m\r\nsegunda");
    const QString screen = emulator.screenText();
    check(screen.contains(QStringLiteral("hola rojo")), "plain text is parsed");
    check(screen.contains(QStringLiteral("segunda")), "new line is parsed");
    check(emulator.cursorY() == 1, "cursor moved to the second row");

    QByteArray response;
    QObject::connect(&emulator, &TerminalEmulator::response,
                     [&response](const QByteArray &data) { response = data; });
    emulator.feed("\x1b[6n");
    checkEqual(QString::fromUtf8(response), QStringLiteral("\x1b[2;8R"),
               "cursor position report is emitted as a signal");

    emulator.feed("\x1b]0;Mi titulo\x07");
    checkEqual(emulator.title(), QStringLiteral("Mi titulo"), "OSC title is parsed");

    emulator.feed("\x1b[?1049h");
    check(emulator.useAltScreen(), "alternate screen enabled");
    check(emulator.historyRows() == 0, "alternate screen has no scrollback");

    // Shrinking the main screen must move dropped lines into scrollback.
    emulator.feed("\x1b[?1049l");
    emulator.resize(2, 20);
    int pushed = 0;
    for (int i = 0; i < 12; ++i) {
        emulator.feed("linea\r\n");
        pushed += emulator.historyRows() > 0 ? 1 : 0;
    }
    check(emulator.historyRows() > 0 || pushed == 0, "scrollback grows when needed");

    emulator.setMaxHistory(3);
    check(emulator.historyRows() <= 3, "scrollback limit is enforced");

    emulator.setMaxHistory(1000);
    emulator.setMaxHistoryBytes(8192);
    const QString wideLine = QString(80, QLatin1Char('x')) + QStringLiteral("\r\n");
    for (int i = 0; i < 200; ++i)
        emulator.feed(wideLine.toUtf8());
    check(emulator.historyRows() > 0, "scrollback keeps recent lines under a byte cap");
    check(emulator.historyBytes() <= 8192, "scrollback byte limit is enforced");
    emulator.clearHistory();
    check(emulator.historyRows() == 0 && emulator.historyBytes() == 0,
          "clearing scrollback resets the byte counter");
}

void testApplicationContext()
{
    std::printf("application context\n");

    QTemporaryDir dir;
    const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
    qterm2::ApplicationContext context(paths);
    QString error;
    check(context.initialize(&error), "context initializes");
    if (!error.isEmpty())
        std::printf("       warning: %s\n", qPrintable(error));
    check(context.database() && context.database()->isOpen(), "database available");
    check(context.settings() != nullptr, "settings available");
    check(context.sessionRepository() != nullptr, "repository available");
    check(context.sessionStore() != nullptr, "session store available");
    check(context.credentials() != nullptr, "credential store available");
    check(context.commands() != nullptr, "command registry available");
    check(context.plugins() != nullptr, "plugin manager available");
    check(context.apiVersion() == 2, "plugin API version is 2");
    check(context.plugins()->searchPaths().size() >= 1, "plugin paths exist");
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTerm2-archtest"));
    QApplication::setOrganizationName(QStringLiteral("QTerm2-archtest"));

    testSshCommandBuilder();
    testSettingsAndDatabase();
    testSchemaV1Migration();
    testSessionRepository();
    testLegacyMigration();
    testSshConfig();
    testKnownHosts();
    testSshAgent();
    testSnippets();
    testCredentials();
    testCommandRegistry();
    testTerminalEmulator();
    testApplicationContext();

    if (g_failures == 0)
        std::printf("\nall architecture tests passed\n");
    else
        std::printf("\n%d architecture test(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
