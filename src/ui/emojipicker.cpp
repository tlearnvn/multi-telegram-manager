#include "ui/emojipicker.h"

#include "core/apppaths.h"
#include "core/settingsstore.h"
#include "ui/theme.h"

#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace {

constexpr int kColumns = 9;
constexpr int kCellSize = 34;

struct Category
{
    const char *label;
    const char *emojis;   //!< các emoji nối liền nhau, tách bằng dấu cách
};

// Bộ emoji thường dùng, gom theo nhóm. Dùng dấu cách để tách vì một emoji có
// thể gồm nhiều điểm mã (ví dụ emoji có màu da hoặc cờ).
const Category kCategories[] = {
    { "Mặt & người",
      "😀 😃 😄 😁 😆 😅 🤣 😂 🙂 🙃 😉 😊 😇 🥰 😍 🤩 😘 😗 😚 😙 😋 😛 😜 🤪 😝 "
      "🤗 🤭 🤫 🤔 🤐 🤨 😐 😑 😶 😏 😒 🙄 😬 😮 😯 😲 😳 🥺 😦 😧 😨 😰 😥 😢 😭 "
      "😱 😖 😣 😞 😓 😩 😫 🥱 😤 😡 😠 🤬 😈 💀 🤡 👻 😺 😸 😹 😻 😽 🙈 🙉 🙊 "
      "🥳 🤓 🧐 😎 🤠 🥶 🥵 🤒 🤕 🤢 🤮 🤧 😷 🤥 😴 😪 🤤" },
    { "Cử chỉ",
      "👍 👎 👌 🤌 🤏 ✌️ 🤞 🤟 🤘 🤙 👈 👉 👆 👇 ☝️ ✋ 🤚 🖐️ 🖖 👋 🤝 🙏 ✍️ 💅 "
      "💪 🦾 👏 🙌 👐 🤲 🫶 ❤️ 🧠 👀 👁️ 👄 🦷 👶 🧑 👨 👩 🧓 👮 🕵️ 💁 🙋 🤦 🤷" },
    { "Trái tim",
      "❤️ 🧡 💛 💚 💙 💜 🖤 🤍 🤎 💔 ❣️ 💕 💞 💓 💗 💖 💘 💝 💟 ✨ ⭐ 🌟 💫 ⚡ 🔥 "
      "💥 💢 💦 💨 🕳️ 💤" },
    { "Động vật & thiên nhiên",
      "🐶 🐱 🐭 🐹 🐰 🦊 🐻 🐼 🐨 🐯 🦁 🐮 🐷 🐸 🐵 🙈 🐔 🐧 🐦 🐤 🦆 🦅 🦉 🦇 🐺 "
      "🐗 🐴 🦄 🐝 🐛 🦋 🐌 🐞 🐜 🕷️ 🦂 🐢 🐍 🦎 🐙 🦑 🦐 🦀 🐡 🐠 🐟 🐬 🐳 🐋 🦈 "
      "🌵 🎄 🌲 🌳 🌴 🌱 🌿 ☘️ 🍀 🎍 🌾 🌷 🌹 🥀 🌺 🌸 🌼 🌻 🌞 🌝 🌚 🌙 ⛅ 🌈 ❄️" },
    { "Ăn uống",
      "🍏 🍎 🍐 🍊 🍋 🍌 🍉 🍇 🍓 🫐 🍈 🍒 🍑 🥭 🍍 🥥 🥝 🍅 🥑 🥦 🥕 🌽 🌶️ 🥒 🥬 "
      "🍞 🥐 🥖 🥨 🧀 🥚 🍳 🧈 🥞 🧇 🥓 🍗 🍖 🌭 🍔 🍟 🍕 🥪 🌮 🌯 🥗 🍝 🍜 🍲 🍛 "
      "🍣 🍱 🥟 🍤 🍙 🍚 🍘 🍥 🥠 🍢 🍡 🍧 🍨 🍦 🥧 🧁 🍰 🎂 🍮 🍭 🍬 🍫 🍿 🍩 🍪 "
      "☕ 🍵 🧋 🥤 🧃 🍺 🍻 🥂 🍷 🥃 🍸 🍹" },
    { "Hoạt động & đồ vật",
      "⚽ 🏀 🏈 ⚾ 🎾 🏐 🏉 🎱 🏓 🏸 🥊 🥋 ⛳ 🎣 🎽 🛹 🛼 🎿 🏂 🏄 🚴 🏊 🏋️ 🤸 🤾 "
      "🎯 🎲 🎮 🕹️ 🎰 🎳 🎼 🎵 🎶 🎤 🎧 🎷 🎸 🎹 🎺 🎻 🥁 📱 💻 🖥️ ⌨️ 🖱️ 🖨️ 💾 "
      "📷 📸 📹 🎥 📺 📻 ⏰ ⌚ 🔋 💡 🔍 🔑 🔒 🔓 🛠️ 🔧 🔨 ⚙️ 📦 📫 📮 ✏️ 📝 📄 📊 "
      "📈 📉 📋 📌 📎 🗂️ 🗓️ 📚 💰 💳 💎 🎁 🎈 🎉 🎊 🏆 🥇 🥈 🥉 🎖️" },
    { "Đi lại & địa điểm",
      "🚗 🚕 🚙 🚌 🚎 🏎️ 🚓 🚑 🚒 🚐 🚚 🚛 🚜 🛴 🚲 🛵 🏍️ 🚨 🚔 🚍 🚞 🚝 🚄 🚅 🚈 "
      "🚂 🚆 🚇 🚊 🚉 ✈️ 🛫 🛬 🛩️ 💺 🚁 🚟 🚠 🛰️ 🚀 🛸 🛶 ⛵ 🚤 🛥️ 🛳️ ⛴️ 🚢 ⚓ "
      "🏠 🏡 🏢 🏣 🏥 🏦 🏨 🏪 🏫 🏬 🏭 🏰 💒 🗼 🗽 ⛪ 🕌 🛕 🕍 ⛩️ 🌋 🏔️ ⛰️ 🏕️ 🏖️" },
    { "Ký hiệu & cờ",
      "✅ ❌ ❎ ➕ ➖ ➗ ✖️ ♻️ ⚠️ 🚫 ⛔ 📛 🔞 ☢️ ☣️ ⬆️ ⬇️ ⬅️ ➡️ ↗️ ↘️ ↙️ ↖️ ↕️ ↔️ "
      "🔄 🔁 🔂 ▶️ ⏸️ ⏹️ ⏺️ ⏭️ ⏮️ 🔼 🔽 ❓ ❔ ❗ ❕ 💬 💭 🗯️ ♠️ ♥️ ♦️ ♣️ 🔴 🟠 🟡 "
      "🟢 🔵 🟣 ⚫ ⚪ 🟤 🔺 🔻 🔶 🔷 🔸 🔹 🏁 🚩 🎌 🏴 🏳️ 🇻🇳 🇺🇸 🇬🇧 🇯🇵 🇰🇷 🇨🇳" }
};

constexpr int kCategoryCount = int(sizeof(kCategories) / sizeof(kCategories[0]));
const QString kRecentKey = QStringLiteral("ui/recent_emojis");

} // namespace

EmojiPicker::EmojiPicker(QWidget *parent)
    : QWidget(parent, Qt::Popup)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(kColumns * kCellSize + 34, 320);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Tìm nhóm biểu tượng…"));
    m_search->setClearButtonEnabled(true);
    root->addWidget(m_search);

    m_tabs = new QTabBar(this);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    m_tabs->setUsesScrollButtons(true);
    root->addWidget(m_tabs);

    m_pages = new QStackedWidget(this);
    root->addWidget(m_pages, 1);

    buildCategories();

    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &query) {
        if (query.trimmed().isEmpty())
            return;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->tabText(i).contains(query.trimmed(), Qt::CaseInsensitive)) {
                m_tabs->setCurrentIndex(i);
                break;
            }
        }
    });

    connect(&Theme::instance(), &Theme::changed, this, &EmojiPicker::applyTheme);
    applyTheme();
}

QWidget *EmojiPicker::buildGrid(const QStringList &emojis)
{
    auto *scroll = new QScrollArea(m_pages);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *page = new QWidget(scroll);
    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);

    int row = 0;
    int column = 0;
    for (const QString &emoji : emojis) {
        auto *button = new QPushButton(emoji, page);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(kCellSize, kCellSize);
        button->setProperty("flat", true);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() * 1.6);
        button->setFont(f);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { border: none; background: transparent; padding: 0; }"
            "QPushButton:hover { background: %1; border-radius: 8px; }")
            .arg(Theme::instance().colors().hoverBg.name()));

        connect(button, &QPushButton::clicked, this, [this, emoji] {
            rememberRecent(emoji);
            emit emojiChosen(emoji);
        });

        grid->addWidget(button, row, column);
        if (++column >= kColumns) {
            column = 0;
            ++row;
        }
    }
    grid->setRowStretch(row + 1, 1);

    scroll->setWidget(page);
    return scroll;
}

void EmojiPicker::buildCategories()
{
    m_recentPage = buildGrid(recentEmojis());
    m_tabs->addTab(tr("Gần đây"));
    m_pages->addWidget(m_recentPage);

    for (int i = 0; i < kCategoryCount; ++i) {
        const QStringList emojis = QString::fromUtf8(kCategories[i].emojis)
                                       .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        m_tabs->addTab(QString::fromUtf8(kCategories[i].label));
        m_pages->addWidget(buildGrid(emojis));
    }

    m_tabs->setCurrentIndex(recentEmojis().isEmpty() ? 1 : 0);
}

void EmojiPicker::refreshRecentGrid()
{
    if (!m_recentPage)
        return;
    const int index = m_pages->indexOf(m_recentPage);
    if (index < 0)
        return;
    QWidget *replacement = buildGrid(recentEmojis());
    m_pages->insertWidget(index, replacement);
    m_pages->removeWidget(m_recentPage);
    m_recentPage->deleteLater();
    m_recentPage = replacement;
}

QStringList EmojiPicker::recentEmojis() const
{
    QSettings settings(AppPaths::configFilePath(), QSettings::IniFormat);
    return settings.value(kRecentKey).toStringList();
}

void EmojiPicker::rememberRecent(const QString &emoji)
{
    QSettings settings(AppPaths::configFilePath(), QSettings::IniFormat);
    QStringList recent = settings.value(kRecentKey).toStringList();
    recent.removeAll(emoji);
    recent.prepend(emoji);
    while (recent.size() > kColumns * 3)
        recent.removeLast();
    settings.setValue(kRecentKey, recent);
    refreshRecentGrid();
}

void EmojiPicker::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral(
        "EmojiPicker { background: %1; border: 1px solid %2; border-radius: 12px; }")
        .arg(c.panelBg.name(), c.divider.name()));
}

void EmojiPicker::popupAbove(QWidget *anchor)
{
    if (!anchor)
        return;

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
    m_search->setFocus();
}
