#include "security/crypto.h"

#include <QRandomGenerator>
#include <QtEndian>

#include <cstring>
#include <memory>

#ifdef QTERM2_HAVE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/evp.h>
#endif

namespace qterm2 {

namespace {

constexpr char kMagic[4] = {'Q', '2', 'S', 'E'};
constexpr quint8 kFormatVersion = 1;

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

} // namespace

bool Crypto::isAvailable()
{
#ifdef QTERM2_HAVE_OPENSSL
    return true;
#else
    return false;
#endif
}

QByteArray Crypto::randomBytes(int size)
{
    if (size <= 0)
        return {};
    QByteArray bytes(size, Qt::Uninitialized);
    QRandomGenerator *rng = QRandomGenerator::system();
    for (int i = 0; i < size; ++i)
        bytes[i] = static_cast<char>(rng->generate() & 0xFF);
    return bytes;
}

QByteArray Crypto::deriveKey(const QByteArray &password, const QByteArray &salt,
                             quint32 iterations, QString *errorMessage)
{
    if (password.isEmpty()) {
        setError(errorMessage, QStringLiteral("Se requiere una contraseña."));
        return {};
    }
    if (static_cast<int>(salt.size()) < 8) {
        setError(errorMessage, QStringLiteral("La sal es demasiado corta."));
        return {};
    }
    if (iterations < kMinIterations || iterations > kMaxIterations) {
        setError(errorMessage,
                 QStringLiteral("El número de iteraciones (%1) está fuera del rango "
                                "aceptado (%2-%3).")
                     .arg(iterations)
                     .arg(kMinIterations)
                     .arg(kMaxIterations));
        return {};
    }

#ifdef QTERM2_HAVE_OPENSSL
    QByteArray key(kKeyBytes, Qt::Uninitialized);
    const int rc = PKCS5_PBKDF2_HMAC(
        password.constData(), password.size(),
        reinterpret_cast<const unsigned char *>(salt.constData()), salt.size(),
        static_cast<int>(iterations), EVP_sha256(), kKeyBytes,
        reinterpret_cast<unsigned char *>(key.data()));
    if (rc != 1) {
        setError(errorMessage,
                 QStringLiteral("No se pudo derivar la clave (PBKDF2 falló)."));
        return {};
    }
    return key;
#else
    Q_UNUSED(salt);
    setError(errorMessage,
             QStringLiteral("Esta compilación de QTerm2 no incluye OpenSSL, así que no "
                            "puede cifrar ni descifrar sesiones."));
    return {};
#endif
}

QByteArray Crypto::buildHeader(quint32 iterations, const QByteArray &salt,
                               const QByteArray &nonce)
{
    QByteArray header;
    header.reserve(kHeaderBytes);
    header.append(kMagic, 4);
    header.append(static_cast<char>(kFormatVersion));

    quint32 beIterations = qToBigEndian(iterations);
    header.append(reinterpret_cast<const char *>(&beIterations), 4);
    header.append(salt);
    header.append(nonce);
    return header;
}

bool Crypto::encrypt(const QByteArray &plaintext, const QString &password,
                     QByteArray &ciphertext, QString *errorMessage)
{
    ciphertext.clear();

    if (password.isEmpty()) {
        setError(errorMessage, QStringLiteral("Se requiere una contraseña."));
        return false;
    }

#ifdef QTERM2_HAVE_OPENSSL
    const quint32 iterations = kDefaultIterations;
    QByteArray salt = randomBytes(kSaltBytes);
    QByteArray nonce = randomBytes(kNonceBytes);
    if (salt.size() != kSaltBytes || nonce.size() != kNonceBytes) {
        setError(errorMessage, QStringLiteral("No se pudo generar la sal o el nonce."));
        return false;
    }

    QString keyError;
    QByteArray passwordBytes = password.toUtf8();
    QByteArray key = deriveKey(passwordBytes, salt, iterations, &keyError);
    OPENSSL_cleanse(passwordBytes.data(), static_cast<size_t>(passwordBytes.size()));
    passwordBytes.clear();
    if (key.isEmpty()) {
        setError(errorMessage, keyError);
        return false;
    }

    const QByteArray header = buildHeader(iterations, salt, nonce);
    QByteArray body(plaintext.size() + kTagBytes, Qt::Uninitialized);
    QByteArray tag(kTagBytes, Qt::Uninitialized);

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
        EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    bool ok = ctx != nullptr
              && EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
              && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) == 1
              && EVP_EncryptInit_ex(
                     ctx.get(), nullptr, nullptr,
                     reinterpret_cast<const unsigned char *>(key.constData()),
                     reinterpret_cast<const unsigned char *>(nonce.constData())) == 1;

    int produced = 0;
    int total = 0;
    if (ok) {
        ok = EVP_EncryptUpdate(ctx.get(), nullptr, &produced,
                               reinterpret_cast<const unsigned char *>(header.constData()),
                               header.size()) == 1;
    }
    if (ok) {
        ok = EVP_EncryptUpdate(
                 ctx.get(), reinterpret_cast<unsigned char *>(body.data()), &produced,
                 reinterpret_cast<const unsigned char *>(plaintext.constData()),
                 plaintext.size()) == 1;
        total = produced;
    }
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx.get(),
                                 reinterpret_cast<unsigned char *>(body.data()) + total,
                                 &produced) == 1;
        total += produced;
    }
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kTagBytes,
                                 tag.data()) == 1;
    }

    OPENSSL_cleanse(key.data(), static_cast<size_t>(key.size()));
    key.clear();

    if (!ok) {
        setError(errorMessage,
                 QStringLiteral("AES-256-GCM falló al cifrar los datos."));
        return false;
    }

    body.resize(total);
    ciphertext = header + body + tag;
    return true;
#else
    Q_UNUSED(plaintext);
    setError(errorMessage,
             QStringLiteral("Esta compilación de QTerm2 no incluye OpenSSL, así que no "
                            "puede cifrar sesiones. Instala OpenSSL y recompila."));
    return false;
#endif
}

bool Crypto::decrypt(const QByteArray &ciphertext, const QString &password,
                     QByteArray &plaintext, QString *errorMessage)
{
    plaintext.clear();

    if (password.isEmpty()) {
        setError(errorMessage, QStringLiteral("Se requiere una contraseña."));
        return false;
    }
    if (ciphertext.size() < kMinBlobBytes) {
        setError(errorMessage,
                 QStringLiteral("El archivo no tiene el formato de exportación cifrada "
                                "de QTerm2."));
        return false;
    }
    if (std::memcmp(ciphertext.constData(), kMagic, sizeof(kMagic)) != 0) {
        setError(errorMessage,
                 QStringLiteral("El archivo no está cifrado por QTerm2 (cabecera "
                                "desconocida)."));
        return false;
    }

    const quint8 version = static_cast<quint8>(ciphertext.at(4));
    if (version != kFormatVersion) {
        setError(errorMessage,
                 QStringLiteral("Versión de formato cifrado no soportada (%1).")
                     .arg(version));
        return false;
    }

    quint32 beIterations = 0;
    std::memcpy(&beIterations, ciphertext.constData() + 5, sizeof(beIterations));
    const quint32 iterations = qFromBigEndian(beIterations);
    if (iterations < kMinIterations || iterations > kMaxIterations) {
        setError(errorMessage,
                 QStringLiteral("El archivo declara un número de iteraciones no válido."));
        return false;
    }

    const QByteArray header = ciphertext.left(kHeaderBytes);
    const QByteArray salt = ciphertext.mid(9, kSaltBytes);
    const QByteArray nonce = ciphertext.mid(9 + kSaltBytes, kNonceBytes);
    const int bodySize = ciphertext.size() - kHeaderBytes - kTagBytes;
    const QByteArray body = ciphertext.mid(kHeaderBytes, bodySize);
    const QByteArray tag = ciphertext.right(kTagBytes);

#ifdef QTERM2_HAVE_OPENSSL
    QString keyError;
    QByteArray passwordBytes = password.toUtf8();
    QByteArray key = deriveKey(passwordBytes, salt, iterations, &keyError);
    OPENSSL_cleanse(passwordBytes.data(), static_cast<size_t>(passwordBytes.size()));
    passwordBytes.clear();
    if (key.isEmpty()) {
        setError(errorMessage, keyError);
        return false;
    }

    QByteArray out(bodySize, Qt::Uninitialized);
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
        EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    bool ok = ctx != nullptr
              && EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
              && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) == 1
              && EVP_DecryptInit_ex(
                     ctx.get(), nullptr, nullptr,
                     reinterpret_cast<const unsigned char *>(key.constData()),
                     reinterpret_cast<const unsigned char *>(nonce.constData())) == 1;

    int produced = 0;
    int total = 0;
    if (ok) {
        ok = EVP_DecryptUpdate(ctx.get(), nullptr, &produced,
                               reinterpret_cast<const unsigned char *>(header.constData()),
                               header.size()) == 1;
    }
    if (ok) {
        ok = EVP_DecryptUpdate(ctx.get(),
                               reinterpret_cast<unsigned char *>(out.data()), &produced,
                               reinterpret_cast<const unsigned char *>(body.constData()),
                               body.size()) == 1;
        total = produced;
    }
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, kTagBytes,
                                 const_cast<char *>(tag.constData())) == 1;
    }
    if (ok) {
        ok = EVP_DecryptFinal_ex(ctx.get(),
                                 reinterpret_cast<unsigned char *>(out.data()) + total,
                                 &produced) == 1;
        total += produced;
    }

    OPENSSL_cleanse(key.data(), static_cast<size_t>(key.size()));
    key.clear();

    if (!ok) {
        setError(errorMessage,
                 QStringLiteral("No se pudo descifrar: contraseña incorrecta o archivo "
                                "alterado."));
        return false;
    }

    out.resize(total);
    plaintext = out;
    return true;
#else
    setError(errorMessage,
             QStringLiteral("Esta compilación de QTerm2 no incluye OpenSSL, así que no "
                            "puede descifrar sesiones."));
    return false;
#endif
}

} // namespace qterm2
