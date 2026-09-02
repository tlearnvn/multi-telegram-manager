#include "ui/chatpickerdialog.h"

#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kAccountRole = Qt::UserRole + 1;
constexpr int kChatRole = Qt::UserRole + 2;

} // namespace

ChatPickerDialog::ChatPickerDialog(AccountManager *manager, TdAccount *account,
                                   bool multiSelect, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_account(account)
    , m_multiSelect(multiSelect)
{
    setWindowTitle(multiSelect ? tr("Chọn các cuộc trò chuyện")
                               : tr("Chọn cuộc trò chuyện"));
    setWindowIcon(Icons::appIcon());
    resize(480, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    m_header->setText(multiSelect
        ? tr("Tích chọn nơi cần gửi. Có thể chọn nhiều nơi trên nhiều tài khoản.")
        : tr("Chọn nơi cần chuyển tiếp tin nhắn."));
    root->addWidget(m_header);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Lọc theo tên…"));
    m_search->setClearButtonEnabled(true);
    root->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(multiSelect ? QAbstractItemView::NoSelection
                                         : QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(34, 34));
    m_list->setUniformItemSizes(false);
    m_list->setSpacing(1);
    root->addWidget(m_list, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Chọn"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("accent", true);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Huỷ"));
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_search, &QLineEdit::textChanged, this, &ChatPickerDialog::reload);
    if (!multiSelect) {
        connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    }

    reload();
}

void ChatPickerDialog::setHeaderText(const QString &text)
{
    m_header->setText(text);
}

void ChatPickerDialog::reload()
{
    m_list->clear();
    const QString filter = m_search->text().trimmed();

    if (m_account) {
        addAccountChats(m_account, filter);
    } else {
        const QList<TdAccount *> accounts = m_manager->accounts();
        for (TdAccount *account : accounts) {
            if (account->isReady())
                addAccountChats(account, filter);
        }
    }

    if (m_list->count() == 0) {
        auto *empty = new QListWidgetItem(
            filter.isEmpty() ? tr("Chưa có cuộc trò chuyện nào.")
                             : tr("Không có kết quả cho “%1”.").arg(filter), m_list);
        empty->setFlags(Qt::NoItemFlags);
    }
}

void ChatPickerDialog::addAccountChats(TdAccount *account, const QString &filter)
{
    const bool showAccountName = m_account == nullptr && m_manager->count() > 1;

    const QList<qint64> chatIds = account->orderedChatIds(false);
    for (qint64 chatId : chatIds) {
        const ChatEntry *entry = account->chat(chatId);
        if (!entry || !entry->canSendMessages)
            continue;
        if (!filter.isEmpty() && !entry->title.contains(filter, Qt::CaseInsensitive))
            continue;

        Avatar::Options options;
        options.photoPath = entry->photoPath;
        options.initials = Avatar::initialsOf(entry->title);
        options.colorIndex = static_cast<int>(qAbs(entry->id) % 7);

        auto *item = new QListWidgetItem(m_list);
        item->setIcon(QIcon(Avatar::make(34, options)));
        item->setText(showAccountName
            ? QStringLiteral("%1\n%2 · %3").arg(entry->title, account->displayName(),
                                                entry->kindLabel())
            : QStringLiteral("%1\n%2").arg(entry->title, entry->kindLabel()));
        item->setData(kAccountRole, QVariant::fromValue(reinterpret_cast<quintptr>(account)));
        item->setData(kChatRole, entry->id);
        if (m_multiSelect) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    }
}

QList<ChatPickerDialog::Selection> ChatPickerDialog::selections() const
{
    QList<Selection> result;

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (!item->data(kChatRole).isValid())
            continue;

        const bool chosen = m_multiSelect
            ? item->checkState() == Qt::Checked
            : item->isSelected();
        if (!chosen)
            continue;

        Selection selection;
        selection.account = reinterpret_cast<TdAccount *>(
            item->data(kAccountRole).value<quintptr>());
        selection.chatId = item->data(kChatRole).toLongLong();
        selection.title = item->text().section(QLatin1Char('\n'), 0, 0);
        result.append(selection);
    }
    return result;
}
