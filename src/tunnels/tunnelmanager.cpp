#include "tunnels/tunnelmanager.h"

#include "ssh/sshcommandbuilder.h"
#include "storage/settingsstore.h"

#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QUuid>

namespace qterm2 {

namespace {
constexpr int kMaxReconnectAttempts = 5;
} // namespace

QJsonObject TunnelConfig::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("id")] = id;
    o[QStringLiteral("name")] = name;
    o[QStringLiteral("host")] = host;
    o[QStringLiteral("port")] = port;
    o[QStringLiteral("user")] = user;
    o[QStringLiteral("auth")] = static_cast<int>(auth);
    o[QStringLiteral("privateKey")] = privateKey;
    o[QStringLiteral("proxyJump")] = proxyJump;
    o[QStringLiteral("hostKeyPolicy")] = static_cast<int>(hostKeyPolicy);
    o[QStringLiteral("keepAlive")] = keepAlive;
    o[QStringLiteral("connectTimeout")] = connectTimeout;
    o[QStringLiteral("autoReconnect")] = autoReconnect;
    o[QStringLiteral("forward")] = forward.toJson();
    return o;
}

TunnelConfig TunnelConfig::fromJson(const QJsonObject &obj)
{
    TunnelConfig c;
    c.id = obj.value(QStringLiteral("id")).toString();
    c.name = obj.value(QStringLiteral("name")).toString();
    c.host = obj.value(QStringLiteral("host")).toString();
    c.port = obj.value(QStringLiteral("port")).toInt(22);
    c.user = obj.value(QStringLiteral("user")).toString();
    c.auth = static_cast<Session::Auth>(obj.value(QStringLiteral("auth")).toInt(static_cast<int>(Session::Auth::PublicKey)));
    c.privateKey = obj.value(QStringLiteral("privateKey")).toString();
    c.proxyJump = obj.value(QStringLiteral("proxyJump")).toString();
    c.hostKeyPolicy = static_cast<Session::HostKeyPolicy>(obj.value(QStringLiteral("hostKeyPolicy")).toInt(static_cast<int>(Session::HostKeyPolicy::Ask)));
    c.keepAlive = obj.value(QStringLiteral("keepAlive")).toBool(true);
    c.connectTimeout = obj.value(QStringLiteral("connectTimeout")).toInt(10);
    c.autoReconnect = obj.value(QStringLiteral("autoReconnect")).toBool(false);
    c.forward = PortForward::fromJson(obj.value(QStringLiteral("forward")).toObject());
    if (c.id.isEmpty())
        c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return c;
}

TunnelManager::TunnelManager(QObject *parent)
    : QObject(parent)
{
}

TunnelManager::~TunnelManager()
{
    // Prevent auto-reconnect during teardown
    for (TunnelRuntime &rt : m_tunnels) {
        rt.config.autoReconnect = false;
        clearReconnect(rt.config.id);
    }
    stopAll();
}

QVector<TunnelRuntime> TunnelManager::tunnels() const
{
    return m_tunnels;
}

TunnelRuntime *TunnelManager::find(const QString &id)
{
    for (TunnelRuntime &t : m_tunnels) {
        if (t.config.id == id)
            return &t;
    }
    return nullptr;
}

const TunnelRuntime *TunnelManager::find(const QString &id) const
{
    for (const TunnelRuntime &t : m_tunnels) {
        if (t.config.id == id)
            return &t;
    }
    return nullptr;
}

QString TunnelManager::addTunnel(const TunnelConfig &config)
{
    TunnelConfig cfg = config;
    if (cfg.id.isEmpty())
        cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (cfg.name.isEmpty())
        cfg.name = QStringLiteral("Tunnel");
    TunnelRuntime rt;
    rt.config = cfg;
    m_tunnels.append(rt);
    emit tunnelsChanged();
    if (m_settings)
        saveToSettings(m_settings);
    return cfg.id;
}

bool TunnelManager::updateTunnel(const TunnelConfig &config)
{
    TunnelRuntime *rt = find(config.id);
    if (!rt)
        return false;
    const bool wasRunning = (rt->state == TunnelState::Running || rt->state == TunnelState::Starting);
    if (wasRunning)
        stopTunnel(config.id);
    rt->config = config;
    emit tunnelsChanged();
    if (m_settings)
        saveToSettings(m_settings);
    return true;
}

bool TunnelManager::removeTunnel(const QString &id)
{
    for (int i = 0; i < m_tunnels.size(); ++i) {
        if (m_tunnels[i].config.id == id) {
            m_tunnels[i].config.autoReconnect = false;
            clearReconnect(id);
            stopTunnel(id);
            if (m_tunnels[i].process) {
                m_tunnels[i].process->deleteLater();
                m_tunnels[i].process = nullptr;
            }
            m_tunnels.removeAt(i);
            emit tunnelsChanged();
            if (m_settings)
                saveToSettings(m_settings);
            return true;
        }
    }
    return false;
}

bool TunnelManager::isPortInUse(int port, const QString &host) const
{
    if (port < 1 || port > 65535)
        return false;
    QTcpServer probe;
    QHostAddress addr;
    if (host.isEmpty() || host == QLatin1String("*") || host == QLatin1String("0.0.0.0"))
        addr = QHostAddress::Any;
    else if (host == QLatin1String("::"))
        addr = QHostAddress::AnyIPv6;
    else if (!addr.setAddress(host))
        addr = QHostAddress::LocalHost;
    const bool ok = probe.listen(addr, static_cast<quint16>(port));
    probe.close();
    return !ok;
}

QStringList TunnelManager::buildArgs(const TunnelConfig &cfg, QString *error) const
{
    if (cfg.host.trimmed().isEmpty()) {
        if (error) *error = tr("El host del túnel está vacío.");
        return {};
    }
    if (!Session::isSafeArgument(cfg.host) || !Session::isSafeArgument(cfg.user)
        || !Session::isSafeArgument(cfg.proxyJump)) {
        if (error) *error = tr("El túnel contiene parámetros no seguros.");
        return {};
    }
    if (cfg.forward.listenPort < 1 || cfg.forward.listenPort > 65535) {
        if (error) *error = tr("Puerto de escucha inválido.");
        return {};
    }
    if (cfg.forward.type != PortForward::Type::Dynamic) {
        if (cfg.forward.targetHost.trimmed().isEmpty() || cfg.forward.targetPort < 1) {
            if (error) *error = tr("Destino del reenvío inválido.");
            return {};
        }
    }

    QStringList args;
    args << QStringLiteral("-N")
         << QStringLiteral("-o") << QStringLiteral("ExitOnForwardFailure=yes")
         << QStringLiteral("-o") << QStringLiteral("BatchMode=no");
    if (cfg.port != 22)
        args << QStringLiteral("-p") << QString::number(cfg.port);
    if (cfg.keepAlive) {
        args << QStringLiteral("-o") << QStringLiteral("ServerAliveInterval=30")
             << QStringLiteral("-o") << QStringLiteral("ServerAliveCountMax=3");
    }
    if (cfg.connectTimeout > 0)
        args << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=%1").arg(cfg.connectTimeout);

    switch (cfg.hostKeyPolicy) {
    case Session::HostKeyPolicy::Strict:
        args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=yes"); break;
    case Session::HostKeyPolicy::AcceptNew:
        args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new"); break;
    case Session::HostKeyPolicy::Ask:
        break;
    }

    if (cfg.auth == Session::Auth::PublicKey) {
        const QString key = ssh::expandPath(cfg.privateKey);
        if (!key.isEmpty() && QFileInfo::exists(key)) {
            args << QStringLiteral("-i") << key
                 << QStringLiteral("-o") << QStringLiteral("IdentitiesOnly=yes");
        }
    } else if (cfg.auth == Session::Auth::Agent) {
        args << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=publickey");
    }

    if (!cfg.proxyJump.trimmed().isEmpty())
        args << QStringLiteral("-J") << cfg.proxyJump.trimmed();

    const QString listenHost = cfg.forward.listenHost.trimmed().isEmpty()
                                   ? QStringLiteral("127.0.0.1")
                                   : cfg.forward.listenHost.trimmed();
    const QString listen = listenHost.contains(QLatin1Char(':')) && !listenHost.startsWith(QLatin1Char('['))
                               ? QStringLiteral("[%1]:%2").arg(listenHost).arg(cfg.forward.listenPort)
                               : QStringLiteral("%1:%2").arg(listenHost).arg(cfg.forward.listenPort);

    if (cfg.forward.type == PortForward::Type::Dynamic) {
        args << QStringLiteral("-D") << listen;
    } else {
        QString th = cfg.forward.targetHost.trimmed();
        if (th.contains(QLatin1Char(':')) && !th.startsWith(QLatin1Char('[')))
            th = QLatin1Char('[') + th + QLatin1Char(']');
        const QString target = QStringLiteral("%1:%2").arg(th).arg(cfg.forward.targetPort);
        args << (cfg.forward.type == PortForward::Type::Local ? QStringLiteral("-L") : QStringLiteral("-R"))
             << (listen + QLatin1Char(':') + target);
    }

    QString dest = cfg.host.trimmed();
    if (dest.contains(QLatin1Char(':')) && !dest.startsWith(QLatin1Char('[')))
        dest = QLatin1Char('[') + dest + QLatin1Char(']');
    if (!cfg.user.trimmed().isEmpty())
        dest = cfg.user.trimmed() + QLatin1Char('@') + dest;
    args << QStringLiteral("--") << dest;
    return args;
}

void TunnelManager::clearReconnect(const QString &id)
{
    TunnelRuntime *rt = find(id);
    if (!rt)
        return;
    if (rt->reconnectTimer) {
        rt->reconnectTimer->stop();
        rt->reconnectTimer->deleteLater();
        rt->reconnectTimer = nullptr;
    }
}

void TunnelManager::scheduleReconnect(const QString &id)
{
    TunnelRuntime *rt = find(id);
    if (!rt || !rt->config.autoReconnect)
        return;
    if (rt->reconnectAttempts >= kMaxReconnectAttempts) {
        rt->lastError = tr("Reconexión automática agotada tras %1 intentos.").arg(kMaxReconnectAttempts);
        emit tunnelStateChanged(id, rt->state, rt->lastError);
        emit tunnelsChanged();
        return;
    }
    ++rt->reconnectAttempts;
    const int delayMs = qMin(30000, 1000 * (1 << qMin(rt->reconnectAttempts, 10)));
    if (!rt->reconnectTimer) {
        rt->reconnectTimer = new QTimer(this);
        rt->reconnectTimer->setSingleShot(true);
        connect(rt->reconnectTimer, &QTimer::timeout, this, [this, id] {
            TunnelRuntime *r = find(id);
            if (!r || !r->config.autoReconnect)
                return;
            QString err;
            startTunnel(id, &err);
        });
    }
    rt->reconnectTimer->start(delayMs);
    rt->lastError = tr("Reintentando en %1 s…").arg(delayMs / 1000);
    emit tunnelStateChanged(id, rt->state, rt->lastError);
    emit tunnelsChanged();
}

bool TunnelManager::startTunnel(const QString &id, QString *error)
{
    TunnelRuntime *rt = find(id);
    if (!rt) {
        if (error) *error = tr("Túnel no encontrado.");
        return false;
    }
    if (rt->state == TunnelState::Running || rt->state == TunnelState::Starting)
        return true;

    const PortForward &f = rt->config.forward;
    if (isPortInUse(f.listenPort, f.listenHost)) {
        rt->state = TunnelState::Error;
        rt->lastError = tr("El puerto %1 ya está en uso.").arg(f.listenPort);
        emit tunnelStateChanged(id, rt->state, rt->lastError);
        emit tunnelsChanged();
        if (error) *error = rt->lastError;
        return false;
    }

    QString argError;
    const QStringList args = buildArgs(rt->config, &argError);
    if (args.isEmpty()) {
        rt->state = TunnelState::Error;
        rt->lastError = argError;
        emit tunnelStateChanged(id, rt->state, rt->lastError);
        emit tunnelsChanged();
        if (error) *error = argError;
        return false;
    }

    if (!rt->process) {
        rt->process = new QProcess(this);
        connect(rt->process, &QProcess::started, this, [this, id] { onProcessStarted(id); });
        connect(rt->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, id](int code, QProcess::ExitStatus st) { onProcessFinished(id, code, st); });
        connect(rt->process, &QProcess::errorOccurred, this,
                [this, id](QProcess::ProcessError e) { onProcessError(id, e); });
        connect(rt->process, &QProcess::readyReadStandardError, this, [this, id] {
            TunnelRuntime *r = find(id);
            if (r && r->process)
                appendLog(id, QString::fromLocal8Bit(r->process->readAllStandardError()));
        });
        connect(rt->process, &QProcess::readyReadStandardOutput, this, [this, id] {
            TunnelRuntime *r = find(id);
            if (r && r->process)
                appendLog(id, QString::fromLocal8Bit(r->process->readAllStandardOutput()));
        });
    }

    clearReconnect(id);
    rt->state = TunnelState::Starting;
    rt->lastError.clear();
    rt->log.clear();
    appendLog(id, tr("Iniciando túnel: ssh %1").arg(args.join(QLatin1Char(' '))));
    emit tunnelStateChanged(id, rt->state, QString());
    emit tunnelsChanged();

    rt->process->start(QStringLiteral("ssh"), args);
    if (!rt->process->waitForStarted(3000)) {
        rt->state = TunnelState::Error;
        rt->lastError = tr("No se pudo iniciar ssh para el túnel.");
        emit tunnelStateChanged(id, rt->state, rt->lastError);
        emit tunnelsChanged();
        if (error) *error = rt->lastError;
        if (rt->config.autoReconnect)
            scheduleReconnect(id);
        return false;
    }
    return true;
}

void TunnelManager::stopTunnel(const QString &id)
{
    TunnelRuntime *rt = find(id);
    if (!rt)
        return;
    clearReconnect(id);
    rt->reconnectAttempts = 0;
    if (rt->process && rt->process->state() != QProcess::NotRunning) {
        appendLog(id, tr("Deteniendo túnel…"));
        rt->process->terminate();
        if (!rt->process->waitForFinished(2000))
            rt->process->kill();
    }
    rt->state = TunnelState::Stopped;
    rt->lastError.clear();
    emit tunnelStateChanged(id, rt->state, QString());
    emit tunnelsChanged();
}

void TunnelManager::stopAll()
{
    for (TunnelRuntime &rt : m_tunnels) {
        rt.config.autoReconnect = false; // prevent reconnect storms during shutdown
        clearReconnect(rt.config.id);
        if (rt.process && rt.process->state() != QProcess::NotRunning) {
            rt.process->terminate();
            if (!rt.process->waitForFinished(1500))
                rt.process->kill();
        }
        rt.state = TunnelState::Stopped;
    }
}


void TunnelManager::appendLog(const QString &id, const QString &text)
{
    TunnelRuntime *rt = find(id);
    if (!rt || text.isEmpty())
        return;
    const QString chunk = text.endsWith(QLatin1Char('\n')) ? text : text + QLatin1Char('\n');
    rt->log += chunk;
    if (rt->log.size() > 20000)
        rt->log = rt->log.right(20000);
    emit tunnelLogAppended(id, chunk);
}

void TunnelManager::onProcessStarted(const QString &id)
{
    TunnelRuntime *rt = find(id);
    if (!rt) return;
    rt->state = TunnelState::Running;
    rt->lastError.clear();
    // Consider stable after some time? simple approach: reset attempts now
    rt->reconnectAttempts = 0;
    emit tunnelStateChanged(id, rt->state, QString());
    emit tunnelsChanged();
}

void TunnelManager::onProcessFinished(const QString &id, int exitCode, QProcess::ExitStatus status)
{
    TunnelRuntime *rt = find(id);
    if (!rt) return;
    const bool intentional = (rt->state == TunnelState::Stopped);
    const bool wasRunning = (rt->state == TunnelState::Running || rt->state == TunnelState::Starting);
    if (rt->process) {
        appendLog(id, QString::fromLocal8Bit(rt->process->readAllStandardError()));
        appendLog(id, QString::fromLocal8Bit(rt->process->readAllStandardOutput()));
    }
    rt->state = (status == QProcess::CrashExit || exitCode != 0) ? TunnelState::Error : TunnelState::Stopped;
    if (rt->state == TunnelState::Error && wasRunning) {
        rt->lastError = rt->log.trimmed().isEmpty()
            ? tr("El túnel terminó con código %1.").arg(exitCode)
            : rt->log.trimmed().section(QLatin1Char('\n'), -3); // tail-ish summary
    }
    appendLog(id, tr("Proceso finalizado (código %1).").arg(exitCode));
    emit tunnelStateChanged(id, rt->state, rt->lastError);
    emit tunnelsChanged();
    if (!intentional && rt->config.autoReconnect && wasRunning)
        scheduleReconnect(id);
}

void TunnelManager::onProcessError(const QString &id, QProcess::ProcessError)
{
    TunnelRuntime *rt = find(id);
    if (!rt) return;
    if (rt->state == TunnelState::Stopped)
        return;
    rt->state = TunnelState::Error;
    if (rt->process)
        rt->lastError = rt->process->errorString();
    emit tunnelStateChanged(id, rt->state, rt->lastError);
    emit tunnelsChanged();
    if (rt->config.autoReconnect)
        scheduleReconnect(id);
}

void TunnelManager::loadFromSettings(SettingsStore *settings)
{
    m_settings = settings;
    if (!settings)
        return;
    const QVariant v = settings->value(QStringLiteral("tunnels/defs"));
    QByteArray json;
    if (v.typeId() == QMetaType::QByteArray)
        json = v.toByteArray();
    else
        json = v.toString().toUtf8();
    if (json.isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray())
        return;
    // Do not duplicate if already loaded
    if (!m_tunnels.isEmpty())
        return;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject()) continue;
        TunnelConfig cfg = TunnelConfig::fromJson(val.toObject());
        TunnelRuntime rt;
        rt.config = cfg;
        m_tunnels.append(rt);
    }
    emit tunnelsChanged();
}

void TunnelManager::saveToSettings(SettingsStore *settings) const
{
    if (!settings)
        return;
    QJsonArray arr;
    for (const TunnelRuntime &rt : m_tunnels)
        arr.append(rt.config.toJson());
    const QJsonDocument doc(arr);
    settings->setValue(QStringLiteral("tunnels/defs"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

TunnelConfig TunnelManager::fromSessionForward(const Session &session, const PortForward &forward)
{
    TunnelConfig cfg;
    cfg.name = session.name;
    cfg.host = session.host;
    cfg.port = session.port;
    cfg.user = session.user;
    cfg.auth = session.auth;
    cfg.privateKey = session.privateKey;
    cfg.proxyJump = session.proxyJump;
    cfg.hostKeyPolicy = session.hostKeyPolicy;
    cfg.keepAlive = session.keepAlive;
    cfg.connectTimeout = session.connectTimeout;
    cfg.forward = forward;
    return cfg;
}

QString TunnelManager::stateName(TunnelState state)
{
    switch (state) {
    case TunnelState::Stopped:  return QObject::tr("Detenido");
    case TunnelState::Starting: return QObject::tr("Iniciando");
    case TunnelState::Running:  return QObject::tr("Activo");
    case TunnelState::Error:    return QObject::tr("Error");
    }
    return QObject::tr("Desconocido");
}

} // namespace qterm2
