// Headless UI test: builds the real MainWindow in the offscreen platform
// plugin and checks the pieces that do not require human interaction.

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>

#include "app/applicationcontext.h"
#include "core/apppaths.h"
#include "sessions/sessionstore.h"
#include "sessions/snippetstore.h"
#include "ssh/knownhosts.h"
#include "ssh/sshagent.h"
#include "ssh/sshconfig.h"
#include "storage/database.h"
#include "storage/settingsstore.h"
#include "storage/sqlitesnippetrepository.h"
#include "terminal/terminalwidget.h"
#include "ui/commandpalettedialog.h"
#include "ui/globalsearchdialog.h"
#include "ui/knownhostsdialog.h"
#include "ui/mainwindow.h"
#include "ui/sessiondialog.h"
#include "ui/sidebar.h"
#include "ui/snippetdialog.h"
#include "ui/sshagentdialog.h"
#include "ui/sshconfigdialog.h"
#include "ui/terminalpane.h"
#include "ui/theme.h"

#include <cstdio>

namespace {

int g_failures = 0;

void check(bool condition, const char *what)
{
    if (condition) {
        std::printf("  ok   %s\n", what);
    } else {
        std::printf("  FAIL %s\n", what);
        ++g_failures;
    }
}

void checkEqual(const QString &actual, const QString &expected, const char *what)
{
    if (actual == expected) {
        std::printf("  ok   %s\n", what);
    } else {
        std::printf("  FAIL %s\n       expected: %s\n       actual:   %s\n",
                    what, qPrintable(expected), qPrintable(actual));
        ++g_failures;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTerm2-uitest"));
    QApplication::setOrganizationName(QStringLiteral("QTerm2-uitest"));

    QTemporaryDir root;
    check(root.isValid(), "temporary application root created");

    qterm2::ApplicationContext context(qterm2::AppPaths::forRoot(root.path()));
    QString error;
    check(context.initialize(&error), "application context initializes");
    if (!error.isEmpty())
        std::printf("       warning: %s\n", qPrintable(error));
    check(context.settings() != nullptr, "settings store available");

    qterm2::SettingsStore *settings = context.settings();
    settings->setValue(QStringLiteral("ui/confirmClose"), false);
    settings->setValue(QStringLiteral("startup/openLocalTab"), false);
    settings->setValue(QStringLiteral("ui/sidebarVisible"), true);
    settings->setValue(QStringLiteral("ui/darkTheme"), true);

    Theme::setCurrentMode(Theme::Mode::Dark);
    Theme::apply(app);

    Session favorite = Session::makeLocal(QStringLiteral("Servidor favorito"));
    favorite.favorite = true;
    favorite.lastUsedAt = QDateTime::currentDateTimeUtc();
    check(context.sessionStore()->addSession(favorite), "favorite session created");

    Session recent = Session::makeLocal(QStringLiteral("Servidor reciente"));
    recent.lastUsedAt = QDateTime::currentDateTimeUtc().addSecs(-3600);
    check(context.sessionStore()->addSession(recent), "recent session created");

    std::printf("theme\n");
    check(!Theme::styleSheet(Theme::Mode::Dark).isEmpty(), "dark style sheet exists");
    check(!Theme::styleSheet(Theme::Mode::Dark).contains(QLatin1Char('@')),
          "all style tokens are expanded");
    check(Theme::TerminalColors::dark().background.isValid(), "dark palette is valid");
    check(Theme::TerminalColors::light().foreground.isValid(), "light palette is valid");

    std::printf("main window\n");
    {
        MainWindow window(&context);
        window.show();
        QCoreApplication::processEvents();

        auto *sidebar = window.findChild<Sidebar *>();
        auto *tabs = window.findChild<QTabWidget *>();
        auto *stack = window.findChild<QStackedWidget *>();
        auto *welcome = window.findChild<QWidget *>(QStringLiteral("Welcome"));

        check(sidebar != nullptr, "sidebar exists");
        check(tabs != nullptr, "tab widget exists");
        check(stack != nullptr, "stacked widget exists");
        check(welcome != nullptr, "welcome page exists");
        check(stack && welcome && stack->currentWidget() == welcome,
              "welcome page is shown when no tab is open");
        check(sidebar && sidebar->isVisible(), "sidebar starts visible");
        check(!window.windowTitle().isEmpty(), "window has a title");

        auto *tree = sidebar ? sidebar->findChild<QTreeWidget *>() : nullptr;
        check(tree != nullptr, "sidebar tree exists");
        const auto groupIndex = [tree](const QString &name) {
            if (!tree)
                return -1;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                if (tree->topLevelItem(i)->text(0).startsWith(name))
                    return i;
            }
            return -1;
        };
        const int favoritesIndex = groupIndex(QStringLiteral("Favorites"));
        const int recentsIndex = groupIndex(QStringLiteral("Recents"));
        const int localsIndex = groupIndex(QStringLiteral("Locales"));
        check(favoritesIndex >= 0, "favorites group is shown");
        check(recentsIndex >= 0, "recent group is shown");
        check(favoritesIndex >= 0 && recentsIndex > favoritesIndex,
              "favorites group precedes recents");
        check(recentsIndex >= 0 && localsIndex > recentsIndex,
              "recents group precedes regular groups");

        QAction *toggle = window.findChild<QAction *>(
            QStringLiteral("ToggleSidebarAction"));
        check(toggle != nullptr, "sidebar toggle action exists");
        if (toggle) {
            toggle->trigger();
            QCoreApplication::processEvents();
            check(sidebar && !sidebar->isVisible(), "sidebar hides");
            toggle->trigger();
            QCoreApplication::processEvents();
            check(sidebar && sidebar->isVisible(), "sidebar shows again");
        }

        check(tabs && tabs->count() == 0, "no startup tab by configuration");

        TerminalWidget terminal;
        terminal.setFontSize(12);
        terminal.setScrollbackLines(500);
        terminal.setCursorBlinking(false);
        check(terminal.fontPointSize() == 12, "terminal font size applied");
        check(!terminal.cursorBlinking(), "terminal cursor blink disabled");
        check(!terminal.isRunning(), "terminal idle before launch");

        terminal.setCursorStyle(TerminalEmulator::CursorStyle::IBeam);
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::IBeam,
              "I-Beam cursor style applied");
        terminal.setCursorStyle(TerminalEmulator::CursorStyle::Underscore);
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::Underscore,
              "underscore cursor style applied");
        terminal.setCursorStyle(TerminalEmulator::CursorStyle::Block);
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::Block,
              "block cursor style applied");

        checkEqual(TerminalWidget::cursorStyleName(
                       TerminalEmulator::CursorStyle::IBeam),
                   QStringLiteral("ibeam"), "I-Beam style name is stable");
        check(TerminalWidget::cursorStyleFromName(QStringLiteral("underscore"))
                  == TerminalEmulator::CursorStyle::Underscore,
              "style name parses back");
        check(TerminalWidget::cursorStyleFromName(QStringLiteral("unknown"))
                  == TerminalEmulator::CursorStyle::Block,
              "unknown style falls back to block");

        // DECSCUSR may change the shape, but never re-enables blinking when
        // the user disabled it in the preferences.
        terminal.emulator()->feed(QByteArrayLiteral("\x1b[5 q"));
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::IBeam,
              "DECSCUSR changes the cursor shape");
        check(!terminal.cursorBlinking(),
              "DECSCUSR keeps the user blink preference disabled");

        // DECSCUSR 0 ("default") must restore the user-configured style.
        terminal.setCursorStyle(TerminalEmulator::CursorStyle::IBeam);
        terminal.emulator()->feed(QByteArrayLiteral("\x1b[3 q"));
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::Underscore,
              "application cursor override is honoured");
        terminal.emulator()->feed(QByteArrayLiteral("\x1b[0 q"));
        check(terminal.cursorStyle() == TerminalEmulator::CursorStyle::IBeam,
              "DECSCUSR 0 restores the configured style");

        window.close();
        QCoreApplication::processEvents();
    }

    std::printf("session dialog\n");
    {
        Session session;
        session.name = QStringLiteral("Producción");
        session.type = Session::Type::Ssh;
        session.host = QStringLiteral("example.com");
        session.port = 22;
        session.user = QStringLiteral("alice");
        session.hostKeyPolicy = Session::HostKeyPolicy::Strict;
        session.connectTimeout = 42;
        session.favorite = true;
        session.autoReconnect = true;
        session.terminalProfile.enabled = true;
        session.terminalProfile.fontSize = 15;
        session.terminalProfile.scrollbackLines = 20000;
        session.terminalProfile.cursorStyle = QStringLiteral("underscore");
        session.terminalProfile.cursorBlink = false;

        PortForward forward;
        forward.type = PortForward::Type::Local;
        forward.listenHost = QStringLiteral("127.0.0.1");
        forward.listenPort = 5432;
        forward.targetHost = QStringLiteral("db.internal");
        forward.targetPort = 5432;
        session.portForwards.append(forward);

        SessionDialog dialog(session, { QStringLiteral("Servidores") });
        auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("HostKeyPolicyCombo"));
        auto *timeout = dialog.findChild<QSpinBox *>(QStringLiteral("ConnectTimeoutSpin"));
        auto *favorite = dialog.findChild<QCheckBox *>(QStringLiteral("FavoriteCheck"));
        auto *autoReconnect = dialog.findChild<QCheckBox *>(QStringLiteral("AutoReconnectCheck"));
        auto *forwards = dialog.findChild<QTableWidget *>(QStringLiteral("ForwardsTable"));
        auto *profile = dialog.findChild<QCheckBox *>(QStringLiteral("TerminalProfileCheck"));
        auto *profileSize = dialog.findChild<QSpinBox *>(QStringLiteral("ProfileFontSizeSpin"));
        auto *profileScrollback =
            dialog.findChild<QSpinBox *>(QStringLiteral("ProfileScrollbackSpin"));
        auto *profileCursor =
            dialog.findChild<QComboBox *>(QStringLiteral("ProfileCursorCombo"));
        auto *profileBlink =
            dialog.findChild<QCheckBox *>(QStringLiteral("ProfileCursorBlinkCheck"));

        check(policy && policy->currentData().toString() == QStringLiteral("strict"),
              "host key policy reaches the dialog");
        check(timeout && timeout->value() == 42, "connect timeout reaches the dialog");
        check(favorite && favorite->isChecked(), "favorite flag reaches the dialog");
        check(autoReconnect && autoReconnect->isChecked(),
              "auto reconnect flag reaches the dialog");
        check(profile && profile->isChecked(), "terminal profile reaches the dialog");
        check(profileSize && profileSize->value() == 15, "profile font size is loaded");
        check(profileScrollback && profileScrollback->value() == 20000,
              "profile scrollback is loaded");
        check(profileCursor
                  && profileCursor->currentData().toString() == QStringLiteral("underscore"),
              "profile cursor is loaded");
        check(profileBlink && !profileBlink->isChecked(), "profile blink is loaded");
        check(forwards && forwards->rowCount() == 1, "port forward reaches the dialog");
        if (forwards && forwards->rowCount() == 1) {
            auto *type = qobject_cast<QComboBox *>(forwards->cellWidget(0, 0));
            auto *listenHost = qobject_cast<QLineEdit *>(forwards->cellWidget(0, 1));
            auto *targetHost = qobject_cast<QLineEdit *>(forwards->cellWidget(0, 3));
            check(type && type->currentData().toInt() == int(PortForward::Type::Local),
                  "forward type is loaded");
            check(listenHost && listenHost->text() == QStringLiteral("127.0.0.1"),
                  "forward listen host is loaded");
            check(targetHost && targetHost->text() == QStringLiteral("db.internal"),
                  "forward target host is loaded");
        }

        if (qEnvironmentVariableIsSet("QTERM2_DIALOG_SCREENSHOT")) {
            auto *tabs = dialog.findChild<QTabWidget *>();
            if (tabs)
                tabs->setCurrentIndex(3);
            dialog.resize(780, 720);
            dialog.show();
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 300)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            check(dialog.grab().save(qEnvironmentVariable("QTERM2_DIALOG_SCREENSHOT")),
                  "dialog screenshot saved");
        }
    }

    std::printf("snippet dialog\n");
    {
        QTemporaryDir dir;
        const qterm2::AppPaths paths = qterm2::AppPaths::forRoot(dir.path());
        qterm2::Database database(paths);
        QString error;
        check(database.open(&error), "snippet dialog database opens");

        qterm2::SqliteSnippetRepository repository(&database);
        qterm2::SnippetStore store(&repository);
        check(store.load(), "snippet dialog store loads");

        qterm2::Snippet deploy;
        deploy.name = QStringLiteral("Despliegue");
        deploy.command = QStringLiteral("ssh ${user}@${host}");
        deploy.description = QStringLiteral("Entrar al servidor");
        check(store.saveSnippet(deploy), "snippet dialog seed is saved");

        Session session = Session::makeLocal();
        session.name = QStringLiteral("Producción");
        session.type = Session::Type::Ssh;
        session.host = QStringLiteral("prod.example.com");
        session.user = QStringLiteral("deploy");
        session.port = 2222;

        SnippetDialog dialog(&store, session);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("SnippetList"));
        auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("SnippetFilter"));
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("SnippetName"));
        auto *command = dialog.findChild<QLineEdit *>(QStringLiteral("SnippetCommand"));
        auto *preview = dialog.findChild<QLabel *>(QStringLiteral("SnippetPreview"));
        auto *newButton = dialog.findChild<QPushButton *>(QStringLiteral("SnippetNewButton"));
        auto *saveButton = dialog.findChild<QPushButton *>(QStringLiteral("SnippetSaveButton"));
        auto *insertButton =
            dialog.findChild<QPushButton *>(QStringLiteral("SnippetInsertButton"));

        check(list && list->count() == 1, "snippet list is populated");
        check(command && command->text() == QStringLiteral("ssh ${user}@${host}"),
              "snippet command is loaded in the editor");
        check(preview && preview->text() == QStringLiteral("ssh deploy@prod.example.com"),
              "snippet preview expands the active session");

        auto itemNamed = [list](const QString &text) -> QListWidgetItem * {
            for (int i = 0; i < list->count(); ++i) {
                if (list->item(i)->text() == text)
                    return list->item(i);
            }
            return nullptr;
        };

        filter->setText(QStringLiteral("zzz"));
        check(list->item(0)->isHidden(), "snippet filter hides unrelated entries");
        filter->clear();
        check(!list->item(0)->isHidden(), "clearing the filter shows snippets again");

        newButton->click();
        name->setText(QStringLiteral("Uptime"));
        command->setText(QStringLiteral("uptime"));
        saveButton->click();
        check(store.snippets().size() == 2, "new snippet is saved from the dialog");
        check(itemNamed(QStringLiteral("Uptime")) != nullptr,
              "new snippet appears in the list");

        filter->setText(QStringLiteral("uptime"));
        check(itemNamed(QStringLiteral("Despliegue"))->isHidden(),
              "filter matches by name");
        check(!itemNamed(QStringLiteral("Uptime"))->isHidden(),
              "filter keeps the matching snippet");
        filter->clear();

        list->setCurrentRow(0);
        check(preview->text() == QStringLiteral("ssh deploy@prod.example.com"),
              "selecting a snippet refreshes the preview");
        insertButton->click();
        checkEqual(dialog.commandToSend(), QStringLiteral("ssh deploy@prod.example.com"),
                   "insert returns the expanded command");
        check(dialog.result() == QDialog::Accepted, "insert accepts the dialog");
    }

    std::printf("global search dialog\n");
    {
        QVector<GlobalSearchTarget> targets;
        targets.append({ QStringLiteral("Servidor A"),
                         QStringLiteral("uno\ndos\nsystemctl status nginx\ncuatro") });
        targets.append({ QStringLiteral("Servidor B"),
                         QStringLiteral("nginx -v\notra línea") });

        GlobalSearchDialog dialog(targets);
        auto *edit = dialog.findChild<QLineEdit *>(QStringLiteral("GlobalSearchEdit"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("GlobalSearchList"));
        auto *hint = dialog.findChild<QLabel *>(QStringLiteral("GlobalSearchHint"));

        check(edit && list && hint, "global search widgets exist");
        check(list && list->count() == 0, "global search starts empty");

        edit->setText(QStringLiteral("nginx"));
        check(list && list->count() == 2, "global search finds all matches");
        check(hint && hint->text().contains(QStringLiteral("2 coincidencias")),
              "global search counts matches");
        check(list && list->item(0)->data(Qt::UserRole).toInt() == 0,
              "first match points at the first target");
        check(list && list->item(0)->data(Qt::UserRole + 1).toInt() == 2,
              "match line is zero-based internally");
        check(list && list->item(0)->text().contains(QStringLiteral("línea 3")),
              "match line is displayed one-based");
        check(list && list->item(1)->data(Qt::UserRole).toInt() == 1,
              "second match points at the second target");

        list->setCurrentRow(1);
        QMetaObject::invokeMethod(edit, "returnPressed", Qt::DirectConnection);
        check(dialog.selectedTargetIndex() == 1, "selected target is returned");
        check(dialog.selectedLine() == 0, "selected line is returned");
        check(dialog.result() == QDialog::Accepted, "global search accepts the result");

        GlobalSearchDialog empty(targets);
        auto *emptyEdit =
            empty.findChild<QLineEdit *>(QStringLiteral("GlobalSearchEdit"));
        auto *emptyHint = empty.findChild<QLabel *>(QStringLiteral("GlobalSearchHint"));
        emptyEdit->setText(QStringLiteral("zzz"));
        check(emptyHint->text().contains(QStringLiteral("Sin coincidencias")),
              "empty global search result is explained");
    }

    std::printf("command palette dialog\n");
    {
        QVector<qterm2::Command> commands;

        qterm2::Command ssh;
        ssh.id = QStringLiteral("qterm2.newSsh");
        ssh.title = QStringLiteral("Nueva conexión SSH");
        ssh.category = QStringLiteral("Archivo");
        ssh.shortcut = QKeySequence(QStringLiteral("Ctrl+N"));
        ssh.action = [] {};
        commands.append(ssh);

        qterm2::Command theme;
        theme.id = QStringLiteral("qterm2.darkTheme");
        theme.title = QStringLiteral("Tema oscuro");
        theme.category = QStringLiteral("Ver");
        theme.action = [] {};
        commands.append(theme);

        qterm2::Command disabled;
        disabled.id = QStringLiteral("qterm2.disabled");
        disabled.title = QStringLiteral("Comando desactivado");
        disabled.category = QStringLiteral("General");
        disabled.enabled = false;
        disabled.action = [] {};
        commands.append(disabled);

        CommandPaletteDialog dialog(commands);
        auto *edit = dialog.findChild<QLineEdit *>(QStringLiteral("CommandPaletteEdit"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("CommandPaletteList"));
        auto *hint = dialog.findChild<QLabel *>(QStringLiteral("CommandPaletteHint"));

        check(edit && list && hint, "command palette widgets exist");
        check(list && list->count() == 3, "command palette lists commands");
        check(list && list->item(2)->isHidden(),
              "disabled command is hidden from the palette");

        edit->setText(QStringLiteral("ssh"));
        check(list && !list->item(0)->isHidden(), "title token matches");
        check(list && list->item(1)->isHidden(), "unrelated command is filtered out");
        check(hint && hint->text().contains(QStringLiteral("1 comandos")),
              "palette hint counts matches");

        edit->setText(QStringLiteral("ver tema"));
        check(list && list->item(1)->isHidden() == false,
              "tokens match across category and title");
        check(list && list->item(0)->isHidden(), "first command is filtered out");

        edit->setText(QStringLiteral("zzz"));
        check(list && list->item(0)->isHidden() && list->item(1)->isHidden(),
              "no-match filter hides everything");
        check(hint && hint->text().contains(QStringLiteral("Sin coincidencias")),
              "empty result is explained");

        edit->setText(QStringLiteral("tema"));
        QMetaObject::invokeMethod(edit, "returnPressed", Qt::DirectConnection);
        checkEqual(dialog.selectedCommandId(), QStringLiteral("qterm2.darkTheme"),
                   "palette returns the selected command id");
        check(dialog.result() == QDialog::Accepted, "palette accepts the selection");
    }

    std::printf("ssh agent dialog\n");
    {
        qterm2::ssh::AgentStatus status;
        status.available = true;
        status.socketPath = QStringLiteral("/run/user/1000/ssh-agent.socket");
        qterm2::ssh::AgentIdentity identity;
        identity.bits = QStringLiteral("256");
        identity.fingerprint = QStringLiteral("SHA256:AAAAB3Nza");
        identity.comment = QStringLiteral("user@laptop");
        identity.keyType = QStringLiteral("ED25519");
        status.identities.append(identity);

        SshAgentDialog dialog(status);
        auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("AgentTable"));
        auto *socket = dialog.findChild<QLabel *>(QStringLiteral("AgentSocketLabel"));
        auto *state = dialog.findChild<QLabel *>(QStringLiteral("AgentStatusLabel"));
        auto *refresh = dialog.findChild<QPushButton *>(QStringLiteral("RefreshAgentButton"));
        check(table && table->rowCount() == 1, "agent identity reaches the dialog");
        check(socket && socket->text() == status.socketPath,
              "agent socket is displayed");
        check(state && state->text().contains(QStringLiteral("1 identidades")),
              "agent state is displayed");
        check(refresh != nullptr, "agent dialog can refresh");
    }

    std::printf("ssh config import dialog\n");
    {
        qterm2::ssh::SshConfigEntry entry;
        entry.alias = QStringLiteral("prod");
        entry.hostName = QStringLiteral("prod.example.com");
        entry.user = QStringLiteral("deploy");
        entry.port = 2222;
        entry.identityFile = QStringLiteral("/home/user/.ssh/id_ed25519");
        entry.sourcePath = QStringLiteral("/home/user/.ssh/config");
        entry.line = 4;

        SshConfigDialog dialog({ entry }, {}, nullptr);
        auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("SshConfigTable"));
        auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("SshConfigFilter"));
        check(table && table->rowCount() == 1, "ssh config entry reaches the dialog");
        check(dialog.selectedEntries().size() == 1, "new host is selected by default");

        auto *table2 = table;
        if (filter && table2) {
            filter->setText(QStringLiteral("no-such-host"));
            check(table2->isRowHidden(0), "filter hides non-matching hosts");
            filter->clear();
            check(!table2->isRowHidden(0), "filter restores matching hosts");
        }

        SshConfigDialog existingDialog(
            { entry }, { QStringLiteral("prod.example.com") }, nullptr);
        check(existingDialog.selectedEntries().isEmpty(),
              "existing host is not preselected");
    }

    std::printf("known_hosts dialog\n");
    {
        QTemporaryDir knownDir;
        const QString path = knownDir.filePath(QStringLiteral("known_hosts"));
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Text),
              "known_hosts fixture created");
        file.write("server.example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAA test@host\n");
        file.close();

        KnownHostsDialog dialog(path);
        auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("KnownHostsTable"));
        auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("KnownHostsFilter"));
        check(table && table->rowCount() == 1, "known_hosts entry reaches the dialog");
        if (table && filter) {
            filter->setText(QStringLiteral("no-such-host"));
            check(table->isRowHidden(0), "known_hosts filter hides non-matching rows");
            filter->clear();
            check(!table->isRowHidden(0), "known_hosts filter restores matching rows");
        }
    }

    std::printf("terminal profile application\n");
    {
        Session session = Session::makeLocal();
        session.terminalProfile.enabled = true;
        session.terminalProfile.fontSize = 15;
        session.terminalProfile.scrollbackLines = 20000;
        session.terminalProfile.cursorStyle = QStringLiteral("underscore");
        session.terminalProfile.cursorBlink = false;

        TerminalPane pane(session);
        pane.applyAppearance(Theme::TerminalColors::dark(), QStringLiteral("Monospace"),
                             11, 10000, true, QStringLiteral("block"));
        QString error;
        check(pane.start(&error), "profile terminal starts");
        TerminalWidget *term = pane.activeTerminal();
        check(term && term->fontPointSize() == 15, "profile font size is applied");
        check(term && term->emulator()->maxHistory() == 20000,
              "profile scrollback is applied");
        check(term && term->cursorStyle() == TerminalEmulator::CursorStyle::Underscore,
              "profile cursor is applied");
        check(term && !term->cursorBlinking(), "profile blink is applied");
    }

    // Optional visual smoke test: QTERM2_SCREENSHOT=/path/out.png
    if (qEnvironmentVariableIsSet("QTERM2_SCREENSHOT")) {
        const QString path = qEnvironmentVariable("QTERM2_SCREENSHOT");
        settings->setValue(QStringLiteral("startup/openLocalTab"), true);
        settings->setValue(QStringLiteral("terminal/cursorStyle"),
                           QStringLiteral("ibeam"));
        settings->setValue(QStringLiteral("terminal/cursorBlink"), false);
        MainWindow window(&context);
        window.resize(1240, 800);
        window.show();
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1500)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        auto *term = window.findChild<TerminalWidget *>();
        check(term != nullptr, "local terminal was created");
        check(term && term->isRunning(), "local terminal is running");
        check(term && term->cursorStyle() == TerminalEmulator::CursorStyle::IBeam,
              "configured I-Beam cursor reaches the terminal");
        if (term && qEnvironmentVariableIsSet("QTERM2_TEST_DUMP"))
            std::printf("--- local screen ---\n%s\n--------------------\n",
                        qPrintable(term->screenText()));
        check(window.grab().save(path), "window screenshot saved");
        window.close();
        QCoreApplication::processEvents();
    }

    if (g_failures == 0)
        std::printf("\nall UI tests passed\n");
    else
        std::printf("\n%d UI test(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
