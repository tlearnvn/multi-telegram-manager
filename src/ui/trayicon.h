#pragma once

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

/*!
 * \brief Biểu tượng khay hệ thống + thông báo desktop.
 *
 * Cho phép đóng cửa sổ mà vẫn giữ mọi tài khoản trực tuyến — điểm quan trọng
 * khi quản lý nhiều tài khoản cùng lúc.
 */
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject *parent = nullptr);

    bool isAvailable() const;
    void setVisible(bool visible);

    //! Cập nhật tổng số chat chưa đọc (hiện trong tooltip).
    void setUnreadTotal(int total);

    //! Hiện thông báo desktop. Trả về false nếu hệ thống không hỗ trợ.
    bool notify(const QString &title, const QString &body, qint64 chatId,
                const QString &accountSlug);

signals:
    //! Người dùng bấm vào biểu tượng hoặc chọn "Mở cửa sổ".
    void showWindowRequested();
    void quitRequested();
    void settingsRequested();
    //! Bấm vào một thông báo — mở đúng tài khoản và cuộc trò chuyện đó.
    void openChatRequested(const QString &accountSlug, qint64 chatId);

private:
    void rebuildMenu();

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_muteAction = nullptr;
    QString m_pendingSlug;
    qint64 m_pendingChatId = 0;
};
