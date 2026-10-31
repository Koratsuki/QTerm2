#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;
class QTimer;

class ServerMonitor : public QDialog
{
    Q_OBJECT
public:
    explicit ServerMonitor(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_refresh = nullptr;
    QTimer *m_timer = nullptr;
};
