#include "ui/setupwizard.h"

#include "core/apppaths.h"
#include "core/settingsstore.h"
#include "td/tdloader.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include "version.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *richLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    return label;
}

} // namespace

SetupWizard::SetupWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Thiết lập lần đầu"));
    setWindowIcon(Icons::appIcon());
    setModal(true);
    resize(600, 540);

    buildUi();
    checkTdlib();
    updateButtons();
}

void SetupWizard::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    auto *headerRow = new QHBoxLayout;
    auto *logo = new QLabel(this);
    logo->setPixmap(Icons::appLogo(52, Theme::instance().colors().accent));
    headerRow->addWidget(logo);

    m_title = new QLabel(tr("Chào mừng đến %1").arg(QStringLiteral(APP_NAME)), this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    m_title->setFont(titleFont);
    headerRow->addWidget(m_title, 1);
    root->addLayout(headerRow);

    m_pages = new QStackedWidget(this);
    root->addWidget(m_pages, 1);

    // --- Trang 0: giới thiệu ----------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(richLabel(tr(
            "<p>Ứng dụng này giúp bạn dùng <b>nhiều tài khoản Telegram cùng lúc</b> "
            "trên một cửa sổ.</p>"
            "<p>Mọi dữ liệu được lưu trong thư mục <code>data</code> ngay cạnh tệp "
            "chạy, nên bạn có thể copy cả thư mục sang máy khác hay để trong USB "
            "mà không mất phiên đăng nhập.</p>"
            "<p>Trước khi bắt đầu cần hai thứ:</p>"
            "<ol>"
            "<li><b>Thư viện TDLib</b> — phần lo việc kết nối tới Telegram.</li>"
            "<li><b>api_id và api_hash</b> của riêng bạn — Telegram yêu cầu mỗi ứng "
            "dụng phải có khoá riêng.</li>"
            "</ol>"
            "<p>Bấm <i>Tiếp tục</i> để làm từng bước.</p>"), page));
        layout->addStretch(1);

        auto *storage = richLabel(tr("<b>Nơi lưu dữ liệu hiện tại:</b><br/><code>%1</code>")
                                      .arg(QDir::toNativeSeparators(AppPaths::dataDir())), page);
        layout->addWidget(storage);
        if (!AppPaths::isPortable()) {
            auto *warning = richLabel(AppPaths::portableFallbackReason(), page);
            warning->setStyleSheet(QStringLiteral("color: %1;")
                                       .arg(Theme::instance().colors().warning.name()));
            layout->addWidget(warning);
        }

        m_pages->addWidget(page);
    }

    // --- Trang 1: TDLib ----------------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(richLabel(tr(
            "<p>Đặt tệp thư viện <b>%1</b> vào thư mục chứa tệp chạy, hoặc vào thư "
            "mục con <code>lib</code>.</p>"
            "<p>Có thể tải bản dựng sẵn hoặc tự biên dịch từ "
            "<a href='https://github.com/tdlib/td'>github.com/tdlib/td</a> "
            "(cần TDLib 1.8.0 trở lên).</p>")
            .arg(TdLoader::expectedFileNames().join(QStringLiteral(" / "))), page));

        auto *pathRow = new QHBoxLayout;
        m_tdPathInput = new QLineEdit(page);
        m_tdPathInput->setPlaceholderText(tr("Hoặc trỏ thẳng tới tệp thư viện…"));
        m_tdPathInput->setText(SettingsStore::instance().tdlibPathOverride());
        auto *browse = new QPushButton(tr("Chọn tệp…"), page);
        pathRow->addWidget(m_tdPathInput, 1);
        pathRow->addWidget(browse);
        layout->addLayout(pathRow);

        auto *checkRow = new QHBoxLayout;
        auto *check = new QPushButton(tr("Kiểm tra lại"), page);
        check->setProperty("accent", true);
        auto *openFolder = new QPushButton(tr("Mở thư mục ứng dụng"), page);
        checkRow->addWidget(check);
        checkRow->addWidget(openFolder);
        checkRow->addStretch(1);
        layout->addLayout(checkRow);

        m_tdStatus = richLabel(QString(), page);
        layout->addWidget(m_tdStatus);
        layout->addStretch(1);

        connect(browse, &QPushButton::clicked, this, [this] {
            const QString file = QFileDialog::getOpenFileName(
                this, tr("Chọn thư viện TDLib"), AppPaths::applicationDir(),
                tr("Thư viện (*.dll *.so *.so.* *.dylib);;Mọi tệp (*)"));
            if (!file.isEmpty()) {
                m_tdPathInput->setText(file);
                SettingsStore::instance().setTdlibPathOverride(file);
                checkTdlib();
            }
        });
        connect(check, &QPushButton::clicked, this, [this] {
            SettingsStore::instance().setTdlibPathOverride(m_tdPathInput->text().trimmed());
            checkTdlib();
        });
        connect(openFolder, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::applicationDir()));
        });

        m_pages->addWidget(page);
    }

    // --- Trang 2: api_id / api_hash ----------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(richLabel(tr(
            "<p>Cách lấy khoá (làm một lần, dùng mãi):</p>"
            "<ol>"
            "<li>Mở <a href='https://my.telegram.org'>my.telegram.org</a> và đăng nhập "
            "bằng số điện thoại Telegram của bạn.</li>"
            "<li>Chọn <i>API development tools</i>.</li>"
            "<li>Điền tên ứng dụng bất kỳ (ví dụ <i>MultiTele</i>), nền tảng chọn "
            "<i>Desktop</i>.</li>"
            "<li>Sao chép <b>App api_id</b> và <b>App api_hash</b> vào hai ô bên dưới.</li>"
            "</ol>"), page));

        m_apiIdInput = new QLineEdit(page);
        m_apiIdInput->setPlaceholderText(tr("api_id — chỉ gồm chữ số"));
        m_apiIdInput->setMinimumHeight(40);
        if (SettingsStore::instance().apiId() > 0)
            m_apiIdInput->setText(QString::number(SettingsStore::instance().apiId()));

        m_apiHashInput = new QLineEdit(page);
        m_apiHashInput->setPlaceholderText(tr("api_hash — 32 ký tự"));
        m_apiHashInput->setMinimumHeight(40);
        m_apiHashInput->setText(SettingsStore::instance().apiHash());

        layout->addWidget(m_apiIdInput);
        layout->addWidget(m_apiHashInput);

        m_apiStatus = richLabel(QString(), page);
        layout->addWidget(m_apiStatus);
        layout->addStretch(1);

        layout->addWidget(richLabel(tr(
            "<p style='color:%1'>Khoá được lưu trong <code>data/config.ini</code>. "
            "Đừng chia sẻ api_hash cho người khác.</p>")
            .arg(Theme::instance().colors().textMuted.name()), page));

        connect(m_apiIdInput, &QLineEdit::textChanged, this, [this] { updateButtons(); });
        connect(m_apiHashInput, &QLineEdit::textChanged, this, [this] { updateButtons(); });

        m_pages->addWidget(page);
    }

    // --- Trang 3: xong ------------------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->addStretch(1);
        layout->addWidget(richLabel(tr(
            "<h3>Xong rồi!</h3>"
            "<p>Bấm <i>Hoàn tất</i> rồi dùng dấu <b>+</b> ở thanh bên trái để thêm "
            "tài khoản Telegram đầu tiên. Bạn có thể thêm bao nhiêu tài khoản cũng "
            "được — tất cả chạy song song.</p>"
            "<p>Mẹo: đăng nhập bằng <b>mã QR</b> là nhanh nhất.</p>"), page));
        layout->addStretch(1);
        m_pages->addWidget(page);
    }

    // --- Nút điều hướng ----------------------------------------------------
    auto *buttonRow = new QHBoxLayout;
    m_backButton = new QPushButton(tr("Quay lại"), this);
    m_backButton->setProperty("flat", true);
    m_nextButton = new QPushButton(tr("Tiếp tục"), this);
    m_nextButton->setProperty("accent", true);
    m_nextButton->setMinimumHeight(40);
    m_nextButton->setDefault(true);

    buttonRow->addWidget(m_backButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_nextButton);
    root->addLayout(buttonRow);

    connect(m_backButton, &QPushButton::clicked, this, &SetupWizard::goBack);
    connect(m_nextButton, &QPushButton::clicked, this, &SetupWizard::goNext);
}

void SetupWizard::checkTdlib()
{
    const Theme::Colors &c = Theme::instance().colors();
    TdLoader &loader = TdLoader::instance();

    if (loader.load()) {
        m_tdStatus->setText(tr("<b style='color:%1'>✓ Đã tìm thấy TDLib</b><br/>"
                               "<code>%2</code>")
                                .arg(c.success.name(), loader.libraryPath().toHtmlEscaped()));
    } else {
        m_tdStatus->setText(tr("<b style='color:%1'>✗ Chưa tìm thấy TDLib</b><br/>"
                               "<span style='color:%2'>%3</span><br/><br/>"
                               "<b>Đã tìm ở:</b><br/><code>%4</code>")
                                .arg(c.danger.name(),
                                     c.textSecondary.name(),
                                     loader.lastError().toHtmlEscaped(),
                                     loader.searchedPaths().join(QStringLiteral("<br/>"))
                                         .toHtmlEscaped()));
    }
    updateButtons();
}

void SetupWizard::saveCredentials()
{
    bool ok = false;
    const int apiId = m_apiIdInput->text().trimmed().toInt(&ok);
    const QString apiHash = m_apiHashInput->text().trimmed();

    SettingsStore &settings = SettingsStore::instance();
    if (ok && apiId > 0)
        settings.setApiId(apiId);
    settings.setApiHash(apiHash);
    settings.flush();
}

void SetupWizard::goNext()
{
    const int index = m_pages->currentIndex();

    if (index == 2) {
        saveCredentials();
        if (!SettingsStore::instance().hasApiCredentials()) {
            m_apiStatus->setText(tr("<span style='color:%1'>api_id phải là số dương và "
                                    "api_hash phải dài ít nhất 16 ký tự.</span>")
                                     .arg(Theme::instance().colors().danger.name()));
            return;
        }
    }

    if (index >= m_pages->count() - 1) {
        m_ready = SettingsStore::instance().hasApiCredentials();
        SettingsStore::instance().setSetupCompleted(true);
        SettingsStore::instance().flush();
        accept();
        return;
    }

    m_pages->setCurrentIndex(index + 1);
    if (m_pages->currentIndex() == 1)
        checkTdlib();
    updateButtons();
}

void SetupWizard::goBack()
{
    if (m_pages->currentIndex() == 0) {
        reject();
        return;
    }
    m_pages->setCurrentIndex(m_pages->currentIndex() - 1);
    updateButtons();
}

void SetupWizard::updateButtons()
{
    const int index = m_pages->currentIndex();

    switch (index) {
    case 0:
        m_title->setText(tr("Chào mừng đến %1").arg(QStringLiteral(APP_NAME)));
        m_nextButton->setText(tr("Tiếp tục"));
        m_backButton->setText(tr("Để sau"));
        m_nextButton->setEnabled(true);
        break;
    case 1:
        m_title->setText(tr("Bước 1 — Thư viện TDLib"));
        m_nextButton->setText(tr("Tiếp tục"));
        m_backButton->setText(tr("Quay lại"));
        // Vẫn cho đi tiếp để người dùng nhập api_id trước, cài TDLib sau.
        m_nextButton->setEnabled(true);
        break;
    case 2:
        m_title->setText(tr("Bước 2 — api_id và api_hash"));
        m_nextButton->setText(tr("Lưu và tiếp tục"));
        m_backButton->setText(tr("Quay lại"));
        m_nextButton->setEnabled(!m_apiIdInput->text().trimmed().isEmpty()
                                && m_apiHashInput->text().trimmed().size() >= 16);
        break;
    default:
        m_title->setText(tr("Hoàn tất thiết lập"));
        m_nextButton->setText(tr("Hoàn tất"));
        m_backButton->setText(tr("Quay lại"));
        m_nextButton->setEnabled(true);
        break;
    }
}
