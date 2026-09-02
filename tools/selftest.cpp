/*!
 * \file selftest.cpp
 * \brief Bộ kiểm tra tự động, không cần TDLib hay tài khoản thật.
 *
 * Kiểm ba nhóm dễ hỏng ngầm mà giao diện không phát hiện ngay:
 *   1. bộ tạo mã QR (so với ma trận chuẩn đã đối chiếu với thư viện tham chiếu),
 *   2. các hàm định dạng và tiện ích JSON,
 *   3. lớp phân tích dữ liệu TDLib — nạp JSON đúng như máy chủ gửi rồi soi kết
 *      quả trong TdAccount.
 *
 * Chạy: cmake -S . -B build -DBUILD_TESTS=ON && ./build/bin/tuan_selftest
 * Trả về 0 nếu mọi phép kiểm đều đạt.
 */

#include "core/apppaths.h"
#include "core/formatting.h"
#include "core/jsonutil.h"
#include "core/qrcode.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const QString &what)
{
    ++g_checks;
    if (condition)
        return;
    ++g_failures;
    std::printf("  ✗ %s\n", qPrintable(what));
}

void checkEqual(const QString &actual, const QString &expected, const QString &what)
{
    ++g_checks;
    if (actual == expected)
        return;
    ++g_failures;
    std::printf("  ✗ %s\n      nhận: %s\n      mong: %s\n",
                qPrintable(what), qPrintable(actual), qPrintable(expected));
}

void section(const char *name)
{
    std::printf("\n== %s\n", name);
}

// Ma trận mã QR chuẩn cho "HELLO WORLD", mức sửa lỗi L, mặt nạ 0.
// Đã đối chiếu khớp từng ô với thư viện python-qrcode.
const char *kGoldenHelloWorldL0[] = {
    "111111100101101111111",
    "100000100111001000001",
    "101110101101101011101",
    "101110100101001011101",
    "101110100010101011101",
    "100000100000101000001",
    "111111101010101111111",
    "000000001101100000000",
    "111011111111011000100",
    "100110000010001100010",
    "011111101010110111111",
    "111000010110000010010",
    "110110111010111110100",
    "000000001001010000110",
    "111111101011000110111",
    "100000101001100100001",
    "101110101001001010100",
    "101110100101001110110",
    "101110101000101010101",
    "100000101001000010010",
    "111111101001101100111"
};

QJsonObject json(const char *text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

} // namespace

int main(int argc, char *argv[])
{
    QTemporaryDir tempData;
    qputenv("TUAN_MULTITELE_DATA", tempData.path().toLocal8Bit());

    QCoreApplication app(argc, argv);
    AppPaths::initialize();

    std::printf("Kiểm tra Tuấn' MultiTele Client\n");

    // -----------------------------------------------------------------------
    section("Bộ tạo mã QR");

    {
        const QrCode code = QrCode::encode(QByteArray("HELLO WORLD"), QrCode::Ecc::Low, 0);
        check(code.isValid(), QStringLiteral("mã hợp lệ"));
        check(code.version() == 1, QStringLiteral("chọn đúng phiên bản 1"));
        check(code.size() == 21, QStringLiteral("kích cỡ 21x21"));

        bool identical = true;
        for (int y = 0; y < code.size() && identical; ++y) {
            const QString expected = QString::fromLatin1(kGoldenHelloWorldL0[y]);
            for (int x = 0; x < code.size(); ++x) {
                const bool want = expected.at(x) == QLatin1Char('1');
                if (code.module(x, y) != want) {
                    identical = false;
                    std::printf("      lệch tại ô (%d,%d)\n", x, y);
                    break;
                }
            }
        }
        check(identical, QStringLiteral("ma trận khớp bản chuẩn"));
    }

    {
        // Liên kết đăng nhập thật dài cỡ này; phải vừa một phiên bản hợp lý.
        const QByteArray link =
            "tg://login?token=AQAAAAB1234567890abcdefGHIJKLMNOPqrstuvwxyz-_";
        const QrCode code = QrCode::encode(link, QrCode::Ecc::Medium);
        check(code.isValid(), QStringLiteral("mã hoá được liên kết đăng nhập"));
        check(code.version() == 4, QStringLiteral("liên kết đăng nhập vừa phiên bản 4"));

        // Ba hoa văn định vị phải có ở ba góc.
        check(code.module(0, 0) && code.module(6, 0) && code.module(0, 6),
              QStringLiteral("hoa văn định vị góc trên trái"));
        check(code.module(code.size() - 1, 0) && code.module(code.size() - 7, 0),
              QStringLiteral("hoa văn định vị góc trên phải"));
        check(code.module(0, code.size() - 1) && code.module(0, code.size() - 7),
              QStringLiteral("hoa văn định vị góc dưới trái"));
        // Ô tối cố định cạnh khối thông tin định dạng.
        check(code.module(8, code.size() - 8), QStringLiteral("ô tối cố định"));
    }

    {
        // Dữ liệu quá dài cho phiên bản tối đa thì phải trả về mã rỗng, không
        // được sinh mã sai.
        const QrCode code = QrCode::encode(QByteArray(4000, 'x'), QrCode::Ecc::Medium);
        check(!code.isValid(), QStringLiteral("từ chối dữ liệu quá dài"));
    }

    // -----------------------------------------------------------------------
    section("Định dạng chuỗi");

    checkEqual(Format::fileSize(0), QStringLiteral("0 B"), QStringLiteral("0 byte"));
    checkEqual(Format::fileSize(1023), QStringLiteral("1023 B"), QStringLiteral("dưới 1 KB"));
    checkEqual(Format::fileSize(1024), QStringLiteral("1,0 KB"), QStringLiteral("đúng 1 KB"));
    checkEqual(Format::fileSize(2483712), QStringLiteral("2,4 MB"), QStringLiteral("2,4 MB"));
    checkEqual(Format::duration(0), QStringLiteral("00:00"), QStringLiteral("0 giây"));
    checkEqual(Format::duration(83), QStringLiteral("01:23"), QStringLiteral("1 phút 23"));
    checkEqual(Format::duration(3723), QStringLiteral("1:02:03"), QStringLiteral("hơn 1 giờ"));

    checkEqual(Format::normalizePhone(QStringLiteral("+84 (912) 345-678")),
               QStringLiteral("+84912345678"), QStringLiteral("chuẩn hoá số điện thoại"));
    checkEqual(Format::maskPhone(QStringLiteral("+84912345678")),
               QStringLiteral("+849******78"), QStringLiteral("che số điện thoại"));
    checkEqual(Format::oneLine(QStringLiteral("dòng một\ndòng   hai"), 100),
               QStringLiteral("dòng một dòng hai"), QStringLiteral("gộp về một dòng"));
    check(Format::oneLine(QString(200, QLatin1Char('a')), 20).size() == 20,
          QStringLiteral("cắt đúng độ dài"));

    // -----------------------------------------------------------------------
    section("Tiện ích JSON");

    {
        const QJsonObject object = json(R"({
            "num": 42, "str": "xin chào", "big": "9007199254740993",
            "flag": true, "nested": { "deep": { "value": "đáy" } },
            "list": [1, 2, "3"]
        })");

        check(Json::integer(object, QStringLiteral("num")) == 42,
              QStringLiteral("đọc số nguyên"));
        checkEqual(Json::str(object, QStringLiteral("str")), QStringLiteral("xin chào"),
                   QStringLiteral("đọc chuỗi"));
        // TDLib gửi int64 dưới dạng chuỗi — phải đọc được cả hai kiểu.
        check(Json::int64(object, QStringLiteral("big")) == Q_INT64_C(9007199254740993),
              QStringLiteral("đọc int64 dạng chuỗi"));
        check(Json::int64(object, QStringLiteral("num")) == 42,
              QStringLiteral("đọc int64 dạng số"));
        check(Json::boolean(object, QStringLiteral("flag")), QStringLiteral("đọc bool"));
        check(Json::integer(object, QStringLiteral("thieu"), 7) == 7,
              QStringLiteral("giá trị mặc định khi thiếu khoá"));
        checkEqual(Json::strAt(object, { QStringLiteral("nested"), QStringLiteral("deep"),
                                         QStringLiteral("value") }),
                   QStringLiteral("đáy"), QStringLiteral("đi sâu nhiều tầng"));
        const QList<qint64> list = Json::toInt64List(Json::array(object, QStringLiteral("list")));
        check(list.size() == 3 && list.at(2) == 3,
              QStringLiteral("mảng số lẫn chuỗi"));
    }

    // -----------------------------------------------------------------------
    section("Phân tích dữ liệu TDLib");

    {
        TdAccount account(QStringLiteral("kiemtra"));

        account.handleIncoming(json(R"({
            "@type": "updateUser",
            "user": { "@type": "user", "id": 111, "first_name": "Nguyễn",
                      "last_name": "Minh Hà", "username": "minhha",
                      "status": { "@type": "userStatusOnline" },
                      "type": { "@type": "userTypeRegular" } }
        })"));

        const UserEntry *user = account.user(111);
        check(user != nullptr, QStringLiteral("lưu được người dùng"));
        if (user) {
            checkEqual(user->displayName(), QStringLiteral("Nguyễn Minh Hà"),
                       QStringLiteral("tên hiển thị"));
            checkEqual(user->handle(), QStringLiteral("@minhha"),
                       QStringLiteral("tên tài khoản"));
            check(user->presence == UserEntry::Presence::Online,
                  QStringLiteral("trạng thái trực tuyến"));
        }

        // Dạng "usernames" mới của TDLib cũng phải đọc được.
        account.handleIncoming(json(R"({
            "@type": "updateUser",
            "user": { "@type": "user", "id": 112, "first_name": "Bảo",
                      "usernames": { "@type": "usernames",
                                     "active_usernames": ["quocbao"],
                                     "editable_username": "quocbao" },
                      "type": { "@type": "userTypeRegular" } }
        })"));
        const UserEntry *newer = account.user(112);
        check(newer && newer->username == QStringLiteral("quocbao"),
              QStringLiteral("đọc usernames kiểu mới"));

        account.handleIncoming(json(R"({
            "@type": "updateNewChat",
            "chat": { "@type": "chat", "id": -1001,
                      "type": { "@type": "chatTypeSupergroup", "supergroup_id": 55,
                                "is_channel": false },
                      "title": "Nhóm thử",
                      "unread_count": 4,
                      "notification_settings": { "mute_for": 600 },
                      "positions": [ { "@type": "chatPosition",
                                       "list": { "@type": "chatListMain" },
                                       "order": "9007199254740000",
                                       "is_pinned": true } ] }
        })"));

        const ChatEntry *chat = account.chat(-1001);
        check(chat != nullptr, QStringLiteral("lưu được cuộc trò chuyện"));
        if (chat) {
            checkEqual(chat->title, QStringLiteral("Nhóm thử"), QStringLiteral("tiêu đề"));
            check(chat->kind == ChatEntry::Kind::Supergroup, QStringLiteral("loại nhóm lớn"));
            check(chat->isPinned, QStringLiteral("chat được ghim"));
            check(chat->isMuted, QStringLiteral("chat bị tắt tiếng"));
            check(chat->unreadCount == 4, QStringLiteral("số tin chưa đọc"));
            check(chat->order == Q_INT64_C(9007199254740000),
                  QStringLiteral("thứ tự int64 không bị tràn"));
        }

        // Vị trí "order = 0" ở danh sách chính không được xoá thứ tự lưu trữ.
        account.handleIncoming(json(R"({
            "@type": "updateChatPosition", "chat_id": -1001,
            "position": { "@type": "chatPosition",
                          "list": { "@type": "chatListArchive" },
                          "order": "500", "is_pinned": false }
        })"));
        account.handleIncoming(json(R"({
            "@type": "updateChatPosition", "chat_id": -1001,
            "position": { "@type": "chatPosition",
                          "list": { "@type": "chatListMain" },
                          "order": "0", "is_pinned": false }
        })"));
        chat = account.chat(-1001);
        check(chat && chat->inArchive && chat->order == 500,
              QStringLiteral("chuyển vào lưu trữ vẫn giữ thứ tự"));

        // Tin nhắn có định dạng và trả lời.
        account.handleIncoming(json(R"({
            "@type": "updateNewMessage",
            "message": { "@type": "message", "id": 900, "chat_id": -1001,
                         "sender_id": { "@type": "messageSenderUser", "user_id": 111 },
                         "date": 1700000000, "is_outgoing": false,
                         "content": { "@type": "messageText",
                                      "text": { "@type": "formattedText",
                                                "text": "chào bạn nhé",
                                                "entities": [ { "@type": "textEntity",
                                                                "offset": 0, "length": 4,
                                                                "type": { "@type": "textEntityTypeBold" } } ] } } }
        })"));

        const MessageEntry *message = account.cachedMessage(-1001, 900);
        check(message != nullptr, QStringLiteral("lưu được tin nhắn"));
        if (message) {
            checkEqual(message->text, QStringLiteral("chào bạn nhé"),
                       QStringLiteral("nội dung tin nhắn"));
            check(message->kind == MessageEntry::Kind::Text, QStringLiteral("loại văn bản"));
            check(message->spans.size() == 1
                      && message->spans.first().style == TextSpan::Style::Bold,
                  QStringLiteral("đoạn định dạng đậm"));
            checkEqual(message->senderName, QStringLiteral("Nguyễn Minh Hà"),
                       QStringLiteral("tên người gửi"));
        }

        // Tin nhắn có tệp: phải lấy đúng tên, cỡ và mã tệp.
        account.handleIncoming(json(R"({
            "@type": "updateNewMessage",
            "message": { "@type": "message", "id": 901, "chat_id": -1001,
                         "sender_id": { "@type": "messageSenderUser", "user_id": 112 },
                         "date": 1700000100, "is_outgoing": false,
                         "reply_to_message_id": 900,
                         "content": { "@type": "messageDocument",
                                      "document": { "@type": "document",
                                                    "file_name": "bao-gia.pdf",
                                                    "mime_type": "application/pdf",
                                                    "document": { "@type": "file", "id": 77,
                                                                  "size": 2483712,
                                                                  "local": { "@type": "localFile",
                                                                             "is_downloading_completed": false } } },
                                      "caption": { "@type": "formattedText",
                                                   "text": "gửi bạn" } } }
        })"));

        const MessageEntry *doc = account.cachedMessage(-1001, 901);
        check(doc != nullptr, QStringLiteral("lưu được tin nhắn tệp"));
        if (doc) {
            check(doc->kind == MessageEntry::Kind::Document, QStringLiteral("loại tệp"));
            checkEqual(doc->fileName, QStringLiteral("bao-gia.pdf"), QStringLiteral("tên tệp"));
            check(doc->fileSize == 2483712, QStringLiteral("cỡ tệp"));
            check(doc->mediaFileId == 77, QStringLiteral("mã tệp để tải"));
            checkEqual(doc->text, QStringLiteral("gửi bạn"), QStringLiteral("chú thích"));
            check(doc->replyToMessageId == 900, QStringLiteral("tin được trả lời"));
            checkEqual(doc->replyPreviewText, QStringLiteral("chào bạn nhé"),
                       QStringLiteral("xem trước tin được trả lời"));
        }

        // Tin nhắn hệ thống.
        account.handleIncoming(json(R"({
            "@type": "updateNewMessage",
            "message": { "@type": "message", "id": 902, "chat_id": -1001,
                         "sender_id": { "@type": "messageSenderUser", "user_id": 111 },
                         "date": 1700000200, "is_outgoing": false,
                         "content": { "@type": "messageChatChangeTitle",
                                      "title": "Tên mới" } }
        })"));
        const MessageEntry *service = account.cachedMessage(-1001, 902);
        check(service && service->kind == MessageEntry::Kind::Service,
              QStringLiteral("nhận ra tin nhắn hệ thống"));

        check(account.cachedMessages(-1001).size() == 3,
              QStringLiteral("lấy lại được các tin đã lưu"));

        // Xoá tin nhắn.
        account.handleIncoming(json(R"({
            "@type": "updateDeleteMessages", "chat_id": -1001,
            "message_ids": [902], "is_permanent": true
        })"));
        check(account.cachedMessage(-1001, 902) == nullptr,
              QStringLiteral("xoá tin nhắn khỏi bộ nhớ"));

        // Nội dung chưa hỗ trợ không được làm sập chương trình.
        account.handleIncoming(json(R"({
            "@type": "updateNewMessage",
            "message": { "@type": "message", "id": 903, "chat_id": -1001,
                         "date": 1700000300,
                         "content": { "@type": "messageMotIeuGiLa" } }
        })"));
        const MessageEntry *unknown = account.cachedMessage(-1001, 903);
        check(unknown && unknown->kind == MessageEntry::Kind::Unsupported,
              QStringLiteral("nội dung lạ được đánh dấu chưa hỗ trợ"));

        // JSON rỗng hoặc sai kiểu cũng phải yên.
        account.handleIncoming(QJsonObject());
        account.handleIncoming(json(R"({ "@type": "updateNewChat" })"));
        account.handleIncoming(json(R"({ "@type": "error", "code": 400,
                                         "message": "Có lỗi" })"));
        check(true, QStringLiteral("dữ liệu thiếu/sai không gây sập"));
    }

    // -----------------------------------------------------------------------
    section("Trình tự thoát an toàn");

    {
        // Không có TDLib thì client chưa từng mở, closeAllAndWait phải trả về
        // ngay chứ không treo đủ thời gian chờ.
        AccountManager manager;
        QElapsedTimer timer;
        timer.start();
        const bool closed = manager.closeAllAndWait(2000);
        check(closed, QStringLiteral("đóng xong khi không có client nào"));
        check(timer.elapsed() < 1000, QStringLiteral("không chờ vô ích"));
    }

    // -----------------------------------------------------------------------
    section("Đường dẫn cơ động");

    check(AppPaths::dataDir().startsWith(tempData.path()),
          QStringLiteral("tôn trọng biến môi trường TUAN_MULTITELE_DATA"));
    check(AppPaths::accountDatabaseDir(QStringLiteral("tk01"))
              .contains(QStringLiteral("accounts")),
          QStringLiteral("thư mục riêng cho từng tài khoản"));

    // -----------------------------------------------------------------------
    std::printf("\n%d phép kiểm, %d lỗi\n", g_checks, g_failures);
    if (g_failures == 0)
        std::printf("TẤT CẢ ĐẠT\n");
    return g_failures == 0 ? 0 : 1;
}
