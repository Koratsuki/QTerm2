#include "ui/sshagentdialog.h"
#include "ui_sshagentdialog.h"

#include <QtConcurrent/QtConcurrentRun>

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

using qterm2::ssh::AgentIdentity;
using qterm2::ssh::AgentStatus;

SshAgentDialog::SshAgentDialog(const AgentStatus &status, QWidget *parent,
                               bool queryOnOpen)
    : QDialog(parent)
    , ui(new Ui::SshAgentDialog)
    , m_status(status)
{
    ui->setupUi(this);
    setWindowTitle(tr("Agente SSH"));
    resize(820, 460);

    // Find widgets from the UI or keep members for compatibility
    m_socket = findChild<QLabel *>(QStringLiteral("AgentSocketLabel"));
    if (!m_socket) {
        m_socket = new QLabel(this);
        m_socket->setObjectName(QStringLiteral("AgentSocketLabel"));
        m_socket->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    m_state = findChild<QLabel *>(QStringLiteral("AgentStatusLabel"));
    if (!m_state) {
        m_state = new QLabel(this);
        m_state->setObjectName(QStringLiteral("AgentStatusLabel"));
        m_state->setWordWrap(true);
    }
    m_table = findChild<QTableWidget *>(QStringLiteral("AgentTable"));
    if (!m_table) {
        m_table = new QTableWidget(0, 4, this);
    }
    m_refresh = findChild<QPushButton *>(QStringLiteral("RefreshAgentButton"));

    if (m_table) {
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({ tr("Bits"), tr("Huella"),
                                             tr("Tipo"), tr("Comentario") });
        m_table->verticalHeader()->setVisible(false);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setAlternatingRowColors(true);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    }

    if (ui->buttonBox) {
        if (ui->buttonBox->button(QDialogButtonBox::Close)) {
            ui->buttonBox->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
        }
        connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    if (m_refresh) {
        connect(m_refresh, &QPushButton::clicked, this, &SshAgentDialog::refresh);
    }

    applyStatus(status);
    if (queryOnOpen)
        startQuery();
}

SshAgentDialog::~SshAgentDialog()
{
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
        delete m_watcher;
    }
    delete ui;
}

void SshAgentDialog::startQuery()
{
    if (m_watcher) // a query is already in flight
        return;
    if (m_refresh)
        m_refresh->setEnabled(false);
    if (m_state)
        m_state->setText(tr("Consultando el agente SSH…"));

    m_watcher = new QFutureWatcher<AgentStatus>(this);
    connect(m_watcher, &QFutureWatcher<AgentStatus>::finished,
            this, &SshAgentDialog::onQueryFinished);
    m_watcher->setFuture(QtConcurrent::run([] { return qterm2::ssh::queryAgent(); }));
}

void SshAgentDialog::onQueryFinished()
{
    if (!m_watcher)
        return;
    const AgentStatus status = m_watcher->result();
    m_watcher->deleteLater();
    m_watcher = nullptr;
    if (m_refresh)
        m_refresh->setEnabled(true);
    applyStatus(status);
}

void SshAgentDialog::applyStatus(const AgentStatus &status)
{
    m_status = status;
    if (m_socket) {
        m_socket->setText(status.socketPath.isEmpty()
                              ? tr("(SSH_AUTH_SOCK no definida)")
                              : status.socketPath);
    }

    if (m_state) {
        if (status.available) {
            m_state->setText(status.identities.isEmpty()
                                 ? tr("Agente disponible, sin identidades cargadas.")
                                 : tr("Agente disponible · %1 identidades.").arg(status.identities.size()));
        } else {
            m_state->setText(status.error.isEmpty()
                                 ? tr("Agente SSH no disponible.")
                                 : tr("Agente SSH no disponible: %1").arg(status.error));
        }
    }

    if (m_table) {
        m_table->setRowCount(status.identities.size());
        for (int i = 0; i < status.identities.size(); ++i) {
            const AgentIdentity &identity = status.identities.at(i);
            m_table->setItem(i, 0, new QTableWidgetItem(identity.bits));
            m_table->setItem(i, 1, new QTableWidgetItem(identity.fingerprint));
            m_table->setItem(i, 2, new QTableWidgetItem(identity.keyType));
            m_table->setItem(i, 3, new QTableWidgetItem(identity.comment));
        }
        m_table->resizeRowsToContents();
    }
}

void SshAgentDialog::refresh()
{
    // queryAgent() waits up to ~11 s on ssh-add; never run it on the GUI thread.
    startQuery();
}
