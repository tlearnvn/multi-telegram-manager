#pragma once

#include <QString>

/*!
 * \brief Quản lý toàn bộ đường dẫn dữ liệu của ứng dụng.
 *
 * Toàn bộ dữ liệu (cấu hình, cơ sở dữ liệu TDLib, tệp tải về, log) nằm trong
 * thư mục con "data" ngay cạnh tệp chạy, để có thể copy cả thư mục sang máy
 * khác hoặc để trong USB mà vẫn dùng được — tính cơ động cao.
 *
 * Nếu thư mục cạnh tệp chạy không cho ghi (ví dụ cài trong C:\Program Files
 * hoặc /usr/bin), ứng dụng tự chuyển sang thư mục dữ liệu người dùng của hệ
 * điều hành và báo cho người dùng biết qua isPortable().
 */
class AppPaths
{
public:
    //! Phải gọi một lần sau khi tạo QApplication.
    static void initialize();

    //! Thư mục chứa tệp chạy (với AppImage là thư mục chứa tệp .AppImage).
    static QString applicationDir();

    //! Gốc dữ liệu: <applicationDir>/data khi chạy ở chế độ portable.
    static QString dataDir();

    //! true nếu dữ liệu thực sự nằm cạnh tệp chạy.
    static bool isPortable();

    //! Lý do phải rời khỏi chế độ portable (rỗng nếu đang portable).
    static QString portableFallbackReason();

    static QString configFilePath();     //!< data/config.ini
    static QString accountsFilePath();   //!< data/accounts.json
    static QString logDir();             //!< data/logs
    static QString cacheDir();           //!< data/cache
    static QString avatarCacheDir();     //!< data/cache/avatars
    static QString downloadDir();        //!< data/downloads
    static QString accountsRootDir();    //!< data/accounts

    //! data/accounts/<slug> — nơi TDLib giữ database của một tài khoản.
    static QString accountDir(const QString &slug);
    static QString accountDatabaseDir(const QString &slug);
    static QString accountFilesDir(const QString &slug);

    //! Tạo thư mục nếu chưa có; trả về chính path để dùng lồng nhau.
    static QString ensureDir(const QString &path);

    //! Chuỗi mô tả gọn dùng cho hộp thoại "Giới thiệu".
    static QString describeStorage();

private:
    static QString s_appDir;
    static QString s_dataDir;
    static bool s_portable;
    static QString s_fallbackReason;
};
