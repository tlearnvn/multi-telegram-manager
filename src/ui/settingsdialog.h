#pragma once

#include <QDialog>

class AccountManager;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

/*!
 * \brief Hộp thoại cài đặt: giao diện, thông báo, tải tệp, proxy, TDLib,
 *        khoá API và dọn dữ liệu.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AccountManager *manager, QWidget *parent = nullptr);

signals:
    void statusMessage(const QString &text);

private slots:
    void applyAppearance();
    void applyProxy();
    void reloadTdlib();
    void clearCache();
    void saveApiCredentials();

private:
    QWidget *buildAppearancePage();
    QWidget *buildNotificationPage();
    QWidget *buildDownloadPage();
    QWidget *buildProxyPage();
    QWidget *buildAdvancedPage();
    void updateProxyFields();
    void updateStorageLabel();

    AccountManager *m_manager;

    // Giao diện
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_accentCombo = nullptr;
    QSlider *m_fontSlider = nullptr;
    QLabel *m_fontLabel = nullptr;
    QCheckBox *m_compactList = nullptr;
    QCheckBox *m_groupAvatars = nullptr;

    // Thông báo
    QCheckBox *m_notifyEnabled = nullptr;
    QCheckBox *m_notifyPreview = nullptr;
    QCheckBox *m_trayEnabled = nullptr;
    QCheckBox *m_closeToTray = nullptr;
    QCheckBox *m_startMinimized = nullptr;

    // Tải tệp
    QCheckBox *m_autoPhotos = nullptr;
    QSpinBox *m_autoMaxMb = nullptr;
    QLabel *m_storageLabel = nullptr;

    // Proxy
    QComboBox *m_proxyKind = nullptr;
    QLineEdit *m_proxyServer = nullptr;
    QSpinBox *m_proxyPort = nullptr;
    QLineEdit *m_proxyUser = nullptr;
    QLineEdit *m_proxyPassword = nullptr;
    QLineEdit *m_proxySecret = nullptr;
    QLabel *m_proxyStatus = nullptr;

    // Nâng cao
    QLineEdit *m_apiId = nullptr;
    QLineEdit *m_apiHash = nullptr;
    QLineEdit *m_tdPath = nullptr;
    QComboBox *m_verbosity = nullptr;
    QLabel *m_tdStatus = nullptr;
};
