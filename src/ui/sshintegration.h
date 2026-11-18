#pragma once

class QWidget;

namespace qterm2 {
class SessionStore;
}

namespace qterm2::ui {

/**
 * `~/.ssh` integration entry points.
 *
 * Each one owns its file pickers, confirmation prompts and dialogs, so
 * MainWindow only has to wire a menu action to it.  They never touch the
 * filesystem without the user picking a path and confirming.
 */
void importSshConfig(QWidget *parent, SessionStore *store);
void exportSshConfig(QWidget *parent, SessionStore *store);
void showKnownHosts(QWidget *parent);
void showSshAgent(QWidget *parent);

} // namespace qterm2::ui
