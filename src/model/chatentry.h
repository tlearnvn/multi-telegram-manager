#pragma once

#include <QString>

//! Một cuộc trò chuyện trong danh sách chat.
struct ChatEntry
{
    enum class Kind { Unknown, Private, Secret, BasicGroup, Supergroup, Channel };

    qint64 id = 0;
    QString title;
    Kind kind = Kind::Unknown;

    // Ảnh đại diện
    int photoFileId = 0;
    QString photoPath;

    // Vị trí trong danh sách chính
    qint64 order = 0;
    bool isPinned = false;
    bool inArchive = false;

    // Tin nhắn cuối
    qint64 lastMessageId = 0;
    qint64 lastMessageDate = 0;
    QString lastMessagePreview;      //!< đã rút gọn một dòng
    QString lastMessageSender;       //!< tên người gửi (nhóm/kênh)
    bool lastMessageOutgoing = false;
    int lastMessageSendState = 0;    //!< 0 = đã gửi, 1 = đang gửi, 2 = lỗi

    // Trạng thái đọc
    int unreadCount = 0;
    int unreadMentionCount = 0;
    bool isMarkedUnread = false;
    qint64 lastReadInboxMessageId = 0;
    qint64 lastReadOutboxMessageId = 0;

    // Thông báo
    bool isMuted = false;

    // Nháp và hành động đang diễn ra ("đang gõ…")
    QString draftText;
    QString actionText;

    // Thông tin bổ sung
    int memberCount = 0;
    int onlineMemberCount = 0;
    bool canSendMessages = true;
    bool isVerified = false;
    bool isForum = false;
    bool isBlocked = false;
    qint64 relatedUserId = 0;        //!< với chat riêng: id người đối diện
    qint64 supergroupId = 0;
    qint64 basicGroupId = 0;
    QString username;                //!< @username của kênh/nhóm công khai
    QString statusLine;              //!< "đang hoạt động", "1.234 thành viên"…

    bool isGroupLike() const
    {
        return kind == Kind::BasicGroup || kind == Kind::Supergroup;
    }

    bool hasUnread() const
    {
        return unreadCount > 0 || isMarkedUnread;
    }

    QString kindLabel() const
    {
        switch (kind) {
        case Kind::Private:     return QStringLiteral("Trò chuyện riêng");
        case Kind::Secret:      return QStringLiteral("Trò chuyện mật");
        case Kind::BasicGroup:  return QStringLiteral("Nhóm");
        case Kind::Supergroup:  return QStringLiteral("Nhóm lớn");
        case Kind::Channel:     return QStringLiteral("Kênh");
        case Kind::Unknown:     break;
        }
        return QStringLiteral("Cuộc trò chuyện");
    }
};
