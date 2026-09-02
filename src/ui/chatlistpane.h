#pragma once

#include "td/tdenums.h"

#include <QWidget>

class ChatListDelegate;
class ChatListModel;
class IconButton;
class TdAccount;
class QLabel;
class QLineEdit;
class QListView;
class QTabBar;
class QTimer;

/*!
 * \brief Cột danh sách trò chuyện: tìm kiếm, bộ lọc nhanh, menu ngữ cảnh.
 */
class ChatListPane : public QWidget
{
    Q_OBJECT

public:
    explicit ChatListPane(QWidget *parent = nullptr);

    void setAccount(TdAccount *account);
    TdAccount *account() const { return m_account; }

    void selectChat(qint64 chatId);
    qint64 currentChatId() const { return m_currentChatId; }

    void focusSearch();
    void clearSearch();

    //! Chuyển sang cuộc trò chuyện chưa đọc kế tiếp (Ctrl+Shift+↓).
    void goToNextUnread();

signals:
    void chatSelected(qint64 chatId);
    void newChatRequested();
    void settingsRequested();
    void dashboardRequested();
    void broadcastRequested();
    void searchInChatRequested(qint64 chatId);

private slots:
    void onFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void runServerSearch();
    void showContextMenu(const QPoint &position);
    void applyTheme();

private:
    void buildUi();
    void updateEmptyState();

    TdAccount *m_account = nullptr;
    ChatListModel *m_model = nullptr;
    ChatListDelegate *m_delegate = nullptr;

    QLineEdit *m_search = nullptr;
    QTabBar *m_filters = nullptr;
    QListView *m_list = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_header = nullptr;
    IconButton *m_newChatButton = nullptr;
    IconButton *m_menuButton = nullptr;
    QTimer *m_searchTimer = nullptr;

    qint64 m_currentChatId = 0;
};
