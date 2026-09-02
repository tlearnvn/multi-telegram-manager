#pragma once

#include <QDialog>
#include <QList>

class TdAccount;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTabWidget;

/*!
 * \brief Tạo cuộc trò chuyện mới: nhắn cho một người, tạo nhóm, tạo kênh,
 *        hoặc tham gia bằng liên kết / @tên.
 */
class NewChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewChatDialog(TdAccount *account, QWidget *parent = nullptr);

    //! Cuộc trò chuyện vừa tạo/mở (0 nếu không có).
    qint64 resultChatId() const { return m_resultChatId; }

signals:
    void chatReady(qint64 chatId);
    void statusMessage(const QString &text);

private slots:
    void loadContacts();
    void startPrivateChat();
    void createGroup();
    void createChannel();
    void joinByLink();

private:
    void buildUi();
    QList<qint64> checkedUserIds(QListWidget *list) const;
    void finishWith(qint64 chatId, const QString &message);

    TdAccount *m_account;
    QTabWidget *m_tabs = nullptr;

    QLineEdit *m_contactFilter = nullptr;
    QListWidget *m_contactList = nullptr;

    QLineEdit *m_groupTitle = nullptr;
    QLineEdit *m_groupFilter = nullptr;
    QListWidget *m_groupMembers = nullptr;

    QLineEdit *m_channelTitle = nullptr;
    QPlainTextEdit *m_channelDescription = nullptr;
    class QCheckBox *m_channelIsGroup = nullptr;

    QLineEdit *m_joinInput = nullptr;
    QLabel *m_status = nullptr;

    qint64 m_resultChatId = 0;
};
