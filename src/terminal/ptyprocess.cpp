#include "terminal/ptyprocess.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <QSocketNotifier>
#include <QTimer>
#include <QProcessEnvironment>
#include <windows.h>

PtyProcess::PtyProcess(QObject *parent) : QObject(parent)
{
}

PtyProcess::~PtyProcess()
{
    terminateProcess();
}

bool PtyProcess::start(const QString &program, const QStringList &args,
                       int cols, int rows, const QString &workDir,
                       const QProcessEnvironment &env)
{
    Q_UNUSED(cols);
    Q_UNUSED(rows);
    Q_UNUSED(env);
    closePty();

    if (program.trimmed().isEmpty())
        return false;

    QString cmd = program;
    for (const QString &arg : args) {
        cmd += " ";
        if (arg.contains(' ') || arg.contains('"'))
            cmd += "\"" + arg;
        else
            cmd += arg;
    }

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hStdInR, hStdInW, hStdOutR, hStdOutW;
    if (!CreatePipe(&hStdOutR, &hStdOutW, &saAttr, 0))
        return false;
    if (!SetHandleInformation(hStdOutR, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hStdOutR);
        CloseHandle(hStdOutW);
        return false;
    }
    if (!CreatePipe(&hStdInR, &hStdInW, &saAttr, 0)) {
        CloseHandle(hStdOutR);
        CloseHandle(hStdOutW);
        return false;
    }
    if (!SetHandleInformation(hStdInW, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hStdOutR);
        CloseHandle(hStdOutW);
        CloseHandle(hStdInR);
        CloseHandle(hStdInW);
        return false;
    }

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hStdOutW;
    siStartInfo.hStdOutput = hStdOutW;
    siStartInfo.hStdInput = hStdInR;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    std::wstring cmdW = cmd.toStdWString();
    std::wstring dirW = workDir.toStdWString();

    LPWSTR cmdLine = cmdW.empty() ? NULL : &cmdW[0];
    LPCWSTR dirLine = dirW.empty() ? NULL : dirW.c_str();

    BOOL bSuccess = CreateProcessW(NULL,
        cmdLine,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        dirLine,
        &siStartInfo,
        &piProcInfo);

    if (!bSuccess) {
        CloseHandle(hStdOutR);
        CloseHandle(hStdOutW);
        CloseHandle(hStdInR);
        CloseHandle(hStdInW);
        return false;
    }

    m_pid = piProcInfo.dwProcessId;
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hStdOutW);
    CloseHandle(hStdInR);
    m_hProcess = piProcInfo.hProcess;
    m_hPipeRead = hStdOutR;
    m_hPipeWrite = hStdInW;
    m_fd = -1;
    m_notifier = nullptr;

    return true;
}

bool PtyProcess::startShell(const QString &shell, int cols, int rows, const QString &workDir)
{
    QString sh = shell.trimmed();
    if (sh.isEmpty())
        sh = "cmd.exe";
    return start(sh, QStringList(), cols, rows, workDir, QProcessEnvironment::systemEnvironment());
}

void PtyProcess::write(const QByteArray &data)
{
    if (!m_hPipeWrite || data.isEmpty())
        return;
    DWORD written = 0;
    WriteFile(m_hPipeWrite, data.constData(), (DWORD)data.size(), &written, NULL);
}

void PtyProcess::resize(int cols, int rows)
{
    Q_UNUSED(cols);
    Q_UNUSED(rows);
}

void PtyProcess::terminateProcess()
{
    closePty();
}

void PtyProcess::closePty()
{
    if (m_hPipeRead) {
        CloseHandle(m_hPipeRead);
        m_hPipeRead = nullptr;
    }
    if (m_hPipeWrite) {
        CloseHandle(m_hPipeWrite);
        m_hPipeWrite = nullptr;
    }
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_pid = 0;
    m_fd = -1;
}

void PtyProcess::reapChild()
{
    if (!m_hProcess)
        return;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hProcess, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            emit exited((int)exitCode);
            closePty();
        }
    }
}

void PtyProcess::onActivated()
{
    if (!m_hPipeRead)
        return;
    char buf[4096];
    DWORD bytesRead = 0;
    if (ReadFile(m_hPipeRead, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0) {
        emit dataReceived(QByteArray(buf, (int)bytesRead));
    }
    reapChild();
}

#else // Unix implementation

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

        // Replace environment if we built one.
        if (!envPtrs.isEmpty()) {
            // putenv takes ownership of the strings we appended; they live in envStorage.
            for (char *ep : envPtrs)
                putenv(ep);
        }

        // Reasonable defaults for an interactive shell.
        setenv("TERM", "xterm-256color", 1);

        execvp(argPtrs[0], argPtrs.data());
        _exit(127);
    }

    // ------------------------------------------------------------ parent
    m_pid = pid;
    m_fd = fd;
    // Make non-blocking for QSocketNotifier/read().
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtyProcess::onActivated);
    return true;
}

bool PtyProcess::startShell(const QString &shell, int cols, int rows, const QString &workDir)
{
    QString sh = shell.trimmed();
    if (sh.isEmpty()) {
        const QByteArray envShell = ::getenv("SHELL");
        if (!envShell.isEmpty())
            sh = QString::fromLocal8Bit(envShell);
        else
            sh = QStringLiteral("/bin/bash");
    }
    return start(sh, QStringList(), cols, rows, workDir, QProcessEnvironment::systemEnvironment());
}

void PtyProcess::write(const QByteArray &data)
{
    if (m_fd < 0 || data.isEmpty())
        return;
    const char *ptr = data.constData();
    qint64 left = data.size();
    while (left > 0) {
        ssize_t n = ::write(m_fd, ptr, static_cast<size_t>(left));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        ptr += n;
        left -= n;
    }
}

void PtyProcess::resize(int cols, int rows)
{
    if (m_fd < 0)
        return;
    struct winsize ws;
    ws.ws_col = static_cast<unsigned short>(qMax(1, cols));
    ws.ws_row = static_cast<unsigned short>(qMax(1, rows));
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ::ioctl(m_fd, TIOCSWINSZ, &ws);
}

void PtyProcess::terminateProcess()
{
    if (m_pid > 0) {
        ::kill(m_pid, SIGHUP);
    }
    closePty();
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
    if (m_pid > 0) {
        int status = 0;
        ::waitpid(m_pid, &status, WNOHANG);
        m_pid = -1;
    }
}

void PtyProcess::reapChild()
{
    if (m_pid <= 0)
        return;
    int status = 0;
    pid_t r = ::waitpid(m_pid, &status, WNOHANG);
    if (r == m_pid) {
        m_pid = -1;
        emit exited(WIFEXITED(status) ? WEXITSTATUS(status) : status);
        closePty();
    }
}

void PtyProcess::onActivated()
{
    if (m_fd < 0)
        return;
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(m_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            // error
            break;
        }
        if (n == 0)
            break;
        emit dataReceived(QByteArray(buf, static_cast<int>(n)));
    }
    reapChild();
}

#endif
