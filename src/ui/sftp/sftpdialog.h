#pragma once

#include <QDialog>

#include "domain/session.h"
#include "sftp/sftpsession.h"
#include "sftp/transferqueue.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QProgressBar;
class QListWidget;

namespace qterm2 { class SessionStore; }

class SftpDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SftpDialog(qterm2::SessionStore *sessions, QWidget *parent = nullptr);
    ~SftpDialog() override;

private slots:
    void connectSelectedSession();
    void disconnectSession();
    void refresh();
    void goUp();
    void navigateTo(const QString &path);
    void onItemActivated(int row, int column);
    void downloadSelected();
    void uploadFile();
    void makeDirectory();
    void removeSelected();
    void renameSelected();
    void editSelected();
    void showTransfers();
    void refreshTransfersView();

private:
    void setBusy(bool busy, const QString &status = QString());
    void populateTable(const QVector<qterm2::RemoteFile> &files);
    qterm2::RemoteFile selectedFile() const;
    void ensureEndpoint();

    qterm2::SessionStore *m_sessions = nullptr;
    qterm2::SftpSession m_sftp;
    qterm2::TransferQueue m_queue;
    Session m_currentSession;
    QString m_currentPath = QStringLiteral(".");

    QComboBox *m_sessionCombo = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;

    QDialog *m_transfersDialog = nullptr;
    QListWidget *m_transfersList = nullptr;
};
