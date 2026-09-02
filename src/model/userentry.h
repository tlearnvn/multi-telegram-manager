#pragma once

#include <QString>

//! Thông tin một người dùng Telegram (rút gọn từ đối tượng "user" của TDLib).
struct UserEntry
{
    enum class Presence { Unknown, Online, Offline, Recently, LastWeek, LastMonth };

    qint64 id = 0;
    QString firstName;
    QString lastName;
    QString username;
    QString phoneNumber;
    bool isBot = false;
    bool isContact = false;
    bool isVerified = false;
    bool isSupport = false;
    bool isDeleted = false;
    Presence presence = Presence::Unknown;
    qint64 lastSeen = 0;         //!< unix seconds, chỉ có khi Offline
    int photoFileId = 0;         //!< ảnh nhỏ, dùng cho avatar
    QString photoPath;           //!< đường dẫn cục bộ sau khi tải xong

    QString displayName() const
    {
        const QString full = QStringLiteral("%1 %2").arg(firstName, lastName).trimmed();
        if (!full.isEmpty())
            return full;
        if (!username.isEmpty())
            return QLatin1Char('@') + username;
        if (isDeleted)
            return QStringLiteral("Tài khoản đã xoá");
        return QStringLiteral("Người dùng %1").arg(id);
    }

    QString handle() const
    {
        return username.isEmpty() ? QString() : QLatin1Char('@') + username;
    }
};
