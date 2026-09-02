#include "ui/logindialog.h"

#include "core/formatting.h"
#include "ui/flatbutton.h"
#include "ui/iconfactory.h"
#include "ui/qrview.h"
#include "ui/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *makeBody(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    return label;
}

} // namespace

LoginDialog::LoginDialog(TdAccount *account, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
{
    setWindowTitle(tr("Đăng nhập Telegram"));
    setWindowIcon(Icons::appIcon());
    setModal(true);
    setMinimumSize(460, 440);

    buildUi();

    connect(m_account, &TdAccount::stateChanged, this, &LoginDialog::onStateChanged);
    connect(m_account, &TdAccount::authPromptChanged, this, &LoginDialog::onPromptChanged);
    connect(m_account, &TdAccount::qrLinkChanged, this, &LoginDialog::onQrLink);
    connect(m_account, &TdAccount::errorOccurred, this, &LoginDialog::onError);
    connect(&Theme::instance(), &Theme::changed, this, &LoginDialog::applyTheme);

    applyTheme();
    onStateChanged(m_account->state());
}

void LoginDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(12);

    // --- Tiêu đề -----------------------------------------------------------
    auto *headerRow = new QHBoxLayout;
    auto *logo = new QLabel(this);
    logo->setPixmap(Icons::appLogo(44, Theme::instance().colors().accent));
    headerRow->addWidget(logo);

    m_title = new QLabel(tr("Thêm tài khoản Telegram"), this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
    m_title->setFont(titleFont);
    headerRow->addWidget(m_title, 1);
    root->addLayout(headerRow);

    m_pages = new QStackedWidget(this);
    root->addWidget(m_pages, 1);

    // --- Trang 0: chọn cách đăng nhập -------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(makeBody(
            tr("Bạn muốn đăng nhập theo cách nào?"), page));

        auto *qrButton = new QPushButton(tr("  Quét mã QR bằng điện thoại  "), page);
        qrButton->setIcon(Icons::icon(Icons::Name::QrCode,
                                      Theme::instance().colors().textOnAccent, 20));
        qrButton->setProperty("accent", true);
        qrButton->setMinimumHeight(44);
        layout->addWidget(qrButton);

        auto *phoneButton = new QPushButton(tr("  Dùng số điện thoại  "), page);
        phoneButton->setIcon(Icons::icon(Icons::Name::Phone,
                                         Theme::instance().colors().textSecondary, 20));
        phoneButton->setMinimumHeight(44);
        layout->addWidget(phoneButton);

        layout->addWidget(makeBody(
            tr("<b>Cách quét QR</b> nhanh và an toàn hơn: mở Telegram trên điện thoại → "
               "<i>Cài đặt → Thiết bị → Liên kết thiết bị máy tính</i> rồi quét mã hiện ra."),
            page));
        layout->addStretch(1);

        connect(qrButton, &QPushButton::clicked, this, [this] {
            m_account->requestQrLogin();
            showPage(PageQr);
            setHint(tr("Đang tạo mã QR…"));
        });
        connect(phoneButton, &QPushButton::clicked, this, [this] { showPage(PagePhone); });

        m_pages->addWidget(page);
    }

    // --- Trang 1: số điện thoại -------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(makeBody(
            tr("Nhập số điện thoại kèm mã quốc gia. Ví dụ Việt Nam: "
               "<b>+84912345678</b>."), page));

        m_phoneInput = new QLineEdit(page);
        m_phoneInput->setPlaceholderText(QStringLiteral("+84…"));
        m_phoneInput->setText(m_account->savedPhone().isEmpty()
                                  ? QStringLiteral("+84") : m_account->savedPhone());
        m_phoneInput->setMinimumHeight(42);
        layout->addWidget(m_phoneInput);
        layout->addStretch(1);

        connect(m_phoneInput, &QLineEdit::returnPressed, this, [this] {
            m_primaryButton->click();
        });

        m_pages->addWidget(page);
    }

    // --- Trang 2: mã xác thực ---------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        m_codeTarget = makeBody(QString(), page);
        layout->addWidget(m_codeTarget);

        m_codeInput = new QLineEdit(page);
        m_codeInput->setPlaceholderText(tr("Mã xác thực"));
        m_codeInput->setMaxLength(8);
        m_codeInput->setMinimumHeight(46);
        QFont codeFont = m_codeInput->font();
        codeFont.setPointSizeF(codeFont.pointSizeF() * 1.5);
        codeFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
        m_codeInput->setFont(codeFont);
        m_codeInput->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_codeInput);

        m_resendButton = new QPushButton(tr("Gửi lại mã"), page);
        m_resendButton->setProperty("flat", true);
        layout->addWidget(m_resendButton, 0, Qt::AlignLeft);
        layout->addStretch(1);

        connect(m_codeInput, &QLineEdit::returnPressed, this,
                [this] { m_primaryButton->click(); });
        connect(m_codeInput, &QLineEdit::textChanged, this, [this](const QString &text) {
            // Tự gửi khi đủ số ký tự — bớt một lần bấm.
            if (text.size() == m_account->authPrompt().codeLength)
                m_primaryButton->click();
        });
        connect(m_resendButton, &QPushButton::clicked, this, [this] {
            m_account->resendCode();
            setHint(tr("Đã yêu cầu gửi lại mã."));
            m_resendButton->setEnabled(false);
            m_resendSeconds = 60;
            m_resendTimer->start();
        });

        m_pages->addWidget(page);
    }

    // --- Trang 3: mật khẩu hai lớp ----------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(makeBody(
            tr("Tài khoản này bật <b>xác thực hai lớp</b>. Nhập mật khẩu đám mây "
               "(cloud password) của bạn."), page));

        m_passwordInput = new QLineEdit(page);
        m_passwordInput->setEchoMode(QLineEdit::Password);
        m_passwordInput->setPlaceholderText(tr("Mật khẩu"));
        m_passwordInput->setMinimumHeight(42);
        layout->addWidget(m_passwordInput);

        m_passwordHint = makeBody(QString(), page);
        layout->addWidget(m_passwordHint);

        auto *recover = new QPushButton(tr("Tôi quên mật khẩu"), page);
        recover->setProperty("flat", true);
        layout->addWidget(recover, 0, Qt::AlignLeft);
        layout->addStretch(1);

        connect(m_passwordInput, &QLineEdit::returnPressed, this,
                [this] { m_primaryButton->click(); });
        connect(recover, &QPushButton::clicked, this, [this] {
            m_account->requestPasswordRecovery();
            setHint(tr("Đã gửi email khôi phục (nếu tài khoản có email dự phòng)."));
        });

        m_pages->addWidget(page);
    }

    // --- Trang 4: đăng ký tên ---------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        layout->addWidget(makeBody(
            tr("Số này chưa có tài khoản Telegram. Nhập tên để tạo mới."), page));

        m_firstNameInput = new QLineEdit(page);
        m_firstNameInput->setPlaceholderText(tr("Tên (bắt buộc)"));
        m_firstNameInput->setMinimumHeight(42);
        m_lastNameInput = new QLineEdit(page);
        m_lastNameInput->setPlaceholderText(tr("Họ (không bắt buộc)"));
        m_lastNameInput->setMinimumHeight(42);
        layout->addWidget(m_firstNameInput);
        layout->addWidget(m_lastNameInput);
        layout->addStretch(1);

        m_pages->addWidget(page);
    }

    // --- Trang 5: mã QR ----------------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        m_qrView = new QrView(page);
        layout->addWidget(m_qrView, 1, Qt::AlignHCenter);

        layout->addWidget(makeBody(
            tr("1. Mở <b>Telegram</b> trên điện thoại<br/>"
               "2. Vào <i>Cài đặt → Thiết bị → Liên kết thiết bị máy tính</i><br/>"
               "3. Quét mã QR ở trên"), page));

        auto *copyRow = new QHBoxLayout;
        m_qrLinkLabel = new QLabel(page);
        m_qrLinkLabel->setWordWrap(true);
        m_qrLinkLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        copyRow->addWidget(m_qrLinkLabel, 1);

        auto *copyButton = new IconButton(Icons::Name::Copy, tr("Chép liên kết"), 18, page);
        copyRow->addWidget(copyButton);
        layout->addLayout(copyRow);

        connect(copyButton, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_account->qrLink());
            setHint(tr("Đã chép liên kết đăng nhập."));
        });

        m_pages->addWidget(page);
    }

    // --- Trang 6: xong -----------------------------------------------------
    {
        auto *page = new QWidget(m_pages);
        auto *layout = new QVBoxLayout(page);
        layout->addStretch(1);
        layout->addWidget(makeBody(
            tr("<h3>Đăng nhập thành công!</h3>"
               "Danh sách trò chuyện đang được tải về máy. "
               "Bạn có thể thêm tài khoản khác bằng dấu “+” ở thanh bên trái."), page));
        layout->addStretch(1);
        m_pages->addWidget(page);
    }

    // --- Chân hộp thoại ----------------------------------------------------
    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    root->addWidget(m_hint);

    auto *buttonRow = new QHBoxLayout;
    m_backButton = new QPushButton(tr("Quay lại"), this);
    m_backButton->setProperty("flat", true);
    m_primaryButton = new QPushButton(tr("Tiếp tục"), this);
    m_primaryButton->setProperty("accent", true);
    m_primaryButton->setMinimumHeight(40);
    m_primaryButton->setDefault(true);

    buttonRow->addWidget(m_backButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_primaryButton);
    root->addLayout(buttonRow);

    connect(m_backButton, &QPushButton::clicked, this, [this] {
        if (m_current == PagePhone || m_current == PageQr)
            showPage(PageChoose);
        else
            reject();
    });

    connect(m_primaryButton, &QPushButton::clicked, this, [this] {
        switch (m_current) {
        case PageChoose:
            showPage(PagePhone);
            break;
        case PagePhone: {
            const QString phone = Format::normalizePhone(m_phoneInput->text());
            if (phone.size() < 8) {
                setHint(tr("Số điện thoại chưa hợp lệ."), true);
                return;
            }
            setBusy(true);
            setHint(tr("Đang gửi mã xác thực…"));
            m_account->submitPhone(phone);
            break;
        }
        case PageCode: {
            const QString code = m_codeInput->text().trimmed();
            if (code.isEmpty()) {
                setHint(tr("Hãy nhập mã xác thực."), true);
                return;
            }
            setBusy(true);
            setHint(tr("Đang kiểm tra mã…"));
            m_account->submitCode(code);
            break;
        }
        case PagePassword: {
            if (m_passwordInput->text().isEmpty()) {
                setHint(tr("Hãy nhập mật khẩu."), true);
                return;
            }
            setBusy(true);
            setHint(tr("Đang kiểm tra mật khẩu…"));
            m_account->submitPassword(m_passwordInput->text());
            break;
        }
        case PageRegister: {
            if (m_firstNameInput->text().trimmed().isEmpty()) {
                setHint(tr("Hãy nhập tên."), true);
                return;
            }
            setBusy(true);
            m_account->submitRegistration(m_firstNameInput->text().trimmed(),
                                          m_lastNameInput->text().trimmed());
            break;
        }
        case PageQr:
            // Không có gì để xác nhận — chỉ chờ quét.
            break;
        case PageDone:
            accept();
            break;
        }
    });

    m_resendTimer = new QTimer(this);
    m_resendTimer->setInterval(1000);
    connect(m_resendTimer, &QTimer::timeout, this, &LoginDialog::tickResend);
}

void LoginDialog::showPage(Page page)
{
    m_current = page;
    m_pages->setCurrentIndex(static_cast<int>(page));
    setBusy(false);

    switch (page) {
    case PageChoose:
        m_title->setText(tr("Thêm tài khoản Telegram"));
        m_primaryButton->setText(tr("Dùng số điện thoại"));
        m_primaryButton->setVisible(false);
        m_backButton->setText(tr("Huỷ"));
        break;
    case PagePhone:
        m_title->setText(tr("Số điện thoại"));
        m_primaryButton->setText(tr("Gửi mã xác thực"));
        m_primaryButton->setVisible(true);
        m_backButton->setText(tr("Quay lại"));
        m_phoneInput->setFocus();
        break;
    case PageCode:
        m_title->setText(tr("Mã xác thực"));
        m_primaryButton->setText(tr("Xác nhận"));
        m_primaryButton->setVisible(true);
        m_backButton->setText(tr("Huỷ"));
        m_codeInput->clear();
        m_codeInput->setFocus();
        break;
    case PagePassword:
        m_title->setText(tr("Xác thực hai lớp"));
        m_primaryButton->setText(tr("Đăng nhập"));
        m_primaryButton->setVisible(true);
        m_backButton->setText(tr("Huỷ"));
        m_passwordInput->setFocus();
        break;
    case PageRegister:
        m_title->setText(tr("Tạo tài khoản mới"));
        m_primaryButton->setText(tr("Tạo tài khoản"));
        m_primaryButton->setVisible(true);
        m_firstNameInput->setFocus();
        break;
    case PageQr:
        m_title->setText(tr("Quét mã QR"));
        m_primaryButton->setVisible(false);
        m_backButton->setText(tr("Dùng số điện thoại"));
        break;
    case PageDone:
        m_title->setText(tr("Hoàn tất"));
        m_primaryButton->setText(tr("Bắt đầu dùng"));
        m_primaryButton->setVisible(true);
        m_backButton->setVisible(false);
        break;
    }
}

void LoginDialog::setBusy(bool busy)
{
    m_primaryButton->setEnabled(!busy);
    if (busy)
        m_primaryButton->setText(tr("Đang xử lý…"));
    else if (m_current == PagePhone)
        m_primaryButton->setText(tr("Gửi mã xác thực"));
    else if (m_current == PageCode)
        m_primaryButton->setText(tr("Xác nhận"));
    else if (m_current == PagePassword)
        m_primaryButton->setText(tr("Đăng nhập"));
}

void LoginDialog::setHint(const QString &text, bool isError)
{
    const Theme::Colors &c = Theme::instance().colors();
    m_hint->setText(text);
    m_hint->setStyleSheet(QStringLiteral("color: %1;")
                              .arg(isError ? c.danger.name() : c.textSecondary.name()));
}

void LoginDialog::onStateChanged(TdAccount::State state)
{
    switch (state) {
    case TdAccount::State::WaitPhone:
        if (m_current != PageChoose && m_current != PageQr)
            showPage(PagePhone);
        break;
    case TdAccount::State::WaitCode:
        showPage(PageCode);
        onPromptChanged();
        break;
    case TdAccount::State::WaitPassword:
        showPage(PagePassword);
        onPromptChanged();
        break;
    case TdAccount::State::WaitRegistration:
        showPage(PageRegister);
        break;
    case TdAccount::State::WaitQrScan:
        showPage(PageQr);
        break;
    case TdAccount::State::Ready:
        showPage(PageDone);
        setHint(QString());
        // Đóng sau một nhịp để người dùng thấy thông báo thành công.
        QTimer::singleShot(1200, this, &QDialog::accept);
        break;
    case TdAccount::State::Failed:
        setHint(m_account->lastError(), true);
        setBusy(false);
        break;
    case TdAccount::State::Starting:
        setHint(tr("Đang khởi động TDLib…"));
        break;
    default:
        break;
    }
}

void LoginDialog::onPromptChanged()
{
    const TdAccount::AuthPrompt prompt = m_account->authPrompt();

    if (m_current == PageCode) {
        m_codeTarget->setText(tr("%1<br/>Số điện thoại: <b>%2</b>")
                                  .arg(prompt.codeSource,
                                       Format::maskPhone(prompt.phoneNumber)));
        m_codeInput->setMaxLength(qMax(4, prompt.codeLength + 2));
        m_resendSeconds = qMax(0, prompt.resendAfterSeconds);
        m_resendButton->setEnabled(m_resendSeconds == 0);
        if (m_resendSeconds > 0)
            m_resendTimer->start();
    }

    if (m_current == PagePassword) {
        QStringList parts;
        if (!prompt.passwordHint.isEmpty())
            parts << tr("Gợi ý: <b>%1</b>").arg(prompt.passwordHint.toHtmlEscaped());
        if (prompt.hasRecoveryEmail && !prompt.recoveryEmailPattern.isEmpty())
            parts << tr("Email dự phòng: %1").arg(prompt.recoveryEmailPattern);
        m_passwordHint->setText(parts.join(QStringLiteral("<br/>")));
    }
}

void LoginDialog::tickResend()
{
    if (--m_resendSeconds <= 0) {
        m_resendSeconds = 0;
        m_resendTimer->stop();
        m_resendButton->setEnabled(true);
        m_resendButton->setText(tr("Gửi lại mã"));
        return;
    }
    m_resendButton->setText(tr("Gửi lại mã sau %1 giây").arg(m_resendSeconds));
}

void LoginDialog::onQrLink(const QString &link)
{
    m_qrView->setData(link);
    m_qrLinkLabel->setText(link);
    if (m_current != PageQr)
        showPage(PageQr);
    setHint(tr("Mã QR sẽ tự làm mới khi hết hạn."));
}

void LoginDialog::onError(const QString &message)
{
    setBusy(false);
    QString friendly = message;
    if (message.contains(QStringLiteral("PHONE_NUMBER_INVALID")))
        friendly = tr("Số điện thoại không hợp lệ.");
    else if (message.contains(QStringLiteral("PHONE_CODE_INVALID")))
        friendly = tr("Mã xác thực không đúng.");
    else if (message.contains(QStringLiteral("PHONE_CODE_EXPIRED")))
        friendly = tr("Mã xác thực đã hết hạn, hãy yêu cầu mã mới.");
    else if (message.contains(QStringLiteral("PASSWORD_HASH_INVALID")))
        friendly = tr("Mật khẩu hai lớp không đúng.");
    else if (message.contains(QStringLiteral("FLOOD_WAIT"))) {
        friendly = tr("Telegram tạm chặn do thử quá nhiều lần. Hãy đợi một lát rồi thử lại. (%1)")
                       .arg(message);
    }
    setHint(friendly, true);
}

void LoginDialog::applyTheme()
{
    const Theme::Colors &c = Theme::instance().colors();
    setStyleSheet(QStringLiteral("LoginDialog { background: %1; }").arg(c.windowBg.name()));
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));
    setHint(m_hint->text());
}
