#include "ui/sftp/sftpdialog.h"

#include "sessions/sessionstore.h"
#include "ssh/sshcommandbuilder.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

SftpDialog::SftpDialog(qterm2::SessionStore *sessions, QWidget *parent)
    : QDialog(parent), m_sessions(sessions), m_queue(this)
{
    setWindowTitle(tr("Explorador SFTP"));
    resize(920, 540);

    auto *layout = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Sesión:"), this));
    m_sessionCombo = new QComboBox(this);
    top->addWidget(m_sessionCombo, 1);
    m_connectBtn = new QPushButton(tr("Conectar"), this);
    m_disconnectBtn = new QPushButton(tr("Desconectar"), this);
    m_disconnectBtn->setEnabled(false);
    top->addWidget(m_connectBtn);
    top->addWidget(m_disconnectBtn);
    layout->addLayout(top);

    auto *pathRow = new QHBoxLayout;
    auto *upBtn = new QPushButton(tr("Arriba"), this);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Ruta remota (ej. /home/user)"));
    auto *goBtn = new QPushButton(tr("Ir"), this);
    auto *refreshBtn = new QPushButton(tr("Actualizar"), this);
    pathRow->addWidget(upBtn);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(goBtn);
    pathRow->addWidget(refreshBtn);
    layout->addLayout(pathRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Nombre"), tr("Tipo"), tr("Tamaño"), tr("Permisos"), tr("Propietario")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *dlBtn = new QPushButton(tr("Descargar"), this);
    auto *ulBtn = new QPushButton(tr("Subir…"), this);
    auto *editBtn = new QPushButton(tr("Editar remoto…"), this);
    auto *mkdirBtn = new QPushButton(tr("Nueva carpeta"), this);
    auto *renBtn = new QPushButton(tr("Renombrar"), this);
    auto *rmBtn = new QPushButton(tr("Eliminar"), this);
    auto *xferBtn = new QPushButton(tr("Transferencias"), this);
    actions->addWidget(dlBtn);
    actions->addWidget(ulBtn);
    actions->addWidget(editBtn);
    actions->addWidget(mkdirBtn);
    actions->addWidget(renBtn);
    actions->addWidget(rmBtn);
    actions->addWidget(xferBtn);
    actions->addStretch();
    layout->addLayout(actions);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);
    m_status = new QLabel(tr("Desconectado."), this);
    layout->addWidget(m_status);

    if (m_sessions) {
        for (const Session &s : m_sessions->sessions()) {
            if (!s.isLocal() && s.type == Session::Type::Ssh)
                m_sessionCombo->addItem(QStringLiteral("%1 (%2)").arg(s.name, s.displayTarget()), s.id);
        }
    }

    connect(m_connectBtn, &QPushButton::clicked, this, &SftpDialog::connectSelectedSession);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &SftpDialog::disconnectSession);
    connect(refreshBtn, &QPushButton::clicked, this, &SftpDialog::refresh);
    connect(goBtn, &QPushButton::clicked, this, [this] { navigateTo(m_pathEdit->text().trimmed()); });
    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this] { navigateTo(m_pathEdit->text().trimmed()); });
    connect(upBtn, &QPushButton::clicked, this, &SftpDialog::goUp);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &SftpDialog::onItemActivated);
    connect(dlBtn, &QPushButton::clicked, this, &SftpDialog::downloadSelected);
    connect(ulBtn, &QPushButton::clicked, this, &SftpDialog::uploadFile);
    connect(editBtn, &QPushButton::clicked, this, &SftpDialog::editSelected);
    connect(mkdirBtn, &QPushButton::clicked, this, &SftpDialog::makeDirectory);
    connect(rmBtn, &QPushButton::clicked, this, &SftpDialog::removeSelected);
    connect(renBtn, &QPushButton::clicked, this, &SftpDialog::renameSelected);
    connect(xferBtn, &QPushButton::clicked, this, &SftpDialog::showTransfers);

    connect(&m_queue, &qterm2::TransferQueue::queueChanged, this, &SftpDialog::refreshTransfersView);
    connect(&m_queue, &qterm2::TransferQueue::itemUpdated, this, [this](const qterm2::TransferItem &item) {
        if (item.status == qterm2::TransferStatus::Completed)
            m_status->setText(tr("Transferencia completada: %1").arg(QFileInfo(item.localPath).fileName()));
        else if (item.status == qterm2::TransferStatus::Failed)
            m_status->setText(tr("Transferencia fallida: %1").arg(item.error));
        refreshTransfersView();
        if (item.status == qterm2::TransferStatus::Completed && item.isDownload == false)
            refresh();
    });
}

SftpDialog::~SftpDialog()
{
    m_sftp.close();
}

void SftpDialog::ensureEndpoint()
{
    qterm2::TransferEndpoint ep;
    ep.host = m_currentSession.host;
    ep.port = m_currentSession.port;
    ep.user = m_currentSession.user;
    ep.keyFile = qterm2::ssh::expandPath(m_currentSession.privateKey);
    m_queue.setEndpoint(ep);
}

void SftpDialog::setBusy(bool busy, const QString &status)
{
    m_progress->setVisible(busy);
    setCursor(busy ? Qt::WaitCursor : Qt::ArrowCursor);
    if (!status.isEmpty())
        m_status->setText(status);
    QApplication::processEvents();
}

void SftpDialog::connectSelectedSession()
{
    if (!m_sessions) return;
    const QString id = m_sessionCombo->currentData().toString();
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("SFTP"), tr("Selecciona una sesión SSH."));
        return;
    }
    m_currentSession = m_sessions->sessionById(id);
    if (m_currentSession.host.isEmpty()) {
        QMessageBox::warning(this, tr("SFTP"), tr("La sesión no es válida."));
        return;
    }
    setBusy(true, tr("Conectando a %1…").arg(m_currentSession.displayTarget()));
    const QString key = qterm2::ssh::expandPath(m_currentSession.privateKey);
    const bool ok = m_sftp.open(m_currentSession.host, m_currentSession.port, m_currentSession.user, key);
    setBusy(false);
    if (!ok) {
        m_status->setText(tr("Error: %1").arg(m_sftp.lastError()));
        QMessageBox::warning(this, tr("SFTP"), tr("No se pudo conectar: %1").arg(m_sftp.lastError()));
        return;
    }
    ensureEndpoint();
    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_currentPath = m_currentSession.startDir.isEmpty() ? QStringLiteral(".") : qterm2::ssh::expandPath(m_currentSession.startDir);
    if (m_currentPath.isEmpty()) m_currentPath = QStringLiteral(".");
    navigateTo(m_currentPath);
}

void SftpDialog::disconnectSession()
{
    m_sftp.close();
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_table->setRowCount(0);
    m_status->setText(tr("Desconectado."));
}

void SftpDialog::populateTable(const QVector<qterm2::RemoteFile> &files)
{
    m_table->setRowCount(files.size());
    for (int i = 0; i < files.size(); ++i) {
        const auto &f = files[i];
        auto *nameItem = new QTableWidgetItem(f.name);
        nameItem->setData(Qt::UserRole, f.path);
        nameItem->setData(Qt::UserRole + 1, static_cast<int>(f.type));
        m_table->setItem(i, 0, nameItem);
        QString type = tr("Archivo");
        if (f.type == qterm2::FileType::Directory) type = tr("Carpeta");
        else if (f.type == qterm2::FileType::Symlink) type = tr("Enlace");
        m_table->setItem(i, 1, new QTableWidgetItem(type));
        m_table->setItem(i, 2, new QTableWidgetItem(f.type == qterm2::FileType::Directory ? QString() : QString::number(f.size)));
        m_table->setItem(i, 3, new QTableWidgetItem(f.permissions));
        m_table->setItem(i, 4, new QTableWidgetItem(f.owner));
    }
    m_table->resizeColumnsToContents();
}

void SftpDialog::navigateTo(const QString &path)
{
    if (!m_sftp.isOpen()) {
        m_status->setText(tr("No conectado."));
        return;
    }
    QString p = path.trimmed().isEmpty() ? QStringLiteral(".") : path.trimmed();
    setBusy(true, tr("Listando %1…").arg(p));
    const QVector<qterm2::RemoteFile> files = m_sftp.list(p);
    setBusy(false);
    m_currentPath = p;
    m_pathEdit->setText(m_currentPath);
    populateTable(files);
    m_status->setText(tr("%1 elementos en %2").arg(files.size()).arg(m_currentPath));
}

void SftpDialog::refresh() { navigateTo(m_currentPath); }

void SftpDialog::goUp()
{
    if (m_currentPath == QLatin1String("/") || m_currentPath == QLatin1String("."))
        return;
    QString p = m_currentPath;
    if (p.endsWith(QLatin1Char('/')) && p.size() > 1) p.chop(1);
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) p = QStringLiteral("/");
    else p = p.left(slash);
    navigateTo(p);
}

void SftpDialog::onItemActivated(int row, int)
{
    if (row < 0 || !m_table->item(row, 0)) return;
    const int type = m_table->item(row, 0)->data(Qt::UserRole + 1).toInt();
    if (static_cast<qterm2::FileType>(type) == qterm2::FileType::Directory) {
        navigateTo(m_table->item(row, 0)->data(Qt::UserRole).toString());
    }
}

qterm2::RemoteFile SftpDialog::selectedFile() const
{
    const int row = m_table->currentRow();
    qterm2::RemoteFile f;
    if (row < 0 || !m_table->item(row, 0)) return f;
    f.name = m_table->item(row, 0)->text();
    f.path = m_table->item(row, 0)->data(Qt::UserRole).toString();
    f.type = static_cast<qterm2::FileType>(m_table->item(row, 0)->data(Qt::UserRole + 1).toInt());
    return f;
}

void SftpDialog::downloadSelected()
{
    if (!m_sftp.isOpen()) return;
    const qterm2::RemoteFile f = selectedFile();
    if (f.path.isEmpty()) { QMessageBox::information(this, tr("SFTP"), tr("Selecciona un archivo.")); return; }
    if (f.type == qterm2::FileType::Directory) { QMessageBox::information(this, tr("SFTP"), tr("La descarga de carpetas no está soportada aún.")); return; }
    const QString local = QFileDialog::getSaveFileName(this, tr("Guardar como"), QDir::home().filePath(f.name));
    if (local.isEmpty()) return;
    ensureEndpoint();
    m_queue.enqueueDownload(f.path, local);
    m_status->setText(tr("Descarga en cola: %1").arg(f.name));
}

void SftpDialog::uploadFile()
{
    if (!m_sftp.isOpen()) return;
    const QString local = QFileDialog::getOpenFileName(this, tr("Seleccionar archivo para subir"));
    if (local.isEmpty()) return;
    const QString name = QFileInfo(local).fileName();
    QString remote = m_currentPath;
    if (!remote.endsWith(QLatin1Char('/'))) remote += QLatin1Char('/');
    remote += name;
    ensureEndpoint();
    m_queue.enqueueUpload(local, remote);
    m_status->setText(tr("Subida en cola: %1").arg(name));
}

void SftpDialog::makeDirectory()
{
    if (!m_sftp.isOpen()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nueva carpeta"), tr("Nombre:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QString path = m_currentPath;
    if (!path.endsWith(QLatin1Char('/'))) path += QLatin1Char('/');
    path += name.trimmed();
    if (!m_sftp.mkdir(path)) QMessageBox::warning(this, tr("SFTP"), tr("No se pudo crear la carpeta: %1").arg(m_sftp.lastError()));
    refresh();
}

void SftpDialog::removeSelected()
{
    if (!m_sftp.isOpen()) return;
    const qterm2::RemoteFile f = selectedFile();
    if (f.path.isEmpty()) return;
    if (QMessageBox::question(this, tr("Eliminar"), tr("¿Eliminar «%1»?").arg(f.name)) != QMessageBox::Yes)
        return;
    bool ok = (f.type == qterm2::FileType::Directory) ? m_sftp.rmdir(f.path) : m_sftp.rm(f.path);
    if (!ok) QMessageBox::warning(this, tr("SFTP"), tr("No se pudo eliminar: %1").arg(m_sftp.lastError()));
    refresh();
}

void SftpDialog::renameSelected()
{
    if (!m_sftp.isOpen()) return;
    const qterm2::RemoteFile f = selectedFile();
    if (f.path.isEmpty()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Renombrar"), tr("Nuevo nombre:"), QLineEdit::Normal, f.name, &ok);
    if (!ok || name.trimmed().isEmpty() || name == f.name) return;
    QString base = f.path;
    const int slash = base.lastIndexOf(QLatin1Char('/'));
    QString newPath = (slash >= 0 ? base.left(slash + 1) : QString()) + name.trimmed();
    if (!m_sftp.rename(f.path, newPath))
        QMessageBox::warning(this, tr("SFTP"), tr("No se pudo renombrar: %1").arg(m_sftp.lastError()));
    refresh();
}

void SftpDialog::editSelected()
{
    if (!m_sftp.isOpen()) return;
    const qterm2::RemoteFile f = selectedFile();
    if (f.path.isEmpty()) { QMessageBox::information(this, tr("SFTP"), tr("Selecciona un archivo.")); return; }
    if (f.type == qterm2::FileType::Directory) { QMessageBox::information(this, tr("SFTP"), tr("Selecciona un archivo, no una carpeta.")); return; }
    if (f.size > 2 * 1024 * 1024) {
        if (QMessageBox::question(this, tr("Editar remoto"),
                tr("El archivo es grande (%1 bytes). ¿Editar de todos modos?").arg(f.size)) != QMessageBox::Yes)
            return;
    }

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        QMessageBox::warning(this, tr("SFTP"), tr("No se pudo crear un directorio temporal."));
        return;
    }
    const QString localPath = tmpDir.filePath(f.name);
    setBusy(true, tr("Descargando %1 para edición…").arg(f.name));
    const bool dl = m_sftp.get(f.path, localPath);
    setBusy(false);
    if (!dl) {
        QMessageBox::warning(this, tr("SFTP"), tr("No se pudo descargar el archivo: %1").arg(m_sftp.lastError()));
        return;
    }

    QFile lf(localPath);
    QByteArray data;
    if (lf.open(QIODevice::ReadOnly)) {
        data = lf.readAll();
        lf.close();
    }
    // Basic binary heuristic
    if (data.contains('\0')) {
        QMessageBox::warning(this, tr("SFTP"), tr("El archivo parece binario y no se puede editar como texto."));
        return;
    }

    QDialog editor(this);
    editor.setWindowTitle(tr("Editar remoto: %1").arg(f.name));
    editor.resize(800, 560);
    auto *lay = new QVBoxLayout(&editor);
    auto *info = new QLabel(tr("Archivo remoto: %1").arg(f.path), &editor);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(info);
    auto *text = new QPlainTextEdit(&editor);
    text->setPlainText(QString::fromUtf8(data));
    lay->addWidget(text, 1);
    auto *box = new QDialogButtonBox(&editor);
    auto *saveBtn = box->addButton(tr("Guardar en remoto"), QDialogButtonBox::AcceptRole);
    auto *closeBtn = box->addButton(tr("Cerrar"), QDialogButtonBox::RejectRole);
    lay->addWidget(box);

    connect(saveBtn, &QPushButton::clicked, &editor, [&]() {
        QSaveFile sf(localPath);
        if (!sf.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(&editor, tr("SFTP"), tr("No se pudo escribir el archivo temporal."));
            return;
        }
        sf.write(text->toPlainText().toUtf8());
        if (!sf.commit()) {
            QMessageBox::warning(&editor, tr("SFTP"), tr("No se pudo guardar el archivo temporal."));
            return;
        }
        setBusy(true, tr("Subiendo cambios de %1…").arg(f.name));
        const bool up = m_sftp.put(localPath, f.path);
        setBusy(false);
        if (!up) {
            QMessageBox::warning(&editor, tr("SFTP"), tr("No se pudo subir el archivo: %1").arg(m_sftp.lastError()));
            return;
        }
        m_status->setText(tr("Guardado en remoto: %1").arg(f.name));
        editor.accept();
    });
    connect(closeBtn, &QPushButton::clicked, &editor, &QDialog::reject);
    editor.exec();
    refresh();
}

void SftpDialog::refreshTransfersView()
{
    if (!m_transfersList) return;
    m_transfersList->clear();
    const auto items = m_queue.items();
    for (const auto &it : items) {
        QString status;
        switch (it.status) {
        case qterm2::TransferStatus::Queued: status = tr("En cola"); break;
        case qterm2::TransferStatus::InProgress: status = tr("En progreso"); break;
        case qterm2::TransferStatus::Completed: status = tr("Completada"); break;
        case qterm2::TransferStatus::Failed: status = tr("Fallida"); break;
        case qterm2::TransferStatus::Canceled: status = tr("Cancelada"); break;
        }
        const QString dir = it.isDownload ? tr("↓") : tr("↑");
        QString line = QStringLiteral("%1 %2  %3  [%4]")
                           .arg(dir,
                                it.isDownload ? QFileInfo(it.localPath).fileName() : QFileInfo(it.remotePath).fileName(),
                                status);
        if (!it.error.isEmpty())
            line += QStringLiteral(" — ") + it.error;
        auto *lw = new QListWidgetItem(line, m_transfersList);
        lw->setData(Qt::UserRole, it.id);
    }
}

void SftpDialog::showTransfers()
{
    if (!m_transfersDialog) {
        m_transfersDialog = new QDialog(this);
        m_transfersDialog->setWindowTitle(tr("Transferencias SFTP"));
        m_transfersDialog->resize(700, 300);
        auto *lay = new QVBoxLayout(m_transfersDialog);
        m_transfersList = new QListWidget(m_transfersDialog);
        lay->addWidget(m_transfersList, 1);
        auto *row = new QHBoxLayout;
        auto *clearBtn = new QPushButton(tr("Limpiar finalizadas"), m_transfersDialog);
        auto *retryBtn = new QPushButton(tr("Reintentar"), m_transfersDialog);
        auto *cancelBtn = new QPushButton(tr("Cancelar seleccionada"), m_transfersDialog);
        auto *closeBtn = new QPushButton(tr("Cerrar"), m_transfersDialog);
        row->addWidget(clearBtn);
        row->addWidget(retryBtn);
        row->addWidget(cancelBtn);
        row->addStretch();
        row->addWidget(closeBtn);
        lay->addLayout(row);
        connect(clearBtn, &QPushButton::clicked, &m_queue, &qterm2::TransferQueue::clearFinished);
        connect(retryBtn, &QPushButton::clicked, this, [this] {
            if (!m_transfersList) return;
            auto *it = m_transfersList->currentItem();
            if (it) m_queue.retry(it->data(Qt::UserRole).toString());
        });
        connect(cancelBtn, &QPushButton::clicked, this, [this] {
            if (!m_transfersList) return;
            auto *it = m_transfersList->currentItem();
            if (it) m_queue.cancel(it->data(Qt::UserRole).toString());
        });
        connect(closeBtn, &QPushButton::clicked, m_transfersDialog, &QDialog::hide);
    }
    refreshTransfersView();
    m_transfersDialog->show();
    m_transfersDialog->raise();
    m_transfersDialog->activateWindow();
}
