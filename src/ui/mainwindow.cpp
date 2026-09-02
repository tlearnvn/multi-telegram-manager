#include "ui/mainwindow.h"

#include "core/apppaths.h"
#include "core/formatting.h"
#include "core/logging.h"
#include "core/settingsstore.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "td/tdloader.h"
#include "ui/aboutdialog.h"
#include "ui/accountrail.h"
#include "ui/broadcastdialog.h"
#include "ui/chatinfopane.h"
#include "ui/chatlistpane.h"
#include "ui/chatpickerdialog.h"
#include "ui/chatview.h"
#include "ui/dashboardpane.h"
#include "ui/flatbutton.h"
#include "ui/iconfactory.h"
#include "ui/logindialog.h"
#include "ui/newchatdialog.h"
#include "ui/settingsdialog.h"
#include "ui/setupwizard.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/trayicon.h"

#include "version.h"

#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

MainWindow::MainWindow(AccountManager *manager, QWidget *parent)
    : QMainWindow(parent)
    , m_manager(manager)
{
    setWindowTitle(QStringLiteral(APP_NAME));
    setWindowIcon(Icons::appIcon());
    setMinimumSize(940, 620);

    buildUi();
    buildMenus();
    buildShortcuts();

    connect(m_manager, &AccountManager::activeAccountChanged,
            this, &MainWindow::onActiveAccountChanged);
    connect(m_manager, &AccountManager::accountAdded, this, &MainWindow::wireAccount);
    connect(m_manager, &AccountManager::aggregateUnreadChanged, this, [this](int total) {
        if (m_tray)
            m_tray->setUnreadTotal(total);
        setWindowTitle(total > 0
            ? QStringLiteral("%1 (%2)").arg(QStringLiteral(APP_NAME)).arg(total)
            : QStringLiteral(APP_NAME));
    });
    connect(m_manager, &AccountManager::accountsChanged, this, &MainWindow::updateStatusBar);
    connect(&Theme::instance(), &Theme::changed, this, &MainWindow::applyTheme);

    // Gắn các tài khoản đã nạp từ đĩa.
    const QList<TdAccount *> accounts = m_manager->accounts();
    for (TdAccount *account : accounts)
        wireAccount(account);

    restoreLayout();
    applyTheme();
    onActiveAccountChanged(m_manager->activeAccount());
    updateStatusBar();
}

MainWindow::~MainWindow()
{
    persistLayout();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rail = new AccountRail(m_manager, central);
    layout->addWidget(m_rail);
    layout->addWidget(new Separator(Qt::Vertical, central));

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    m_chatList = new ChatListPane(m_splitter);
    m_chatList->setMinimumWidth(260);
    m_splitter->addWidget(m_chatList);

    m_centerStack = new QStackedWidget(m_splitter);
    m_chatView = new ChatView(m_centerStack);
    m_dashboard = new DashboardPane(m_manager, m_centerStack);
    m_centerStack->addWidget(m_chatView);
    m_centerStack->addWidget(m_dashboard);
    m_centerStack->setCurrentWidget(m_chatView);
    m_splitter->addWidget(m_centerStack);

    m_infoPane = new ChatInfoPane(m_splitter);
    m_infoPane->hide();
    m_splitter->addWidget(m_infoPane);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    layout->addWidget(m_splitter, 1);

    setCentralWidget(central);

    // --- Thanh trạng thái --------------------------------------------------
    m_statusLeft = new QLabel(this);
    m_statusRight = new QLabel(this);
    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusRight);
    statusBar()->setSizeGripEnabled(true);

    // --- Khay hệ thống -----------------------------------------------------
    m_tray = new TrayIcon(this);
    if (m_tray->isAvailable()) {
        m_tray->setVisible(SettingsStore::instance().trayEnabled());
        connect(m_tray, &TrayIcon::showWindowRequested, this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        connect(m_tray, &TrayIcon::quitRequested, this, [this] {
            m_reallyQuit = true;
            close();
        });
        connect(m_tray, &TrayIcon::settingsRequested, this, &MainWindow::openSettings);
        connect(m_tray, &TrayIcon::openChatRequested, this,
                [this](const QString &slug, qint64 chatId) {
            if (TdAccount *account = m_manager->accountBySlug(slug)) {
                m_manager->setActiveAccount(account);
                QTimer::singleShot(120, this, [this, chatId] { onChatSelected(chatId); });
            }
        });
    }

    // --- Kết nối các cột ---------------------------------------------------
    connect(m_chatList, &ChatListPane::chatSelected, this, &MainWindow::onChatSelected);
    connect(m_chatList, &ChatListPane::newChatRequested, this, &MainWindow::onNewChatRequested);
    connect(m_chatList, &ChatListPane::settingsRequested, this, &MainWindow::openSettings);
    connect(m_chatList, &ChatListPane::dashboardRequested, this, &MainWindow::openDashboard);
    connect(m_chatList, &ChatListPane::broadcastRequested, this, &MainWindow::openBroadcast);
    connect(m_chatList, &ChatListPane::searchInChatRequested, this, [this](qint64 chatId) {
        onChatSelected(chatId);
        m_chatView->openSearchBar();
    });

    connect(m_chatView, &ChatView::forwardRequested, this, &MainWindow::onForwardRequested);
    connect(m_chatView, &ChatView::chatInfoRequested, this, &MainWindow::showChatInfo);
    connect(m_chatView, &ChatView::statusMessage, this, &MainWindow::showToast);

    connect(m_infoPane, &ChatInfoPane::closeRequested, this, [this] { m_infoPane->hide(); });
    connect(m_infoPane, &ChatInfoPane::statusMessage, this, &MainWindow::showToast);
    connect(m_infoPane, &ChatInfoPane::openChatRequested, this, &MainWindow::onChatSelected);

    connect(m_rail, &AccountRail::addAccountRequested, this, &MainWindow::addAccount);
    connect(m_rail, &AccountRail::settingsRequested, this, &MainWindow::openSettings);
    connect(m_rail, &AccountRail::dashboardRequested, this, &MainWindow::openDashboard);
    connect(m_rail, &AccountRail::accountMenuRequested, this, &MainWindow::onAccountMenu);

    connect(m_dashboard, &DashboardPane::closeRequested, this, [this] {
        m_centerStack->setCurrentWidget(m_chatView);
    });
    connect(m_dashboard, &DashboardPane::addAccountRequested, this, &MainWindow::addAccount);
    connect(m_dashboard, &DashboardPane::statusMessage, this, &MainWindow::showToast);
    connect(m_dashboard, &DashboardPane::openAccountRequested, this, [this](TdAccount *account) {
        m_manager->setActiveAccount(account);
        m_centerStack->setCurrentWidget(m_chatView);
    });
    connect(m_dashboard, &DashboardPane::loginRequested, this, &MainWindow::showAccountLogin);
}

void MainWindow::buildMenus()
{
    auto *accountMenu = menuBar()->addMenu(tr("&Tài khoản"));
    accountMenu->addAction(tr("Thêm tài khoản…"), QKeySequence(QStringLiteral("Ctrl+Shift+N")),
                           this, &MainWindow::addAccount);
    accountMenu->addAction(tr("Bảng điều khiển"), QKeySequence(QStringLiteral("Ctrl+D")),
                           this, &MainWindow::openDashboard);
    accountMenu->addAction(tr("Gửi tin hàng loạt…"), this, &MainWindow::openBroadcast);
    accountMenu->addSeparator();
    accountMenu->addAction(tr("Thoát"), QKeySequence(QStringLiteral("Ctrl+Q")), this, [this] {
        m_reallyQuit = true;
        close();
    });

    auto *chatMenu = menuBar()->addMenu(tr("&Trò chuyện"));
    chatMenu->addAction(tr("Trò chuyện mới…"), QKeySequence(QStringLiteral("Ctrl+N")),
                        this, &MainWindow::onNewChatRequested);
    chatMenu->addAction(tr("Tìm cuộc trò chuyện"), QKeySequence(QStringLiteral("Ctrl+F")),
                        this, [this] { m_chatList->focusSearch(); });
    chatMenu->addAction(tr("Tìm trong cuộc trò chuyện"),
                        QKeySequence(QStringLiteral("Ctrl+Shift+F")),
                        this, [this] { m_chatView->openSearchBar(); });
    chatMenu->addAction(tr("Cuộc trò chuyện chưa đọc kế tiếp"),
                        QKeySequence(QStringLiteral("Ctrl+Shift+Down")),
                        this, [this] { m_chatList->goToNextUnread(); });

    auto *viewMenu = menuBar()->addMenu(tr("&Xem"));
    viewMenu->addAction(tr("Bảng thông tin"), QKeySequence(QStringLiteral("Ctrl+I")),
                        this, [this] {
        if (m_infoPane->isVisible())
            m_infoPane->hide();
        else
            showChatInfo(m_chatView->chatId());
    });
    viewMenu->addAction(tr("Đổi sáng / tối"), QKeySequence(QStringLiteral("Ctrl+Shift+T")),
                        this, [this] {
        SettingsStore &settings = SettingsStore::instance();
        settings.setThemeMode(Theme::instance().isDark() ? SettingsStore::ThemeMode::Light
                                                         : SettingsStore::ThemeMode::Dark);
        settings.flush();
        Icons::clearCache();
        Theme::instance().apply();
        showToast(Theme::instance().isDark() ? tr("Đã chuyển sang chủ đề tối")
                                             : tr("Đã chuyển sang chủ đề sáng"));
    });
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Cài đặt…"), QKeySequence(QStringLiteral("Ctrl+,")),
                        this, &MainWindow::openSettings);

    auto *helpMenu = menuBar()->addMenu(tr("&Trợ giúp"));
    helpMenu->addAction(tr("Hướng dẫn thiết lập lại"), this, [this] {
        SetupWizard wizard(this);
        wizard.exec();
        updateStatusBar();
    });
    helpMenu->addAction(tr("Mở thư mục dữ liệu"), this, [this] {
        showToast(tr("Dữ liệu nằm ở: %1").arg(QDir::toNativeSeparators(AppPaths::dataDir())));
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Giới thiệu %1").arg(QStringLiteral(APP_NAME)), this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });
}

void MainWindow::buildShortcuts()
{
    // Chuyển tài khoản bằng Ctrl+1..Ctrl+9.
    for (int index = 0; index < 9; ++index) {
        auto *shortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+%1").arg(index + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, index] {
            if (TdAccount *account = m_manager->accountAt(index)) {
                m_manager->setActiveAccount(account);
                m_centerStack->setCurrentWidget(m_chatView);
            }
        });
    }

    auto *nextAccount = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")), this);
    connect(nextAccount, &QShortcut::activated, this, [this] {
        if (m_manager->count() < 2)
            return;
        const int current = m_manager->indexOf(m_manager->activeAccount());
        m_manager->setActiveIndex((current + 1) % m_manager->count());
    });

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (m_infoPane->isVisible())
            m_infoPane->hide();
        else if (m_centerStack->currentWidget() == m_dashboard)
            m_centerStack->setCurrentWidget(m_chatView);
        else
            m_chatList->clearSearch();
    });
}

void MainWindow::wireAccount(TdAccount *account)
{
    if (!account)
        return;

    connect(account, &TdAccount::notificationRequested, this, &MainWindow::onNotification,
            Qt::UniqueConnection);
    connect(account, &TdAccount::errorOccurred, this, [this, account](const QString &message) {
        // Chỉ báo lỗi của tài khoản đang xem để không làm rối người dùng.
        if (account == m_activeAccount)
            showToast(message);
    }, Qt::UniqueConnection);
    connect(account, &TdAccount::connectionStateChanged, this,
            [this](TdConnectionState) { updateStatusBar(); }, Qt::UniqueConnection);
    connect(account, &TdAccount::stateChanged, this,
            [this, account](TdAccount::State state) {
        updateStatusBar();
        // Tài khoản mới cần đăng nhập → mở hộp thoại nếu đang là tài khoản hiện hành.
        if (account == m_activeAccount
            && (state == TdAccount::State::WaitPhone
                || state == TdAccount::State::WaitCode
                || state == TdAccount::State::WaitPassword)) {
            // Không tự mở khi người dùng đã có hộp thoại — LoginDialog tự lo.
        }
    }, Qt::UniqueConnection);
}

void MainWindow::startUp()
{
    if (SettingsStore::instance().startMinimized() && m_tray && m_tray->isAvailable()) {
        m_tray->setVisible(true);
        hide();
    } else {
        show();
    }

    QTimer::singleShot(400, this, &MainWindow::maybeRunSetupWizard);
}

void MainWindow::maybeRunSetupWizard()
{
    SettingsStore &settings = SettingsStore::instance();
    const bool needsApi = !settings.hasApiCredentials();
    const bool needsTdlib = !TdLoader::instance().load();

    if (!needsApi && !needsTdlib && settings.setupCompleted())
        return;

    if (!isVisible())
        show();

    SetupWizard wizard(this);
    wizard.exec();
    updateStatusBar();

    if (wizard.isReady() && m_manager->count() == 0)
        addAccount();
}

void MainWindow::onActiveAccountChanged(TdAccount *account)
{
    m_activeAccount = account;
    m_chatList->setAccount(account);
    m_chatView->setAccount(account);
    m_infoPane->setAccount(account);
    m_infoPane->hide();

    if (account) {
        SettingsStore::instance().setLastAccountSlug(account->slug());
        wireAccount(account);
    }
    updateStatusBar();
}

void MainWindow::onChatSelected(qint64 chatId)
{
    m_centerStack->setCurrentWidget(m_chatView);
    m_chatList->selectChat(chatId);
    m_chatView->setChat(chatId);
    if (m_infoPane->isVisible())
        m_infoPane->setChat(chatId);
}

void MainWindow::showChatInfo(qint64 chatId)
{
    if (chatId == 0) {
        showToast(tr("Hãy mở một cuộc trò chuyện trước."));
        return;
    }
    m_infoPane->setChat(chatId);
    m_infoPane->show();
}

void MainWindow::onNewChatRequested()
{
    TdAccount *account = m_manager->activeAccount();
    if (!account || !account->isReady()) {
        showToast(tr("Cần một tài khoản đã đăng nhập để tạo cuộc trò chuyện."));
        return;
    }

    NewChatDialog dialog(account, this);
    connect(&dialog, &NewChatDialog::statusMessage, this, &MainWindow::showToast);
    connect(&dialog, &NewChatDialog::chatReady, this, &MainWindow::onChatSelected);
    dialog.exec();
}

void MainWindow::onForwardRequested(qint64 fromChatId, const QList<qint64> &messageIds)
{
    TdAccount *source = m_manager->activeAccount();
    if (!source || messageIds.isEmpty())
        return;

    ChatPickerDialog picker(m_manager, nullptr, true, this);
    picker.setHeaderText(tr("Chuyển tiếp %1 tin nhắn tới:").arg(messageIds.size()));
    if (picker.exec() != QDialog::Accepted)
        return;

    const QList<ChatPickerDialog::Selection> targets = picker.selections();
    if (targets.isEmpty())
        return;

    int forwarded = 0;
    int copied = 0;
    for (const ChatPickerDialog::Selection &target : targets) {
        if (target.account == source) {
            source->forwardMessages(target.chatId, fromChatId, messageIds, false);
            ++forwarded;
        } else {
            // Khác tài khoản thì TDLib không chuyển tiếp trực tiếp được — gửi lại
            // phần văn bản dưới dạng tin mới.
            for (qint64 messageId : messageIds) {
                const MessageEntry *entry = source->cachedMessage(fromChatId, messageId);
                if (!entry)
                    continue;
                QString text = entry->text;
                if (text.isEmpty())
                    text = tr("[%1]").arg(entry->kindLabel());
                target.account->sendText(target.chatId, text);
            }
            ++copied;
        }
    }

    if (copied > 0) {
        showToast(tr("Đã chuyển tiếp tới %1 nơi; %2 nơi khác tài khoản được gửi lại "
                     "dưới dạng tin mới.").arg(forwarded).arg(copied));
    } else {
        showToast(tr("Đã chuyển tiếp tới %1 nơi.").arg(forwarded));
    }
}

void MainWindow::onAccountMenu(TdAccount *account, const QPoint &globalPos)
{
    if (!account)
        return;

    const Theme::Colors &c = Theme::instance().colors();
    QMenu menu(this);

    menu.addAction(tr("Chuyển sang tài khoản này"), this, [this, account] {
        m_manager->setActiveAccount(account);
        m_centerStack->setCurrentWidget(m_chatView);
    });

    if (!account->isReady()) {
        menu.addAction(Icons::icon(Icons::Name::Key, c.accent), tr("Đăng nhập…"), this,
                       [this, account] { showAccountLogin(account); });
    }

    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::Name::Edit, c.textSecondary), tr("Đổi tên hiển thị…"),
                   this, [this, account] {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("Tên hiển thị"),
            tr("Đặt nhãn riêng cho tài khoản này (để phân biệt khi dùng nhiều "
               "tài khoản):"),
            QLineEdit::Normal, account->label(), &ok);
        if (ok) {
            account->setLabel(name.trimmed());
            m_rail->refresh();
        }
    });

    menu.addAction(tr("Đổi màu nhấn…"), this, [this, account] {
        const QColor color = QColorDialog::getColor(QColor(account->accentColor()), this,
                                                    tr("Chọn màu cho tài khoản"));
        if (color.isValid()) {
            account->setAccentColor(color.name());
            m_rail->refresh();
        }
    });

    menu.addAction(Icons::icon(Icons::Name::Refresh, c.textSecondary), tr("Mở lại kết nối"),
                   this, [this, account] {
        account->open();
        showToast(tr("Đang mở lại %1…").arg(account->displayName()));
    });

    menu.addSeparator();

    if (account->isReady()) {
        menu.addAction(Icons::icon(Icons::Name::Logout, c.danger), tr("Đăng xuất"), this,
                       [this, account] {
            if (QMessageBox::question(this, tr("Đăng xuất"),
                    tr("Đăng xuất khỏi %1?").arg(account->displayName()))
                == QMessageBox::Yes) {
                account->logOut();
            }
        });
    }

    menu.addAction(Icons::icon(Icons::Name::Trash, c.danger),
                   tr("Xoá tài khoản khỏi ứng dụng"), this, [this, account] {
        const QString slug = account->slug();
        const QString name = account->displayName();
        if (QMessageBox::warning(this, tr("Xoá tài khoản"),
                tr("Xoá %1 khỏi ứng dụng và xoá toàn bộ dữ liệu cục bộ của tài khoản "
                   "này?\n\nTài khoản Telegram không bị ảnh hưởng.").arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            m_manager->removeAccount(slug, true);
            showToast(tr("Đã xoá %1.").arg(name));
        }
    });

    menu.exec(globalPos);
}

void MainWindow::showAccountLogin(TdAccount *account)
{
    if (!account)
        return;

    if (!SettingsStore::instance().hasApiCredentials()) {
        showToast(tr("Chưa có api_id/api_hash — hãy vào Cài đặt → Nâng cao."));
        SetupWizard wizard(this);
        wizard.exec();
        if (!SettingsStore::instance().hasApiCredentials())
            return;
    }

    if (!TdLoader::instance().load()) {
        QMessageBox::warning(this, tr("Thiếu TDLib"), TdLoader::instance().installationHint());
        return;
    }

    account->open();

    LoginDialog dialog(account, this);
    dialog.exec();
    m_rail->refresh();
    updateStatusBar();
}

void MainWindow::addAccount()
{
    if (!SettingsStore::instance().hasApiCredentials()
        || !TdLoader::instance().load()) {
        SetupWizard wizard(this);
        wizard.exec();
        if (!SettingsStore::instance().hasApiCredentials())
            return;
        if (!TdLoader::instance().load()) {
            QMessageBox::warning(this, tr("Thiếu TDLib"),
                                 TdLoader::instance().installationHint());
            return;
        }
    }

    TdAccount *account = m_manager->createAccount();
    wireAccount(account);
    showAccountLogin(account);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_manager, this);
    connect(&dialog, &SettingsDialog::statusMessage, this, &MainWindow::showToast);
    dialog.exec();

    if (m_tray)
        m_tray->setVisible(SettingsStore::instance().trayEnabled());
    updateStatusBar();
}

void MainWindow::openDashboard()
{
    m_centerStack->setCurrentWidget(m_dashboard);
}

void MainWindow::openBroadcast()
{
    if (m_manager->count() == 0) {
        showToast(tr("Cần ít nhất một tài khoản đã đăng nhập."));
        return;
    }
    BroadcastDialog dialog(m_manager, this);
    connect(&dialog, &BroadcastDialog::statusMessage, this, &MainWindow::showToast);
    dialog.exec();
}

void MainWindow::onNotification(qint64 chatId, const QString &title, const QString &body)
{
    if (!SettingsStore::instance().notificationsEnabled())
        return;

    auto *account = qobject_cast<TdAccount *>(sender());

    // Đang xem đúng cuộc trò chuyện đó và cửa sổ đang hoạt động → khỏi báo.
    const bool viewingThisChat = account == m_activeAccount
        && m_chatView->chatId() == chatId
        && isActiveWindow();
    if (viewingThisChat)
        return;

    if (m_tray && m_tray->isAvailable() && SettingsStore::instance().trayEnabled()) {
        m_tray->notify(title, body, chatId, account ? account->slug() : QString());
        return;
    }

    // Không có khay hệ thống thì hiện thông báo trong cửa sổ.
    if (isVisible())
        showToast(QStringLiteral("%1: %2").arg(title, body));
}

void MainWindow::showToast(const QString &message)
{
    Toast::popup(this, message);
    if (m_statusLeft)
        m_statusLeft->setText(message);
}

void MainWindow::updateStatusBar()
{
    TdAccount *account = m_manager->activeAccount();

    QString left;
    if (!TdLoader::instance().isLoaded()) {
        left = tr("Chưa nạp TDLib — vào Cài đặt → Nâng cao để chỉ đường dẫn.");
    } else if (!account) {
        left = tr("Chưa có tài khoản. Bấm “+” ở thanh bên trái để thêm.");
    } else if (!account->isReady()) {
        left = tr("%1 — %2").arg(account->displayName(),
                                 account->lastError().isEmpty()
                                     ? tr("chưa đăng nhập")
                                     : account->lastError());
    } else {
        left = tr("%1 — %2").arg(account->displayName(),
                                 connectionStateLabel(account->connectionState()));
    }
    m_statusLeft->setText(left);

    const int accounts = m_manager->count();
    const int unread = m_manager->totalUnread();
    m_statusRight->setText(tr("%1 tài khoản · %2 chưa đọc · v%3")
                               .arg(accounts).arg(unread)
                               .arg(QStringLiteral(APP_VERSION_STRING)));
}

void MainWindow::restoreLayout()
{
    const SettingsStore &settings = SettingsStore::instance();

    const QByteArray geometry = settings.windowGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1180, 760);

    const QByteArray splitter = settings.splitterState();
    if (!splitter.isEmpty())
        m_splitter->restoreState(splitter);
    else
        m_splitter->setSizes({ 320, 700, 300 });

    // Mở lại tài khoản dùng lần trước.
    const QString lastSlug = settings.lastAccountSlug();
    if (!lastSlug.isEmpty()) {
        if (TdAccount *account = m_manager->accountBySlug(lastSlug))
            m_manager->setActiveAccount(account);
    }
}

void MainWindow::persistLayout()
{
    SettingsStore &settings = SettingsStore::instance();
    settings.setWindowGeometry(saveGeometry());
    settings.setSplitterState(m_splitter->saveState());
    settings.flush();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    persistLayout();

    const bool toTray = !m_reallyQuit
        && SettingsStore::instance().closeToTray()
        && m_tray && m_tray->isAvailable()
        && SettingsStore::instance().trayEnabled();

    if (toTray) {
        hide();
        event->ignore();
        return;
    }

    m_manager->setOnlineAll(false);
    event->accept();
    QApplication::quit();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    // Báo cho Telegram biết mình đang hoạt động hay không.
    if (event->type() == QEvent::ActivationChange) {
        if (TdAccount *account = m_manager->activeAccount()) {
            if (account->isReady())
                account->setOnline(isActiveWindow());
        }
    }
}

void MainWindow::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    m_statusLeft->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
    m_statusRight->setStyleSheet(QStringLiteral("color: %1;").arg(c.textMuted.name()));
    menuBar()->setStyleSheet(QStringLiteral(
        "QMenuBar { background: %1; color: %2; border-bottom: 1px solid %3; }")
        .arg(c.windowBg.name(), c.textPrimary.name(), c.divider.name()));
}
