#include "ui/aboutdialog.h"

#include "core/apppaths.h"
#include "core/logging.h"
#include "td/tdloader.h"
#include "ui/flatbutton.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include "version.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSysInfo>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Giới thiệu %1").arg(QStringLiteral(APP_NAME)));
    setWindowIcon(Icons::appIcon());
    resize(600, 520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    // --- Đầu trang --------------------------------------------------------
    auto *headerRow = new QHBoxLayout;
    auto *logo = new QLabel(this);
    logo->setPixmap(Icons::appLogo(64, Theme::instance().colors().accent));
    headerRow->addWidget(logo);

    auto *titleColumn = new QVBoxLayout;
    auto *name = new QLabel(QStringLiteral(APP_NAME), this);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.5);
    name->setFont(nameFont);

    auto *version = new QLabel(tr("Phiên bản %1").arg(QStringLiteral(APP_VERSION_FULL)), this);
    version->setStyleSheet(QStringLiteral("color: %1;")
                               .arg(Theme::instance().colors().textSecondary.name()));
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);

    titleColumn->addWidget(name);
    titleColumn->addWidget(version);
    headerRow->addLayout(titleColumn, 1);
    root->addLayout(headerRow);

    // --- Các tab ----------------------------------------------------------
    auto *tabs = new QTabWidget(this);

    // Tab 1: thông tin chung
    {
        auto *page = new QWidget(tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(8);

        auto *info = new QLabel(page);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setOpenExternalLinks(true);
        info->setTextInteractionFlags(Qt::TextBrowserInteraction);

        const TdLoader &loader = TdLoader::instance();
        const QString tdStatus = loader.isLoaded()
            ? tr("<span>Đã nạp từ <code>%1</code></span>").arg(loader.libraryPath().toHtmlEscaped())
            : tr("<b>Chưa nạp được</b> — hãy đặt %1 cạnh tệp chạy")
                  .arg(TdLoader::expectedFileNames().join(QStringLiteral(" hoặc ")));

        info->setText(tr(
            "<p>Trình khách Telegram đa tài khoản, viết bằng C++ với Qt %1.</p>"
            "<table cellpadding='3'>"
            "<tr><td><b>Nơi lưu dữ liệu</b></td><td><code>%2</code></td></tr>"
            "<tr><td><b>Chế độ</b></td><td>%3</td></tr>"
            "<tr><td><b>Thư viện TDLib</b></td><td>%4</td></tr>"
            "<tr><td><b>Hệ điều hành</b></td><td>%5</td></tr>"
            "<tr><td><b>Kiến trúc</b></td><td>%6</td></tr>"
            "<tr><td><b>Mã nguồn</b></td><td><a href='%7'>%7</a></td></tr>"
            "</table>")
            .arg(QStringLiteral(QT_VERSION_STR),
                 QDir::toNativeSeparators(AppPaths::dataDir()).toHtmlEscaped(),
                 AppPaths::isPortable() ? tr("Cơ động (dữ liệu cạnh tệp chạy)")
                                        : tr("Thư mục người dùng"),
                 tdStatus,
                 QSysInfo::prettyProductName().toHtmlEscaped(),
                 QSysInfo::currentCpuArchitecture(),
                 QStringLiteral(APP_HOMEPAGE)));
        layout->addWidget(info);
        layout->addStretch(1);

        auto *buttonRow = new QHBoxLayout;
        auto *openData = new QPushButton(tr("Mở thư mục dữ liệu"), page);
        auto *copyInfo = new QPushButton(tr("Chép thông tin chẩn đoán"), page);
        buttonRow->addWidget(openData);
        buttonRow->addWidget(copyInfo);
        buttonRow->addStretch(1);
        layout->addLayout(buttonRow);

        connect(openData, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
        });
        connect(copyInfo, &QPushButton::clicked, this, [] {
            const QString text = QStringLiteral(
                "%1\nPhiên bản: %2\nQt: %3\nHệ điều hành: %4 (%5)\n"
                "Dữ liệu: %6\nTDLib: %7")
                .arg(QStringLiteral(APP_NAME),
                     QStringLiteral(APP_VERSION_FULL),
                     QStringLiteral(QT_VERSION_STR),
                     QSysInfo::prettyProductName(),
                     QSysInfo::currentCpuArchitecture(),
                     AppPaths::dataDir(),
                     TdLoader::instance().isLoaded()
                         ? TdLoader::instance().libraryPath()
                         : QStringLiteral("chưa nạp"));
            QApplication::clipboard()->setText(text);
        });

        tabs->addTab(page, tr("Thông tin"));
    }

    // Tab 2: log
    {
        auto *page = new QWidget(tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(8);

        auto *view = new QPlainTextEdit(page);
        view->setReadOnly(true);
        view->setLineWrapMode(QPlainTextEdit::NoWrap);
        QFont monospace(QStringLiteral("monospace"));
        monospace.setStyleHint(QFont::TypeWriter);
        monospace.setPointSizeF(font().pointSizeF() * 0.88);
        view->setFont(monospace);
        view->setPlainText(Logging::tail(500));
        layout->addWidget(view, 1);

        auto *row = new QHBoxLayout;
        auto *reload = new QPushButton(tr("Tải lại"), page);
        auto *openLog = new QPushButton(tr("Mở tệp log"), page);
        row->addWidget(reload);
        row->addWidget(openLog);
        row->addStretch(1);
        layout->addLayout(row);

        connect(reload, &QPushButton::clicked, view,
                [view] { view->setPlainText(Logging::tail(500)); });
        connect(openLog, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(Logging::logFilePath()));
        });

        tabs->addTab(page, tr("Bản ghi hoạt động"));
    }

    // Tab 3: giấy phép & lưu ý
    {
        auto *page = new QWidget(tabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);

        auto *text = new QLabel(page);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        text->setOpenExternalLinks(true);
        text->setText(tr(
            "<p><b>Giấy phép:</b> MIT.</p>"
            "<p>Ứng dụng dùng <a href='https://github.com/tdlib/td'>TDLib</a> "
            "(Boost Software License 1.0) để nói chuyện với máy chủ Telegram, và "
            "<a href='https://www.qt.io'>Qt 6</a> (LGPL v3) cho giao diện. Hai thư "
            "viện này thuộc về tác giả của chúng.</p>"
            "<p><b>Lưu ý về api_id:</b> mỗi người dùng cần tự tạo api_id/api_hash "
            "riêng tại <a href='https://my.telegram.org'>my.telegram.org</a>. "
            "Không dùng chung khoá của người khác.</p>"
            "<p><b>Bảo mật:</b> phiên đăng nhập nằm trong thư mục "
            "<code>data/accounts</code>. Ai có thư mục đó là có quyền truy cập tài "
            "khoản Telegram của bạn — hãy giữ USB/thư mục cẩn thận.</p>"));
        layout->addWidget(text);
        layout->addStretch(1);

        tabs->addTab(page, tr("Giấy phép"));
    }

    root->addWidget(tabs, 1);

    auto *closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto *close = new QPushButton(tr("Đóng"), this);
    close->setProperty("accent", true);
    closeRow->addWidget(close);
    root->addLayout(closeRow);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}
