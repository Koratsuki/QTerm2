#include "ui/sidebar.h"

using qterm2::SessionStore;

#include "domain/session.h"
#include "sessions/sessionstore.h"
#include "ui/theme.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

enum Roles {
    SessionIdRole = Qt::UserRole + 1,
    GroupRole,
    TargetRole,
    ColorRole,
    TypeRole,
    IsGroupRole,
};

class SidebarDelegate : public QStyledItemDelegate
{
public:
    explicit SidebarDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.data(IsGroupRole).toBool())
            return QSize(option.rect.width(), 30);
        return QSize(option.rect.width(), 46);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        const bool group = index.data(IsGroupRole).toBool();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool dark = Theme::currentMode() == Theme::Mode::Dark;

        const QColor text = dark ? QColor(0xd8, 0xde, 0xe9) : QColor(0x24, 0x29, 0x2e);
        const QColor dim  = dark ? QColor(0x83, 0x8c, 0x9b) : QColor(0x6a, 0x73, 0x7d);
        const QColor accent = dark ? QColor(0x3b, 0x82, 0xf6) : QColor(0x0b, 0x63, 0xd3);
        const QColor hoverBg = dark ? QColor(0x26, 0x2b, 0x34) : QColor(0xee, 0xf1, 0xf5);

        const QRect r = option.rect.adjusted(6, 2, -6, -2);

        if (group) {
            QFont f = option.font;
            f.setPointSize(8);
            f.setBold(true);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 0.9);
            p->setFont(f);
            p->setPen(dim);
            p->drawText(r.adjusted(4, 6, -4, -4),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        index.data(Qt::DisplayRole).toString().toUpper());

            if (index.model()->hasChildren(index)) {
                const QTreeView *view = qobject_cast<const QTreeView *>(option.widget);
                const bool expanded = view ? view->isExpanded(index) : true;
                p->setPen(dim);
                p->drawText(r.adjusted(0, 5, -4, -5),
                            Qt::AlignRight | Qt::AlignVCenter,
                            expanded ? QStringLiteral("\u25be") : QStringLiteral("\u25b8"));
            }
            p->restore();
            return;
        }

        if (selected || hovered) {
            p->setPen(Qt::NoPen);
            p->setBrush(selected ? accent : hoverBg);
            p->drawRoundedRect(r, 7, 7);
        }

        const QColor dot(index.data(ColorRole).toString());
        if (dot.isValid()) {
            p->setPen(Qt::NoPen);
            p->setBrush(dot);
            p->drawEllipse(QPointF(r.left() + 13, r.top() + 15), 4.0, 4.0);
        }

        QFont nameFont = option.font;
        nameFont.setPointSize(10);
        nameFont.setWeight(QFont::DemiBold);
        p->setFont(nameFont);
        p->setPen(selected ? QColor(Qt::white) : text);
        const QRect nameRect(r.left() + 25, r.top() + 5, r.width() - 33, 18);
        const QString name = index.data(Qt::DisplayRole).toString();
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, nameRect.width()));

        QFont targetFont = option.font;
        targetFont.setPointSize(8);
        p->setFont(targetFont);
        p->setPen(selected ? QColor(255, 255, 255, 200) : dim);
        const QString target = index.data(TargetRole).toString();
        const QRect targetRect(r.left() + 25, r.top() + 23, r.width() - 33, 15);
        p->drawText(targetRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(targetFont).elidedText(target, Qt::ElideRight, targetRect.width()));

        const bool local = index.data(TypeRole).toInt() == int(Session::Type::Local);
        if (local) {
            p->setPen(QColor(selected ? QColor(255, 255, 255, 180) : dim));
            QFont badge = option.font;
            badge.setPointSize(7);
            badge.setBold(true);
            p->setFont(badge);
            p->drawText(r.adjusted(0, 4, -8, 0), Qt::AlignRight | Qt::AlignTop,
                        QStringLiteral("LOCAL"));
        }

        p->restore();
    }
};

} // namespace

Sidebar::Sidebar(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("Sidebar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(210);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(2, 0, 0, 0);
    auto *title = new QLabel(tr("SESIONES"), this);
    title->setObjectName(QStringLiteral("SidebarTitle"));
    header->addWidget(title);
    header->addStretch(1);

    auto *addButton = new QToolButton(this);
    addButton->setText(QStringLiteral("+"));
    addButton->setToolTip(tr("Añadir una conexión"));
    addButton->setPopupMode(QToolButton::InstantPopup);
    addButton->setFixedSize(24, 24);
    auto *addMenu = new QMenu(addButton);
    QAction *sshAction = addMenu->addAction(tr("Nueva conexión SSH…"));
    QAction *localAction = addMenu->addAction(tr("Nueva terminal local"));
    addButton->setMenu(addMenu);
    header->addWidget(addButton);

    auto *hideButton = new QToolButton(this);
    hideButton->setText(QStringLiteral("\u2039"));
    hideButton->setToolTip(tr("Ocultar panel de sesiones (Ctrl+B)"));
    hideButton->setFixedSize(24, 24);
    header->addWidget(hideButton);
    layout->addLayout(header);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Buscar servidor, usuario…"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setIndentation(8);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(false);
    m_tree->setMouseTracking(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setItemDelegate(new SidebarDelegate(m_tree));
    m_tree->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_tree, 1);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(6);
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("Hint"));
    footer->addWidget(m_count, 1);

    auto *localShortcut = new QToolButton(this);
    localShortcut->setText(tr("Terminal local"));
    localShortcut->setToolTip(tr("Abrir una terminal local (Ctrl+T)"));
    localShortcut->setObjectName(QStringLiteral("SidebarAction"));
    footer->addWidget(localShortcut);

    auto *newSsh = new QToolButton(this);
    newSsh->setText(tr("Nueva SSH"));
    newSsh->setToolTip(tr("Configurar un nuevo servidor SSH"));
    newSsh->setObjectName(QStringLiteral("SidebarPrimary"));
    footer->addWidget(newSsh);
    layout->addLayout(footer);

    connect(m_filter, &QLineEdit::textChanged, this, [this] { updateFilter(); });
    connect(hideButton, &QToolButton::clicked, this, &Sidebar::hideRequested);
    connect(addButton->menu(), &QMenu::triggered, this, [this, sshAction, localAction](QAction *a) {
        if (a == sshAction)
            emit newSshRequested();
        else if (a == localAction)
            emit newLocalRequested();
    });
    connect(localShortcut, &QToolButton::clicked, this, &Sidebar::newLocalRequested);
    connect(newSsh, &QToolButton::clicked, this, &Sidebar::newSshRequested);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (!item)
            return;
        if (item->data(0, IsGroupRole).toBool()) {
            item->setExpanded(!item->isExpanded());
            return;
        }
        const QString id = item->data(0, SessionIdRole).toString();
        if (!id.isEmpty())
            emit connectRequested(id);
    });
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (!current)
            return;
        if (!current->data(0, IsGroupRole).toBool())
            m_selectedId = current->data(0, SessionIdRole).toString();
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &Sidebar::showContextMenu);

    if (m_store)
        connect(m_store, &SessionStore::changed, this, &Sidebar::refresh);

    refresh();
}

void Sidebar::refresh()
{
    m_expandedGroups.clear();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        if (item->isExpanded())
            m_expandedGroups.insert(item->text(0).section(QStringLiteral("  ·"), 0, 0));
    }

    const QString keep = m_selectedId;
    buildTree();
    if (!keep.isEmpty())
        selectSession(keep);
    updateFilter();
}

QTreeWidgetItem *Sidebar::itemForSession(const QString &id) const
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *group = m_tree->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem *child = group->child(j);
            if (child->data(0, SessionIdRole).toString() == id)
                return child;
        }
    }
    return nullptr;
}

void Sidebar::buildTree()
{
    m_tree->clear();
    if (!m_store)
        return;

    QVector<Session> sessions = m_store->sessions();
    std::sort(sessions.begin(), sessions.end(), [](const Session &a, const Session &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    const auto groupNameFor = [](const Session &s) {
        return s.group.trimmed().isEmpty() ? QStringLiteral("Servidores")
                                           : s.group.trimmed();
    };

    const auto addSession = [this](QTreeWidgetItem *parent, const Session &s,
                                   const QString &groupName) {
        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, s.name);
        item->setData(0, SessionIdRole, s.id);
        item->setData(0, GroupRole, groupName);
        item->setData(0, TypeRole, int(s.type));
        item->setData(0, ColorRole, Session::colorForTag(s.colorTag));
        item->setData(0, TargetRole,
                      s.isLocal() ? tr("Terminal local") : s.displayTarget());
        item->setToolTip(0, QStringLiteral("%1\n%2\nAutenticación: %3")
                                .arg(s.name, s.isLocal() ? s.shell : s.displayTarget(),
                                     s.isLocal() ? tr("—") : Session::authName(s.auth)));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    };

    const auto addGroup = [this, &addSession](const QString &title,
                                              const QVector<Session> &list) {
        if (list.isEmpty())
            return;

        auto *groupItem = new QTreeWidgetItem(m_tree);
        groupItem->setText(0, QStringLiteral("%1  · %2").arg(title).arg(list.size()));
        groupItem->setData(0, IsGroupRole, true);
        groupItem->setFlags(Qt::ItemIsEnabled);
        QFont gf = groupItem->font(0);
        gf.setBold(true);
        groupItem->setFont(0, gf);
        groupItem->setFirstColumnSpanned(true);

        for (const Session &s : list)
            addSession(groupItem, s, s.group.trimmed().isEmpty()
                                         ? QStringLiteral("Servidores")
                                         : s.group.trimmed());

        const bool expand = m_expandedGroups.isEmpty()
                            || m_expandedGroups.contains(title)
                            || title == QLatin1String("Favoritos")
                            || title == QLatin1String("Recientes")
                            || title == QLatin1String("Servidores")
                            || title == QLatin1String("Locales");
        groupItem->setExpanded(expand);
    };

    QVector<Session> favorites;
    for (const Session &s : sessions) {
        if (s.favorite)
            favorites.append(s);
    }
    addGroup(tr("Favoritos"), favorites);

    QVector<Session> recents;
    for (const Session &s : sessions) {
        if (s.lastUsedAt.isValid())
            recents.append(s);
    }
    std::sort(recents.begin(), recents.end(), [](const Session &a, const Session &b) {
        return a.lastUsedAt > b.lastUsedAt;
    });
    if (recents.size() > 8)
        recents.resize(8);
    addGroup(tr("Recientes"), recents);

    QStringList groups;
    for (const Session &s : sessions) {
        const QString g = groupNameFor(s);
        if (!groups.contains(g))
            groups.append(g);
    }
    std::sort(groups.begin(), groups.end(), [](const QString &a, const QString &b) {
        if (a == QLatin1String("Locales")) return true;
        if (b == QLatin1String("Locales")) return false;
        if (a == QLatin1String("Servidores")) return true;
        if (b == QLatin1String("Servidores")) return false;
        return a.localeAwareCompare(b) < 0;
    });

    for (const QString &groupName : groups) {
        QVector<Session> members;
        for (const Session &s : sessions) {
            if (groupNameFor(s) == groupName)
                members.append(s);
        }
        addGroup(groupName, members);
    }

    updateCount();
}

void Sidebar::updateFilter()
{
    const QString needle = m_filter ? m_filter->text().trimmed() : QString();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *group = m_tree->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem *child = group->child(j);
            const bool matches = needle.isEmpty()
                                 || child->text(0).contains(needle, Qt::CaseInsensitive)
                                 || child->data(0, TargetRole).toString().contains(needle, Qt::CaseInsensitive)
                                 || child->data(0, GroupRole).toString().contains(needle, Qt::CaseInsensitive);
            child->setHidden(!matches);
            anyVisible = anyVisible || matches;
        }
        if (group->childCount() == 0)
            anyVisible = needle.isEmpty();
        group->setHidden(!anyVisible);
        if (!needle.isEmpty() && anyVisible)
            group->setExpanded(true);
    }
}

void Sidebar::updateCount()
{
    if (!m_store) {
        m_count->setText(QString());
        return;
    }
    int servers = 0;
    int locals = 0;
    for (const Session &s : m_store->sessions())
        (s.isLocal() ? locals : servers)++;
    m_count->setText(tr("%1 servidores · %2 local").arg(servers).arg(locals));
}

void Sidebar::selectSession(const QString &id)
{
    QTreeWidgetItem *item = itemForSession(id);
    if (!item)
        return;
    m_selectedId = id;
    if (item->parent())
        item->parent()->setExpanded(true);
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
}

void Sidebar::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item || item->data(0, IsGroupRole).toBool()) {
        QMenu menu(this);
        menu.addAction(tr("Nueva conexión SSH…"), this, &Sidebar::newSshRequested);
        menu.addAction(tr("Nueva terminal local"), this, &Sidebar::newLocalRequested);
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
        return;
    }

    const QString id = item->data(0, SessionIdRole).toString();
    if (!m_store || !m_store->contains(id))
        return;
    const Session s = m_store->sessionById(id);

    QMenu menu(this);
    QAction *connectAction = menu.addAction(tr("Conectar"));
    QAction *editAction = menu.addAction(tr("Editar…"));
    QAction *favoriteAction = menu.addAction(
        s.favorite ? tr("Quitar de favoritos") : tr("Añadir a favoritos"));
    QAction *duplicateAction = menu.addAction(tr("Duplicar"));
    menu.addSeparator();
    QAction *keyAction = nullptr;
    QAction *copyCommandAction = nullptr;
    if (!s.isLocal()) {
        keyAction = menu.addAction(tr("Instalar llave pública en el servidor…"));
        copyCommandAction = menu.addAction(tr("Copiar comando SSH"));
        menu.addSeparator();
    }
    QAction *deleteAction = menu.addAction(tr("Eliminar…"));

    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == connectAction)        emit connectRequested(id);
    else if (chosen == editAction)      emit editRequested(id);
    else if (chosen == favoriteAction)  emit favoriteToggleRequested(id);
    else if (chosen == duplicateAction) emit duplicateRequested(id);
    else if (chosen == keyAction)       emit installKeyRequested(id);
    else if (chosen == copyCommandAction) emit copyCommandRequested(id);
    else if (chosen == deleteAction)    emit deleteRequested(id);
}

void Sidebar::repaintItems()
{
    m_tree->viewport()->update();
}
