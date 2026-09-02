#include "ui/iconfactory.h"

#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <cmath>

namespace {

//! Hằng số pi tự khai báo — M_PI không có sẵn trên mọi trình biên dịch.
constexpr double kPi = 3.14159265358979323846;

QHash<QString, QPixmap> g_cache;

QString keyFor(Icons::Name name, const QColor &color, int size, qreal dpr)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(static_cast<int>(name))
        .arg(color.name(QColor::HexArgb))
        .arg(size)
        .arg(dpr, 0, 'f', 2);
}

/*!
 * Mọi hình được vẽ trong hệ toạ độ 24x24 rồi scale về kích cỡ mong muốn, giống
 * cách các bộ icon vector (Feather/Lucide) hoạt động.
 */
void paintIcon(QPainter &p, Icons::Name name)
{
    using N = Icons::Name;

    auto line = [&p](qreal x1, qreal y1, qreal x2, qreal y2) {
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    auto circle = [&p](qreal cx, qreal cy, qreal r) {
        p.drawEllipse(QPointF(cx, cy), r, r);
    };
    auto roundRect = [&p](qreal x, qreal y, qreal w, qreal h, qreal r) {
        p.drawRoundedRect(QRectF(x, y, w, h), r, r);
    };
    auto polyline = [&p](const QList<QPointF> &points) {
        QPainterPath path(points.first());
        for (int i = 1; i < points.size(); ++i)
            path.lineTo(points.at(i));
        p.drawPath(path);
    };
    auto filledPolygon = [&p](const QList<QPointF> &points) {
        QPolygonF polygon;
        for (const QPointF &point : points)
            polygon << point;
        QPen saved = p.pen();
        p.setBrush(saved.color());
        p.setPen(Qt::NoPen);
        p.drawPolygon(polygon);
        p.setPen(saved);
        p.setBrush(Qt::NoBrush);
    };

    switch (name) {
    case N::Send:
        // Máy bay giấy đặc.
        filledPolygon({ { 3, 12 }, { 21, 4 }, { 13, 21 }, { 11, 14 } });
        break;
    case N::Attach:
        polyline({ { 8, 7 }, { 8, 16 } });
        p.drawArc(QRectF(6.5, 3.5, 7, 7), 0 * 16, 180 * 16);
        p.drawArc(QRectF(5, 12, 10, 9), 180 * 16, 180 * 16);
        polyline({ { 15, 8 }, { 15, 16 } });
        break;
    case N::Emoji:
        circle(12, 12, 8.5);
        circle(9, 10, 0.9);
        circle(15, 10, 0.9);
        p.drawArc(QRectF(7.5, 10.5, 9, 7), 200 * 16, 140 * 16);
        break;
    case N::Search:
        circle(10.5, 10.5, 6.5);
        line(15.3, 15.3, 20.5, 20.5);
        break;
    case N::Settings:
        circle(12, 12, 3.2);
        for (int i = 0; i < 6; ++i) {
            const qreal angle = i * kPi / 3.0;
            const qreal inner = 5.6;
            const qreal outer = 9.2;
            line(12 + inner * std::cos(angle), 12 + inner * std::sin(angle),
                 12 + outer * std::cos(angle), 12 + outer * std::sin(angle));
        }
        break;
    case N::Plus:
        line(12, 5, 12, 19);
        line(5, 12, 19, 12);
        break;
    case N::Menu:
        line(4, 7, 20, 7);
        line(4, 12, 20, 12);
        line(4, 17, 20, 17);
        break;
    case N::Back:
        polyline({ { 14, 5 }, { 7, 12 }, { 14, 19 } });
        break;
    case N::Forward:
        polyline({ { 6, 8 }, { 12, 14 }, { 6, 20 } });
        polyline({ { 12, 14 }, { 20, 14 }, { 20, 5 } });
        break;
    case N::ChevronRight:
        polyline({ { 10, 6 }, { 16, 12 }, { 10, 18 } });
        break;
    case N::ChevronDown:
        polyline({ { 6, 10 }, { 12, 16 }, { 18, 10 } });
        break;
    case N::Microphone:
        roundRect(9, 3, 6, 11, 3);
        p.drawArc(QRectF(5.5, 8, 13, 11), 180 * 16, 180 * 16);
        line(12, 19, 12, 21.5);
        break;
    case N::Pin:
        polyline({ { 12, 21 }, { 12, 14 } });
        p.drawArc(QRectF(7, 3, 10, 11), 0, 360 * 16);
        break;
    case N::Unpin:
        polyline({ { 12, 21 }, { 12, 14 } });
        p.drawArc(QRectF(7, 3, 10, 11), 0, 360 * 16);
        line(4, 4, 20, 20);
        break;
    case N::Bell:
        polyline({ { 6, 16 }, { 6, 10.5 } });
        p.drawArc(QRectF(6, 3.5, 12, 13), 0 * 16, 180 * 16);
        polyline({ { 18, 10.5 }, { 18, 16 } });
        line(4.5, 16.5, 19.5, 16.5);
        p.drawArc(QRectF(10, 17, 4, 4), 180 * 16, 180 * 16);
        break;
    case N::BellOff:
        polyline({ { 6, 16 }, { 6, 10.5 } });
        p.drawArc(QRectF(6, 3.5, 12, 13), 0 * 16, 180 * 16);
        polyline({ { 18, 10.5 }, { 18, 16 } });
        line(4.5, 16.5, 19.5, 16.5);
        line(3.5, 3.5, 20.5, 20.5);
        break;
    case N::Check:
        polyline({ { 5, 13 }, { 10, 18 }, { 19.5, 6.5 } });
        break;
    case N::DoubleCheck:
        polyline({ { 2.5, 13 }, { 7, 17.5 }, { 15.5, 7 } });
        polyline({ { 10.5, 15.5 }, { 12.5, 17.5 }, { 21.5, 7 } });
        break;
    case N::Clock:
        circle(12, 12, 8.5);
        polyline({ { 12, 7 }, { 12, 12.5 }, { 16, 14.5 } });
        break;
    case N::Download:
        polyline({ { 12, 3.5 }, { 12, 15 } });
        polyline({ { 7, 10.5 }, { 12, 15.5 }, { 17, 10.5 } });
        polyline({ { 4.5, 19.5 }, { 19.5, 19.5 } });
        break;
    case N::Close:
        line(6, 6, 18, 18);
        line(18, 6, 6, 18);
        break;
    case N::User:
        circle(12, 8.5, 4);
        p.drawArc(QRectF(4.5, 13, 15, 14), 0 * 16, 180 * 16);
        break;
    case N::Users:
        circle(9.5, 8.5, 3.6);
        p.drawArc(QRectF(3, 12.5, 13, 13), 0 * 16, 180 * 16);
        p.drawArc(QRectF(13.5, 5.5, 7, 7), 270 * 16, 180 * 16);
        p.drawArc(QRectF(14, 12.5, 9, 12), 0 * 16, 90 * 16);
        break;
    case N::Megaphone:
        filledPolygon({ { 4, 10 }, { 12, 6 }, { 12, 18 }, { 4, 14 } });
        p.drawArc(QRectF(13, 7, 8, 10), 270 * 16, 180 * 16);
        break;
    case N::Broadcast:
        circle(12, 12, 2.4);
        p.drawArc(QRectF(6.5, 6.5, 11, 11), 0, 360 * 16);
        p.drawArc(QRectF(2.5, 2.5, 19, 19), 0, 360 * 16);
        break;
    case N::Moon:
        p.drawArc(QRectF(3.5, 3.5, 17, 17), 55 * 16, 250 * 16);
        p.drawArc(QRectF(7, 2, 17, 17), 130 * 16, 105 * 16);
        break;
    case N::Sun:
        circle(12, 12, 4.6);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * kPi / 4.0;
            line(12 + 7 * std::cos(angle), 12 + 7 * std::sin(angle),
                 12 + 9.5 * std::cos(angle), 12 + 9.5 * std::sin(angle));
        }
        break;
    case N::Trash:
        polyline({ { 4.5, 7 }, { 19.5, 7 } });
        polyline({ { 9.5, 7 }, { 9.5, 4.5 }, { 14.5, 4.5 }, { 14.5, 7 } });
        polyline({ { 6.5, 7 }, { 7.5, 20 }, { 16.5, 20 }, { 17.5, 7 } });
        line(10.5, 10.5, 10.5, 17);
        line(13.5, 10.5, 13.5, 17);
        break;
    case N::Edit:
        polyline({ { 4, 20 }, { 4, 16 }, { 16.5, 3.5 }, { 20.5, 7.5 }, { 8, 20 }, { 4, 20 } });
        line(14, 6, 18, 10);
        break;
    case N::Reply:
        polyline({ { 10, 7 }, { 4, 13 }, { 10, 19 } });
        polyline({ { 4, 13 }, { 14, 13 } });
        p.drawArc(QRectF(9, 5, 11, 16), 270 * 16, 90 * 16);
        break;
    case N::Copy:
        roundRect(8.5, 3.5, 12, 12, 2.5);
        polyline({ { 15.5, 18.5 }, { 5.5, 18.5 }, { 5.5, 8.5 } });
        break;
    case N::QrCode:
        roundRect(3.5, 3.5, 7, 7, 1.5);
        roundRect(13.5, 3.5, 7, 7, 1.5);
        roundRect(3.5, 13.5, 7, 7, 1.5);
        line(14, 14, 17, 14);
        line(14, 17.5, 14, 20.5);
        line(17.5, 17.5, 20.5, 17.5);
        line(20.5, 13.5, 20.5, 15);
        break;
    case N::Phone:
        polyline({ { 6, 3.5 }, { 9.5, 3.5 }, { 11, 8 }, { 8.5, 10 },
                   { 14, 15.5 }, { 16, 13 }, { 20.5, 14.5 }, { 20.5, 18 } });
        p.drawArc(QRectF(3, 15, 20, 8), 180 * 16, 90 * 16);
        break;
    case N::Key:
        circle(8, 8, 4.2);
        polyline({ { 11, 11 }, { 20, 20 } });
        line(17, 20, 20, 17);
        break;
    case N::Logout:
        polyline({ { 10, 4.5 }, { 4.5, 4.5 }, { 4.5, 19.5 }, { 10, 19.5 } });
        polyline({ { 14, 8 }, { 19.5, 12 }, { 14, 16 } });
        line(9, 12, 19, 12);
        break;
    case N::Dashboard:
        roundRect(3.5, 3.5, 7.5, 7.5, 1.8);
        roundRect(13, 3.5, 7.5, 4.5, 1.8);
        roundRect(3.5, 13, 7.5, 7.5, 1.8);
        roundRect(13, 10, 7.5, 10.5, 1.8);
        break;
    case N::Archive:
        roundRect(3.5, 4, 17, 4.5, 1.4);
        polyline({ { 5, 8.5 }, { 5, 19.5 }, { 19, 19.5 }, { 19, 8.5 } });
        line(9.5, 12.5, 14.5, 12.5);
        break;
    case N::Image:
        roundRect(3.5, 4.5, 17, 15, 2.5);
        circle(8.8, 9.8, 1.7);
        polyline({ { 4.5, 17 }, { 10, 12 }, { 14, 15.5 }, { 16.5, 13 }, { 19.5, 16 } });
        break;
    case N::File:
        polyline({ { 6, 3.5 }, { 14, 3.5 }, { 19, 8.5 }, { 19, 20.5 }, { 6, 20.5 }, { 6, 3.5 } });
        polyline({ { 14, 3.5 }, { 14, 8.5 }, { 19, 8.5 } });
        break;
    case N::Folder:
        polyline({ { 3.5, 19 }, { 3.5, 5.5 }, { 9.5, 5.5 }, { 11.5, 8 },
                   { 20.5, 8 }, { 20.5, 19 }, { 3.5, 19 } });
        break;
    case N::Play:
        filledPolygon({ { 8, 5 }, { 19, 12 }, { 8, 19 } });
        break;
    case N::Info:
        circle(12, 12, 8.5);
        line(12, 11, 12, 16.5);
        circle(12, 7.8, 0.9);
        break;
    case N::Refresh:
        p.drawArc(QRectF(4, 4, 16, 16), 60 * 16, 260 * 16);
        filledPolygon({ { 17, 2.5 }, { 20.5, 7 }, { 15, 7.5 } });
        break;
    case N::Warning:
        polyline({ { 12, 3.5 }, { 21.5, 20 }, { 2.5, 20 }, { 12, 3.5 } });
        line(12, 9, 12, 14.5);
        circle(12, 17.2, 0.85);
        break;
    case N::Link:
        p.drawArc(QRectF(2.5, 8.5, 11, 7), 90 * 16, 180 * 16);
        p.drawArc(QRectF(10.5, 8.5, 11, 7), 270 * 16, 180 * 16);
        line(8, 12, 16, 12);
        break;
    case N::Robot:
        roundRect(4.5, 7.5, 15, 12, 3);
        circle(9.5, 13, 1.2);
        circle(14.5, 13, 1.2);
        line(12, 4, 12, 7.5);
        circle(12, 3.2, 1.1);
        break;
    case N::Star:
        filledPolygon({ { 12, 3 }, { 14.7, 9.3 }, { 21.5, 9.9 }, { 16.4, 14.4 },
                        { 17.9, 21 }, { 12, 17.5 }, { 6.1, 21 }, { 7.6, 14.4 },
                        { 2.5, 9.9 }, { 9.3, 9.3 } });
        break;
    case N::Save:
        polyline({ { 4.5, 4.5 }, { 16, 4.5 }, { 19.5, 8 }, { 19.5, 19.5 }, { 4.5, 19.5 }, { 4.5, 4.5 } });
        roundRect(8, 4.5, 8, 5, 1);
        roundRect(7.5, 13, 9, 6.5, 1);
        break;
    case N::Eye:
        p.drawArc(QRectF(2, 5.5, 20, 13), 0 * 16, 180 * 16);
        p.drawArc(QRectF(2, 5.5, 20, 13), 180 * 16, 180 * 16);
        circle(12, 12, 3);
        break;
    }
}

} // namespace

namespace Icons {

QPixmap pixmap(Name name, const QColor &color, int size, qreal devicePixelRatio)
{
    const QString key = keyFor(name, color, size, devicePixelRatio);
    auto it = g_cache.constFind(key);
    if (it != g_cache.constEnd())
        return it.value();

    QPixmap canvas(QSize(size, size) * devicePixelRatio);
    canvas.setDevicePixelRatio(devicePixelRatio);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size / 24.0, size / 24.0);

    QPen pen(color);
    // Nét dày theo tỉ lệ để icon nhỏ vẫn rõ, icon lớn không bị thô.
    pen.setWidthF(size <= 18 ? 2.0 : 1.85);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    paintIcon(painter, name);
    painter.end();

    g_cache.insert(key, canvas);
    return canvas;
}

QIcon icon(Name name, const QColor &color, int size)
{
    QIcon result;
    result.addPixmap(pixmap(name, color, size, 1.0));
    result.addPixmap(pixmap(name, color, size, 2.0));
    QColor disabled = color;
    disabled.setAlphaF(0.45f);
    result.addPixmap(pixmap(name, disabled, size, 1.0), QIcon::Disabled);
    return result;
}

QIcon icon(Name name, const QColor &normal, const QColor &active, int size)
{
    QIcon result;
    result.addPixmap(pixmap(name, normal, size, 1.0), QIcon::Normal, QIcon::Off);
    result.addPixmap(pixmap(name, normal, size, 2.0), QIcon::Normal, QIcon::Off);
    result.addPixmap(pixmap(name, active, size, 1.0), QIcon::Normal, QIcon::On);
    result.addPixmap(pixmap(name, active, size, 2.0), QIcon::Normal, QIcon::On);
    result.addPixmap(pixmap(name, active, size, 1.0), QIcon::Active, QIcon::Off);
    return result;
}

QPixmap appLogo(int size, const QColor &accent)
{
    QPixmap canvas(size, size);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = size / 64.0;
    painter.scale(s, s);

    // Nền tròn chuyển màu.
    QLinearGradient gradient(0, 0, 64, 64);
    gradient.setColorAt(0.0, accent.lighter(125));
    gradient.setColorAt(1.0, accent.darker(125));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(1, 1, 62, 62), 16, 16);

    // Máy bay giấy.
    painter.setBrush(QColor(255, 255, 255, 235));
    QPolygonF plane;
    plane << QPointF(14, 33) << QPointF(50, 16) << QPointF(35, 50)
          << QPointF(29, 40) << QPointF(41, 24) << QPointF(26, 35);
    painter.drawPolygon(plane);

    painter.end();
    return canvas;
}

QIcon appIcon()
{
    static QIcon cached;
    if (!cached.isNull())
        return cached;

    const QColor accent(QStringLiteral("#2ea6ff"));
    QIcon result;
    for (int size : { 16, 24, 32, 48, 64, 128, 256 })
        result.addPixmap(appLogo(size, accent));
    cached = result;
    return cached;
}

void clearCache()
{
    g_cache.clear();
}

} // namespace Icons
