#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;

class AppLockDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AppLockDialog(QWidget *parent = nullptr);
    static bool unlock(QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    QLineEdit *m_pass = nullptr;
    QLabel *m_error = nullptr;
};
