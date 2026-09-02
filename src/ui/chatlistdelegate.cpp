#include "ui/chatlistdelegate.h"

#include "core/formatting.h"
#include "model/chatlistmodel.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QFontMetrics>
#include <QPainter>

namespace {

constexpr int kPadX = 12;
constexpr int kPadY = 9;
constexpr int kAvatarNormal = 50;
constexpr int kAvatarCompact = 38;
constexpr int kGap = 11;

} // namespace

ChatListDelegate::ChatListDelegate(ChatListModel *model, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
{
}

void ChatListDelegate::setCompact(bool compact)
{
    m_compact = compact;
}

QSize ChatListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    Q_UNUSED(index)
    const int avatar = m_compact ? kAvatarCompact : kAvatarNormal;
    const QFontMetrics metrics(option.font);
    const int textHeight = m_compact ? metrics.height() + 2
                                     : metrics.height() * 2 + 6;
    return QSize(option.rect.width(), qMax(avatar, textHeight) + kPadY * 2);
}

void ChatListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const ChatEntry *entry = m_model ? m_model->entryAt(index) : nullptr;
    if (!entry) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const Theme::Colors &c = Theme::instance().colors();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // --- Nền -------------------------------------------------------------
    const QRect row = option.rect.adjusted(6, 2, -6, -2);
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.accent);
        painter->drawRoundedRect(row, 11, 11);
    } else if (hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.hoverBg);
        painter->drawRoundedRect(row, 11, 11);
    }

    const QColor titleColor = selected ? c.textOnAccent : c.textPrimary;
    const QColor bodyColor = selected ? QColor(255, 255, 255, 215) : c.textSecondary;
    const QColor metaColor = selected ? QColor(255, 255, 255, 190) : c.textMuted;

    // --- Avatar ----------------------------------------------------------
    const int avatarSize = m_compact ? kAvatarCompact : kAvatarNormal;
    const QRect avatarRect(row.left() + kPadX,
                           row.top() + (row.height() - avatarSize) / 2,
                           avatarSize, avatarSize);

    Avatar::Options avatarOptions;
    avatarOptions.photoPath = entry->photoPath;
    avatarOptions.initials = Avatar::initialsOf(entry->title);
    avatarOptions.colorIndex = static_cast<int>(qAbs(entry->id) % 7);

    if (entry->kind == ChatEntry::Kind::Private && m_model && m_model->account()) {
        if (const UserEntry *peer = m_model->account()->user(entry->relatedUserId)) {
            avatarOptions.showOnlineDot = !peer->isBot;
            avatarOptions.online = peer->presence == UserEntry::Presence::Online;
        }
    }
    Avatar::paint(painter, avatarRect, avatarOptions);

    // --- Vùng chữ --------------------------------------------------------
    int textLeft = avatarRect.right() + kGap;
    int textRight = row.right() - kPadX;

    const QFontMetrics metrics(option.font);

    // Huy hiệu số chưa đọc (vẽ trước để biết chỗ còn lại cho chữ).
    QRect badgeRect;
    if (entry->hasUnread()) {
        QFont badgeFont = option.font;
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.86);
        const QFontMetrics badgeMetrics(badgeFont);

        const QString badgeText = entry->unreadCount > 999
            ? QStringLiteral("999+")
            : (entry->unreadCount > 0 ? QString::number(entry->unreadCount) : QString());
        const int badgeHeight = badgeMetrics.height() + 4;
        const int badgeWidth = badgeText.isEmpty()
            ? badgeHeight
            : qMax(badgeHeight, badgeMetrics.horizontalAdvance(badgeText) + 12);

        badgeRect = QRect(textRight - badgeWidth,
                          row.bottom() - kPadY - badgeHeight + 2,
                          badgeWidth, badgeHeight);

        QColor badgeColor = entry->isMuted ? c.badgeMuted : c.badge;
        if (selected)
            badgeColor = c.textOnAccent;

        painter->setPen(Qt::NoPen);
        painter->setBrush(badgeColor);
        painter->drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);

        if (!badgeText.isEmpty()) {
            painter->setFont(badgeFont);
            painter->setPen(selected ? c.accent : c.textOnAccent);
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
        }
        textRight = badgeRect.left() - 8;
    } else if (entry->isPinned) {
        const int glyph = 14;
        const QRect pinRect(textRight - glyph, row.bottom() - kPadY - glyph, glyph, glyph);
        painter->drawPixmap(pinRect.topLeft(),
                            Icons::pixmap(Icons::Name::Pin, metaColor, glyph,
                                          painter->device()->devicePixelRatioF()));
        textRight = pinRect.left() - 8;
    }

    // Dòng 1: tiêu đề + giờ.
    QFont titleFont = option.font;
    titleFont.setBold(entry->hasUnread());
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.02);
    const QFontMetrics titleMetrics(titleFont);

    const QString timeText = Format::chatListTime(entry->lastMessageDate);
    QFont metaFont = option.font;
    metaFont.setPointSizeF(metaFont.pointSizeF() * 0.85);
    const QFontMetrics metaMetrics(metaFont);
    const int timeWidth = timeText.isEmpty() ? 0 : metaMetrics.horizontalAdvance(timeText) + 8;

    const int titleTop = m_compact
        ? row.top() + (row.height() - titleMetrics.height()) / 2
        : row.top() + kPadY;
    QRect titleRect(textLeft, titleTop,
                    qMax(20, textRight - textLeft - timeWidth), titleMetrics.height());

    // Biểu tượng phụ trước tiêu đề (kênh / nhóm / bot / tắt tiếng).
    Icons::Name kindGlyph = Icons::Name::User;
    bool drawKindGlyph = false;
    switch (entry->kind) {
    case ChatEntry::Kind::Channel:
        kindGlyph = Icons::Name::Megaphone;
        drawKindGlyph = true;
        break;
    case ChatEntry::Kind::BasicGroup:
    case ChatEntry::Kind::Supergroup:
        kindGlyph = Icons::Name::Users;
        drawKindGlyph = true;
        break;
    default:
        break;
    }
    if (drawKindGlyph) {
        const int glyph = titleMetrics.height() - 3;
        painter->drawPixmap(QPoint(titleRect.left(), titleRect.top() + 2),
                            Icons::pixmap(kindGlyph, metaColor, glyph,
                                          painter->device()->devicePixelRatioF()));
        titleRect.setLeft(titleRect.left() + glyph + 5);
    }

    painter->setFont(titleFont);
    painter->setPen(titleColor);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(entry->title, Qt::ElideRight, titleRect.width()));

    if (!timeText.isEmpty()) {
        painter->setFont(metaFont);
        painter->setPen(metaColor);
        const QRect timeRect(textRight - timeWidth + 8, titleRect.top(),
                             timeWidth - 8, titleRect.height());
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeText);
    }

    // Biểu tượng tắt tiếng ngay sau giờ.
    if (entry->isMuted && !timeText.isEmpty()) {
        const int glyph = 13;
        painter->drawPixmap(QPoint(textRight - timeWidth - glyph - 2, titleRect.top() + 3),
                            Icons::pixmap(Icons::Name::BellOff, metaColor, glyph,
                                          painter->device()->devicePixelRatioF()));
    }

    // Dòng 2: xem trước / đang gõ / nháp.
    if (!m_compact) {
        QFont previewFont = option.font;
        previewFont.setPointSizeF(previewFont.pointSizeF() * 0.94);
        const QFontMetrics previewMetrics(previewFont);
        const QRect previewRect(textLeft, titleRect.bottom() + 3,
                                qMax(20, textRight - textLeft), previewMetrics.height() + 2);

        QString previewText;
        QColor previewColor = bodyColor;

        if (!entry->actionText.isEmpty()) {
            previewText = entry->actionText;
            previewColor = selected ? c.textOnAccent : c.accent;
        } else if (!entry->draftText.isEmpty()) {
            previewText = QStringLiteral("Nháp: %1").arg(Format::oneLine(entry->draftText, 100));
            previewColor = selected ? c.textOnAccent : c.danger;
        } else {
            previewText = entry->lastMessagePreview;
            if (entry->isGroupLike() && !entry->lastMessageSender.isEmpty()
                && !previewText.isEmpty()) {
                previewText = QStringLiteral("%1: %2")
                                  .arg(entry->lastMessageSender, previewText);
            } else if (entry->lastMessageOutgoing && !previewText.isEmpty()) {
                previewText = QStringLiteral("Bạn: %1").arg(previewText);
            }
            if (previewText.isEmpty())
                previewText = QStringLiteral("Chưa có tin nhắn");
        }

        painter->setFont(previewFont);
        painter->setPen(previewColor);
        painter->drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter,
                          previewMetrics.elidedText(previewText, Qt::ElideRight,
                                                    previewRect.width()));

        // Dấu đã gửi / đã đọc cho tin nhắn của mình.
        if (entry->lastMessageOutgoing && entry->lastMessageId != 0
            && entry->actionText.isEmpty() && entry->draftText.isEmpty()) {
            const bool read = entry->lastReadOutboxMessageId >= entry->lastMessageId;
            Icons::Name tick = Icons::Name::Check;
            if (entry->lastMessageSendState == 1)
                tick = Icons::Name::Clock;
            else if (entry->lastMessageSendState == 2)
                tick = Icons::Name::Warning;
            else if (read)
                tick = Icons::Name::DoubleCheck;

            const QColor tickColor = entry->lastMessageSendState == 2
                ? c.danger
                : (selected ? c.textOnAccent : (read ? c.accent : metaColor));
            const int glyph = 14;
            painter->drawPixmap(QPoint(textLeft - glyph - 2, previewRect.top() + 2),
                                Icons::pixmap(tick, tickColor, glyph,
                                              painter->device()->devicePixelRatioF()));
        }
    }

    // Vạch nhấn cho chat có nhắc đến mình.
    if (entry->unreadMentionCount > 0 && !selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.danger);
        painter->drawRoundedRect(QRect(row.left() + 2, row.center().y() - 9, 3, 18), 2, 2);
    }

    painter->restore();
}
