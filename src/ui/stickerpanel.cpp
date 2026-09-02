#include "ui/stickerpanel.h"

#include "core/jsonutil.h"
#include "td/filecache.h"
#include "td/tdaccount.h"
#include "ui/theme.h"

#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace {
constexpr int kColumns = 5;
constexpr int kCell = 76;
}

// ---------------------------------------------------------------------------
//  StickerButton
// ---------------------------------------------------------------------------

StickerButton::StickerButton(const Info &info, QWidget *parent)
    : QWidget(parent)
    , m_info(info)
{
    setFixedSize(kCell, kCell);
    setCursor(Qt::PointingHandCursor);
    setToolTip(info.emoji);
}

void StickerButton::setThumbPath(const QString &path)
{
    if (m_info.thumbPath == path)
        return;
    m_info.thumbPath = path;
    update();
}

void StickerButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_hovered) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::instance().colors().hoverBg);
        painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 10, 10);
    }

    const QRect box = rect().adjusted(6, 6, -6, -6);
    const QPixmap image = m_info.thumbPath.isEmpty()
        ? QPixmap()
        : FileCache::instance().scaled(m_info.thumbPath, box.size() * 2);

    if (!image.isNull()) {
        QRect target(QPoint(0, 0), image.size().scaled(box.size(), Qt::KeepAspectRatio));
        target.moveCenter(box.center());
        painter.drawPixmap(target, image);
        return;
    }

    // Chưa có ảnh thu nhỏ (đang tải, hoặc là nhãn dán động) — hiện emoji.
    QFont font = painter.font();
    font.setPixelSize(box.height() / 2);
    painter.setFont(font);
    painter.setPen(Theme::instance().colors().textSecondary);
    painter.drawText(box, Qt::AlignCenter,
                     m_info.emoji.isEmpty() ? QStringLiteral("🙂") : m_info.emoji);
}

void StickerButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void StickerButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void StickerButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit chosen(m_info);
    QWidget::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
//  StickerPanel
// ---------------------------------------------------------------------------

StickerPanel::StickerPanel(QWidget *parent)
    : QWidget(parent, Qt::Popup)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(kColumns * kCell + 30, 340);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    m_tabs = new QTabBar(this);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    m_tabs->addTab(tr("Gần đây"));
    m_tabs->addTab(tr("Yêu thích"));
    root->addWidget(m_tabs);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildGrid({}, tr("Đang tải nhãn dán…")));
    m_pages->addWidget(buildGrid({}, tr("Đang tải nhãn dán…")));
    root->addWidget(m_pages, 1);

    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(&Theme::instance(), &Theme::changed, this, &StickerPanel::applyTheme);
    applyTheme();
}

void StickerPanel::setAccount(TdAccount *account)
{
    if (m_account == account)
        return;
    if (m_account)
        m_account->disconnect(this);

    m_account = account;
    if (m_account)
        connect(m_account, &TdAccount::fileReady, this, &StickerPanel::onFileReady);
}

QWidget *StickerPanel::buildGrid(const QList<StickerButton::Info> &stickers,
                                 const QString &emptyText)
{
    auto *scroll = new QScrollArea(m_pages);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *page = new QWidget(scroll);
    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    if (stickers.isEmpty()) {
        auto *label = new QLabel(emptyText, page);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        label->setStyleSheet(QStringLiteral("color: %1; padding: 24px;")
                                 .arg(Theme::instance().colors().textMuted.name()));
        grid->addWidget(label, 0, 0, 1, kColumns);
    } else {
        int row = 0;
        int column = 0;
        for (const StickerButton::Info &info : stickers) {
            auto *button = new StickerButton(info, page);
            m_buttons.append(button);
            connect(button, &StickerButton::chosen, this,
                    [this](const StickerButton::Info &chosen) {
                emit stickerChosen(chosen.fileId, chosen.emoji, chosen.width, chosen.height);
                hide();
            });
            grid->addWidget(button, row, column);
            if (++column >= kColumns) {
                column = 0;
                ++row;
            }
        }
        grid->setRowStretch(row + 1, 1);
    }

    scroll->setWidget(page);
    return scroll;
}

QList<StickerButton::Info> StickerPanel::parseStickers(const QJsonObject &result) const
{
    QList<StickerButton::Info> stickers;

    const QJsonArray array = Json::array(result, QStringLiteral("stickers"));
    for (const QJsonValue &value : array) {
        const QJsonObject sticker = value.toObject();

        StickerButton::Info info;
        info.emoji = Json::str(sticker, QStringLiteral("emoji"));
        info.width = qMax(1, Json::integer(sticker, QStringLiteral("width"), 512));
        info.height = qMax(1, Json::integer(sticker, QStringLiteral("height"), 512));

        const QJsonObject file = Json::object(sticker, QStringLiteral("sticker"));
        info.fileId = Json::integer(file, QStringLiteral("id"));

        const QJsonObject thumbnail = Json::object(sticker, QStringLiteral("thumbnail"));
        const QJsonObject thumbFile = Json::object(thumbnail, QStringLiteral("file"));
        info.thumbFileId = Json::integer(thumbFile, QStringLiteral("id"));

        const QJsonObject local = Json::object(thumbFile, QStringLiteral("local"));
        if (Json::boolean(local, QStringLiteral("is_downloading_completed")))
            info.thumbPath = Json::str(local, QStringLiteral("path"));

        // Nhãn dán tĩnh (webp) dùng được luôn tệp chính để hiển thị.
        if (info.thumbFileId == 0) {
            info.thumbFileId = info.fileId;
            const QJsonObject mainLocal = Json::object(file, QStringLiteral("local"));
            if (Json::boolean(mainLocal, QStringLiteral("is_downloading_completed")))
                info.thumbPath = Json::str(mainLocal, QStringLiteral("path"));
        }

        if (info.fileId != 0)
            stickers.append(info);
    }
    return stickers;
}

void StickerPanel::fillPage(int pageIndex, const QList<StickerButton::Info> &stickers,
                            const QString &emptyText)
{
    if (pageIndex < 0 || pageIndex >= m_pages->count())
        return;

    // Bỏ các ô của trang cũ khỏi danh sách theo dõi trước khi xoá widget.
    QWidget *old = m_pages->widget(pageIndex);
    const QList<StickerButton *> stale = old->findChildren<StickerButton *>();
    for (StickerButton *button : stale)
        m_buttons.removeAll(button);

    QWidget *replacement = buildGrid(stickers, emptyText);
    m_pages->insertWidget(pageIndex, replacement);
    m_pages->removeWidget(old);
    old->deleteLater();
    m_pages->setCurrentIndex(m_tabs->currentIndex());

    // Xin tải ảnh thu nhỏ cho những ô chưa có.
    if (!m_account)
        return;
    for (const StickerButton::Info &info : stickers) {
        if (info.thumbPath.isEmpty() && info.thumbFileId != 0)
            m_account->downloadFile(info.thumbFileId, 8);
    }
}

void StickerPanel::reload()
{
    if (!m_account || !m_account->isReady()) {
        fillPage(0, {}, tr("Cần một tài khoản đã đăng nhập."));
        fillPage(1, {}, tr("Cần một tài khoản đã đăng nhập."));
        return;
    }

    QPointer<StickerPanel> guard(this);

    m_account->fetchRecentStickers([this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        fillPage(0, ok ? parseStickers(result) : QList<StickerButton::Info>(),
                 ok ? tr("Chưa có nhãn dán nào dùng gần đây.\n\nGửi một nhãn dán từ điện "
                         "thoại rồi mở lại bảng này.")
                    : tr("Không lấy được danh sách nhãn dán."));
    });

    m_account->fetchFavoriteStickers([this, guard](const QJsonObject &result, bool ok) {
        if (!guard)
            return;
        fillPage(1, ok ? parseStickers(result) : QList<StickerButton::Info>(),
                 tr("Chưa có nhãn dán yêu thích."));
    });
}

void StickerPanel::onFileReady(int fileId, const QString &localPath)
{
    for (StickerButton *button : m_buttons) {
        if (button->info().thumbFileId == fileId)
            button->setThumbPath(localPath);
    }
}

void StickerPanel::popupAbove(QWidget *anchor)
{
    if (!anchor)
        return;

    reload();

    QPoint position = anchor->mapToGlobal(QPoint(0, 0));
    position.setY(position.y() - height() - 6);
    position.setX(position.x() - 8);

    if (QScreen *screen = anchor->screen()) {
        const QRect available = screen->availableGeometry();
        if (position.y() < available.top())
            position.setY(anchor->mapToGlobal(QPoint(0, anchor->height() + 6)).y());
        if (position.x() + width() > available.right())
            position.setX(available.right() - width() - 4);
        if (position.x() < available.left())
            position.setX(available.left() + 4);
    }

    move(position);
    show();
    raise();
}

void StickerPanel::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral(
        "StickerPanel { background: %1; border: 1px solid %2; border-radius: 12px; }")
        .arg(c.panelBg.name(), c.divider.name()));
}
