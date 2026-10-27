#include "terminal/ptyprocess.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <QSocketNotifier>
#include <QVector>

PtyProcess::PtyProcess(QObject *parent) : QObject(parent)
{
    // A remote process closing its side must never terminate QTerm2.
    ::signal(SIGPIPE, SIG_IGN);
}

PtyProcess::~PtyProcess()
{
    terminateProcess();
}

bool PtyProcess::start(const QString &program, const QStringList &args,
                       int cols, int rows, const QString &workDir,
                       const QProcessEnvironment &env)
{
    closePty();

    if (program.trimmed().isEmpty())
        return false;

    // Everything the child needs is converted to raw C data *before* fork().
    // Only async-signal-safe POSIX calls run between fork() and exec().
    const QByteArray dirBytes = workDir.toLocal8Bit();

    const QStringList envKeys = env.keys();
    QVector<QByteArray> envStorage;
    envStorage.reserve(envKeys.size());
    for (const QString &key : envKeys)
        envStorage.append(key.toLocal8Bit() + '=' + env.value(key).toLocal8Bit());

    QVector<QByteArray> argStorage;
    argStorage.reserve(args.size() + 1);
    argStorage.append(program.toLocal8Bit());
    for (const QString &arg : args)
        argStorage.append(arg.toLocal8Bit());

    QVector<char *> argPtrs;
    argPtrs.reserve(argStorage.size() + 1);
    for (QByteArray &arg : argStorage)
        argPtrs.append(arg.data());
    argPtrs.append(nullptr);

    QVector<char *> envPtrs;
    envPtrs.reserve(envStorage.size());
    for (QByteArray &entry : envStorage)
        envPtrs.append(entry.data());

    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_row = static_cast<unsigned short>(qMax(1, rows));
    ws.ws_col = static_cast<unsigned short>(qMax(1, cols));

    int fd = -1;
    const pid_t pid = forkpty(&fd, nullptr, nullptr, &ws);
    if (pid < 0)
        return false;

    if (pid == 0) {
        // ------------------------------------------------------------ child
        if (!dirBytes.isEmpty())
            (void)chdir(dirBytes.constData()); // best effort

        for (int i = 0; i < envPtrs.size(); ++i)
            putenv(envPtrs.at(i));

        execvp(argPtrs.at(0), argPtrs.data());
        _exit(127);
    }

    // --------------------------------------------------------------- parent
    m_fd = fd;
    m_pid = pid;
    fcntl(m_fd, F_SETFL, O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &PtyProcess::onActivated);
    return true;
}

bool PtyProcess::startShell(const QString &shell, int cols, int rows,
                            const QString &workDir)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("QTerm2"));
    env.insert(QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("1.0"));

    // A UTF-8 locale is required for zsh/readline to handle wide glyphs.
    const QString locale = env.value(QStringLiteral("LANG"));
    if (!locale.contains(QStringLiteral("UTF-8"), Qt::CaseInsensitive)
        && !locale.contains(QStringLiteral("utf8"), Qt::CaseInsensitive)) {
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    }

    QString actual = shell;
    if (actual.isEmpty())
        actual = QStringLiteral("/bin/zsh");
    return start(actual, QStringList(), cols, rows, workDir, env);
}

void PtyProcess::write(const QByteArray &data)
{
    if (m_fd < 0 || data.isEmpty())
        return;

    const char *p = data.constData();
    qsizetype remaining = data.size();
    while (remaining > 0) {
        const ssize_t n = ::write(m_fd, p, size_t(remaining));
        if (n > 0) {
            p += n;
            remaining -= n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // The PTY input buffer is full. Wait briefly for the child to
            // consume it instead of silently dropping keystrokes or paste.
            struct pollfd pfd;
            pfd.fd = m_fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;
            if (::poll(&pfd, 1, 1000) > 0)
                continue;
        }
        break; // broken PTY or the peer stopped reading
    }
}

void PtyProcess::resize(int cols, int rows)
{
    if (m_fd < 0)
        return;
    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_row = static_cast<unsigned short>(qMax(1, rows));
    ws.ws_col = static_cast<unsigned short>(qMax(1, cols));
    if (ioctl(m_fd, TIOCSWINSZ, &ws) == 0 && m_pid > 0)
        ::kill(m_pid, SIGWINCH);
}

void PtyProcess::onActivated()
{
    char buf[16384];
    for (;;) {
        const ssize_t n = ::read(m_fd, buf, sizeof(buf));
        if (n > 0) {
            emit dataReceived(QByteArray(buf, int(n)));
            continue;
        }
        if (n == 0) {
            reapChild();
            closePty();
            emit exited(0);
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        // EIO is the normal "the slave side closed" notification on Linux.
        reapChild();
        closePty();
        emit exited(-1);
        return;
    }
}

void PtyProcess::terminateProcess()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_pid > 0)
        ::kill(-m_pid, SIGHUP);
    reapChild();
    closePty();
}

void PtyProcess::reapChild()
{
    if (m_pid > 0) {
        int status = 0;
        for (int i = 0; i < 10; ++i) {
            const pid_t r = waitpid(m_pid, &status, WNOHANG);
            if (r == m_pid || r == -1)
                break;
            ::kill(-m_pid, SIGKILL);
            ::usleep(20000);
        }
        m_pid = -1;
    }
}

void PtyProcess::closePty()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}
