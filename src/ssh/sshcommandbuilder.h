#pragma once

#include <QString>

#include "core/processspec.h"
#include "domain/session.h"

namespace qterm2::ssh {

/**
 * Translates a session profile into an OpenSSH command line.
 *
 * The SSH subsystem never stores or handles passwords: password and
 * keyboard-interactive authentication are delegated to the system OpenSSH
 * client running inside a PTY.
 */
ProcessSpec buildConnectSpec(const Session &session);
ProcessSpec buildInstallKeySpec(const Session &session);

QString expandPath(const QString &path);

} // namespace qterm2::ssh
