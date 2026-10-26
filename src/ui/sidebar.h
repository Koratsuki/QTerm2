#pragma once

#include <QWidget>
#include <QSet>
#include <QString>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace qterm2 {
class SessionStore;
}

class Sidebar : public QWidget
{
    Q_OBJECT
public:
    explicit Sidebar(qterm2::SessionStore *store, QWidget *parent = nullptr);

    void refresh();
    void updateFilter();
    void selectSession(const QString &id);
    void repaintItems();

signals:
    void connectRequested(const QString &id);
    void editRequested(const QString &id);
    void deleteRequested(const QString &id);
    void duplicateRequested(const QString &id);
    void favoriteToggleRequested(const QString &id);
    void installKeyRequested(const QString &id);
    void copyCommandRequested(const QString &id);
    void newSshRequested();
    void newLocalRequested();
    void hideRequested();

private:
    void buildTree();
    void showContextMenu(const QPoint &pos);
    QTreeWidgetItem *itemForSession(const QString &id) const;
    void updateCount();

    qterm2::SessionStore *m_store = nullptr;
    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_count = nullptr;
    QSet<QString> m_expandedGroups;
    QString m_selectedId;
};
