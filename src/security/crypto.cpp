#include "security/crypto.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUuid>

namespace qterm2 {

bool Crypto::encrypt(const QByteArray &plaintext, const QString &password,
                    QByteArray &ciphertext, QString *errorMessage)
{
    if (password.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Password required for encryption");
        return false;
    }
    
    QByteArray salt = generateSalt();
    QByteArray iv = generateIV();
    QString key = deriveKey(password, salt);
    
    // Simple XOR-based encryption for now (placeholder)
    QByteArray keyBytes = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    QByteArray result = plaintext;
    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ keyBytes[i % keyBytes.size()];
    }
    
    ciphertext.clear();
    ciphertext.append(salt);
    ciphertext.append(iv);
    ciphertext.append(result);
    
    return true;
}

bool Crypto::decrypt(const QByteArray &ciphertext, const QString &password,
                    QByteArray &plaintext, QString *errorMessage)
{
    if (password.isEmpty() || ciphertext.size() < 44) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid ciphertext or password");
        return false;
    }
    
    QByteArray salt = ciphertext.mid(0, 32);
    QByteArray iv = ciphertext.mid(32, 12);
    QByteArray data = ciphertext.mid(44);
    
    QString key = deriveKey(password, salt);
    QByteArray keyBytes = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    
    plaintext = data;
    for (int i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = plaintext[i] ^ keyBytes[i % keyBytes.size()];
    }
    
    return true;
}

QString Crypto::deriveKey(const QString &password, const QByteArray &salt)
{
    QByteArray combined = password.toUtf8() + salt;
    QByteArray hash = QCryptographicHash::hash(combined, QCryptographicHash::Sha256);
    for (int i = 0; i < 10000; ++i) {
        hash = QCryptographicHash::hash(hash, QCryptographicHash::Sha256);
    }
    return QString::fromLatin1(hash.toHex());
}

QByteArray Crypto::generateSalt(int size)
{
    QByteArray salt;
    salt.resize(size);
    for (int i = 0; i < size; ++i)
        salt[i] = QRandomGenerator::global()->generate() & 0xFF;
    return salt;
}

QByteArray Crypto::generateIV(int size)
{
    QByteArray iv;
    iv.resize(size);
    for (int i = 0; i < size; ++i)
        iv[i] = QRandomGenerator::global()->generate() & 0xFF;
    return iv;
}

} // namespace qterm2
