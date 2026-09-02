#pragma once

#include <QMainWindow>
#include <QPointer>

class AccountManager;
class AccountRail;
class ChatInfoPane;
class ChatListPane;
class ChatView;
class DashboardPane;
class TdAccount;
class TrayIcon;
class QLabel;
class QSplitter;
class QStackedWidget;

/*!
 * \brief Cửa sổ chính.
 *
 * Bố cục ba (bốn) cột: thanh tài khoản → danh sách trò chuyện → khung hội
 * thoại → bảng thông tin (ẩn/hiện được). Bảng điều khiển tài khoản dùng chung
 * vùng với khung hội thoại.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AccountManager *manager, QWidget *parent = nullptr);
    ~MainWindow() override;

    //! Hiện cửa sổ theo cấu hình (bình thường hoặc thu nhỏ vào khay).
    void startUp();

public slots:
    void showToast(const QString &message);
    void openSettings();
    void openDashboard();
    void openBroadcast();
    void addAccount();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onActiveAccountChanged(TdAccount *account);
    void onChatSelected(qint64 chatId);
    void onForwardRequested(qint64 fromChatId, const QList<qint64> &messageIds);
    void onAccountMenu(TdAccount *account, const QPoint &globalPos);
    void onNotification(qint64 chatId, const QString &title, const QString &body);
    void onNewChatRequested();
    void updateStatusBar();
    void applyTheme();

private:
    void buildUi();
    void buildMenus();
    void buildShortcuts();
    void wireAccount(TdAccount *account);
    void showChatInfo(qint64 chatId);
    void restoreLayout();
    void persistLayout();
    void maybeRunSetupWizard();
    void showAccountLogin(TdAccount *account);

    AccountManager *m_manager;

    AccountRail *m_rail = nullptr;
    ChatListPane *m_chatList = nullptr;
    ChatView *m_chatView = nullptr;
    ChatInfoPane *m_infoPane = nullptr;
    DashboardPane *m_dashboard = nullptr;
    QStackedWidget *m_centerStack = nullptr;
    QSplitter *m_splitter = nullptr;

    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;

    TrayIcon *m_tray = nullptr;
    QPointer<TdAccount> m_activeAccount;
    bool m_reallyQuit = false;
};
