#include "ui/chatview.h"

#include "core/formatting.h"
#include "core/jsonutil.h"
#include "core/settingsstore.h"
#include "model/messagemodel.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/composer.h"
#include "ui/emojipicker.h"
#include "ui/flatbutton.h"
#include "ui/messagedelegate.h"
#include "ui/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

//! Khung tròn nhỏ vẽ avatar của cuộc trò chuyện trên thanh tiêu đề.
class HeaderAvatar : public QWidget
{
public:
    explicit HeaderAvatar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(40, 40);
    }

    void setOptions(const Avatar::Options &options)
    {
        m_options = options;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Avatar::paint(&painter, rect(), m_options);
    }

private:
    Avatar::Options m_options;
};

} // namespace

ChatView::ChatView(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    connect(&Theme::instance(), &Theme::changed, this, &ChatView::applyTheme);
    applyTheme();
    setChat(0);
}

void ChatView::buildUi()
{
    // QWidget thuần không tự vẽ nền khai báo trong stylesheet; cờ này bật
    // việc đó lên, nếu không widget sẽ trong suốt và lộ màu nền cửa sổ.
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Thanh tiêu đề ----------------------------------------------------
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("chatHeader"));
    m_header->setAttribute(Qt::WA_StyledBackground, true);
    m_header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 8, 8, 8);
    headerLayout->setSpacing(10);

    m_avatarBox = new HeaderAvatar(m_header);
    headerLayout->addWidget(m_avatarBox);

    auto *titleColumn = new QVBoxLayout;
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(1);
    m_title = new QLabel(m_header);
    m_subtitle = new QLabel(m_header);
    titleColumn->addWidget(m_title);
    titleColumn->addWidget(m_subtitle);
    headerLayout->addLayout(titleColumn, 1);

    m_searchButton = new IconButton(Icons::Name::Search, tr("Tìm trong cuộc trò chuyện"), 20, m_header);
    m_infoButton = new IconButton(Icons::Name::Info, tr("Thông tin"), 20, m_header);
    m_menuButton = new IconButton(Icons::Name::Menu, tr("Tuỳ chọn"), 20, m_header);
    headerLayout->addWidget(m_searchButton);
    headerLayout->addWidget(m_infoButton);
    headerLayout->addWidget(m_menuButton);
    root->addWidget(m_header);
    root->addWidget(new Separator(Qt::Horizontal, this));

    // --- Thanh tìm kiếm trong chat ----------------------------------------
    m_searchBar = new QWidget(this);
    m_searchBar->setObjectName(QStringLiteral("chatSearchBar"));
    m_searchBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *searchLayout = new QHBoxLayout(m_searchBar);
    searchLayout->setContentsMargins(12, 6, 8, 6);
    searchLayout->setSpacing(6);
    m_searchInput = new QLineEdit(m_searchBar);
    m_searchInput->setPlaceholderText(tr("Từ khoá cần tìm trong cuộc trò chuyện này…"));
    m_searchStatus = new QLabel(m_searchBar);
    m_searchPrev = new IconButton(Icons::Name::ChevronDown, tr("Kết quả cũ hơn"), 18, m_searchBar);
    m_searchNext = new IconButton(Icons::Name::ChevronUp, tr("Kết quả mới hơn"), 18, m_searchBar);
    auto *searchClose = new IconButton(Icons::Name::Close, tr("Đóng"), 18, m_searchBar);
    searchLayout->addWidget(m_searchInput, 1);
    searchLayout->addWidget(m_searchStatus);
    searchLayout->addWidget(m_searchPrev);
    searchLayout->addWidget(m_searchNext);
    searchLayout->addWidget(searchClose);
    m_searchBar->hide();
    root->addWidget(m_searchBar);

    // --- Thanh chọn nhiều tin ---------------------------------------------
    m_selectionBar = new QWidget(this);
    m_selectionBar->setObjectName(QStringLiteral("chatSelectionBar"));
    m_selectionBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *selectionLayout = new QHBoxLayout(m_selectionBar);
    selectionLayout->setContentsMargins(12, 6, 8, 6);
    selectionLayout->setSpacing(8);
    m_selectionLabel = new QLabel(m_selectionBar);
    auto *forwardSelected = new IconButton(Icons::Name::Forward, tr("Chuyển tiếp"), 18, m_selectionBar);
    auto *copySelected = new IconButton(Icons::Name::Copy, tr("Chép"), 18, m_selectionBar);
    auto *deleteSelected = new IconButton(Icons::Name::Trash, tr("Xoá"), 18, m_selectionBar);
    deleteSelected->setDanger(true);
    auto *clearSelection = new IconButton(Icons::Name::Close, tr("Bỏ chọn"), 18, m_selectionBar);
    selectionLayout->addWidget(m_selectionLabel, 1);
    selectionLayout->addWidget(forwardSelected);
    selectionLayout->addWidget(copySelected);
    selectionLayout->addWidget(deleteSelected);
    selectionLayout->addWidget(clearSelection);
    m_selectionBar->hide();
    root->addWidget(m_selectionBar);

    // --- Thân: danh sách tin nhắn + ô soạn --------------------------------
    m_body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_model = new MessageModel(this);
    m_delegate = new MessageDelegate(m_model, this);
    m_delegate->setShowGroupAvatars(SettingsStore::instance().showAvatarsInGroups());

    m_list = new QListView(m_body);
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMouseTracking(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setWordWrap(true);
    m_delegate->setView(m_list);
    bodyLayout->addWidget(m_list, 1);

    m_composer = new Composer(m_body);
    bodyLayout->addWidget(m_composer);
    root->addWidget(m_body, 1);

    // --- Trạng thái rỗng ---------------------------------------------------
    m_placeholder = new QLabel(this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    root->addWidget(m_placeholder, 1);

    // --- Nút cuộn xuống cuối ----------------------------------------------
    m_scrollDownButton = new QPushButton(m_list);
    m_scrollDownButton->setCursor(Qt::PointingHandCursor);
    m_scrollDownButton->setFixedSize(38, 38);
    m_scrollDownButton->setToolTip(tr("Xuống tin nhắn mới nhất"));
    m_scrollDownButton->hide();

    m_emoji = new EmojiPicker(this);

    m_readTimer = new QTimer(this);
    m_readTimer->setSingleShot(true);
    m_readTimer->setInterval(600);

    // --- Kết nối ----------------------------------------------------------
    connect(m_composer, &Composer::sendRequested, this, &ChatView::onSend);
    connect(m_composer, &Composer::editSubmitted, this, &ChatView::onEditSubmitted);
    connect(m_composer, &Composer::filesRequested, this, &ChatView::onFiles);
    connect(m_composer, &Composer::typingActive, this, [this] {
        if (m_account && m_chatId != 0)
            m_account->sendChatAction(m_chatId, true);
    });
    connect(m_composer, &Composer::emojiPanelRequested, this, [this] {
        m_emoji->popupAbove(m_composer);
    });
    connect(m_emoji, &EmojiPicker::emojiChosen, this, [this](const QString &emoji) {
        m_composer->setText(m_composer->text() + emoji);
        m_composer->focusInput();
    });

    connect(m_list, &QListView::clicked, this, &ChatView::onListClicked);
    connect(m_list, &QListView::doubleClicked, this, &ChatView::onListDoubleClicked);
    connect(m_list, &QListView::customContextMenuRequested, this, &ChatView::showMessageMenu);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatView::onScrolled);
    connect(m_model, &MessageModel::newestMessageAppended, this, &ChatView::onNewestAppended);
    connect(m_model, &MessageModel::olderMessagesInserted, this, &ChatView::onOlderInserted);
    connect(m_readTimer, &QTimer::timeout, this, &ChatView::markVisibleAsRead);

    connect(m_searchButton, &QPushButton::clicked, this, &ChatView::openSearchBar);
    connect(searchClose, &QPushButton::clicked, this, [this] {
        m_searchBar->hide();
        m_searchHits.clear();
        m_searchCursor = -1;
    });
    connect(m_searchInput, &QLineEdit::returnPressed, this, &ChatView::runInChatSearch);
    connect(m_searchNext, &QPushButton::clicked, this, [this] {
        if (m_searchHits.isEmpty())
            return;
        m_searchCursor = qMax(0, m_searchCursor - 1);
        jumpToMessage(m_searchHits.at(m_searchCursor));
        m_searchStatus->setText(tr("%1/%2").arg(m_searchCursor + 1).arg(m_searchHits.size()));
    });
    connect(m_searchPrev, &QPushButton::clicked, this, [this] {
        if (m_searchHits.isEmpty())
            return;
        m_searchCursor = qMin(m_searchHits.size() - 1, m_searchCursor + 1);
        jumpToMessage(m_searchHits.at(m_searchCursor));
        m_searchStatus->setText(tr("%1/%2").arg(m_searchCursor + 1).arg(m_searchHits.size()));
    });

    connect(m_infoButton, &QPushButton::clicked, this,
            [this] { emit chatInfoRequested(m_chatId); });

    connect(m_scrollDownButton, &QPushButton::clicked, this, [this] {
        m_list->scrollToBottom();
        m_scrollDownButton->hide();
    });

    connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        const int count = m_list->selectionModel()->selectedIndexes().size();
        if (count > 1) {
            m_selectionLabel->setText(tr("Đã chọn %1 tin nhắn").arg(count));
            m_selectionBar->show();
        } else {
            m_selectionBar->hide();
        }
    });
    connect(clearSelection, &QPushButton::clicked, this, [this] {
        m_list->clearSelection();
        m_selectionBar->hide();
    });
    connect(copySelected, &QPushButton::clicked, this, &ChatView::copySelection);
    connect(deleteSelected, &QPushButton::clicked, this, &ChatView::deleteSelection);
    connect(forwardSelected, &QPushButton::clicked, this, [this] {
        const QList<qint64> ids = selectedMessageIds();
        if (!ids.isEmpty())
            emit forwardRequested(m_chatId, ids);
    });

    connect(m_delegate, &MessageDelegate::downloadRequested, this, [this](int fileId) {
        if (m_account)
            m_account->downloadFile(fileId);
    });

    connect(&SettingsStore::instance(), &SettingsStore::changed, this,
            [this](const QString &key) {
        if (key == QStringLiteral("ui/group_avatars")) {
            m_delegate->setShowGroupAvatars(SettingsStore::instance().showAvatarsInGroups());
            m_list->reset();
        }
    });

    // --- Menu tiêu đề -----------------------------------------------------
    auto *menu = new QMenu(this);
    menu->addAction(tr("Thông tin cuộc trò chuyện"), this,
                    [this] { emit chatInfoRequested(m_chatId); });
    menu->addAction(tr("Đánh dấu đã đọc hết"), this, [this] {
        if (m_account && m_chatId != 0)
            m_account->readAllChat(m_chatId);
    });
    menu->addSeparator();
    menu->addAction(tr("Tắt / bật thông báo"), this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        if (const ChatEntry *entry = m_account->chat(m_chatId))
            m_account->setChatMuted(m_chatId, !entry->isMuted);
    });
    menu->addAction(tr("Lưu trữ cuộc trò chuyện"), this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        if (const ChatEntry *entry = m_account->chat(m_chatId))
            m_account->setChatArchived(m_chatId, !entry->inArchive);
    });
    menu->addSeparator();
    menu->addAction(tr("Xoá lịch sử trò chuyện"), this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        if (QMessageBox::question(this, tr("Xoá lịch sử"),
                tr("Xoá toàn bộ tin nhắn trong cuộc trò chuyện này ở máy bạn?"))
            == QMessageBox::Yes) {
            m_account->deleteChatHistory(m_chatId, false, false);
        }
    });
    m_menuButton->setMenu(menu);
}

void ChatView::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;

    if (m_account) {
        m_account->disconnect(this);
        if (m_chatId != 0)
            m_account->closeChatSession(m_chatId);
    }

    m_account = account;
    m_model->setAccount(account);
    m_chatId = 0;
    m_model->setChat(0);

    if (m_account) {
        connect(m_account, &TdAccount::chatUpserted, this, [this](qint64 chatId) {
            if (chatId == m_chatId)
                refreshHeader();
        });
        connect(m_account, &TdAccount::chatActionChanged, this, [this](qint64 chatId) {
            if (chatId == m_chatId)
                refreshHeader();
        });
    }

    setChat(0);
}

void ChatView::setChat(qint64 chatId)
{
    // Chọn lại đúng cuộc trò chuyện đang mở thì không dựng lại gì cả, nếu không
    // nội dung đang soạn dở sẽ bị xoá.
    if (chatId != 0 && chatId == m_chatId && m_account) {
        m_composer->focusInput();
        return;
    }

    if (m_account && m_chatId != 0 && m_chatId != chatId) {
        m_account->saveDraft(m_chatId, m_composer->text(), m_composer->replyTarget());
        m_account->closeChatSession(m_chatId);
        m_account->sendChatAction(m_chatId, false);
    }

    m_chatId = chatId;
    m_searchHits.clear();
    m_searchCursor = -1;
    m_searchBar->hide();
    m_selectionBar->hide();
    m_composer->setReplyTarget(0, QString(), QString());
    m_composer->setEditTarget(0, QString());
    m_composer->clearInput();

    // Chỉ mở khung hội thoại khi thực sự biết cuộc trò chuyện này; nếu không sẽ
    // hiện một khung trống không tiêu đề.
    const bool hasChat = chatId != 0 && m_account != nullptr
                      && m_account->chat(chatId) != nullptr;
    m_header->setVisible(hasChat);
    m_body->setVisible(hasChat);
    m_placeholder->setVisible(!hasChat);

    if (!hasChat) {
        if (!m_account)
            m_placeholder->setText(tr("Thêm tài khoản Telegram để bắt đầu nhắn tin."));
        else if (chatId != 0)
            m_placeholder->setText(tr("Đang tải cuộc trò chuyện…"));
        else
            m_placeholder->setText(
                tr("Chọn một cuộc trò chuyện ở danh sách bên trái để bắt đầu."));
        m_model->setChat(0);
        return;
    }

    m_account->openChatSession(chatId);
    m_model->setChat(chatId);

    if (const ChatEntry *entry = m_account->chat(chatId)) {
        if (!entry->draftText.isEmpty())
            m_composer->setText(entry->draftText);
    }

    refreshHeader();
    updateComposerState();
    m_composer->focusInput();
}

void ChatView::refreshHeader()
{
    if (!m_account || m_chatId == 0)
        return;
    const ChatEntry *entry = m_account->chat(m_chatId);
    if (!entry)
        return;

    m_title->setText(entry->title);

    QString subtitle = entry->actionText.isEmpty() ? entry->statusLine : entry->actionText;
    if (subtitle.isEmpty())
        subtitle = entry->kindLabel();
    m_subtitle->setText(subtitle);

    const Theme::Colors &c = Theme::instance().colors();
    m_subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(
        entry->actionText.isEmpty() ? c.textSecondary.name() : c.accent.name()));

    Avatar::Options options;
    options.photoPath = entry->photoPath;
    options.initials = Avatar::initialsOf(entry->title);
    options.colorIndex = static_cast<int>(qAbs(entry->id) % 7);
    if (entry->kind == ChatEntry::Kind::Private) {
        if (const UserEntry *peer = m_account->user(entry->relatedUserId)) {
            options.showOnlineDot = !peer->isBot;
            options.online = peer->presence == UserEntry::Presence::Online;
        }
    }
    static_cast<HeaderAvatar *>(m_avatarBox)->setOptions(options);
}

void ChatView::updateComposerState()
{
    if (!m_account || m_chatId == 0)
        return;
    const ChatEntry *entry = m_account->chat(m_chatId);
    if (!entry)
        return;

    if (!entry->canSendMessages) {
        m_composer->setBlockedReason(entry->kind == ChatEntry::Kind::Channel
            ? tr("Bạn chỉ có thể đọc kênh này.")
            : tr("Bạn không có quyền gửi tin nhắn ở đây."));
    } else if (entry->isBlocked) {
        m_composer->setBlockedReason(tr("Bạn đã chặn người này. Bỏ chặn để nhắn tin lại."));
    } else {
        m_composer->setBlockedReason(QString());
        m_composer->setPlaceholder(entry->kind == ChatEntry::Kind::Channel
            ? tr("Đăng vào kênh…") : tr("Nhắn gì đó…"));
    }
}

void ChatView::focusComposer()
{
    m_composer->focusInput();
}

void ChatView::openSearchBar()
{
    if (m_chatId == 0)
        return;
    m_searchBar->show();
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void ChatView::runInChatSearch()
{
    if (!m_account || m_chatId == 0)
        return;
    const QString query = m_searchInput->text().trimmed();
    if (query.isEmpty())
        return;

    m_searchStatus->setText(tr("đang tìm…"));
    QPointer<ChatView> guard(this);
    m_account->searchMessagesInChat(m_chatId, query,
                                    [this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        m_searchHits.clear();
        m_searchCursor = -1;
        if (!ok) {
            m_searchStatus->setText(tr("lỗi tìm kiếm"));
            return;
        }
        const QJsonArray messages = Json::array(result, QStringLiteral("messages"));
        for (const QJsonValue &value : messages)
            m_searchHits.append(Json::int64(value.toObject(), QStringLiteral("id")));

        if (m_searchHits.isEmpty()) {
            m_searchStatus->setText(tr("không có kết quả"));
            return;
        }
        m_searchCursor = 0;
        m_searchStatus->setText(tr("1/%1").arg(m_searchHits.size()));
        jumpToMessage(m_searchHits.first());
    });
}

void ChatView::jumpToMessage(qint64 messageId)
{
    QModelIndex index = m_model->indexOfMessage(messageId);
    if (!index.isValid()) {
        // Chưa nạp tới đoạn đó — tải lịch sử quanh tin nhắn cần tới.
        if (m_account)
            m_account->loadHistory(m_chatId, messageId, 30);
        emit statusMessage(tr("Đang tải phần lịch sử chứa tin nhắn…"));
        return;
    }
    m_list->setCurrentIndex(index);
    m_list->scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void ChatView::onSend(const QString &text)
{
    if (!m_account || m_chatId == 0)
        return;
    m_account->sendText(m_chatId, text, m_composer->replyTarget());
    m_composer->setReplyTarget(0, QString(), QString());
    m_account->sendChatAction(m_chatId, false);
    m_list->scrollToBottom();
}

void ChatView::onEditSubmitted(qint64 messageId, const QString &text)
{
    if (m_account && m_chatId != 0)
        m_account->editMessageText(m_chatId, messageId, text);
}

void ChatView::onFiles(const QStringList &paths)
{
    if (!m_account || m_chatId == 0)
        return;
    const qint64 reply = m_composer->replyTarget();
    const QString caption = m_composer->text().trimmed();
    for (int i = 0; i < paths.size(); ++i) {
        // Chú thích chỉ gắn vào tệp đầu tiên, giống Telegram.
        m_account->sendFile(m_chatId, paths.at(i), i == 0 ? caption : QString(), reply);
    }
    m_composer->clearInput();
    m_composer->setReplyTarget(0, QString(), QString());
    emit statusMessage(tr("Đang gửi %1 tệp…").arg(paths.size()));
}

void ChatView::onListClicked(const QModelIndex &index)
{
    const MessageEntry *entry = m_model->entryAt(index);
    if (!entry || !m_account)
        return;

    QStyleOptionViewItem option;
    option.initFrom(m_list);
    option.rect = m_list->visualRect(index);
    option.font = m_list->font();

    const QPoint position = m_list->viewport()->mapFromGlobal(QCursor::pos());
    const MessageDelegate::Hit hit = m_delegate->hitTest(option, index, position);

    switch (hit.kind) {
    case MessageDelegate::HitKind::Link:
        if (!hit.link.isEmpty()) {
            QUrl url(hit.link);
            if (url.scheme().isEmpty())
                url = QUrl(QStringLiteral("https://") + hit.link);
            QDesktopServices::openUrl(url);
        }
        break;
    case MessageDelegate::HitKind::ReplyQuote:
        if (hit.replyToMessageId != 0)
            jumpToMessage(hit.replyToMessageId);
        break;
    case MessageDelegate::HitKind::Media:
    case MessageDelegate::HitKind::FileAction:
        openMedia(*entry);
        break;
    default:
        break;
    }
}

void ChatView::onListDoubleClicked(const QModelIndex &index)
{
    const MessageEntry *entry = m_model->entryAt(index);
    if (!entry)
        return;
    m_composer->setReplyTarget(entry->id, entry->senderName,
                               Format::oneLine(entry->text.isEmpty() ? entry->kindLabel()
                                                                     : entry->text, 90));
}

void ChatView::openMedia(const MessageEntry &entry)
{
    if (!m_account)
        return;

    if (entry.isDownloaded && !entry.mediaPath.isEmpty()
        && QFileInfo::exists(entry.mediaPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(entry.mediaPath));
        return;
    }
    if (entry.mediaFileId != 0) {
        m_account->downloadFile(entry.mediaFileId, 32);
        emit statusMessage(tr("Đang tải %1…").arg(entry.fileName.isEmpty()
                                                      ? entry.kindLabel() : entry.fileName));
    }
}

void ChatView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionScrollDownButton();
}

void ChatView::positionScrollDownButton()
{
    if (!m_scrollDownButton->isVisible())
        return;
    m_scrollDownButton->move(qMax(0, m_list->viewport()->width() - 54),
                             qMax(0, m_list->viewport()->height() - 54));
}

void ChatView::onScrolled(int value)
{
    QScrollBar *bar = m_list->verticalScrollBar();

    // Gần đỉnh → nạp thêm lịch sử cũ. Ghi lại tin nhắn đang ở đỉnh để sau khi
    // chèn còn cuộn về đúng chỗ.
    if (value <= bar->minimum() + 40 && !m_model->isLoading()) {
        const QModelIndex top = m_list->indexAt(QPoint(4, 4));
        if (const MessageEntry *entry = m_model->entryAt(top)) {
            m_anchorMessageId = entry->id;
            m_anchorOffset = m_list->visualRect(top).top();
        }
        m_model->loadOlder();
    }

    m_scrollDownButton->setVisible(!isNearBottom() && m_model->rowCount() > 0);
    positionScrollDownButton();

    m_readTimer->start();
}

bool ChatView::isNearBottom() const
{
    QScrollBar *bar = m_list->verticalScrollBar();
    return bar->value() >= bar->maximum() - 120;
}

void ChatView::onNewestAppended()
{
    if (isNearBottom())
        QTimer::singleShot(0, this, [this] { m_list->scrollToBottom(); });
    m_readTimer->start();
}

void ChatView::onOlderInserted(int count)
{
    Q_UNUSED(count)

    // Sau khi chèn vào đầu, giá trị thanh cuộn cũ trỏ vào nội dung khác nên
    // khung sẽ nhảy. Cuộn lại về đúng tin nhắn vừa neo.
    if (m_anchorMessageId == 0)
        return;

    const qint64 anchorId = m_anchorMessageId;
    const int anchorOffset = m_anchorOffset;
    m_anchorMessageId = 0;

    QTimer::singleShot(0, this, [this, anchorId, anchorOffset] {
        const QModelIndex index = m_model->indexOfMessage(anchorId);
        if (!index.isValid())
            return;
        m_list->scrollTo(index, QAbstractItemView::PositionAtTop);
        if (anchorOffset != 0) {
            QScrollBar *bar = m_list->verticalScrollBar();
            bar->setValue(bar->value() - anchorOffset);
        }
    });
}

void ChatView::markVisibleAsRead()
{
    if (!m_account || m_chatId == 0 || !isVisible())
        return;

    const QModelIndex first = m_list->indexAt(QPoint(4, 4));
    const QModelIndex last = m_list->indexAt(
        QPoint(4, m_list->viewport()->height() - 4));
    const int firstRow = first.isValid() ? first.row() : 0;
    const int lastRow = last.isValid() ? last.row() : m_model->rowCount() - 1;

    const QList<qint64> ids = m_model->messageIdsInRange(firstRow, lastRow);
    if (!ids.isEmpty())
        m_account->viewMessages(m_chatId, ids);
}

QList<qint64> ChatView::selectedMessageIds() const
{
    QList<qint64> ids;
    const QModelIndexList selection = m_list->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : selection) {
        if (const MessageEntry *entry = m_model->entryAt(index))
            ids.append(entry->id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void ChatView::copySelection()
{
    QStringList lines;
    const QModelIndexList selection = m_list->selectionModel()->selectedIndexes();
    QList<QModelIndex> sorted = selection;
    std::sort(sorted.begin(), sorted.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    for (const QModelIndex &index : sorted) {
        if (const MessageEntry *entry = m_model->entryAt(index)) {
            lines << QStringLiteral("[%1] %2: %3")
                        .arg(Format::clock(entry->date),
                             entry->senderName.isEmpty() ? tr("Bạn") : entry->senderName,
                             entry->text);
        }
    }
    if (lines.isEmpty())
        return;
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    emit statusMessage(tr("Đã chép %1 tin nhắn.").arg(lines.size()));
}

void ChatView::deleteSelection()
{
    if (!m_account || m_chatId == 0)
        return;
    const QList<qint64> ids = selectedMessageIds();
    if (ids.isEmpty())
        return;

    bool canRevokeAll = true;
    for (qint64 id : ids) {
        const MessageEntry *entry = m_account->cachedMessage(m_chatId, id);
        if (!entry || !entry->canBeDeletedForAll) {
            canRevokeAll = false;
            break;
        }
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Xoá tin nhắn"));
    box.setText(tr("Xoá %1 tin nhắn đã chọn?").arg(ids.size()));
    QPushButton *everyone = canRevokeAll
        ? box.addButton(tr("Xoá ở mọi người"), QMessageBox::DestructiveRole) : nullptr;
    QPushButton *mine = box.addButton(tr("Chỉ xoá ở máy tôi"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(tr("Huỷ"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.exec();

    if (box.clickedButton() == cancel)
        return;
    m_account->deleteMessages(m_chatId, ids, box.clickedButton() == everyone);
    m_list->clearSelection();
    m_selectionBar->hide();
    Q_UNUSED(mine)
}

void ChatView::showMessageMenu(const QPoint &position)
{
    const QModelIndex index = m_list->indexAt(position);
    const MessageEntry *entryPtr = m_model->entryAt(index);
    if (!entryPtr || !m_account)
        return;

    const MessageEntry entry = *entryPtr;
    const Theme::Colors &c = Theme::instance().colors();
    QMenu menu(this);

    menu.addAction(Icons::icon(Icons::Name::Reply, c.textSecondary), tr("Trả lời"), this,
                   [this, entry] {
        m_composer->setReplyTarget(entry.id, entry.senderName,
                                   Format::oneLine(entry.text.isEmpty() ? entry.kindLabel()
                                                                        : entry.text, 90));
    });

    if (entry.canBeForwarded) {
        menu.addAction(Icons::icon(Icons::Name::Forward, c.textSecondary),
                       tr("Chuyển tiếp"), this, [this, entry] {
            emit forwardRequested(m_chatId, { entry.id });
        });
    }

    if (!entry.text.isEmpty()) {
        menu.addAction(Icons::icon(Icons::Name::Copy, c.textSecondary),
                       tr("Chép nội dung"), this, [this, entry] {
            QApplication::clipboard()->setText(entry.text);
            emit statusMessage(tr("Đã chép nội dung tin nhắn."));
        });
    }

    if (entry.canBeEdited && entry.kind == MessageEntry::Kind::Text) {
        menu.addAction(Icons::icon(Icons::Name::Edit, c.textSecondary), tr("Sửa"), this,
                       [this, entry] { m_composer->setEditTarget(entry.id, entry.text); });
    }

    menu.addAction(Icons::icon(entry.isPinned ? Icons::Name::Unpin : Icons::Name::Pin,
                               c.textSecondary),
                   entry.isPinned ? tr("Bỏ ghim") : tr("Ghim tin nhắn"), this,
                   [this, entry] { m_account->pinMessage(m_chatId, entry.id, !entry.isPinned); });

    if (entry.hasMedia()) {
        menu.addSeparator();
        if (entry.isDownloaded && !entry.mediaPath.isEmpty()) {
            menu.addAction(Icons::icon(Icons::Name::Folder, c.textSecondary),
                           tr("Mở thư mục chứa tệp"), this, [this, entry] {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(entry.mediaPath).absolutePath()));
            });
            menu.addAction(Icons::icon(Icons::Name::Play, c.textSecondary),
                           tr("Mở tệp"), this, [this, entry] {
                QDesktopServices::openUrl(QUrl::fromLocalFile(entry.mediaPath));
            });
        } else {
            menu.addAction(Icons::icon(Icons::Name::Download, c.textSecondary),
                           tr("Tải về"), this, [this, entry] {
                m_account->downloadFile(entry.mediaFileId, 32);
            });
        }
    }

    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::Name::Trash, c.danger), tr("Xoá tin nhắn"), this,
                   [this, index] {
        m_list->clearSelection();
        m_list->selectionModel()->select(index, QItemSelectionModel::Select);
        deleteSelection();
    });

    menu.exec(m_list->viewport()->mapToGlobal(position));
}

void ChatView::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();

    setStyleSheet(QStringLiteral("ChatView { background: %1; }").arg(c.chatBg.name()));
    m_header->setStyleSheet(QStringLiteral("#chatHeader { background: %1;"
                                           " border-bottom: 1px solid %2; }")
                                .arg(c.sidebarBg.name(), c.divider.name()));
    m_list->setStyleSheet(QStringLiteral("QListView { background: %1; }")
                              .arg(c.chatBg.name()));

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(font().pointSizeF() * 1.06);
    m_title->setFont(titleFont);
    m_title->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                               .arg(c.textPrimary.name()));

    QFont subtitleFont = m_subtitle->font();
    subtitleFont.setPointSizeF(font().pointSizeF() * 0.88);
    m_subtitle->setFont(subtitleFont);
    m_subtitle->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                  .arg(c.textSecondary.name()));

    m_placeholder->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; padding: 40px;")
                                     .arg(c.textMuted.name()));
    m_searchBar->setStyleSheet(QStringLiteral("#chatSearchBar { background: %1; }")
                                   .arg(c.panelBg.name()));
    m_selectionBar->setStyleSheet(QStringLiteral("#chatSelectionBar { background: %1; }")
                                      .arg(c.cardBg.name()));
    m_selectionLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                        .arg(c.textPrimary.name()));
    m_searchStatus->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                      .arg(c.textSecondary.name()));

    m_scrollDownButton->setIcon(Icons::icon(Icons::Name::ChevronDown, c.accent, 20));
    m_scrollDownButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 19px; }"
        "QPushButton:hover { background: %3; }")
        .arg(c.cardBg.name(), c.divider.name(), c.hoverBg.name()));
}
