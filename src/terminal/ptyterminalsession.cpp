#include "terminal/ptyterminalsession.h"

namespace qterm2 {

PtyTerminalSession::PtyTerminalSession(QObject *parent)
    : TerminalSession(parent)
    , m_pty(new PtyProcess(this))
{
    connect(m_pty, &PtyProcess::dataReceived,
            this, &TerminalSession::dataReceived);
    connect(m_pty, &PtyProcess::exited, this, [this](int) {
        emit finished();
    });
}

bool PtyTerminalSession::start(const QString &program, const QStringList &args,
                               int cols, int rows, const QString &workDir,
                               const QProcessEnvironment &env)
{
    return m_pty->start(program, args, cols, rows, workDir, env);
}

bool PtyTerminalSession::startShell(const QString &shell, int cols, int rows,
                                    const QString &workDir)
{
    return m_pty->startShell(shell, cols, rows, workDir);
}

void PtyTerminalSession::write(const QByteArray &data)
{
    m_pty->write(data);
}

void PtyTerminalSession::resize(int cols, int rows)
{
    m_pty->resize(cols, rows);
}

void PtyTerminalSession::terminate()
{
    m_pty->terminateProcess();
}

bool PtyTerminalSession::isRunning() const
{
    return m_pty->isRunning();
}

} // namespace qterm2
