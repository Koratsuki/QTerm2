#pragma once

#include <QDialog>

#include "tunnels/tunnelmanager.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace qterm2 { class SessionStore; }

class TunnelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TunnelDialog(qterm2::TunnelManager *manager,
                          qterm2::SessionStore *sessions,
                          QWidget *parent = nullptr);

private slots:
    void refreshTable();
    void addFromSession();
    void addManual();
    void editSelected();
    void startSelected();
    void stopSelected();
    void removeSelected();
    void toggleAutoReconnectSelected();
    void showLog();

private:
    QString selectedTunnelId() const;

    qterm2::TunnelManager *manager() const { return m_manager; }
    qterm2::TunnelManager *m_manager = nullptr;
    qterm2::SessionStore *m_sessions = nullptr;
    QTableWidget *m_table = nullptr;
};
