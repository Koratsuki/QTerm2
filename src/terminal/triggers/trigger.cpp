#include "terminal/triggers/trigger.h"
#include <QDebug>

namespace qterm2 {

TriggerManager::TriggerManager(QObject *parent)
    : QObject(parent)
{
}

void TriggerManager::addTrigger(const Trigger &trigger)
{
    Trigger t = trigger;
    t.pattern = QRegExp(t.regex);
    if (t.pattern.isValid())
        m_triggers.append(t);
}

void TriggerManager::removeTrigger(int index)
{
    if (index >= 0 && index < m_triggers.size())
        m_triggers.removeAt(index);
}

void TriggerManager::clearTriggers()
{
    m_triggers.clear();
}

void TriggerManager::processOutput(const QString &text)
{
    for (const auto &trigger : m_triggers) {
        if (!trigger.enabled)
            continue;
        if (trigger.pattern.indexIn(text) >= 0) {
            emit triggered(trigger.name, trigger.pattern.cap(0));
        }
    }
}

} // namespace qterm2
