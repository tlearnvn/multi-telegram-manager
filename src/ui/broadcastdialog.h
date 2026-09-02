#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class AccountManager;
class TdAccount;
class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTimer;

/*!
 * \brief Gửi một tin nhắn tới nhiều cuộc trò chuyện, trên nhiều tài khoản.
 *
 * Đây là tính năng "quản lý" chính của ứng dụng đa tài khoản. Có giãn cách
 * giữa các lần gửi để tránh bị Telegram giới hạn tốc độ, kèm nhật ký kết quả
 * từng nơi gửi.
 */
class BroadcastDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BroadcastDialog(AccountManager *manager, QWidget *parent = nullptr);

signals:
    void statusMessage(const QString &text);

private slots:
    void pickTargets();
    void startSending();
    void stopSending();
    void sendNext();

private:
    struct Target
    {
        TdAccount *account = nullptr;
        qint64 chatId = 0;
        QString label;
    };

    void buildUi();
    void refreshTargetList();
    void appendLog(const QString &text, bool isError = false);
    void setRunning(bool running);

    AccountManager *m_manager;

    QPlainTextEdit *m_message = nullptr;
    QListWidget *m_targetList = nullptr;
    QListWidget *m_logList = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QCheckBox *m_stopOnError = nullptr;
    QLabel *m_summary = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_pickButton = nullptr;
    QString m_attachmentPath;
    QLabel *m_attachmentLabel = nullptr;

    QList<Target> m_targets;
    QTimer *m_timer = nullptr;
    int m_cursor = 0;
    int m_sent = 0;
    int m_failed = 0;
    bool m_running = false;
};
