#include "ui/composer.h"

#include "core/apppaths.h"
#include "ui/flatbutton.h"
#include "ui/theme.h"

#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
//  ComposerEdit
// ---------------------------------------------------------------------------

ComposerEdit::ComposerEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setAcceptDrops(true);
    setTabChangesFocus(true);
    setPlaceholderText(tr("Nhắn gì đó…"));

    m_lineHeight = QFontMetrics(font()).lineSpacing();

    connect(this, &QPlainTextEdit::textChanged, this, &ComposerEdit::updateHeight);
    updateHeight();
}

QSize ComposerEdit::minimumSizeHint() const
{
    return QSize(120, m_lineHeight + 12);
}

QSize ComposerEdit::sizeHint() const
{
    return QSize(320, m_lineHeight + 12);
}

void ComposerEdit::updateHeight()
{
    // QPlainTextEdit dùng QPlainTextDocumentLayout, ở đó lineCount() là số dòng
    // sau khi đã ngắt dòng — đúng cái ta cần để ô nhập tự cao dần.
    m_lineHeight = QFontMetrics(font()).lineSpacing();
    const int wanted = document()->lineCount();
    const int lines = qBound(1, wanted, 7);
    const int target = lines * m_lineHeight + 14;
    if (height() != target || minimumHeight() != target)
        setFixedHeight(target);

    // Chỉ hiện thanh cuộn khi nội dung thực sự vượt 7 dòng, nếu không sẽ có một
    // vạch mảnh nằm cạnh nút gửi trông như lỗi vẽ.
    setVerticalScrollBarPolicy(wanted > 7 ? Qt::ScrollBarAsNeeded
                                          : Qt::ScrollBarAlwaysOff);
}

void ComposerEdit::keyPressEvent(QKeyEvent *event)
{
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

    if (enter && !shift) {
        emit sendPressed();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        emit escapePressed();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ComposerEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QPlainTextEdit::dragEnterEvent(event);
}

void ComposerEdit::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        QPlainTextEdit::dropEvent(event);
        return;
    }

    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
    }
}

void ComposerEdit::insertFromMimeData(const QMimeData *source)
{
    if (!source)
        return;

    // Dán ảnh từ clipboard: lưu tạm rồi gửi như tệp.
    if (source->hasImage()) {
        const QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull()) {
            const QString dir = AppPaths::ensureDir(
                QDir(AppPaths::cacheDir()).filePath(QStringLiteral("paste")));
            const QString path = QDir(dir).filePath(
                QStringLiteral("dan-%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
            if (image.save(path, "PNG")) {
                emit pastedImage(path);
                return;
            }
        }
    }
    if (source->hasUrls()) {
        QStringList paths;
        const QList<QUrl> urls = source->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile())
                paths << url.toLocalFile();
        }
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
            return;
        }
    }
    insertPlainText(source->text());
}

// ---------------------------------------------------------------------------
//  Composer
// ---------------------------------------------------------------------------

Composer::Composer(QWidget *parent)
    : QWidget(parent)
{
    // QWidget thuần không tự vẽ nền khai báo trong stylesheet; cờ này bật
    // việc đó lên, nếu không widget sẽ trong suốt và lộ màu nền cửa sổ.
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 6, 10, 10);
    root->setSpacing(6);

    // --- Thanh trả lời / sửa ---------------------------------------------
    m_infoBar = new QWidget(this);
    m_infoBar->setObjectName(QStringLiteral("composerInfoBar"));
    auto *infoLayout = new QHBoxLayout(m_infoBar);
    infoLayout->setContentsMargins(10, 6, 6, 6);
    infoLayout->setSpacing(8);

    auto *infoText = new QVBoxLayout;
    infoText->setContentsMargins(0, 0, 0, 0);
    infoText->setSpacing(0);
    m_infoTitle = new QLabel(m_infoBar);
    m_infoBody = new QLabel(m_infoBar);
    m_infoBody->setTextFormat(Qt::PlainText);
    infoText->addWidget(m_infoTitle);
    infoText->addWidget(m_infoBody);
    infoLayout->addLayout(infoText, 1);

    m_infoClose = new IconButton(Icons::Name::Close, tr("Bỏ"), 16, m_infoBar);
    infoLayout->addWidget(m_infoClose);
    m_infoBar->hide();
    root->addWidget(m_infoBar);

    // --- Thông báo không gửi được ----------------------------------------
    m_blockedLabel = new QLabel(this);
    m_blockedLabel->setAlignment(Qt::AlignCenter);
    m_blockedLabel->setWordWrap(true);
    m_blockedLabel->hide();
    root->addWidget(m_blockedLabel);

    // --- Hàng nhập liệu ---------------------------------------------------
    m_inputRow = new QWidget(this);
    m_inputRow->setObjectName(QStringLiteral("composerInputRow"));
    auto *row = new QHBoxLayout(m_inputRow);
    row->setContentsMargins(6, 4, 6, 4);
    row->setSpacing(4);

    m_emojiButton = new IconButton(Icons::Name::Emoji, tr("Biểu tượng cảm xúc"), 21, m_inputRow);
    m_attachButton = new IconButton(Icons::Name::Attach, tr("Gửi tệp hoặc ảnh"), 21, m_inputRow);
    m_edit = new ComposerEdit(m_inputRow);
    m_sendButton = new IconButton(Icons::Name::Send, tr("Gửi (Enter)"), 21, m_inputRow);
    m_sendButton->setAccented(true);

    row->addWidget(m_emojiButton, 0, Qt::AlignBottom);
    row->addWidget(m_attachButton, 0, Qt::AlignBottom);
    row->addWidget(m_edit, 1);
    row->addWidget(m_sendButton, 0, Qt::AlignBottom);
    root->addWidget(m_inputRow);

    connect(m_sendButton, &QPushButton::clicked, this, &Composer::handleSend);
    connect(m_edit, &ComposerEdit::sendPressed, this, &Composer::handleSend);
    connect(m_edit, &ComposerEdit::escapePressed, this, [this] {
        if (m_editMessageId != 0) {
            setEditTarget(0, QString());
            emit editCleared();
        } else if (m_replyMessageId != 0) {
            setReplyTarget(0, QString(), QString());
            emit replyCleared();
        }
    });
    connect(m_edit, &ComposerEdit::filesDropped, this, &Composer::filesRequested);
    connect(m_edit, &ComposerEdit::pastedImage, this, [this](const QString &path) {
        emit filesRequested({ path });
    });
    connect(m_edit, &QPlainTextEdit::textChanged, this, &Composer::handleTextChanged);
    connect(m_attachButton, &QPushButton::clicked, this, &Composer::handleAttachClicked);
    connect(m_emojiButton, &QPushButton::clicked, this, &Composer::emojiPanelRequested);
    connect(m_infoClose, &QPushButton::clicked, this, [this] {
        if (m_editMessageId != 0) {
            setEditTarget(0, QString());
            emit editCleared();
        } else {
            setReplyTarget(0, QString(), QString());
            emit replyCleared();
        }
    });

    connect(&Theme::instance(), &Theme::changed, this, &Composer::refreshTheme);
    refreshTheme();
}

void Composer::refreshTheme()
{
    const Theme::Colors &c = Theme::instance().colors();

    setStyleSheet(QStringLiteral("Composer { background: %1; }").arg(c.chatBg.name()));

    m_inputRow->setStyleSheet(QStringLiteral(
        "#composerInputRow { background: %1; border: 1px solid %2; border-radius: 16px; }")
        .arg(c.cardBg.name(), c.divider.name()));

    m_edit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: transparent; border: none; padding: 4px 2px;"
        " color: %1; selection-background-color: %2; }")
        .arg(c.textPrimary.name(), c.accent.name()));

    m_infoBar->setStyleSheet(QStringLiteral(
        "#composerInfoBar { background: %1; border-radius: 11px; }")
        .arg(c.cardBg.name()));

    QFont titleFont = m_infoTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(font().pointSizeF() * 0.9);
    m_infoTitle->setFont(titleFont);
    m_infoTitle->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(c.accent.name()));

    QFont bodyFont = m_infoBody->font();
    bodyFont.setPointSizeF(font().pointSizeF() * 0.9);
    m_infoBody->setFont(bodyFont);
    m_infoBody->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                  .arg(c.textSecondary.name()));

    m_blockedLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border-radius: 11px; padding: 10px; }")
        .arg(c.textSecondary.name(), c.cardBg.name()));
}

QString Composer::text() const
{
    return m_edit->toPlainText();
}

void Composer::setText(const QString &text)
{
    m_edit->setPlainText(text);
    m_edit->moveCursor(QTextCursor::End);
}

void Composer::clearInput()
{
    m_edit->clear();
}

void Composer::focusInput()
{
    m_edit->setFocus();
}

void Composer::setPlaceholder(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

void Composer::setSendEnabled(bool enabled)
{
    m_sendButton->setEnabled(enabled);
    m_attachButton->setEnabled(enabled);
    m_edit->setReadOnly(!enabled);
}

void Composer::setBlockedReason(const QString &reason)
{
    if (reason.isEmpty()) {
        m_blockedLabel->hide();
        m_inputRow->show();
    } else {
        m_blockedLabel->setText(reason);
        m_blockedLabel->show();
        m_inputRow->hide();
    }
}

void Composer::setReplyTarget(qint64 messageId, const QString &sender, const QString &preview)
{
    m_replyMessageId = messageId;
    m_replySender = sender;
    m_replyPreview = preview;
    if (messageId != 0)
        m_editMessageId = 0;
    updateInfoBar();
    if (messageId != 0)
        focusInput();
}

void Composer::setEditTarget(qint64 messageId, const QString &currentText)
{
    m_editMessageId = messageId;
    if (messageId != 0) {
        m_replyMessageId = 0;
        setText(currentText);
    }
    updateInfoBar();
    if (messageId != 0)
        focusInput();
}

void Composer::updateInfoBar()
{
    if (m_editMessageId != 0) {
        m_infoTitle->setText(tr("Đang sửa tin nhắn"));
        m_infoBody->setText(tr("Nhấn Esc để huỷ, Enter để lưu"));
        m_infoBar->show();
    } else if (m_replyMessageId != 0) {
        m_infoTitle->setText(tr("Trả lời %1").arg(m_replySender.isEmpty()
                                                      ? tr("tin nhắn") : m_replySender));
        m_infoBody->setText(m_replyPreview);
        m_infoBar->show();
    } else {
        m_infoBar->hide();
    }
}

void Composer::handleSend()
{
    const QString content = m_edit->toPlainText().trimmed();
    if (content.isEmpty())
        return;

    if (m_editMessageId != 0) {
        const qint64 target = m_editMessageId;
        setEditTarget(0, QString());
        clearInput();
        emit editSubmitted(target, content);
        return;
    }

    emit sendRequested(content);
    clearInput();
}

void Composer::handleAttachClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Chọn tệp để gửi"), QDir::homePath(),
        tr("Mọi tệp (*);;Ảnh (*.png *.jpg *.jpeg *.webp *.gif);;Video (*.mp4 *.mkv *.mov)"));
    if (!files.isEmpty())
        emit filesRequested(files);
}

void Composer::handleTextChanged()
{
    const bool hasText = !m_edit->toPlainText().trimmed().isEmpty();
    m_sendButton->setEnabled(hasText || m_editMessageId != 0);

    if (!hasText)
        return;

    // Chỉ báo "đang gõ" nhiều nhất 3 giây một lần.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_lastTypingSignal >= 3) {
        m_lastTypingSignal = now;
        emit typingActive();
    }
}
