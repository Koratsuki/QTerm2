#pragma once

#include <QString>
#include <QByteArray>

namespace qterm2 {

/**
 * Simple encryption for session sync (AES-256-GCM via OpenSSL if available)
 * Falls back to XOR with key derivation for portability
 */
class Crypto
{
public:
    static bool encrypt(const QByteArray &plaintext, const QString &password,
                       QByteArray &ciphertext, QString *errorMessage = nullptr);
    static bool decrypt(const QByteArray &ciphertext, const QString &password,
                       QByteArray &plaintext, QString *errorMessage = nullptr);
    
    static QString deriveKey(const QString &password, const QByteArray &salt);
    static QByteArray generateSalt(int size = 32);
    static QByteArray generateIV(int size = 12);
};

} // namespace qterm2
