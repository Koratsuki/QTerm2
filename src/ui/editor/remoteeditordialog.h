#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

namespace qterm2 {

class RemoteEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RemoteEditorDialog(const QString &remotePath, QWidget *parent = nullptr);

private slots:
    void saveAndUpload();

private:
    QString m_remotePath;
    QString m_localTempPath;
    QTextEdit *m_editor;
};

} // namespace qterm2
