/*!
 * \file tdcheck.cpp
 * \brief Kiểm tra cầu nối tdjson với thư viện TDLib thật.
 *
 * Bộ kiểm tra tự động (tuan_selftest) chạy được mà không cần TDLib: nó nạp
 * JSON mẫu trực tiếp vào lớp phân tích. Công cụ này làm phần còn lại — thử
 * đúng những chỗ chỉ vỡ khi có thư viện thật:
 *
 *   1. nạp được tdjson (đúng thứ tự tìm kiếm, đúng tên tệp);
 *   2. gọi được td_execute (yêu cầu đồng bộ, không cần client);
 *   3. td_create_client_id + td_send + td_receive đi vòng trọn vẹn, và luồng
 *      nhận phân phối phản hồi về đúng client theo "@client_id";
 *   4. TDLib nhận tham số khởi tạo rồi chuyển sang bước "cần số điện thoại".
 *
 * Không cần tài khoản thật và không gửi gì lên Telegram: api_id giả chỉ bị
 * kiểm tra khi thật sự đăng nhập, còn các bước trên diễn ra hoàn toàn cục bộ.
 *
 * Chạy:
 *   TDJSON_PATH=/duong/dan/libtdjson.so ./build/bin/tuan_tdcheck
 *
 * Trả về 0 nếu mọi phép kiểm đều đạt, 1 nếu có lỗi, 2 nếu không tìm thấy
 * thư viện (khi đó đây là thông tin chẩn đoán, không phải lỗi mã nguồn).
 */

#include "core/apppaths.h"
#include "core/jsonutil.h"
#include "core/settingsstore.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "td/tdloader.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QTemporaryDir>

#include <cstdio>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const QString &what)
{
    ++g_checks;
    std::printf("%s  %s\n", condition ? "  ĐẠT" : "* LỖI", qUtf8Printable(what));
    if (!condition)
        ++g_failures;
}

void section(const QString &title)
{
    std::printf("\n== %s\n", qUtf8Printable(title));
}

//! Chờ tới khi \a predicate đúng hoặc hết \a msTimeout, vẫn xử lý sự kiện.
template <typename Predicate>
bool waitFor(Predicate predicate, int msTimeout)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < msTimeout)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return predicate();
}

QString stateName(TdAccount::State state)
{
    return QString::fromLatin1(QMetaEnum::fromType<TdAccount::State>()
                                   .valueToKey(static_cast<int>(state)));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tuan-tdcheck"));

    // Mọi thứ ghi vào thư mục tạm rồi xoá — không đụng dữ liệu thật của người dùng.
    QTemporaryDir sandbox;
    if (!sandbox.isValid()) {
        std::fprintf(stderr, "Không tạo được thư mục tạm.\n");
        return 1;
    }
    qputenv("TUAN_MULTITELE_DATA", sandbox.path().toUtf8());
    AppPaths::initialize();

    section(QStringLiteral("Nạp thư viện tdjson"));

    TdLoader &loader = TdLoader::instance();
    if (!loader.load()) {
        std::printf("  BỎ QUA  không tìm thấy tdjson.\n\n%s\n",
                    qUtf8Printable(loader.installationHint()));
        return 2;
    }
    check(true, QStringLiteral("nạp được thư viện: %1").arg(loader.libraryPath()));
    loader.setVerbosity(0);

    // --- 2. Yêu cầu đồng bộ, không cần client -----------------------------
    section(QStringLiteral("Yêu cầu đồng bộ (td_execute)"));
    {
        QJsonObject request;
        request.insert(QStringLiteral("@type"), QStringLiteral("getTextEntities"));
        request.insert(QStringLiteral("text"), QStringLiteral("Xin chào @tuan #tele"));

        const QByteArray raw = loader.execute(QJsonDocument(request).toJson(QJsonDocument::Compact));
        const QJsonObject reply = QJsonDocument::fromJson(raw).object();

        check(Json::type(reply) == QStringLiteral("textEntities"),
              QStringLiteral("getTextEntities trả về textEntities (nhận: %1)")
                  .arg(Json::type(reply)));
        // "@tuan" và "#tele" — TDLib phải nhận ra hai thực thể.
        check(Json::array(reply, QStringLiteral("entities")).size() == 2,
              QStringLiteral("nhận ra 2 thực thể trong chuỗi tiếng Việt"));
    }

    // --- 3 & 4. Vòng đời client thật --------------------------------------
    section(QStringLiteral("Vòng đời client TDLib"));

    // api_id giả: đủ để TDLib nhận tham số và đi tới bước nhập số điện thoại.
    SettingsStore &settings = SettingsStore::instance();
    settings.setApiId(1);
    settings.setApiHash(QStringLiteral("00000000000000000000000000000000"));

    AccountManager manager;
    TdAccount *account = manager.createAccount(QStringLiteral("Kiểm tra"));
    check(account != nullptr, QStringLiteral("tạo được tài khoản thử"));
    if (!account)
        return 1;

    check(account->clientId() >= 0,
          QStringLiteral("td_create_client_id trả về mã hợp lệ (%1)").arg(account->clientId()));

    // Luồng nhận phải chuyển được update về đúng client — nếu sai, trạng thái
    // sẽ nằm mãi ở Starting.
    const bool reached = waitFor([account] {
        return account->state() == TdAccount::State::WaitPhone
            || account->state() == TdAccount::State::WaitQrScan
            || account->state() == TdAccount::State::Ready
            || account->state() == TdAccount::State::Failed;
    }, 20000);

    check(reached, QStringLiteral("TDLib phản hồi trong 20 giây (trạng thái: %1)")
                       .arg(stateName(account->state())));
    check(account->state() == TdAccount::State::WaitPhone,
          QStringLiteral("chuyển sang bước cần số điện thoại (trạng thái: %1%2)")
              .arg(stateName(account->state()),
                   account->lastError().isEmpty()
                       ? QString()
                       : QStringLiteral(", lỗi: ") + account->lastError()));

    // Trước đây setTdlibParameters bị gửi hai lần (một cho update tự đẩy, một
    // cho phản hồi getAuthorizationState) làm hiện lỗi giả ngay ở màn đăng nhập.
    check(account->lastError().isEmpty(),
          QStringLiteral("không có lỗi giả khi khởi tạo (lỗi: \"%1\")")
              .arg(account->lastError()));

    // --- 5. Đóng có trật tự ------------------------------------------------
    section(QStringLiteral("Đóng client"));
    const bool closed = manager.closeAllAndWait(8000);
    check(closed, QStringLiteral("mọi client đóng hẳn trước khi thoát"));

    std::printf("\n%d phép kiểm, %d lỗi\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "TẤT CẢ ĐẠT" : "CÓ LỖI");
    return g_failures == 0 ? 0 : 1;
}
