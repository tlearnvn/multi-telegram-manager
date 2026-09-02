#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

/*!
 * \brief Trình hướng dẫn lần chạy đầu.
 *
 * Hai việc bắt buộc trước khi dùng được: (1) khai báo api_id/api_hash riêng
 * của người dùng, (2) có thư viện TDLib cạnh tệp chạy. Hộp thoại này giải
 * thích bằng tiếng Việt và kiểm tra ngay tại chỗ.
 */
class SetupWizard : public QDialog
{
    Q_OBJECT

public:
    explicit SetupWizard(QWidget *parent = nullptr);

    //! true nếu người dùng đã hoàn tất và có thể bắt đầu thêm tài khoản.
    bool isReady() const { return m_ready; }

private slots:
    void checkTdlib();
    void saveCredentials();
    void goNext();
    void goBack();

private:
    void buildUi();
    void updateButtons();

    QStackedWidget *m_pages = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_tdStatus = nullptr;
    QLabel *m_apiStatus = nullptr;
    QLineEdit *m_apiIdInput = nullptr;
    QLineEdit *m_apiHashInput = nullptr;
    QLineEdit *m_tdPathInput = nullptr;
    QPushButton *m_nextButton = nullptr;
    QPushButton *m_backButton = nullptr;
    bool m_ready = false;
};
