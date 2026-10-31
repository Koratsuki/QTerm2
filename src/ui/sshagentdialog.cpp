#include "ui/sshagentdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

using qterm2::ssh::AgentIdentity;
using qterm2::ssh::AgentStatus;

SshAgentDialog::SshAgentDialog(const AgentStatus &status, QWidget *parent)
    : QDialog(parent)
    , m_status(status)
{
    setWindowTitle(tr("SSH Agent"));
    resize(820, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "Identidades que OpenSSH puede usar sin volver a pedir la llave. Se "
        "cargan y retiran con «ssh-add»; QTerm2 solo las muestra. Las llaves "
        "con passphrase las desbloquea el agente, nunca QTerm2."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *socketRow = new QHBoxLayout;
    socketRow->addWidget(new QLabel(tr("Socket:"), this));
    m_socket = new QLabel(this);
    m_socket->setObjectName(QStringLiteral("AgentSocketLabel"));
    m_socket->setTextInteractionFlags(Qt::TextSelectableByMouse);
    socketRow->addWidget(m_socket, 1);
    layout->addLayout(socketRow);

    m_state = new QLabel(this);
    m_state->setObjectName(QStringLiteral("AgentStatusLabel"));
    m_state->setWordWrap(true);
    layout->addWidget(m_state);

    m_table = new QTableWidget(0, 4, this);
    m_table->setObjectName(QStringLiteral("AgentTable"));
    m_table->setHorizontalHeaderLabels({ tr("Bits"), tr("Huella"),
                                         tr("Type"), tr("Comentario") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);

    auto *bottom = new QHBoxLayout;
    m_refresh = new QPushButton(tr("Volver a consultar"), this);
    m_refresh->setObjectName(QStringLiteral("RefreshAgentButton"));
    bottom->addWidget(m_refresh);
    bottom->addStretch(1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    box->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    bottom->addWidget(box);
    layout->addLayout(bottom);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &SshAgentDialog::refresh);

    applyStatus(status);
}

void SshAgentDialog::applyStatus(const AgentStatus &status)
{
    m_status = status;
    m_socket->setText(status.socketPath.isEmpty()
                          ? tr("(SSH_AUTH_SOCK no definida)")
                          : status.socketPath);

    if (status.available) {
        m_state->setText(status.identities.isEmpty()
                             ? tr("Agente disponible, sin identidades cargadas.")
                             : tr("Agente disponible · %1 identidades.")
                                   .arg(status.identities.size()));
    } else {
        m_state->setText(status.error.isEmpty()
                             ? tr("Agente SSH no disponible.")
                             : tr("Agente SSH no disponible: %1").arg(status.error));
    }

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

void SshAgentDialog::refresh()
{
    applyStatus(qterm2::ssh::queryAgent());
}
