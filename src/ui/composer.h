#pragma once

#include <QPlainTextEdit>
#include <QString>
#include <QStringList>
#include <QWidget>

class IconButton;
class QLabel;
class QVBoxLayout;

/*!
 * \brief Ô nhập tin nhắn tự giãn theo nội dung.
 */
class ComposerEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ComposerEdit(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void sendPressed();
    void escapePressed();
    void filesDropped(const QStringList &paths);
    void pastedImage(const QString &savedPath);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;

private slots:
    void updateHeight();

private:
    int m_lineHeight = 20;
};

/*!
 * \brief Khu vực soạn tin: trả lời / sửa, nút emoji, kèm tệp, gửi.
 */
class Composer : public QWidget
{
    Q_OBJECT

public:
    explicit Composer(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    void clearInput();
    void focusInput();

    //! Bật thanh "Đang trả lời"; truyền 0 để tắt.
    void setReplyTarget(qint64 messageId, const QString &sender, const QString &preview);
    qint64 replyTarget() const { return m_replyMessageId; }

    //! Bật chế độ sửa tin nhắn; truyền 0 để tắt.
    void setEditTarget(qint64 messageId, const QString &currentText);
    qint64 editTarget() const { return m_editMessageId; }

    void setSendEnabled(bool enabled);
    void setPlaceholder(const QString &text);

    //! Thông báo không thể gửi (kênh chỉ đọc, bị chặn…).
    void setBlockedReason(const QString &reason);

signals:
    void sendRequested(const QString &text);
    void editSubmitted(qint64 messageId, const QString &text);
    void filesRequested(const QStringList &paths);
    void typingActive();
    void emojiPanelRequested();
    void stickerPanelRequested();
    void replyCleared();
    void editCleared();
    void voiceNoteRequested();

private slots:
    void handleSend();
    void handleAttachClicked();
    void handleTextChanged();
    void refreshTheme();

private:
    void updateInfoBar();

    ComposerEdit *m_edit = nullptr;
    QWidget *m_infoBar = nullptr;
    QLabel *m_infoTitle = nullptr;
    QLabel *m_infoBody = nullptr;
    IconButton *m_infoClose = nullptr;
    IconButton *m_emojiButton = nullptr;
    IconButton *m_stickerButton = nullptr;
    IconButton *m_attachButton = nullptr;
    IconButton *m_sendButton = nullptr;
    QLabel *m_blockedLabel = nullptr;
    QWidget *m_inputRow = nullptr;

    qint64 m_replyMessageId = 0;
    qint64 m_editMessageId = 0;
    QString m_replySender;
    QString m_replyPreview;
    qint64 m_lastTypingSignal = 0;
};
