#pragma once

#include "td/tdaccount.h"

#include <QDialog>

class QrView;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;

/*!
 * \brief Hộp thoại đăng nhập một tài khoản Telegram.
 *
 * Bám theo máy trạng thái của TDLib: chọn cách đăng nhập → số điện thoại → mã
 * xác thực → mật khẩu hai lớp (nếu có) → xong. Hoặc hiển thị mã QR để quét
 * bằng ứng dụng Telegram trên điện thoại.
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(TdAccount *account, QWidget *parent = nullptr);

private slots:
    void onStateChanged(TdAccount::State state);
    void onPromptChanged();
    void onQrLink(const QString &link);
    void onError(const QString &message);
    void applyTheme();
    void tickResend();

private:
    enum Page { PageChoose, PagePhone, PageCode, PagePassword, PageRegister, PageQr, PageDone };

    void buildUi();
    void showPage(Page page);
    void setBusy(bool busy);
    void setHint(const QString &text, bool isError = false);

    TdAccount *m_account;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;

    QLineEdit *m_phoneInput = nullptr;
    QLineEdit *m_codeInput = nullptr;
    QLineEdit *m_passwordInput = nullptr;
    QLineEdit *m_firstNameInput = nullptr;
    QLineEdit *m_lastNameInput = nullptr;

    QLabel *m_codeTarget = nullptr;
    QLabel *m_passwordHint = nullptr;
    QPushButton *m_resendButton = nullptr;
    QPushButton *m_primaryButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QrView *m_qrView = nullptr;
    QLabel *m_qrLinkLabel = nullptr;

    QTimer *m_resendTimer = nullptr;
    int m_resendSeconds = 0;
    Page m_current = PageChoose;
};
