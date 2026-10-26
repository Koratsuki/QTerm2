#pragma once

#include <QDialog>
#include <QTableView>
#include <QPushButton>

namespace qterm2 {

class TunnelManager;

class TunnelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TunnelDialog(TunnelManager *manager, QWidget *parent = nullptr);

private slots:
    void addTunnel();
    void startTunnel();
    void stopTunnel();

private:
    TunnelManager *m_manager;
    QTableView *m_table;
    QPushButton *m_addBtn;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
};

} // namespace qterm2
