#pragma once

#include <QObject>
#include <QHash>
#include <QKeySequence>
#include <QString>
#include <QVector>

#include <functional>

namespace qterm2 {

struct Command
{
    QString id;
    QString title;
    QString category;
    QKeySequence shortcut;
    QString pluginId;
    std::function<void()> action;
    bool enabled = true;
};

/**
 * Central command registry used by menus, shortcuts, the future command
 * palette and plugins.  UI shells register their native commands here; plugins
 * only receive this registry and never touch widgets directly.
 */
class CommandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit CommandRegistry(QObject *parent = nullptr);

    bool registerCommand(const Command &command);
    bool unregisterCommand(const QString &id);
    void setEnabled(const QString &id, bool enabled);

    QVector<Command> commands() const;
    QVector<Command> commandsForPlugin(const QString &pluginId) const;
    Command command(const QString &id) const;
    bool contains(const QString &id) const;

signals:
    void changed();

private:
    QHash<QString, Command> m_commands;
};

} // namespace qterm2
