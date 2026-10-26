#include "core/processspec.h"

#include "domain/session.h"

namespace qterm2 {

QString ProcessSpec::commandLine() const
{
    if (!ok)
        return QString();
    QString command = commandLineFor(program, args);
    if (!workDir.isEmpty())
        command = QStringLiteral("cd %1 && %2").arg(shellQuote(workDir), command);
    return command;
}

} // namespace qterm2
