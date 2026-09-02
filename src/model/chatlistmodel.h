#pragma once

#include "model/chatentry.h"
#include "td/tdenums.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class TdAccount;
class QTimer;

/*!
 * \brief Danh sách chat của một tài khoản, có lọc nhanh và tìm kiếm.
 *
 * Model chỉ giữ danh sách chatId đã sắp xếp; dữ liệu thật vẫn nằm trong
 * TdAccount, nên không nhân đôi bộ nhớ. Delegate lấy ChatEntry qua entryAt().
 */
class ChatListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ChatIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        UnreadRole
    };

    explicit ChatListModel(QObject *parent = nullptr);

    void setAccount(TdAccount *account);
    TdAccount *account() const { return m_account; }

    void setFilter(ChatFilterKind filter);
    ChatFilterKind filter() const { return m_filter; }

    void setSearchText(const QString &text);
    QString searchText() const { return m_search; }

    //! Chèn kết quả tìm kiếm phía máy chủ (chat công khai chưa có trong danh sách).
    void setExtraSearchResults(const QList<qint64> &chatIds);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    const ChatEntry *entryAt(const QModelIndex &index) const;
    const ChatEntry *entryAtRow(int row) const;
    qint64 chatIdAt(int row) const;
    QModelIndex indexOfChat(qint64 chatId) const;

public slots:
    void refresh();

private slots:
    void onChatUpserted(qint64 chatId);
    void onChatRemoved(qint64 chatId);
    void scheduleRebuild();

private:
    bool matchesFilter(const ChatEntry &entry) const;

    TdAccount *m_account = nullptr;
    ChatFilterKind m_filter = ChatFilterKind::All;
    QString m_search;
    QList<qint64> m_rows;
    QList<qint64> m_extraResults;
    QTimer *m_rebuildTimer = nullptr;
};
