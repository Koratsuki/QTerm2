#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
typedef DWORD pid_t;
#else
#include <sys/types.h>
#endif

class QSocketNotifier;

/**
 * A child process attached to a pseudo-terminal.
 *
 * QTerm2 always talks to a PTY (never to plain pipes): SSH, zsh and every
 * full-screen program (vim, htop, tmux...) need a real terminal to behave
 * the same way they do in iTerm2 or a physical console.
 */
class PtyProcess : public QObject
{
    Q_OBJECT
public:
    explicit PtyProcess(QObject *parent = nullptr);
    ~PtyProcess() override;

    bool start(const QString &program, const QStringList &args,
               int cols, int rows, const QString &workDir,
               const QProcessEnvironment &env);

    bool startShell(const QString &shell, int cols, int rows,
                    const QString &workDir);

    void write(const QByteArray &data);
    void resize(int cols, int rows);
    void terminateProcess();

    bool isRunning() const {
#ifndef _WIN32
        return m_fd >= 0;
#else
        return m_hProcess != nullptr;
#endif
    }
    qint64 pid() const { return static_cast<qint64>(m_pid); }
    bool isForegroundProcess() const {
#ifndef _WIN32
        return m_fd >= 0;
#else
        return m_hProcess != nullptr;
#endif
    }

signals:
    void dataReceived(const QByteArray &data);
    void exited(int status);

private slots:
    void onActivated();

private:
    void closePty();
    void reapChild();

#ifndef _WIN32
    int m_fd = -1;
    pid_t m_pid = -1;
    QSocketNotifier *m_notifier = nullptr;
#else
    HANDLE m_hProcess = nullptr;
    HANDLE m_hPipeRead = nullptr;
    HANDLE m_hPipeWrite = nullptr;
    pid_t m_pid = 0;
    QSocketNotifier *m_notifier = nullptr;
#endif
};
