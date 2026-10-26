#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include "core/commandregistry.h"

class QLabel;
class QLineEdit;
class QListWidget;

/**
 * Fuzzy-ish command launcher: filters by title, category and id, and runs the
 * selected command through the registry.
 *
 * The dialog only chooses an id; MainWindow executes it, so plugin commands and
 * native actions follow exactly the same path.
 */
class CommandPaletteDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CommandPaletteDialog(const QVector<qterm2::Command> &commands,
                                  QWidget *parent = nullptr);

    QString selectedCommandId() const { return m_selectedId; }

private:
    void applyFilter(const QString &text);
    void acceptCurrent();

    QVector<qterm2::Command> m_commands;
    QLineEdit *m_edit = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
    QString m_selectedId;
};
