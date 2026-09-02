#pragma once

#include <QDateTime>
#include <QString>

/*!
 * \brief Định dạng chuỗi hiển thị theo thói quen tiếng Việt.
 */
namespace Format {

//! "14:32" nếu hôm nay, "Hôm qua", "Thứ Ba", "12/08" hoặc "12/08/2024".
QString chatListTime(qint64 unixSeconds);

//! "14:32"
QString clock(qint64 unixSeconds);

//! "Thứ Ba, 12 tháng 8, 2025" — dùng cho dải phân cách ngày trong khung chat.
QString dayDivider(qint64 unixSeconds);

//! "12/08/2025 14:32"
QString fullDateTime(qint64 unixSeconds);

//! "vừa xong", "3 phút trước", "2 giờ trước", "hôm qua", "12/08/2025".
QString relative(qint64 unixSeconds);

//! "1,2 MB" — dùng dấu phẩy thập phân kiểu Việt Nam.
QString fileSize(qint64 bytes);

//! "01:23" hoặc "1:02:03" cho thời lượng audio/video.
QString duration(int seconds);

//! Rút gọn văn bản một dòng cho danh sách chat (bỏ xuống dòng, giới hạn ký tự).
QString oneLine(const QString &text, int maxChars = 120);

//! Chuyển chuỗi bất kỳ thành tên thư mục an toàn (dùng cho slug tài khoản).
QString slugify(const QString &text);

//! Mã hoá số điện thoại khi hiển thị: +84 90 *** ** 67
QString maskPhone(const QString &phone);

//! Chuẩn hoá số điện thoại: bỏ khoảng trắng, gạch, ngoặc; giữ dấu +.
QString normalizePhone(const QString &phone);

//! "3 thành viên", "1 thành viên" — số lượng kèm danh từ tiếng Việt.
QString countLabel(int count, const QString &noun);

} // namespace Format
