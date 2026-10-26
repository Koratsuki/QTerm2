#include "ui/commandpalettedialog.h"

#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

CommandPaletteDialog::CommandPaletteDialog(const QVector<qterm2::Command> &commands,
                                           QWidget *parent)
    : QDialog(parent)
    , m_commands(commands)
{
    setWindowTitle(tr("Command Palette"));
    resize(640, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("CommandPaletteEdit"));
    m_edit->setPlaceholderText(tr("Escribe un comando, categoría o atajo…"));
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("CommandPaletteList"));
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_hint = new QLabel(tr("Enter para ejecutar · Esc para cerrar"), this);
    m_hint->setObjectName(QStringLiteral("CommandPaletteHint"));
    layout->addWidget(m_hint);

    for (const qterm2::Command &command : m_commands) {
        QString text = command.title;
        if (!command.category.isEmpty())
            text = QStringLiteral("%1  ·  %2").arg(command.category, command.title);
        if (!command.shortcut.isEmpty())
            text += QStringLiteral("   (%1)").arg(command.shortcut.toString(QKeySequence::NativeText));

        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, command.id);
        item->setData(Qt::UserRole + 1,
                      QStringList{ command.title, command.category, command.id,
                                   command.shortcut.toString() });
        if (!command.enabled) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setHidden(true);
        }
    }

    connect(m_edit, &QLineEdit::textChanged, this, &CommandPaletteDialog::applyFilter);
    connect(m_edit, &QLineEdit::returnPressed, this, &CommandPaletteDialog::acceptCurrent);
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *) { acceptCurrent(); });

    applyFilter(QString());
}

void CommandPaletteDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    const QStringList tokens = needle.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QListWidgetItem *firstVisible = nullptr;
    int visible = 0;

    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (!(item->flags() & Qt::ItemIsEnabled)) {
            item->setHidden(true);
            continue;
        }

        const QStringList fields = item->data(Qt::UserRole + 1).toStringList();
        bool matches = true;
        for (const QString &token : tokens) {
            bool tokenMatches = false;
            for (const QString &field : fields) {
                if (field.contains(token, Qt::CaseInsensitive)) {
                    tokenMatches = true;
                    break;
                }
            }
            if (!tokenMatches) {
                matches = false;
                break;
            }
        }

        item->setHidden(!matches);
        if (matches) {
            ++visible;
            if (!firstVisible)
                firstVisible = item;
        }
    }

    if (firstVisible)
        m_list->setCurrentItem(firstVisible);
    else
        m_list->setCurrentRow(-1);

    m_hint->setText(visible == 0
                        ? tr("Sin coincidencias · Esc para cerrar")
                        : tr("%1 comandos · Enter para ejecutar · Esc para cerrar")
                              .arg(visible));
}

void CommandPaletteDialog::acceptCurrent()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item || item->isHidden())
        return;
    m_selectedId = item->data(Qt::UserRole).toString();
    if (m_selectedId.isEmpty())
        return;
    accept();
}
