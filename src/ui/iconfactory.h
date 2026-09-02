#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>

/*!
 * \brief Bộ biểu tượng vẽ bằng QPainter.
 *
 * Ứng dụng không dùng tệp ảnh ngoài: mọi biểu tượng đều là hình vector được vẽ
 * trực tiếp. Nhờ vậy bản build chỉ có một tệp chạy duy nhất, luôn nét ở mọi
 * mức DPI và tự đổi màu theo chủ đề sáng/tối.
 */
namespace Icons {

enum class Name {
    Send, Attach, Emoji, Search, Settings, Plus, Menu, Back, Forward,
    Microphone, Pin, Unpin, Bell, BellOff, Check, DoubleCheck, Clock,
    Download, Close, User, Users, Megaphone, Moon, Sun, Trash, Edit,
    Reply, Copy, QrCode, Phone, Key, Logout, Dashboard, Broadcast,
    Archive, Image, File, Play, Info, Refresh, Warning, Link, Folder,
    ChevronDown, ChevronUp, ChevronRight, Robot, Star, Save, Eye, Sticker
};

//! Biểu tượng đơn sắc theo màu cho trước.
QIcon icon(Name name, const QColor &color, int size = 20);

//! Biểu tượng đổi màu theo trạng thái (thường / đang chọn).
QIcon icon(Name name, const QColor &normal, const QColor &active, int size = 20);

QPixmap pixmap(Name name, const QColor &color, int size = 20, qreal devicePixelRatio = 1.0);

//! Logo ứng dụng (máy bay giấy + chữ T) dùng cho cửa sổ và khay hệ thống.
QIcon appIcon();
QPixmap appLogo(int size, const QColor &accent);

//! Xoá bộ đệm khi đổi chủ đề.
void clearCache();

} // namespace Icons
