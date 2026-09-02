#include "ui/chatlistpane.h"

#include "core/formatting.h"
#include "core/jsonutil.h"
#include "core/settingsstore.h"
#include "model/chatlistmodel.h"
#include "td/tdaccount.h"
#include "ui/chatlistdelegate.h"
#include "ui/flatbutton.h"
#include "ui/theme.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

ChatListPane::ChatListPane(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    connect(&Theme::instance(), &Theme::changed, this, &ChatListPane::applyTheme);
    connect(&SettingsStore::instance(), &SettingsStore::changed, this,
            [this](const QString &key) {
        if (key.startsWith(QStringLiteral("ui/"))) {
            m_delegate->setCompact(SettingsStore::instance().compactChatList());
            m_list->reset();
        }
    });
    applyTheme();
}

void ChatListPane::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Đầu cột ----------------------------------------------------------
    m_header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(10, 10, 8, 6);
    headerLayout->setSpacing(4);

    m_search = new QLineEdit(m_header);
    m_search->setPlaceholderText(tr("Tìm cuộc trò chuyện, @tên, tin nhắn…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Name::Search,
                                    Theme::instance().colors().textMuted, 16),
                        QLineEdit::LeadingPosition);
    headerLayout->addWidget(m_search, 1);

    m_newChatButton = new IconButton(Icons::Name::Plus, tr("Trò chuyện mới"), 20, m_header);
    m_menuButton = new IconButton(Icons::Name::Menu, tr("Menu"), 20, m_header);
    headerLayout->addWidget(m_newChatButton);
    headerLayout->addWidget(m_menuButton);
    root->addWidget(m_header);

    // --- Bộ lọc nhanh -----------------------------------------------------
    m_filters = new QTabBar(this);
    m_filters->setExpanding(false);
    m_filters->setDrawBase(false);
    m_filters->setUsesScrollButtons(true);
    m_filters->setFocusPolicy(Qt::NoFocus);
    const ChatFilterKind order[] = {
        ChatFilterKind::All, ChatFilterKind::Unread, ChatFilterKind::Private,
        ChatFilterKind::Groups, ChatFilterKind::Channels, ChatFilterKind::Bots,
        ChatFilterKind::Archived
    };
    for (ChatFilterKind kind : order) {
        const int index = m_filters->addTab(chatFilterLabel(kind));
        m_filters->setTabData(index, static_cast<int>(kind));
    }
    root->addWidget(m_filters);
    root->addWidget(new Separator(Qt::Horizontal, this));

    // --- Danh sách ---------------------------------------------------------
    m_model = new ChatListModel(this);
    m_delegate = new ChatListDelegate(m_model, this);
    m_delegate->setCompact(SettingsStore::instance().compactChatList());

    m_list = new QListView(this);
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_list, 1);

    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();
    root->addWidget(m_emptyLabel);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(350);

    connect(m_filters, &QTabBar::currentChanged, this, &ChatListPane::onFilterChanged);
    connect(m_search, &QLineEdit::textChanged, this, &ChatListPane::onSearchTextChanged);
    connect(m_searchTimer, &QTimer::timeout, this, &ChatListPane::runServerSearch);
    connect(m_list, &QListView::customContextMenuRequested,
            this, &ChatListPane::showContextMenu);
    connect(m_newChatButton, &QPushButton::clicked, this, &ChatListPane::newChatRequested);

    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
        const ChatEntry *entry = m_model->entryAt(current);
        if (!entry || entry->id == m_currentChatId)
            return;
        m_currentChatId = entry->id;
        emit chatSelected(entry->id);
    });

    connect(m_model, &ChatListModel::modelReset, this, [this] {
        updateEmptyState();
        if (m_currentChatId != 0) {
            const QModelIndex index = m_model->indexOfChat(m_currentChatId);
            if (index.isValid()) {
                m_list->selectionModel()->setCurrentIndex(
                    index, QItemSelectionModel::SelectCurrent);
            }
        }
    });

    // --- Menu chính -------------------------------------------------------
    auto *menu = new QMenu(this);
    menu->addAction(Icons::icon(Icons::Name::Dashboard,
                                Theme::instance().colors().textSecondary),
                    tr("Bảng điều khiển tài khoản"), this,
                    [this] { emit dashboardRequested(); });
    menu->addAction(Icons::icon(Icons::Name::Broadcast,
                                Theme::instance().colors().textSecondary),
                    tr("Gửi tin hàng loạt"), this,
                    [this] { emit broadcastRequested(); });
    menu->addSeparator();
    menu->addAction(Icons::icon(Icons::Name::Archive,
                                Theme::instance().colors().textSecondary),
                    tr("Xem cuộc trò chuyện đã lưu trữ"), this, [this] {
        for (int i = 0; i < m_filters->count(); ++i) {
            if (m_filters->tabData(i).toInt() == static_cast<int>(ChatFilterKind::Archived)) {
                m_filters->setCurrentIndex(i);
                break;
            }
        }
    });
    menu->addSeparator();
    menu->addAction(Icons::icon(Icons::Name::Settings,
                                Theme::instance().colors().textSecondary),
                    tr("Cài đặt"), this, [this] { emit settingsRequested(); });
    m_menuButton->setMenu(menu);
}

void ChatListPane::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;
    m_account = account;
    m_currentChatId = 0;
    m_model->setAccount(account);
    updateEmptyState();
}

void ChatListPane::selectChat(qint64 chatId)
{
    m_currentChatId = chatId;
    const QModelIndex index = m_model->indexOfChat(chatId);
    if (index.isValid()) {
        m_list->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);
        m_list->scrollTo(index, QAbstractItemView::EnsureVisible);
    }
}

void ChatListPane::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

void ChatListPane::clearSearch()
{
    m_search->clear();
}

void ChatListPane::goToNextUnread()
{
    if (!m_account)
        return;
    const int rows = m_model->rowCount();
    const int start = m_list->currentIndex().isValid() ? m_list->currentIndex().row() + 1 : 0;
    for (int offset = 0; offset < rows; ++offset) {
        const int row = (start + offset) % rows;
        const ChatEntry *entry = m_model->entryAtRow(row);
        if (entry && entry->hasUnread()) {
            selectChat(entry->id);
            emit chatSelected(entry->id);
            return;
        }
    }
}

void ChatListPane::onFilterChanged(int index)
{
    if (index < 0)
        return;
    const auto kind = static_cast<ChatFilterKind>(m_filters->tabData(index).toInt());
    m_model->setFilter(kind);
    updateEmptyState();
}

void ChatListPane::onSearchTextChanged(const QString &text)
{
    m_model->setSearchText(text);
    updateEmptyState();
    if (text.trimmed().size() >= 2)
        m_searchTimer->start();
    else
        m_searchTimer->stop();
}

void ChatListPane::runServerSearch()
{
    if (!m_account || !m_account->isReady())
        return;
    const QString query = m_search->text().trimmed();
    if (query.size() < 2)
        return;

    // searchChats tìm trong danh sách đã có; searchPublicChats hỏi máy chủ để
    // tìm cả kênh/nhóm công khai chưa từng mở.
    m_account->searchPublicChats(query, [this, query](const QJsonObject &result, bool ok) {
        if (!ok || m_search->text().trimmed() != query)
            return;
        const QList<qint64> ids = Json::toInt64List(
            Json::array(result, QStringLiteral("chat_ids")));
        m_model->setExtraSearchResults(ids);
        updateEmptyState();
    });
}

void ChatListPane::updateEmptyState()
{
    const bool empty = m_model->rowCount() == 0;
    m_list->setVisible(!empty);
    m_emptyLabel->setVisible(empty);

    if (!empty)
        return;

    if (!m_account) {
        m_emptyLabel->setText(tr("Chưa có tài khoản nào.\n\nBấm dấu “+” ở thanh bên trái "
                                 "để thêm tài khoản Telegram đầu tiên."));
    } else if (!m_account->isReady()) {
        m_emptyLabel->setText(tr("Tài khoản chưa đăng nhập xong.\n\n%1")
                                  .arg(connectionStateLabel(m_account->connectionState())));
    } else if (!m_search->text().trimmed().isEmpty()) {
        m_emptyLabel->setText(tr("Không tìm thấy kết quả cho “%1”.")
                                  .arg(m_search->text().trimmed()));
    } else if (m_model->filter() == ChatFilterKind::Archived) {
        m_emptyLabel->setText(tr("Chưa có cuộc trò chuyện nào trong mục lưu trữ."));
    } else if (m_model->filter() == ChatFilterKind::Unread) {
        m_emptyLabel->setText(tr("Bạn đã đọc hết tin nhắn. 🎉"));
    } else {
        m_emptyLabel->setText(tr("Đang tải danh sách trò chuyện…"));
    }
}

void ChatListPane::showContextMenu(const QPoint &position)
{
    const QModelIndex index = m_list->indexAt(position);
    const ChatEntry *entry = m_model->entryAt(index);
    if (!entry || !m_account)
        return;

    const qint64 chatId = entry->id;
    const bool muted = entry->isMuted;
    const bool pinned = entry->isPinned;
    const bool archived = entry->inArchive;
    const bool groupLike = entry->isGroupLike() || entry->kind == ChatEntry::Kind::Channel;
    const QString title = entry->title;

    QMenu menu(this);
    const Theme::Colors &c = Theme::instance().colors();

    menu.addAction(tr("Mở cuộc trò chuyện"), this, [this, chatId] {
        selectChat(chatId);
        emit chatSelected(chatId);
    });
    menu.addAction(Icons::icon(Icons::Name::Search, c.textSecondary),
                   tr("Tìm trong cuộc trò chuyện"), this,
                   [this, chatId] { emit searchInChatRequested(chatId); });
    menu.addSeparator();

    menu.addAction(Icons::icon(pinned ? Icons::Name::Unpin : Icons::Name::Pin, c.textSecondary),
                   pinned ? tr("Bỏ ghim") : tr("Ghim lên đầu"), this,
                   [this, chatId, pinned] { m_account->setChatPinned(chatId, !pinned); });
    menu.addAction(Icons::icon(muted ? Icons::Name::Bell : Icons::Name::BellOff, c.textSecondary),
                   muted ? tr("Bật thông báo") : tr("Tắt thông báo"), this,
                   [this, chatId, muted] { m_account->setChatMuted(chatId, !muted); });
    menu.addAction(Icons::icon(Icons::Name::Archive, c.textSecondary),
                   archived ? tr("Bỏ lưu trữ") : tr("Lưu trữ"), this,
                   [this, chatId, archived] { m_account->setChatArchived(chatId, !archived); });

    if (entry->hasUnread()) {
        menu.addAction(Icons::icon(Icons::Name::Check, c.textSecondary),
                       tr("Đánh dấu đã đọc"), this,
                       [this, chatId] { m_account->readAllChat(chatId); });
    } else {
        menu.addAction(tr("Đánh dấu chưa đọc"), this,
                       [this, chatId] { m_account->setChatMarkedUnread(chatId, true); });
    }

    menu.addSeparator();

    if (groupLike) {
        menu.addAction(Icons::icon(Icons::Name::Logout, c.danger),
                       tr("Rời khỏi %1").arg(entry->kind == ChatEntry::Kind::Channel
                                                 ? tr("kênh") : tr("nhóm")),
                       this, [this, chatId, title] {
            if (QMessageBox::question(this, tr("Xác nhận"),
                    tr("Rời khỏi “%1”?").arg(title)) == QMessageBox::Yes) {
                m_account->leaveChat(chatId);
            }
        });
    }

    menu.addAction(Icons::icon(Icons::Name::Trash, c.danger),
                   tr("Xoá cuộc trò chuyện"), this, [this, chatId, title] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Xoá cuộc trò chuyện"));
        box.setText(tr("Xoá toàn bộ lịch sử của “%1”?").arg(title));
        box.setInformativeText(tr("Thao tác này không thể hoàn tác."));
        QPushButton *forMe = box.addButton(tr("Chỉ xoá ở máy tôi"), QMessageBox::DestructiveRole);
        QPushButton *cancel = box.addButton(tr("Huỷ"), QMessageBox::RejectRole);
        box.setDefaultButton(cancel);
        box.exec();
        if (box.clickedButton() == forMe)
            m_account->deleteChatHistory(chatId, true, false);
    });

    menu.exec(m_list->viewport()->mapToGlobal(position));
}

void ChatListPane::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();

    setStyleSheet(QStringLiteral("ChatListPane { background: %1; }").arg(c.sidebarBg.name()));
    m_list->setStyleSheet(QStringLiteral("QListView { background: %1; }")
                              .arg(c.sidebarBg.name()));
    m_emptyLabel->setStyleSheet(QStringLiteral("color: %1; padding: 28px;")
                                    .arg(c.textMuted.name()));
    m_filters->setStyleSheet(QStringLiteral(
        "QTabBar { background: transparent; }"
        "QTabBar::tab { padding: 6px 12px; margin: 2px 3px; border-radius: 8px;"
        "  color: %1; background: transparent; }"
        "QTabBar::tab:selected { background: %2; color: %3; font-weight: 600; }"
        "QTabBar::tab:hover:!selected { background: %4; }")
        .arg(c.textSecondary.name(),
             c.accent.name(),
             c.textOnAccent.name(),
             c.hoverBg.name()));
}
