#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QGraphicsOpacityEffect;
class QTimer;

/*!
 * \brief Thông báo nhỏ hiện lên rồi tự tắt ở góc dưới cửa sổ.
 *
 * Dùng cho các phản hồi ngắn ("Đã chép", "Đang tải tệp…") mà không cần hộp
 * thoại làm gián đoạn người dùng.
 */
class Toast : public QWidget
{
    Q_OBJECT

public:
    enum class Kind { Info, Success, Warning, Error };

    //! Hiện thông báo bên trong \a host (thường là cửa sổ chính).
    static void popup(QWidget *host, const QString &message,
                      Kind kind = Kind::Info, int milliseconds = 2600);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Toast(QWidget *host, const QString &message, Kind kind, int milliseconds);

    void reposition();

    QLabel *m_label = nullptr;
    Kind m_kind;
    QTimer *m_timer = nullptr;
    QGraphicsOpacityEffect *m_opacity = nullptr;
};
