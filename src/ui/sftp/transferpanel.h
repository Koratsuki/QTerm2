#pragma once

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QProgressBar>

namespace qterm2 {

class TransferQueue;

class TransferPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransferPanel(TransferQueue *queue, QWidget *parent = nullptr);

private slots:
    void pauseTransfer();
    void resumeTransfer();
    void cancelTransfer();

private:
    TransferQueue *m_queue;
    QTableView *m_table;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_cancelBtn;
};

} // namespace qterm2
