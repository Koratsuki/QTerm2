#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include <QByteArray>

#ifdef _WIN32
#include <windows.h>
typedef unsigned long pid_t;
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

    bool isRunning() const { return m_fd >= 0; }
    qint64 pid() const { return m_pid; }
    bool isForegroundProcess() const { return m_fd >= 0; }

signals:
    void dataReceived(const QByteArray &data);
    void exited(int status);

private slots:
    void onActivated();

private:
    void closePty();
    void reapChild();

#ifdef _WIN32
    HANDLE m_hProcess = nullptr;
    HANDLE m_hPipeRead = nullptr;
    HANDLE m_hPipeWrite = nullptr;
    int m_fd = -1;
    pid_t m_pid = 0;
#else
    int m_fd = -1;
    pid_t m_pid = -1;
#endif
    QSocketNotifier *m_notifier = nullptr;
};
