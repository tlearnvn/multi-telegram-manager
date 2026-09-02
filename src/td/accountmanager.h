#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

class TdAccount;

/*!
 * \brief Quản lý danh sách tài khoản và phân phối dữ liệu từ TDLib.
 *
 * Toàn bộ tiến trình chỉ có một luồng nhận (TdTransport). Lớp này lắng nghe
 * luồng đó rồi chuyển từng phản hồi cho đúng TdAccount dựa vào "@client_id".
 *
 * Danh sách tài khoản được lưu ở data/accounts.json để lần sau mở lên là có
 * sẵn — không cần đăng nhập lại vì TDLib đã giữ phiên trong thư mục riêng của
 * từng tài khoản.
 */
class AccountManager : public QObject
{
    Q_OBJECT

public:
    explicit AccountManager(QObject *parent = nullptr);
    ~AccountManager() override;

    //! Đọc data/accounts.json và mở lại các tài khoản đã lưu.
    void loadFromDisk();

    //! Ghi danh sách tài khoản xuống đĩa.
    void saveToDisk() const;

    QList<TdAccount *> accounts() const { return m_ordered; }
    int count() const { return m_ordered.size(); }

    TdAccount *accountBySlug(const QString &slug) const;
    TdAccount *accountAt(int index) const;
    TdAccount *activeAccount() const { return m_active; }
    int indexOf(TdAccount *account) const;

    //! Tạo tài khoản mới (chưa đăng nhập) và mở client TDLib cho nó.
    TdAccount *createAccount(const QString &label = QString());

    //! Đăng xuất, đóng client và xoá toàn bộ dữ liệu cục bộ của tài khoản.
    void removeAccount(const QString &slug, bool alsoLogOut = true);

    void setActiveAccount(TdAccount *account);
    void setActiveIndex(int index);

    //! Tổng số cuộc trò chuyện chưa đọc trên tất cả tài khoản.
    int totalUnread() const;

    //! Mở lại toàn bộ client (dùng sau khi nạp được TDLib hoặc đổi proxy).
    void reopenAll();

    //! Áp dụng lại cấu hình proxy cho mọi tài khoản đang chạy.
    void applyProxyToAll();

    //! Đặt trạng thái trực tuyến cho mọi tài khoản.
    void setOnlineAll(bool online);

signals:
    void accountsChanged();
    void activeAccountChanged(TdAccount *account);
    void accountAdded(TdAccount *account);
    void accountRemoved(const QString &slug);
    void aggregateUnreadChanged(int total);

private slots:
    void onTransportMessage(int clientId, const QJsonObject &object);

private:
    void wireAccount(TdAccount *account);
    QString allocateSlug() const;

    QList<TdAccount *> m_ordered;
    QHash<QString, TdAccount *> m_bySlug;
    TdAccount *m_active = nullptr;
};
