#include "telemetry/telemetry.h"
#include <QJsonArray>
#include <QSettings>

namespace qterm2 {

Telemetry::Telemetry(QObject *parent)
    : QObject(parent)
    , m_startTime(QDateTime::currentDateTime())
{
}

void Telemetry::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void Telemetry::recordEvent(const QString &category, const QString &action, const QJsonObject &data)
{
    if (!m_enabled)
        return;
    QJsonObject event;
    event["category"] = category;
    event["action"] = action;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!data.isEmpty())
        event["data"] = data;
    m_events.append(event);
    emit eventRecorded(category, action);
}

void Telemetry::recordUsage(const QString &feature)
{
    recordEvent("usage", feature);
}

void Telemetry::recordError(const QString &error)
{
    QJsonObject data;
    data["error"] = error;
    recordEvent("error", "occurred", data);
}

QJsonObject Telemetry::report() const
{
    QJsonObject report;
    report["enabled"] = m_enabled;
    report["startTime"] = m_startTime.toString(Qt::ISODate);
    report["eventCount"] = m_events.size();
    QJsonArray events;
    for (const auto &e : m_events)
        events.append(e);
    report["events"] = events;
    return report;
}

void Telemetry::clearData()
{
    m_events.clear();
}

} // namespace qterm2
