#pragma once

#include <QDialog>
#include <QStringList>

#include "domain/session.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QWidget;

class SessionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SessionDialog(const Session &session, const QStringList &groups,
                           QWidget *parent = nullptr);

    Session resultSession() const { return m_result; }

private slots:
    void updateVisibility();
    void browseKey();
    void generateKey();
    void showPublicKey();
    void copyPublicKey();
    void accept() override;

private:
    void buildGeneralTab();
    void buildAuthTab();
    void buildAdvancedTab();
    void buildForwardsTab();
    void buildTerminalTab();
    void updateProfileUi();
    void addPortForwardRow(const PortForward &forward = PortForward());
    void updateForwardRow(int row);
    bool readPortForwards(QVector<PortForward> &forwards);
    void load(const Session &session);
    void fillColorCombo();
    void updateAuthUi();
    bool validate();
    QString expandedKeyPath() const;

    Session m_session;
    Session m_result;
    QStringList m_groups;

    QTabWidget *m_tabs = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    QLineEdit *m_name = nullptr;
    QComboBox *m_group = nullptr;
    QComboBox *m_type = nullptr;
    QComboBox *m_color = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_startDir = nullptr;
    QComboBox *m_shell = nullptr;

    QComboBox *m_auth = nullptr;
    QLineEdit *m_key = nullptr;
    QPushButton *m_browseKey = nullptr;
    QPushButton *m_generateKey = nullptr;
    QPushButton *m_showKey = nullptr;
    QPushButton *m_copyKey = nullptr;
    QLabel *m_authHint = nullptr;
    QLabel *m_keyStatus = nullptr;

    QLineEdit *m_proxyJump = nullptr;
    QLineEdit *m_extraArgs = nullptr;
    QLineEdit *m_initialCommand = nullptr;
    QComboBox *m_hostKeyPolicy = nullptr;
    QSpinBox *m_connectTimeout = nullptr;
    QCheckBox *m_favorite = nullptr;
    QCheckBox *m_x11 = nullptr;
    QCheckBox *m_agentForward = nullptr;
    QCheckBox *m_keepAlive = nullptr;
    QCheckBox *m_autoReconnect = nullptr;
    QPlainTextEdit *m_notes = nullptr;

    QCheckBox *m_profileEnabled = nullptr;
    QFontComboBox *m_profileFont = nullptr;
    QSpinBox *m_profileFontSize = nullptr;
    QSpinBox *m_profileScrollback = nullptr;
    QComboBox *m_profileCursor = nullptr;
    QCheckBox *m_profileBlink = nullptr;

    QTableWidget *m_forwards = nullptr;
    QPushButton *m_addForward = nullptr;
    QPushButton *m_removeForward = nullptr;

    QWidget *m_sshBox = nullptr;
    QWidget *m_localBox = nullptr;
    QWidget *m_keyRow = nullptr;
    QWidget *m_proxyRow = nullptr;
    QWidget *m_initialRow = nullptr;
    QWidget *m_forwardsPage = nullptr;
};
