#pragma once

#include <QWidget>

class AccountManager;
class IconButton;
class TdAccount;
class QVBoxLayout;

/*!
 * \brief Một ô tài khoản trên thanh dọc: avatar, huy hiệu chưa đọc, trạng thái.
 */
class AccountButton : public QWidget
{
    Q_OBJECT

public:
    AccountButton(TdAccount *account, bool active, QWidget *parent = nullptr);

    TdAccount *account() const { return m_account; }
    void setActive(bool active);
    void refresh();

signals:
    void activated(TdAccount *account);
    void contextRequested(TdAccount *account, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString buildTooltip() const;

    TdAccount *m_account;
    bool m_active;
    bool m_hovered = false;
};

/*!
 * \brief Thanh dọc bên trái chứa avatar của từng tài khoản.
 *
 * Đây là điểm mấu chốt của tính năng đa tài khoản: mọi tài khoản đều đang chạy
 * song song, huy hiệu số tin chưa đọc cập nhật liên tục, bấm vào là chuyển
 * ngay sang tài khoản đó mà không phải đăng nhập lại.
 */
class AccountRail : public QWidget
{
    Q_OBJECT

public:
    explicit AccountRail(AccountManager *manager, QWidget *parent = nullptr);

    void refresh();

signals:
    void addAccountRequested();
    void settingsRequested();
    void dashboardRequested();
    void accountMenuRequested(TdAccount *account, const QPoint &globalPos);

private slots:
    void applyTheme();

private:
    void rebuildButtons();
    void wireButton(AccountButton *button);

    AccountManager *m_manager;
    QVBoxLayout *m_buttonLayout = nullptr;
    QWidget *m_buttonHost = nullptr;
    IconButton *m_addButton = nullptr;
    IconButton *m_dashboardButton = nullptr;
    IconButton *m_settingsButton = nullptr;
};
