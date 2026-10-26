#include "core/commandregistry.h"

#include <algorithm>

namespace qterm2 {

CommandRegistry::CommandRegistry(QObject *parent) : QObject(parent) {}

bool CommandRegistry::registerCommand(const Command &command)
{
    if (command.id.trimmed().isEmpty() || command.title.trimmed().isEmpty())
        return false;
    if (m_commands.contains(command.id))
        return false;
    m_commands.insert(command.id, command);
    emit changed();
    return true;
}

bool CommandRegistry::unregisterCommand(const QString &id)
{
    if (!m_commands.remove(id))
        return false;
    emit changed();
    return true;
}

void CommandRegistry::setEnabled(const QString &id, bool enabled)
{
    auto it = m_commands.find(id);
    if (it == m_commands.end() || it->enabled == enabled)
        return;
    it->enabled = enabled;
    emit changed();
}

QVector<Command> CommandRegistry::commands() const
{
    QVector<Command> result;
    result.reserve(m_commands.size());
    for (auto it = m_commands.cbegin(); it != m_commands.cend(); ++it)
        result.append(it.value());
    std::sort(result.begin(), result.end(), [](const Command &a, const Command &b) {
        const int byCategory = a.category.localeAwareCompare(b.category);
        if (byCategory != 0)
            return byCategory < 0;
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return result;
}

QVector<Command> CommandRegistry::commandsForPlugin(const QString &pluginId) const
{
    QVector<Command> result;
    for (const Command &command : commands()) {
        if (command.pluginId == pluginId)
            result.append(command);
    }
    return result;
}

Command CommandRegistry::command(const QString &id) const
{
    return m_commands.value(id);
}

bool CommandRegistry::contains(const QString &id) const
{
    return m_commands.contains(id);
}

} // namespace qterm2
