// Headless smoke tests for the pieces that do not need a window manager.
//
// Covered here:
//   * SSH / local launch command construction
//   * Session JSON round-trip
//   * SessionStore persistence over the SQLite repository
//   * PTY + terminal parser (UTF-8, SGR colours, cursor movement)

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "app/applicationcontext.h"
#include "core/apppaths.h"
#include "domain/session.h"
#include "sessions/sessionlauncher.h"
#include "sessions/sessionstore.h"
#include "ssh/keymanager.h"
#include "terminal/terminalwidget.h"

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

void testSessions()
{
    std::printf("sessions\n");

    Session ssh;
    ssh.name = QStringLiteral("Producción");
    ssh.type = Session::Type::Ssh;
    ssh.host = QStringLiteral("example.com");
    ssh.user = QStringLiteral("alice");
    ssh.port = 2222;
    ssh.auth = Session::Auth::Agent;
    ssh.keepAlive = true;
    ssh.proxyJump = QStringLiteral("bastion.example.com");

    checkEqual(ssh.destination(), QStringLiteral("alice@example.com"), "ssh destination");

    const qterm2::ProcessSpec spec = qterm2::sessions::SessionLauncher::specFor(ssh);
    check(spec.ok, "ssh launch spec is valid");
    checkEqual(spec.program, QStringLiteral("ssh"), "ssh program");
    check(contains(spec.args, QStringLiteral("-p")) &&
              contains(spec.args, QStringLiteral("2222")),
          "custom port is forwarded");
    check(contains(spec.args, QStringLiteral("-J")), "ProxyJump is forwarded");
    check(spec.args.last() == QStringLiteral("alice@example.com"),
          "destination is the last argument");

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

    Session copy = Session::fromJson(ssh.toJson());
    checkEqual(copy.name, ssh.name, "JSON keeps name");
    checkEqual(copy.host, ssh.host, "JSON keeps host");
    check(copy.port == 2222, "JSON keeps port");
    check(copy.auth == Session::Auth::Agent, "JSON keeps auth method");
    checkEqual(copy.proxyJump, ssh.proxyJump, "JSON keeps proxy jump");
}

void testStore()
{
    std::printf("session store\n");

    QTemporaryDir dir;
    check(dir.isValid(), "temporary root created");
    qterm2::ApplicationContext context(qterm2::AppPaths::forRoot(dir.path()));
    QString error;
    check(context.initialize(&error), "application context initializes");
    if (!error.isEmpty())
        std::printf("       warning: %s\n", qPrintable(error));

    qterm2::SessionStore *store = context.sessionStore();
    check(store != nullptr, "store available");
    if (!store)
        return;

    {
        Session a = Session::makeLocal(QStringLiteral("Uno"));
        a.group = QStringLiteral("Trabajo");
        Session b;
        b.name = QStringLiteral("Dos");
        b.host = QStringLiteral("host.example");
        b.user = QStringLiteral("bob");
        b.group = QStringLiteral("Trabajo");
        b.auth = Session::Auth::Password;

        check(store->addSession(a), "first session added");
        check(store->addSession(b), "second session added");
        check(store->sessions().size() == 2, "two sessions present");
        check(store->groups().contains(QStringLiteral("Trabajo")), "group is listed");
    }

    check(store->load(), "store reloads");
    check(store->sessions().size() == 2, "two sessions reloaded");
    Session reloaded;
    for (const Session &candidate : store->sessions()) {
        if (candidate.name == QStringLiteral("Dos"))
            reloaded = candidate;
    }
    checkEqual(reloaded.host, QStringLiteral("host.example"), "reloaded host");

    check(store->removeSession(reloaded.id), "session removed");
    check(store->load(), "store reloads after removal");
    check(store->sessions().size() == 1, "removal persisted");
}

void testTerminal()
{
    std::printf("terminal\n");

    TerminalWidget terminal;
    terminal.resize(640, 400);
    terminal.setScrollbackLines(1000);

    const bool started = terminal.startProgram(
        QStringLiteral("/bin/sh"),
        {QStringLiteral("-c"),
         QStringLiteral("printf 'hola \\033[31mrojo\\033[0m fin\\n'; "
                        "printf 'línea dos\\n'; "
                        "printf '\\033[4;5Hsalto'")});
    check(started, "terminal starts /bin/sh");

    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    guard.setInterval(8000);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&terminal, &TerminalWidget::terminalFinished, &loop, &QEventLoop::quit);
    guard.start();
    loop.exec();
    QCoreApplication::processEvents();

    const QString text = terminal.screenText();
    if (qEnvironmentVariableIsSet("QTERM2_TEST_DUMP"))
        std::printf("--- screen ---\n%s\n--------------\n", qPrintable(text));
    check(text.contains(QStringLiteral("hola rojo fin")), "UTF-8 text and SGR parsed");
    check(text.contains(QStringLiteral("línea dos")), "second line parsed");
    check(text.contains(QStringLiteral("salto")), "cursor positioning parsed");
    check(!terminal.isRunning(), "terminal reports process exit");
}

void testZsh()
{
    std::printf("zsh (interactive)\n");

    const QString zsh = QStringLiteral("/bin/zsh");
    if (!QFile::exists(zsh)) {
        std::printf("  skip zsh is not installed\n");
        return;
    }

    TerminalWidget terminal;
    terminal.resize(720, 400);
    check(terminal.startShell(zsh, QDir::homePath()), "zsh starts in a PTY");

    terminal.sendText(QStringLiteral("print -r -- 'ZSH·READY-OUT'\n"));

    const QString token = QStringLiteral("ZSH·READY-OUT");
    QElapsedTimer timer;
    timer.start();
    QString text;
    while (timer.elapsed() < 8000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        text = terminal.screenText();
        if (text.count(token) >= 2)
            break;
    }
    check(text.count(token) >= 2, "zsh echoes UTF-8 output");

    terminal.sendText(QStringLiteral("exit\n"));
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    guard.setInterval(5000);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&terminal, &TerminalWidget::terminalFinished, &loop, &QEventLoop::quit);
    guard.start();
    loop.exec();
    check(!terminal.isRunning(), "zsh exits cleanly");
}

void testKeys()
{
    std::printf("ssh keys\n");

    if (QStandardPaths::findExecutable(QStringLiteral("ssh-keygen")).isEmpty()) {
        std::printf("  skip ssh-keygen is not installed\n");
        return;
    }

    QTemporaryDir dir;
    check(dir.isValid(), "temporary key directory created");
    const QString key = dir.filePath(QStringLiteral("id_ed25519_test"));

    QString error;
    check(qterm2::ssh::KeyManager::generateEd25519(key, QStringLiteral("qterm2@test"),
                                                   QString(), &error),
          "ed25519 key generated");
    check(QFile::exists(key), "private key exists");
    check(QFile::exists(key + QStringLiteral(".pub")), "public key exists");

    const QFile::Permissions privatePerms = QFile::permissions(key);
    check(privatePerms.testFlag(QFileDevice::ReadOwner)
              && privatePerms.testFlag(QFileDevice::WriteOwner)
              && !privatePerms.testFlag(QFileDevice::ReadOther),
          "private key is not world readable");

    const QString pub = qterm2::ssh::KeyManager::publicKeyFor(key);
    check(pub.startsWith(QStringLiteral("ssh-ed25519 ")), "public key has expected format");

    error.clear();
    check(!qterm2::ssh::KeyManager::generateEd25519(key, QStringLiteral("again"), QString(), &error),
          "existing key is not overwritten");
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTerm2-test"));
    QApplication::setOrganizationName(QStringLiteral("QTerm2-test"));

    testSessions();
    testStore();
    testKeys();
    testTerminal();
    testZsh();

    if (g_failures == 0)
        std::printf("\nall smoke tests passed\n");
    else
        std::printf("\n%d smoke test(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
