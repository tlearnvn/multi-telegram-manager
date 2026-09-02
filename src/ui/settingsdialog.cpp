#include "ui/settingsdialog.h"

#include "core/apppaths.h"
#include "core/formatting.h"
#include "core/settingsstore.h"
#include "td/accountmanager.h"
#include "td/filecache.h"
#include "td/tdaccount.h"
#include "td/tdloader.h"
#include "td/tdtransport.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

struct AccentChoice
{
    const char *label;
    const char *hex;
};

const AccentChoice kAccents[] = {
    { "Xanh Telegram", "#2ea6ff" },
    { "Xanh ngọc",     "#26c6a6" },
    { "Tím",           "#a06bf0" },
    { "Hồng san hô",   "#f2775c" },
    { "Cam",           "#f5a623" },
    { "Lá",            "#4caf6d" },
    { "Đỏ mận",        "#e0567c" }
};

qint64 folderSize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // namespace

SettingsDialog::SettingsDialog(AccountManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Cài đặt"));
    setWindowIcon(Icons::appIcon());
    resize(660, 620);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(12);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildAppearancePage(), tr("Giao diện"));
    tabs->addTab(buildNotificationPage(), tr("Thông báo"));
    tabs->addTab(buildDownloadPage(), tr("Tệp & bộ đệm"));
    tabs->addTab(buildProxyPage(), tr("Proxy"));
    tabs->addTab(buildAdvancedPage(), tr("Nâng cao"));
    root->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Đóng"));
    buttons->button(QDialogButtonBox::Close)->setProperty("accent", true);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);
}

// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildAppearancePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    SettingsStore &settings = SettingsStore::instance();

    auto *themeBox = new QGroupBox(tr("Chủ đề"), page);
    auto *themeForm = new QFormLayout(themeBox);

    m_themeCombo = new QComboBox(themeBox);
    m_themeCombo->addItem(tr("Tối"), static_cast<int>(SettingsStore::ThemeMode::Dark));
    m_themeCombo->addItem(tr("Sáng"), static_cast<int>(SettingsStore::ThemeMode::Light));
    m_themeCombo->addItem(tr("Theo hệ thống"), static_cast<int>(SettingsStore::ThemeMode::System));
    m_themeCombo->setCurrentIndex(m_themeCombo->findData(static_cast<int>(settings.themeMode())));
    themeForm->addRow(tr("Kiểu hiển thị:"), m_themeCombo);

    m_accentCombo = new QComboBox(themeBox);
    for (const AccentChoice &choice : kAccents) {
        QPixmap swatch(16, 16);
        swatch.fill(QColor(QString::fromLatin1(choice.hex)));
        m_accentCombo->addItem(QIcon(swatch), QString::fromUtf8(choice.label),
                               QString::fromLatin1(choice.hex));
    }
    const int accentIndex = m_accentCombo->findData(settings.accentColor());
    m_accentCombo->setCurrentIndex(accentIndex >= 0 ? accentIndex : 0);
    themeForm->addRow(tr("Màu nhấn:"), m_accentCombo);

    auto *fontRow = new QWidget(themeBox);
    auto *fontLayout = new QHBoxLayout(fontRow);
    fontLayout->setContentsMargins(0, 0, 0, 0);
    m_fontSlider = new QSlider(Qt::Horizontal, fontRow);
    m_fontSlider->setRange(80, 150);
    m_fontSlider->setSingleStep(5);
    m_fontSlider->setPageStep(10);
    m_fontSlider->setValue(settings.fontScalePercent());
    m_fontLabel = new QLabel(QStringLiteral("%1%").arg(settings.fontScalePercent()), fontRow);
    m_fontLabel->setMinimumWidth(46);
    fontLayout->addWidget(m_fontSlider, 1);
    fontLayout->addWidget(m_fontLabel);
    themeForm->addRow(tr("Cỡ chữ:"), fontRow);

    layout->addWidget(themeBox);

    auto *listBox = new QGroupBox(tr("Danh sách & tin nhắn"), page);
    auto *listLayout = new QVBoxLayout(listBox);
    m_compactList = new QCheckBox(tr("Danh sách chat gọn (một dòng mỗi cuộc trò chuyện)"), listBox);
    m_compactList->setChecked(settings.compactChatList());
    m_groupAvatars = new QCheckBox(tr("Hiện ảnh đại diện người gửi trong nhóm"), listBox);
    m_groupAvatars->setChecked(settings.showAvatarsInGroups());
    listLayout->addWidget(m_compactList);
    listLayout->addWidget(m_groupAvatars);
    layout->addWidget(listBox);

    layout->addStretch(1);

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::applyAppearance);
    connect(m_accentCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::applyAppearance);
    connect(m_fontSlider, &QSlider::valueChanged, this, [this](int value) {
        m_fontLabel->setText(QStringLiteral("%1%").arg(value));
    });
    connect(m_fontSlider, &QSlider::sliderReleased, this, &SettingsDialog::applyAppearance);
    connect(m_compactList, &QCheckBox::toggled, this, &SettingsDialog::applyAppearance);
    connect(m_groupAvatars, &QCheckBox::toggled, this, &SettingsDialog::applyAppearance);

    return page;
}

void SettingsDialog::applyAppearance()
{
    SettingsStore &settings = SettingsStore::instance();
    settings.setThemeMode(static_cast<SettingsStore::ThemeMode>(
        m_themeCombo->currentData().toInt()));
    settings.setAccentColor(m_accentCombo->currentData().toString());
    settings.setFontScalePercent(m_fontSlider->value());
    settings.setCompactChatList(m_compactList->isChecked());
    settings.setShowAvatarsInGroups(m_groupAvatars->isChecked());
    settings.flush();

    Icons::clearCache();
    Theme::instance().apply();
}

// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildNotificationPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    SettingsStore &settings = SettingsStore::instance();

    m_notifyEnabled = new QCheckBox(tr("Bật thông báo tin nhắn mới"), page);
    m_notifyEnabled->setChecked(settings.notificationsEnabled());
    m_notifyPreview = new QCheckBox(tr("Hiện nội dung tin trong thông báo"), page);
    m_notifyPreview->setChecked(settings.notificationPreview());
    m_trayEnabled = new QCheckBox(tr("Hiện biểu tượng ở khay hệ thống"), page);
    m_trayEnabled->setChecked(settings.trayEnabled());
    m_closeToTray = new QCheckBox(tr("Bấm X thì thu vào khay thay vì thoát"), page);
    m_closeToTray->setChecked(settings.closeToTray());
    m_startMinimized = new QCheckBox(tr("Mở ứng dụng ở trạng thái thu nhỏ"), page);
    m_startMinimized->setChecked(settings.startMinimized());

    layout->addWidget(m_notifyEnabled);
    layout->addWidget(m_notifyPreview);
    layout->addWidget(new QLabel(QString(), page));
    layout->addWidget(m_trayEnabled);
    layout->addWidget(m_closeToTray);
    layout->addWidget(m_startMinimized);

    auto *note = new QLabel(tr(
        "Giữ ứng dụng ở khay hệ thống giúp mọi tài khoản vẫn trực tuyến và nhận "
        "tin nhắn ngay cả khi bạn đã đóng cửa sổ."), page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: %1;")
                            .arg(Theme::instance().colors().textSecondary.name()));
    layout->addWidget(note);
    layout->addStretch(1);

    auto save = [this] {
        SettingsStore &store = SettingsStore::instance();
        store.setNotificationsEnabled(m_notifyEnabled->isChecked());
        store.setNotificationPreview(m_notifyPreview->isChecked());
        store.setTrayEnabled(m_trayEnabled->isChecked());
        store.setCloseToTray(m_closeToTray->isChecked());
        store.setStartMinimized(m_startMinimized->isChecked());
        store.flush();
    };
    connect(m_notifyEnabled, &QCheckBox::toggled, this, save);
    connect(m_notifyPreview, &QCheckBox::toggled, this, save);
    connect(m_trayEnabled, &QCheckBox::toggled, this, save);
    connect(m_closeToTray, &QCheckBox::toggled, this, save);
    connect(m_startMinimized, &QCheckBox::toggled, this, save);

    return page;
}

// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildDownloadPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    SettingsStore &settings = SettingsStore::instance();

    auto *autoBox = new QGroupBox(tr("Tải tự động"), page);
    auto *autoLayout = new QVBoxLayout(autoBox);

    m_autoPhotos = new QCheckBox(tr("Tự tải ảnh trong cuộc trò chuyện đang mở"), autoBox);
    m_autoPhotos->setChecked(settings.autoDownloadPhotos());
    autoLayout->addWidget(m_autoPhotos);

    auto *sizeRow = new QWidget(autoBox);
    auto *sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->addWidget(new QLabel(tr("Tự tải tệp nhỏ hơn:"), sizeRow));
    m_autoMaxMb = new QSpinBox(sizeRow);
    m_autoMaxMb->setRange(0, 2048);
    m_autoMaxMb->setSuffix(tr(" MB"));
    m_autoMaxMb->setSpecialValueText(tr("không tự tải"));
    m_autoMaxMb->setValue(settings.autoDownloadMaxMegabytes());
    sizeLayout->addWidget(m_autoMaxMb);
    sizeLayout->addStretch(1);
    autoLayout->addWidget(sizeRow);
    layout->addWidget(autoBox);

    auto *storageBox = new QGroupBox(tr("Dữ liệu trên đĩa"), page);
    auto *storageLayout = new QVBoxLayout(storageBox);

    m_storageLabel = new QLabel(storageBox);
    m_storageLabel->setWordWrap(true);
    m_storageLabel->setTextFormat(Qt::RichText);
    storageLayout->addWidget(m_storageLabel);

    auto *buttonRow = new QHBoxLayout;
    auto *openFolder = new QPushButton(tr("Mở thư mục dữ liệu"), storageBox);
    auto *clearButton = new QPushButton(tr("Dọn bộ đệm tệp"), storageBox);
    clearButton->setProperty("danger", true);
    buttonRow->addWidget(openFolder);
    buttonRow->addWidget(clearButton);
    buttonRow->addStretch(1);
    storageLayout->addLayout(buttonRow);
    layout->addWidget(storageBox);

    layout->addStretch(1);
    updateStorageLabel();

    auto save = [this] {
        SettingsStore &store = SettingsStore::instance();
        store.setAutoDownloadPhotos(m_autoPhotos->isChecked());
        store.setAutoDownloadMaxMegabytes(m_autoMaxMb->value());
        store.flush();
    };
    connect(m_autoPhotos, &QCheckBox::toggled, this, save);
    connect(m_autoMaxMb, &QSpinBox::valueChanged, this, save);
    connect(openFolder, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
    });
    connect(clearButton, &QPushButton::clicked, this, &SettingsDialog::clearCache);

    return page;
}

void SettingsDialog::updateStorageLabel()
{
    if (!m_storageLabel)
        return;
    const qint64 total = folderSize(AppPaths::dataDir());
    const qint64 cache = folderSize(AppPaths::cacheDir());
    m_storageLabel->setText(tr(
        "Tổng dung lượng: <b>%1</b><br/>"
        "Bộ đệm ảnh/avatar: <b>%2</b><br/>"
        "Đường dẫn: <code>%3</code><br/>"
        "Chế độ: %4")
        .arg(Format::fileSize(total),
             Format::fileSize(cache),
             QDir::toNativeSeparators(AppPaths::dataDir()).toHtmlEscaped(),
             AppPaths::isPortable() ? tr("cơ động (cạnh tệp chạy)")
                                    : tr("thư mục người dùng")));
}

void SettingsDialog::clearCache()
{
    if (QMessageBox::question(this, tr("Dọn bộ đệm"),
            tr("Xoá ảnh và tệp đã tải về trong bộ đệm?\n\n"
               "Tin nhắn và phiên đăng nhập không bị ảnh hưởng; các tệp sẽ được "
               "tải lại khi cần."))
        != QMessageBox::Yes) {
        return;
    }

    FileCache::instance().clear();

    QDir cacheDir(AppPaths::cacheDir());
    cacheDir.removeRecursively();
    AppPaths::ensureDir(AppPaths::cacheDir());
    AppPaths::ensureDir(AppPaths::avatarCacheDir());

    const QList<TdAccount *> accounts = m_manager->accounts();
    for (TdAccount *account : accounts) {
        if (account->isReady())
            account->optimizeStorage(nullptr);
    }

    updateStorageLabel();
    emit statusMessage(tr("Đã dọn bộ đệm."));
}

// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildProxyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    SettingsStore &settings = SettingsStore::instance();

    auto *form = new QFormLayout;

    m_proxyKind = new QComboBox(page);
    m_proxyKind->addItem(tr("Không dùng proxy"),
                         static_cast<int>(SettingsStore::ProxyKind::None));
    m_proxyKind->addItem(QStringLiteral("SOCKS5"),
                         static_cast<int>(SettingsStore::ProxyKind::Socks5));
    m_proxyKind->addItem(QStringLiteral("HTTP"),
                         static_cast<int>(SettingsStore::ProxyKind::Http));
    m_proxyKind->addItem(QStringLiteral("MTProto"),
                         static_cast<int>(SettingsStore::ProxyKind::MtProto));
    m_proxyKind->setCurrentIndex(m_proxyKind->findData(static_cast<int>(settings.proxyKind())));
    form->addRow(tr("Loại:"), m_proxyKind);

    m_proxyServer = new QLineEdit(settings.proxyServer(), page);
    m_proxyServer->setPlaceholderText(tr("địa chỉ máy chủ"));
    form->addRow(tr("Máy chủ:"), m_proxyServer);

    m_proxyPort = new QSpinBox(page);
    m_proxyPort->setRange(1, 65535);
    m_proxyPort->setValue(settings.proxyPort());
    form->addRow(tr("Cổng:"), m_proxyPort);

    m_proxyUser = new QLineEdit(settings.proxyUsername(), page);
    form->addRow(tr("Tên đăng nhập:"), m_proxyUser);

    m_proxyPassword = new QLineEdit(settings.proxyPassword(), page);
    m_proxyPassword->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Mật khẩu:"), m_proxyPassword);

    m_proxySecret = new QLineEdit(settings.proxySecret(), page);
    m_proxySecret->setPlaceholderText(tr("chuỗi secret của MTProto"));
    form->addRow(tr("Secret:"), m_proxySecret);

    layout->addLayout(form);

    auto *buttonRow = new QHBoxLayout;
    auto *apply = new QPushButton(tr("Áp dụng cho mọi tài khoản"), page);
    apply->setProperty("accent", true);
    buttonRow->addWidget(apply);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    m_proxyStatus = new QLabel(page);
    m_proxyStatus->setWordWrap(true);
    layout->addWidget(m_proxyStatus);

    layout->addWidget(new QLabel(tr(
        "Proxy hữu ích khi Telegram bị chặn. Cấu hình dùng chung cho tất cả tài "
        "khoản."), page));
    layout->addStretch(1);

    connect(m_proxyKind, &QComboBox::currentIndexChanged, this,
            [this] { updateProxyFields(); });
    connect(apply, &QPushButton::clicked, this, &SettingsDialog::applyProxy);

    updateProxyFields();
    return page;
}

void SettingsDialog::updateProxyFields()
{
    const auto kind = static_cast<SettingsStore::ProxyKind>(m_proxyKind->currentData().toInt());
    const bool enabled = kind != SettingsStore::ProxyKind::None;
    const bool credentials = kind == SettingsStore::ProxyKind::Socks5
                          || kind == SettingsStore::ProxyKind::Http;

    m_proxyServer->setEnabled(enabled);
    m_proxyPort->setEnabled(enabled);
    m_proxyUser->setEnabled(credentials);
    m_proxyPassword->setEnabled(credentials);
    m_proxySecret->setEnabled(kind == SettingsStore::ProxyKind::MtProto);
}

void SettingsDialog::applyProxy()
{
    SettingsStore &settings = SettingsStore::instance();
    const auto kind = static_cast<SettingsStore::ProxyKind>(m_proxyKind->currentData().toInt());

    if (kind != SettingsStore::ProxyKind::None && m_proxyServer->text().trimmed().isEmpty()) {
        m_proxyStatus->setText(tr("Hãy nhập địa chỉ máy chủ proxy."));
        return;
    }

    settings.setProxyKind(kind);
    settings.setProxyServer(m_proxyServer->text());
    settings.setProxyPort(m_proxyPort->value());
    settings.setProxyUsername(m_proxyUser->text());
    settings.setProxyPassword(m_proxyPassword->text());
    settings.setProxySecret(m_proxySecret->text());
    settings.flush();

    m_manager->applyProxyToAll();
    m_proxyStatus->setText(kind == SettingsStore::ProxyKind::None
        ? tr("Đã tắt proxy trên mọi tài khoản.")
        : tr("Đã áp dụng proxy. Theo dõi trạng thái kết nối ở thanh tài khoản."));
    emit statusMessage(tr("Đã cập nhật cấu hình proxy."));
}

// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildAdvancedPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    SettingsStore &settings = SettingsStore::instance();

    // --- Khoá API ---------------------------------------------------------
    auto *apiBox = new QGroupBox(tr("Khoá API Telegram"), page);
    auto *apiLayout = new QVBoxLayout(apiBox);

    auto *apiNote = new QLabel(tr(
        "Lấy tại <a href='https://my.telegram.org'>my.telegram.org</a> → "
        "<i>API development tools</i>. Đổi khoá sẽ cần đăng nhập lại các tài khoản."),
        apiBox);
    apiNote->setWordWrap(true);
    apiNote->setOpenExternalLinks(true);
    apiLayout->addWidget(apiNote);

    auto *apiForm = new QFormLayout;
    m_apiId = new QLineEdit(apiBox);
    if (settings.apiId() > 0)
        m_apiId->setText(QString::number(settings.apiId()));
    m_apiId->setPlaceholderText(tr("chỉ gồm chữ số"));
    apiForm->addRow(QStringLiteral("api_id:"), m_apiId);

    m_apiHash = new QLineEdit(settings.apiHash(), apiBox);
    m_apiHash->setPlaceholderText(tr("32 ký tự"));
    apiForm->addRow(QStringLiteral("api_hash:"), m_apiHash);
    apiLayout->addLayout(apiForm);

    auto *saveApi = new QPushButton(tr("Lưu khoá API"), apiBox);
    apiLayout->addWidget(saveApi, 0, Qt::AlignLeft);
    layout->addWidget(apiBox);

    connect(saveApi, &QPushButton::clicked, this, &SettingsDialog::saveApiCredentials);

    // --- TDLib ------------------------------------------------------------
    auto *tdBox = new QGroupBox(tr("Thư viện TDLib"), page);
    auto *tdLayout = new QVBoxLayout(tdBox);

    m_tdStatus = new QLabel(tdBox);
    m_tdStatus->setWordWrap(true);
    m_tdStatus->setTextFormat(Qt::RichText);
    tdLayout->addWidget(m_tdStatus);

    auto *pathRow = new QHBoxLayout;
    m_tdPath = new QLineEdit(settings.tdlibPathOverride(), tdBox);
    m_tdPath->setPlaceholderText(tr("để trống = tự tìm cạnh tệp chạy"));
    auto *browse = new QPushButton(tr("Chọn tệp…"), tdBox);
    pathRow->addWidget(m_tdPath, 1);
    pathRow->addWidget(browse);
    tdLayout->addLayout(pathRow);

    auto *tdRow = new QHBoxLayout;
    auto *reload = new QPushButton(tr("Nạp lại TDLib"), tdBox);
    reload->setProperty("accent", true);
    tdRow->addWidget(reload);

    tdRow->addWidget(new QLabel(tr("Mức ghi log:"), tdBox));
    m_verbosity = new QComboBox(tdBox);
    m_verbosity->addItem(tr("0 — tắt"), 0);
    m_verbosity->addItem(tr("1 — lỗi"), 1);
    m_verbosity->addItem(tr("2 — cảnh báo"), 2);
    m_verbosity->addItem(tr("3 — thông tin"), 3);
    m_verbosity->addItem(tr("4 — gỡ lỗi"), 4);
    m_verbosity->addItem(tr("5 — rất chi tiết"), 5);
    m_verbosity->setCurrentIndex(m_verbosity->findData(settings.tdlibVerbosity()));
    tdRow->addWidget(m_verbosity);
    tdRow->addStretch(1);
    tdLayout->addLayout(tdRow);
    layout->addWidget(tdBox);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Chọn thư viện TDLib"), AppPaths::applicationDir(),
            tr("Thư viện (*.dll *.so *.so.* *.dylib);;Mọi tệp (*)"));
        if (!file.isEmpty())
            m_tdPath->setText(file);
    });
    connect(reload, &QPushButton::clicked, this, &SettingsDialog::reloadTdlib);
    connect(m_verbosity, &QComboBox::currentIndexChanged, this, [this] {
        const int level = m_verbosity->currentData().toInt();
        SettingsStore::instance().setTdlibVerbosity(level);
        SettingsStore::instance().flush();
        TdLoader::instance().setVerbosity(level);
    });

    // --- Khởi động lại tài khoản ------------------------------------------
    auto *maintenanceBox = new QGroupBox(tr("Bảo trì"), page);
    auto *maintenanceLayout = new QHBoxLayout(maintenanceBox);
    auto *reopen = new QPushButton(tr("Mở lại mọi kết nối"), maintenanceBox);
    maintenanceLayout->addWidget(reopen);
    maintenanceLayout->addStretch(1);
    layout->addWidget(maintenanceBox);

    connect(reopen, &QPushButton::clicked, this, [this] {
        m_manager->reopenAll();
        emit statusMessage(tr("Đang mở lại kết nối cho mọi tài khoản…"));
    });

    layout->addStretch(1);

    // Chỉ hiển thị trạng thái; việc nạp lại chỉ xảy ra khi người dùng bấm nút.
    refreshTdlibStatus();
    return page;
}

void SettingsDialog::saveApiCredentials()
{
    bool ok = false;
    const int apiId = m_apiId->text().trimmed().toInt(&ok);
    const QString apiHash = m_apiHash->text().trimmed();

    if (!ok || apiId <= 0 || apiHash.size() < 16) {
        QMessageBox::warning(this, tr("Khoá không hợp lệ"),
                             tr("api_id phải là số dương và api_hash dài ít nhất 16 ký tự."));
        return;
    }

    SettingsStore &settings = SettingsStore::instance();
    const bool changed = settings.apiId() != apiId || settings.apiHash() != apiHash;
    settings.setApiId(apiId);
    settings.setApiHash(apiHash);
    settings.flush();

    emit statusMessage(tr("Đã lưu khoá API."));
    if (changed) {
        QMessageBox::information(this, tr("Đã lưu"),
            tr("Khoá API mới sẽ có hiệu lực với các tài khoản thêm sau. "
               "Các tài khoản đang đăng nhập vẫn dùng khoá cũ cho tới khi bạn "
               "đăng nhập lại."));
    }
}

void SettingsDialog::reloadTdlib()
{
    SettingsStore &settings = SettingsStore::instance();
    if (m_tdPath)
        settings.setTdlibPathOverride(m_tdPath->text().trimmed());
    settings.flush();

    TdLoader &loader = TdLoader::instance();

    // Chưa nạp được thì thử nạp lại rồi mở lại các tài khoản.
    if (!loader.isLoaded() && loader.load()) {
        TdTransport::instance().start();
        m_manager->reopenAll();
        emit statusMessage(tr("Đã nạp TDLib và mở lại các tài khoản."));
    }
    refreshTdlibStatus();
}

void SettingsDialog::refreshTdlibStatus()
{
    if (!m_tdStatus)
        return;

    TdLoader &loader = TdLoader::instance();
    const Theme::Colors &c = Theme::instance().colors();

    if (loader.isLoaded()) {
        m_tdStatus->setText(tr("<b style='color:%1'>Đang dùng TDLib</b><br/><code>%2</code>"
                               "<br/><br/><span style='color:%3'>Muốn đổi sang tệp khác thì "
                               "chọn đường dẫn rồi mở lại ứng dụng.</span>")
                                .arg(c.success.name(),
                                     loader.libraryPath().toHtmlEscaped(),
                                     c.textSecondary.name()));
        return;
    }

    m_tdStatus->setText(tr("<b style='color:%1'>Chưa nạp được TDLib</b><br/>"
                           "<span style='color:%2'>%3</span>")
                            .arg(c.danger.name(),
                                 c.textSecondary.name(),
                                 loader.lastError().toHtmlEscaped()));
}
