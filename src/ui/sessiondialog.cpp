#include "ui/sessiondialog.h"

#include "ssh/keymanager.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {

QIcon colorIcon(const QString &colorTag)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Session::colorForTag(colorTag)));
    p.drawEllipse(1, 1, 12, 12);
    p.end();
    return QIcon(pm);
}

QString colorLabel(const QString &tag)
{
    if (tag == QLatin1String("green"))  return QObject::tr("Verde");
    if (tag == QLatin1String("purple")) return QObject::tr("Morado");
    if (tag == QLatin1String("orange")) return QObject::tr("Naranja");
    if (tag == QLatin1String("red"))    return QObject::tr("Rojo");
    if (tag == QLatin1String("yellow")) return QObject::tr("Amarillo");
    if (tag == QLatin1String("gray"))   return QObject::tr("Gris");
    return QObject::tr("Azul");
}

QWidget *rowWidget(std::initializer_list<QWidget *> widgets, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (QWidget *widget : widgets)
        layout->addWidget(widget);
    return w;
}

// ------------------------------------------------------------------ keygen --

class KeyGenDialog : public QDialog
{
public:
    KeyGenDialog(const Session &session, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Generar llave SSH ed25519"));
        setMinimumWidth(520);

        auto *layout = new QVBoxLayout(this);
        auto *intro = new QLabel(tr(
            "Se creará una llave ed25519 en tu carpeta ~/.ssh. "
            "La llave privada nunca sale de este equipo; la pública se puede "
            "instalar en el servidor desde el menú de la sesión."), this);
        intro->setWordWrap(true);
        intro->setObjectName(QStringLiteral("Hint"));
        layout->addWidget(intro);

        auto *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        QString suggested = qterm2::ssh::KeyManager::suggestedKeyPath(session.user, session.host);
        QFileInfo fi(suggested);
        int suffix = 2;
        while (QFileInfo::exists(suggested)
               || QFileInfo::exists(suggested + QStringLiteral(".pub"))) {
            suggested = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                        + QStringLiteral("_%1").arg(suffix++);
        }

        m_path = new QLineEdit(suggested, this);
        auto *browse = new QPushButton(tr("Examinar…"), this);
        form->addRow(tr("Ruta de la llave"), rowWidget({ m_path, browse }, this));

        m_comment = new QLineEdit(this);
        const QString who = session.user.isEmpty() ? QString::fromLocal8Bit(qgetenv("USER")) : session.user;
        m_comment->setText(QStringLiteral("%1@%2").arg(who,
                            session.host.isEmpty() ? QStringLiteral("qterm2") : session.host));
        form->addRow(tr("Comentario"), m_comment);

        m_passphrase = new QLineEdit(this);
        m_passphrase->setEchoMode(QLineEdit::Password);
        m_passphrase->setPlaceholderText(tr("Recomendado; puede quedar vacío"));
        form->addRow(tr("Passphrase"), m_passphrase);

        m_confirm = new QLineEdit(this);
        m_confirm->setEchoMode(QLineEdit::Password);
        form->addRow(tr("Repetir passphrase"), m_confirm);

        auto *show = new QCheckBox(tr("Mostrar passphrase"), this);
        form->addRow(QString(), show);
        layout->addLayout(form);

        auto *warning = new QLabel(tr(
            "Sin passphrase la llave queda protegida únicamente por los permisos "
            "del archivo (0600). ssh-agent evita escribirla en cada conexión."), this);
        warning->setWordWrap(true);
        warning->setObjectName(QStringLiteral("Hint"));
        layout->addWidget(warning);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
        auto *create = buttons->addButton(tr("Generar llave"), QDialogButtonBox::AcceptRole);
        create->setDefault(true);
        layout->addWidget(buttons);

        connect(show, &QCheckBox::toggled, this, [this](bool on) {
            const QLineEdit::EchoMode mode = on ? QLineEdit::Normal : QLineEdit::Password;
            m_passphrase->setEchoMode(mode);
            m_confirm->setEchoMode(mode);
        });
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString file = QFileDialog::getSaveFileName(
                this, tr("Guardar llave privada"), m_path->text(),
                tr("Llaves SSH (*)"));
            if (!file.isEmpty())
                m_path->setText(file);
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(create, &QPushButton::clicked, this, &KeyGenDialog::accept);
    }

    QString keyPath() const { return m_path->text().trimmed(); }
    QString comment() const { return m_comment->text().trimmed(); }

    void accept() override
    {
        const QString path = keyPath();
        if (path.isEmpty()) {
            QMessageBox::warning(this, tr("Falta la ruta"),
                                 tr("Elige dónde guardar la llave privada."));
            return;
        }
        if (QFileInfo::exists(path) || QFileInfo::exists(path + QStringLiteral(".pub"))) {
            QMessageBox::warning(this, tr("La llave ya existe"),
                                 tr("Ya hay un archivo en:\n%1").arg(path));
            return;
        }
        if (m_passphrase->text() != m_confirm->text()) {
            QMessageBox::warning(this, tr("No coinciden"),
                                 tr("La passphrase y su confirmación son diferentes."));
            return;
        }
        QString error;
        if (!qterm2::ssh::KeyManager::generateEd25519(path, comment(), m_passphrase->text(), &error)) {
            QMessageBox::critical(this, tr("No se pudo generar la llave"), error);
            return;
        }
        QDialog::accept();
    }

private:
    QLineEdit *m_path = nullptr;
    QLineEdit *m_comment = nullptr;
    QLineEdit *m_passphrase = nullptr;
    QLineEdit *m_confirm = nullptr;
};

} // namespace

// --------------------------------------------------------------- SessionDialog

SessionDialog::SessionDialog(const Session &session, const QStringList &groups,
                             QWidget *parent)
    : QDialog(parent)
    , m_session(session)
    , m_result(session)
    , m_groups(groups)
{
    setWindowTitle(session.id.isEmpty() ? tr("Nueva conexión") : tr("Editar sesión"));
    setMinimumSize(720, 640);
    resize(780, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    layout->addWidget(m_tabs, 1);

    buildGeneralTab();
    buildAuthTab();
    buildAdvancedTab();
    buildForwardsTab();
    buildTerminalTab();

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *save = m_buttons->addButton(tr("Guardar sesión"), QDialogButtonBox::AcceptRole);
    save->setDefault(true);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &SessionDialog::accept);

    load(session);
    updateVisibility();
    updateAuthUi();
}

void SessionDialog::buildGeneralTab()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setSpacing(9);

    m_name = new QLineEdit(page);
    m_name->setPlaceholderText(tr("Producción, NAS de casa, Laboratorio…"));
    form->addRow(tr("Nombre"), m_name);

    m_group = new QComboBox(page);
    m_group->setEditable(true);
    m_group->addItems(m_groups);
    if (!m_groups.contains(QStringLiteral("Servidores")))
        m_group->addItem(QStringLiteral("Servidores"));
    if (!m_groups.contains(QStringLiteral("Locales")))
        m_group->addItem(QStringLiteral("Locales"));
    form->addRow(tr("Grupo"), m_group);

    m_type = new QComboBox(page);
    m_type->addItem(tr("Servidor SSH"), QStringLiteral("ssh"));
    m_type->addItem(tr("Telnet"), QStringLiteral("telnet"));
    m_type->addItem(tr("Terminal local"), QStringLiteral("local"));
    form->addRow(tr("Tipo"), m_type);

    m_color = new QComboBox(page);
    fillColorCombo();
    form->addRow(tr("Color"), m_color);

    m_favorite = new QCheckBox(tr("Marcar como favorito"), page);
    m_favorite->setObjectName(QStringLiteral("FavoriteCheck"));
    m_favorite->setToolTip(tr("Los favoritos aparecen en un grupo propio en el panel lateral."));
    form->addRow(QString(), m_favorite);

    m_sshBox = new QWidget(page);
    auto *sshForm = new QFormLayout(m_sshBox);
    sshForm->setContentsMargins(0, 0, 0, 0);
    sshForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    sshForm->setSpacing(9);
    m_host = new QLineEdit(m_sshBox);
    m_host->setPlaceholderText(tr("servidor.ejemplo.com o 192.168.1.20"));
    sshForm->addRow(tr("Servidor"), m_host);

    m_port = new QSpinBox(m_sshBox);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    sshForm->addRow(tr("Puerto"), m_port);

    m_user = new QLineEdit(m_sshBox);
    m_user->setPlaceholderText(tr("usuario remoto"));
    sshForm->addRow(tr("Usuario"), m_user);
    form->addRow(m_sshBox);

    m_localBox = new QWidget(page);
    auto *localForm = new QFormLayout(m_localBox);
    localForm->setContentsMargins(0, 0, 0, 0);
    localForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    localForm->setSpacing(9);
    m_shell = new QComboBox(m_localBox);
    m_shell->setEditable(true);
    m_shell->addItems({ QStringLiteral("/bin/zsh"), QStringLiteral("/bin/bash"),
                        QStringLiteral("/bin/fish"), QStringLiteral("/bin/sh") });
    localForm->addRow(tr("Shell"), m_shell);
    form->addRow(m_localBox);

    m_startDir = new QLineEdit(page);
    m_startDir->setPlaceholderText(tr("~ (por defecto)"));
    form->addRow(tr("Directorio inicial"), m_startDir);

    outer->addLayout(form);

    auto *hint = new QLabel(tr(
        "Consejo: guarda varias sesiones y agrupa tus servidores. El panel "
        "lateral se oculta o muestra con Ctrl+B."), page);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("Hint"));
    outer->addWidget(hint);
    outer->addStretch(1);

    m_tabs->addTab(page, tr("General"));
}

void SessionDialog::buildAuthTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(9);

    m_auth = new QComboBox(page);
    m_auth->addItem(tr("Llave pública (recomendado)"), QStringLiteral("publickey"));
    m_auth->addItem(tr("Agente SSH (ssh-agent)"), QStringLiteral("agent"));
    m_auth->addItem(tr("Contraseña (se pide en la terminal)"), QStringLiteral("password"));
    form->addRow(tr("Método"), m_auth);

    m_key = new QLineEdit(page);
    m_key->setPlaceholderText(tr("~/.ssh/id_ed25519"));
    m_browseKey = new QPushButton(tr("Examinar…"), page);
    m_generateKey = new QPushButton(tr("Generar…"), page);
    m_keyRow = rowWidget({ m_key, m_browseKey, m_generateKey }, page);
    form->addRow(tr("Llave privada"), m_keyRow);

    m_showKey = new QPushButton(tr("Ver llave pública"), page);
    m_copyKey = new QPushButton(tr("Copiar llave pública"), page);
    form->addRow(QString(), rowWidget({ m_showKey, m_copyKey }, page));

    m_keyStatus = new QLabel(page);
    m_keyStatus->setWordWrap(true);
    m_keyStatus->setObjectName(QStringLiteral("Hint"));
    form->addRow(QString(), m_keyStatus);

    layout->addLayout(form);

    m_authHint = new QLabel(page);
    m_authHint->setWordWrap(true);
    m_authHint->setObjectName(QStringLiteral("Hint"));
    layout->addWidget(m_authHint);

    auto *installHint = new QLabel(tr(
        "Para instalar la llave pública en un servidor ya guardado, haz clic "
        "derecho sobre la sesión en el panel lateral y elige "
        "«Instalar llave pública en el servidor…». Se abrirá una terminal "
        "donde ssh-copy-id pedirá la contraseña si hace falta."), page);
    installHint->setWordWrap(true);
    installHint->setObjectName(QStringLiteral("Hint"));
    layout->addWidget(installHint);
    layout->addStretch(1);

    m_tabs->addTab(page, tr("Autenticación"));

    connect(m_auth, &QComboBox::currentIndexChanged, this, [this] { updateAuthUi(); });
    connect(m_key, &QLineEdit::textChanged, this, [this] { updateAuthUi(); });
    connect(m_browseKey, &QPushButton::clicked, this, &SessionDialog::browseKey);
    connect(m_generateKey, &QPushButton::clicked, this, &SessionDialog::generateKey);
    connect(m_showKey, &QPushButton::clicked, this, &SessionDialog::showPublicKey);
    connect(m_copyKey, &QPushButton::clicked, this, &SessionDialog::copyPublicKey);
}

void SessionDialog::buildAdvancedTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(9);

    m_proxyJump = new QLineEdit(page);
    m_proxyJump->setPlaceholderText(tr("usuario@bastion:22 (opcional)"));
    m_proxyRow = m_proxyJump;
    form->addRow(tr("ProxyJump"), m_proxyJump);

    m_extraArgs = new QLineEdit(page);
    m_extraArgs->setPlaceholderText(tr("-o Compression=yes (opcional)"));
    form->addRow(tr("Argumentos ssh"), m_extraArgs);

    m_initialCommand = new QLineEdit(page);
    m_initialCommand->setPlaceholderText(tr("tmux attach || tmux new (opcional)"));
    m_initialRow = m_initialCommand;
    form->addRow(tr("Comando inicial"), m_initialCommand);

    m_hostKeyPolicy = new QComboBox(page);
    m_hostKeyPolicy->setObjectName(QStringLiteral("HostKeyPolicyCombo"));
    m_hostKeyPolicy->addItem(Session::hostKeyPolicyName(Session::HostKeyPolicy::Ask),
                             QStringLiteral("ask"));
    m_hostKeyPolicy->addItem(Session::hostKeyPolicyName(Session::HostKeyPolicy::Strict),
                             QStringLiteral("strict"));
    m_hostKeyPolicy->addItem(Session::hostKeyPolicyName(Session::HostKeyPolicy::AcceptNew),
                             QStringLiteral("accept-new"));
    m_hostKeyPolicy->setToolTip(tr(
        "Preguntar: OpenSSH pide confirmación la primera vez.\n"
        "Estricta: rechaza hosts desconocidos o con clave cambiada.\n"
        "Aceptar nuevas: confía en hosts nuevos, pero rechaza cambios de clave."));
    form->addRow(tr("Host key"), m_hostKeyPolicy);

    m_connectTimeout = new QSpinBox(page);
    m_connectTimeout->setObjectName(QStringLiteral("ConnectTimeoutSpin"));
    m_connectTimeout->setRange(1, 300);
    m_connectTimeout->setSuffix(tr(" s"));
    m_connectTimeout->setValue(10);
    form->addRow(tr("Timeout de conexión"), m_connectTimeout);

    layout->addLayout(form);

    auto *options = new QGroupBox(tr("Opciones de conexión"), page);
    auto *optionsLayout = new QVBoxLayout(options);
    m_x11 = new QCheckBox(tr("Reenviar X11 (-X)"), options);
    m_agentForward = new QCheckBox(tr("Reenviar agente SSH (-A)"), options);
    m_keepAlive = new QCheckBox(tr("Mantener viva la conexión (keepalive cada 30 s)"), options);
    m_keepAlive->setChecked(true);
    m_autoReconnect = new QCheckBox(tr("Reconectar automáticamente si la conexión se cae"), options);
    m_autoReconnect->setObjectName(QStringLiteral("AutoReconnectCheck"));
    m_autoReconnect->setToolTip(tr(
        "Reintenta la conexión hasta 5 veces con espera creciente (1, 2, 4, 8 y "
        "16 segundos). No se aplica a terminales locales."));
    optionsLayout->addWidget(m_x11);
    optionsLayout->addWidget(m_agentForward);
    optionsLayout->addWidget(m_keepAlive);
    optionsLayout->addWidget(m_autoReconnect);
    layout->addWidget(options);

    auto *notesLabel = new QLabel(tr("Notas"), page);
    layout->addWidget(notesLabel);
    m_notes = new QPlainTextEdit(page);
    m_notes->setPlaceholderText(tr("Recordatorios, propósito del servidor, etc."));
    m_notes->setMaximumHeight(110);
    layout->addWidget(m_notes);
    layout->addStretch(1);

    m_tabs->addTab(page, tr("Avanzado"));
}

void SessionDialog::buildForwardsTab()
{
    m_forwardsPage = new QWidget(this);
    auto *layout = new QVBoxLayout(m_forwardsPage);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "Los port forwards se aplican al abrir la sesión. «Local (-L)» abre un "
        "puerto en este equipo y lo conecta a un destino a través del servidor; "
        "«Remoto (-R)» abre un puerto en el servidor y lo conecta hacia este "
        "equipo; «SOCKS5 (-D)» crea un proxy dinámico."), m_forwardsPage);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("Hint"));
    layout->addWidget(hint);

    m_forwards = new QTableWidget(0, 6, m_forwardsPage);
    m_forwards->setObjectName(QStringLiteral("ForwardsTable"));
    m_forwards->setHorizontalHeaderLabels({ tr("Tipo"), tr("Host escucha"),
                                            tr("Puerto"), tr("Destino"),
                                            tr("Puerto destino"), tr("Activo") });
    m_forwards->verticalHeader()->setVisible(false);
    m_forwards->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_forwards->setSelectionMode(QAbstractItemView::SingleSelection);
    m_forwards->setAlternatingRowColors(true);
    m_forwards->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_forwards->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_forwards->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_forwards->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_forwards->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_forwards->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    layout->addWidget(m_forwards, 1);

    auto *buttons = new QHBoxLayout;
    m_addForward = new QPushButton(tr("Añadir forward"), m_forwardsPage);
    m_addForward->setObjectName(QStringLiteral("AddForwardButton"));
    m_removeForward = new QPushButton(tr("Quitar seleccionado"), m_forwardsPage);
    m_removeForward->setObjectName(QStringLiteral("RemoveForwardButton"));
    buttons->addWidget(m_addForward);
    buttons->addWidget(m_removeForward);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    connect(m_addForward, &QPushButton::clicked, this, [this] {
        addPortForwardRow();
        m_forwards->selectRow(m_forwards->rowCount() - 1);
    });
    connect(m_removeForward, &QPushButton::clicked, this, [this] {
        const int row = m_forwards->currentRow();
        if (row < 0) {
            QMessageBox::information(this, tr("Sin selección"),
                                     tr("Selecciona un forward para quitarlo."));
            return;
        }
        m_forwards->removeRow(row);
    });

    m_tabs->addTab(m_forwardsPage, tr("Túneles"));
}

void SessionDialog::buildTerminalTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "El perfil se aplica a todas las terminales y divisiones de esta sesión. "
        "Los campos que dejes vacíos usan la preferencia global."), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_profileEnabled = new QCheckBox(tr("Usar un perfil propio para esta sesión"), page);
    m_profileEnabled->setObjectName(QStringLiteral("TerminalProfileCheck"));
    layout->addWidget(m_profileEnabled);

    auto *box = new QGroupBox(tr("Apariencia"), page);
    auto *form = new QFormLayout(box);

    m_profileFont = new QFontComboBox(box);
    m_profileFont->setObjectName(QStringLiteral("ProfileFontCombo"));
    m_profileFont->setFontFilters(QFontComboBox::MonospacedFonts);
    form->addRow(tr("Fuente"), m_profileFont);

    m_profileFontSize = new QSpinBox(box);
    m_profileFontSize->setObjectName(QStringLiteral("ProfileFontSizeSpin"));
    m_profileFontSize->setRange(6, 48);
    m_profileFontSize->setValue(11);
    m_profileFontSize->setSuffix(tr(" pt"));
    form->addRow(tr("Tamaño"), m_profileFontSize);

    m_profileScrollback = new QSpinBox(box);
    m_profileScrollback->setObjectName(QStringLiteral("ProfileScrollbackSpin"));
    m_profileScrollback->setRange(1000, 200000);
    m_profileScrollback->setSingleStep(1000);
    m_profileScrollback->setValue(10000);
    m_profileScrollback->setSuffix(tr(" líneas"));
    form->addRow(tr("Historial"), m_profileScrollback);

    m_profileCursor = new QComboBox(box);
    m_profileCursor->setObjectName(QStringLiteral("ProfileCursorCombo"));
    m_profileCursor->addItem(tr("Bloque"), QStringLiteral("block"));
    m_profileCursor->addItem(tr("Barra (I-Beam)"), QStringLiteral("ibeam"));
    m_profileCursor->addItem(tr("Guion bajo"), QStringLiteral("underscore"));
    form->addRow(tr("Cursor"), m_profileCursor);

    m_profileBlink = new QCheckBox(tr("Cursor parpadeante"), box);
    m_profileBlink->setObjectName(QStringLiteral("ProfileCursorBlinkCheck"));
    m_profileBlink->setChecked(true);
    form->addRow(QString(), m_profileBlink);

    layout->addWidget(box);
    layout->addStretch(1);

    connect(m_profileEnabled, &QCheckBox::toggled, this,
            &SessionDialog::updateProfileUi);

    m_tabs->addTab(page, tr("Terminal"));
}

void SessionDialog::updateProfileUi()
{
    const bool enabled = m_profileEnabled && m_profileEnabled->isChecked();
    if (m_profileFont)
        m_profileFont->setEnabled(enabled);
    if (m_profileFontSize)
        m_profileFontSize->setEnabled(enabled);
    if (m_profileScrollback)
        m_profileScrollback->setEnabled(enabled);
    if (m_profileCursor)
        m_profileCursor->setEnabled(enabled);
    if (m_profileBlink)
        m_profileBlink->setEnabled(enabled);
}

void SessionDialog::addPortForwardRow(const PortForward &forward)
{
    if (!m_forwards)
        return;

    const int row = m_forwards->rowCount();
    m_forwards->insertRow(row);

    auto *type = new QComboBox(m_forwards);
    type->setObjectName(QStringLiteral("ForwardType"));
    type->addItem(PortForward::typeName(PortForward::Type::Local),
                  int(PortForward::Type::Local));
    type->addItem(PortForward::typeName(PortForward::Type::Remote),
                  int(PortForward::Type::Remote));
    type->addItem(PortForward::typeName(PortForward::Type::Dynamic),
                  int(PortForward::Type::Dynamic));
    const int typeIndex = type->findData(int(forward.type));
    type->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    m_forwards->setCellWidget(row, 0, type);

    auto *listenHost = new QLineEdit(forward.listenHost.isEmpty()
                                         ? QStringLiteral("127.0.0.1")
                                         : forward.listenHost,
                                     m_forwards);
    listenHost->setObjectName(QStringLiteral("ForwardListenHost"));
    m_forwards->setCellWidget(row, 1, listenHost);

    auto *listenPort = new QSpinBox(m_forwards);
    listenPort->setObjectName(QStringLiteral("ForwardListenPort"));
    listenPort->setRange(0, 65535);
    listenPort->setSpecialValueText(tr("auto"));
    listenPort->setValue(qBound(0, forward.listenPort, 65535));
    m_forwards->setCellWidget(row, 2, listenPort);

    auto *targetHost = new QLineEdit(forward.targetHost, m_forwards);
    targetHost->setObjectName(QStringLiteral("ForwardTargetHost"));
    targetHost->setPlaceholderText(tr("host o IP de destino"));
    m_forwards->setCellWidget(row, 3, targetHost);

    auto *targetPort = new QSpinBox(m_forwards);
    targetPort->setObjectName(QStringLiteral("ForwardTargetPort"));
    targetPort->setRange(0, 65535);
    targetPort->setValue(qBound(0, forward.targetPort, 65535));
    m_forwards->setCellWidget(row, 4, targetPort);

    auto *enabled = new QTableWidgetItem;
    enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    enabled->setCheckState(forward.enabled ? Qt::Checked : Qt::Unchecked);
    enabled->setTextAlignment(Qt::AlignCenter);
    m_forwards->setItem(row, 5, enabled);

    m_forwards->setRowHeight(row, 34);

    connect(type, &QComboBox::currentIndexChanged, this, [this, type] {
        for (int i = 0; i < m_forwards->rowCount(); ++i) {
            if (m_forwards->cellWidget(i, 0) == type) {
                updateForwardRow(i);
                return;
            }
        }
    });
    connect(listenHost, &QLineEdit::textChanged, this, [this] { m_forwards->resizeRowsToContents(); });

    updateForwardRow(row);
}

void SessionDialog::updateForwardRow(int row)
{
    if (!m_forwards || row < 0 || row >= m_forwards->rowCount())
        return;
    auto *type = qobject_cast<QComboBox *>(m_forwards->cellWidget(row, 0));
    if (!type)
        return;
    const bool dynamic = type->currentData().toInt() == int(PortForward::Type::Dynamic);

    auto *targetHost = qobject_cast<QLineEdit *>(m_forwards->cellWidget(row, 3));
    auto *targetPort = qobject_cast<QSpinBox *>(m_forwards->cellWidget(row, 4));
    if (targetHost) {
        targetHost->setEnabled(!dynamic);
        targetHost->setPlaceholderText(dynamic ? tr("No aplica")
                                               : tr("host o IP de destino"));
    }
    if (targetPort) {
        targetPort->setEnabled(!dynamic);
        if (dynamic)
            targetPort->setValue(0);
    }
}

bool SessionDialog::readPortForwards(QVector<PortForward> &forwards)
{
    forwards.clear();
    if (!m_forwards || !m_tabs->isTabVisible(3))
        return true;

    for (int row = 0; row < m_forwards->rowCount(); ++row) {
        auto *type = qobject_cast<QComboBox *>(m_forwards->cellWidget(row, 0));
        auto *listenHost = qobject_cast<QLineEdit *>(m_forwards->cellWidget(row, 1));
        auto *listenPort = qobject_cast<QSpinBox *>(m_forwards->cellWidget(row, 2));
        auto *targetHost = qobject_cast<QLineEdit *>(m_forwards->cellWidget(row, 3));
        auto *targetPort = qobject_cast<QSpinBox *>(m_forwards->cellWidget(row, 4));
        auto *enabledItem = m_forwards->item(row, 5);
        if (!type || !listenHost || !listenPort || !targetHost || !targetPort)
            continue;

        PortForward forward;
        forward.type = PortForward::Type(type->currentData().toInt());
        forward.listenHost = listenHost->text().trimmed();
        if (forward.listenHost.isEmpty())
            forward.listenHost = QStringLiteral("127.0.0.1");
        forward.listenPort = listenPort->value();
        forward.targetHost = targetHost->text().trimmed();
        forward.targetPort = targetPort->value();
        forward.enabled = enabledItem && enabledItem->checkState() == Qt::Checked;

        if (forward.type == PortForward::Type::Dynamic) {
            forward.targetHost.clear();
            forward.targetPort = 0;
        } else if (forward.enabled
                   && (forward.targetHost.isEmpty() || forward.targetPort <= 0)) {
            QMessageBox::warning(
                this, tr("Forward incompleto"),
                tr("La fila %1 necesita un host y un puerto de destino, o "
                   "desmárcala para guardarla desactivada.").arg(row + 1));
            m_tabs->setCurrentWidget(m_forwardsPage);
            m_forwards->selectRow(row);
            return false;
        }
        forwards.append(forward);
    }
    return true;
}

void SessionDialog::fillColorCombo()
{
    for (const QString &tag : Session::colorTags())
        m_color->addItem(colorIcon(tag), colorLabel(tag), tag);
}

void SessionDialog::load(const Session &session)
{
    m_session = session;

    m_name->setText(session.name);
    m_group->setCurrentText(session.group.isEmpty() ? QStringLiteral("Servidores") : session.group);
    m_type->setCurrentIndex(session.isLocal() ? 1 : 0);

    const int colorIndex = m_color->findData(session.colorTag);
    m_color->setCurrentIndex(colorIndex >= 0 ? colorIndex : 0);

    m_host->setText(session.host);
    m_port->setValue(session.port > 0 ? session.port : 22);
    m_user->setText(session.user);
    m_shell->setCurrentText(session.shell.isEmpty() ? defaultShellPath() : session.shell);
    m_startDir->setText(session.startDir);

    const QString authValue = session.auth == Session::Auth::Agent ? QStringLiteral("agent")
                              : session.auth == Session::Auth::Password ? QStringLiteral("password")
                              : QStringLiteral("publickey");
    const int authIndex = m_auth->findData(authValue);
    m_auth->setCurrentIndex(authIndex >= 0 ? authIndex : 0);
    m_key->setText(session.privateKey);

    m_proxyJump->setText(session.proxyJump);
    m_extraArgs->setText(session.extraArgs);
    m_initialCommand->setText(session.initialCommand);
    const QString policy = session.hostKeyPolicy == Session::HostKeyPolicy::Strict
                               ? QStringLiteral("strict")
                               : session.hostKeyPolicy == Session::HostKeyPolicy::AcceptNew
                                     ? QStringLiteral("accept-new")
                                     : QStringLiteral("ask");
    const int policyIndex = m_hostKeyPolicy->findData(policy);
    m_hostKeyPolicy->setCurrentIndex(policyIndex >= 0 ? policyIndex : 0);
    m_connectTimeout->setValue(session.connectTimeout > 0 ? session.connectTimeout : 10);
    m_favorite->setChecked(session.favorite);
    m_x11->setChecked(session.x11Forwarding);
    m_agentForward->setChecked(session.forwardAgent);
    m_keepAlive->setChecked(session.keepAlive);
    m_autoReconnect->setChecked(session.autoReconnect);
    m_notes->setPlainText(session.notes);

    const TerminalProfile &profile = session.terminalProfile;
    m_profileEnabled->setChecked(profile.enabled);
    if (!profile.fontFamily.trimmed().isEmpty())
        m_profileFont->setCurrentFont(QFont(profile.fontFamily));
    m_profileFontSize->setValue(profile.fontSize > 0 ? profile.fontSize : 11);
    m_profileScrollback->setValue(profile.scrollbackLines > 0
                                      ? profile.scrollbackLines
                                      : 10000);
    const int cursorIndex = m_profileCursor->findData(
        profile.cursorStyle.isEmpty() ? QStringLiteral("block") : profile.cursorStyle);
    m_profileCursor->setCurrentIndex(cursorIndex >= 0 ? cursorIndex : 0);
    m_profileBlink->setChecked(profile.cursorBlink);
    updateProfileUi();

    m_forwards->setRowCount(0);
    for (const PortForward &forward : session.portForwards)
        addPortForwardRow(forward);

    if (session.id.isEmpty()) {
        setWindowTitle(tr("Nueva conexión"));
        if (session.name.isEmpty())
            m_name->setFocus();
    } else {
        setWindowTitle(tr("Editar sesión — %1").arg(session.name));
    }
}

void SessionDialog::updateVisibility()
{
    const QString typeStr = m_type->currentData().toString();
    const bool ssh = typeStr == QStringLiteral("ssh");
    const bool telnet = typeStr == QStringLiteral("telnet");
    const bool remote = ssh || telnet;

    if (m_sshBox)
        m_sshBox->setVisible(remote);
    if (m_localBox)
        m_localBox->setVisible(!remote);
    if (m_tabs->count() > 1)
        m_tabs->setTabVisible(1, ssh);
    if (m_tabs->count() > 2)
        m_tabs->setTabVisible(2, ssh);
    if (m_tabs->count() > 3)
        m_tabs->setTabVisible(3, ssh);

    if (m_proxyRow)
        m_proxyRow->setEnabled(ssh);
    if (m_initialRow)
        m_initialRow->setEnabled(ssh);
    if (m_hostKeyPolicy)
        m_hostKeyPolicy->setEnabled(ssh);
    if (m_connectTimeout)
        m_connectTimeout->setEnabled(ssh);
    if (m_x11)
        m_x11->setEnabled(ssh);
    if (m_agentForward)
        m_agentForward->setEnabled(ssh);
    if (m_keepAlive)
        m_keepAlive->setEnabled(ssh);
    if (m_autoReconnect)
        m_autoReconnect->setEnabled(ssh);
    if (m_forwards)
        m_forwards->setEnabled(ssh);
    if (m_addForward)
        m_addForward->setEnabled(ssh);
    if (m_removeForward)
        m_removeForward->setEnabled(ssh);

    if (!ssh) {
        if (m_shell->currentText().isEmpty())
            m_shell->setCurrentText(defaultShellPath());
    }
}

void SessionDialog::updateAuthUi()
{
    const QString auth = m_auth->currentData().toString();
    const bool publicKey = auth == QStringLiteral("publickey");
    const bool agent = auth == QStringLiteral("agent");

    m_key->setEnabled(publicKey || agent);
    m_browseKey->setEnabled(publicKey || agent);
    m_generateKey->setEnabled(publicKey || agent);
    m_showKey->setEnabled(publicKey || agent);
    m_copyKey->setEnabled(publicKey || agent);

    if (publicKey) {
        m_authHint->setText(tr(
            "QTerm2 invoca a OpenSSH con «-i» y «IdentitiesOnly=yes». Si la llave "
            "tiene passphrase, ssh la pedirá en la terminal."));
    } else if (agent) {
        m_authHint->setText(tr(
            "Se usará el agente SSH (SSH_AUTH_SOCK). Asegúrate de haber añadido "
            "la llave con «ssh-add»."));
    } else {
        m_authHint->setText(tr(
            "La contraseña se escribe directamente en la terminal y no se "
            "almacena en disco. OpenSSH la solicitará al conectar."));
    }

    if (publicKey || agent) {
        const QString path = expandedKeyPath();
        if (path.isEmpty()) {
            m_keyStatus->setText(tr("Sin llave seleccionada."));
        } else if (!QFileInfo::exists(path)) {
            m_keyStatus->setText(tr("⚠ No existe la llave privada: %1").arg(path));
        } else if (!QFileInfo::exists(path + QStringLiteral(".pub"))) {
            m_keyStatus->setText(tr("⚠ La llave existe, pero no se encontró su archivo .pub."));
        } else {
            m_keyStatus->setText(tr("✓ Llave encontrada: %1").arg(path));
        }
    } else {
        m_keyStatus->clear();
    }
}

QString SessionDialog::expandedKeyPath() const
{
    return qterm2::ssh::KeyManager::expandPath(m_key->text().trimmed());
}

void SessionDialog::browseKey()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Seleccionar llave privada"), qterm2::ssh::KeyManager::sshDirectory(),
        tr("Llaves SSH (*)"));
    if (file.isEmpty())
        return;
    m_key->setText(file);
    const int idx = m_auth->findData(QStringLiteral("publickey"));
    if (idx >= 0)
        m_auth->setCurrentIndex(idx);
    updateAuthUi();
}

void SessionDialog::generateKey()
{
    Session temp = m_result;
    temp.host = m_host->text().trimmed();
    temp.user = m_user->text().trimmed();

    KeyGenDialog dialog(temp, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_key->setText(dialog.keyPath());
    const int idx = m_auth->findData(QStringLiteral("publickey"));
    if (idx >= 0)
        m_auth->setCurrentIndex(idx);
    updateAuthUi();

    QMessageBox::information(
        this, tr("Llave generada"),
        tr("Se creó la llave:\n%1\n\nGuarda la sesión y usa «Instalar llave "
           "pública en el servidor…» desde el menú lateral para autorizarla.")
            .arg(dialog.keyPath()));
}

void SessionDialog::showPublicKey()
{
    const QString path = expandedKeyPath();
    const QString pub = qterm2::ssh::KeyManager::publicKeyFor(path);
    if (pub.isEmpty()) {
        QMessageBox::warning(this, tr("Sin llave pública"),
                             tr("No se encontró el archivo .pub de la llave seleccionada."));
        return;
    }

    QString fingerprint;
    QProcess proc;
    proc.start(QStringLiteral("ssh-keygen"),
               { QStringLiteral("-lf"), path + QStringLiteral(".pub") });
    if (proc.waitForFinished(3000) && proc.exitCode() == 0)
        fingerprint = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Llave pública"));
    dialog.resize(680, 300);
    auto *layout = new QVBoxLayout(&dialog);
    if (!fingerprint.isEmpty()) {
        auto *fp = new QLabel(fingerprint, &dialog);
        fp->setWordWrap(true);
        fp->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(fp);
    }
    auto *text = new QPlainTextEdit(pub, &dialog);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(text, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *copy = buttons->addButton(tr("Copiar"), QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, this, [pub] {
        QApplication::clipboard()->setText(pub);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
}

void SessionDialog::copyPublicKey()
{
    const QString pub = qterm2::ssh::KeyManager::publicKeyFor(expandedKeyPath());
    if (pub.isEmpty()) {
        QMessageBox::warning(this, tr("Sin llave pública"),
                             tr("No se encontró el archivo .pub de la llave seleccionada."));
        return;
    }
    QApplication::clipboard()->setText(pub);
}

bool SessionDialog::validate()
{
    const bool ssh = m_type->currentData().toString() == QStringLiteral("ssh");

    QString name = m_name->text().trimmed();
    if (name.isEmpty())
        name = ssh ? m_host->text().trimmed() : tr("Terminal local");
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Falta el nombre"),
                             tr("Escribe un nombre o al menos el servidor."));
        return false;
    }

    if (ssh && m_host->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Falta el servidor"),
                             tr("El nombre o la IP del servidor es obligatorio."));
        m_tabs->setCurrentIndex(0);
        m_host->setFocus();
        return false;
    }

    if (ssh && m_auth->currentData().toString() == QStringLiteral("publickey")) {
        const QString path = expandedKeyPath();
        if (path.isEmpty()) {
            QMessageBox::warning(this, tr("Falta la llave"),
                                 tr("Selecciona una llave privada o genera una nueva."));
            m_tabs->setCurrentIndex(1);
            m_key->setFocus();
            return false;
        }
        if (!QFileInfo::exists(path)) {
            QMessageBox::warning(this, tr("Llave no encontrada"),
                                 tr("No existe la llave privada:\n%1").arg(path));
            m_tabs->setCurrentIndex(1);
            m_key->setFocus();
            return false;
        }
    }

    if (!ssh && m_shell->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Falta la shell"),
                             tr("Indica la shell que se debe ejecutar."));
        return false;
    }

    QVector<PortForward> forwards;
    if (ssh && !readPortForwards(forwards))
        return false;

    Session s = m_session;
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = name;
    s.group = m_group->currentText().trimmed();
    const QString typeStr = m_type->currentData().toString();
    const bool isTelnet = typeStr == QStringLiteral("telnet");

    s.type = ssh ? Session::Type::Ssh 
           : isTelnet ? Session::Type::Telnet 
           : Session::Type::Local;
    s.host = m_host->text().trimmed();
    s.port = m_port->value();
    s.user = m_user->text().trimmed();
    
    if (s.group.isEmpty())
        s.group = (ssh || isTelnet) ? QStringLiteral("Servidores") : QStringLiteral("Locales");

    const QString auth = m_auth->currentData().toString();
    s.auth = auth == QStringLiteral("agent") ? Session::Auth::Agent
              : auth == QStringLiteral("password") ? Session::Auth::Password
              : Session::Auth::PublicKey;
    s.privateKey = m_key->text().trimmed();
    s.proxyJump = m_proxyJump->text().trimmed();
    s.extraArgs = m_extraArgs->text().trimmed();
    s.initialCommand = m_initialCommand->text().trimmed();
    s.startDir = m_startDir->text().trimmed();
    s.shell = m_shell->currentText().trimmed();
    s.x11Forwarding = m_x11->isChecked();
    s.forwardAgent = m_agentForward->isChecked();
    s.keepAlive = m_keepAlive->isChecked();
    s.autoReconnect = m_autoReconnect->isChecked();
    const QString policy = m_hostKeyPolicy->currentData().toString();
    s.hostKeyPolicy = policy == QLatin1String("strict")
                          ? Session::HostKeyPolicy::Strict
                          : policy == QLatin1String("accept-new")
                                ? Session::HostKeyPolicy::AcceptNew
                                : Session::HostKeyPolicy::Ask;
    s.connectTimeout = m_connectTimeout->value();
    s.favorite = m_favorite->isChecked();
    s.portForwards = ssh ? forwards : QVector<PortForward>();

    TerminalProfile profile;
    profile.enabled = m_profileEnabled->isChecked();
    if (m_profileFont) {
        const QString family = m_profileFont->currentFont().family().trimmed();
        profile.fontFamily = family;
    }
    profile.fontSize = m_profileFontSize->value();
    profile.scrollbackLines = m_profileScrollback->value();
    profile.cursorStyle = m_profileCursor->currentData().toString();
    profile.cursorBlink = m_profileBlink->isChecked();
    s.terminalProfile = profile;
    s.colorTag = m_color->currentData().toString();
    s.notes = m_notes->toPlainText();

    m_result = s;
    return true;
}

void SessionDialog::accept()
{
    if (!validate())
        return;
    QDialog::accept();
}
