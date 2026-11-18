#include "ui/networkdiscoverydialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

NetworkDiscoveryDialog::NetworkDiscoveryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Descubrimiento de red (SSH)"));
    resize(760, 420);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Descubre hosts SSH en la red mediante Avahi/mDNS o nmap (si están instalados)."), this));

    auto *top = new QHBoxLayout;
    m_method = new QComboBox(this);
    m_method->addItem(tr("Avahi: _ssh._tcp (mDNS)"), QStringLiteral("avahi"));
    m_method->addItem(tr("nmap: escaneo puerto 22"), QStringLiteral("nmap"));
    top->addWidget(new QLabel(tr("Método:"), this));
    top->addWidget(m_method);
    top->addWidget(new QLabel(tr("Objetivo:"), this));
    m_target = new QLineEdit(this);
    m_target->setPlaceholderText(tr("Para nmap: ej. 192.168.1.0/24 o 192.168.1.10-50"));
    top->addWidget(m_target, 1);
    m_start = new QPushButton(tr("Iniciar"), this);
    m_stop = new QPushButton(tr("Detener"), this);
    m_stop->setEnabled(false);
    top->addWidget(m_start);
    top->addWidget(m_stop);
    layout->addLayout(top);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Host"), tr("Dirección"), tr("Origen"), tr("Detalles")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *bottom = new QHBoxLayout;
    auto *copyBtn = new QPushButton(tr("Copiar dirección"), this);
    auto *createBtn = new QPushButton(tr("Crear sesión SSH…"), this);
    bottom->addWidget(copyBtn);
    bottom->addWidget(createBtn);
    bottom->addStretch();
    m_status = new QLabel(tr("Listo."), this);
    bottom->addWidget(m_status);
    auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottom->addWidget(closeBox);
    layout->addLayout(bottom);

    m_process = new QProcess(this);
    connect(m_start, &QPushButton::clicked, this, &NetworkDiscoveryDialog::startDiscovery);
    connect(m_stop, &QPushButton::clicked, this, &NetworkDiscoveryDialog::stopDiscovery);
    connect(copyBtn, &QPushButton::clicked, this, &NetworkDiscoveryDialog::copySelected);
    connect(createBtn, &QPushButton::clicked, this, &NetworkDiscoveryDialog::createSessionForSelected);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &NetworkDiscoveryDialog::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &NetworkDiscoveryDialog::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &NetworkDiscoveryDialog::onReadyRead);

    connect(m_method, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const bool nmap = m_method->currentData().toString() == QLatin1String("nmap");
        m_target->setEnabled(nmap);
    });
    m_target->setEnabled(false);
}

NetworkDiscoveryDialog::~NetworkDiscoveryDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString NetworkDiscoveryDialog::buildCommand(QStringList &args) const
{
    const QString method = m_method->currentData().toString();
    if (method == QLatin1String("avahi")) {
        args << QStringLiteral("-tpr") << QStringLiteral("_ssh._tcp");
        return QStringLiteral("avahi-browse");
    }
    // nmap
    const QString target = m_target->text().trimmed();
    args << QStringLiteral("-p") << QStringLiteral("22") << QStringLiteral("--open") << QStringLiteral("-oG") << QStringLiteral("-") << target;
    return QStringLiteral("nmap");
}

void NetworkDiscoveryDialog::appendHost(const QString &host, const QString &address, const QString &source, const QString &details)
{
    if (address.isEmpty() && host.isEmpty())
        return;
    // de-duplicate by address+host
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->text() == host && m_table->item(r, 1)->text() == address)
            return;
    }
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(host));
    m_table->setItem(row, 1, new QTableWidgetItem(address));
    m_table->setItem(row, 2, new QTableWidgetItem(source));
    m_table->setItem(row, 3, new QTableWidgetItem(details));
}

void NetworkDiscoveryDialog::startDiscovery()
{
    if (m_process->state() != QProcess::NotRunning)
        return;
    const QString method = m_method->currentData().toString();
    if (method == QLatin1String("nmap") && m_target->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Descubrimiento de red"), tr("Indica un objetivo para nmap (ej. 192.168.1.0/24)."));
        return;
    }
    m_table->setRowCount(0);
    m_buffer.clear();
    QStringList args;
    const QString prog = buildCommand(args);
    m_status->setText(tr("Ejecutando %1…").arg(prog));
    m_start->setEnabled(false);
    m_stop->setEnabled(true);
    m_process->start(prog, args);
    if (!m_process->waitForStarted(2000)) {
        m_status->setText(tr("No se pudo iniciar %1. ¿Está instalado?").arg(prog));
        m_start->setEnabled(true);
        m_stop->setEnabled(false);
    }
}

void NetworkDiscoveryDialog::stopDiscovery()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_status->setText(tr("Detenido."));
    }
}

void NetworkDiscoveryDialog::onReadyRead()
{
    m_buffer += m_process->readAllStandardOutput();
    m_buffer += m_process->readAllStandardError();
    const QList<QByteArray> lines = m_buffer.split('\n');
    if (lines.isEmpty()) return;
    m_buffer = lines.last();
    for (int i = 0; i < lines.size() - 1; ++i) {
        const QString line = QString::fromUtf8(lines[i]).trimmed();
        if (line.isEmpty()) continue;
        const QString method = m_method->currentData().toString();
        if (method == QLatin1String("avahi")) {
            // avahi-browse -tpr fields: =;iface;proto;name;type;domain;hostname;address;port;txt
            if (line.startsWith(QLatin1Char('='))) {
                const QStringList parts = line.split(QLatin1Char(';'));
                if (parts.size() >= 9) {
                    appendHost(parts.value(6), parts.value(7), tr("Avahi/mDNS"),
                               tr("puerto %1").arg(parts.value(8)));
                }
            }
        } else {
            // nmap grepable: Host: 192.168.1.5 (name)	Status: Up  Ports: 22/open/tcp//ssh///
            QRegularExpression re(QStringLiteral(R"(^Host:\s+(\S+)\s*(?:\(([^)]*)\))?.*Ports:\s*([^ ]*22/open[^ ]*))"));
            auto m = re.match(line);
            if (m.hasMatch()) {
                const QString addr = m.captured(1);
                QString host = m.captured(2).trimmed();
                if (host.isEmpty()) host = addr;
                appendHost(host, addr, tr("nmap"), m.captured(3));
            }
        }
    }
}

void NetworkDiscoveryDialog::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    // process remaining buffer
    if (!m_buffer.trimmed().isEmpty()) {
        const QString line = QString::fromUtf8(m_buffer).trimmed();
        m_buffer.clear();
        // try parse last line
        if (m_method->currentData().toString() == QLatin1String("nmap")) {
            QRegularExpression re(QStringLiteral(R"(^Host:\s+(\S+)\s*(?:\(([^)]*)\))?.*Ports:\s*([^ ]*22/open[^ ]*))"));
            auto m = re.match(line);
            if (m.hasMatch()) {
                const QString addr = m.captured(1);
                QString host = m.captured(2).trimmed();
                if (host.isEmpty()) host = addr;
                appendHost(host, addr, tr("nmap"), m.captured(3));
            }
        }
    }
    m_start->setEnabled(true);
    m_stop->setEnabled(false);
    m_status->setText(tr("Finalizado. %1 host(s) encontrado(s).").arg(m_table->rowCount()));
    m_table->resizeColumnsToContents();
}

void NetworkDiscoveryDialog::copySelected()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString addr = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();
    const QString host = m_table->item(row, 0) ? m_table->item(row, 0)->text() : QString();
    const QString text = !addr.isEmpty() ? addr : host;
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        m_status->setText(tr("Copiado: %1").arg(text));
    }
}

void NetworkDiscoveryDialog::createSessionForSelected()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Descubrimiento de red"), tr("Selecciona un host."));
        return;
    }
    const QString addr = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();
    const QString host = m_table->item(row, 0) ? m_table->item(row, 0)->text() : QString();
    const QString target = !host.isEmpty() ? host : addr;
    if (target.isEmpty() && !addr.isEmpty()) {
        m_selected = {addr, addr};
    } else {
        m_selected = {target, addr};
    }
    const QString chosen = !addr.isEmpty() ? addr : target;
    if (chosen.isEmpty())
        return;
    emit createSessionRequested(chosen);
    accept();
}
