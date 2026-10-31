#include "ui/networkdiscoverydialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>

NetworkDiscoveryDialog::NetworkDiscoveryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Network Discovery (SSH on port 22)"));
    setMinimumSize(400, 300);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Scanning local network for SSH servers...")));
    m_list = new QListWidget(this);
    layout->addWidget(m_list);
    auto *buttons = new QHBoxLayout;
    m_scan = new QPushButton(tr("Scan"), this);
    connect(m_scan, &QPushButton::clicked, this, &NetworkDiscoveryDialog::startScan);
    buttons->addWidget(m_scan);
    buttons->addStretch();
    layout->addLayout(buttons);
    startScan();
}

void NetworkDiscoveryDialog::startScan()
{
    m_list->clear();
    m_scan->setEnabled(false);
    const QStringList hosts = {QStringLiteral("127.0.0.1")};
    for (const auto &h : hosts) {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->connectToHost(h, 22);
        connect(sock, &QTcpSocket::connected, this, [this, h, sock]() {
            m_list->addItem(h + QStringLiteral(":22"));
            sock->deleteLater();
        });
        connect(sock, &QTcpSocket::errorOccurred, this, [sock]() { sock->deleteLater(); });
        QTimer::singleShot(2000, sock, &QTcpSocket::deleteLater);
    }
    QTimer::singleShot(2500, this, [this]() { m_scan->setEnabled(true); });
}
