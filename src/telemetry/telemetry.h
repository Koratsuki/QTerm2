#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDateTime>

namespace qterm2 {

class Telemetry : public QObject
{
    Q_OBJECT
public:
    explicit Telemetry(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void recordEvent(const QString &category, const QString &action, const QJsonObject &data = QJsonObject());
    void recordUsage(const QString &feature);
    void recordError(const QString &error);

    QJsonObject report() const;
    void clearData();

signals:
    void eventRecorded(const QString &category, const QString &action);

private:
    bool m_enabled = false; // opt-in only
    QVector<QJsonObject> m_events;
    QDateTime m_startTime;
};

} // namespace qterm2
