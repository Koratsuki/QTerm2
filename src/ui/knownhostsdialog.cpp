#include "ui/knownhostsdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

using qterm2::ssh::KnownHostEntry;

KnownHostsDialog::KnownHostsDialog(const QString &path, QWidget *parent)
    : QDialog(parent)
    , m_path(path)
{
    setWindowTitle(tr("known_hosts"));
    resize(900, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "Hosts con clave registrada por OpenSSH. Elimina una entrada si un "
        "servidor cambió de clave o ya no existe. Las entradas con hash solo "
        "pueden borrarse, no mostrarse por nombre. Se crea una copia de "
        "seguridad antes de cada cambio."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("KnownHostsFilter"));
    m_filter->setPlaceholderText(tr("Filtrar por host, tipo de clave o comentario…"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_table = new QTableWidget(0, 5, this);
    m_table->setObjectName(QStringLiteral("KnownHostsTable"));
    m_table->setHorizontalHeaderLabels({ tr("Línea"), tr("Hosts"), tr("Type"),
                                         tr("Clave"), tr("Comentario") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    auto *bottom = new QHBoxLayout;
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("KnownHostsStatus"));
    bottom->addWidget(m_status, 1);

    auto *remove = new QPushButton(tr("Eliminar seleccionadas"), this);
    remove->setObjectName(QStringLiteral("RemoveKnownHostsButton"));
    bottom->addWidget(remove);
    layout->addLayout(bottom);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    box->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(remove, &QPushButton::clicked, this, &KnownHostsDialog::removeSelected);
    connect(m_filter, &QLineEdit::textChanged, this, &KnownHostsDialog::applyFilter);

    reload();
}

void KnownHostsDialog::reload()
{
    QString error;
    const qterm2::ssh::KnownHostsDocument document =
        qterm2::ssh::parseKnownHosts(m_path, &error);
    m_entries = document.entries;

    m_table->setRowCount(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        const KnownHostEntry &entry = m_entries.at(i);
        const QString keyPreview = entry.key.size() > 28
                                       ? entry.key.left(28) + QStringLiteral("…")
                                       : entry.key;
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(entry.line)));
        auto *hosts = new QTableWidgetItem(
            entry.hashed ? tr("(hash) %1").arg(entry.hosts) : entry.hosts);
        if (entry.hashed)
            hosts->setToolTip(tr("Entrada con hash: no es posible mostrar el nombre del host."));
        m_table->setItem(i, 1, hosts);
        m_table->setItem(i, 2, new QTableWidgetItem(entry.keyType));
        m_table->setItem(i, 3, new QTableWidgetItem(keyPreview));
        m_table->setItem(i, 4, new QTableWidgetItem(entry.comment));
    }
    m_table->resizeRowsToContents();

    QString message = tr("%1 entradas · %2").arg(m_entries.size()).arg(m_path);
    if (!error.isEmpty())
        message = error;
    else if (!document.warnings.isEmpty())
        message += tr(" · %1 avisos").arg(document.warnings.size());
    m_status->setText(message);
    applyFilter(m_filter->text());
}

void KnownHostsDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_entries.size(); ++i) {
        const KnownHostEntry &entry = m_entries.at(i);
        const bool matches =
            needle.isEmpty()
            || entry.hosts.contains(needle, Qt::CaseInsensitive)
            || entry.keyType.contains(needle, Qt::CaseInsensitive)
            || entry.comment.contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(i, !matches);
    }
}

void KnownHostsDialog::removeSelected()
{
    QSet<int> lines;
    for (const QModelIndex &index : m_table->selectionModel()->selectedRows())
        lines.insert(m_entries.at(index.row()).line);
    if (lines.isEmpty()) {
        QMessageBox::information(this, tr("Sin selección"),
                                 tr("Selecciona una o más entradas para eliminarlas."));
        return;
    }

    const auto answer = QMessageBox::question(
        this, tr("Eliminar entradas de known_hosts"),
        tr("Se eliminarán %1 entradas. La próxima conexión volverá a pedir "
           "confirmación para esos hosts.\n\n¿Continuar?")
            .arg(lines.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!qterm2::ssh::removeKnownHostLines(m_path, lines, &error)) {
        QMessageBox::warning(this, tr("No se pudo actualizar known_hosts"), error);
        return;
    }
    reload();
}
