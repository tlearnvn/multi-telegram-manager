#pragma once

#include <QColor>
#include <QPixmap>
#include <QRect>
#include <QString>

class QPainter;

/*!
 * \brief Vẽ ảnh đại diện tròn.
 *
 * Nếu đã tải được ảnh thì dùng ảnh (cắt tròn), nếu chưa thì vẽ vòng tròn màu
 * kèm chữ cái đầu — giống cách Telegram làm, và luôn có gì đó để nhìn ngay cả
 * khi ảnh còn đang tải.
 */
namespace Avatar {

struct Options
{
    QString photoPath;
    QString initials;
    int colorIndex = 0;
    bool showOnlineDot = false;
    bool online = false;
    QColor ringColor;      //!< viền quanh avatar (dùng cho tài khoản đang chọn)
    int ringWidth = 0;
};

void paint(QPainter *painter, const QRect &rect, const Options &options);

QPixmap make(int diameter, const Options &options, qreal devicePixelRatio = 1.0);

//! Chữ cái đầu từ tên hiển thị (tối đa 2 ký tự).
QString initialsOf(const QString &name);

} // namespace Avatar
