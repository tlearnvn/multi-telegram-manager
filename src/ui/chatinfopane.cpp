#include "ui/chatinfopane.h"

#include "core/formatting.h"
#include "core/jsonutil.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/flatbutton.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

//! Avatar lớn ở đầu bảng thông tin.
class BigAvatar : public QWidget
{
public:
    explicit BigAvatar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(96, 96);
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

constexpr int kUserRole = Qt::UserRole + 1;

} // namespace

ChatInfoPane::ChatInfoPane(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(300);
    buildUi();
    connect(&Theme::instance(), &Theme::changed, this, &ChatInfoPane::applyTheme);
    applyTheme();
}

void ChatInfoPane::buildUi()
{
    // QWidget thuần không tự vẽ nền khai báo trong stylesheet; cờ này bật
    // việc đó lên, nếu không widget sẽ trong suốt và lộ màu nền cửa sổ.
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Thanh trên -------------------------------------------------------
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 8, 8);
    auto *headerLabel = new QLabel(tr("Thông tin"), header);
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLayout->addWidget(headerLabel, 1);
    m_closeButton = new IconButton(Icons::Name::Close, tr("Đóng bảng"), 18, header);
    headerLayout->addWidget(m_closeButton);
    root->addWidget(header);
    root->addWidget(new Separator(Qt::Horizontal, this));

    connect(m_closeButton, &QPushButton::clicked, this, &ChatInfoPane::closeRequested);

    // --- Nội dung cuộn ----------------------------------------------------
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *body = new QWidget(scroll);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_avatarBox = new BigAvatar(body);
    layout->addWidget(m_avatarBox, 0, Qt::AlignHCenter);

    m_title = new QLabel(body);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setWordWrap(true);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);

    m_subtitle = new QLabel(body);
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);

    layout->addWidget(new Separator(Qt::Horizontal, body));

    m_details = new QLabel(body);
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    m_details->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_details->setOpenExternalLinks(true);
    layout->addWidget(m_details);

    // --- Hành động ---------------------------------------------------------
    m_muteButton = new QPushButton(tr("Tắt thông báo"), body);
    m_clearButton = new QPushButton(tr("Xoá lịch sử trò chuyện"), body);
    m_blockButton = new QPushButton(tr("Chặn người này"), body);
    m_blockButton->setProperty("danger", true);
    m_leaveButton = new QPushButton(tr("Rời khỏi nhóm"), body);
    m_leaveButton->setProperty("danger", true);

    layout->addWidget(m_muteButton);
    layout->addWidget(m_clearButton);
    layout->addWidget(m_blockButton);
    layout->addWidget(m_leaveButton);

    layout->addWidget(new Separator(Qt::Horizontal, body));

    m_membersHeader = new QLabel(tr("Thành viên"), body);
    QFont membersFont = m_membersHeader->font();
    membersFont.setBold(true);
    m_membersHeader->setFont(membersFont);
    layout->addWidget(m_membersHeader);

    m_members = new QListWidget(body);
    m_members->setIconSize(QSize(30, 30));
    m_members->setMinimumHeight(180);
    layout->addWidget(m_members, 1);

    layout->addStretch(1);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    // --- Kết nối -----------------------------------------------------------
    connect(m_muteButton, &QPushButton::clicked, this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        if (const ChatEntry *entry = m_account->chat(m_chatId)) {
            m_account->setChatMuted(m_chatId, !entry->isMuted);
            emit statusMessage(entry->isMuted ? tr("Đã bật thông báo.")
                                              : tr("Đã tắt thông báo."));
        }
    });

    connect(m_clearButton, &QPushButton::clicked, this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        if (QMessageBox::question(this, tr("Xoá lịch sử"),
                tr("Xoá toàn bộ tin nhắn của cuộc trò chuyện này ở máy bạn?"))
            == QMessageBox::Yes) {
            m_account->deleteChatHistory(m_chatId, false, false);
            emit statusMessage(tr("Đã xoá lịch sử trò chuyện."));
        }
    });

    connect(m_blockButton, &QPushButton::clicked, this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        const ChatEntry *entry = m_account->chat(m_chatId);
        if (!entry || entry->relatedUserId == 0)
            return;
        const bool blocked = entry->isBlocked;
        m_account->setUserBlocked(entry->relatedUserId, !blocked);
        emit statusMessage(blocked ? tr("Đã bỏ chặn.") : tr("Đã chặn người này."));
    });

    connect(m_leaveButton, &QPushButton::clicked, this, [this] {
        if (!m_account || m_chatId == 0)
            return;
        const ChatEntry *entry = m_account->chat(m_chatId);
        if (!entry)
            return;
        if (QMessageBox::question(this, tr("Xác nhận"),
                tr("Rời khỏi “%1”?").arg(entry->title)) == QMessageBox::Yes) {
            m_account->leaveChat(m_chatId);
            emit statusMessage(tr("Đã rời khỏi cuộc trò chuyện."));
            emit closeRequested();
        }
    });

    connect(m_members, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!m_account || !item)
            return;
        const qint64 userId = item->data(kUserRole).toLongLong();
        if (userId == 0)
            return;
        QPointer<ChatInfoPane> guard(this);
        m_account->createPrivateChat(userId, [this, guard](const QJsonObject &result, bool ok) {
            if (guard && ok)
                emit openChatRequested(Json::int64(result, QStringLiteral("id")));
        });
    });
}

void ChatInfoPane::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;
    if (m_account)
        m_account->disconnect(this);

    m_account = account;
    if (m_account) {
        connect(m_account, &TdAccount::chatUpserted, this, [this](qint64 chatId) {
            if (chatId == m_chatId)
                reload();
        });
    }
    reload();
}

void ChatInfoPane::setChat(qint64 chatId)
{
    m_chatId = chatId;
    if (m_account && chatId != 0)
        m_account->requestChatDetails(chatId);
    reload();
    loadMembers();
}

void ChatInfoPane::reload()
{
    if (!m_account || m_chatId == 0) {
        m_title->setText(tr("Chưa chọn cuộc trò chuyện"));
        m_subtitle->clear();
        m_details->clear();
        m_members->clear();
        m_muteButton->hide();
        m_clearButton->hide();
        m_blockButton->hide();
        m_leaveButton->hide();
        m_membersHeader->hide();
        m_members->hide();
        return;
    }

    const ChatEntry *entry = m_account->chat(m_chatId);
    if (!entry)
        return;

    Avatar::Options options;
    options.photoPath = entry->photoPath;
    options.initials = Avatar::initialsOf(entry->title);
    options.colorIndex = static_cast<int>(qAbs(entry->id) % 7);
    static_cast<BigAvatar *>(m_avatarBox)->setOptions(options);

    m_title->setText(entry->title);
    m_subtitle->setText(entry->statusLine);

    QStringList rows;
    rows << tr("<b>Loại:</b> %1").arg(entry->kindLabel());
    if (!entry->username.isEmpty()) {
        rows << tr("<b>Tên công khai:</b> <a href='https://t.me/%1'>@%1</a>")
                    .arg(entry->username);
    }
    if (entry->relatedUserId != 0) {
        if (const UserEntry *peer = m_account->user(entry->relatedUserId)) {
            if (!peer->phoneNumber.isEmpty())
                rows << tr("<b>Điện thoại:</b> +%1").arg(peer->phoneNumber);
            if (peer->isBot)
                rows << tr("<b>Đây là bot</b>");
            if (peer->isVerified)
                rows << tr("<b>Đã xác minh</b>");
        }
    }
    if (entry->memberCount > 0)
        rows << tr("<b>Số thành viên:</b> %1").arg(entry->memberCount);
    if (entry->unreadCount > 0)
        rows << tr("<b>Chưa đọc:</b> %1 tin").arg(entry->unreadCount);
    rows << tr("<b>Mã cuộc trò chuyện:</b> <code>%1</code>").arg(entry->id);
    m_details->setText(rows.join(QStringLiteral("<br/>")));

    const bool isPrivate = entry->kind == ChatEntry::Kind::Private;
    const bool groupLike = entry->isGroupLike() || entry->kind == ChatEntry::Kind::Channel;

    m_muteButton->show();
    m_muteButton->setText(entry->isMuted ? tr("Bật thông báo") : tr("Tắt thông báo"));
    m_clearButton->show();
    m_blockButton->setVisible(isPrivate);
    m_blockButton->setText(entry->isBlocked ? tr("Bỏ chặn người này")
                                            : tr("Chặn người này"));
    m_leaveButton->setVisible(groupLike);
    m_leaveButton->setText(entry->kind == ChatEntry::Kind::Channel
                               ? tr("Rời khỏi kênh") : tr("Rời khỏi nhóm"));
    m_membersHeader->setVisible(groupLike);
    m_members->setVisible(groupLike);
}

void ChatInfoPane::loadMembers()
{
    m_members->clear();
    if (!m_account || m_chatId == 0)
        return;

    const ChatEntry *entry = m_account->chat(m_chatId);
    if (!entry || !(entry->isGroupLike() || entry->kind == ChatEntry::Kind::Channel))
        return;

    QPointer<ChatInfoPane> guard(this);
    m_account->fetchChatMembers(m_chatId, [this, guard](const QJsonObject &result, bool ok) {
        if (!guard || !ok)
            return;

        m_members->clear();

        // getSupergroupMembers trả về "members"; getBasicGroupFullInfo trả về
        // "basic_group_full_info.members".
        QJsonArray members = Json::array(result, QStringLiteral("members"));
        if (members.isEmpty()) {
            members = Json::array(Json::object(result, QStringLiteral("basic_group_full_info")),
                                  QStringLiteral("members"));
        }

        int count = 0;
        for (const QJsonValue &value : members) {
            const QJsonObject member = value.toObject();
            qint64 userId = Json::int64(member, QStringLiteral("user_id"));
            if (userId == 0) {
                userId = Json::int64(Json::object(member, QStringLiteral("member_id")),
                                     QStringLiteral("user_id"));
            }
            if (userId == 0)
                continue;

            const UserEntry *user = m_account->user(userId);
            const QString name = user ? user->displayName()
                                      : tr("Người dùng %1").arg(userId);

            Avatar::Options options;
            options.initials = Avatar::initialsOf(name);
            options.colorIndex = static_cast<int>(qAbs(userId) % 7);
            if (user)
                options.photoPath = user->photoPath;

            const QString status = Json::type(Json::object(member, QStringLiteral("status")));
            QString suffix;
            if (status == QStringLiteral("chatMemberStatusCreator"))
                suffix = tr("  · người tạo");
            else if (status == QStringLiteral("chatMemberStatusAdministrator"))
                suffix = tr("  · quản trị");

            auto *item = new QListWidgetItem(QIcon(Avatar::make(30, options)),
                                             name + suffix, m_members);
            item->setData(kUserRole, userId);
            ++count;
        }

        m_membersHeader->setText(count > 0
            ? tr("Thành viên (%1) — bấm đôi để nhắn riêng").arg(count)
            : tr("Thành viên"));
    });
}

void ChatInfoPane::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral("ChatInfoPane { background: %1; }").arg(c.panelBg.name()));
    m_subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
    m_details->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));
}
