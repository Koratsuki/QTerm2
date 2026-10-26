#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace qterm2 {

/**
 * Byte transport between a terminal view and a running program.
 *
 * The default implementation is a local pseudo-terminal.  Future backends
 * (libssh channels, serial consoles, plugin-provided containers) only have to
 * implement this interface; the emulator and the view remain unchanged.
 */
class TerminalSession : public QObject
{
    Q_OBJECT
public:
    explicit TerminalSession(QObject *parent = nullptr) : QObject(parent) {}
    ~TerminalSession() override = default;

    virtual bool start(const QString &program, const QStringList &args,
                       int cols, int rows, const QString &workDir,
                       const QProcessEnvironment &env) = 0;
    virtual bool startShell(const QString &shell, int cols, int rows,
                            const QString &workDir) = 0;
    virtual void write(const QByteArray &data) = 0;
    virtual void resize(int cols, int rows) = 0;
    virtual void terminate() = 0;
    virtual bool isRunning() const = 0;

signals:
    void dataReceived(const QByteArray &data);
    void finished();
};

} // namespace qterm2
