#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLabel;
class QLineEdit;
class QListWidget;

/** One searchable terminal snapshot. */
struct GlobalSearchTarget
{
    QString label;
    QString text;       // full scrollback, one line per entry
};

/**
 * Searches the scrollback of every open terminal at once.
 *
 * The dialog works on a snapshot: MainWindow passes the current text of each
 * terminal, and the dialog reports which target and absolute line the user
 * selected. It never touches live widgets, so the search cannot block the PTYs.
 */
class GlobalSearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GlobalSearchDialog(const QVector<GlobalSearchTarget> &targets,
                                QWidget *parent = nullptr);

    QString query() const { return m_query; }
    /** Index into the vector passed to the constructor, or -1. */
    int selectedTargetIndex() const { return m_targetIndex; }
    /** Absolute scrollback line inside that terminal, or -1. */
    int selectedLine() const { return m_line; }

private:
    void updateResults(const QString &text);
    void acceptCurrent();

    QVector<GlobalSearchTarget> m_targets;
    QLineEdit *m_edit = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
    QString m_query;
    int m_targetIndex = -1;
    int m_line = -1;
};
