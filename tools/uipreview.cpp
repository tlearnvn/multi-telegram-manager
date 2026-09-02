/*!
 * \file uipreview.cpp
 * \brief Công cụ xuất ảnh xem trước giao diện (không đóng gói kèm ứng dụng).
 *
 * Dựng cửa sổ chính với dữ liệu giả rồi lưu ảnh PNG, để soi giao diện mà không
 * cần TDLib hay tài khoản Telegram thật.
 *
 * Điểm hay: dữ liệu giả được đưa vào bằng đúng định dạng JSON mà TDLib gửi, qua
 * đúng hàm TdAccount::handleIncoming() mà bản chạy thật dùng. Nhờ vậy công cụ
 * này vừa xem được giao diện, vừa kiểm tra luôn phần phân tích JSON.
 *
 * Cách dùng:
 *   cmake -S . -B build -DBUILD_UI_PREVIEW=ON
 *   cmake --build build --target tuan_uipreview
 *   QT_QPA_PLATFORM=offscreen ./build/tuan_uipreview /thu/muc/xuat-anh
 */

#include "core/apppaths.h"
#include "core/settingsstore.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "ui/iconfactory.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

namespace {

QJsonObject parse(const QString &json)
{
    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        std::fprintf(stderr, "JSON mẫu sai ở vị trí %d: %s\n",
                     error.offset, qPrintable(error.errorString()));
        std::fflush(stderr);
    }
    return document.object();
}

void feed(TdAccount *account, const QString &json)
{
    account->handleIncoming(parse(json));
}

qint64 minutesAgo(int minutes)
{
    return QDateTime::currentSecsSinceEpoch() - qint64(minutes) * 60;
}

//! Đưa một tài khoản vào trạng thái "đã đăng nhập" mà không cần TDLib.
void markReady(TdAccount *account)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateAuthorizationState",
        "authorization_state": { "@type": "authorizationStateReady" }
    })"));
    feed(account, QStringLiteral(R"({
        "@type": "updateConnectionState",
        "state": { "@type": "connectionStateReady" }
    })"));
}

void addUser(TdAccount *account, qint64 id, const QString &first, const QString &last,
             const QString &username, const QString &status)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateUser",
        "user": {
            "@type": "user", "id": %1,
            "first_name": "%2", "last_name": "%3", "username": "%4",
            "phone_number": "84912345%5",
            "status": { "@type": "%6" },
            "type": { "@type": "userTypeRegular" },
            "is_contact": true
        }
    })").arg(id).arg(first, last, username)
        .arg(id % 1000, 3, 10, QLatin1Char('0'))
        .arg(status));
}

void addPrivateChat(TdAccount *account, qint64 chatId, qint64 userId, const QString &title,
                    qint64 order, int unread, const QString &lastText, int minutes,
                    bool outgoing, bool muted = false, bool pinned = false)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateNewChat",
        "chat": {
            "@type": "chat", "id": %1,
            "type": { "@type": "chatTypePrivate", "user_id": %2 },
            "title": "%3",
            "unread_count": %4,
            "notification_settings": { "@type": "chatNotificationSettings", "mute_for": %5 },
            "permissions": { "@type": "chatPermissions", "can_send_basic_messages": true },
            "positions": [ { "@type": "chatPosition",
                             "list": { "@type": "chatListMain" },
                             "order": "%6", "is_pinned": %7 } ],
            "last_message": {
                "@type": "message", "id": %8, "chat_id": %1,
                "sender_id": { "@type": "messageSenderUser", "user_id": %9 },
                "date": %10, "is_outgoing": %11,
                "content": { "@type": "messageText",
                             "text": { "@type": "formattedText", "text": "%12" } }
            }
        }
    })").arg(chatId).arg(userId).arg(title).arg(unread)
        .arg(muted ? 31536000 : 0)
        .arg(order)
        .arg(pinned ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(chatId * 100 + 1)
        .arg(outgoing ? account->myId() : userId)
        .arg(minutesAgo(minutes))
        .arg(outgoing ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(lastText));
}

void addGroupChat(TdAccount *account, qint64 chatId, qint64 supergroupId, const QString &title,
                  bool isChannel, int members, qint64 order, int unread,
                  const QString &senderName, const QString &lastText, int minutes)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateSupergroup",
        "supergroup": { "@type": "supergroup", "id": %1, "member_count": %2,
                        "is_channel": %3, "username": "kenh%1" }
    })").arg(supergroupId).arg(members)
        .arg(isChannel ? QStringLiteral("true") : QStringLiteral("false")));

    feed(account, QStringLiteral(R"({
        "@type": "updateNewChat",
        "chat": {
            "@type": "chat", "id": %1,
            "type": { "@type": "chatTypeSupergroup", "supergroup_id": %2,
                      "is_channel": %3 },
            "title": "%4",
            "unread_count": %5,
            "notification_settings": { "@type": "chatNotificationSettings", "mute_for": 0 },
            "permissions": { "@type": "chatPermissions", "can_send_basic_messages": %6 },
            "positions": [ { "@type": "chatPosition",
                             "list": { "@type": "chatListMain" },
                             "order": "%7", "is_pinned": false } ],
            "last_message": {
                "@type": "message", "id": %8, "chat_id": %1,
                "sender_id": { "@type": "messageSenderUser", "user_id": 900001 },
                "date": %9, "is_outgoing": false,
                "content": { "@type": "messageText",
                             "text": { "@type": "formattedText", "text": "%10" } }
            }
        }
    })").arg(chatId).arg(supergroupId)
        .arg(isChannel ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(title).arg(unread)
        .arg(isChannel ? QStringLiteral("false") : QStringLiteral("true"))
        .arg(order)
        .arg(chatId * 100 + 1)
        .arg(minutesAgo(minutes))
        .arg(lastText));
    Q_UNUSED(senderName)
}

/*!
 * Dựng mảng entity giống TDLib: tìm đoạn con trong chuỗi rồi lấy đúng offset
 * theo QChar (UTF-16), thay vì đếm tay — đếm tay là lệch ngay với tiếng Việt.
 */
QString entitiesFor(const QString &text,
                    const QList<QPair<QString, QString>> &parts)
{
    QJsonArray array;
    for (const auto &part : parts) {
        const int offset = text.indexOf(part.first);
        if (offset < 0) {
            std::fprintf(stderr, "Không tìm thấy đoạn \"%s\" trong tin nhắn mẫu\n",
                         qPrintable(part.first));
            continue;
        }
        QJsonObject type;
        type.insert(QStringLiteral("@type"), part.second);
        QJsonObject entity;
        entity.insert(QStringLiteral("@type"), QStringLiteral("textEntity"));
        entity.insert(QStringLiteral("offset"), offset);
        entity.insert(QStringLiteral("length"), part.first.size());
        entity.insert(QStringLiteral("type"), type);
        array.append(entity);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

//! Một tin nhắn văn bản trong khung hội thoại.
void addMessage(TdAccount *account, qint64 chatId, qint64 messageId, qint64 senderId,
                bool outgoing, int minutes, const QString &text,
                const QString &entities = QStringLiteral("[]"),
                qint64 replyTo = 0)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateNewMessage",
        "message": {
            "@type": "message", "id": %1, "chat_id": %2,
            "sender_id": { "@type": "messageSenderUser", "user_id": %3 },
            "date": %4, "is_outgoing": %5,
            "can_be_edited": %5, "can_be_deleted_for_all_users": %5,
            "reply_to_message_id": %6,
            "content": { "@type": "messageText",
                         "text": { "@type": "formattedText", "text": "%7",
                                   "entities": %8 } }
        }
    })").arg(messageId).arg(chatId).arg(senderId)
        .arg(minutesAgo(minutes))
        .arg(outgoing ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(replyTo)
        .arg(text)
        .arg(entities));
}

void addServiceMessage(TdAccount *account, qint64 chatId, qint64 messageId, qint64 senderId,
                       int minutes, const QString &title)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateNewMessage",
        "message": {
            "@type": "message", "id": %1, "chat_id": %2,
            "sender_id": { "@type": "messageSenderUser", "user_id": %3 },
            "date": %4, "is_outgoing": false,
            "content": { "@type": "messageChatChangeTitle", "title": "%5" }
        }
    })").arg(messageId).arg(chatId).arg(senderId).arg(minutesAgo(minutes)).arg(title));
}

void addDocumentMessage(TdAccount *account, qint64 chatId, qint64 messageId, qint64 senderId,
                        bool outgoing, int minutes, const QString &fileName, qint64 size)
{
    feed(account, QStringLiteral(R"({
        "@type": "updateNewMessage",
        "message": {
            "@type": "message", "id": %1, "chat_id": %2,
            "sender_id": { "@type": "messageSenderUser", "user_id": %3 },
            "date": %4, "is_outgoing": %5,
            "content": {
                "@type": "messageDocument",
                "document": {
                    "@type": "document", "file_name": "%6",
                    "mime_type": "application/pdf",
                    "document": { "@type": "file", "id": 5001, "size": %7,
                                  "local": { "@type": "localFile", "path": "",
                                             "is_downloading_completed": false,
                                             "downloaded_size": 0 } }
                },
                "caption": { "@type": "formattedText", "text": "Bảng giá tháng này nha" }
            }
        }
    })").arg(messageId).arg(chatId).arg(senderId).arg(minutesAgo(minutes))
        .arg(outgoing ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(fileName).arg(size));
}

void save(QWidget *widget, const QString &path)
{
    // Cho Qt kịp bố cục xong trước khi chụp.
    for (int i = 0; i < 6; ++i)
        QApplication::processEvents();

    const QPixmap shot = widget->grab();
    if (shot.save(path))
        std::printf("Đã lưu %s (%dx%d)\n", qPrintable(path), shot.width(), shot.height());
    else
        std::fprintf(stderr, "Không lưu được %s\n", qPrintable(path));
    std::fflush(stdout);
}

} // namespace

int main(int argc, char *argv[])
{
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QDir::currentPath();
    QDir().mkpath(outDir);

    // Dữ liệu của bản xem trước nằm ở thư mục tạm, không đụng vào data/ thật.
    QTemporaryDir tempData;
    qputenv("TUAN_MULTITELE_DATA", tempData.path().toLocal8Bit());

    QApplication app(argc, argv);
    app.setWindowIcon(Icons::appIcon());

    AppPaths::initialize();

    SettingsStore &settings = SettingsStore::instance();
    settings.setApiId(123456);
    settings.setApiHash(QStringLiteral("0123456789abcdef0123456789abcdef"));
    settings.setSetupCompleted(true);
    settings.setThemeMode(SettingsStore::ThemeMode::Dark);
    settings.setTrayEnabled(false);

    Theme::instance().apply();

    AccountManager manager;

    // --- Ba tài khoản, mỗi tài khoản một màu ------------------------------
    struct Seed { const char *label; const char *color; qint64 myId; };
    const Seed seeds[] = {
        { "Tuấn (cá nhân)", "#2ea6ff", 700001 },
        { "Shop Tuấn",      "#4dd0a0", 700002 },
        { "Việc công ty",   "#f5a623", 700003 },
    };

    QList<TdAccount *> accounts;
    for (const Seed &seed : seeds) {
        TdAccount *account = manager.createAccount(QString::fromUtf8(seed.label));
        account->setAccentColor(QString::fromLatin1(seed.color));
        markReady(account);
        addUser(account, seed.myId, QString::fromUtf8(seed.label), QString(),
                QStringLiteral("tuan"), QStringLiteral("userStatusOnline"));
        accounts.append(account);
    }

    TdAccount *primary = accounts.first();
    // createAccount() tự chuyển sang tài khoản mới nhất, nên chọn lại tài khoản
    // đầu — đó là tài khoản có dữ liệu mẫu.
    manager.setActiveAccount(primary);

    // --- Danh bạ và danh sách chat cho tài khoản đầu ----------------------
    addUser(primary, 900001, QStringLiteral("Nguyễn"), QStringLiteral("Minh Hà"),
            QStringLiteral("minhha"), QStringLiteral("userStatusOnline"));
    addUser(primary, 900002, QStringLiteral("Trần"), QStringLiteral("Quốc Bảo"),
            QStringLiteral("quocbao"), QStringLiteral("userStatusRecently"));
    addUser(primary, 900003, QStringLiteral("Lê"), QStringLiteral("Thị Thu"),
            QStringLiteral("lethu"), QStringLiteral("userStatusOffline"));
    addUser(primary, 900004, QStringLiteral("Phạm"), QStringLiteral("Văn Dũng"),
            QStringLiteral("vandung"), QStringLiteral("userStatusLastWeek"));

    addPrivateChat(primary, 900001, 900001, QStringLiteral("Nguyễn Minh Hà"),
                   9000, 3, QStringLiteral("Ok mai mình gặp ở quán cũ nhé 😄"), 2, false, false, true);
    addGroupChat(primary, -1001, 1001, QStringLiteral("Nhóm dự án MultiTele"),
                 false, 24, 8900, 12, QStringLiteral("Quốc Bảo"),
                 QStringLiteral("Quốc Bảo: bản build mới đã lên artifacts rồi"), 8);
    addPrivateChat(primary, 900002, 900002, QStringLiteral("Trần Quốc Bảo"),
                   8800, 0, QStringLiteral("Mình gửi file rồi nha"), 25, true);
    addGroupChat(primary, -1002, 1002, QStringLiteral("Kênh Thông báo Công ty"),
                 true, 1420, 8700, 2, QStringLiteral(""),
                 QStringLiteral("Lịch nghỉ lễ tháng này, mọi người xem kỹ giúp"), 95);
    addPrivateChat(primary, 900003, 900003, QStringLiteral("Lê Thị Thu"),
                   8600, 0, QStringLiteral("Cảm ơn bạn nhiều!"), 190, false, true);
    addPrivateChat(primary, 900004, 900004, QStringLiteral("Phạm Văn Dũng"),
                   8500, 1, QStringLiteral("Anh xem lại hợp đồng giúp em với"), 1500, false);

    // Đang gõ trong nhóm.
    feed(primary, QStringLiteral(R"({
        "@type": "updateChatAction", "chat_id": -1001,
        "sender_id": { "@type": "messageSenderUser", "user_id": 900002 },
        "action": { "@type": "chatActionTyping" }
    })"));

    // --- Hội thoại mẫu trong nhóm ----------------------------------------
    // (đưa vào trước khi mở cửa sổ; MessageModel tự nạp lại từ bộ nhớ)
    addServiceMessage(primary, -1001, 100101, 900001, 1450,
                      QStringLiteral("Nhóm dự án MultiTele"));
    addMessage(primary, -1001, 100102, 900001, false, 1400,
               QStringLiteral("Chào cả nhà, mình vừa đẩy phần đa tài khoản lên nhánh chính."));
    addMessage(primary, -1001, 100103, 900002, false, 1380,
               QStringLiteral("Đỉnh quá! Mình test thử với 3 tài khoản cùng lúc, chuyển qua lại rất mượt."));
    {
        // TDLib gửi văn bản sạch kèm danh sách entity — không có ký hiệu Markdown.
        const QString text = QStringLiteral(
            "Mình có thêm bảng điều khiển để xem dung lượng từng tài khoản nữa, "
            "vào bằng Ctrl+D nha.");
        addMessage(primary, -1001, 100104, 700001, true, 1360, text,
                   entitiesFor(text, {
                       { QStringLiteral("bảng điều khiển"),
                         QStringLiteral("textEntityTypeBold") },
                       { QStringLiteral("Ctrl+D"),
                         QStringLiteral("textEntityTypeCode") },
                   }));
    }
    addMessage(primary, -1001, 100105, 900003, false, 40,
               QStringLiteral("Cho mình hỏi phần gửi tin hàng loạt có giới hạn số nơi gửi không?"));
    addMessage(primary, -1001, 100106, 700001, true, 35,
               QStringLiteral("Không giới hạn, nhưng nên để giãn cách 3–10 giây để tránh bị "
                              "Telegram chặn tốc độ."),
               QStringLiteral("[]"), 100105);
    addDocumentMessage(primary, -1001, 100107, 900002, false, 20,
                       QStringLiteral("bao-gia-thang-9.pdf"), 2483712);
    {
        const QString link = QStringLiteral("https://github.com/tlearnvn/multi-telegram-manager");
        const QString text = QStringLiteral(
            "Bản build mới đã lên artifacts rồi, mọi người tải về thử nhé. "
            "Xem chi tiết ở ") + link;
        addMessage(primary, -1001, 100108, 900002, false, 8, text,
                   entitiesFor(text, { { link, QStringLiteral("textEntityTypeUrl") } }));
    }

    // --- Cửa sổ ------------------------------------------------------------
    MainWindow window(&manager);
    window.resize(1360, 860);
    window.show();

    for (int i = 0; i < 8; ++i)
        QApplication::processEvents();

    window.onChatSelected(-1001);
    save(&window, QDir(outDir).filePath(QStringLiteral("01-chat-toi.png")));

    window.openDashboard();
    save(&window, QDir(outDir).filePath(QStringLiteral("02-bang-dieu-khien.png")));

    window.onChatSelected(900001);
    settings.setThemeMode(SettingsStore::ThemeMode::Light);
    Icons::clearCache();
    Theme::instance().apply();
    save(&window, QDir(outDir).filePath(QStringLiteral("03-chat-sang.png")));

    settings.setThemeMode(SettingsStore::ThemeMode::Dark);
    settings.setCompactChatList(true);
    Icons::clearCache();
    Theme::instance().apply();
    window.onChatSelected(-1001);
    save(&window, QDir(outDir).filePath(QStringLiteral("04-danh-sach-gon.png")));

    return 0;
}
