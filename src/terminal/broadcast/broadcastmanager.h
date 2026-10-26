#pragma once

#include <QObject>
#include <QList>
#include <QSet>

namespace qterm2 {

class BroadcastManager : public QObject
{
    Q_OBJECT
public:
    explicit BroadcastManager(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void addTarget(QObject *target);
    void removeTarget(QObject *target);
    void clearTargets();

    void broadcastInput(const QByteArray &data);

signals:
    void inputBroadcasted(const QByteArray &data, QObject *source);

private:
    bool m_enabled = false;
    QSet<QObject *> m_targets;
};

} // namespace qterm2
