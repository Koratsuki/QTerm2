#include "terminal/ptyprocess.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
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
#include <QTimer>

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
    argPtrs.resize(argStorage.size() + 1);
    for (int i = 0; i < argStorage.size(); ++i)
        argPtrs[i] = argStorage[i].data();
    argPtrs.back() = nullptr;

    QVector<char *> envPtrs;
    envPtrs.resize(envStorage.size() + 1);
    for (int i = 0; i < envStorage.size(); ++i)
        envPtrs[i] = envStorage[i].data();
    envPtrs.back() = nullptr;

    int master = -1;
    pid_t pid = ::forkpty(&master, nullptr, nullptr, nullptr);
    if (pid < 0)
        return false;
    if (pid == 0) {
        // Child
        if (!dirBytes.isEmpty())
            ::chdir(dirBytes.constData());

        if (envPtrs.back() == nullptr && !envStorage.isEmpty()) {
            for (char **ep = envPtrs.data(); *ep != nullptr; ++ep)
                ::putenv(*ep);
        } else if (envPtrs.back() == nullptr && envStorage.isEmpty()) {
            // env list built; skip
        }

        ::setenv("TERM", "xterm-256color", 1);
        ::execvp(argPtrs[0], argPtrs.data());
        ::_exit(127);
    }

    // Parent
    m_pid = pid;
    m_fd = master;
    ::fcntl(m_fd, F_SETFL, ::fcntl(m_fd, F_GETFL, 0) | O_NONBLOCK);

    struct winsize ws;
    ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
    ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ::ioctl(m_fd, TIOCSWINSZ, &ws);

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtyProcess::onActivated);
    return true;
}

bool PtyProcess::startShell(const QString &shell, int cols, int rows, const QString &workDir)
{
    QString sh = shell.trimmed();
    if (sh.isEmpty()) {
        const QByteArray envShell = ::getenv("SHELL");
        if (envShell && envShell[0])
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
    ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
    ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ::ioctl(m_fd, TIOCSWINSZ, &ws);
}

void PtyProcess::terminateProcess()
{
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
        ::kill(m_pid, SIGHUP);
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
        emit exited(status);
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
            break;
        }
        if (n == 0)
            break;
        emit dataReceived(QByteArray(buf, static_cast<int>(n)));
    }
    reapChild();
}

#else // Windows implementation

#include <QSocketNotifier>
#include <QTimer>

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
    closePty();

    if (program.trimmed().isEmpty())
        return false;

    QString cmd = program;
    for (const QString &arg : args) {
        cmd += " ";
        if (arg.contains(' ') || arg.contains('"'))
            cmd += "\"" + arg + "\"";
        else
            cmd += arg;
    }

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hPipeRead, &m_hPipeWrite, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(m_hPipeRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = m_hPipeWrite;
    siStartInfo.hStdOutput = m_hPipeWrite;
    siStartInfo.hStdInput = m_hPipeRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    std::wstring cmdW = (L"cmd.exe /c " + cmd).toStdWString();
    std::wstring dirW = workDir.toStdWString();

    BOOL bSuccess = CreateProcessW(NULL,
        &cmdW[0],
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        dirW.empty() ? NULL : dirW.c_str(),
        &siStartInfo,
        &piProcInfo);

    if (!bSuccess) {
        closePty();
        return false;
    }

    m_hProcess = piProcInfo.hProcess;
    m_pid = piProcInfo.dwProcessId;
    CloseHandle(piProcInfo.hThread);
    CloseHandle(m_hPipeWrite);
    m_hPipeWrite = nullptr;

    m_notifier = new QSocketNotifier((int)(uintptr_t)m_hPipeRead, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtyProcess::onActivated);
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
    WriteFile(m_hPipeWrite, data.constData(), data.size(), &written, NULL);
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
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
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
    DWORD available = 0;
    if (!PeekNamedPipe(m_hPipeRead, NULL, 0, NULL, &available, NULL) || available == 0)
        return;
    DWORD bytesRead = 0;
    if (ReadFile(m_hPipeRead, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0) {
        emit dataReceived(QByteArray(buf, (int)bytesRead));
    }
    reapChild();
}

#endif
