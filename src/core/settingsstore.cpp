#include "core/settingsstore.h"

#include "core/apppaths.h"

namespace {

// Khoá cấu hình gom lại một chỗ để tránh gõ sai chuỗi rải rác trong mã.
namespace Key {
const QString ApiId              = QStringLiteral("telegram/api_id");
const QString ApiHash            = QStringLiteral("telegram/api_hash");

const QString Theme              = QStringLiteral("ui/theme");
const QString Accent             = QStringLiteral("ui/accent");
const QString FontScale          = QStringLiteral("ui/font_scale");
const QString CompactList        = QStringLiteral("ui/compact_chat_list");
const QString GroupAvatars       = QStringLiteral("ui/group_avatars");

const QString Notifications      = QStringLiteral("notify/enabled");
const QString NotifyPreview      = QStringLiteral("notify/preview");
const QString Tray               = QStringLiteral("notify/tray");
const QString CloseToTray        = QStringLiteral("notify/close_to_tray");
const QString StartMinimized     = QStringLiteral("notify/start_minimized");

const QString AutoPhotos         = QStringLiteral("download/auto_photos");
const QString AutoMaxMb          = QStringLiteral("download/auto_max_mb");

const QString ProxyKind          = QStringLiteral("proxy/kind");
const QString ProxyServer        = QStringLiteral("proxy/server");
const QString ProxyPort          = QStringLiteral("proxy/port");
const QString ProxyUser          = QStringLiteral("proxy/username");
const QString ProxyPassword      = QStringLiteral("proxy/password");
const QString ProxySecret        = QStringLiteral("proxy/secret");

const QString TdlibPath          = QStringLiteral("tdlib/path");
const QString TdlibVerbosity     = QStringLiteral("tdlib/verbosity");

const QString WindowGeometry     = QStringLiteral("session/window_geometry");
const QString SplitterState      = QStringLiteral("session/splitter_state");
const QString LastAccount        = QStringLiteral("session/last_account");
const QString SetupDone          = QStringLiteral("session/setup_completed");
} // namespace Key

// Mã hoá nhẹ mật khẩu proxy để không nằm dạng thuần trong config.ini.
// Đây không phải bảo mật thật — chỉ tránh lộ khi nhìn qua vai.
QString obfuscate(const QString &plain)
{
    if (plain.isEmpty())
        return QString();
    QByteArray data = plain.toUtf8();
    for (int i = 0; i < data.size(); ++i)
        data[i] = static_cast<char>(data.at(i) ^ static_cast<char>(0x5A + (i % 7)));
    return QString::fromLatin1(data.toBase64());
}

QString deobfuscate(const QString &stored)
{
    if (stored.isEmpty())
        return QString();
    QByteArray data = QByteArray::fromBase64(stored.toLatin1());
    for (int i = 0; i < data.size(); ++i)
        data[i] = static_cast<char>(data.at(i) ^ static_cast<char>(0x5A + (i % 7)));
    return QString::fromUtf8(data);
}

} // namespace

SettingsStore &SettingsStore::instance()
{
    static SettingsStore store;
    return store;
}

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(AppPaths::configFilePath(), QSettings::IniFormat))
{
    // Qt 6 luôn đọc/ghi tệp INI bằng UTF-8, nên tiếng Việt trong cấu hình an toàn.
}

QVariant SettingsStore::read(const QString &key, const QVariant &fallback) const
{
    return m_settings->value(key, fallback);
}

void SettingsStore::write(const QString &key, const QVariant &value)
{
    if (m_settings->value(key) == value)
        return;
    m_settings->setValue(key, value);
    emit changed(key);
}

// --- API -------------------------------------------------------------------

int SettingsStore::apiId() const
{
    return read(Key::ApiId, 0).toInt();
}

void SettingsStore::setApiId(int id)
{
    write(Key::ApiId, id);
}

QString SettingsStore::apiHash() const
{
    return read(Key::ApiHash, QString()).toString();
}

void SettingsStore::setApiHash(const QString &hash)
{
    write(Key::ApiHash, hash.trimmed());
}

bool SettingsStore::hasApiCredentials() const
{
    return apiId() > 0 && apiHash().size() >= 16;
}

// --- Giao diện -------------------------------------------------------------

SettingsStore::ThemeMode SettingsStore::themeMode() const
{
    const QString value = read(Key::Theme, QStringLiteral("Dark")).toString();
    if (value.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0)
        return ThemeMode::Light;
    if (value.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0)
        return ThemeMode::System;
    return ThemeMode::Dark;
}

void SettingsStore::setThemeMode(ThemeMode mode)
{
    const char *name = "Dark";
    switch (mode) {
    case ThemeMode::Light:  name = "Light";  break;
    case ThemeMode::System: name = "System"; break;
    case ThemeMode::Dark:   name = "Dark";   break;
    }
    write(Key::Theme, QString::fromLatin1(name));
}

QString SettingsStore::accentColor() const
{
    return read(Key::Accent, QStringLiteral("#2ea6ff")).toString();
}

void SettingsStore::setAccentColor(const QString &hex)
{
    write(Key::Accent, hex);
}

int SettingsStore::fontScalePercent() const
{
    return qBound(80, read(Key::FontScale, 100).toInt(), 150);
}

void SettingsStore::setFontScalePercent(int percent)
{
    write(Key::FontScale, qBound(80, percent, 150));
}

bool SettingsStore::compactChatList() const
{
    return read(Key::CompactList, false).toBool();
}

void SettingsStore::setCompactChatList(bool compact)
{
    write(Key::CompactList, compact);
}

bool SettingsStore::showAvatarsInGroups() const
{
    return read(Key::GroupAvatars, true).toBool();
}

void SettingsStore::setShowAvatarsInGroups(bool show)
{
    write(Key::GroupAvatars, show);
}

// --- Thông báo -------------------------------------------------------------

bool SettingsStore::notificationsEnabled() const
{
    return read(Key::Notifications, true).toBool();
}

void SettingsStore::setNotificationsEnabled(bool enabled)
{
    write(Key::Notifications, enabled);
}

bool SettingsStore::notificationPreview() const
{
    return read(Key::NotifyPreview, true).toBool();
}

void SettingsStore::setNotificationPreview(bool enabled)
{
    write(Key::NotifyPreview, enabled);
}

bool SettingsStore::trayEnabled() const
{
    return read(Key::Tray, true).toBool();
}

void SettingsStore::setTrayEnabled(bool enabled)
{
    write(Key::Tray, enabled);
}

bool SettingsStore::closeToTray() const
{
    return read(Key::CloseToTray, true).toBool();
}

void SettingsStore::setCloseToTray(bool enabled)
{
    write(Key::CloseToTray, enabled);
}

bool SettingsStore::startMinimized() const
{
    return read(Key::StartMinimized, false).toBool();
}

void SettingsStore::setStartMinimized(bool enabled)
{
    write(Key::StartMinimized, enabled);
}

// --- Tải tệp ---------------------------------------------------------------

bool SettingsStore::autoDownloadPhotos() const
{
    return read(Key::AutoPhotos, true).toBool();
}

void SettingsStore::setAutoDownloadPhotos(bool enabled)
{
    write(Key::AutoPhotos, enabled);
}

int SettingsStore::autoDownloadMaxMegabytes() const
{
    return qBound(0, read(Key::AutoMaxMb, 8).toInt(), 2048);
}

void SettingsStore::setAutoDownloadMaxMegabytes(int mb)
{
    write(Key::AutoMaxMb, qBound(0, mb, 2048));
}

// --- Proxy -----------------------------------------------------------------

SettingsStore::ProxyKind SettingsStore::proxyKind() const
{
    const QString value = read(Key::ProxyKind, QStringLiteral("None")).toString();
    if (value.compare(QStringLiteral("Socks5"), Qt::CaseInsensitive) == 0)
        return ProxyKind::Socks5;
    if (value.compare(QStringLiteral("Http"), Qt::CaseInsensitive) == 0)
        return ProxyKind::Http;
    if (value.compare(QStringLiteral("MtProto"), Qt::CaseInsensitive) == 0)
        return ProxyKind::MtProto;
    return ProxyKind::None;
}

void SettingsStore::setProxyKind(ProxyKind kind)
{
    const char *name = "None";
    switch (kind) {
    case ProxyKind::Socks5:  name = "Socks5";  break;
    case ProxyKind::Http:    name = "Http";    break;
    case ProxyKind::MtProto: name = "MtProto"; break;
    case ProxyKind::None:    name = "None";    break;
    }
    write(Key::ProxyKind, QString::fromLatin1(name));
}

QString SettingsStore::proxyServer() const
{
    return read(Key::ProxyServer, QString()).toString();
}

void SettingsStore::setProxyServer(const QString &server)
{
    write(Key::ProxyServer, server.trimmed());
}

int SettingsStore::proxyPort() const
{
    return read(Key::ProxyPort, 1080).toInt();
}

void SettingsStore::setProxyPort(int port)
{
    write(Key::ProxyPort, qBound(1, port, 65535));
}

QString SettingsStore::proxyUsername() const
{
    return read(Key::ProxyUser, QString()).toString();
}

void SettingsStore::setProxyUsername(const QString &user)
{
    write(Key::ProxyUser, user);
}

QString SettingsStore::proxyPassword() const
{
    return deobfuscate(read(Key::ProxyPassword, QString()).toString());
}

void SettingsStore::setProxyPassword(const QString &password)
{
    write(Key::ProxyPassword, obfuscate(password));
}

QString SettingsStore::proxySecret() const
{
    return read(Key::ProxySecret, QString()).toString();
}

void SettingsStore::setProxySecret(const QString &secret)
{
    write(Key::ProxySecret, secret.trimmed());
}

// --- TDLib -----------------------------------------------------------------

QString SettingsStore::tdlibPathOverride() const
{
    return read(Key::TdlibPath, QString()).toString();
}

void SettingsStore::setTdlibPathOverride(const QString &path)
{
    write(Key::TdlibPath, path);
}

int SettingsStore::tdlibVerbosity() const
{
    return qBound(0, read(Key::TdlibVerbosity, 1).toInt(), 5);
}

void SettingsStore::setTdlibVerbosity(int level)
{
    write(Key::TdlibVerbosity, qBound(0, level, 5));
}

// --- Phiên làm việc --------------------------------------------------------

QByteArray SettingsStore::windowGeometry() const
{
    return read(Key::WindowGeometry, QByteArray()).toByteArray();
}

void SettingsStore::setWindowGeometry(const QByteArray &data)
{
    write(Key::WindowGeometry, data);
}

QByteArray SettingsStore::splitterState() const
{
    return read(Key::SplitterState, QByteArray()).toByteArray();
}

void SettingsStore::setSplitterState(const QByteArray &data)
{
    write(Key::SplitterState, data);
}

QString SettingsStore::lastAccountSlug() const
{
    return read(Key::LastAccount, QString()).toString();
}

void SettingsStore::setLastAccountSlug(const QString &slug)
{
    write(Key::LastAccount, slug);
}

bool SettingsStore::setupCompleted() const
{
    return read(Key::SetupDone, false).toBool();
}

void SettingsStore::setSetupCompleted(bool done)
{
    write(Key::SetupDone, done);
}

void SettingsStore::flush()
{
    m_settings->sync();
}
