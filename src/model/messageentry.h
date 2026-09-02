#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

//! Một đoạn định dạng trong nội dung tin nhắn (đậm, nghiêng, liên kết…).
struct TextSpan
{
    enum class Style { Plain, Bold, Italic, Underline, Strike, Mono, Code, Link, Mention, Hashtag, Spoiler };

    int offset = 0;      //!< tính theo QChar (UTF-16), khớp với TDLib
    int length = 0;
    Style style = Style::Plain;
    QString url;         //!< chỉ dùng với Link
};

//! Một tin nhắn.
struct MessageEntry
{
    enum class Kind {
        Text, Photo, Video, Animation, Audio, Voice, Document, Sticker,
        Location, Venue, Contact, Poll, VideoNote, Service, Unsupported
    };

    enum class SendState { Sent, Sending, Failed };

    qint64 id = 0;
    qint64 chatId = 0;
    qint64 senderUserId = 0;
    qint64 senderChatId = 0;
    QString senderName;
    QString senderInitials;
    int senderColorIndex = 0;

    qint64 date = 0;
    qint64 editDate = 0;
    bool isOutgoing = false;
    bool isPinned = false;
    bool canBeEdited = false;
    bool canBeDeletedForAll = false;
    bool canBeForwarded = true;
    bool containsUnreadMention = false;
    SendState sendState = SendState::Sent;
    QString failureReason;

    Kind kind = Kind::Text;
    QString text;                 //!< nội dung hoặc chú thích của media
    QList<TextSpan> spans;

    // Trả lời / chuyển tiếp
    qint64 replyToMessageId = 0;
    QString replyPreviewSender;
    QString replyPreviewText;
    QString forwardFromName;

    // Media
    int mediaFileId = 0;
    QString mediaPath;
    int thumbFileId = 0;
    QString thumbPath;
    int mediaWidth = 0;
    int mediaHeight = 0;
    QString fileName;
    qint64 fileSize = 0;
    qint64 downloadedSize = 0;
    bool isDownloading = false;
    bool isDownloaded = false;
    int durationSeconds = 0;
    QString mimeType;
    QString performer;            //!< audio
    QString audioTitle;

    // Kênh
    int viewCount = 0;
    int forwardCount = 0;

    // Phản ứng gộp: "👍 3  ❤️ 1"
    QString reactionSummary;

    bool hasMedia() const
    {
        switch (kind) {
        case Kind::Photo:
        case Kind::Video:
        case Kind::Animation:
        case Kind::Audio:
        case Kind::Voice:
        case Kind::Document:
        case Kind::Sticker:
        case Kind::VideoNote:
            return true;
        default:
            return false;
        }
    }

    bool isVisualMedia() const
    {
        return kind == Kind::Photo || kind == Kind::Video
            || kind == Kind::Animation || kind == Kind::Sticker
            || kind == Kind::VideoNote;
    }

    QString kindLabel() const
    {
        switch (kind) {
        case Kind::Photo:       return QStringLiteral("Ảnh");
        case Kind::Video:       return QStringLiteral("Video");
        case Kind::Animation:   return QStringLiteral("Ảnh động");
        case Kind::Audio:       return QStringLiteral("Bản nhạc");
        case Kind::Voice:       return QStringLiteral("Tin nhắn thoại");
        case Kind::Document:    return QStringLiteral("Tệp");
        case Kind::Sticker:     return QStringLiteral("Nhãn dán");
        case Kind::Location:    return QStringLiteral("Vị trí");
        case Kind::Venue:       return QStringLiteral("Địa điểm");
        case Kind::Contact:     return QStringLiteral("Danh thiếp");
        case Kind::Poll:        return QStringLiteral("Bình chọn");
        case Kind::VideoNote:   return QStringLiteral("Video tròn");
        case Kind::Service:     return QStringLiteral("Thông báo");
        case Kind::Unsupported: return QStringLiteral("Nội dung chưa hỗ trợ");
        case Kind::Text:        break;
        }
        return QStringLiteral("Tin nhắn");
    }
};

Q_DECLARE_METATYPE(MessageEntry)
Q_DECLARE_METATYPE(QList<MessageEntry>)
