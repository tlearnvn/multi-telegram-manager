#include "ui/newchatdialog.h"

#include "core/jsonutil.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kUserRole = Qt::UserRole + 1;
}

NewChatDialog::NewChatDialog(TdAccount *account, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
{
    setWindowTitle(tr("Trò chuyện mới"));
    setWindowIcon(Icons::appIcon());
    resize(500, 580);

    buildUi();
    loadContacts();
}

void NewChatDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs, 1);

    // --- Tab 1: nhắn cho một người ----------------------------------------
    {
        auto *page = new QWidget(m_tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        m_contactFilter = new QLineEdit(page);
        m_contactFilter->setPlaceholderText(tr("Tìm trong danh bạ, hoặc nhập @tên…"));
        m_contactFilter->setClearButtonEnabled(true);
        layout->addWidget(m_contactFilter);

        m_contactList = new QListWidget(page);
        m_contactList->setIconSize(QSize(34, 34));
        layout->addWidget(m_contactList, 1);

        auto *row = new QHBoxLayout;
        auto *start = new QPushButton(tr("Bắt đầu trò chuyện"), page);
        start->setProperty("accent", true);
        auto *findUsername = new QPushButton(tr("Tìm theo @tên"), page);
        row->addWidget(start);
        row->addWidget(findUsername);
        row->addStretch(1);
        layout->addLayout(row);

        connect(start, &QPushButton::clicked, this, &NewChatDialog::startPrivateChat);
        connect(m_contactList, &QListWidget::itemDoubleClicked,
                this, &NewChatDialog::startPrivateChat);
        connect(m_contactFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
            for (int row = 0; row < m_contactList->count(); ++row) {
                QListWidgetItem *item = m_contactList->item(row);
                item->setHidden(!text.trimmed().isEmpty()
                                && !item->text().contains(text.trimmed(), Qt::CaseInsensitive));
            }
        });
        connect(findUsername, &QPushButton::clicked, this, [this] {
            const QString query = m_contactFilter->text().trimmed();
            if (query.isEmpty()) {
                m_status->setText(tr("Nhập @tên người dùng hoặc kênh cần tìm."));
                return;
            }
            m_status->setText(tr("Đang tìm “%1”…").arg(query));
            QPointer<NewChatDialog> guard(this);
            m_account->searchByUsername(query, [this, guard](const QJsonObject &result, bool ok) {
                if (!guard)
                    return;
                if (!ok) {
                    m_status->setText(tr("Không tìm thấy: %1")
                                          .arg(Json::str(result, QStringLiteral("message"))));
                    return;
                }
                finishWith(Json::int64(result, QStringLiteral("id")),
                           tr("Đã mở cuộc trò chuyện."));
            });
        });

        m_tabs->addTab(page, tr("Nhắn cho một người"));
    }

    // --- Tab 2: nhóm mới ---------------------------------------------------
    {
        auto *page = new QWidget(m_tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        m_groupTitle = new QLineEdit(page);
        m_groupTitle->setPlaceholderText(tr("Tên nhóm"));
        m_groupTitle->setMinimumHeight(38);
        layout->addWidget(m_groupTitle);

        m_groupFilter = new QLineEdit(page);
        m_groupFilter->setPlaceholderText(tr("Lọc danh bạ…"));
        m_groupFilter->setClearButtonEnabled(true);
        layout->addWidget(m_groupFilter);

        layout->addWidget(new QLabel(tr("Tích chọn thành viên muốn thêm:"), page));
        m_groupMembers = new QListWidget(page);
        m_groupMembers->setIconSize(QSize(30, 30));
        layout->addWidget(m_groupMembers, 1);

        auto *create = new QPushButton(tr("Tạo nhóm"), page);
        create->setProperty("accent", true);
        layout->addWidget(create, 0, Qt::AlignLeft);

        connect(create, &QPushButton::clicked, this, &NewChatDialog::createGroup);
        connect(m_groupFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
            for (int row = 0; row < m_groupMembers->count(); ++row) {
                QListWidgetItem *item = m_groupMembers->item(row);
                item->setHidden(!text.trimmed().isEmpty()
                                && !item->text().contains(text.trimmed(), Qt::CaseInsensitive));
            }
        });

        m_tabs->addTab(page, tr("Nhóm mới"));
    }

    // --- Tab 3: kênh / nhóm lớn -------------------------------------------
    {
        auto *page = new QWidget(m_tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        m_channelTitle = new QLineEdit(page);
        m_channelTitle->setPlaceholderText(tr("Tên kênh"));
        m_channelTitle->setMinimumHeight(38);
        layout->addWidget(m_channelTitle);

        m_channelDescription = new QPlainTextEdit(page);
        m_channelDescription->setPlaceholderText(tr("Mô tả (không bắt buộc)"));
        m_channelDescription->setMaximumHeight(110);
        layout->addWidget(m_channelDescription);

        m_channelIsGroup = new QCheckBox(tr("Tạo nhóm lớn (supergroup) thay vì kênh"), page);
        layout->addWidget(m_channelIsGroup);

        layout->addWidget(new QLabel(tr(
            "Kênh dùng để phát thông báo một chiều; nhóm lớn cho phép mọi thành "
            "viên nhắn tin và chứa tới 200.000 người."), page));

        auto *create = new QPushButton(tr("Tạo"), page);
        create->setProperty("accent", true);
        layout->addWidget(create, 0, Qt::AlignLeft);
        layout->addStretch(1);

        connect(create, &QPushButton::clicked, this, &NewChatDialog::createChannel);

        m_tabs->addTab(page, tr("Kênh / nhóm lớn"));
    }

    // --- Tab 4: tham gia ---------------------------------------------------
    {
        auto *page = new QWidget(m_tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        layout->addWidget(new QLabel(tr("Dán liên kết mời hoặc @tên công khai:"), page));
        m_joinInput = new QLineEdit(page);
        m_joinInput->setPlaceholderText(QStringLiteral("https://t.me/… hoặc @kenh"));
        m_joinInput->setMinimumHeight(38);
        layout->addWidget(m_joinInput);

        auto *join = new QPushButton(tr("Tham gia"), page);
        join->setProperty("accent", true);
        layout->addWidget(join, 0, Qt::AlignLeft);
        layout->addStretch(1);

        connect(join, &QPushButton::clicked, this, &NewChatDialog::joinByLink);
        connect(m_joinInput, &QLineEdit::returnPressed, this, &NewChatDialog::joinByLink);

        m_tabs->addTab(page, tr("Tham gia bằng liên kết"));
    }

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral("color: %1;")
                                .arg(Theme::instance().colors().textSecondary.name()));
    root->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Đóng"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void NewChatDialog::loadContacts()
{
    if (!m_account || !m_account->isReady()) {
        m_status->setText(tr("Tài khoản chưa đăng nhập xong."));
        return;
    }

    m_status->setText(tr("Đang tải danh bạ…"));
    QPointer<NewChatDialog> guard(this);
    m_account->fetchContacts([this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            m_status->setText(tr("Không tải được danh bạ: %1")
                                  .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }

        const QList<qint64> userIds = Json::toInt64List(
            Json::array(result, QStringLiteral("user_ids")));

        m_contactList->clear();
        m_groupMembers->clear();

        for (qint64 userId : userIds) {
            const UserEntry *user = m_account->user(userId);
            if (!user)
                continue;

            Avatar::Options options;
            options.photoPath = user->photoPath;
            options.initials = Avatar::initialsOf(user->displayName());
            options.colorIndex = static_cast<int>(qAbs(userId) % 7);
            const QIcon icon(Avatar::make(34, options));

            const QString label = user->handle().isEmpty()
                ? user->displayName()
                : QStringLiteral("%1  (%2)").arg(user->displayName(), user->handle());

            auto *item = new QListWidgetItem(icon, label, m_contactList);
            item->setData(kUserRole, userId);

            auto *memberItem = new QListWidgetItem(icon, label, m_groupMembers);
            memberItem->setData(kUserRole, userId);
            memberItem->setFlags(memberItem->flags() | Qt::ItemIsUserCheckable);
            memberItem->setCheckState(Qt::Unchecked);
        }

        m_status->setText(m_contactList->count() > 0
            ? tr("Đã tải %1 liên hệ.").arg(m_contactList->count())
            : tr("Danh bạ trống — bạn vẫn có thể tìm theo @tên."));
    });
}

QList<qint64> NewChatDialog::checkedUserIds(QListWidget *list) const
{
    QList<qint64> ids;
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem *item = list->item(row);
        if (item->checkState() == Qt::Checked)
            ids.append(item->data(kUserRole).toLongLong());
    }
    return ids;
}

void NewChatDialog::finishWith(qint64 chatId, const QString &message)
{
    if (chatId == 0) {
        m_status->setText(tr("Không lấy được cuộc trò chuyện."));
        return;
    }
    m_resultChatId = chatId;
    emit statusMessage(message);
    emit chatReady(chatId);
    accept();
}

void NewChatDialog::startPrivateChat()
{
    QListWidgetItem *item = m_contactList->currentItem();
    if (!item) {
        m_status->setText(tr("Hãy chọn một liên hệ trong danh sách."));
        return;
    }

    const qint64 userId = item->data(kUserRole).toLongLong();
    m_status->setText(tr("Đang mở cuộc trò chuyện…"));
    QPointer<NewChatDialog> guard(this);
    m_account->createPrivateChat(userId, [this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            m_status->setText(tr("Lỗi: %1").arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        finishWith(Json::int64(result, QStringLiteral("id")), tr("Đã mở cuộc trò chuyện."));
    });
}

void NewChatDialog::createGroup()
{
    const QString title = m_groupTitle->text().trimmed();
    if (title.isEmpty()) {
        m_status->setText(tr("Hãy nhập tên nhóm."));
        return;
    }
    const QList<qint64> members = checkedUserIds(m_groupMembers);
    if (members.isEmpty()) {
        m_status->setText(tr("Nhóm cần ít nhất một thành viên khác."));
        return;
    }

    m_status->setText(tr("Đang tạo nhóm…"));
    QPointer<NewChatDialog> guard(this);
    m_account->createGroup(title, members, [this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            m_status->setText(tr("Không tạo được nhóm: %1")
                                  .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        finishWith(Json::int64(result, QStringLiteral("id")), tr("Đã tạo nhóm."));
    });
}

void NewChatDialog::createChannel()
{
    const QString title = m_channelTitle->text().trimmed();
    if (title.isEmpty()) {
        m_status->setText(tr("Hãy nhập tên."));
        return;
    }

    m_status->setText(tr("Đang tạo…"));
    QPointer<NewChatDialog> guard(this);
    m_account->createChannel(title, m_channelDescription->toPlainText().trimmed(),
                             m_channelIsGroup->isChecked(),
                             [this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            m_status->setText(tr("Không tạo được: %1")
                                  .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        finishWith(Json::int64(result, QStringLiteral("id")), tr("Đã tạo xong."));
    });
}

void NewChatDialog::joinByLink()
{
    const QString input = m_joinInput->text().trimmed();
    if (input.isEmpty()) {
        m_status->setText(tr("Hãy dán liên kết hoặc @tên."));
        return;
    }

    m_status->setText(tr("Đang xử lý…"));

    // @tên hay t.me/tên là kênh công khai; t.me/+xxx hay /joinchat/ là liên kết mời.
    const bool isInviteLink = input.contains(QStringLiteral("joinchat"))
                           || input.contains(QStringLiteral("t.me/+"))
                           || input.contains(QStringLiteral("t.me/ "));

    QPointer<NewChatDialog> guard(this);

    if (isInviteLink) {
        m_account->joinByInviteLink(input, [this, guard](const QJsonObject &result, bool ok) {
            if (!guard)
                return;
            if (!ok) {
                m_status->setText(tr("Không tham gia được: %1")
                                      .arg(Json::str(result, QStringLiteral("message"))));
                return;
            }
            finishWith(Json::int64(result, QStringLiteral("id")), tr("Đã tham gia."));
        });
        return;
    }

    QString username = input;
    username.remove(QStringLiteral("https://"));
    username.remove(QStringLiteral("http://"));
    username.remove(QStringLiteral("t.me/"));
    username.remove(QLatin1Char('@'));
    username = username.section(QLatin1Char('/'), 0, 0).section(QLatin1Char('?'), 0, 0);

    m_account->searchByUsername(username, [this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            m_status->setText(tr("Không tìm thấy: %1")
                                  .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        const qint64 chatId = Json::int64(result, QStringLiteral("id"));
        // Với kênh/nhóm công khai cần gọi joinChat để thực sự tham gia.
        QJsonObject payload = Json::request(QStringLiteral("joinChat"));
        payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
        m_account->request(payload, [this, guard, chatId](const QJsonObject &, bool) {
            if (guard)
                finishWith(chatId, tr("Đã mở cuộc trò chuyện."));
        });
    });
}
