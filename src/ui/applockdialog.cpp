#include "ui/applockdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSettings>

AppLockDialog::AppLockDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Application Lock"));
    setModal(true);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password"), m_pass);
    layout->addLayout(form);
    m_error = new QLabel(this);
    m_error->setStyleSheet(QStringLiteral("color: red;"));
    layout->addWidget(m_error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AppLockDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void AppLockDialog::accept()
{
    QSettings s(QStringLiteral("QTerm2"), QStringLiteral("QTerm2"));
    const QString saved = s.value(QStringLiteral("app/lockPassword")).toString();
    if (saved.isEmpty()) {
        s.setValue(QStringLiteral("app/lockPassword"), m_pass->text());
        QDialog::accept();
        return;
    }
    if (m_pass->text() == saved) {
        QDialog::accept();
    } else {
        m_error->setText(tr("Incorrect password"));
    }
}

bool AppLockDialog::unlock(QWidget *parent)
{
    QSettings s(QStringLiteral("QTerm2"), QStringLiteral("QTerm2"));
    const QString saved = s.value(QStringLiteral("app/lockPassword")).toString();
    if (saved.isEmpty())
        return true;
    AppLockDialog dlg(parent);
    return dlg.exec() == QDialog::Accepted;
}
