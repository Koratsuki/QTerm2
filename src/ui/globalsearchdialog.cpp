#include "ui/globalsearchdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kMaxResults = 1000;
}

GlobalSearchDialog::GlobalSearchDialog(const QVector<GlobalSearchTarget> &targets,
                                       QWidget *parent)
    : QDialog(parent)
    , m_targets(targets)
{
    setWindowTitle(tr("Buscar en todas las pestañas"));
    resize(760, 480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("GlobalSearchEdit"));
    m_edit->setPlaceholderText(tr("Texto a buscar en el scrollback de todas las pestañas…"));
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("GlobalSearchList"));
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_hint = new QLabel(tr("Escribe para buscar · Enter para ir al resultado"), this);
    m_hint->setObjectName(QStringLiteral("GlobalSearchHint"));
    layout->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_edit, &QLineEdit::textChanged, this, &GlobalSearchDialog::updateResults);
    connect(m_edit, &QLineEdit::returnPressed, this, &GlobalSearchDialog::acceptCurrent);
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *) { acceptCurrent(); });

    updateResults(QString());
}

void GlobalSearchDialog::updateResults(const QString &text)
{
    m_query = text.trimmed();
    m_list->clear();
    m_targetIndex = -1;
    m_line = -1;

    if (m_query.isEmpty()) {
        m_hint->setText(tr("Escribe para buscar · Enter para ir al resultado"));
        return;
    }

    int matches = 0;
    bool truncated = false;
    for (int targetIndex = 0; targetIndex < m_targets.size() && !truncated;
         ++targetIndex) {
        const GlobalSearchTarget &target = m_targets.at(targetIndex);
        const QStringList lines = target.text.split(QLatin1Char('\n'));
        for (int line = 0; line < lines.size(); ++line) {
            const QString &content = lines.at(line);
            if (!content.contains(m_query, Qt::CaseInsensitive))
                continue;

            QString preview = content.trimmed();
            if (preview.size() > 160)
                preview = preview.left(157) + QStringLiteral("…");

            auto *item = new QListWidgetItem(
                QStringLiteral("%1 · línea %2 — %3").arg(target.label).arg(line + 1).arg(preview),
                m_list);
            item->setData(Qt::UserRole, targetIndex);
            item->setData(Qt::UserRole + 1, line);
            item->setToolTip(content);

            ++matches;
            if (matches >= kMaxResults) {
                truncated = true;
                break;
            }
        }
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    if (matches == 0) {
        m_hint->setText(tr("Sin coincidencias · Esc para cerrar"));
    } else if (truncated) {
        m_hint->setText(tr("%1 resultados (limitado a %2) · Enter para ir al resultado")
                            .arg(matches)
                            .arg(kMaxResults));
    } else {
        m_hint->setText(tr("%1 coincidencias en %2 pestañas · Enter para ir al resultado")
                            .arg(matches)
                            .arg(m_targets.size()));
    }
}

void GlobalSearchDialog::acceptCurrent()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    m_targetIndex = item->data(Qt::UserRole).toInt();
    m_line = item->data(Qt::UserRole + 1).toInt();
    accept();
}
