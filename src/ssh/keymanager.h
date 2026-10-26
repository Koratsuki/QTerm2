#pragma once

#include <QString>
#include <QStringList>

namespace qterm2::ssh {

/**
 * Thin, careful wrapper around OpenSSH's own key tooling.  QTerm2 never
 * handles private key material itself; ssh-keygen and ssh do that.
 */
namespace KeyManager {

QString sshDirectory();
QString expandPath(const QString &path);
QStringList listPrivateKeys();
QString publicKeyFor(const QString &privateKeyPath);
bool generateEd25519(const QString &privateKeyPath, const QString &comment,
                     const QString &passphrase, QString *errorMessage);
QString suggestedKeyPath(const QString &user, const QString &host);
bool ensureSshDirectory(QString *errorMessage);

} // namespace KeyManager

} // namespace qterm2::ssh
