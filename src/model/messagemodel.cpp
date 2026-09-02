#include "model/messagemodel.h"

#include "td/tdaccount.h"

#include <QDateTime>

#include <algorithm>

namespace {

//! Hai tin nhắn có được gộp thành một chuỗi (cùng người, cách nhau < 5 phút)?
bool sameGroup(const MessageEntry &a, const MessageEntry &b)
{
    if (a.kind == MessageEntry::Kind::Service || b.kind == MessageEntry::Kind::Service)
        return false;
    if (a.senderUserId != b.senderUserId || a.senderChatId != b.senderChatId)
        return false;
    if (a.isOutgoing != b.isOutgoing)
        return false;
    return qAbs(b.date - a.date) < 300;
}

} // namespace

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void MessageModel::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;

    if (m_account)
        m_account->disconnect(this);

    m_account = account;

    if (m_account) {
        connect(m_account, &TdAccount::historyReady, this, &MessageModel::onHistoryReady);
        connect(m_account, &TdAccount::newMessageArrived, this, &MessageModel::onNewMessage);
        connect(m_account, &TdAccount::messageChanged, this, &MessageModel::onMessageChanged);
        connect(m_account, &TdAccount::messagesDeleted, this, &MessageModel::onMessagesDeleted);
        connect(m_account, &TdAccount::messageSendSucceeded, this, &MessageModel::onSendSucceeded);
    }
}

void MessageModel::setChat(qint64 chatId)
{
    if (m_chatId == chatId)
        return;

    beginResetModel();
    m_chatId = chatId;
    // Hiển thị ngay những tin đã có trong bộ nhớ để không thấy khung trắng khi
    // chuyển qua lại giữa các cuộc trò chuyện; lịch sử đầy đủ nạp sau.
    m_items = (m_account && chatId != 0) ? m_account->cachedMessages(chatId, 60)
                                         : QList<MessageEntry>();
    m_reachedOldest = false;
    endResetModel();

    if (m_account && m_chatId != 0) {
        setLoading(true);
        m_account->loadHistory(m_chatId, 0, 40);
        if (!m_items.isEmpty())
            emit newestMessageAppended();
    }
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    const MessageEntry *entry = entryAt(index);
    if (!entry)
        return QVariant();

    switch (role) {
    case MessageIdRole:
        return entry->id;
    case DateRole:
        return QDateTime::fromSecsSinceEpoch(entry->date);
    case OutgoingRole:
        return entry->isOutgoing;
    case Qt::DisplayRole:
        return entry->text;
    default:
        break;
    }
    return QVariant();
}

const MessageEntry *MessageModel::entryAtRow(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

const MessageEntry *MessageModel::entryAt(const QModelIndex &index) const
{
    return index.isValid() ? entryAtRow(index.row()) : nullptr;
}

QModelIndex MessageModel::indexOfMessage(qint64 messageId) const
{
    const int row = rowForId(messageId);
    return row < 0 ? QModelIndex() : index(row, 0);
}

bool MessageModel::showDayDivider(int row) const
{
    const MessageEntry *entry = entryAtRow(row);
    if (!entry)
        return false;
    if (row == 0)
        return true;
    const MessageEntry *previous = entryAtRow(row - 1);
    if (!previous)
        return true;
    const QDate a = QDateTime::fromSecsSinceEpoch(previous->date).date();
    const QDate b = QDateTime::fromSecsSinceEpoch(entry->date).date();
    return a != b;
}

bool MessageModel::showSenderName(int row) const
{
    const MessageEntry *entry = entryAtRow(row);
    if (!entry || entry->isOutgoing || entry->kind == MessageEntry::Kind::Service)
        return false;
    if (showDayDivider(row))
        return true;
    const MessageEntry *previous = entryAtRow(row - 1);
    if (!previous)
        return true;
    return !sameGroup(*previous, *entry);
}

bool MessageModel::isTailOfGroup(int row) const
{
    const MessageEntry *entry = entryAtRow(row);
    if (!entry)
        return true;
    const MessageEntry *next = entryAtRow(row + 1);
    if (!next)
        return true;
    if (showDayDivider(row + 1))
        return true;
    return !sameGroup(*entry, *next);
}

void MessageModel::loadOlder()
{
    if (!m_account || m_chatId == 0 || m_loading || m_reachedOldest)
        return;
    const qint64 oldest = m_items.isEmpty() ? 0 : m_items.first().id;
    setLoading(true);
    m_account->loadHistory(m_chatId, oldest, 40);
}

QList<qint64> MessageModel::messageIdsInRange(int firstRow, int lastRow) const
{
    QList<qint64> ids;
    const int from = qMax(0, firstRow);
    const int to = qMin(m_items.size() - 1, lastRow);
    for (int row = from; row <= to; ++row)
        ids.append(m_items.at(row).id);
    return ids;
}

void MessageModel::onHistoryReady(qint64 chatId, const QList<MessageEntry> &messages,
                                  bool reachedOldest)
{
    if (chatId != m_chatId)
        return;

    setLoading(false);

    if (messages.isEmpty()) {
        if (reachedOldest)
            m_reachedOldest = true;
        return;
    }

    // TDLib trả về mới → cũ; ta cần cũ → mới.
    QList<MessageEntry> incoming = messages;
    std::sort(incoming.begin(), incoming.end(),
              [](const MessageEntry &a, const MessageEntry &b) { return a.id < b.id; });

    // Bỏ các tin đã có trong danh sách.
    QList<MessageEntry> fresh;
    fresh.reserve(incoming.size());
    for (const MessageEntry &entry : incoming) {
        if (rowForId(entry.id) < 0)
            fresh.append(entry);
    }
    if (fresh.isEmpty())
        return;

    const bool prepend = !m_items.isEmpty() && fresh.last().id < m_items.first().id;
    if (prepend) {
        beginInsertRows(QModelIndex(), 0, fresh.size() - 1);
        for (int i = fresh.size() - 1; i >= 0; --i)
            m_items.prepend(fresh.at(i));
        endInsertRows();
        emit olderMessagesInserted(fresh.size());
    } else {
        for (const MessageEntry &entry : fresh)
            insertSorted(entry);
        emit newestMessageAppended();
    }
}

void MessageModel::onNewMessage(qint64 chatId, qint64 messageId)
{
    if (chatId != m_chatId || !m_account)
        return;
    const MessageEntry *entry = m_account->cachedMessage(chatId, messageId);
    if (!entry)
        return;
    if (rowForId(messageId) >= 0) {
        onMessageChanged(chatId, messageId);
        return;
    }
    insertSorted(*entry);
    emit newestMessageAppended();
}

void MessageModel::onMessageChanged(qint64 chatId, qint64 messageId)
{
    if (chatId != m_chatId || !m_account)
        return;
    const int row = rowForId(messageId);
    if (row < 0)
        return;
    if (const MessageEntry *entry = m_account->cachedMessage(chatId, messageId))
        m_items[row] = *entry;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
}

void MessageModel::onMessagesDeleted(qint64 chatId, const QList<qint64> &messageIds)
{
    if (chatId != m_chatId)
        return;
    for (qint64 messageId : messageIds) {
        const int row = rowForId(messageId);
        if (row < 0)
            continue;
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }
}

void MessageModel::onSendSucceeded(qint64 chatId, qint64 oldMessageId, qint64 newMessageId)
{
    if (chatId != m_chatId || !m_account)
        return;

    const int row = rowForId(oldMessageId);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }
    if (const MessageEntry *entry = m_account->cachedMessage(chatId, newMessageId)) {
        if (rowForId(newMessageId) < 0) {
            insertSorted(*entry);
            emit newestMessageAppended();
        }
    }
}

void MessageModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged(loading);
}

int MessageModel::rowForId(qint64 messageId) const
{
    for (int row = m_items.size() - 1; row >= 0; --row) {
        if (m_items.at(row).id == messageId)
            return row;
    }
    return -1;
}

void MessageModel::insertSorted(const MessageEntry &entry)
{
    int row = m_items.size();
    while (row > 0 && m_items.at(row - 1).id > entry.id)
        --row;

    beginInsertRows(QModelIndex(), row, row);
    m_items.insert(row, entry);
    endInsertRows();

    // Dòng liền trước/sau có thể phải đổi cách vẽ (gộp nhóm, dải ngày).
    if (row > 0) {
        const QModelIndex previous = index(row - 1, 0);
        emit dataChanged(previous, previous);
    }
    if (row + 1 < m_items.size()) {
        const QModelIndex next = index(row + 1, 0);
        emit dataChanged(next, next);
    }
}
