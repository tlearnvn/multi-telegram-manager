#pragma once

#include <QByteArray>
#include <QLibrary>
#include <QScopedPointer>
#include <QString>
#include <QStringList>

/*!
 * \brief Nạp thư viện tdjson (TDLib) lúc chạy.
 *
 * Ứng dụng không liên kết tĩnh với TDLib mà nạp động tdjson, nhờ vậy:
 *   * bản build nhẹ, không cần biên dịch TDLib cùng ứng dụng;
 *   * người dùng chỉ cần đặt tdjson.dll / libtdjson.so cạnh tệp chạy là xong,
 *     rất phù hợp với bản portable;
 *   * nâng cấp TDLib chỉ là thay tệp thư viện.
 *
 * Thứ tự tìm kiếm (dừng ở tệp đầu tiên nạp được):
 *   1. đường dẫn do người dùng chỉ định trong Cài đặt;
 *   2. biến môi trường TDJSON_PATH;
 *   3. <thư mục chạy>, <thư mục chạy>/lib, /tdlib, /td;
 *   4. thư mục làm việc hiện tại;
 *   5. đường dẫn thư viện mặc định của hệ điều hành.
 */
class TdLoader
{
public:
    static TdLoader &instance();

    bool isLoaded() const { return m_loaded; }

    //! Thử nạp thư viện. Trả về true nếu thành công (idempotent).
    bool load();

    QString libraryPath() const { return m_libraryPath; }
    QString lastError() const { return m_lastError; }
    QStringList searchedPaths() const { return m_searched; }

    //! Danh sách tên tệp mong đợi trên nền tảng hiện tại — dùng cho hướng dẫn.
    static QStringList expectedFileNames();

    //! Gợi ý bằng tiếng Việt khi không tìm thấy thư viện.
    QString installationHint() const;

    // --- Bọc API C của tdjson ---------------------------------------------
    int createClientId();
    void send(int clientId, const QByteArray &requestJson);
    QByteArray receive(double timeoutSeconds);
    QByteArray execute(const QByteArray &requestJson);

    //! Đặt mức log của TDLib (0 = tắt, 5 = rất chi tiết).
    void setVerbosity(int level);

private:
    TdLoader() = default;
    Q_DISABLE_COPY_MOVE(TdLoader)

    bool tryLoad(const QString &path);
    bool resolveSymbols();

    using CreateClientIdFn = int (*)();
    using SendFn = void (*)(int, const char *);
    using ReceiveFn = const char *(*)(double);
    using ExecuteFn = const char *(*)(const char *);

    QScopedPointer<QLibrary> m_library;
    CreateClientIdFn m_createClientId = nullptr;
    SendFn m_send = nullptr;
    ReceiveFn m_receive = nullptr;
    ExecuteFn m_execute = nullptr;

    bool m_loaded = false;
    QString m_libraryPath;
    QString m_lastError;
    QStringList m_searched;
};
