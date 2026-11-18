#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QVector>
#include <QHash>
#include <QTimer>

#include "domain/session.h"

namespace qterm2 {

class SettingsStore;

struct TunnelConfig
{
    QString id;
    QString name;
    // Connection parameters (standalone tunnel; may be derived from a session)
    QString host;
    int port = 22;
    QString user;
    Session::Auth auth = Session::Auth::PublicKey;
    QString privateKey;
    QString proxyJump;
    Session::HostKeyPolicy hostKeyPolicy = Session::HostKeyPolicy::Ask;
    bool keepAlive = true;
    int connectTimeout = 10;
    bool autoReconnect = false;

    PortForward forward;

    QJsonObject toJson() const;
    static TunnelConfig fromJson(const QJsonObject &obj);
};

enum class TunnelState { Stopped, Starting, Running, Error };

struct TunnelRuntime
{
    TunnelConfig config;
    TunnelState state = TunnelState::Stopped;
    QString lastError;
    QString log;
    QProcess *process = nullptr;
    int reconnectAttempts = 0;
    QTimer *reconnectTimer = nullptr;
};

class TunnelManager : public QObject
{
    Q_OBJECT
public:
    explicit TunnelManager(QObject *parent = nullptr);
    ~TunnelManager() override;

    QVector<TunnelRuntime> tunnels() const;
    QString addTunnel(const TunnelConfig &config);
    bool updateTunnel(const TunnelConfig &config);
    bool removeTunnel(const QString &id);
    bool startTunnel(const QString &id, QString *error = nullptr);
    void stopTunnel(const QString &id);
    void stopAll();
    bool isPortInUse(int port, const QString &host = QStringLiteral("127.0.0.1")) const;

    void loadFromSettings(SettingsStore *settings);
    void saveToSettings(SettingsStore *settings) const;

    static TunnelConfig fromSessionForward(const Session &session, const PortForward &forward);
    static QString stateName(TunnelState state);

signals:
    void tunnelsChanged();
    void tunnelStateChanged(const QString &id, TunnelState state, const QString &message);
    void tunnelLogAppended(const QString &id, const QString &text);

private:
    TunnelRuntime *find(const QString &id);
    const TunnelRuntime *find(const QString &id) const;
    QStringList buildArgs(const TunnelConfig &config, QString *error) const;
    void appendLog(const QString &id, const QString &text);
    void onProcessStarted(const QString &id);
    void onProcessFinished(const QString &id, int exitCode, QProcess::ExitStatus status);
    void onProcessError(const QString &id, QProcess::ProcessError error);
    void scheduleReconnect(const QString &id);
    void clearReconnect(const QString &id);

    QVector<TunnelRuntime> m_tunnels;
    SettingsStore *m_settings = nullptr;
};

} // namespace qterm2
