#include "ui/toast.h"

#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QTimer>

namespace {

Icons::Name glyphFor(Toast::Kind kind)
{
    switch (kind) {
    case Toast::Kind::Success: return Icons::Name::Check;
    case Toast::Kind::Warning: return Icons::Name::Warning;
    case Toast::Kind::Error:   return Icons::Name::Warning;
    case Toast::Kind::Info:    break;
    }
    return Icons::Name::Info;
}

QColor colorFor(Toast::Kind kind)
{
    const Theme::Colors &c = Theme::instance().colors();
    switch (kind) {
    case Toast::Kind::Success: return c.success;
    case Toast::Kind::Warning: return c.warning;
    case Toast::Kind::Error:   return c.danger;
    case Toast::Kind::Info:    break;
    }
    return c.accent;
}

} // namespace

void Toast::popup(QWidget *host, const QString &message, Kind kind, int milliseconds)
{
    if (!host || message.trimmed().isEmpty())
        return;

    // Chỉ giữ một toast mỗi lúc để không xếp lớp lên nhau.
    const QList<Toast *> existing = host->findChildren<Toast *>(QString(),
                                                                Qt::FindDirectChildrenOnly);
    for (Toast *old : existing)
        old->deleteLater();

    auto *toast = new Toast(host, message, kind, milliseconds);
    toast->reposition();
    toast->show();
    toast->raise();

    // Hiệu ứng mờ dần phải dùng QGraphicsOpacityEffect vì windowOpacity chỉ có
    // tác dụng với cửa sổ cấp cao nhất.
    auto *fade = new QPropertyAnimation(toast->m_opacity, "opacity", toast);
    fade->setDuration(160);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

Toast::Toast(QWidget *host, const QString &message, Kind kind, int milliseconds)
    : QWidget(host)
    , m_kind(kind)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(1.0);
    setGraphicsEffect(m_opacity);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 16, 10);
    layout->setSpacing(9);

    auto *icon = new QLabel(this);
    icon->setPixmap(Icons::pixmap(glyphFor(kind), colorFor(kind), 18,
                                  devicePixelRatioF()));
    layout->addWidget(icon);

    m_label = new QLabel(message, this);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                               .arg(Theme::instance().colors().textPrimary.name()));
    layout->addWidget(m_label, 1);

    setMaximumWidth(460);
    adjustSize();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(qMax(800, milliseconds));
    connect(m_timer, &QTimer::timeout, this, [this] {
        auto *fade = new QPropertyAnimation(m_opacity, "opacity", this);
        fade->setDuration(220);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        connect(fade, &QPropertyAnimation::finished, this, &QObject::deleteLater);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
    m_timer->start();
}

void Toast::reposition()
{
    QWidget *host = parentWidget();
    if (!host)
        return;
    adjustSize();
    const int x = (host->width() - width()) / 2;
    const int y = host->height() - height() - 28;
    move(qMax(8, x), qMax(8, y));
}

void Toast::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const Theme::Colors &c = Theme::instance().colors();

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    painter.setPen(Qt::NoPen);
    painter.setBrush(c.panelBg);
    painter.drawPath(path);

    painter.setPen(QPen(c.divider, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // Vạch màu bên trái theo loại thông báo.
    painter.setPen(Qt::NoPen);
    painter.setBrush(colorFor(m_kind));
    painter.drawRoundedRect(QRect(0, 8, 4, height() - 16), 2, 2);
}
