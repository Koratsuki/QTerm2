#include "ui/snippetdialog.h"

#include "sessions/snippetstore.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

using qterm2::Snippet;
using qterm2::SnippetStore;

SnippetDialog::SnippetDialog(SnippetStore *store, const Session &context,
                             QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_context(context)
{
    setWindowTitle(tr("Snippets"));
    resize(880, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr(
        "Los snippets se escriben en la terminal activa, nunca se ejecutan solos. "
        "Admite ${host}, ${user}, ${port}, ${name} y ${group}; usa $$ para un "
        "dólar literal."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter, 1);

    // ---- catalog ---------------------------------------------------------
    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 10, 0);
    leftLayout->setSpacing(8);

    m_filter = new QLineEdit(left);
    m_filter->setObjectName(QStringLiteral("SnippetFilter"));
    m_filter->setPlaceholderText(tr("Filtrar snippets…"));
    m_filter->setClearButtonEnabled(true);
    leftLayout->addWidget(m_filter);

    m_list = new QListWidget(left);
    m_list->setObjectName(QStringLiteral("SnippetList"));
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_list, 1);

    m_new = new QPushButton(tr("Nuevo"), left);
    m_new->setObjectName(QStringLiteral("SnippetNewButton"));
    leftLayout->addWidget(m_new);
    splitter->addWidget(left);

    // ---- editor ----------------------------------------------------------
    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(10, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    m_name = new QLineEdit(right);
    m_name->setObjectName(QStringLiteral("SnippetName"));
    form->addRow(tr("Nombre"), m_name);
    rightLayout->addLayout(form);

    rightLayout->addWidget(new QLabel(tr("Comando"), right));
    m_command = new QLineEdit(right);
    m_command->setObjectName(QStringLiteral("SnippetCommand"));
    m_command->setPlaceholderText(
        tr("systemctl status ${name}"));
    rightLayout->addWidget(m_command);

    rightLayout->addWidget(new QLabel(tr("Descripción"), right));
    m_description = new QPlainTextEdit(right);
    m_description->setObjectName(QStringLiteral("SnippetDescription"));
    m_description->setMaximumHeight(90);
    rightLayout->addWidget(m_description);

    auto *previewBox = new QWidget(right);
    auto *previewLayout = new QVBoxLayout(previewBox);
    previewLayout->setContentsMargins(0, 6, 0, 0);
    previewLayout->setSpacing(4);
    previewLayout->addWidget(new QLabel(tr("Vista previa para esta sesión"), right));
    m_preview = new QLabel(previewBox);
    m_preview->setObjectName(QStringLiteral("SnippetPreview"));
    m_preview->setWordWrap(true);
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_preview->setStyleSheet(QStringLiteral(
        "font-family: monospace; padding: 6px; border: 1px solid palette(mid);"
        " border-radius: 4px;"));
    previewLayout->addWidget(m_preview);
    rightLayout->addWidget(previewBox);

    rightLayout->addStretch(1);

    auto *actions = new QHBoxLayout;
    m_save = new QPushButton(tr("Guardar"), right);
    m_save->setObjectName(QStringLiteral("SnippetSaveButton"));
    m_delete = new QPushButton(tr("Eliminar"), right);
    m_delete->setObjectName(QStringLiteral("SnippetDeleteButton"));
    m_insert = new QPushButton(tr("Insertar en la terminal"), right);
    m_insert->setObjectName(QStringLiteral("SnippetInsertButton"));
    actions->addWidget(m_save);
    actions->addWidget(m_delete);
    actions->addStretch(1);
    actions->addWidget(m_insert);
    rightLayout->addLayout(actions);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_filter, &QLineEdit::textChanged, this, &SnippetDialog::applyFilter);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        if (QListWidgetItem *item = m_list->item(row))
            loadIntoEditor(m_store->snippetById(item->data(Qt::UserRole).toString()));
    });
    connect(m_new, &QPushButton::clicked, this, &SnippetDialog::clearEditor);
    connect(m_save, &QPushButton::clicked, this, &SnippetDialog::saveCurrent);
    connect(m_delete, &QPushButton::clicked, this, &SnippetDialog::deleteCurrent);
    connect(m_insert, &QPushButton::clicked, this, &SnippetDialog::insertCurrent);
    connect(m_command, &QLineEdit::textChanged, this, &SnippetDialog::updatePreview);

    reload();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    else
        clearEditor();
}

void SnippetDialog::reload(const QString &selectId)
{
    if (!m_store)
        return;

    m_list->clear();
    for (const Snippet &snippet : m_store->snippets()) {
        auto *item = new QListWidgetItem(snippet.name, m_list);
        item->setData(Qt::UserRole, snippet.id);
        item->setToolTip(snippet.command);
    }

    if (!selectId.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == selectId) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    }
    applyFilter(m_filter->text());
    m_delete->setEnabled(!m_currentId.isEmpty());
}

void SnippetDialog::loadIntoEditor(const Snippet &snippet)
{
    m_currentId = snippet.id;
    m_name->setText(snippet.name);
    m_command->setText(snippet.command);
    m_description->setPlainText(snippet.description);
    m_delete->setEnabled(!snippet.id.isEmpty());
    updatePreview();
}

void SnippetDialog::clearEditor()
{
    m_currentId.clear();
    m_name->clear();
    m_command->clear();
    m_description->clear();
    m_list->setCurrentRow(-1);
    m_delete->setEnabled(false);
    updatePreview();
    m_name->setFocus();
}

void SnippetDialog::saveCurrent()
{
    if (!m_store)
        return;

    const QString name = m_name->text().trimmed();
    const QString command = m_command->text().trimmed();
    if (name.isEmpty() || command.isEmpty()) {
        QMessageBox::warning(this, tr("Snippet incompleto"),
                             tr("El nombre y el comando son obligatorios."));
        return;
    }

    Snippet snippet = m_store->snippetById(m_currentId);
    snippet.name = name;
    snippet.command = command;
    snippet.description = m_description->toPlainText().trimmed();
    if (!m_store->saveSnippet(snippet))
        return;

    m_currentId = snippet.id;
    reload(snippet.id);
    loadIntoEditor(m_store->snippetById(snippet.id));
}

void SnippetDialog::deleteCurrent()
{
    if (!m_store || m_currentId.isEmpty())
        return;
    if (QMessageBox::question(this, tr("Eliminar snippet"),
                              tr("¿Eliminar «%1»?").arg(m_name->text().trimmed()))
        != QMessageBox::Yes)
        return;

    if (!m_store->removeSnippet(m_currentId))
        return;
    clearEditor();
    reload();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void SnippetDialog::insertCurrent()
{
    const QString command = m_command->text().trimmed();
    if (command.isEmpty()) {
        m_command->setFocus();
        return;
    }
    m_commandToSend = qterm2::expandSnippet(command, m_context);
    accept();
}

void SnippetDialog::updatePreview()
{
    const QString command = m_command->text().trimmed();
    if (command.isEmpty()) {
        m_preview->setText(tr("(sin comando)"));
        m_insert->setEnabled(false);
        return;
    }
    m_preview->setText(qterm2::expandSnippet(command, m_context));
    m_insert->setEnabled(true);
}

void SnippetDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const Snippet snippet = m_store->snippetById(item->data(Qt::UserRole).toString());
        const bool matches =
            needle.isEmpty() || snippet.name.contains(needle, Qt::CaseInsensitive)
            || snippet.command.contains(needle, Qt::CaseInsensitive)
            || snippet.description.contains(needle, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}
