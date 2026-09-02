#pragma once

#include <QWidget>

class AccountManager;
class TdAccount;
class QLabel;
class QTableWidget;
class QTimer;

/*!
 * \brief Bảng điều khiển tổng quan mọi tài khoản.
 *
 * Xem nhanh: tài khoản nào đang đăng nhập, trạng thái kết nối, số cuộc trò
 * chuyện, tin chưa đọc, dung lượng đã dùng — và thao tác nhanh (mở, đăng
 * xuất, xoá, nạp lại).
 */
class DashboardPane : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPane(AccountManager *manager, QWidget *parent = nullptr);

signals:
    void closeRequested();
    void addAccountRequested();
    void openAccountRequested(TdAccount *account);
    void loginRequested(TdAccount *account);
    void statusMessage(const QString &text);

private slots:
    void refresh();
    void applyTheme();

private:
    void buildUi();
    void updateTotals();

    AccountManager *m_manager;
    QTableWidget *m_table = nullptr;
    QLabel *m_totals = nullptr;
    QLabel *m_storageLabel = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
