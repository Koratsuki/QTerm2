#include "ui/tunnels/tunneldialog.h"

#include "sessions/sessionstore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

class ManualTunnelDialog : public QDialog
{
public:
    explicit ManualTunnelDialog(qterm2::SessionStore *sessions, QWidget *parent = nullptr,
                                const qterm2::TunnelConfig *existing = nullptr)
        : QDialog(parent), m_sessions(sessions), m_existing(existing ? *existing : qterm2::TunnelConfig{})
    {
        setWindowTitle(m_existing.id.isEmpty() ? tr("Nuevo túnel SSH") : tr("Editar túnel SSH"));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;

        m_name = new QLineEdit(this);
        m_session = new QComboBox(this);
        m_session->addItem(tr("(Conexión manual)"), QString());
        if (m_sessions) {
            for (const Session &s : m_sessions->sessions()) {
                if (!s.isLocal())
                    m_session->addItem(QStringLiteral("%1 (%2)").arg(s.name, s.displayTarget()), s.id);
            }
        }
        m_host = new QLineEdit(this);
        m_port = new QSpinBox(this); m_port->setRange(1, 65535); m_port->setValue(22);
        m_user = new QLineEdit(this);
        m_type = new QComboBox(this);
        m_type->addItem(tr("Local (-L)"), static_cast<int>(PortForward::Type::Local));
        m_type->addItem(tr("Remoto (-R)"), static_cast<int>(PortForward::Type::Remote));
        m_type->addItem(tr("Dinámico (-D)"), static_cast<int>(PortForward::Type::Dynamic));
        m_listenHost = new QLineEdit(QStringLiteral("127.0.0.1"), this);
        m_listenPort = new QSpinBox(this); m_listenPort->setRange(1, 65535); m_listenPort->setValue(8080);
        m_targetHost = new QLineEdit(QStringLiteral("127.0.0.1"), this);
        m_targetPort = new QSpinBox(this); m_targetPort->setRange(1, 65535); m_targetPort->setValue(80);
        m_autoReconnect = new QCheckBox(tr("Reconectar automáticamente"), this);

        form->addRow(tr("Nombre"), m_name);
        form->addRow(tr("Sesión base"), m_session);
        form->addRow(tr("Host SSH"), m_host);
        form->addRow(tr("Puerto SSH"), m_port);
        form->addRow(tr("Usuario"), m_user);
        form->addRow(tr("Tipo"), m_type);
        form->addRow(tr("Host escucha"), m_listenHost);
        form->addRow(tr("Puerto escucha"), m_listenPort);
        form->addRow(tr("Host destino"), m_targetHost);
        form->addRow(tr("Puerto destino"), m_targetPort);
        form->addRow(QString(), m_autoReconnect);
        layout->addLayout(form);

        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(box);

        connect(m_session, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { applySession(); });
        connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            const bool dynamic = m_type->currentData().toInt() == static_cast<int>(PortForward::Type::Dynamic);
            m_targetHost->setEnabled(!dynamic);
            m_targetPort->setEnabled(!dynamic);
        });
        applySession();
        loadExisting();
    }

    qterm2::TunnelConfig config() const
    {
        qterm2::TunnelConfig c = m_existing;
        c.name = m_name->text().trimmed();
        const QString sid = m_session->currentData().toString();
        if (!sid.isEmpty() && m_sessions && m_existing.id.isEmpty()) {
            const Session s = m_sessions->sessionById(sid);
            c.host = s.host; c.port = s.port; c.user = s.user;
            c.auth = s.auth; c.privateKey = s.privateKey; c.proxyJump = s.proxyJump;
            c.hostKeyPolicy = s.hostKeyPolicy; c.keepAlive = s.keepAlive; c.connectTimeout = s.connectTimeout;
        }
        if (!m_host->text().trimmed().isEmpty() || m_host->isEnabled()) c.host = m_host->text().trimmed();
        c.port = m_port->value();
        if (!m_user->text().trimmed().isEmpty() || m_user->isEnabled()) c.user = m_user->text().trimmed();
        c.forward.type = static_cast<PortForward::Type>(m_type->currentData().toInt());
        c.forward.listenHost = m_listenHost->text().trimmed();
        c.forward.listenPort = m_listenPort->value();
        c.forward.targetHost = m_targetHost->text().trimmed();
        c.forward.targetPort = m_targetPort->value();
        c.forward.enabled = true;
        c.autoReconnect = m_autoReconnect->isChecked();
        if (c.name.isEmpty())
            c.name = QStringLiteral("%1 %2:%3").arg(PortForward::typeName(c.forward.type)).arg(c.forward.listenPort).arg(c.host);
        return c;
    }

private:
    void applySession()
    {
        const QString sid = m_session->currentData().toString();
        if (sid.isEmpty() || !m_sessions) {
            m_host->setEnabled(true); m_port->setEnabled(true); m_user->setEnabled(true);
            return;
        }
        const Session s = m_sessions->sessionById(sid);
        m_host->setText(s.host);
        m_port->setValue(s.port);
        m_user->setText(s.user);
        m_host->setEnabled(false); m_port->setEnabled(false); m_user->setEnabled(false);
        if (m_name->text().trimmed().isEmpty())
            m_name->setText(s.name);
    }

    void loadExisting()
    {
        if (m_existing.id.isEmpty())
            return;
        m_session->setCurrentIndex(0); // manual for editing
        m_name->setText(m_existing.name);
        m_host->setText(m_existing.host);
        m_port->setValue(m_existing.port > 0 ? m_existing.port : 22);
        m_user->setText(m_existing.user);
        const int typeIdx = m_type->findData(static_cast<int>(m_existing.forward.type));
        if (typeIdx >= 0) m_type->setCurrentIndex(typeIdx);
        m_listenHost->setText(m_existing.forward.listenHost.isEmpty() ? QStringLiteral("127.0.0.1") : m_existing.forward.listenHost);
        m_listenPort->setValue(m_existing.forward.listenPort > 0 ? m_existing.forward.listenPort : 8080);
        m_targetHost->setText(m_existing.forward.targetHost.isEmpty() ? QStringLiteral("127.0.0.1") : m_existing.forward.targetHost);
        m_targetPort->setValue(m_existing.forward.targetPort > 0 ? m_existing.forward.targetPort : 80);
        m_autoReconnect->setChecked(m_existing.autoReconnect);
        m_host->setEnabled(true); m_port->setEnabled(true); m_user->setEnabled(true);
        const bool dynamic = m_type->currentData().toInt() == static_cast<int>(PortForward::Type::Dynamic);
        m_targetHost->setEnabled(!dynamic);
        m_targetPort->setEnabled(!dynamic);
    }

    qterm2::SessionStore *m_sessions = nullptr;
    qterm2::TunnelConfig m_existing;
    QLineEdit *m_name = nullptr;
    QComboBox *m_session = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QComboBox *m_type = nullptr;
    QLineEdit *m_listenHost = nullptr;
    QSpinBox *m_listenPort = nullptr;
    QLineEdit *m_targetHost = nullptr;
    QSpinBox *m_targetPort = nullptr;
    QCheckBox *m_autoReconnect = nullptr;
};

} // namespace

TunnelDialog::TunnelDialog(qterm2::TunnelManager *manager,
                           qterm2::SessionStore *sessions,
                           QWidget *parent)
    : QDialog(parent), m_manager(manager), m_sessions(sessions)
{
    setWindowTitle(tr("Túneles SSH"));
    resize(880, 400);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Gestiona reenvíos de puertos SSH (-L/-R/-D) como procesos independientes."), this));

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({tr("Nombre"), tr("Tipo"), tr("Escucha"),
                                        tr("Destino"), tr("Host"), tr("Auto"), tr("Estado")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *buttons = new QHBoxLayout;
    auto *addSessionBtn = new QPushButton(tr("Desde sesión…"), this);
    auto *addManualBtn = new QPushButton(tr("Manual…"), this);
    auto *editBtn = new QPushButton(tr("Editar"), this);
    auto *startBtn = new QPushButton(tr("Iniciar"), this);
    auto *stopBtn = new QPushButton(tr("Detener"), this);
    auto *autoBtn = new QPushButton(tr("Auto-reconectar"), this);
    auto *logBtn = new QPushButton(tr("Ver log"), this);
    auto *removeBtn = new QPushButton(tr("Eliminar"), this);
    buttons->addWidget(addSessionBtn);
    buttons->addWidget(addManualBtn);
    buttons->addWidget(editBtn);
    buttons->addWidget(startBtn);
    buttons->addWidget(stopBtn);
    buttons->addWidget(autoBtn);
    buttons->addWidget(logBtn);
    buttons->addWidget(removeBtn);
    buttons->addStretch();
    auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addWidget(closeBox);
    layout->addLayout(buttons);

    connect(addSessionBtn, &QPushButton::clicked, this, &TunnelDialog::addFromSession);
    connect(addManualBtn, &QPushButton::clicked, this, &TunnelDialog::addManual);
    connect(editBtn, &QPushButton::clicked, this, &TunnelDialog::editSelected);
    connect(startBtn, &QPushButton::clicked, this, &TunnelDialog::startSelected);
    connect(stopBtn, &QPushButton::clicked, this, &TunnelDialog::stopSelected);
    connect(autoBtn, &QPushButton::clicked, this, &TunnelDialog::toggleAutoReconnectSelected);
    connect(logBtn, &QPushButton::clicked, this, &TunnelDialog::showLog);
    connect(removeBtn, &QPushButton::clicked, this, &TunnelDialog::removeSelected);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (m_manager) {
        connect(m_manager, &qterm2::TunnelManager::tunnelsChanged, this, &TunnelDialog::refreshTable);
        connect(m_manager, &qterm2::TunnelManager::tunnelStateChanged, this,
                [this](const QString &, qterm2::TunnelState, const QString &) { refreshTable(); });
        connect(m_manager, &qterm2::TunnelManager::tunnelLogAppended, this,
                [this](const QString &, const QString &) { /* log dialog reads live when opened */ });
    }
    refreshTable();
}

QString TunnelDialog::selectedTunnelId() const
{
    if (!m_table) return {};
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 6)) return {};
    return m_table->item(row, 6)->data(Qt::UserRole).toString();
}

void TunnelDialog::refreshTable()
{
    if (!m_table || !m_manager)
        return;
    const auto tunnels = m_manager->tunnels();
    const QString prev = selectedTunnelId();
    m_table->setRowCount(tunnels.size());
    int restoreRow = -1;
    for (int row = 0; row < tunnels.size(); ++row) {
        const auto &rt = tunnels[row];
        const auto &c = rt.config;
        if (c.id == prev) restoreRow = row;
        m_table->setItem(row, 0, new QTableWidgetItem(c.name));
        m_table->setItem(row, 1, new QTableWidgetItem(PortForward::typeName(c.forward.type)));
        const QString listen = QStringLiteral("%1:%2").arg(
            c.forward.listenHost.isEmpty() ? QStringLiteral("127.0.0.1") : c.forward.listenHost)
                                   .arg(c.forward.listenPort);
        m_table->setItem(row, 2, new QTableWidgetItem(listen));
        QString target = tr("dinámico");
        if (c.forward.type != PortForward::Type::Dynamic)
            target = QStringLiteral("%1:%2").arg(c.forward.targetHost).arg(c.forward.targetPort);
        m_table->setItem(row, 3, new QTableWidgetItem(target));
        m_table->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1@%2:%3").arg(c.user, c.host).arg(c.port)));
        m_table->setItem(row, 5, new QTableWidgetItem(c.autoReconnect ? tr("Sí") : tr("No")));
        QString state = qterm2::TunnelManager::stateName(rt.state);
        if (!rt.lastError.isEmpty())
            state += QStringLiteral(" — ") + rt.lastError.left(80);
        auto *item = new QTableWidgetItem(state);
        item->setData(Qt::UserRole, c.id);
        m_table->setItem(row, 6, item);
    }
    if (restoreRow >= 0)
        m_table->selectRow(restoreRow);
    m_table->resizeColumnsToContents();
}

void TunnelDialog::addFromSession()
{
    if (!m_manager || !m_sessions)
        return;
    const QList<Session> sessions = m_sessions->sessions();
    QStringList names;
    QVector<QPair<QString, PortForward>> candidates;
    for (const Session &s : sessions) {
        if (s.isLocal())
            continue;
        for (const PortForward &f : s.portForwards) {
            if (!f.enabled) continue;
            names << QStringLiteral("%1 — %2 %3:%4").arg(s.name, PortForward::typeName(f.type))
                         .arg(f.listenHost.isEmpty() ? QStringLiteral("127.0.0.1") : f.listenHost)
                         .arg(f.listenPort);
            candidates.append({s.id, f});
        }
    }
    if (names.isEmpty()) {
        QMessageBox::information(this, tr("Túneles SSH"),
                                 tr("No hay reenvíos de puertos configurados en sesiones SSH.\n"
                                    "Añade reenvíos en el editor de sesión o crea un túnel manual."));
        return;
    }
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Agregar túnel"),
                                                 tr("Reenvío:"), names, 0, false, &ok);
    if (!ok) return;
    const int idx = names.indexOf(choice);
    if (idx < 0 || idx >= candidates.size()) return;
    const Session s = m_sessions->sessionById(candidates[idx].first);
    qterm2::TunnelConfig cfg = qterm2::TunnelManager::fromSessionForward(s, candidates[idx].second);
    cfg.name = s.name + QStringLiteral(" · ") + PortForward::typeName(candidates[idx].second.type)
               + QStringLiteral(" ") + QString::number(candidates[idx].second.listenPort);
    m_manager->addTunnel(cfg);
}

void TunnelDialog::addManual()
{
    if (!m_manager) return;
    ManualTunnelDialog dlg(m_sessions, this);
    if (dlg.exec() != QDialog::Accepted) return;
    qterm2::TunnelConfig cfg = dlg.config();
    if (cfg.host.trimmed().isEmpty() || cfg.forward.listenPort < 1) {
        QMessageBox::warning(this, tr("Túnel SSH"), tr("Completa al menos host y puerto de escucha."));
        return;
    }
    m_manager->addTunnel(cfg);
}

void TunnelDialog::editSelected()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (id.isEmpty()) return;
    qterm2::TunnelConfig existing;
    bool found = false;
    for (const auto &rt : m_manager->tunnels()) {
        if (rt.config.id == id) { existing = rt.config; found = true; break; }
    }
    if (!found) return;
    ManualTunnelDialog dlg(m_sessions, this, &existing);
    if (dlg.exec() != QDialog::Accepted) return;
    qterm2::TunnelConfig cfg = dlg.config();
    cfg.id = existing.id;
    if (cfg.host.trimmed().isEmpty() || cfg.forward.listenPort < 1) {
        QMessageBox::warning(this, tr("Túnel SSH"), tr("Completa al menos host y puerto de escucha."));
        return;
    }
    m_manager->updateTunnel(cfg);
}

void TunnelDialog::startSelected()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (id.isEmpty()) return;
    QString err;
    if (!m_manager->startTunnel(id, &err) && !err.isEmpty())
        QMessageBox::warning(this, tr("Túnel SSH"), err);
}

void TunnelDialog::stopSelected()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (!id.isEmpty()) m_manager->stopTunnel(id);
}

void TunnelDialog::removeSelected()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (!id.isEmpty()) m_manager->removeTunnel(id);
}

void TunnelDialog::toggleAutoReconnectSelected()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (id.isEmpty()) return;
    for (const auto &rt : m_manager->tunnels()) {
        if (rt.config.id == id) {
            qterm2::TunnelConfig cfg = rt.config;
            cfg.autoReconnect = !cfg.autoReconnect;
            m_manager->updateTunnel(cfg);
            break;
        }
    }
}

void TunnelDialog::showLog()
{
    if (!m_manager) return;
    const QString id = selectedTunnelId();
    if (id.isEmpty()) return;
    QString text = tr("(sin registro)");
    for (const auto &rt : m_manager->tunnels()) {
        if (rt.config.id == id) { text = rt.log.isEmpty() ? tr("(sin registro)") : rt.log; break; }
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Registro del túnel"));
    dlg.resize(760, 420);
    auto *lay = new QVBoxLayout(&dlg);
    auto *edit = new QPlainTextEdit(&dlg);
    edit->setReadOnly(true);
    edit->setPlainText(text);
    lay->addWidget(edit);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(box);
    dlg.exec();
}
