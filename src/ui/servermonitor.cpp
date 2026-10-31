#include "ui/servermonitor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

ServerMonitor::ServerMonitor(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Server Monitoring"));
    setMinimumSize(400, 300);
    auto *layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    layout->addWidget(m_list);
    auto *buttons = new QHBoxLayout;
    m_refresh = new QPushButton(tr("Refresh"), this);
    connect(m_refresh, &QPushButton::clicked, this, &ServerMonitor::refresh);
    buttons->addWidget(m_refresh);
    buttons->addStretch();
    layout->addLayout(buttons);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ServerMonitor::refresh);
    m_timer->start(5000);
    refresh();
}

void ServerMonitor::refresh()
{
    m_list->clear();
    m_list->addItem(tr("CPU: N/A"));
    m_list->addItem(tr("Memory: N/A"));
    m_list->addItem(tr("Disk: N/A"));
    m_list->addItem(tr("Processes: N/A"));
}
