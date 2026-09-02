#pragma once

#include <QMetaType>
#include <QString>

//! Bộ lọc nhanh phía trên danh sách chat.
enum class ChatFilterKind {
    All,
    Unread,
    Private,
    Groups,
    Channels,
    Bots,
    Archived
};

inline QString chatFilterLabel(ChatFilterKind kind)
{
    switch (kind) {
    case ChatFilterKind::All:      return QStringLiteral("Tất cả");
    case ChatFilterKind::Unread:   return QStringLiteral("Chưa đọc");
    case ChatFilterKind::Private:  return QStringLiteral("Riêng");
    case ChatFilterKind::Groups:   return QStringLiteral("Nhóm");
    case ChatFilterKind::Channels: return QStringLiteral("Kênh");
    case ChatFilterKind::Bots:     return QStringLiteral("Bot");
    case ChatFilterKind::Archived: return QStringLiteral("Lưu");
    }
    return QStringLiteral("Tất cả");
}

//! Trạng thái kết nối tới máy chủ Telegram.
enum class TdConnectionState {
    Unknown,
    WaitingForNetwork,
    ConnectingToProxy,
    Connecting,
    Updating,
    Ready
};

inline QString connectionStateLabel(TdConnectionState state)
{
    switch (state) {
    case TdConnectionState::WaitingForNetwork: return QStringLiteral("Đang chờ mạng…");
    case TdConnectionState::ConnectingToProxy: return QStringLiteral("Đang kết nối proxy…");
    case TdConnectionState::Connecting:        return QStringLiteral("Đang kết nối…");
    case TdConnectionState::Updating:          return QStringLiteral("Đang cập nhật…");
    case TdConnectionState::Ready:             return QStringLiteral("Đã kết nối");
    case TdConnectionState::Unknown:           break;
    }
    return QStringLiteral("Chưa kết nối");
}

Q_DECLARE_METATYPE(ChatFilterKind)
Q_DECLARE_METATYPE(TdConnectionState)
