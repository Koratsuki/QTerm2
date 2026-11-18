// Session and pane lifecycle for MainWindow.
//
// Split out of mainwindow.cpp so construction/UI code and session behavior can
// be read and reviewed independently.  This is still a MainWindow translation
// unit: it reaches the same private members, no new coupling is introduced.

#include "ui/mainwindow.h"

using qterm2::SessionStore;

#include "app/applicationcontext.h"
#include "sessions/sessionlauncher.h"
#include "sessions/sessionstore.h"
#include "ssh/keymanager.h"
#include "ssh/sshcommandbuilder.h"
#include "terminal/terminalwidget.h"
#include "ui/sessiondialog.h"
#include "ui/sidebar.h"
#include "ui/sshintegration.h"
#include "ui/terminalpane.h"
#include "ui/uihelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUuid>

#include <algorithm>

using qterm2::uihelpers::dotIcon;

namespace {
constexpr int kMaxReconnectAttempts = 5;
constexpr int kReconnectStableMs = 30000;
} // namespace

// The block below is the sessions section lifted verbatim from mainwindow.cpp.
// ---------------------------------------------------------------- sessions

void MainWindow::newLocalTab()
{
    Session local = Session::makeLocal();
    local.name = tr("Terminal local");
    local.shell = m_localShell;
    local.startDir = QDir::homePath();
    local.colorTag = QStringLiteral("green");
    addSessionTab(local);
}

TerminalPane *MainWindow::addSessionTab(const Session &session)
{
    auto *pane = new TerminalPane(session, m_tabs);
    pane->applyAppearance(Theme::TerminalColors::forMode(
                              m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light),
                          m_fontFamily, m_fontSize, m_scrollback, m_cursorBlink,
                          m_cursorStyle);
    QString error;
    if (!pane->start(&error)) {
        delete pane;
        QMessageBox::warning(this, tr("No se pudo abrir la sesión"), error);
        return nullptr;
    }

    const int index = m_tabs->addTab(pane, session.name);
    m_tabs->setTabIcon(index, dotIcon(session.colorTag));
    wirePane(pane);
    refreshTabInfo(pane);
    m_tabs->setCurrentIndex(index);
    updateWelcomeState();
    updateStatus();

    statusBar()->showMessage(session.isLocal()
                                 ? tr("Terminal local iniciada")
                                 : tr("Conectando a %1…").arg(session.displayTarget()),
                             3500);
    return pane;
}

TerminalPane *MainWindow::addCommandTab(const QString &title, const Session &session,
                                        const QString &program, const QStringList &args)
{
    Session display = session;
    display.name = title;
    auto *pane = new TerminalPane(display, m_tabs);
    pane->applyAppearance(Theme::TerminalColors::forMode(
                              m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light),
                          m_fontFamily, m_fontSize, m_scrollback, m_cursorBlink,
                          m_cursorStyle);
    QString error;
    if (!pane->startCommand(program, args, QDir::homePath(), &error)) {
        delete pane;
        QMessageBox::warning(this, tr("No se pudo ejecutar el comando"), error);
        return nullptr;
    }
    const int index = m_tabs->addTab(pane, title);
    m_tabs->setTabIcon(index, dotIcon(session.colorTag));
    wirePane(pane);
    refreshTabInfo(pane);
    m_tabs->setCurrentIndex(index);
    updateWelcomeState();
    return pane;
}

void MainWindow::wirePane(TerminalPane *pane)
{
    connect(pane, &TerminalPane::activeTerminalChanged, this, [this, pane](TerminalWidget *) {
        if (currentPane() == pane)
            updateStatus();
    });
    connect(pane, &TerminalPane::titleChanged, this, [this, pane] {
        refreshTabInfo(pane);
        if (currentPane() == pane)
            updateStatus();
    });
    connect(pane, &TerminalPane::terminalFinished, this, [this, pane](TerminalWidget *terminal) {
        if (currentPane() == pane) {
            statusBar()->showMessage(tr("El proceso terminó. La pestaña sigue abierta."), 4000);
            updateStatus();
        }
        refreshTabInfo(pane);
        if (!m_closing && pane->session().autoReconnect && !pane->session().isLocal())
            scheduleReconnect(pane, terminal);
    });
    connect(pane, &TerminalPane::bellRang, this, [this](TerminalWidget *) {
        QApplication::beep();
    });
    connect(pane, &TerminalPane::lastPaneClosed, this, [this, pane] {
        const int index = m_tabs->indexOf(pane);
        if (index >= 0)
            closeTabAt(index, false);
    });
    connect(pane, &TerminalPane::terminalAdded, this, [this](TerminalWidget *terminal) {
        connectTerminalBroadcast(terminal);
    });
    for (TerminalWidget *terminal : pane->terminals())
        connectTerminalBroadcast(terminal);
}

void MainWindow::scheduleReconnect(TerminalPane *pane, TerminalWidget *terminal)
{
    if (!pane || !terminal || m_closing || m_tabs->indexOf(pane) < 0)
        return;
    if (pane->session().isLocal() || !pane->session().autoReconnect)
        return;

    int attempt = m_reconnectAttempts.value(terminal, 0);
    if (attempt >= kMaxReconnectAttempts) {
        m_reconnectAttempts.remove(terminal);
        if (QTimer *timer = m_reconnectTimers.take(terminal))
            timer->deleteLater();
        statusBar()->showMessage(
            tr("No se pudo reconectar a %1 tras %2 intentos.")
                .arg(pane->session().displayTarget())
                .arg(kMaxReconnectAttempts),
            8000);
        return;
    }

    ++attempt;
    m_reconnectAttempts.insert(terminal, attempt);
    const int delaySeconds = qMin(30, 1 << (attempt - 1));

    QTimer *timer = m_reconnectTimers.value(terminal);
    if (!timer) {
        timer = new QTimer(terminal);
        timer->setSingleShot(true);
        m_reconnectTimers.insert(terminal, timer);
        // Deferred callbacks must not keep raw pointers to widgets that may
        // have been deleted (and whose address could have been reused).
        const QPointer<TerminalPane> paneGuard(pane);
        const QPointer<TerminalWidget> terminalGuard(terminal);
        connect(timer, &QTimer::timeout, this, [this, paneGuard, terminalGuard] {
            if (m_closing || paneGuard.isNull() || terminalGuard.isNull()
                || m_tabs->indexOf(paneGuard.data()) < 0)
                return;
            TerminalPane *pane = paneGuard.data();
            TerminalWidget *terminal = terminalGuard.data();

            QString error;
            if (!pane->restartTerminal(terminal, &error)) {
                statusBar()->showMessage(tr("Reconexión fallida: %1").arg(error), 5000);
                scheduleReconnect(pane, terminal);
                return;
            }

            statusBar()->showMessage(
                tr("Reconectando a %1 (intento %2/%3)…")
                    .arg(pane->session().displayTarget())
                    .arg(m_reconnectAttempts.value(terminal))
                    .arg(kMaxReconnectAttempts),
                5000);

            // Only a connection that stays up counts as success.
            QTimer::singleShot(kReconnectStableMs, terminal,
                               [this, terminalGuard] {
                if (!terminalGuard.isNull() && terminalGuard->isRunning())
                    m_reconnectAttempts.remove(terminalGuard.data());
            });
        });
        connect(terminal, &QObject::destroyed, this, [this, terminal] {
            m_reconnectAttempts.remove(terminal);
            m_reconnectTimers.remove(terminal);
        });
    }

    timer->start(delaySeconds * 1000);
    statusBar()->showMessage(
        tr("Conexión perdida. Reintentando en %1 s (intento %2/%3)…")
            .arg(delaySeconds)
            .arg(attempt)
            .arg(kMaxReconnectAttempts),
        6000);
}

void MainWindow::refreshTabInfo(TerminalPane *pane)
{
    const int index = m_tabs->indexOf(pane);
    if (index < 0)
        return;
    const Session &s = pane->session();
    m_tabs->setTabText(index, s.name);
    m_tabs->setTabIcon(index, dotIcon(s.colorTag));

    QStringList tips;
    tips << s.name;
    tips << (s.isLocal() ? tr("Terminal local: %1").arg(s.shell) : s.displayTarget());
    if (!s.isLocal())
        tips << tr("Autenticación: %1").arg(Session::authName(s.auth));
    tips << tr("Paneles: %1").arg(pane->terminals().size());
    if (TerminalWidget *term = pane->activeTerminal()) {
        if (!term->title().isEmpty())
            tips << term->title();
    }
    m_tabs->setTabToolTip(index, tips.join(QLatin1Char('\n')));
    updateWindowTitle();
}

void MainWindow::updateSessionsMenu()
{
    if (!m_sessionsMenu)
        return;
    m_sessionsMenu->clear();
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("newSsh")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("newLocal")));
    m_sessionsMenu->addSeparator();
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("importSshConfig")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("exportSshConfig")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("knownHosts")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("sshAgent")));
    m_sessionsMenu->addAction(m_actions.value(QStringLiteral("snippets")));
    m_sessionsMenu->addSeparator();

    if (!m_store || m_store->sessions().isEmpty()) {
        QAction *empty = m_sessionsMenu->addAction(tr("No hay sesiones guardadas"));
        empty->setEnabled(false);
        return;
    }

    QStringList groups = m_store->groups();
    if (groups.isEmpty())
        groups << QStringLiteral("Servidores");
    for (const QString &group : groups) {
        QMenu *sub = m_sessionsMenu->addMenu(group);
        QVector<Session> list;
        for (const Session &s : m_store->sessions()) {
            const QString g = s.group.trimmed().isEmpty() ? QStringLiteral("Servidores") : s.group.trimmed();
            if (g == group)
                list.append(s);
        }
        std::sort(list.begin(), list.end(), [](const Session &a, const Session &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        for (const Session &s : list) {
            QAction *action = sub->addAction(dotIcon(s.colorTag), s.name);
            const QString id = s.id;
            connect(action, &QAction::triggered, this, [this, id] { connectSession(id); });
        }
    }


}

void MainWindow::connectSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    addSessionTab(session);
    m_sidebar->selectSession(id);
    m_store->markUsed(id);
}

void MainWindow::promptNewSession()
{
    Session base;
    base.type = Session::Type::Ssh;
    base.group = m_store->suggestGroup();
    base.port = 22;
    base.keepAlive = true;
    base.connectTimeout = 10;
    base.hostKeyPolicy = Session::HostKeyPolicy::Ask;
    base.auth = Session::Auth::PublicKey;
    base.shell = m_localShell;

    const QStringList keys = qterm2::ssh::KeyManager::listPrivateKeys();
    for (const QString &preferred : { QStringLiteral("id_ed25519"), QStringLiteral("id_rsa"),
                                      QStringLiteral("id_ecdsa") }) {
        for (const QString &key : keys) {
            if (QFileInfo(key).fileName() == preferred) {
                base.privateKey = key;
                break;
            }
        }
        if (!base.privateKey.isEmpty())
            break;
    }
    if (base.privateKey.isEmpty() && !keys.isEmpty())
        base.privateKey = keys.first();

    SessionDialog dialog(base, m_store->groups(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session session = dialog.resultSession();
    if (!m_store->addSession(session)) {
        QMessageBox::warning(this, tr("No se pudo guardar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(session.id);
    addSessionTab(session);
}

void MainWindow::promptEditSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session current = m_store->sessionById(id);
    SessionDialog dialog(current, m_store->groups(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session updated = dialog.resultSession();
    if (!m_store->updateSession(updated)) {
        QMessageBox::warning(this, tr("No se pudo actualizar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(updated.id);

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *pane = qobject_cast<TerminalPane *>(m_tabs->widget(i));
        if (pane && pane->session().id == updated.id) {
            pane->updateSession(updated);
            refreshTabInfo(pane);
        }
    }
    // Re-apply so an edited terminal profile takes effect without reconnecting.
    applyTerminalAppearance();
    statusBar()->showMessage(tr("Sesión actualizada"), 2000);
}

void MainWindow::duplicateSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    Session copy = m_store->sessionById(id);
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name = tr("%1 (copia)").arg(copy.name);
    copy.createdAt = QDateTime();
    if (!m_store->addSession(copy)) {
        QMessageBox::warning(this, tr("No se pudo duplicar la sesión"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(copy.id);
}

void MainWindow::toggleFavorite(const QString &id)
{
    if (!m_store->contains(id))
        return;
    Session session = m_store->sessionById(id);
    session.favorite = !session.favorite;
    if (!m_store->updateSession(session)) {
        QMessageBox::warning(this, tr("No se pudo actualizar el favorito"),
                             m_store->lastError());
        return;
    }
    m_sidebar->selectSession(id);
    statusBar()->showMessage(session.favorite
                                 ? tr("«%1» añadida a favoritos").arg(session.name)
                                 : tr("«%1» quitada de favoritos").arg(session.name),
                             2000);
}

void MainWindow::deleteSession(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    const auto answer = QMessageBox::question(
        this, tr("Eliminar sesión"),
        tr("¿Eliminar «%1» del panel?\nLas terminales ya abiertas no se cerrarán.")
            .arg(session.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    if (!m_store->removeSession(id)) {
        QMessageBox::warning(this, tr("No se pudo eliminar la sesión"),
                             m_store->lastError());
        return;
    }
    // Clear from recents
    statusBar()->showMessage(tr("Sesión eliminada"), 2000);
}

void MainWindow::installPublicKey(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const Session session = m_store->sessionById(id);
    if (session.isLocal()) {
        QMessageBox::information(this, tr("Sesión local"),
                                 tr("Esta acción solo aplica a servidores SSH."));
        return;
    }

    const qterm2::ProcessSpec spec = qterm2::ssh::buildInstallKeySpec(session);
    if (!spec.ok) {
        QMessageBox::warning(this, tr("No se puede instalar la llave"), spec.error);
        return;
    }

    Session display = Session::makeLocal(tr("Instalar llave · %1").arg(session.name));
    display.colorTag = QStringLiteral("orange");
    TerminalPane *pane = addCommandTab(display.name, display, spec.program, spec.args);
    if (pane) {
        statusBar()->showMessage(
            tr("ssh-copy-id: escribe la contraseña del servidor si se solicita."), 6000);
    }
}

void MainWindow::copyCommand(const QString &id)
{
    if (!m_store->contains(id))
        return;
    const QString command =
        qterm2::sessions::SessionLauncher::commandLineFor(m_store->sessionById(id));
    if (command.isEmpty()) {
        QMessageBox::warning(this, tr("Comando no disponible"),
                             tr("Revisa la configuración de la sesión (host y llave)."));
        return;
    }
    QApplication::clipboard()->setText(command);
    statusBar()->showMessage(tr("Comando SSH copiado al portapapeles"), 2500);
}

void MainWindow::importSshConfig()
{
    qterm2::ui::importSshConfig(this, m_store);
}

void MainWindow::exportSshConfig()
{
    qterm2::ui::exportSshConfig(this, m_store);
}

void MainWindow::showKnownHosts()
{
    qterm2::ui::showKnownHosts(this);
}

void MainWindow::showSshAgent()
{
    qterm2::ui::showSshAgent(this);
}