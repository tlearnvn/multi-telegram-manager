#pragma once

#include "model/messageentry.h"

#include <QAbstractListModel>
#include <QList>

class TdAccount;

/*!
 * \brief Lịch sử tin nhắn của một cuộc trò chuyện.
 *
 * Danh sách xếp từ cũ đến mới (giống thứ tự hiển thị). Khi cuộn lên đầu,
 * loadOlder() nạp thêm trang tin nhắn cũ hơn rồi chèn vào đầu danh sách.
 */
class MessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        MessageIdRole = Qt::UserRole + 1,
        DateRole,
        OutgoingRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    void setAccount(TdAccount *account);
    TdAccount *account() const { return m_account; }

    void setChat(qint64 chatId);
    qint64 chatId() const { return m_chatId; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    const MessageEntry *entryAtRow(int row) const;
    const MessageEntry *entryAt(const QModelIndex &index) const;
    QModelIndex indexOfMessage(qint64 messageId) const;

    //! Có cần vẽ dải phân cách ngày phía trên dòng này không.
    bool showDayDivider(int row) const;
    //! Có cần in tên người gửi (đầu một chuỗi tin của cùng người) không.
    bool showSenderName(int row) const;
    //! Tin cuối trong chuỗi liên tiếp của cùng người gửi (vẽ đuôi bong bóng).
    bool isTailOfGroup(int row) const;

    void loadOlder();
    bool isLoading() const { return m_loading; }
    bool reachedOldest() const { return m_reachedOldest; }

    //! Danh sách id đang hiển thị trong khoảng dòng cho trước (để đánh dấu đã đọc).
    QList<qint64> messageIdsInRange(int firstRow, int lastRow) const;

signals:
    void olderMessagesInserted(int count);
    void newestMessageAppended();
    void loadingChanged(bool loading);

private slots:
    void onHistoryReady(qint64 chatId, const QList<MessageEntry> &messages, bool reachedOldest);
    void onNewMessage(qint64 chatId, qint64 messageId);
    void onMessageChanged(qint64 chatId, qint64 messageId);
    void onMessagesDeleted(qint64 chatId, const QList<qint64> &messageIds);
    void onSendSucceeded(qint64 chatId, qint64 oldMessageId, qint64 newMessageId);

private:
    void setLoading(bool loading);
    int rowForId(qint64 messageId) const;
    void insertSorted(const MessageEntry &entry);

    TdAccount *m_account = nullptr;
    qint64 m_chatId = 0;
    QList<MessageEntry> m_items;
    bool m_loading = false;
    bool m_reachedOldest = false;
};
