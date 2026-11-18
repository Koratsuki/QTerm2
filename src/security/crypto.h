#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace qterm2 {

/**
 * Authenticated encryption for exported session files.
 *
 * Format v1 (integers are big-endian):
 *
 *   offset  size  field
 *   0       4     magic "Q2SE"
 *   4       1     format version (1)
 *   5       4     PBKDF2 iteration count
 *   9       16    salt
 *   25      12    nonce
 *   37      n     AES-256-GCM ciphertext (same length as the plaintext)
 *   37+n    16    GCM authentication tag
 *
 * The 37-byte header is fed to GCM as additional authenticated data, so
 * tampering with the version, the iteration count, the salt or the nonce
 * invalidates the tag instead of silently changing the parameters.
 *
 * The key is PBKDF2-HMAC-SHA256(password UTF-8, salt, iterations, 32 bytes).
 * The iteration count is bounded on both sides: a tiny value would weaken the
 * key derivation, a huge one would let a malicious file stall decryption.
 *
 * Encryption requires OpenSSL. There is deliberately NO weaker fallback: if
 * the build lacks OpenSSL, isAvailable() returns false and encrypt() fails
 * with an explanatory error instead of pretending to protect the data.
 */
class Crypto
{
public:
    static constexpr quint32 kDefaultIterations = 210000;
    static constexpr quint32 kMinIterations = 10000;
    // Availability bound for *untrusted* files: the iteration count is read
    // from the (not yet authenticated) header, so a hostile export can make us
    // derive a key before the GCM tag can reject it.  1M iterations is still
    // a strong KDF for our own exports (210k default) but caps that CPU cost.
    static constexpr quint32 kMaxIterations = 1000000;
    static constexpr int kSaltBytes = 16;
    static constexpr int kNonceBytes = 12;
    static constexpr int kTagBytes = 16;
    static constexpr int kKeyBytes = 32;
    static constexpr int kHeaderBytes = 37;
    /** Every encrypted blob is at least this large. */
    static constexpr int kMinBlobBytes = kHeaderBytes + kTagBytes;

    /** True when the build provides AES-256-GCM (OpenSSL was found). */
    static bool isAvailable();

    static bool encrypt(const QByteArray &plaintext, const QString &password,
                        QByteArray &ciphertext, QString *errorMessage = nullptr);
    static bool decrypt(const QByteArray &ciphertext, const QString &password,
                        QByteArray &plaintext, QString *errorMessage = nullptr);

    /** PBKDF2-HMAC-SHA256 helper. Returns an empty array on failure. */
    static QByteArray deriveKey(const QByteArray &password, const QByteArray &salt,
                                quint32 iterations, QString *errorMessage = nullptr);

    /** Cryptographically secure random bytes (OS CSPRNG). */
    static QByteArray randomBytes(int size);

private:
    static QByteArray buildHeader(quint32 iterations, const QByteArray &salt,
                                  const QByteArray &nonce);
};

} // namespace qterm2
