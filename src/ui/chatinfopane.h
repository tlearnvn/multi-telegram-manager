#pragma once

#include <QWidget>

class IconButton;
class TdAccount;
class QLabel;
class QListWidget;
class QPushButton;

/*!
 * \brief Bảng thông tin bên phải: chi tiết cuộc trò chuyện và thành viên.
 */
class ChatInfoPane : public QWidget
{
    Q_OBJECT

public:
    explicit ChatInfoPane(QWidget *parent = nullptr);

    void setAccount(TdAccount *account);
    void setChat(qint64 chatId);
    qint64 chatId() const { return m_chatId; }

signals:
    void closeRequested();
    void statusMessage(const QString &text);
    void openChatRequested(qint64 chatId);

private slots:
    void reload();
    void applyTheme();

private:
    void buildUi();
    void loadMembers();

    TdAccount *m_account = nullptr;
    qint64 m_chatId = 0;

    QWidget *m_avatarBox = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_details = nullptr;
    QListWidget *m_members = nullptr;
    QLabel *m_membersHeader = nullptr;
    QPushButton *m_muteButton = nullptr;
    QPushButton *m_blockButton = nullptr;
    QPushButton *m_leaveButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    IconButton *m_closeButton = nullptr;
};
