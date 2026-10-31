#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;

class NetworkDiscoveryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NetworkDiscoveryDialog(QWidget *parent = nullptr);

private slots:
    void startScan();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_scan = nullptr;
};
