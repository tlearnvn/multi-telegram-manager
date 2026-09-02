#include "td/tdloader.h"

#include "core/apppaths.h"
#include "core/jsonutil.h"
#include "core/logging.h"
#include "core/settingsstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

TdLoader &TdLoader::instance()
{
    static TdLoader loader;
    return loader;
}

QStringList TdLoader::expectedFileNames()
{
#if defined(Q_OS_WIN)
    return { QStringLiteral("tdjson.dll") };
#elif defined(Q_OS_MACOS)
    return { QStringLiteral("libtdjson.dylib") };
#else
    return { QStringLiteral("libtdjson.so"),
             QStringLiteral("libtdjson.so.1.8.0") };
#endif
}

bool TdLoader::load()
{
    if (m_loaded)
        return true;

    m_searched.clear();
    m_lastError.clear();

    // 1. Đường dẫn người dùng chỉ định.
    const QString override = SettingsStore::instance().tdlibPathOverride();
    if (!override.isEmpty() && tryLoad(override))
        return true;

    // 2. Biến môi trường.
    const QString fromEnv = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("TDJSON_PATH"));
    if (!fromEnv.isEmpty() && tryLoad(fromEnv))
        return true;

    // 3. Các thư mục cạnh tệp chạy.
    QStringList dirs;
    const QString appDir = AppPaths::applicationDir();
    dirs << appDir
         << QDir(appDir).filePath(QStringLiteral("lib"))
         << QDir(appDir).filePath(QStringLiteral("tdlib"))
         << QDir(appDir).filePath(QStringLiteral("td"))
         << QDir::currentPath();

    // Với AppImage, thư viện đóng kèm nằm trong ảnh đã mount.
    const QString mounted = QCoreApplication::applicationDirPath();
    if (mounted != appDir) {
        dirs << mounted
             << QDir(mounted).filePath(QStringLiteral("lib"))
             << QDir(mounted).filePath(QStringLiteral("../lib"));
    }

    const QStringList names = expectedFileNames();
    for (const QString &dir : dirs) {
        for (const QString &name : names) {
            if (tryLoad(QDir(dir).filePath(name)))
                return true;
        }
    }

    // 4. Để hệ điều hành tự tìm (LD_LIBRARY_PATH / PATH / /usr/lib...).
    if (tryLoad(QStringLiteral("tdjson")))
        return true;

    if (m_lastError.isEmpty())
        m_lastError = QCoreApplication::translate("TdLoader", "Không tìm thấy thư viện tdjson.");
    qCWarning(logTd) << "Không nạp được tdjson:" << m_lastError;
    return false;
}

bool TdLoader::tryLoad(const QString &path)
{
    m_searched.append(QDir::toNativeSeparators(path));

    // Với đường dẫn tuyệt đối có phần mở rộng, bỏ qua nếu tệp không tồn tại —
    // đỡ tốn thời gian và cho thông báo lỗi rõ ràng hơn.
    const QFileInfo info(path);
    if (info.isAbsolute() && !info.exists())
        return false;

    auto *candidate = new QLibrary(path);
    candidate->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!candidate->load()) {
        m_lastError = candidate->errorString();
        delete candidate;
        return false;
    }

    m_library.reset(candidate);
    if (!resolveSymbols()) {
        m_library->unload();
        m_library.reset();
        return false;
    }

    m_loaded = true;
    m_libraryPath = QDir::toNativeSeparators(m_library->fileName());
    qCInfo(logTd) << "Đã nạp tdjson từ" << m_libraryPath;

    setVerbosity(SettingsStore::instance().tdlibVerbosity());
    return true;
}

bool TdLoader::resolveSymbols()
{
    m_createClientId = reinterpret_cast<CreateClientIdFn>(m_library->resolve("td_create_client_id"));
    m_send = reinterpret_cast<SendFn>(m_library->resolve("td_send"));
    m_receive = reinterpret_cast<ReceiveFn>(m_library->resolve("td_receive"));
    m_execute = reinterpret_cast<ExecuteFn>(m_library->resolve("td_execute"));

    if (!m_createClientId || !m_send || !m_receive || !m_execute) {
        m_lastError = QCoreApplication::translate("TdLoader",
            "Tệp %1 không phải tdjson hợp lệ (thiếu hàm td_send/td_receive).")
            .arg(QDir::toNativeSeparators(m_library->fileName()));
        m_createClientId = nullptr;
        m_send = nullptr;
        m_receive = nullptr;
        m_execute = nullptr;
        return false;
    }
    return true;
}

int TdLoader::createClientId()
{
    if (!m_loaded || !m_createClientId)
        return -1;
    return m_createClientId();
}

void TdLoader::send(int clientId, const QByteArray &requestJson)
{
    if (!m_loaded || !m_send || clientId < 0)
        return;
    m_send(clientId, requestJson.constData());
}

QByteArray TdLoader::receive(double timeoutSeconds)
{
    if (!m_loaded || !m_receive)
        return QByteArray();
    const char *result = m_receive(timeoutSeconds);
    if (!result)
        return QByteArray();
    // Chuỗi trả về chỉ hợp lệ tới lần gọi td_receive kế tiếp trong cùng luồng,
    // nên phải copy ngay.
    return QByteArray(result);
}

QByteArray TdLoader::execute(const QByteArray &requestJson)
{
    if (!m_loaded || !m_execute)
        return QByteArray();
    const char *result = m_execute(requestJson.constData());
    return result ? QByteArray(result) : QByteArray();
}

void TdLoader::setVerbosity(int level)
{
    if (!m_loaded)
        return;
    QJsonObject request = Json::request(QStringLiteral("setLogVerbosityLevel"));
    request.insert(QStringLiteral("new_verbosity_level"), qBound(0, level, 5));
    execute(Json::compact(request));
}

QString TdLoader::installationHint() const
{
#if defined(Q_OS_WIN)
    const QString fileName = QStringLiteral("tdjson.dll");
#elif defined(Q_OS_MACOS)
    const QString fileName = QStringLiteral("libtdjson.dylib");
#else
    const QString fileName = QStringLiteral("libtdjson.so");
#endif

    return QCoreApplication::translate("TdLoader",
        "Ứng dụng cần thư viện TDLib (%1) để kết nối Telegram.\n\n"
        "Cách khắc phục:\n"
        "  1. Tải TDLib bản dựng sẵn cho hệ điều hành của bạn, hoặc tự biên dịch "
        "từ https://github.com/tdlib/td\n"
        "  2. Đặt tệp %1 vào thư mục chứa tệp chạy, hoặc vào thư mục con \"lib\".\n"
        "  3. Mở lại ứng dụng (hoặc bấm \"Nạp lại TDLib\" trong Cài đặt).\n\n"
        "Bạn cũng có thể trỏ thẳng tới tệp thư viện trong Cài đặt → Nâng cao.\n\n"
        "Thư mục đã tìm:\n%2")
        .arg(fileName, searchedPaths().join(QLatin1Char('\n')));
}
