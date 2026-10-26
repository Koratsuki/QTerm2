#include "ui/tunnels/tunneldialog.h"
#include "tunnels/tunnelmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace qterm2 {

TunnelDialog::TunnelDialog(TunnelManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Túneles gestionados (-L/-R/-D)"));
    setMinimumSize(600, 400);

    auto *layout = new QVBoxLayout(this);
    m_table = new QTableView(this);
    layout->addWidget(m_table);

    auto *btnLayout = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Añadir túnel"), this);
    m_startBtn = new QPushButton(tr("Iniciar"), this);
    m_stopBtn = new QPushButton(tr("Detener"), this);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_addBtn, &QPushButton::clicked, this, &TunnelDialog::addTunnel);
    connect(m_startBtn, &QPushButton::clicked, this, &TunnelDialog::startTunnel);
    connect(m_stopBtn, &QPushButton::clicked, this, &TunnelDialog::stopTunnel);
}

void TunnelDialog::addTunnel() {}
void TunnelDialog::startTunnel() {}
void TunnelDialog::stopTunnel() {}

} // namespace qterm2
