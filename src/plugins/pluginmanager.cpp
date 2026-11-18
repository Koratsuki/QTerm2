#include "plugins/pluginmanager.h"

#include "core/apppaths.h"
#include "core/commandregistry.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef QTERM2_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#endif

namespace qterm2 {

PluginManager::PluginManager(PluginHost *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
}

QStringList PluginManager::searchPaths() const
{
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();

    paths << QDir::cleanPath(appDir + QStringLiteral("/plugins"));
    paths << QDir::cleanPath(appDir + QStringLiteral("/../lib/qterm2/plugins"));
    paths << QDir::homePath() + QStringLiteral("/.local/lib/qterm2/plugins");

    QStringList result;
    for (const QString &path : paths) {
        if (!result.contains(path))
            result.append(path);
    }
    return result;
}

bool PluginManager::loadAll(QString *errorMessage)
{
    unloadAll();
    m_errors.clear();

    for (const QString &dirPath : searchPaths()) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QFileInfoList entries =
            dir.entryInfoList(QStringList() << QStringLiteral("*.so")
                                            << QStringLiteral("*.dylib"),
                              QDir::Files | QDir::Readable);
        for (const QFileInfo &entry : entries)
            loadFile(entry.absoluteFilePath());
    }

    if (!m_errors.isEmpty() && errorMessage)
        *errorMessage = m_errors.join(QLatin1Char('\n'));
    emit changed();
    return true;
}

void PluginManager::loadFile(const QString &path)
{
    // Integrity and signature verification (defense-in-depth for native plugins).
    // 1) If <plugin>.sha256 exists, the binary hash must match.
    // 2) If <plugin>.sha256.sig exists, verify detached signature of the
    //    .sha256 manifest against a trusted public key (<plugin>.pub,
    //    <dir>/trusted.pem, or app config plugins/trusted.pem). This provides a
    //    cryptographic authenticity check when OpenSSL is available.
    // Native in-process sandboxing is not provided; plugins remain trusted code.
    const QString shaPath = path + QStringLiteral(".sha256");
    QByteArray manifestBytes;
    if (QFileInfo::exists(shaPath)) {
        QFile mf(shaPath);
        QString expected;
        if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            manifestBytes = mf.readAll();
            const QString firstLine = QString::fromUtf8(manifestBytes).section(QLatin1Char('\n'), 0, 0).trimmed();
            expected = firstLine.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0);
        }
        QFile pf(path);
        if (expected.isEmpty() || !pf.open(QIODevice::ReadOnly)) {
            m_errors << QStringLiteral("%1: no se pudo verificar la integridad (manifiesto SHA-256)").arg(path);
            return;
        }
        const QByteArray actual = QCryptographicHash::hash(pf.readAll(), QCryptographicHash::Sha256).toHex();
        if (actual != expected.toLatin1().toLower()) {
            m_errors << QStringLiteral("%1: la suma SHA-256 no coincide con %2").arg(path, shaPath);
            return;
        }

        const QString sigPath = shaPath + QStringLiteral(".sig");
        if (QFileInfo::exists(sigPath)) {
#ifdef QTERM2_HAVE_OPENSSL
            const QStringList keyCandidates = {
                path + QStringLiteral(".pub"),
                QFileInfo(path).dir().filePath(QStringLiteral("trusted.pem")),
                QDir(AppPaths::system().configDir).filePath(QStringLiteral("plugins/trusted.pem")),
            };
            QString keyPath;
            for (const QString &c : keyCandidates) {
                if (QFileInfo::exists(c)) { keyPath = c; break; }
            }
            if (keyPath.isEmpty()) {
                m_errors << QStringLiteral("%1: firma presente (%2) pero no se encontró clave pública de confianza").arg(path, sigPath);
                return;
            }
            QFile kf(keyPath);
            QFile sf(sigPath);
            if (!kf.open(QIODevice::ReadOnly) || !sf.open(QIODevice::ReadOnly)) {
                m_errors << QStringLiteral("%1: no se pudo leer la clave pública o la firma").arg(path);
                return;
            }
            const QByteArray keyData = kf.readAll();
            const QByteArray sigData = sf.readAll();
            if (manifestBytes.isEmpty()) {
                QFile mf2(shaPath);
                if (mf2.open(QIODevice::ReadOnly)) manifestBytes = mf2.readAll();
            }

            bool verified = false;
            BIO *kbio = BIO_new_mem_buf(keyData.constData(), int(keyData.size()));
            EVP_PKEY *pkey = PEM_read_bio_PUBKEY(kbio, nullptr, nullptr, nullptr);
            if (!pkey) {
                BIO_free(kbio);
                kbio = BIO_new_mem_buf(keyData.constData(), int(keyData.size()));
                // try reading as X509 cert public key
                X509 *cert = PEM_read_bio_X509(kbio, nullptr, nullptr, nullptr);
                if (cert) {
                    pkey = X509_get_pubkey(cert);
                    X509_free(cert);
                }
            }
            if (pkey) {
                EVP_MD_CTX *ctx = EVP_MD_CTX_new();
                if (ctx && EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1
                    && EVP_DigestVerifyUpdate(ctx, manifestBytes.constData(), size_t(manifestBytes.size())) == 1
                    && EVP_DigestVerifyFinal(ctx, reinterpret_cast<const unsigned char *>(sigData.constData()), size_t(sigData.size())) == 1) {
                    verified = true;
                }
                if (ctx) EVP_MD_CTX_free(ctx);
                EVP_PKEY_free(pkey);
            }
            BIO_free(kbio);
            if (!verified) {
                m_errors << QStringLiteral("%1: la firma criptográfica no es válida (%2)").arg(path, sigPath);
                return;
            }
#else
            m_errors << QStringLiteral("%1: firma presente (%2) pero OpenSSL no está disponible para verificarla").arg(path, sigPath);
            return;
#endif
        }
    }

    auto *loader = new QPluginLoader(path, this);
    QObject *instance = loader->instance();
    if (!instance) {
        m_errors << QStringLiteral("%1: %2").arg(path, loader->errorString());
        delete loader;
        return;
    }

    auto *plugin = qobject_cast<Plugin *>(instance);
    if (!plugin) {
        m_errors << QStringLiteral("%1: no implementa la interfaz QTerm2Plugin").arg(path);
        delete loader;
        return;
    }

    const PluginMetadata metadata = plugin->metadata();
    if (metadata.id.trimmed().isEmpty()) {
        m_errors << QStringLiteral("%1: metadatos sin id").arg(path);
        delete loader;
        return;
    }
    if (metadata.apiVersion != kPluginApiVersion) {
        m_errors << QStringLiteral("%1: API %2 no soportada (se esperaba %3)")
                        .arg(path)
                        .arg(metadata.apiVersion)
                        .arg(kPluginApiVersion);
        delete loader;
        return;
    }
    for (const LoadedPlugin &loaded : m_plugins) {
        if (loaded.id == metadata.id) {
            m_errors << QStringLiteral("%1: plugin duplicado %2").arg(path, metadata.id);
            delete loader;
            return;
        }
    }

    QString error;
    if (!plugin->initialize(m_host, &error)) {
        m_errors << QStringLiteral("%1: %2")
                        .arg(path, error.isEmpty() ? QStringLiteral("initialize() falló")
                                                   : error);
        plugin->shutdown();
        delete loader;
        return;
    }

    LoadedPlugin loaded;
    loaded.id = metadata.id;
    loaded.path = path;
    loaded.instance = instance;
    loaded.plugin = plugin;
    loaded.loader = loader;
    m_plugins.append(loaded);
}

void PluginManager::unloadAll()
{
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it) {
        if (it->plugin)
            it->plugin->shutdown();
        if (it->loader) {
            it->loader->unload();
            delete it->loader;
        }
    }
    m_plugins.clear();
    emit changed();
}

QStringList PluginManager::loadedPluginIds() const
{
    QStringList ids;
    for (const LoadedPlugin &loaded : m_plugins)
        ids << loaded.id;
    return ids;
}

} // namespace qterm2
