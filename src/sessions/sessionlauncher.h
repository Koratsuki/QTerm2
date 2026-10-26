#pragma once

#include "core/processspec.h"
#include "domain/session.h"

namespace qterm2::sessions {

/**
 * Resolves a persisted session into a concrete process to run in a PTY.
 *
 * Keeping this out of the Session value object prevents the domain model from
 * depending on OpenSSH, process environments or the local filesystem.
 */
class SessionLauncher
{
public:
    static ProcessSpec specFor(const Session &session);
    static QString commandLineFor(const Session &session);
};

} // namespace qterm2::sessions
