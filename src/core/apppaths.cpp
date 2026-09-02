#include "core/apppaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryFile>

QString AppPaths::s_appDir;
QString AppPaths::s_dataDir;
bool AppPaths::s_portable = true;
QString AppPaths::s_fallbackReason;

namespace {

bool directoryIsWritable(const QString &dir)
{
    QDir d(dir);
    if (!d.exists() && !d.mkpath(QStringLiteral(".")))
        return false;

    QTemporaryFile probe(d.filePath(QStringLiteral("ghi-thu-XXXXXX.tmp")));
    probe.setAutoRemove(true);
    return probe.open();
}

} // namespace

void AppPaths::initialize()
{
    // Với AppImage, applicationDirPath() trỏ vào thư mục tạm đã mount nên phải
    // dùng biến môi trường APPIMAGE để lấy vị trí thật của tệp .AppImage.
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImage = env.value(QStringLiteral("APPIMAGE"));
    if (!appImage.isEmpty() && QFileInfo::exists(appImage))
        s_appDir = QFileInfo(appImage).absolutePath();
    else
        s_appDir = QCoreApplication::applicationDirPath();

    const QString portableData = QDir(s_appDir).filePath(QStringLiteral("data"));

    // Cho phép ép đường dẫn dữ liệu qua biến môi trường (tiện khi thử nghiệm).
    const QString override = env.value(QStringLiteral("TUAN_MULTITELE_DATA"));
    if (!override.isEmpty()) {
        s_dataDir = QDir(override).absolutePath();
        s_portable = false;
        s_fallbackReason = QObject::tr("Đường dẫn dữ liệu bị ghi đè bởi biến môi trường TUAN_MULTITELE_DATA.");
    } else if (directoryIsWritable(portableData)) {
        s_dataDir = portableData;
        s_portable = true;
        s_fallbackReason.clear();
    } else {
        s_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        s_portable = false;
        s_fallbackReason = QObject::tr(
            "Không thể ghi vào thư mục cạnh tệp chạy (%1) nên dữ liệu được lưu ở "
            "thư mục người dùng. Hãy chuyển ứng dụng sang thư mục có quyền ghi "
            "(ví dụ Desktop hoặc USB) để dùng chế độ cơ động.").arg(QDir::toNativeSeparators(s_appDir));
    }

    ensureDir(s_dataDir);
    ensureDir(logDir());
    ensureDir(cacheDir());
    ensureDir(avatarCacheDir());
    ensureDir(downloadDir());
    ensureDir(accountsRootDir());
}

QString AppPaths::applicationDir()
{
    return s_appDir;
}

QString AppPaths::dataDir()
{
    return s_dataDir;
}

bool AppPaths::isPortable()
{
    return s_portable;
}

QString AppPaths::portableFallbackReason()
{
    return s_fallbackReason;
}

QString AppPaths::configFilePath()
{
    return QDir(s_dataDir).filePath(QStringLiteral("config.ini"));
}

QString AppPaths::accountsFilePath()
{
    return QDir(s_dataDir).filePath(QStringLiteral("accounts.json"));
}

QString AppPaths::logDir()
{
    return QDir(s_dataDir).filePath(QStringLiteral("logs"));
}

QString AppPaths::cacheDir()
{
    return QDir(s_dataDir).filePath(QStringLiteral("cache"));
}

QString AppPaths::avatarCacheDir()
{
    return QDir(cacheDir()).filePath(QStringLiteral("avatars"));
}

QString AppPaths::downloadDir()
{
    return QDir(s_dataDir).filePath(QStringLiteral("downloads"));
}

QString AppPaths::accountsRootDir()
{
    return QDir(s_dataDir).filePath(QStringLiteral("accounts"));
}

QString AppPaths::accountDir(const QString &slug)
{
    return QDir(accountsRootDir()).filePath(slug);
}

QString AppPaths::accountDatabaseDir(const QString &slug)
{
    return QDir(accountDir(slug)).filePath(QStringLiteral("td"));
}

QString AppPaths::accountFilesDir(const QString &slug)
{
    return QDir(accountDir(slug)).filePath(QStringLiteral("files"));
}

QString AppPaths::ensureDir(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

QString AppPaths::describeStorage()
{
    if (s_portable) {
        return QObject::tr("Chế độ cơ động — mọi dữ liệu nằm trong %1")
            .arg(QDir::toNativeSeparators(s_dataDir));
    }
    return QObject::tr("Dữ liệu lưu tại %1\n%2")
        .arg(QDir::toNativeSeparators(s_dataDir), s_fallbackReason);
}
