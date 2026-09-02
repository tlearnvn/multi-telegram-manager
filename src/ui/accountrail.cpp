#include "ui/accountrail.h"

#include "core/formatting.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "ui/avatarpainter.h"
#include "ui/flatbutton.h"
#include "ui/theme.h"

#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {
constexpr int kRailWidth = 66;
constexpr int kButtonSize = 46;
}

// ---------------------------------------------------------------------------
//  AccountButton
// ---------------------------------------------------------------------------

AccountButton::AccountButton(TdAccount *account, bool active, QWidget *parent)
    : QWidget(parent)
    , m_account(account)
    , m_active(active)
{
    setFixedSize(kRailWidth - 8, kButtonSize + 10);
    setCursor(Qt::PointingHandCursor);
    setToolTip(buildTooltip());
}

void AccountButton::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

void AccountButton::refresh()
{
    setToolTip(buildTooltip());
    update();
}

void AccountButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const Theme::Colors &c = Theme::instance().colors();
    const QRect box((width() - kButtonSize) / 2, 5, kButtonSize, kButtonSize);

    if (m_hovered && !m_active) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.hoverBg);
        painter.drawRoundedRect(rect().adjusted(2, 1, -2, -1), 12, 12);
    }

    Avatar::Options options;
    options.initials = Avatar::initialsOf(m_account->displayName());
    options.colorIndex = static_cast<int>(qHash(m_account->slug()) % 7);
    const UserEntry self = m_account->me();
    if (!self.photoPath.isEmpty())
        options.photoPath = self.photoPath;
    if (m_active) {
        options.ringColor = QColor(m_account->accentColor());
        options.ringWidth = 2;
    }
    Avatar::paint(&painter, box, options);

    // Vạch nhỏ bên trái cho tài khoản đang chọn.
    if (m_active) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(m_account->accentColor()));
        painter.drawRoundedRect(QRect(0, box.center().y() - 12, 3, 24), 2, 2);
    }

    // Đốm trạng thái khi chưa đăng nhập xong.
    const TdAccount::State state = m_account->state();
    if (state != TdAccount::State::Ready) {
        const int dot = 13;
        const QRect dotRect(box.right() - dot + 2, box.top() - 1, dot, dot);
        QColor color = c.warning;
        if (state == TdAccount::State::Failed || state == TdAccount::State::Closed)
            color = c.danger;
        painter.setPen(QPen(c.railBg, 2));
        painter.setBrush(color);
        painter.drawEllipse(dotRect);
    }

    // Huy hiệu số cuộc trò chuyện chưa đọc.
    const int unread = m_account->totalUnreadChats();
    if (unread > 0) {
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(10);
        painter.setFont(font);

        const QString text = unread > 99 ? QStringLiteral("99+") : QString::number(unread);
        const QFontMetrics metrics(font);
        const int badgeHeight = 16;
        const int badgeWidth = qMax(badgeHeight, metrics.horizontalAdvance(text) + 9);
        const QRect badge(box.right() - badgeWidth + 4, box.bottom() - badgeHeight + 3,
                          badgeWidth, badgeHeight);

        painter.setPen(QPen(c.railBg, 2));
        painter.setBrush(c.badge);
        painter.drawRoundedRect(badge, badgeHeight / 2.0, badgeHeight / 2.0);
        painter.setPen(c.textOnAccent);
        painter.drawText(badge, Qt::AlignCenter, text);
    }
}

void AccountButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void AccountButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void AccountButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit activated(m_account);
    QWidget::mouseReleaseEvent(event);
}

void AccountButton::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextRequested(m_account, event->globalPos());
    event->accept();
}

QString AccountButton::buildTooltip() const
{
    QString status;
    switch (m_account->state()) {
    case TdAccount::State::Ready:
        status = connectionStateLabel(m_account->connectionState());
        break;
    case TdAccount::State::WaitPhone:
        status = tr("Chưa đăng nhập — cần số điện thoại");
        break;
    case TdAccount::State::WaitCode:
        status = tr("Đang chờ mã xác thực");
        break;
    case TdAccount::State::WaitPassword:
        status = tr("Đang chờ mật khẩu hai lớp");
        break;
    case TdAccount::State::WaitQrScan:
        status = tr("Đang chờ quét mã QR");
        break;
    case TdAccount::State::WaitRegistration:
        status = tr("Cần hoàn tất đăng ký");
        break;
    case TdAccount::State::Starting:
        status = tr("Đang khởi động…");
        break;
    case TdAccount::State::LoggingOut:
        status = tr("Đang đăng xuất…");
        break;
    case TdAccount::State::Closed:
        status = tr("Đã đóng");
        break;
    case TdAccount::State::Failed:
        status = tr("Lỗi: %1").arg(m_account->lastError());
        break;
    case TdAccount::State::Idle:
        status = tr("Chưa mở");
        break;
    }

    QString tip = QStringLiteral("<b>%1</b><br/>%2")
                      .arg(m_account->displayName().toHtmlEscaped(), status);
    if (!m_account->savedPhone().isEmpty())
        tip += QStringLiteral("<br/>%1").arg(Format::maskPhone(m_account->savedPhone()));
    const int unread = m_account->totalUnreadChats();
    if (unread > 0)
        tip += tr("<br/>%1 cuộc trò chuyện chưa đọc").arg(unread);
    return tip;
}

// ---------------------------------------------------------------------------
//  AccountRail
// ---------------------------------------------------------------------------

AccountRail::AccountRail(AccountManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    setFixedWidth(kRailWidth);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 8, 0, 8);
    root->setSpacing(6);

    m_buttonHost = new QWidget(this);
    m_buttonLayout = new QVBoxLayout(m_buttonHost);
    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(2);
    root->addWidget(m_buttonHost);
    root->addStretch(1);

    m_addButton = new IconButton(Icons::Name::Plus, tr("Thêm tài khoản Telegram"), 22, this);
    m_dashboardButton = new IconButton(Icons::Name::Dashboard, tr("Bảng điều khiển"), 20, this);
    m_settingsButton = new IconButton(Icons::Name::Settings, tr("Cài đặt"), 20, this);

    root->addWidget(m_addButton, 0, Qt::AlignHCenter);
    root->addWidget(m_dashboardButton, 0, Qt::AlignHCenter);
    root->addWidget(m_settingsButton, 0, Qt::AlignHCenter);

    connect(m_addButton, &QPushButton::clicked, this, &AccountRail::addAccountRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &AccountRail::settingsRequested);
    connect(m_dashboardButton, &QPushButton::clicked, this, &AccountRail::dashboardRequested);

    connect(m_manager, &AccountManager::accountsChanged, this, &AccountRail::refresh);
    connect(m_manager, &AccountManager::activeAccountChanged, this,
            [this](TdAccount *) { refresh(); });
    connect(m_manager, &AccountManager::aggregateUnreadChanged, this,
            [this](int) { refresh(); });
    connect(&Theme::instance(), &Theme::changed, this, &AccountRail::applyTheme);

    applyTheme();
    rebuildButtons();
}

void AccountRail::wireButton(AccountButton *button)
{
    connect(button, &AccountButton::activated, this, [this](TdAccount *account) {
        m_manager->setActiveAccount(account);
    });
    connect(button, &AccountButton::contextRequested, this,
            [this](TdAccount *account, const QPoint &globalPos) {
        emit accountMenuRequested(account, globalPos);
    });
}

void AccountRail::rebuildButtons()
{
    QLayoutItem *item = nullptr;
    while ((item = m_buttonLayout->takeAt(0)) != nullptr) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const QList<TdAccount *> accounts = m_manager->accounts();
    for (TdAccount *account : accounts) {
        auto *button = new AccountButton(account, account == m_manager->activeAccount(),
                                         m_buttonHost);
        wireButton(button);
        m_buttonLayout->addWidget(button, 0, Qt::AlignHCenter);
    }
}

void AccountRail::refresh()
{
    const QList<AccountButton *> buttons =
        m_buttonHost->findChildren<AccountButton *>(QString(), Qt::FindDirectChildrenOnly);

    if (buttons.size() != m_manager->count()) {
        rebuildButtons();
        return;
    }

    for (AccountButton *button : buttons) {
        button->setActive(button->account() == m_manager->activeAccount());
        button->refresh();
    }
}

void AccountRail::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral("AccountRail { background: %1; }").arg(c.railBg.name()));
}
