#include "ui/trayicon.h"

#include "core/settingsstore.h"
#include "ui/iconfactory.h"

#include "version.h"

#include <QAction>
#include <QApplication>
#include <QMenu>

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(Icons::appIcon(), this);
    m_tray->setToolTip(QStringLiteral(APP_NAME));
    rebuildMenu();

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showWindowRequested();
    });

    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        emit showWindowRequested();
        if (!m_pendingSlug.isEmpty() && m_pendingChatId != 0)
            emit openChatRequested(m_pendingSlug, m_pendingChatId);
    });
}

void TrayIcon::rebuildMenu()
{
    if (!m_tray)
        return;

    m_menu = new QMenu;
    m_menu->addAction(tr("Mở %1").arg(QStringLiteral(APP_NAME)), this,
                      [this] { emit showWindowRequested(); });
    m_menu->addSeparator();

    m_muteAction = m_menu->addAction(tr("Tắt thông báo"));
    m_muteAction->setCheckable(true);
    m_muteAction->setChecked(!SettingsStore::instance().notificationsEnabled());
    connect(m_muteAction, &QAction::toggled, this, [](bool muted) {
        SettingsStore::instance().setNotificationsEnabled(!muted);
    });

    m_menu->addAction(tr("Cài đặt…"), this, [this] { emit settingsRequested(); });
    m_menu->addSeparator();
    m_menu->addAction(tr("Thoát"), this, [this] { emit quitRequested(); });

    m_tray->setContextMenu(m_menu);
}

bool TrayIcon::isAvailable() const
{
    return m_tray != nullptr;
}

void TrayIcon::setVisible(bool visible)
{
    if (!m_tray)
        return;
    m_tray->setVisible(visible);
}

void TrayIcon::setUnreadTotal(int total)
{
    if (!m_tray)
        return;
    m_tray->setToolTip(total > 0
        ? tr("%1 — %2 cuộc trò chuyện chưa đọc").arg(QStringLiteral(APP_NAME)).arg(total)
        : QStringLiteral(APP_NAME));
}

bool TrayIcon::notify(const QString &title, const QString &body, qint64 chatId,
                      const QString &accountSlug)
{
    if (!m_tray || !m_tray->isVisible())
        return false;
    if (!QSystemTrayIcon::supportsMessages())
        return false;

    m_pendingSlug = accountSlug;
    m_pendingChatId = chatId;

    const SettingsStore &settings = SettingsStore::instance();
    const QString shown = settings.notificationPreview() ? body : tr("Tin nhắn mới");
    m_tray->showMessage(title, shown, Icons::appIcon(), 5000);
    return true;
}
