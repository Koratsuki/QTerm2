#include "ui/sftp/transferpanel.h"
#include "sftp/transferqueue.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace qterm2 {

TransferPanel::TransferPanel(TransferQueue *queue, QWidget *parent)
    : QWidget(parent)
    , m_queue(queue)
{
    auto *layout = new QVBoxLayout(this);
    m_table = new QTableView(this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table);

    auto *btnLayout = new QHBoxLayout;
    m_pauseBtn = new QPushButton(tr("Pausar"), this);
    m_resumeBtn = new QPushButton(tr("Reanudar"), this);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_resumeBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_pauseBtn, &QPushButton::clicked, this, &TransferPanel::pauseTransfer);
    connect(m_resumeBtn, &QPushButton::clicked, this, &TransferPanel::resumeTransfer);
    connect(m_cancelBtn, &QPushButton::clicked, this, &TransferPanel::cancelTransfer);
}

void TransferPanel::pauseTransfer() {}
void TransferPanel::resumeTransfer() {}
void TransferPanel::cancelTransfer() {}

} // namespace qterm2
