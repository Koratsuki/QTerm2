#pragma once

#include <QString>

namespace qterm2 {

/** Small status type used by services that do not need exceptions. */
struct Status
{
    bool ok = true;
    QString message;

    static Status success() { return {}; }
    static Status failure(const QString &message)
    {
        Status status;
        status.ok = false;
        status.message = message;
        return status;
    }

    explicit operator bool() const { return ok; }
};

} // namespace qterm2
