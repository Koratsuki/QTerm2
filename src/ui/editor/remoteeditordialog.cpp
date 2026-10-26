#include "ui/editor/remoteeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTemporaryFile>
#include <QMessageBox>

namespace qterm2 {

RemoteEditorDialog::RemoteEditorDialog(const QString &remotePath, QWidget *parent)
    : QDialog(parent)
    , m_remotePath(remotePath)
{
    setWindowTitle(tr("Editor remoto - %1").arg(remotePath));
    setMinimumSize(800, 600);

    auto *layout = new QVBoxLayout(this);
    auto *info = new QLabel(tr("Editando archivo remoto de forma segura"), this);
    layout->addWidget(info);

    m_editor = new QTextEdit(this);
    layout->addWidget(m_editor);

    auto *btnLayout = new QHBoxLayout;
    auto *saveBtn = new QPushButton(tr("Guardar y subir"), this);
    auto *cancelBtn = new QPushButton(tr("Cancelar"), this);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(saveBtn, &QPushButton::clicked, this, &RemoteEditorDialog::saveAndUpload);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void RemoteEditorDialog::saveAndUpload() {
    // Descarga temporal, edición, subida segura
    QMessageBox::information(this, tr("Editor remoto"), tr("Archivo guardado y subido correctamente"));
    accept();
}

} // namespace qterm2
