#include "ui/theme.h"

#include "core/settingsstore.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace {

//! 7 màu dùng cho tên người gửi / avatar chữ cái, hài hoà ở cả hai chế độ.
const char *kSenderDark[] = {
    "#7fb3ff", "#67d6ad", "#ffc266", "#ff9db4", "#c39cff", "#6fdbe8", "#ffab8c"
};
const char *kSenderLight[] = {
    "#1f6fd0", "#12836a", "#a8620a", "#c0345c", "#6c3fbc", "#0f7f8f", "#b64a1f"
};
const char *kAvatarDark[] = {
    "#3d6ea8", "#2f7f68", "#9a6b1e", "#a34a63", "#6a4a9e", "#20707c", "#9c5334"
};
const char *kAvatarLight[] = {
    "#4c8bd8", "#2fa287", "#d99a2b", "#dd6c86", "#8b6cd6", "#37a3b2", "#d2764e"
};

QString hexOf(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

QString rgbaOf(const QColor &color, int alphaPercent)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(alphaPercent / 100.0, 'f', 3));
}

} // namespace

Theme::Theme()
{
    buildColors();
}

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
}

void Theme::buildColors()
{
    const SettingsStore &settings = SettingsStore::instance();
    SettingsStore::ThemeMode mode = settings.themeMode();

    if (mode == SettingsStore::ThemeMode::System) {
        bool systemDark = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (const QStyleHints *hints = QGuiApplication::styleHints())
            systemDark = hints->colorScheme() == Qt::ColorScheme::Dark;
#else
        // Qt < 6.5 chưa có API màu hệ thống: suy ra từ bảng màu mặc định.
        const QPalette base = QApplication::palette();
        systemDark = base.color(QPalette::Window).lightness() < 128;
#endif
        mode = systemDark ? SettingsStore::ThemeMode::Dark : SettingsStore::ThemeMode::Light;
    }

    Colors c;
    c.dark = mode == SettingsStore::ThemeMode::Dark;

    QColor accent(settings.accentColor());
    if (!accent.isValid())
        accent = QColor(QStringLiteral("#2ea6ff"));

    if (c.dark) {
        c.windowBg    = QColor(0x14, 0x17, 0x1c);
        c.railBg      = QColor(0x0e, 0x11, 0x15);
        c.sidebarBg   = QColor(0x18, 0x1c, 0x22);
        c.chatBg      = QColor(0x1d, 0x21, 0x28);
        c.panelBg     = QColor(0x20, 0x25, 0x2d);
        c.cardBg      = QColor(0x26, 0x2c, 0x35);
        c.hoverBg     = QColor(0x2a, 0x31, 0x3b);
        c.selectedBg  = accent.darker(220);
        c.divider     = QColor(0x2e, 0x35, 0x40);
        c.shadow      = QColor(0, 0, 0, 110);

        c.textPrimary   = QColor(0xec, 0xef, 0xf4);
        c.textSecondary = QColor(0x9d, 0xa7, 0xb4);
        c.textMuted     = QColor(0x74, 0x7f, 0x8c);
        c.textOnAccent  = QColor(0xff, 0xff, 0xff);

        c.bubbleIn      = QColor(0x27, 0x2d, 0x36);
        c.bubbleOut     = accent.darker(190);
        c.bubbleInText  = c.textPrimary;
        c.bubbleOutText = QColor(0xf2, 0xf6, 0xff);
        c.bubbleMeta    = QColor(0x8d, 0x98, 0xa6);

        c.badgeMuted = QColor(0x55, 0x5f, 0x6c);
        c.danger     = QColor(0xf2, 0x6b, 0x6b);
        c.success    = QColor(0x5a, 0xd1, 0x9b);
        c.warning    = QColor(0xf0, 0xb1, 0x4a);
        c.link       = accent.lighter(120);
    } else {
        c.windowBg    = QColor(0xf2, 0xf4, 0xf8);
        c.railBg      = QColor(0xe6, 0xea, 0xf1);
        c.sidebarBg   = QColor(0xff, 0xff, 0xff);
        c.chatBg      = QColor(0xf7, 0xf9, 0xfc);
        c.panelBg     = QColor(0xff, 0xff, 0xff);
        c.cardBg      = QColor(0xf0, 0xf3, 0xf8);
        c.hoverBg     = QColor(0xe8, 0xed, 0xf5);
        c.selectedBg  = accent.lighter(180);
        c.divider     = QColor(0xdd, 0xe3, 0xec);
        c.shadow      = QColor(0x8a, 0x96, 0xa8, 70);

        c.textPrimary   = QColor(0x16, 0x1b, 0x22);
        c.textSecondary = QColor(0x5c, 0x67, 0x76);
        c.textMuted     = QColor(0x8b, 0x95, 0xa3);
        c.textOnAccent  = QColor(0xff, 0xff, 0xff);

        c.bubbleIn      = QColor(0xff, 0xff, 0xff);
        c.bubbleOut     = accent.lighter(172);
        c.bubbleInText  = c.textPrimary;
        c.bubbleOutText = QColor(0x10, 0x27, 0x3c);
        c.bubbleMeta    = QColor(0x7a, 0x86, 0x95);

        c.badgeMuted = QColor(0xb4, 0xbe, 0xcb);
        c.danger     = QColor(0xd6, 0x3c, 0x3c);
        c.success    = QColor(0x1f, 0x9d, 0x6b);
        c.warning    = QColor(0xc7, 0x86, 0x0d);
        c.link       = accent.darker(115);
    }

    c.accent      = accent;
    c.accentHover = c.dark ? accent.lighter(115) : accent.darker(108);
    c.accentSoft  = QColor(accent.red(), accent.green(), accent.blue(), c.dark ? 46 : 34);
    c.badge       = accent;

    m_colors = c;
}

void Theme::apply()
{
    buildColors();

    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app)
        return;

    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const Colors &c = m_colors;

    QPalette palette;
    palette.setColor(QPalette::Window, c.windowBg);
    palette.setColor(QPalette::WindowText, c.textPrimary);
    palette.setColor(QPalette::Base, c.panelBg);
    palette.setColor(QPalette::AlternateBase, c.cardBg);
    palette.setColor(QPalette::Text, c.textPrimary);
    palette.setColor(QPalette::PlaceholderText, c.textMuted);
    palette.setColor(QPalette::Button, c.cardBg);
    palette.setColor(QPalette::ButtonText, c.textPrimary);
    palette.setColor(QPalette::BrightText, c.danger);
    palette.setColor(QPalette::Highlight, c.accent);
    palette.setColor(QPalette::HighlightedText, c.textOnAccent);
    palette.setColor(QPalette::ToolTipBase, c.cardBg);
    palette.setColor(QPalette::ToolTipText, c.textPrimary);
    palette.setColor(QPalette::Link, c.link);
    palette.setColor(QPalette::LinkVisited, c.link.darker(120));
    palette.setColor(QPalette::Mid, c.divider);
    palette.setColor(QPalette::Dark, c.divider.darker(120));
    palette.setColor(QPalette::Shadow, c.shadow);
    palette.setColor(QPalette::Disabled, QPalette::Text, c.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, c.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.textMuted);
    app->setPalette(palette);

    // Cỡ chữ theo tỉ lệ người dùng chọn.
    QFont font = app->font();
    const int basePoint = 10;
    const double scale = SettingsStore::instance().fontScalePercent() / 100.0;
    font.setPointSizeF(basePoint * scale);
#if defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Segoe UI"));
#endif
    font.setHintingPreference(QFont::PreferDefaultHinting);
    app->setFont(font);

    app->setStyleSheet(styleSheet());

    emit changed();
}

QColor Theme::senderColor(int index) const
{
    const int i = qAbs(index) % 7;
    return QColor(QString::fromLatin1(m_colors.dark ? kSenderDark[i] : kSenderLight[i]));
}

QColor Theme::avatarColor(int index) const
{
    const int i = qAbs(index) % 7;
    return QColor(QString::fromLatin1(m_colors.dark ? kAvatarDark[i] : kAvatarLight[i]));
}

QString Theme::styleSheet() const
{
    const Colors &c = m_colors;

    return QStringLiteral(R"(
QWidget {
    color: %TEXT%;
}
QMainWindow, QDialog {
    background: %WINDOW%;
}
QToolTip {
    background: %CARD%;
    color: %TEXT%;
    border: 1px solid %DIVIDER%;
    padding: 5px 8px;
    border-radius: 6px;
}

/* --- Ô nhập liệu --- */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QComboBox {
    background: %CARD%;
    color: %TEXT%;
    border: 1px solid %DIVIDER%;
    border-radius: 9px;
    padding: 7px 10px;
    selection-background-color: %ACCENT%;
    selection-color: %ONACCENT%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QComboBox:focus {
    border: 1px solid %ACCENT%;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QComboBox:disabled {
    color: %MUTED%;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background: %PANEL%;
    border: 1px solid %DIVIDER%;
    border-radius: 8px;
    padding: 4px;
    outline: none;
    selection-background-color: %ACCENTSOFT%;
    selection-color: %TEXT%;
}

/* --- Nút --- */
QPushButton {
    background: %CARD%;
    color: %TEXT%;
    border: 1px solid %DIVIDER%;
    border-radius: 9px;
    padding: 8px 16px;
    font-weight: 500;
}
QPushButton:hover  { background: %HOVER%; }
QPushButton:pressed { background: %DIVIDER%; }
QPushButton:disabled { color: %MUTED%; background: %CARD%; }
QPushButton[accent="true"] {
    background: %ACCENT%;
    color: %ONACCENT%;
    border: 1px solid %ACCENT%;
    font-weight: 600;
}
QPushButton[accent="true"]:hover   { background: %ACCENTHOVER%; }
QPushButton[accent="true"]:disabled { background: %BADGEMUTED%; border-color: %BADGEMUTED%; }
QPushButton[danger="true"] {
    color: %DANGER%;
    border: 1px solid %DANGER%;
    background: transparent;
}
QPushButton[danger="true"]:hover { background: %DANGERSOFT%; }
QPushButton[flat="true"] {
    background: transparent;
    border: none;
    padding: 6px 10px;
}
QPushButton[flat="true"]:hover { background: %HOVER%; }

/* --- Danh sách --- */
QListView, QTreeView, QTableView {
    background: transparent;
    border: none;
    outline: none;
}
QListView::item, QTreeView::item {
    border: none;
}

/* --- Thanh cuộn mảnh --- */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px 2px 2px 0;
}
QScrollBar::handle:vertical {
    background: %SCROLL%;
    min-height: 32px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover { background: %SCROLLHOVER%; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0 2px 2px 2px;
}
QScrollBar::handle:horizontal {
    background: %SCROLL%;
    min-width: 32px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover { background: %SCROLLHOVER%; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

/* --- Menu --- */
QMenu {
    background: %PANEL%;
    border: 1px solid %DIVIDER%;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 7px 22px 7px 14px;
    border-radius: 7px;
    color: %TEXT%;
}
QMenu::item:selected { background: %ACCENTSOFT%; }
QMenu::item:disabled { color: %MUTED%; }
QMenu::separator {
    height: 1px;
    background: %DIVIDER%;
    margin: 5px 8px;
}
QMenuBar { background: %WINDOW%; }
QMenuBar::item { padding: 6px 10px; background: transparent; }
QMenuBar::item:selected { background: %HOVER%; border-radius: 6px; }

/* --- Tab --- */
QTabWidget::pane {
    border: 1px solid %DIVIDER%;
    border-radius: 10px;
    top: -1px;
    background: %PANEL%;
}
QTabBar::tab {
    background: transparent;
    color: %SECONDARY%;
    padding: 8px 16px;
    margin-right: 4px;
    border-radius: 8px;
}
QTabBar::tab:selected {
    background: %ACCENTSOFT%;
    color: %TEXT%;
    font-weight: 600;
}
QTabBar::tab:hover:!selected { background: %HOVER%; }

/* --- Khác --- */
QGroupBox {
    border: 1px solid %DIVIDER%;
    border-radius: 10px;
    margin-top: 14px;
    padding: 12px 12px 8px 12px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: %SECONDARY%;
}
QCheckBox, QRadioButton { spacing: 8px; padding: 3px 0; }
QCheckBox::indicator, QRadioButton::indicator { width: 17px; height: 17px; }
QCheckBox::indicator {
    border: 1px solid %DIVIDER%;
    border-radius: 5px;
    background: %CARD%;
}
QCheckBox::indicator:checked {
    background: %ACCENT%;
    border-color: %ACCENT%;
}
QRadioButton::indicator {
    border: 1px solid %DIVIDER%;
    border-radius: 9px;
    background: %CARD%;
}
QRadioButton::indicator:checked {
    background: %ACCENT%;
    border: 5px solid %CARD%;
}
QSlider::groove:horizontal {
    height: 4px;
    background: %DIVIDER%;
    border-radius: 2px;
}
QSlider::sub-page:horizontal { background: %ACCENT%; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 14px;
    height: 14px;
    margin: -6px 0;
    border-radius: 7px;
    background: %ACCENT%;
}
QProgressBar {
    border: none;
    border-radius: 4px;
    background: %DIVIDER%;
    height: 6px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk { background: %ACCENT%; border-radius: 4px; }
QSplitter::handle { background: %DIVIDER%; }
QSplitter::handle:horizontal { width: 1px; }
QStatusBar { background: %WINDOW%; color: %SECONDARY%; }
QStatusBar::item { border: none; }
)")
        .replace(QStringLiteral("%TEXT%"), hexOf(c.textPrimary))
        .replace(QStringLiteral("%SECONDARY%"), hexOf(c.textSecondary))
        .replace(QStringLiteral("%MUTED%"), hexOf(c.textMuted))
        .replace(QStringLiteral("%WINDOW%"), hexOf(c.windowBg))
        .replace(QStringLiteral("%PANEL%"), hexOf(c.panelBg))
        .replace(QStringLiteral("%CARD%"), hexOf(c.cardBg))
        .replace(QStringLiteral("%HOVER%"), hexOf(c.hoverBg))
        .replace(QStringLiteral("%DIVIDER%"), hexOf(c.divider))
        .replace(QStringLiteral("%ACCENTHOVER%"), hexOf(c.accentHover))
        .replace(QStringLiteral("%ACCENTSOFT%"), rgbaOf(c.accent, c.dark ? 22 : 16))
        .replace(QStringLiteral("%ACCENT%"), hexOf(c.accent))
        .replace(QStringLiteral("%ONACCENT%"), hexOf(c.textOnAccent))
        .replace(QStringLiteral("%DANGERSOFT%"), rgbaOf(c.danger, 14))
        .replace(QStringLiteral("%DANGER%"), hexOf(c.danger))
        .replace(QStringLiteral("%BADGEMUTED%"), hexOf(c.badgeMuted))
        .replace(QStringLiteral("%SCROLLHOVER%"), rgbaOf(c.textSecondary, 55))
        .replace(QStringLiteral("%SCROLL%"), rgbaOf(c.textSecondary, 28));
}
