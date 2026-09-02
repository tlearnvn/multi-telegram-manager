#pragma once

#include "model/chatentry.h"
#include "model/messageentry.h"
#include "model/userentry.h"
#include "td/tdenums.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

class QTimer;

/*!
 * \brief Một tài khoản Telegram = một client TDLib độc lập.
 *
 * Mỗi tài khoản có thư mục dữ liệu riêng trong data/accounts/<slug>, nên nhiều
 * tài khoản chạy song song không đụng nhau. Lớp này giữ toàn bộ trạng thái
 * (người dùng, chat, tin nhắn) và phát tín hiệu cho giao diện.
 */
class TdAccount : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,             //!< chưa mở client
        Starting,         //!< đã tạo client, đang chờ TDLib
        WaitPhone,        //!< cần số điện thoại
        WaitCode,         //!< cần mã xác thực
        WaitPassword,     //!< cần mật khẩu hai lớp
        WaitRegistration, //!< số mới, cần đăng ký tên
        WaitQrScan,       //!< đang chờ quét mã QR
        Ready,            //!< đã đăng nhập
        LoggingOut,
        Closed,
        Failed
    };
    Q_ENUM(State)

    //! Thông tin phụ trợ cho các bước đăng nhập.
    struct AuthPrompt
    {
        QString phoneNumber;
        QString codeSource;        //!< "Telegram", "SMS", "Cuộc gọi"…
        int codeLength = 5;
        int resendAfterSeconds = 0;
        QString passwordHint;
        QString recoveryEmailPattern;
        bool hasRecoveryEmail = false;
    };

    using ResultHandler = std::function<void(const QJsonObject &result, bool ok)>;

    explicit TdAccount(const QString &slug, QObject *parent = nullptr);
    ~TdAccount() override;

    // --- Thông tin định danh ---------------------------------------------
    QString slug() const { return m_slug; }
    int clientId() const { return m_clientId; }

    QString label() const;                     //!< tên hiển thị người dùng đặt
    void setLabel(const QString &label);
    QString accentColor() const { return m_accentColor; }
    void setAccentColor(const QString &hex);
    QString savedPhone() const { return m_savedPhone; }
    void setSavedPhone(const QString &phone) { m_savedPhone = phone; }

    State state() const { return m_state; }
    TdConnectionState connectionState() const { return m_connection; }
    QString lastError() const { return m_lastError; }
    AuthPrompt authPrompt() const { return m_prompt; }
    QString qrLink() const { return m_qrLink; }
    bool isReady() const { return m_state == State::Ready; }

    UserEntry me() const;
    qint64 myId() const { return m_myId; }
    QString displayName() const;               //!< nhãn hoặc tên Telegram

    // --- Vòng đời ---------------------------------------------------------
    bool open();          //!< tạo client TDLib và bắt đầu đăng nhập
    void close();         //!< đóng client, giữ nguyên dữ liệu đã lưu
    void logOut();        //!< đăng xuất khỏi Telegram (xoá phiên)

    // --- Các bước đăng nhập ----------------------------------------------
    void submitPhone(const QString &phone);
    void submitCode(const QString &code);
    void submitPassword(const QString &password);
    void submitRegistration(const QString &firstName, const QString &lastName);
    void requestQrLogin();
    void resendCode();
    void requestPasswordRecovery();

    // --- Truy cập dữ liệu -------------------------------------------------
    const UserEntry *user(qint64 userId) const;
    const ChatEntry *chat(qint64 chatId) const;
    QList<qint64> orderedChatIds(bool archived = false) const;
    int totalUnreadChats() const;
    int totalUnreadMessages() const;
    QString chatTitle(qint64 chatId) const;
    QString senderNameFor(const MessageEntry &message) const;

    // --- Hành động trên chat ---------------------------------------------
    void loadMoreChats(bool archived = false);
    void openChatSession(qint64 chatId);
    void closeChatSession(qint64 chatId);
    void loadHistory(qint64 chatId, qint64 fromMessageId, int limit = 40);
    void requestChatDetails(qint64 chatId);

    void sendText(qint64 chatId, const QString &text, qint64 replyToMessageId = 0,
                  bool disableWebPreview = false);
    void sendFile(qint64 chatId, const QString &filePath, const QString &caption,
                  qint64 replyToMessageId = 0);
    void editMessageText(qint64 chatId, qint64 messageId, const QString &text);
    void deleteMessages(qint64 chatId, const QList<qint64> &messageIds, bool revoke);
    void forwardMessages(qint64 toChatId, qint64 fromChatId,
                         const QList<qint64> &messageIds, bool asCopy);
    void viewMessages(qint64 chatId, const QList<qint64> &messageIds);
    void pinMessage(qint64 chatId, qint64 messageId, bool pinned);
    void sendChatAction(qint64 chatId, bool typing);
    void saveDraft(qint64 chatId, const QString &text, qint64 replyToMessageId);

    void setChatMuted(qint64 chatId, bool muted);
    void setChatPinned(qint64 chatId, bool pinned);
    void setChatArchived(qint64 chatId, bool archived);
    void setChatMarkedUnread(qint64 chatId, bool unread);
    void readAllChat(qint64 chatId);
    void leaveChat(qint64 chatId);
    void deleteChatHistory(qint64 chatId, bool removeFromList, bool revoke);
    void setUserBlocked(qint64 userId, bool blocked);

    // --- Tệp --------------------------------------------------------------
    void downloadFile(int fileId, int priority = 16);
    void cancelDownload(int fileId);
    QString localPathForFile(int fileId) const;

    // --- Tìm kiếm / tạo mới ----------------------------------------------
    void searchChatsLocal(const QString &query, const ResultHandler &handler);
    void searchPublicChats(const QString &query, const ResultHandler &handler);
    void searchMessagesInChat(qint64 chatId, const QString &query, const ResultHandler &handler);
    void searchMessagesGlobal(const QString &query, const ResultHandler &handler);
    void fetchContacts(const ResultHandler &handler);
    void createPrivateChat(qint64 userId, const ResultHandler &handler);
    void createGroup(const QString &title, const QList<qint64> &userIds, const ResultHandler &handler);
    void createChannel(const QString &title, const QString &description, bool megagroup,
                       const ResultHandler &handler);
    void joinByInviteLink(const QString &link, const ResultHandler &handler);
    void searchByUsername(const QString &username, const ResultHandler &handler);
    void fetchChatMembers(qint64 chatId, const ResultHandler &handler);
    void addChatMembers(qint64 chatId, const QList<qint64> &userIds, const ResultHandler &handler);
    void addContact(const QString &phone, const QString &firstName,
                    const QString &lastName, const ResultHandler &handler);

    // --- Bảo trì ----------------------------------------------------------
    void applyProxySettings();
    void setOnline(bool online);
    void fetchStorageStatistics(const ResultHandler &handler);
    void optimizeStorage(const ResultHandler &handler);

    //! Gửi một yêu cầu thô (dùng cho tính năng nâng cao / gỡ lỗi).
    void request(QJsonObject payload, ResultHandler handler = nullptr);

    //! Nhận dữ liệu từ TdTransport — chỉ AccountManager gọi.
    void handleIncoming(const QJsonObject &object);

signals:
    void stateChanged(TdAccount::State state);
    void connectionStateChanged(TdConnectionState state);
    void authPromptChanged();
    void qrLinkChanged(const QString &link);
    void errorOccurred(const QString &message);
    void profileChanged();

    void chatUpserted(qint64 chatId);
    void chatRemoved(qint64 chatId);
    void chatOrderChanged();
    void chatActionChanged(qint64 chatId);

    void newMessageArrived(qint64 chatId, qint64 messageId);
    void messageChanged(qint64 chatId, qint64 messageId);
    void messagesDeleted(qint64 chatId, const QList<qint64> &messageIds);
    void historyReady(qint64 chatId, const QList<MessageEntry> &messages, bool reachedOldest);
    void messageSendSucceeded(qint64 chatId, qint64 oldMessageId, qint64 newMessageId);
    void messageSendFailed(qint64 chatId, qint64 messageId, const QString &reason);

    void fileProgress(int fileId, qint64 downloaded, qint64 total);
    void fileReady(int fileId, const QString &localPath);

    void unreadCountsChanged();
    void notificationRequested(qint64 chatId, const QString &title, const QString &body);

public:
    //! Tra tin nhắn đã nạp trong bộ nhớ (rỗng nếu chưa có).
    const MessageEntry *cachedMessage(qint64 chatId, qint64 messageId) const;

    /*!
     * \brief \a limit tin nhắn mới nhất đang có trong bộ nhớ, xếp từ cũ đến mới.
     *
     * Dùng để hiển thị ngay khi mở lại một cuộc trò chuyện, không phải chờ
     * TDLib trả lịch sử về.
     */
    QList<MessageEntry> cachedMessages(qint64 chatId, int limit = 60) const;

private:
    void setState(State state);
    void setError(const QString &message);

    void send(const QJsonObject &payload);
    void sendTdlibParameters();
    void onAuthorizationState(const QJsonObject &authState);
    void onUpdate(const QString &type, const QJsonObject &update);
    void onFileUpdate(const QJsonObject &file);

    void upsertUserFromJson(const QJsonObject &user);
    void upsertChatFromJson(const QJsonObject &chat);
    void applyChatPositions(ChatEntry &entry, const QJsonArray &positions);
    void refreshChatStatusLine(ChatEntry &entry);
    void requestChatPhoto(ChatEntry &entry);
    void requestUserPhoto(UserEntry &entry);

    MessageEntry parseMessage(const QJsonObject &message) const;
    void fillSenderInfo(MessageEntry &entry) const;
    QString previewForMessage(const MessageEntry &entry) const;
    void rememberMessage(const MessageEntry &entry);
    void bootstrapAfterLogin();
    void maybeNotify(const MessageEntry &entry);

    QString m_slug;
    int m_clientId = -1;
    State m_state = State::Idle;
    TdConnectionState m_connection = TdConnectionState::Unknown;
    QString m_lastError;
    AuthPrompt m_prompt;
    QString m_qrLink;
    QString m_customLabel;
    QString m_accentColor;
    QString m_savedPhone;

    qint64 m_myId = 0;
    quint64 m_nextExtra = 1;
    QHash<QString, ResultHandler> m_handlers;

    QHash<qint64, UserEntry> m_users;
    QHash<qint64, ChatEntry> m_chats;
    QHash<qint64, QHash<qint64, MessageEntry>> m_messages; // chatId -> (messageId -> tin nhắn)
    QHash<qint64, QJsonObject> m_supergroups;   // supergroupId -> supergroup
    QHash<qint64, QJsonObject> m_basicGroups;   // basicGroupId -> basicGroup
    QHash<int, QJsonObject> m_files;            // fileId -> file
    QSet<qint64> m_openChats;
    QSet<int> m_photoRequests;
    QSet<qint64> m_pendingChatDetails;
    QSet<qint64> m_historyRetried;

    int m_unreadChats = 0;
    int m_unreadMessages = 0;
    bool m_bootstrapped = false;
    QTimer *m_typingTimer = nullptr;
    qint64 m_typingChatId = 0;
};
