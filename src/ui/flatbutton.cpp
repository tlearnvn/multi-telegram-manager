#include "ui/flatbutton.h"

#include "ui/theme.h"

#include <QFont>
#include <QPainter>

IconButton::IconButton(Icons::Name name, const QString &tooltip, int iconSize, QWidget *parent)
    : QPushButton(parent)
    , m_name(name)
    , m_iconSize(iconSize)
{
    setToolTip(tooltip);
    setAccessibleName(tooltip);
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
    setFixedSize(iconSize + 16, iconSize + 16);
    setFocusPolicy(Qt::TabFocus);

    connect(&Theme::instance(), &Theme::changed, this, &IconButton::refreshIcon);
    refreshIcon();
}

void IconButton::setIconName(Icons::Name name)
{
    m_name = name;
    refreshIcon();
}

void IconButton::setAccented(bool accented)
{
    if (m_accented == accented)
        return;
    m_accented = accented;
    refreshIcon();
}

void IconButton::setDanger(bool danger)
{
    if (m_danger == danger)
        return;
    m_danger = danger;
    refreshIcon();
}

void IconButton::setBadgeCount(int count)
{
    if (m_badge == count)
        return;
    m_badge = count;
    update();
}

void IconButton::refreshIcon()
{
    const Theme::Colors &c = Theme::instance().colors();
    QColor color = c.textSecondary;
    if (m_danger)
        color = c.danger;
    else if (m_accented)
        color = c.accent;
    setIcon(Icons::icon(m_name, color, m_iconSize));
    setIconSize(QSize(m_iconSize, m_iconSize));
    update();
}

void IconButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void IconButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const Theme::Colors &c = Theme::instance().colors();

    if (isDown() || isChecked()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.accentSoft);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);
    } else if (m_hovered && isEnabled()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.hoverBg);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);
    }

    QColor color = c.textSecondary;
    if (!isEnabled())
        color = c.textMuted;
    else if (m_danger)
        color = c.danger;
    else if (m_accented || isChecked())
        color = c.accent;
    else if (m_hovered)
        color = c.textPrimary;

    const QPixmap glyph = Icons::pixmap(m_name, color, m_iconSize, devicePixelRatioF());
    const QPoint topLeft(rect().center().x() - m_iconSize / 2,
                         rect().center().y() - m_iconSize / 2);
    painter.drawPixmap(topLeft, glyph);

    if (m_badge > 0) {
        QFont font = painter.font();
        font.setPixelSize(qMax(8, m_iconSize / 2));
        font.setBold(true);
        painter.setFont(font);

        const QString text = m_badge > 99 ? QStringLiteral("99+") : QString::number(m_badge);
        const int textWidth = painter.fontMetrics().horizontalAdvance(text);
        const int badgeHeight = painter.fontMetrics().height() + 2;
        const int badgeWidth = qMax(badgeHeight, textWidth + 8);
        const QRect badgeRect(rect().right() - badgeWidth, 1, badgeWidth, badgeHeight);

        painter.setPen(Qt::NoPen);
        painter.setBrush(c.badge);
        painter.drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);
        painter.setPen(c.textOnAccent);
        painter.drawText(badgeRect, Qt::AlignCenter, text);
    }
}

// ---------------------------------------------------------------------------

SectionLabel::SectionLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
{
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.02);
    setFont(f);
    setContentsMargins(0, 8, 0, 2);
}

// ---------------------------------------------------------------------------

Separator::Separator(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    if (orientation == Qt::Horizontal) {
        setFixedHeight(1);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        setFixedWidth(1);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    connect(&Theme::instance(), &Theme::changed, this, [this] { update(); });
}

void Separator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), Theme::instance().colors().divider);
}
