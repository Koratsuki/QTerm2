#pragma once

#include <QDialog>
#include <QVector>

#include "ssh/knownhosts.h"

class QLabel;
class QLineEdit;
class QTableWidget;

/** Read-only viewer for `~/.ssh/known_hosts` with selective deletion. */
class KnownHostsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KnownHostsDialog(const QString &path, QWidget *parent = nullptr);

private:
    void reload();
    void applyFilter(const QString &text);
    void removeSelected();

    QString m_path;
    QVector<qterm2::ssh::KnownHostEntry> m_entries;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_status = nullptr;
};
