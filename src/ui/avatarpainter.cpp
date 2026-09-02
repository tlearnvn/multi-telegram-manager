#include "ui/avatarpainter.h"

#include "td/filecache.h"
#include "ui/theme.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace Avatar {

QString initialsOf(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("?");

    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return trimmed.left(1).toUpper();
    if (parts.size() == 1)
        return parts.first().left(1).toUpper();
    return (parts.first().left(1) + parts.last().left(1)).toUpper();
}

void paint(QPainter *painter, const QRect &rect, const Options &options)
{
    if (!painter || rect.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRect box = rect;
    if (options.ringWidth > 0 && options.ringColor.isValid()) {
        QPen ringPen(options.ringColor);
        ringPen.setWidth(options.ringWidth);
        painter->setPen(ringPen);
        painter->setBrush(Qt::NoBrush);
        const qreal inset = options.ringWidth / 2.0;
        painter->drawEllipse(QRectF(box).adjusted(inset, inset, -inset, -inset));
        box = box.adjusted(options.ringWidth + 1, options.ringWidth + 1,
                           -(options.ringWidth + 1), -(options.ringWidth + 1));
    }

    QPainterPath clip;
    clip.addEllipse(QRectF(box));

    const QPixmap photo = options.photoPath.isEmpty()
        ? QPixmap()
        : FileCache::instance().thumbnail(options.photoPath, box.size() * 2);

    if (!photo.isNull()) {
        painter->setClipPath(clip);
        painter->drawPixmap(box, photo);
        painter->setClipping(false);
    } else {
        const QColor base = Theme::instance().avatarColor(options.colorIndex);
        QLinearGradient gradient(box.topLeft(), box.bottomRight());
        gradient.setColorAt(0.0, base.lighter(118));
        gradient.setColorAt(1.0, base.darker(112));
        painter->setPen(Qt::NoPen);
        painter->setBrush(gradient);
        painter->drawEllipse(box);

        QString text = options.initials;
        if (text.isEmpty())
            text = QStringLiteral("?");

        QFont font = painter->font();
        font.setBold(true);
        font.setPixelSize(qMax(9, static_cast<int>(box.height() * 0.40)));
        painter->setFont(font);
        painter->setPen(QColor(255, 255, 255, 235));
        painter->drawText(box, Qt::AlignCenter, text);
    }

    if (options.showOnlineDot) {
        const int dot = qMax(8, box.height() / 4);
        const QRect dotRect(box.right() - dot + 1, box.bottom() - dot + 1, dot, dot);
        painter->setPen(QPen(Theme::instance().colors().sidebarBg, qMax(2, dot / 5)));
        painter->setBrush(options.online ? Theme::instance().colors().success
                                         : Theme::instance().colors().badgeMuted);
        painter->drawEllipse(dotRect);
    }

    painter->restore();
}

QPixmap make(int diameter, const Options &options, qreal devicePixelRatio)
{
    QPixmap canvas(QSize(diameter, diameter) * devicePixelRatio);
    canvas.setDevicePixelRatio(devicePixelRatio);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    paint(&painter, QRect(0, 0, diameter, diameter), options);
    painter.end();
    return canvas;
}

} // namespace Avatar
