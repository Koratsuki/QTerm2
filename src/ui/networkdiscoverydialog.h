#pragma once

#include <QDialog>
#include <QProcess>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class NetworkDiscoveryDialog : public QDialog
{
    Q_OBJECT
public:
    struct DiscoveredHost {
        QString host;
        QString address;
    };

    explicit NetworkDiscoveryDialog(QWidget *parent = nullptr);
    ~NetworkDiscoveryDialog() override;

    DiscoveredHost selectedHost() const { return m_selected; }

signals:
    void createSessionRequested(const QString &hostOrAddress);

private slots:
    void startDiscovery();
    void stopDiscovery();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onReadyRead();
    void copySelected();
    void createSessionForSelected();

private:
    void appendHost(const QString &host, const QString &address, const QString &source, const QString &details);
    QString buildCommand(QStringList &args) const;

    QComboBox *m_method = nullptr;
    QLineEdit *m_target = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_stop = nullptr;
    QLabel *m_status = nullptr;
    QTableWidget *m_table = nullptr;
    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    DiscoveredHost m_selected;
};
