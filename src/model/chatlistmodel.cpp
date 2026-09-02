#include "model/chatlistmodel.h"

#include "td/tdaccount.h"

#include <QTimer>

ChatListModel::ChatListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Gộp nhiều thay đổi thứ tự thành một lần dựng lại để không nhấp nháy.
    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(80);
    connect(m_rebuildTimer, &QTimer::timeout, this, &ChatListModel::refresh);
}

void ChatListModel::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;

    if (m_account)
        m_account->disconnect(this);

    m_account = account;

    if (m_account) {
        connect(m_account, &TdAccount::chatUpserted, this, &ChatListModel::onChatUpserted);
        connect(m_account, &TdAccount::chatRemoved, this, &ChatListModel::onChatRemoved);
        connect(m_account, &TdAccount::chatOrderChanged, this, &ChatListModel::scheduleRebuild);
        connect(m_account, &TdAccount::chatActionChanged, this, &ChatListModel::onChatUpserted);
    }

    refresh();
}

void ChatListModel::setFilter(ChatFilterKind filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    refresh();
}

void ChatListModel::setSearchText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (m_search == trimmed)
        return;
    m_search = trimmed;
    if (m_search.isEmpty())
        m_extraResults.clear();
    refresh();
}

void ChatListModel::setExtraSearchResults(const QList<qint64> &chatIds)
{
    m_extraResults = chatIds;
    refresh();
}

void ChatListModel::scheduleRebuild()
{
    if (!m_rebuildTimer->isActive())
        m_rebuildTimer->start();
}

void ChatListModel::refresh()
{
    QList<qint64> next;

    if (m_account) {
        const bool archived = m_filter == ChatFilterKind::Archived;
        const QList<qint64> source = m_account->orderedChatIds(archived);
        for (qint64 chatId : source) {
            const ChatEntry *entry = m_account->chat(chatId);
            if (entry && matchesFilter(*entry))
                next.append(chatId);
        }

        // Bổ sung kết quả tìm kiếm từ máy chủ (chưa nằm trong danh sách).
        for (qint64 chatId : m_extraResults) {
            if (!next.contains(chatId) && m_account->chat(chatId))
                next.append(chatId);
        }
    }

    if (next == m_rows)
        return;

    beginResetModel();
    m_rows = next;
    endResetModel();
}

bool ChatListModel::matchesFilter(const ChatEntry &entry) const
{
    if (!m_search.isEmpty()) {
        const bool hit = entry.title.contains(m_search, Qt::CaseInsensitive)
                      || entry.username.contains(m_search, Qt::CaseInsensitive)
                      || entry.lastMessagePreview.contains(m_search, Qt::CaseInsensitive);
        if (!hit)
            return false;
    }

    switch (m_filter) {
    case ChatFilterKind::All:
    case ChatFilterKind::Archived:
        return true;
    case ChatFilterKind::Unread:
        return entry.hasUnread();
    case ChatFilterKind::Private:
        return entry.kind == ChatEntry::Kind::Private || entry.kind == ChatEntry::Kind::Secret;
    case ChatFilterKind::Groups:
        return entry.isGroupLike();
    case ChatFilterKind::Channels:
        return entry.kind == ChatEntry::Kind::Channel;
    case ChatFilterKind::Bots: {
        if (entry.kind != ChatEntry::Kind::Private || !m_account)
            return false;
        const UserEntry *peer = m_account->user(entry.relatedUserId);
        return peer && peer->isBot;
    }
    }
    return true;
}

int ChatListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant ChatListModel::data(const QModelIndex &index, int role) const
{
    const ChatEntry *entry = entryAt(index);
    if (!entry)
        return QVariant();

    switch (role) {
    case ChatIdRole:
        return entry->id;
    case TitleRole:
    case Qt::DisplayRole:
        return entry->title;
    case PreviewRole:
        return entry->lastMessagePreview;
    case UnreadRole:
        return entry->unreadCount;
    case Qt::ToolTipRole:
        return QStringLiteral("%1\n%2").arg(entry->title, entry->kindLabel());
    default:
        break;
    }
    return QVariant();
}

const ChatEntry *ChatListModel::entryAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return entryAtRow(index.row());
}

const ChatEntry *ChatListModel::entryAtRow(int row) const
{
    if (!m_account || row < 0 || row >= m_rows.size())
        return nullptr;
    return m_account->chat(m_rows.at(row));
}

qint64 ChatListModel::chatIdAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return 0;
    return m_rows.at(row);
}

QModelIndex ChatListModel::indexOfChat(qint64 chatId) const
{
    const int row = m_rows.indexOf(chatId);
    return row < 0 ? QModelIndex() : index(row, 0);
}

void ChatListModel::onChatUpserted(qint64 chatId)
{
    const int row = m_rows.indexOf(chatId);
    if (row < 0) {
        scheduleRebuild();
        return;
    }
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
}

void ChatListModel::onChatRemoved(qint64 chatId)
{
    const int row = m_rows.indexOf(chatId);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_rows.removeAt(row);
    endRemoveRows();
}
