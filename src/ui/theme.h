#pragma once

#include <QColor>
#include <QObject>
#include <QString>

/*!
 * \brief Bảng màu và stylesheet của toàn ứng dụng.
 *
 * Giao diện dùng style "Fusion" của Qt làm nền (đảm bảo giống nhau trên
 * Windows và Linux) rồi phủ lên một bảng màu + QSS riêng để có dáng vẻ hiện
 * đại: góc bo, nền phẳng, màu nhấn tuỳ chọn, hai chế độ sáng/tối.
 */
class Theme : public QObject
{
    Q_OBJECT

public:
    struct Colors
    {
        bool dark = true;

        QColor windowBg;
        QColor railBg;        //!< thanh tài khoản bên trái
        QColor sidebarBg;     //!< danh sách chat
        QColor chatBg;        //!< khung hội thoại
        QColor panelBg;       //!< hộp thoại, panel thông tin
        QColor cardBg;
        QColor hoverBg;
        QColor selectedBg;
        QColor divider;
        QColor shadow;

        QColor textPrimary;
        QColor textSecondary;
        QColor textMuted;
        QColor textOnAccent;

        QColor accent;
        QColor accentHover;
        QColor accentSoft;

        QColor bubbleIn;
        QColor bubbleOut;
        QColor bubbleInText;
        QColor bubbleOutText;
        QColor bubbleMeta;

        QColor badge;
        QColor badgeMuted;
        QColor danger;
        QColor success;
        QColor warning;
        QColor link;
    };

    static Theme &instance();

    //! Áp dụng bảng màu + stylesheet cho qApp theo cấu hình hiện tại.
    void apply();

    const Colors &colors() const { return m_colors; }
    bool isDark() const { return m_colors.dark; }

    //! Màu tên người gửi trong nhóm (7 màu quay vòng như Telegram).
    QColor senderColor(int index) const;

    //! Màu nền avatar mặc định khi chưa có ảnh.
    QColor avatarColor(int index) const;

    QString styleSheet() const;

signals:
    void changed();

private:
    Theme();

    void buildColors();

    Colors m_colors;
};
