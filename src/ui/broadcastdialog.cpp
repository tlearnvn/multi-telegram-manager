#include "ui/broadcastdialog.h"

#include "core/formatting.h"
#include "core/jsonutil.h"
#include "td/accountmanager.h"
#include "td/tdaccount.h"
#include "ui/chatpickerdialog.h"
#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

BroadcastDialog::BroadcastDialog(AccountManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Gửi tin hàng loạt"));
    setWindowIcon(Icons::appIcon());
    resize(680, 640);

    buildUi();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &BroadcastDialog::sendNext);
}

void BroadcastDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *intro = new QLabel(tr(
        "Soạn một tin nhắn rồi gửi tới nhiều cuộc trò chuyện — kể cả trên các "
        "tài khoản khác nhau. Hãy để giãn cách hợp lý để tránh bị Telegram "
        "giới hạn tốc độ."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    // --- Nội dung ---------------------------------------------------------
    auto *messageBox = new QGroupBox(tr("Nội dung"), this);
    auto *messageLayout = new QVBoxLayout(messageBox);
    m_message = new QPlainTextEdit(messageBox);
    m_message->setPlaceholderText(tr("Nội dung tin nhắn (hỗ trợ Markdown: **đậm**, "
                                     "__nghiêng__, `mã`)…"));
    m_message->setMinimumHeight(110);
    messageLayout->addWidget(m_message);

    auto *attachRow = new QHBoxLayout;
    auto *attachButton = new QPushButton(tr("Kèm tệp…"), messageBox);
    auto *clearAttach = new QPushButton(tr("Bỏ tệp"), messageBox);
    m_attachmentLabel = new QLabel(tr("Không kèm tệp"), messageBox);
    m_attachmentLabel->setStyleSheet(QStringLiteral("color: %1;")
        .arg(Theme::instance().colors().textSecondary.name()));
    attachRow->addWidget(attachButton);
    attachRow->addWidget(clearAttach);
    attachRow->addWidget(m_attachmentLabel, 1);
    messageLayout->addLayout(attachRow);
    root->addWidget(messageBox);

    connect(attachButton, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Chọn tệp gửi kèm"));
        if (file.isEmpty())
            return;
        m_attachmentPath = file;
        m_attachmentLabel->setText(tr("Kèm: %1 (%2)")
            .arg(QFileInfo(file).fileName(), Format::fileSize(QFileInfo(file).size())));
    });
    connect(clearAttach, &QPushButton::clicked, this, [this] {
        m_attachmentPath.clear();
        m_attachmentLabel->setText(tr("Không kèm tệp"));
    });

    // --- Nơi gửi ----------------------------------------------------------
    auto *targetBox = new QGroupBox(tr("Nơi gửi"), this);
    auto *targetLayout = new QVBoxLayout(targetBox);

    auto *targetRow = new QHBoxLayout;
    m_pickButton = new QPushButton(tr("Chọn cuộc trò chuyện…"), targetBox);
    m_pickButton->setProperty("accent", true);
    auto *clearTargets = new QPushButton(tr("Xoá danh sách"), targetBox);
    m_summary = new QLabel(tr("Chưa chọn nơi nào"), targetBox);
    targetRow->addWidget(m_pickButton);
    targetRow->addWidget(clearTargets);
    targetRow->addWidget(m_summary, 1);
    targetLayout->addLayout(targetRow);

    m_targetList = new QListWidget(targetBox);
    m_targetList->setMaximumHeight(130);
    targetLayout->addWidget(m_targetList);
    root->addWidget(targetBox);

    connect(m_pickButton, &QPushButton::clicked, this, &BroadcastDialog::pickTargets);
    connect(clearTargets, &QPushButton::clicked, this, [this] {
        m_targets.clear();
        refreshTargetList();
    });

    // --- Tuỳ chọn ---------------------------------------------------------
    auto *optionsRow = new QHBoxLayout;
    optionsRow->addWidget(new QLabel(tr("Giãn cách giữa hai lần gửi:"), this));
    m_delaySpin = new QSpinBox(this);
    m_delaySpin->setRange(0, 300);
    m_delaySpin->setValue(3);
    m_delaySpin->setSuffix(tr(" giây"));
    optionsRow->addWidget(m_delaySpin);

    m_stopOnError = new QCheckBox(tr("Dừng nếu gặp lỗi"), this);
    optionsRow->addWidget(m_stopOnError);
    optionsRow->addStretch(1);
    root->addLayout(optionsRow);

    // --- Tiến trình & nhật ký ---------------------------------------------
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    root->addWidget(m_progress);

    m_logList = new QListWidget(this);
    root->addWidget(m_logList, 1);

    // --- Nút ---------------------------------------------------------------
    auto *buttonRow = new QHBoxLayout;
    m_startButton = new QPushButton(tr("Bắt đầu gửi"), this);
    m_startButton->setProperty("accent", true);
    m_startButton->setMinimumHeight(40);
    m_stopButton = new QPushButton(tr("Dừng"), this);
    m_stopButton->setEnabled(false);
    auto *close = new QPushButton(tr("Đóng"), this);

    buttonRow->addWidget(m_startButton);
    buttonRow->addWidget(m_stopButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(close);
    root->addLayout(buttonRow);

    connect(m_startButton, &QPushButton::clicked, this, &BroadcastDialog::startSending);
    connect(m_stopButton, &QPushButton::clicked, this, &BroadcastDialog::stopSending);
    connect(close, &QPushButton::clicked, this, [this] {
        if (m_running) {
            if (QMessageBox::question(this, tr("Đang gửi"),
                    tr("Đang gửi dở. Dừng lại và đóng?")) != QMessageBox::Yes) {
                return;
            }
            stopSending();
        }
        accept();
    });
}

void BroadcastDialog::pickTargets()
{
    ChatPickerDialog picker(m_manager, nullptr, true, this);
    picker.setHeaderText(tr("Tích chọn mọi nơi cần gửi. Danh sách gộp tất cả tài khoản "
                            "đang đăng nhập."));
    if (picker.exec() != QDialog::Accepted)
        return;

    const QList<ChatPickerDialog::Selection> chosen = picker.selections();
    for (const ChatPickerDialog::Selection &selection : chosen) {
        bool duplicate = false;
        for (const Target &existing : m_targets) {
            if (existing.account == selection.account && existing.chatId == selection.chatId) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        Target target;
        target.account = selection.account;
        target.chatId = selection.chatId;
        target.label = QStringLiteral("%1 · %2")
                           .arg(selection.account->displayName(), selection.title);
        m_targets.append(target);
    }
    refreshTargetList();
}

void BroadcastDialog::refreshTargetList()
{
    m_targetList->clear();
    for (const Target &target : m_targets)
        m_targetList->addItem(target.label);

    m_summary->setText(m_targets.isEmpty()
        ? tr("Chưa chọn nơi nào")
        : tr("Đã chọn %1 nơi").arg(m_targets.size()));
    m_progress->setRange(0, qMax(1, m_targets.size()));
    m_progress->setValue(0);
}

void BroadcastDialog::appendLog(const QString &text, bool isError)
{
    auto *item = new QListWidgetItem(
        QStringLiteral("[%1] %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                      text),
        m_logList);
    if (isError)
        item->setForeground(Theme::instance().colors().danger);
    m_logList->scrollToBottom();
}

void BroadcastDialog::setRunning(bool running)
{
    m_running = running;
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_pickButton->setEnabled(!running);
    m_message->setReadOnly(running);
    m_delaySpin->setEnabled(!running);
}

void BroadcastDialog::startSending()
{
    const QString text = m_message->toPlainText().trimmed();
    if (text.isEmpty() && m_attachmentPath.isEmpty()) {
        QMessageBox::information(this, tr("Thiếu nội dung"),
                                 tr("Hãy nhập nội dung hoặc chọn tệp gửi kèm."));
        return;
    }
    if (m_targets.isEmpty()) {
        QMessageBox::information(this, tr("Chưa chọn nơi gửi"),
                                 tr("Hãy bấm “Chọn cuộc trò chuyện…” trước."));
        return;
    }

    const auto answer = QMessageBox::question(this, tr("Xác nhận"),
        tr("Gửi tin này tới %1 cuộc trò chuyện?").arg(m_targets.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_cursor = 0;
    m_sent = 0;
    m_failed = 0;
    m_logList->clear();
    m_progress->setRange(0, m_targets.size());
    m_progress->setValue(0);
    setRunning(true);
    appendLog(tr("Bắt đầu gửi tới %1 nơi…").arg(m_targets.size()));
    sendNext();
}

void BroadcastDialog::stopSending()
{
    if (!m_running)
        return;
    m_timer->stop();
    setRunning(false);
    appendLog(tr("Đã dừng. Thành công %1, lỗi %2, còn lại %3.")
                  .arg(m_sent).arg(m_failed).arg(m_targets.size() - m_cursor));
}

void BroadcastDialog::sendNext()
{
    if (!m_running)
        return;

    if (m_cursor >= m_targets.size()) {
        setRunning(false);
        appendLog(tr("Hoàn tất: thành công %1, lỗi %2.").arg(m_sent).arg(m_failed));
        emit statusMessage(tr("Gửi hàng loạt xong: %1 thành công, %2 lỗi.")
                               .arg(m_sent).arg(m_failed));
        return;
    }

    const Target target = m_targets.at(m_cursor);
    ++m_cursor;
    m_progress->setValue(m_cursor);

    if (!target.account || !target.account->isReady()) {
        ++m_failed;
        appendLog(tr("%1 — tài khoản chưa sẵn sàng, bỏ qua.").arg(target.label), true);
        m_timer->start(200);
        return;
    }

    const QString text = m_message->toPlainText().trimmed();

    if (!m_attachmentPath.isEmpty())
        target.account->sendFile(target.chatId, m_attachmentPath, text);
    else
        target.account->sendText(target.chatId, text);

    ++m_sent;
    appendLog(tr("%1 — đã gửi.").arg(target.label));

    // sendMessage của TDLib là bất đồng bộ; giãn cách giúp tránh FLOOD_WAIT.
    m_timer->start(qMax(200, m_delaySpin->value() * 1000));
}
