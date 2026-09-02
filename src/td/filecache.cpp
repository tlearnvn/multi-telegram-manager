#include "td/filecache.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>

namespace {

QString cacheKey(const QString &path, const QSize &size, bool crop)
{
    return QStringLiteral("%1|%2x%3|%4")
        .arg(path)
        .arg(size.width())
        .arg(size.height())
        .arg(crop ? QLatin1Char('c') : QLatin1Char('f'));
}

} // namespace

FileCache::FileCache()
    : m_cache(240)   // ~240 ảnh nhỏ
    , m_sizes(2000)
{
}

FileCache &FileCache::instance()
{
    static FileCache cache;
    return cache;
}

QPixmap FileCache::thumbnail(const QString &path, const QSize &size)
{
    if (path.isEmpty() || !size.isValid())
        return QPixmap();
    return load(cacheKey(path, size, true), path, size, true);
}

QPixmap FileCache::scaled(const QString &path, const QSize &bounds)
{
    if (path.isEmpty() || !bounds.isValid())
        return QPixmap();
    return load(cacheKey(path, bounds, false), path, bounds, false);
}

QPixmap FileCache::original(const QString &path)
{
    if (path.isEmpty())
        return QPixmap();
    QPixmap pixmap;
    if (!pixmap.load(path))
        return QPixmap();
    return pixmap;
}

QSize FileCache::imageSize(const QString &path)
{
    if (path.isEmpty())
        return QSize();
    if (const QSize *cached = m_sizes.object(path))
        return *cached;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    m_sizes.insert(path, new QSize(size));
    return size;
}

QPixmap FileCache::load(const QString &key, const QString &path, const QSize &size, bool crop)
{
    if (const QPixmap *cached = m_cache.object(key))
        return *cached;

    if (!QFileInfo::exists(path))
        return QPixmap();

    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead())
        return QPixmap();

    // Nhờ QImageReader thu nhỏ ngay khi giải mã: nhanh và ít RAM hơn nhiều so
    // với đọc ảnh gốc rồi scale.
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid()) {
        QSize target = sourceSize;
        if (crop)
            target = sourceSize.scaled(size, Qt::KeepAspectRatioByExpanding);
        else
            target = sourceSize.scaled(size, Qt::KeepAspectRatio);
        if (target.isValid() && (target.width() < sourceSize.width()
                                 || target.height() < sourceSize.height())) {
            reader.setScaledSize(target);
        }
    }

    QImage image = reader.read();
    if (image.isNull())
        return QPixmap();

    if (crop) {
        // Cắt phần giữa cho vừa khung vuông/chữ nhật.
        const int x = qMax(0, (image.width() - size.width()) / 2);
        const int y = qMax(0, (image.height() - size.height()) / 2);
        image = image.copy(x, y, qMin(size.width(), image.width()),
                           qMin(size.height(), image.height()));
    }

    auto *pixmap = new QPixmap(QPixmap::fromImage(image));
    const int cost = qMax(1, (pixmap->width() * pixmap->height()) / 4096);
    QPixmap result = *pixmap;
    m_cache.insert(key, pixmap, cost);
    return result;
}

void FileCache::invalidate(const QString &path)
{
    const QList<QString> keys = m_cache.keys();
    for (const QString &key : keys) {
        if (key.startsWith(path + QLatin1Char('|')))
            m_cache.remove(key);
    }
    m_sizes.remove(path);
}

void FileCache::clear()
{
    m_cache.clear();
    m_sizes.clear();
}
