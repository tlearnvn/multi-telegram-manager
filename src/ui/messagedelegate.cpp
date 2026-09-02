#include "ui/messagedelegate.h"

#include "core/formatting.h"
#include "model/messagemodel.h"
#include "td/filecache.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>
#include <QTextDocument>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kBubblePadX = 12;
constexpr int kBubblePadY = 8;
constexpr int kRowPadY = 3;
constexpr int kSidePad = 14;
constexpr int kAvatarSize = 34;
constexpr int kAvatarGap = 8;
constexpr int kDividerHeight = 34;
constexpr int kMaxBubbleWidth = 620;
constexpr int kMinBubbleWidth = 96;
constexpr int kFileCardHeight = 52;
constexpr int kRadius = 14;
constexpr int kMaxMediaHeight = 340;

QString escapeHtml(const QString &text)
{
    QString out;
    out.reserve(text.size() + 16);
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case '&':  out += QLatin1String("&amp;");  break;
        case '<':  out += QLatin1String("&lt;");   break;
        case '>':  out += QLatin1String("&gt;");   break;
        case '"':  out += QLatin1String("&quot;"); break;
        case '\n': out += QLatin1String("<br/>");  break;
        default:   out += ch;                      break;
        }
    }
    return out;
}

//! Ghép chuỗi + danh sách đoạn định dạng thành HTML nhỏ gọn cho QTextDocument.
QString spansToHtml(const QString &text, const QList<TextSpan> &spans, const QColor &linkColor)
{
    if (spans.isEmpty())
        return escapeHtml(text);

    QList<TextSpan> ordered = spans;
    std::sort(ordered.begin(), ordered.end(), [](const TextSpan &a, const TextSpan &b) {
        if (a.offset != b.offset)
            return a.offset < b.offset;
        return a.length > b.length;
    });

    QString html;
    int cursor = 0;
    for (const TextSpan &span : ordered) {
        if (span.offset < cursor || span.offset > text.size())
            continue; // bỏ đoạn chồng lấn để HTML không bị lệch thẻ
        const int end = qMin(text.size(), span.offset + span.length);
        if (end <= span.offset)
            continue;

        html += escapeHtml(text.mid(cursor, span.offset - cursor));
        const QString inner = escapeHtml(text.mid(span.offset, end - span.offset));

        switch (span.style) {
        case TextSpan::Style::Bold:
            html += QStringLiteral("<b>%1</b>").arg(inner);
            break;
        case TextSpan::Style::Italic:
            html += QStringLiteral("<i>%1</i>").arg(inner);
            break;
        case TextSpan::Style::Underline:
            html += QStringLiteral("<u>%1</u>").arg(inner);
            break;
        case TextSpan::Style::Strike:
            html += QStringLiteral("<s>%1</s>").arg(inner);
            break;
        case TextSpan::Style::Mono:
            html += QStringLiteral("<code style=\"font-family:monospace\">%1</code>").arg(inner);
            break;
        case TextSpan::Style::Code:
            html += QStringLiteral("<pre style=\"font-family:monospace;margin:2px 0\">%1</pre>")
                        .arg(inner);
            break;
        case TextSpan::Style::Link:
            html += QStringLiteral("<a href=\"%1\" style=\"color:%2;text-decoration:none\">%3</a>")
                        .arg(span.url.toHtmlEscaped(), linkColor.name(), inner);
            break;
        case TextSpan::Style::Mention:
        case TextSpan::Style::Hashtag:
            html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                        .arg(linkColor.name(), inner);
            break;
        case TextSpan::Style::Spoiler:
            html += QStringLiteral("<span style=\"background-color:%1;color:%1\">%2</span>")
                        .arg(linkColor.name(), inner);
            break;
        case TextSpan::Style::Plain:
            html += inner;
            break;
        }
        cursor = end;
    }
    html += escapeHtml(text.mid(cursor));
    return html;
}

Icons::Name glyphForKind(MessageEntry::Kind kind)
{
    switch (kind) {
    case MessageEntry::Kind::Audio:  return Icons::Name::Play;
    case MessageEntry::Kind::Voice:  return Icons::Name::Microphone;
    case MessageEntry::Kind::Video:  return Icons::Name::Play;
    case MessageEntry::Kind::Photo:  return Icons::Name::Image;
    default:                         return Icons::Name::File;
    }
}

} // namespace

MessageDelegate::MessageDelegate(MessageModel *model, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
{
    connect(&Theme::instance(), &Theme::changed, this, &MessageDelegate::invalidateCache);
}

MessageDelegate::~MessageDelegate() = default;

void MessageDelegate::invalidateCache()
{
    m_documents.clear();
    m_heights.clear();
}

void MessageDelegate::setShowGroupAvatars(bool show)
{
    if (m_showGroupAvatars == show)
        return;
    m_showGroupAvatars = show;
    invalidateCache();
}

QTextDocument *MessageDelegate::documentFor(const MessageEntry &entry, int width,
                                            bool outgoing, const QFont &font) const
{
    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(entry.id)
                            .arg(width)
                            .arg(entry.editDate)
                            .arg(font.pointSizeF(), 0, 'f', 1);
    auto it = m_documents.constFind(key);
    if (it != m_documents.constEnd())
        return it.value().data();

    if (m_documents.size() > 500)
        m_documents.clear();

    const Theme::Colors &c = Theme::instance().colors();
    const QColor linkColor = outgoing
        ? (c.dark ? QColor(0xcf, 0xe6, 0xff) : QColor(0x0b, 0x50, 0x9b))
        : c.link;

    auto document = QSharedPointer<QTextDocument>::create();
    document->setDocumentMargin(0);
    document->setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document->setDefaultTextOption(textOption);

    document->setHtml(spansToHtml(entry.text, entry.spans, linkColor));
    document->setTextWidth(width);

    m_documents.insert(key, document);
    return document.data();
}

QSize MessageDelegate::mediaDisplaySize(const MessageEntry &entry, int maxWidth) const
{
    int width = entry.mediaWidth;
    int height = entry.mediaHeight;

    if (width <= 0 || height <= 0) {
        const QString path = entry.mediaPath.isEmpty() ? entry.thumbPath : entry.mediaPath;
        const QSize real = FileCache::instance().imageSize(path);
        if (real.isValid() && real.width() > 0) {
            width = real.width();
            height = real.height();
        }
    }
    if (width <= 0 || height <= 0) {
        width = 320;
        height = 220;
    }

    if (entry.kind == MessageEntry::Kind::Sticker) {
        const int side = qMin(180, maxWidth);
        const double ratio = static_cast<double>(height) / qMax(1, width);
        return QSize(side, qMax(60, static_cast<int>(side * ratio)));
    }
    if (entry.kind == MessageEntry::Kind::VideoNote) {
        const int side = qMin(220, maxWidth);
        return QSize(side, side);
    }

    double scale = 1.0;
    if (width > maxWidth)
        scale = static_cast<double>(maxWidth) / width;
    if (height * scale > kMaxMediaHeight)
        scale = static_cast<double>(kMaxMediaHeight) / height;

    return QSize(qMax(80, static_cast<int>(width * scale)),
                 qMax(60, static_cast<int>(height * scale)));
}

QString MessageDelegate::metaTextFor(const MessageEntry &entry) const
{
    QStringList parts;
    if (entry.viewCount > 0)
        parts << QStringLiteral("%1 lượt xem").arg(entry.viewCount);
    if (entry.editDate > 0)
        parts << QStringLiteral("đã sửa");
    parts << Format::clock(entry.date);
    return parts.join(QStringLiteral(" · "));
}

int MessageDelegate::effectiveWidth(const QStyleOptionViewItem &option) const
{
    // option.rect rỗng ở một số lần gọi sizeHint() đầu tiên — lúc đó lấy chiều
    // rộng vùng hiển thị của view.
    if (option.rect.width() > 80)
        return option.rect.width();
    if (m_view && m_view->viewport()->width() > 80)
        return m_view->viewport()->width();
    return 480;
}

MessageDelegate::Layout MessageDelegate::computeLayout(const QStyleOptionViewItem &option,
                                                       const QModelIndex &index,
                                                       const MessageEntry &entry) const
{
    Layout layout;

    const int viewWidth = qMax(240, effectiveWidth(option));
    const QFontMetrics metrics(option.font);
    int y = option.rect.top() + kRowPadY;

    // --- Dải phân cách ngày ----------------------------------------------
    if (m_model && m_model->showDayDivider(index.row())) {
        layout.dividerRect = QRect(option.rect.left(), y, viewWidth, kDividerHeight);
        y += kDividerHeight;
    }

    // --- Tin nhắn hệ thống: viên thuốc căn giữa --------------------------
    if (entry.kind == MessageEntry::Kind::Service) {
        const QString text = entry.senderName.isEmpty()
            ? entry.text
            : QStringLiteral("%1 %2").arg(entry.senderName, entry.text);
        const int textWidth = qMin(viewWidth - 80, metrics.horizontalAdvance(text) + 24);
        const int height = metrics.height() + 12;
        layout.servicePill = QRect(option.rect.left() + (viewWidth - textWidth) / 2, y,
                                   qMax(60, textWidth), height);
        layout.totalHeight = layout.servicePill.bottom() - option.rect.top() + kRowPadY + 2;
        return layout;
    }

    const bool outgoing = entry.isOutgoing;
    const bool showAvatar = !outgoing && m_showGroupAvatars
        && m_model && m_model->showSenderName(index.row());
    const bool showSender = !outgoing && m_model && m_model->showSenderName(index.row());

    const int avatarLane = (!outgoing && m_showGroupAvatars) ? (kAvatarSize + kAvatarGap) : 0;

    int maxBubble = qMin(kMaxBubbleWidth, static_cast<int>(viewWidth * 0.74));
    maxBubble = qMax(kMinBubbleWidth + 40, maxBubble - avatarLane - kSidePad * 2);

    const int contentMax = maxBubble - kBubblePadX * 2;

    // --- Các khối trong bong bóng ----------------------------------------
    int contentWidth = 0;
    int contentHeight = 0;

    int senderHeight = 0;
    if (showSender) {
        senderHeight = metrics.height() + 2;
        contentWidth = qMax(contentWidth, metrics.horizontalAdvance(entry.senderName) + 8);
    }

    int forwardHeight = 0;
    if (!entry.forwardFromName.isEmpty()) {
        forwardHeight = metrics.height() + 2;
        contentWidth = qMax(contentWidth,
                            metrics.horizontalAdvance(entry.forwardFromName) + 60);
    }

    int replyHeight = 0;
    if (entry.replyToMessageId != 0) {
        replyHeight = metrics.height() * 2 + 10;
        contentWidth = qMax(contentWidth, 180);
    }

    QSize mediaSize;
    if (entry.isVisualMedia()) {
        mediaSize = mediaDisplaySize(entry, contentMax);
        layout.hasMediaBox = true;
        contentWidth = qMax(contentWidth, mediaSize.width());
        contentHeight += mediaSize.height();
    } else if (entry.hasMedia()) {
        layout.hasFileCard = true;
        contentWidth = qMax(contentWidth, qMin(contentMax, 300));
        contentHeight += kFileCardHeight;
    }

    int textHeight = 0;
    if (!entry.text.isEmpty() && entry.kind != MessageEntry::Kind::Sticker) {
        QTextDocument *document = documentFor(entry, contentMax, outgoing, option.font);
        const int ideal = qMin(contentMax, static_cast<int>(std::ceil(document->idealWidth())));
        // Chỉ dùng chiều rộng lý tưởng nếu chữ ngắn hơn khung tối đa.
        QTextDocument *fitted = documentFor(entry, qMax(60, ideal), outgoing, option.font);
        textHeight = static_cast<int>(std::ceil(fitted->size().height()));
        contentWidth = qMax(contentWidth, ideal);
        contentHeight += textHeight + (layout.hasMediaBox || layout.hasFileCard ? 6 : 0);
    }

    int reactionHeight = 0;
    if (!entry.reactionSummary.isEmpty()) {
        reactionHeight = metrics.height() + 8;
        contentWidth = qMax(contentWidth,
                            metrics.horizontalAdvance(entry.reactionSummary) + 16);
        contentHeight += reactionHeight;
    }

    // Dòng thông tin (giờ, đã sửa, dấu tích) nằm cùng hàng cuối nếu vừa.
    QFont metaFont = option.font;
    metaFont.setPointSizeF(metaFont.pointSizeF() * 0.82);
    const QFontMetrics metaMetrics(metaFont);
    const QString metaText = metaTextFor(entry);
    const int tickWidth = outgoing ? 18 : 0;
    const int metaWidth = metaMetrics.horizontalAdvance(metaText) + tickWidth + 6;
    const int metaHeight = metaMetrics.height();

    contentWidth = qMax(contentWidth, metaWidth);
    contentWidth = qMin(contentWidth, contentMax);
    contentWidth = qMax(contentWidth, kMinBubbleWidth - kBubblePadX * 2);

    contentHeight += senderHeight + forwardHeight + replyHeight + metaHeight + 2;

    const int bubbleWidth = contentWidth + kBubblePadX * 2;
    const int bubbleHeight = contentHeight + kBubblePadY * 2;

    const int bubbleLeft = outgoing
        ? option.rect.right() - kSidePad - bubbleWidth
        : option.rect.left() + kSidePad + avatarLane;

    layout.bubbleRect = QRect(bubbleLeft, y, bubbleWidth, bubbleHeight);

    if (showAvatar) {
        layout.avatarRect = QRect(option.rect.left() + kSidePad,
                                  y + bubbleHeight - kAvatarSize,
                                  kAvatarSize, kAvatarSize);
    }

    // --- Vị trí từng khối bên trong --------------------------------------
    int innerY = y + kBubblePadY;
    const int innerX = bubbleLeft + kBubblePadX;

    if (senderHeight > 0) {
        layout.senderRect = QRect(innerX, innerY, contentWidth, senderHeight);
        innerY += senderHeight;
    }
    if (forwardHeight > 0) {
        layout.forwardRect = QRect(innerX, innerY, contentWidth, forwardHeight);
        innerY += forwardHeight;
    }
    if (replyHeight > 0) {
        layout.replyRect = QRect(innerX, innerY, contentWidth, replyHeight - 4);
        innerY += replyHeight;
    }
    if (layout.hasMediaBox) {
        layout.mediaRect = QRect(innerX, innerY, mediaSize.width(), mediaSize.height());
        innerY += mediaSize.height() + (textHeight > 0 ? 6 : 0);
    }
    if (layout.hasFileCard) {
        layout.fileCardRect = QRect(innerX, innerY, contentWidth, kFileCardHeight);
        layout.fileButtonRect = QRect(innerX + 4, innerY + 6, 40, 40);
        innerY += kFileCardHeight + (textHeight > 0 ? 6 : 0);
    }
    if (textHeight > 0) {
        layout.textRect = QRect(innerX, innerY, contentWidth, textHeight);
        innerY += textHeight;
    }
    if (reactionHeight > 0) {
        layout.reactionRect = QRect(innerX, innerY, contentWidth, reactionHeight);
        innerY += reactionHeight;
    }

    layout.metaRect = QRect(bubbleLeft + bubbleWidth - kBubblePadX - metaWidth,
                            y + bubbleHeight - kBubblePadY - metaHeight,
                            metaWidth, metaHeight);

    layout.totalHeight = layout.bubbleRect.bottom() - option.rect.top() + kRowPadY + 2;
    return layout;
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const MessageEntry *entry = m_model ? m_model->entryAt(index) : nullptr;
    if (!entry)
        return QStyledItemDelegate::sizeHint(option, index);

    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(entry->id)
                            .arg(effectiveWidth(option))
                            .arg(entry->editDate)
                            .arg(entry->isDownloaded ? 1 : 0);
    auto cached = m_heights.constFind(key);
    if (cached != m_heights.constEnd())
        return QSize(option.rect.width(), cached.value());

    if (m_heights.size() > 4000)
        m_heights.clear();

    const Layout layout = computeLayout(option, index, *entry);
    m_heights.insert(key, layout.totalHeight);
    return QSize(effectiveWidth(option), layout.totalHeight);
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    const MessageEntry *messagePtr = m_model ? m_model->entryAt(index) : nullptr;
    if (!messagePtr) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const MessageEntry &entry = *messagePtr;
    const Theme::Colors &c = Theme::instance().colors();
    const Layout layout = computeLayout(option, index, entry);
    const qreal dpr = painter->device()->devicePixelRatioF();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    QFont metaFont = option.font;
    metaFont.setPointSizeF(metaFont.pointSizeF() * 0.82);

    // --- Dải ngày --------------------------------------------------------
    if (!layout.dividerRect.isNull()) {
        const QString text = Format::dayDivider(entry.date);
        const QFontMetrics metrics(metaFont);
        const int width = metrics.horizontalAdvance(text) + 26;
        const QRect pill(layout.dividerRect.center().x() - width / 2,
                         layout.dividerRect.top() + 6,
                         width, metrics.height() + 8);
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 22));
        painter->drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
        painter->setFont(metaFont);
        painter->setPen(c.textSecondary);
        painter->drawText(pill, Qt::AlignCenter, text);
    }

    // --- Tin nhắn hệ thống ------------------------------------------------
    if (entry.kind == MessageEntry::Kind::Service) {
        const QString text = entry.senderName.isEmpty()
            ? entry.text
            : QStringLiteral("%1 %2").arg(entry.senderName, entry.text);
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.dark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 16));
        painter->drawRoundedRect(layout.servicePill,
                                 layout.servicePill.height() / 2.0,
                                 layout.servicePill.height() / 2.0);
        painter->setFont(metaFont);
        painter->setPen(c.textSecondary);
        painter->drawText(layout.servicePill, Qt::AlignCenter,
                          QFontMetrics(metaFont).elidedText(text, Qt::ElideRight,
                                                            layout.servicePill.width() - 16));
        painter->restore();
        return;
    }

    const bool outgoing = entry.isOutgoing;
    const QColor bubbleColor = outgoing ? c.bubbleOut : c.bubbleIn;
    const QColor textColor = outgoing ? c.bubbleOutText : c.bubbleInText;
    const QColor metaColor = outgoing
        ? QColor(textColor.red(), textColor.green(), textColor.blue(), 170)
        : c.bubbleMeta;

    // --- Avatar ----------------------------------------------------------
    if (!layout.avatarRect.isNull()) {
        Avatar::Options options;
        options.initials = entry.senderInitials;
        options.colorIndex = entry.senderColorIndex;
        if (m_model && m_model->account()) {
            if (const UserEntry *sender = m_model->account()->user(entry.senderUserId))
                options.photoPath = sender->photoPath;
        }
        Avatar::paint(painter, layout.avatarRect, options);
    }

    // --- Bong bóng --------------------------------------------------------
    const bool tail = m_model ? m_model->isTailOfGroup(index.row()) : true;
    QPainterPath bubblePath;
    if (tail) {
        // Bớt bo một góc dưới ở phía người gửi để có cảm giác "đuôi" bong bóng.
        const QRectF r(layout.bubbleRect);
        const qreal big = kRadius;
        const qreal small = 5.0;
        bubblePath.moveTo(r.left() + big, r.top());
        bubblePath.lineTo(r.right() - big, r.top());
        bubblePath.quadTo(r.right(), r.top(), r.right(), r.top() + big);
        if (outgoing) {
            bubblePath.lineTo(r.right(), r.bottom() - small);
            bubblePath.quadTo(r.right(), r.bottom(), r.right() - small, r.bottom());
        } else {
            bubblePath.lineTo(r.right(), r.bottom() - big);
            bubblePath.quadTo(r.right(), r.bottom(), r.right() - big, r.bottom());
        }
        if (outgoing) {
            bubblePath.lineTo(r.left() + big, r.bottom());
            bubblePath.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - big);
        } else {
            bubblePath.lineTo(r.left() + small, r.bottom());
            bubblePath.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - small);
        }
        bubblePath.lineTo(r.left(), r.top() + big);
        bubblePath.quadTo(r.left(), r.top(), r.left() + big, r.top());
    } else {
        bubblePath.addRoundedRect(QRectF(layout.bubbleRect), kRadius, kRadius);
    }

    const bool sticker = entry.kind == MessageEntry::Kind::Sticker;
    if (!sticker) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(bubbleColor);
        painter->drawPath(bubblePath);
        if (!c.dark && !outgoing) {
            painter->setPen(QPen(c.divider, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(bubblePath);
        }
    }

    if (option.state & QStyle::State_Selected) {
        painter->setPen(QPen(c.accent, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(bubblePath);
    }

    // --- Tên người gửi ----------------------------------------------------
    if (!layout.senderRect.isNull()) {
        QFont senderFont = option.font;
        senderFont.setBold(true);
        senderFont.setPointSizeF(senderFont.pointSizeF() * 0.94);
        painter->setFont(senderFont);
        painter->setPen(Theme::instance().senderColor(entry.senderColorIndex));
        painter->drawText(layout.senderRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(senderFont).elidedText(entry.senderName, Qt::ElideRight,
                                                              layout.senderRect.width()));
    }

    // --- Chuyển tiếp từ ---------------------------------------------------
    if (!layout.forwardRect.isNull()) {
        QFont forwardFont = option.font;
        forwardFont.setItalic(true);
        forwardFont.setPointSizeF(forwardFont.pointSizeF() * 0.88);
        painter->setFont(forwardFont);
        painter->setPen(metaColor);
        painter->drawText(layout.forwardRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("Chuyển tiếp từ %1").arg(entry.forwardFromName));
    }

    // --- Khối trả lời -----------------------------------------------------
    if (!layout.replyRect.isNull()) {
        const QColor stripe = outgoing ? textColor : c.accent;
        painter->setPen(Qt::NoPen);
        painter->setBrush(outgoing ? QColor(255, 255, 255, 34)
                                   : QColor(c.accent.red(), c.accent.green(), c.accent.blue(), 26));
        painter->drawRoundedRect(layout.replyRect, 7, 7);
        painter->setBrush(stripe);
        painter->drawRoundedRect(QRect(layout.replyRect.left(), layout.replyRect.top(),
                                       3, layout.replyRect.height()), 2, 2);

        QFont nameFont = option.font;
        nameFont.setBold(true);
        nameFont.setPointSizeF(nameFont.pointSizeF() * 0.86);
        const QFontMetrics nameMetrics(nameFont);
        const QRect nameRect(layout.replyRect.left() + 9, layout.replyRect.top() + 3,
                             layout.replyRect.width() - 14, nameMetrics.height());
        painter->setFont(nameFont);
        painter->setPen(stripe);
        const QString replySender = entry.replyPreviewSender.isEmpty()
            ? QStringLiteral("Tin nhắn được trả lời") : entry.replyPreviewSender;
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics.elidedText(replySender, Qt::ElideRight, nameRect.width()));

        QFont bodyFont = option.font;
        bodyFont.setPointSizeF(bodyFont.pointSizeF() * 0.86);
        const QFontMetrics bodyMetrics(bodyFont);
        const QRect bodyRect(nameRect.left(), nameRect.bottom(),
                             nameRect.width(), bodyMetrics.height());
        painter->setFont(bodyFont);
        painter->setPen(metaColor);
        const QString replyBody = entry.replyPreviewText.isEmpty()
            ? QStringLiteral("(chưa tải được nội dung)") : entry.replyPreviewText;
        painter->drawText(bodyRect, Qt::AlignLeft | Qt::AlignVCenter,
                          bodyMetrics.elidedText(replyBody, Qt::ElideRight, bodyRect.width()));
    }

    // --- Ảnh / video / nhãn dán -------------------------------------------
    if (layout.hasMediaBox) {
        const QString path = entry.isDownloaded && !entry.mediaPath.isEmpty()
            ? entry.mediaPath : entry.thumbPath;
        const QPixmap image = path.isEmpty()
            ? QPixmap()
            : FileCache::instance().scaled(path, layout.mediaRect.size() * 2);

        QPainterPath mediaClip;
        mediaClip.addRoundedRect(QRectF(layout.mediaRect), sticker ? 4 : 10,
                                 sticker ? 4 : 10);

        if (image.isNull()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(c.dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 22));
            painter->drawPath(mediaClip);
            painter->drawPixmap(layout.mediaRect.center() - QPoint(14, 14),
                                Icons::pixmap(glyphForKind(entry.kind), metaColor, 28, dpr));
        } else {
            painter->save();
            painter->setClipPath(mediaClip);
            // Vẽ căn giữa, giữ tỉ lệ.
            QRect target = layout.mediaRect;
            const QSize scaled = image.size().scaled(target.size(), Qt::KeepAspectRatioByExpanding);
            QRect source(QPoint(0, 0), scaled);
            source.moveCenter(target.center());
            painter->drawPixmap(source, image);
            painter->restore();
        }

        // Lớp phủ cho video: nút play + thời lượng.
        if (entry.kind == MessageEntry::Kind::Video
            || entry.kind == MessageEntry::Kind::Animation
            || entry.kind == MessageEntry::Kind::VideoNote) {
            const int badge = 46;
            const QRect playRect(layout.mediaRect.center().x() - badge / 2,
                                 layout.mediaRect.center().y() - badge / 2, badge, badge);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 120));
            painter->drawEllipse(playRect);
            painter->drawPixmap(playRect.center() - QPoint(11, 11),
                                Icons::pixmap(Icons::Name::Play, Qt::white, 22, dpr));

            if (entry.durationSeconds > 0) {
                const QString duration = Format::duration(entry.durationSeconds);
                const QFontMetrics metrics(metaFont);
                const QRect tag(layout.mediaRect.left() + 8, layout.mediaRect.top() + 8,
                                metrics.horizontalAdvance(duration) + 14, metrics.height() + 4);
                painter->setBrush(QColor(0, 0, 0, 130));
                painter->drawRoundedRect(tag, tag.height() / 2.0, tag.height() / 2.0);
                painter->setFont(metaFont);
                painter->setPen(Qt::white);
                painter->drawText(tag, Qt::AlignCenter, duration);
            }
        }

        // Tiến trình tải.
        if (entry.isDownloading && entry.fileSize > 0) {
            const int barWidth = qMin(160, layout.mediaRect.width() - 24);
            const QRect bar(layout.mediaRect.center().x() - barWidth / 2,
                            layout.mediaRect.bottom() - 22, barWidth, 5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 90));
            painter->drawRoundedRect(bar, 2.5, 2.5);
            const double ratio = qBound(0.0,
                static_cast<double>(entry.downloadedSize) / entry.fileSize, 1.0);
            QRect done = bar;
            done.setWidth(static_cast<int>(bar.width() * ratio));
            painter->setBrush(Qt::white);
            painter->drawRoundedRect(done, 2.5, 2.5);
        }
    }

    // --- Thẻ tệp / âm thanh ------------------------------------------------
    if (layout.hasFileCard) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(outgoing ? QColor(255, 255, 255, 30)
                                   : (c.dark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 14)));
        painter->drawRoundedRect(layout.fileCardRect, 9, 9);

        // Nút tròn: tải về hoặc mở.
        painter->setBrush(outgoing ? QColor(255, 255, 255, 60) : c.accent);
        painter->drawEllipse(layout.fileButtonRect);
        const Icons::Name action = entry.isDownloaded
            ? glyphForKind(entry.kind) : Icons::Name::Download;
        painter->drawPixmap(layout.fileButtonRect.center() - QPoint(10, 10),
                            Icons::pixmap(action, outgoing ? textColor : c.textOnAccent, 20, dpr));

        QFont nameFont = option.font;
        nameFont.setBold(true);
        nameFont.setPointSizeF(nameFont.pointSizeF() * 0.92);
        const QFontMetrics nameMetrics(nameFont);

        const int infoLeft = layout.fileButtonRect.right() + 10;
        const int infoWidth = layout.fileCardRect.right() - infoLeft - 8;

        QString primary = entry.fileName;
        if (entry.kind == MessageEntry::Kind::Voice)
            primary = QStringLiteral("Tin nhắn thoại");
        else if (entry.kind == MessageEntry::Kind::Audio && !entry.audioTitle.isEmpty())
            primary = entry.audioTitle;
        if (primary.isEmpty())
            primary = entry.kindLabel();

        painter->setFont(nameFont);
        painter->setPen(textColor);
        painter->drawText(QRect(infoLeft, layout.fileCardRect.top() + 8, infoWidth,
                                nameMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics.elidedText(primary, Qt::ElideMiddle, infoWidth));

        QStringList details;
        if (entry.durationSeconds > 0)
            details << Format::duration(entry.durationSeconds);
        if (!entry.performer.isEmpty())
            details << entry.performer;
        if (entry.fileSize > 0) {
            details << (entry.isDownloading
                ? QStringLiteral("%1 / %2").arg(Format::fileSize(entry.downloadedSize),
                                                Format::fileSize(entry.fileSize))
                : Format::fileSize(entry.fileSize));
        }
        if (entry.isDownloaded)
            details << QStringLiteral("đã tải");

        painter->setFont(metaFont);
        painter->setPen(metaColor);
        painter->drawText(QRect(infoLeft, layout.fileCardRect.top() + 8 + nameMetrics.height(),
                                infoWidth, QFontMetrics(metaFont).height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          details.join(QStringLiteral(" · ")));
    }

    // --- Nội dung chữ ------------------------------------------------------
    if (!layout.textRect.isNull()) {
        QTextDocument *document = documentFor(entry, layout.textRect.width(), outgoing,
                                              option.font);
        painter->save();
        painter->translate(layout.textRect.topLeft());
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Text, textColor);
        context.clip = QRectF(0, 0, layout.textRect.width(), layout.textRect.height() + 4);
        document->documentLayout()->draw(painter, context);
        painter->restore();
    }

    // --- Phản ứng ---------------------------------------------------------
    if (!layout.reactionRect.isNull()) {
        painter->setFont(metaFont);
        painter->setPen(outgoing ? textColor : c.accent);
        painter->drawText(layout.reactionRect, Qt::AlignLeft | Qt::AlignVCenter,
                          entry.reactionSummary);
    }

    // --- Giờ + dấu tích ----------------------------------------------------
    painter->setFont(metaFont);
    painter->setPen(metaColor);
    const QString metaText = metaTextFor(entry);
    QRect metaTextRect = layout.metaRect;
    if (outgoing)
        metaTextRect.setWidth(metaTextRect.width() - 18);
    painter->drawText(metaTextRect, Qt::AlignRight | Qt::AlignVCenter, metaText);

    if (outgoing) {
        Icons::Name tick = Icons::Name::Check;
        QColor tickColor = metaColor;
        switch (entry.sendState) {
        case MessageEntry::SendState::Sending:
            tick = Icons::Name::Clock;
            break;
        case MessageEntry::SendState::Failed:
            tick = Icons::Name::Warning;
            tickColor = c.danger;
            break;
        case MessageEntry::SendState::Sent: {
            bool read = false;
            if (m_model && m_model->account()) {
                if (const ChatEntry *chatEntry = m_model->account()->chat(entry.chatId))
                    read = chatEntry->lastReadOutboxMessageId >= entry.id;
            }
            if (read) {
                tick = Icons::Name::DoubleCheck;
                tickColor = c.dark ? QColor(0xbf, 0xe4, 0xff) : QColor(0x1c, 0x74, 0xc7);
            }
            break;
        }
        }
        painter->drawPixmap(QPoint(layout.metaRect.right() - 16, layout.metaRect.top() + 1),
                            Icons::pixmap(tick, tickColor, 15, dpr));
    }

    if (entry.isPinned) {
        painter->drawPixmap(QPoint(layout.bubbleRect.right() - 16, layout.bubbleRect.top() + 6),
                            Icons::pixmap(Icons::Name::Pin, metaColor, 12, dpr));
    }

    painter->restore();
}

MessageDelegate::Hit MessageDelegate::hitTest(const QStyleOptionViewItem &option,
                                              const QModelIndex &index,
                                              const QPoint &position) const
{
    Hit hit;
    const MessageEntry *entry = m_model ? m_model->entryAt(index) : nullptr;
    if (!entry)
        return hit;

    const Layout layout = computeLayout(option, index, *entry);

    if (layout.hasFileCard && layout.fileCardRect.contains(position)) {
        hit.kind = HitKind::FileAction;
        return hit;
    }
    if (layout.hasMediaBox && layout.mediaRect.contains(position)) {
        hit.kind = HitKind::Media;
        return hit;
    }
    if (!layout.replyRect.isNull() && layout.replyRect.contains(position)) {
        hit.kind = HitKind::ReplyQuote;
        hit.replyToMessageId = entry->replyToMessageId;
        return hit;
    }
    if (!layout.textRect.isNull() && layout.textRect.contains(position)) {
        QTextDocument *document = documentFor(*entry, layout.textRect.width(),
                                              entry->isOutgoing, option.font);
        const QPointF local = position - layout.textRect.topLeft();
        const QString anchor = document->documentLayout()->anchorAt(local);
        if (!anchor.isEmpty()) {
            hit.kind = HitKind::Link;
            hit.link = anchor;
            return hit;
        }
    }
    if (layout.bubbleRect.contains(position)) {
        hit.kind = HitKind::Bubble;
        return hit;
    }
    return hit;
}
