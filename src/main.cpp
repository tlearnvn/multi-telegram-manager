#include "core/apppaths.h"
#include "core/logging.h"
#include "core/settingsstore.h"
#include "model/messageentry.h"
#include "td/accountmanager.h"
#include "td/tdenums.h"
#include "td/tdloader.h"
#include "td/tdtransport.h"
#include "ui/iconfactory.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include "version.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLocale>
#include <QMetaType>
#include <QTimer>

#include <cstdio>

/*!
 * Điểm khởi động của Tuấn' MultiTele Client.
 *
 * Thứ tự khởi tạo quan trọng:
 *   1. QApplication (để biết đường dẫn tệp chạy),
 *   2. AppPaths (chốt nơi lưu dữ liệu — cạnh tệp chạy nếu ghi được),
 *   3. Logging (ghi vào data/logs),
 *   4. Theme (đọc cấu hình từ data/config.ini),
 *   5. AccountManager (mở lại các tài khoản đã lưu),
 *   6. MainWindow.
 */
int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral(APP_EXECUTABLE_NAME));
    QApplication::setApplicationDisplayName(QStringLiteral(APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION_FULL));
    QApplication::setOrganizationName(QStringLiteral(APP_ORGANIZATION));
    QApplication::setDesktopFileName(QStringLiteral("tuan-multitele-client"));

    // Qt 6 luôn bật High-DPI, không cần cờ riêng như Qt 5.

    QApplication app(argc, argv);
    app.setWindowIcon(Icons::appIcon());

    // Giao diện mặc định bằng tiếng Việt.
    QLocale::setDefault(QLocale(QLocale::Vietnamese, QLocale::Vietnam));

    qRegisterMetaType<MessageEntry>("MessageEntry");
    qRegisterMetaType<QList<MessageEntry>>("QList<MessageEntry>");
    qRegisterMetaType<TdConnectionState>("TdConnectionState");
    qRegisterMetaType<ChatFilterKind>("ChatFilterKind");

    // --- Tham số dòng lệnh -------------------------------------------------
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("%1 — trình khách Telegram đa tài khoản.\n"
                       "Mọi dữ liệu được lưu trong thư mục \"data\" cạnh tệp chạy.")
            .arg(QStringLiteral(APP_NAME)));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption minimizedOption(
        QStringList{ QStringLiteral("m"), QStringLiteral("minimized") },
        QStringLiteral("Mở ở trạng thái thu nhỏ vào khay hệ thống."));
    parser.addOption(minimizedOption);

    QCommandLineOption dataDirOption(
        QStringList{ QStringLiteral("d"), QStringLiteral("data-dir") },
        QStringLiteral("Dùng thư mục dữ liệu khác (thay cho ./data)."),
        QStringLiteral("đường-dẫn"));
    parser.addOption(dataDirOption);

    QCommandLineOption tdlibOption(
        QStringList{ QStringLiteral("t"), QStringLiteral("tdlib") },
        QStringLiteral("Đường dẫn tới thư viện tdjson."),
        QStringLiteral("tệp"));
    parser.addOption(tdlibOption);

    parser.process(app);

    if (parser.isSet(dataDirOption)) {
        qputenv("TUAN_MULTITELE_DATA",
                parser.value(dataDirOption).toLocal8Bit());
    }

    // --- Khởi tạo ----------------------------------------------------------
    AppPaths::initialize();
    Logging::install();

    qInfo("%s %s bắt đầu chạy", APP_NAME, APP_VERSION_FULL);
    qInfo("Dữ liệu: %s (%s)", qPrintable(AppPaths::dataDir()),
          AppPaths::isPortable() ? "cơ động" : "thư mục người dùng");

    if (parser.isSet(tdlibOption))
        SettingsStore::instance().setTdlibPathOverride(parser.value(tdlibOption));

    if (parser.isSet(minimizedOption))
        SettingsStore::instance().setStartMinimized(true);

    Theme::instance().apply();

    // Nạp TDLib sớm để giao diện biết ngay còn thiếu gì; nếu chưa có thì trình
    // hướng dẫn trong MainWindow sẽ chỉ cách bổ sung.
    if (TdLoader::instance().load())
        TdTransport::instance().start();

    AccountManager manager;
    manager.loadFromDisk();

    MainWindow window(&manager);
    window.startUp();

    const int code = app.exec();

    // Thứ tự thoát quan trọng: báo offline → đóng client và CHỜ TDLib ghi xong
    // cơ sở dữ liệu → mới dừng luồng nhận, vì chính luồng đó mang xác nhận về.
    manager.setOnlineAll(false);
    manager.saveToDisk();
    SettingsStore::instance().flush();
    manager.closeAllAndWait();
    TdTransport::instance().stop();

    qInfo("Đã thoát an toàn");
    Logging::shutdown();
    return code;
}
