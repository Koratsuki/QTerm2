#include "ui/sshintegration.h"

#include "domain/session.h"
#include "sessions/sessionstore.h"
#include "ssh/knownhosts.h"
#include "ssh/sshagent.h"
#include "ssh/sshconfig.h"
#include "ui/knownhostsdialog.h"
#include "ui/sshagentdialog.h"
#include "ui/sshconfigdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSet>
#include <QString>

namespace qterm2::ui {

namespace {

QString sshConfigPath()
{
    return QDir::home().filePath(QStringLiteral(".ssh/config"));
}

} // namespace

void importSshConfig(QWidget *parent, SessionStore *store)
{
    if (!store)
        return;

    const QString defaultPath = sshConfigPath();
    const QString path = QFileDialog::getOpenFileName(
        parent, QObject::tr("Import SSH config"), defaultPath,
        QObject::tr("Configuración SSH (*);;Todos los archivos (*)"));
    if (path.isEmpty())
        return;

    QString error;
    const ssh::SshConfigDocument document = ssh::parseSshConfig(path, &error);
    if (document.entries.isEmpty()) {
        QMessageBox::information(
            parent, QObject::tr("Sin hosts importables"),
            error.isEmpty()
                ? QObject::tr("No se encontraron bloques «Host» concretos en %1.").arg(path)
                : error);
        return;
    }

    QSet<QString> existing;
    for (const Session &session : store->sessions()) {
        if (!session.isLocal())
            existing.insert(session.host.trimmed().toLower());
    }

    SshConfigDialog dialog(document.entries, existing, parent);
    if (dialog.exec() != QDialog::Accepted)
        return;

    ssh::SshConfigDocument selectedDocument;
    selectedDocument.entries = dialog.selectedEntries();
    if (selectedDocument.entries.isEmpty()) {
        QMessageBox::information(parent, QObject::tr("Sin selección"),
                                 QObject::tr("No se seleccionó ningún host para importar."));
        return;
    }

    int added = 0;
    for (const Session &session : ssh::sessionsFromSshConfig(selectedDocument)) {
        const QString host = session.host.trimmed().toLower();
        if (existing.contains(host))
            continue;
        if (store->addSession(session)) {
            ++added;
            existing.insert(host);
        }
    }

    if (added == 0) {
        QMessageBox::information(parent, QObject::tr("Nada importado"),
                                 QObject::tr("Todas las entradas seleccionadas ya existían."));
        return;
    }

    QString message = QObject::tr("Se importaron %1 sesiones desde %2.").arg(added).arg(path);
    if (!document.warnings.isEmpty())
        message += QObject::tr("\n\nAvisos: %1")
                       .arg(document.warnings.join(QLatin1Char('\n')));
    QMessageBox::information(parent, QObject::tr("Importación completada"), message);
}

void exportSshConfig(QWidget *parent, SessionStore *store)
{
    if (!store)
        return;

    const QString block = ssh::renderSshConfig(store->sessions());
    if (block.isEmpty()) {
        QMessageBox::information(
            parent, QObject::tr("Nada que exportar"),
            QObject::tr("No hay sesiones SSH con servidor para exportar."));
        return;
    }

    int count = 0;
    for (const Session &session : store->sessions()) {
        if (!session.isLocal() && !session.host.trimmed().isEmpty())
            ++count;
    }

    const QString defaultPath = sshConfigPath();
    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("Export SSH config"), defaultPath,
        QObject::tr("Configuración SSH (*);;Todos los archivos (*)"));
    if (path.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        parent, QObject::tr("Export SSH config"),
        QObject::tr("Se escribirán %1 sesiones dentro del bloque gestionado por QTerm2 "
                    "en:\n%2\n\nEl contenido que ya exista fuera del bloque se conserva y "
                    "se crea una copia de seguridad del archivo.\n\n¿Continuar?")
            .arg(count)
            .arg(path),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!ssh::writeManagedSshConfig(path, block, &error)) {
        QMessageBox::warning(parent, QObject::tr("No se pudo exportar"), error);
        return;
    }

    QMessageBox::information(
        parent, QObject::tr("Exportación completada"),
        QObject::tr("Se exportaron %1 sesiones a %2.\n\nLos cambios se aplican a nuevas "
                    "conexiones de OpenSSH.")
            .arg(count)
            .arg(path));
}

void showKnownHosts(QWidget *parent)
{
    const QString path = QDir::home().filePath(QStringLiteral(".ssh/known_hosts"));
    if (!QFileInfo::exists(path)) {
        QMessageBox::information(
            parent, QObject::tr("known_hosts no encontrado"),
            QObject::tr("Todavía no existe %1. Se creará la primera vez que conectes a "
                        "un servidor con OpenSSH.")
                .arg(path));
        return;
    }

    KnownHostsDialog dialog(path, parent);
    dialog.exec();
}

void showSshAgent(QWidget *parent)
{
    // The dialog queries ssh-add on a worker thread instead of blocking the UI.
    SshAgentDialog dialog(ssh::AgentStatus{}, parent, true);
    dialog.exec();
}

} // namespace qterm2::ui
