#pragma once

#include <QDialog>
#include <QSet>
#include <QVector>

#include "ssh/sshconfig.h"

class QLineEdit;
class QLabel;
class QTableWidget;

/**
 * Lets the user choose which concrete hosts from an OpenSSH config become
 * QTerm2 sessions. Hosts already present in the catalog are shown disabled.
 */
class SshConfigDialog : public QDialog
{
    Q_OBJECT
public:
    SshConfigDialog(const QVector<qterm2::ssh::SshConfigEntry> &entries,
                    const QSet<QString> &existingTargets,
                    QWidget *parent = nullptr);

    QVector<qterm2::ssh::SshConfigEntry> selectedEntries() const;

private:
    struct Row {
        qterm2::ssh::SshConfigEntry entry;
        bool existing = false;
    };

    void applyFilter(const QString &text);
    void setAllChecked(bool checked);

    QVector<Row> m_rows;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_summary = nullptr;
};
