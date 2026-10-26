#pragma once

#include "terminal/ptyprocess.h"
#include "terminal/terminalsession.h"

namespace qterm2 {

/** Local pseudo-terminal transport backed by forkpty(). */
class PtyTerminalSession final : public TerminalSession
{
    Q_OBJECT
public:
    explicit PtyTerminalSession(QObject *parent = nullptr);

    bool start(const QString &program, const QStringList &args,
               int cols, int rows, const QString &workDir,
               const QProcessEnvironment &env) override;
    bool startShell(const QString &shell, int cols, int rows,
                    const QString &workDir) override;
    void write(const QByteArray &data) override;
    void resize(int cols, int rows) override;
    void terminate() override;
    bool isRunning() const override;

private:
    PtyProcess *m_pty = nullptr;
};

} // namespace qterm2
