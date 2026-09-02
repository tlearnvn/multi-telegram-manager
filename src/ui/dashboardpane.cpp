#include "ui/dashboardpane.h"

#include "core/apppaths.h"
#include "core/formatting.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/flatbutton.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

//! Tổng dung lượng một thư mục (đệ quy).
qint64 directorySize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString stateLabel(TdAccount::State state)
{
    switch (state) {
    case TdAccount::State::Ready:            return QObject::tr("Đã đăng nhập");
    case TdAccount::State::Starting:         return QObject::tr("Đang khởi động");
    case TdAccount::State::WaitPhone:        return QObject::tr("Cần số điện thoại");
    case TdAccount::State::WaitCode:         return QObject::tr("Cần mã xác thực");
    case TdAccount::State::WaitPassword:     return QObject::tr("Cần mật khẩu 2 lớp");
    case TdAccount::State::WaitQrScan:       return QObject::tr("Chờ quét QR");
    case TdAccount::State::WaitRegistration: return QObject::tr("Cần đăng ký");
    case TdAccount::State::LoggingOut:       return QObject::tr("Đang đăng xuất");
    case TdAccount::State::Closed:           return QObject::tr("Đã đóng");
    case TdAccount::State::Failed:           return QObject::tr("Lỗi");
    case TdAccount::State::Idle:             return QObject::tr("Chưa mở");
    }
    return QString();
}

enum Column { ColAccount, ColState, ColConnection, ColChats, ColUnread, ColStorage, ColCount };

} // namespace

DashboardPane::DashboardPane(AccountManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    buildUi();

    connect(m_manager, &AccountManager::accountsChanged, this, &DashboardPane::refresh);
    connect(m_manager, &AccountManager::aggregateUnreadChanged, this,
            [this](int) { updateTotals(); });
    connect(&Theme::instance(), &Theme::changed, this, &DashboardPane::applyTheme);

    // Dung lượng thay đổi chậm, làm mới mỗi 20 giây là đủ.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(20000);
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardPane::refresh);
    m_refreshTimer->start();

    applyTheme();
    refresh();
}

void DashboardPane::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // --- Đầu trang --------------------------------------------------------
    auto *headerRow = new QHBoxLayout;
    auto *title = new QLabel(tr("Bảng điều khiển tài khoản"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    title->setFont(titleFont);
    headerRow->addWidget(title, 1);

    auto *addButton = new QPushButton(tr("Thêm tài khoản"), this);
    addButton->setProperty("accent", true);
    addButton->setIcon(Icons::icon(Icons::Name::Plus,
                                   Theme::instance().colors().textOnAccent, 18));
    auto *refreshButton = new QPushButton(tr("Làm mới"), this);
    auto *closeButton = new QPushButton(tr("Đóng"), this);
    headerRow->addWidget(addButton);
    headerRow->addWidget(refreshButton);
    headerRow->addWidget(closeButton);
    root->addLayout(headerRow);

    connect(addButton, &QPushButton::clicked, this, &DashboardPane::addAccountRequested);
    connect(refreshButton, &QPushButton::clicked, this, &DashboardPane::refresh);
    connect(closeButton, &QPushButton::clicked, this, &DashboardPane::closeRequested);

    m_totals = new QLabel(this);
    m_totals->setWordWrap(true);
    root->addWidget(m_totals);

    // --- Bảng --------------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({
        tr("Tài khoản"), tr("Trạng thái"), tr("Kết nối"),
        tr("Cuộc trò chuyện"), tr("Chưa đọc"), tr("Dung lượng")
    });
    m_table->horizontalHeader()->setSectionResizeMode(ColAccount, QHeaderView::Stretch);
    for (int column = 1; column < ColCount; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setIconSize(QSize(30, 30));
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (TdAccount *account = m_manager->accountAt(row)) {
            if (account->isReady())
                emit openAccountRequested(account);
            else
                emit loginRequested(account);
        }
    });

    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
        const int row = m_table->rowAt(position.y());
        TdAccount *account = m_manager->accountAt(row);
        if (!account)
            return;

        QMenu menu(this);
        if (account->isReady()) {
            menu.addAction(tr("Mở tài khoản này"), this,
                           [this, account] { emit openAccountRequested(account); });
        } else {
            menu.addAction(tr("Đăng nhập"), this,
                           [this, account] { emit loginRequested(account); });
        }
        menu.addAction(tr("Mở lại kết nối"), this, [this, account] {
            account->open();
            emit statusMessage(tr("Đang mở lại %1…").arg(account->displayName()));
        });
        menu.addSeparator();
        menu.addAction(tr("Dọn bộ đệm tệp"), this, [this, account] {
            account->optimizeStorage([this, account](const QJsonObject &, bool ok) {
                emit statusMessage(ok
                    ? tr("Đã dọn bộ đệm của %1.").arg(account->displayName())
                    : tr("Không dọn được bộ đệm."));
                refresh();
            });
        });
        menu.addAction(tr("Mở thư mục dữ liệu"), this, [account] {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(AppPaths::accountDir(account->slug())));
        });
        menu.addSeparator();
        if (account->isReady()) {
            menu.addAction(tr("Đăng xuất"), this, [this, account] {
                if (QMessageBox::question(this, tr("Đăng xuất"),
                        tr("Đăng xuất khỏi %1?").arg(account->displayName()))
                    == QMessageBox::Yes) {
                    account->logOut();
                }
            });
        }
        menu.addAction(tr("Xoá tài khoản khỏi ứng dụng"), this, [this, account] {
            const QString slug = account->slug();
            const QString name = account->displayName();
            if (QMessageBox::warning(this, tr("Xoá tài khoản"),
                    tr("Xoá %1 khỏi ứng dụng và xoá toàn bộ dữ liệu cục bộ?\n\n"
                       "Tài khoản Telegram của bạn không bị ảnh hưởng, chỉ mất phiên "
                       "đăng nhập trên máy này.").arg(name),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
                m_manager->removeAccount(slug, true);
                emit statusMessage(tr("Đã xoá %1.").arg(name));
            }
        });
        menu.exec(m_table->viewport()->mapToGlobal(position));
    });

    m_storageLabel = new QLabel(this);
    m_storageLabel->setWordWrap(true);
    root->addWidget(m_storageLabel);
}

void DashboardPane::refresh()
{
    const QList<TdAccount *> accounts = m_manager->accounts();
    m_table->setRowCount(accounts.size());

    for (int row = 0; row < accounts.size(); ++row) {
        TdAccount *account = accounts.at(row);

        Avatar::Options options;
        options.initials = Avatar::initialsOf(account->displayName());
        options.colorIndex = static_cast<int>(qHash(account->slug()) % 7);
        const UserEntry self = account->me();
        if (!self.photoPath.isEmpty())
            options.photoPath = self.photoPath;

        QString name = account->displayName();
        if (!account->savedPhone().isEmpty())
            name += QStringLiteral("\n%1").arg(Format::maskPhone(account->savedPhone()));

        auto *nameItem = new QTableWidgetItem(QIcon(Avatar::make(30, options)), name);
        m_table->setItem(row, ColAccount, nameItem);

        auto *stateItem = new QTableWidgetItem(stateLabel(account->state()));
        if (account->state() == TdAccount::State::Failed)
            stateItem->setForeground(Theme::instance().colors().danger);
        else if (account->state() == TdAccount::State::Ready)
            stateItem->setForeground(Theme::instance().colors().success);
        m_table->setItem(row, ColState, stateItem);

        m_table->setItem(row, ColConnection,
                         new QTableWidgetItem(connectionStateLabel(account->connectionState())));

        const int chatCount = account->orderedChatIds(false).size()
                            + account->orderedChatIds(true).size();
        auto *chatsItem = new QTableWidgetItem(QString::number(chatCount));
        chatsItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, ColChats, chatsItem);

        auto *unreadItem = new QTableWidgetItem(QString::number(account->totalUnreadChats()));
        unreadItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, ColUnread, unreadItem);

        const qint64 bytes = directorySize(AppPaths::accountDir(account->slug()));
        auto *storageItem = new QTableWidgetItem(Format::fileSize(bytes));
        storageItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, ColStorage, storageItem);
    }

    m_table->resizeRowsToContents();
    updateTotals();
}

void DashboardPane::updateTotals()
{
    const QList<TdAccount *> accounts = m_manager->accounts();
    int ready = 0;
    int unread = 0;
    for (const TdAccount *account : accounts) {
        if (account->isReady())
            ++ready;
        unread += account->totalUnreadChats();
    }

    m_totals->setText(tr("<b>%1</b> tài khoản (%2 đang đăng nhập) · "
                         "<b>%3</b> cuộc trò chuyện chưa đọc")
                          .arg(accounts.size()).arg(ready).arg(unread));

    const qint64 total = directorySize(AppPaths::dataDir());
    m_storageLabel->setText(tr("Tổng dung lượng dữ liệu: <b>%1</b> tại <code>%2</code>"
                               " — bấm phải vào một dòng để dọn bộ đệm.")
                                .arg(Format::fileSize(total),
                                     QDir::toNativeSeparators(AppPaths::dataDir())
                                         .toHtmlEscaped()));
}

void DashboardPane::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral("DashboardPane { background: %1; }").arg(c.chatBg.name()));
    m_totals->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));
    m_storageLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; border: 1px solid %2; border-radius: 10px;"
        "  gridline-color: %2; }"
        "QTableWidget::item { padding: 7px; }"
        "QTableWidget::item:selected { background: %3; color: %4; }"
        "QHeaderView::section { background: %5; color: %6; border: none;"
        "  border-bottom: 1px solid %2; padding: 8px; font-weight: 600; }")
        .arg(c.panelBg.name(), c.divider.name(), c.accent.name(),
             c.textOnAccent.name(), c.cardBg.name(), c.textSecondary.name()));
}
