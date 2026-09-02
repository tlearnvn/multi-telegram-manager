#pragma once

#include <QByteArray>
#include <QObject>
#include <QScopedPointer>
#include <QSettings>
#include <QString>
#include <QVariant>

/*!
 * \brief Cấu hình ứng dụng, lưu trong data/config.ini cạnh tệp chạy.
 *
 * Dùng như một singleton: SettingsStore::instance(). Mỗi khi một giá trị thay
 * đổi, tín hiệu changed() được phát để giao diện tự cập nhật.
 */
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    enum class ThemeMode { System, Dark, Light };
    Q_ENUM(ThemeMode)

    enum class ProxyKind { None, Socks5, Http, MtProto };
    Q_ENUM(ProxyKind)

    static SettingsStore &instance();

    // --- Thông tin API Telegram (bắt buộc, lấy từ my.telegram.org) ---------
    int apiId() const;
    void setApiId(int id);
    QString apiHash() const;
    void setApiHash(const QString &hash);
    bool hasApiCredentials() const;

    // --- Giao diện ---------------------------------------------------------
    ThemeMode themeMode() const;
    void setThemeMode(ThemeMode mode);
    QString accentColor() const;
    void setAccentColor(const QString &hex);
    int fontScalePercent() const;          //!< 80..150
    void setFontScalePercent(int percent);
    bool compactChatList() const;
    void setCompactChatList(bool compact);
    bool showAvatarsInGroups() const;
    void setShowAvatarsInGroups(bool show);

    // --- Thông báo ---------------------------------------------------------
    bool notificationsEnabled() const;
    void setNotificationsEnabled(bool enabled);
    bool notificationPreview() const;
    void setNotificationPreview(bool enabled);
    bool trayEnabled() const;
    void setTrayEnabled(bool enabled);
    bool closeToTray() const;
    void setCloseToTray(bool enabled);
    bool startMinimized() const;
    void setStartMinimized(bool enabled);

    // --- Tải tệp tự động --------------------------------------------------
    bool autoDownloadPhotos() const;
    void setAutoDownloadPhotos(bool enabled);
    int autoDownloadMaxMegabytes() const;  //!< 0 = không tự tải tệp lớn
    void setAutoDownloadMaxMegabytes(int mb);

    // --- Proxy -------------------------------------------------------------
    ProxyKind proxyKind() const;
    void setProxyKind(ProxyKind kind);
    QString proxyServer() const;
    void setProxyServer(const QString &server);
    int proxyPort() const;
    void setProxyPort(int port);
    QString proxyUsername() const;
    void setProxyUsername(const QString &user);
    QString proxyPassword() const;
    void setProxyPassword(const QString &password);
    QString proxySecret() const;
    void setProxySecret(const QString &secret);

    // --- TDLib -------------------------------------------------------------
    QString tdlibPathOverride() const;
    void setTdlibPathOverride(const QString &path);
    int tdlibVerbosity() const;
    void setTdlibVerbosity(int level);

    // --- Trạng thái cửa sổ / phiên làm việc -------------------------------
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &data);
    QByteArray splitterState() const;
    void setSplitterState(const QByteArray &data);
    QString lastAccountSlug() const;
    void setLastAccountSlug(const QString &slug);
    bool setupCompleted() const;
    void setSetupCompleted(bool done);

    //! Ghi ngay xuống đĩa.
    void flush();

signals:
    void changed(const QString &key);

private:
    explicit SettingsStore(QObject *parent = nullptr);

    QVariant read(const QString &key, const QVariant &fallback) const;
    void write(const QString &key, const QVariant &value);

    QScopedPointer<QSettings> m_settings;
};
