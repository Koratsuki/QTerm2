#include "ui/sshconfigdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

using qterm2::ssh::SshConfigEntry;

SshConfigDialog::SshConfigDialog(const QVector<SshConfigEntry> &entries,
                                 const QSet<QString> &existingTargets,
                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Importar desde la configuración SSH"));
    resize(860, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "Selecciona los hosts que quieres añadir al catálogo. Los hosts con "
        "comodines no se importan. Las entradas ya existentes aparecen "
        "desactivadas."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("SshConfigFilter"));
    m_filter->setPlaceholderText(tr("Filtrar por alias, servidor, usuario…"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_table = new QTableWidget(0, 7, this);
    m_table->setObjectName(QStringLiteral("SshConfigTable"));
    m_table->setHorizontalHeaderLabels({ QString(), tr("Alias"), tr("Host"),
                                         tr("User"), tr("Port"), tr("Llave"),
                                         tr("Origen") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    for (const SshConfigEntry &entry : entries) {
        Row row;
        row.entry = entry;
        row.existing = existingTargets.contains(entry.effectiveHost().toLower());
        m_rows.append(row);
    }

    m_table->setRowCount(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        auto *check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        check->setCheckState(row.existing ? Qt::Unchecked : Qt::Checked);
        if (row.existing) {
            check->setFlags(Qt::ItemIsEnabled);
            check->setToolTip(tr("Ya existe una sesión para este servidor"));
        }
        m_table->setItem(i, 0, check);
        m_table->setItem(i, 1, new QTableWidgetItem(row.entry.alias));
        m_table->setItem(i, 2, new QTableWidgetItem(row.entry.effectiveHost()));
        m_table->setItem(i, 3, new QTableWidgetItem(row.entry.user));
        m_table->setItem(i, 4, new QTableWidgetItem(
            row.entry.port > 0 ? QString::number(row.entry.port) : QStringLiteral("22")));
        m_table->setItem(i, 5, new QTableWidgetItem(row.entry.identityFile));
        m_table->setItem(i, 6, new QTableWidgetItem(
            QStringLiteral("%1:%2").arg(row.entry.sourcePath).arg(row.entry.line)));
        if (row.existing) {
            for (int column = 1; column < 7; ++column) {
                if (QTableWidgetItem *item = m_table->item(i, column))
                    item->setForeground(palette().color(QPalette::Disabled,
                                                        QPalette::Text));
            }
        }
    }
    m_table->resizeRowsToContents();

    auto *buttons = new QHBoxLayout;
    auto *selectAll = new QPushButton(tr("Select All"), this);
    auto *selectNone = new QPushButton(tr("Seleccionar nada"), this);
    selectAll->setObjectName(QStringLiteral("SelectAllHosts"));
    selectNone->setObjectName(QStringLiteral("SelectNoHosts"));
    buttons->addWidget(selectAll);
    buttons->addWidget(selectNone);
    buttons->addStretch(1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("SshConfigSummary"));
    buttons->addWidget(m_summary);
    layout->addLayout(buttons);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     this);
    box->button(QDialogButtonBox::Ok)->setText(tr("Importar seleccionadas"));
    box->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(selectAll, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    connect(selectNone, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    connect(m_filter, &QLineEdit::textChanged, this, &SshConfigDialog::applyFilter);
    connect(m_table, &QTableWidget::itemChanged, this,
            [this] { applyFilter(m_filter->text()); });

    applyFilter(QString());
}

QVector<SshConfigEntry> SshConfigDialog::selectedEntries() const
{
    QVector<SshConfigEntry> selected;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        if (row.existing)
            continue;
        const QTableWidgetItem *check = m_table->item(i, 0);
        if (check && check->checkState() == Qt::Checked)
            selected.append(row.entry);
    }
    return selected;
}

void SshConfigDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    int visible = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        const bool matches =
            needle.isEmpty()
            || row.entry.alias.contains(needle, Qt::CaseInsensitive)
            || row.entry.effectiveHost().contains(needle, Qt::CaseInsensitive)
            || row.entry.user.contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(i, !matches);
        if (matches)
            ++visible;
    }

    int selected = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        const QTableWidgetItem *check = m_table->item(i, 0);
        if (!row.existing && check && check->checkState() == Qt::Checked)
            ++selected;
    }
    m_summary->setText(tr("%1 de %2 visibles · %3 seleccionadas")
                           .arg(visible)
                           .arg(m_rows.size())
                           .arg(selected));
}

void SshConfigDialog::setAllChecked(bool checked)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).existing)
            continue;
        if (QTableWidgetItem *item = m_table->item(i, 0))
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    applyFilter(m_filter->text());
}
