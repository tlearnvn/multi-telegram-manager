#pragma once

#include <QCache>
#include <QPixmap>
#include <QSize>
#include <QString>

/*!
 * \brief Bộ đệm ảnh đã giải mã & thu nhỏ.
 *
 * Delegate vẽ danh sách chat và bong bóng tin nhắn được gọi rất nhiều lần nên
 * không thể đọc tệp ảnh mỗi lần vẽ. Lớp này giữ sẵn QPixmap theo (đường dẫn,
 * kích cỡ) với giới hạn dung lượng để không phình bộ nhớ.
 */
class FileCache
{
public:
    static FileCache &instance();

    //! Ảnh đã thu về đúng \a size (giữ tỉ lệ, cắt giữa). Rỗng nếu không đọc được.
    QPixmap thumbnail(const QString &path, const QSize &size);

    //! Ảnh vừa khít trong \a bounds, giữ nguyên tỉ lệ (không cắt).
    QPixmap scaled(const QString &path, const QSize &bounds);

    //! Ảnh gốc (dùng khi mở xem ảnh cỡ lớn).
    QPixmap original(const QString &path);

    //! Kích cỡ gốc của ảnh mà không cần giải mã toàn bộ.
    QSize imageSize(const QString &path);

    void invalidate(const QString &path);
    void clear();

private:
    FileCache();

    QPixmap load(const QString &key, const QString &path, const QSize &size, bool crop);

    QCache<QString, QPixmap> m_cache;
    QCache<QString, QSize> m_sizes;
};
