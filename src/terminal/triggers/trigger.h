#pragma once

#include <QObject>
#include <QString>
#include <QRegExp>
#include <QList>

namespace qterm2 {

enum class TriggerAction {
    Highlight,
    ShowNotification,
    RunCommand,
    PlayBell
};

class Trigger {
public:
    QString name;
    QString regex;
    TriggerAction action;
    QString parameters;
    bool enabled = true;
    QRegExp pattern;
};

class TriggerManager : public QObject
{
    Q_OBJECT
public:
    explicit TriggerManager(QObject *parent = nullptr);

    void addTrigger(const Trigger &trigger);
    void removeTrigger(int index);
    void clearTriggers();
    QList<Trigger> triggers() const { return m_triggers; }

    void processOutput(const QString &text);

signals:
    void triggered(const QString &name, const QString &match);

private:
    QList<Trigger> m_triggers;
};

} // namespace qterm2
