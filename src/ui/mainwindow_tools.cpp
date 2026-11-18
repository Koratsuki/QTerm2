// Command palette, snippets, global search, in-terminal search and help dialogs
// for MainWindow.
//
// Split out of mainwindow.cpp to keep UI construction and per-feature behavior
// readable.  This is still a MainWindow translation unit: it reaches the same
// private members, no new coupling is introduced.

#include "ui/mainwindow.h"

#include "app/applicationcontext.h"
#include "core/commandregistry.h"
#include "domain/session.h"
#include "sessions/snippetstore.h"
#include "terminal/terminalwidget.h"
#include "ui/commandpalettedialog.h"
#include "ui/globalsearchdialog.h"
#include "ui/snippetdialog.h"
#include "ui/terminalpane.h"

#include <QAction>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QVector>
#include <QWidget>

void MainWindow::showSnippets()
{
    if (!m_context || !m_context->snippetStore())
        return;

    const Session context = currentPane() ? currentPane()->session()
                                          : Session::makeLocal();
    SnippetDialog dialog(m_context->snippetStore(), context, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString command = dialog.commandToSend();
    if (command.isEmpty())
        return;

    TerminalWidget *term = activeTerminal();
    if (!term || !term->isRunning()) {
        statusBar()->showMessage(
            tr("Abre una terminal activa para insertar el snippet"), 3000);
        return;
    }
    term->sendText(command);
    term->setFocus();
    statusBar()->showMessage(tr("Snippet insertado en la terminal"), 2000);
}

void MainWindow::showCommandPalette()
{
    if (!m_context || !m_context->commands())
        return;

    QVector<qterm2::Command> commands;
    for (const qterm2::Command &command : m_context->commands()->commands()) {
        if (command.enabled)
            commands.append(command);
    }
    if (commands.isEmpty()) {
        statusBar()->showMessage(tr("No hay comandos disponibles"), 2500);
        return;
    }

    CommandPaletteDialog dialog(commands, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString id = dialog.selectedCommandId();
    if (id.isEmpty())
        return;
    const qterm2::Command command = m_context->commands()->command(id);
    if (command.action)
        command.action();
}

void MainWindow::showGlobalSearch()
{
    struct Target
    {
        int tabIndex = -1;
        TerminalWidget *terminal = nullptr;
    };

    QVector<Target> targets;
    QVector<GlobalSearchTarget> snapshots;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
        if (!pane)
            continue;
        const QList<TerminalWidget *> terminals = pane->terminals();
        for (int j = 0; j < terminals.size(); ++j) {
            TerminalWidget *terminal = terminals.at(j);
            QString label = m_tabs->tabText(i);
            if (terminals.size() > 1)
                label += tr(" · panel %1").arg(j + 1);
            targets.append({ i, terminal });
            snapshots.append({ label, terminal->screenText() });
        }
    }

    if (snapshots.isEmpty()) {
        statusBar()->showMessage(tr("No hay terminales abiertas"), 2500);
        return;
    }

    GlobalSearchDialog dialog(snapshots, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const int index = dialog.selectedTargetIndex();
    if (index < 0 || index >= targets.size())
        return;

    const Target &target = targets.at(index);
    m_tabs->setCurrentIndex(target.tabIndex);
    if (auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(target.tabIndex)))
        pane->activateTerminal(target.terminal);
    if (!dialog.query().isEmpty())
        target.terminal->setSearchText(dialog.query());
    target.terminal->scrollToLine(dialog.selectedLine());
    target.terminal->setFocus();
    statusBar()->showMessage(
        tr("Resultado en %1").arg(m_tabs->tabText(target.tabIndex)), 2500);
}

void MainWindow::registerCommands()
{
    if (!m_context || !m_context->commands())
        return;
    qterm2::CommandRegistry *registry = m_context->commands();

    for (const QString &id : m_registeredCommandIds)
        registry->unregisterCommand(id);
    m_registeredCommandIds.clear();

    const QHash<QString, QString> categories = {
        { QStringLiteral("newLocal"), tr("Archivo") },
        { QStringLiteral("newSsh"), tr("Archivo") },
        { QStringLiteral("closeTab"), tr("Archivo") },
        { QStringLiteral("closePane"), tr("Archivo") },
        { QStringLiteral("quit"), tr("Archivo") },
        { QStringLiteral("importSshConfig"), tr("Sesiones") },
        { QStringLiteral("exportSshConfig"), tr("Sesiones") },
        { QStringLiteral("knownHosts"), tr("Sesiones") },
        { QStringLiteral("sshAgent"), tr("Sesiones") },
        { QStringLiteral("snippets"), tr("Sesiones") },
        { QStringLiteral("copy"), tr("Editar") },
        { QStringLiteral("paste"), tr("Editar") },
        { QStringLiteral("selectAll"), tr("Editar") },
        { QStringLiteral("search"), tr("Editar") },
        { QStringLiteral("globalSearch"), tr("Editar") },
        { QStringLiteral("clear"), tr("Terminal") },
        { QStringLiteral("reset"), tr("Terminal") },
        { QStringLiteral("splitRight"), tr("Terminal") },
        { QStringLiteral("splitDown"), tr("Terminal") },
        { QStringLiteral("zoomIn"), tr("Terminal") },
        { QStringLiteral("zoomOut"), tr("Terminal") },
        { QStringLiteral("zoomReset"), tr("Terminal") },
        { QStringLiteral("toggleSidebar"), tr("Ver") },
        { QStringLiteral("darkTheme"), tr("Ver") },
        { QStringLiteral("fullscreen"), tr("Ver") },
        { QStringLiteral("nextTab"), tr("Ventana") },
        { QStringLiteral("prevTab"), tr("Ventana") },
        { QStringLiteral("commandPalette"), tr("Aplicación") },
        { QStringLiteral("prefs"), tr("Aplicación") },
        { QStringLiteral("about"), tr("Ayuda") },
        { QStringLiteral("shortcuts"), tr("Ayuda") },
    };

    for (auto it = m_actions.cbegin(); it != m_actions.cend(); ++it) {
        QAction *action = it.value();
        qterm2::Command command;
        command.id = QStringLiteral("qterm2.") + it.key();
        command.title = action->text();
        command.category = categories.value(it.key(), tr("General"));
        command.shortcut = action->shortcuts().value(0);
        command.action = [action] {
            if (action && action->isEnabled())
                action->trigger();
        };
        if (registry->registerCommand(command))
            m_registeredCommandIds.append(command.id);
    }
}

// ------------------------------------------------------------------ search

void MainWindow::showSearch()
{
    if (!currentPane())
        return;
    m_searchBar->show();
    if (TerminalWidget *term = activeTerminal()) {
        const QString selection = term->selectedText();
        if (!selection.isEmpty() && !selection.contains(QLatin1Char('\n'))
            && selection.size() < 120)
            m_searchEdit->setText(selection);
    }
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    m_searchEdit->selectAll();
}

void MainWindow::hideSearch()
{
    if (m_searchBar->isHidden())
        return;
    m_searchBar->hide();
    m_searchStatus->clear();
    if (TerminalWidget *term = activeTerminal())
        term->clearSearch();
    m_searchEdit->clear();
    if (TerminalPane *pane = currentPane())
        pane->focusActive();
}

bool MainWindow::findInTerminal(bool forward)
{
    TerminalWidget *term = activeTerminal();
    if (!term || m_searchEdit->text().isEmpty())
        return false;
    term->setSearchText(m_searchEdit->text());
    const bool found = term->findNext(forward);
    m_searchStatus->setText(found ? tr("Coincidencia encontrada") : tr("Sin coincidencias"));
    m_searchStatus->setStyleSheet(found ? QString()
                                        : QStringLiteral("color: #f87171;"));
    return found;
}

// -------------------------------------------------------------------- help

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("Acerca de QTerm2"),
        tr("<h3>QTerm2 v1.1</h3>"
           "<p>Un terminal para Linux inspirado en iTerm2, con gestor de "
           "sesiones SSH integrado.</p>"
           "<p>Emulador xterm-256color con color de 24 bits, pestañas, paneles "
           "divididos, historial desplazable, búsqueda y llaves OpenSSH.</p>"
           "<p>Qt %1 · C++17 · OpenSSH</p>")
            .arg(QString::fromLatin1(qVersion())));
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("Atajos de teclado"),
        tr("<b>Pestañas y paneles</b><br>"
           "Ctrl+T · nueva terminal local<br>"
           "Ctrl+Shift+N · nueva conexión SSH<br>"
           "Ctrl+W · cerrar pestaña<br>"
           "Ctrl+Shift+W · cerrar panel dividido<br>"
           "Ctrl+Shift+D · dividir a la derecha<br>"
           "Ctrl+Shift+E · dividir abajo<br>"
           "Ctrl+Tab / Ctrl+Shift+Tab · cambiar pestaña<br>"
           "Ctrl+1…9 · ir a la pestaña<br><br>"
           "<b>Panel y terminal</b><br>"
           "Ctrl+B · mostrar/ocultar panel lateral<br>"
           "Ctrl+Shift+F · buscar<br>"
           "F3 / Shift+F3 · siguiente/anterior coincidencia<br>"
           "Ctrl+Shift+C / Ctrl+Shift+V · copiar/pegar<br>"
           "Ctrl+Shift+A · seleccionar todo<br>"
           "Ctrl+Shift+K · limpiar historial<br>"
           "Ctrl++ / Ctrl+- / Ctrl+0 · zoom<br>"
           "Ctrl+Shift+rueda o Shift+PageUp · historial<br>"
           "Ctrl+clic · abrir enlaces<br>"
           "F11 · pantalla completa"));
}
