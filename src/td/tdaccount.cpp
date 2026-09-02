#include "td/tdaccount.h"

#include "core/apppaths.h"
#include "core/formatting.h"
#include "core/jsonutil.h"
#include "core/logging.h"
#include "core/settingsstore.h"
#include "td/tdloader.h"
#include "td/tdtransport.h"

#include "version.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>

#include <algorithm>

namespace {

constexpr int kChatPageSize = 40;

//! TDLib gửi kèm "@extra" để ta ghép phản hồi với yêu cầu đã gửi.
const QString kExtraKey = QStringLiteral("@extra");

QString textFromMaybeFormatted(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isObject())
        return value.toObject().value(QStringLiteral("text")).toString();
    return QString();
}

TextSpan::Style spanStyleFor(const QString &tdType)
{
    if (tdType == QStringLiteral("textEntityTypeBold"))           return TextSpan::Style::Bold;
    if (tdType == QStringLiteral("textEntityTypeItalic"))         return TextSpan::Style::Italic;
    if (tdType == QStringLiteral("textEntityTypeUnderline"))      return TextSpan::Style::Underline;
    if (tdType == QStringLiteral("textEntityTypeStrikethrough"))  return TextSpan::Style::Strike;
    if (tdType == QStringLiteral("textEntityTypeCode"))           return TextSpan::Style::Mono;
    if (tdType == QStringLiteral("textEntityTypePre"))            return TextSpan::Style::Code;
    if (tdType == QStringLiteral("textEntityTypePreCode"))        return TextSpan::Style::Code;
    if (tdType == QStringLiteral("textEntityTypeTextUrl"))        return TextSpan::Style::Link;
    if (tdType == QStringLiteral("textEntityTypeUrl"))            return TextSpan::Style::Link;
    if (tdType == QStringLiteral("textEntityTypeEmailAddress"))   return TextSpan::Style::Link;
    if (tdType == QStringLiteral("textEntityTypePhoneNumber"))    return TextSpan::Style::Link;
    if (tdType == QStringLiteral("textEntityTypeMention"))        return TextSpan::Style::Mention;
    if (tdType == QStringLiteral("textEntityTypeMentionName"))    return TextSpan::Style::Mention;
    if (tdType == QStringLiteral("textEntityTypeHashtag"))        return TextSpan::Style::Hashtag;
    if (tdType == QStringLiteral("textEntityTypeCashtag"))        return TextSpan::Style::Hashtag;
    if (tdType == QStringLiteral("textEntityTypeBotCommand"))     return TextSpan::Style::Hashtag;
    if (tdType == QStringLiteral("textEntityTypeSpoiler"))        return TextSpan::Style::Spoiler;
    return TextSpan::Style::Plain;
}

QList<TextSpan> parseEntities(const QJsonArray &entities, const QString &text)
{
    QList<TextSpan> spans;
    for (const QJsonValue &value : entities) {
        const QJsonObject entity = value.toObject();
        const QJsonObject typeObject = Json::object(entity, QStringLiteral("type"));
        TextSpan span;
        span.offset = Json::integer(entity, QStringLiteral("offset"));
        span.length = Json::integer(entity, QStringLiteral("length"));
        span.style = spanStyleFor(Json::type(typeObject));
        if (span.style == TextSpan::Style::Link) {
            span.url = Json::str(typeObject, QStringLiteral("url"));
            if (span.url.isEmpty() && span.offset >= 0
                && span.offset + span.length <= text.size()) {
                span.url = text.mid(span.offset, span.length);
            }
        }
        if (span.length > 0 && span.style != TextSpan::Style::Plain)
            spans.append(span);
    }
    return spans;
}

//! Lấy tệp lớn nhất mà vẫn hợp lý trong danh sách kích cỡ ảnh.
QJsonObject bestPhotoSize(const QJsonArray &sizes, int *width, int *height)
{
    QJsonObject best;
    int bestArea = -1;
    for (const QJsonValue &value : sizes) {
        const QJsonObject size = value.toObject();
        const int w = Json::integer(size, QStringLiteral("width"));
        const int h = Json::integer(size, QStringLiteral("height"));
        const int area = w * h;
        // Giới hạn ~1600px để không tải ảnh khổng lồ chỉ để xem trước.
        if (area > bestArea && w <= 1600 && h <= 1600) {
            bestArea = area;
            best = size;
            if (width) *width = w;
            if (height) *height = h;
        }
    }
    if (best.isEmpty() && !sizes.isEmpty()) {
        best = sizes.last().toObject();
        if (width) *width = Json::integer(best, QStringLiteral("width"));
        if (height) *height = Json::integer(best, QStringLiteral("height"));
    }
    return best;
}

int fileIdOf(const QJsonObject &fileObject)
{
    return Json::integer(fileObject, QStringLiteral("id"));
}

QString localPathOf(const QJsonObject &fileObject)
{
    const QJsonObject local = Json::object(fileObject, QStringLiteral("local"));
    if (Json::boolean(local, QStringLiteral("is_downloading_completed")))
        return Json::str(local, QStringLiteral("path"));
    return QString();
}

QString actionLabel(const QString &actionType)
{
    if (actionType == QStringLiteral("chatActionTyping"))
        return QStringLiteral("đang gõ…");
    if (actionType == QStringLiteral("chatActionRecordingVoiceNote"))
        return QStringLiteral("đang ghi âm…");
    if (actionType == QStringLiteral("chatActionUploadingVoiceNote"))
        return QStringLiteral("đang gửi tin thoại…");
    if (actionType == QStringLiteral("chatActionRecordingVideoNote"))
        return QStringLiteral("đang ghi video…");
    if (actionType == QStringLiteral("chatActionUploadingVideoNote"))
        return QStringLiteral("đang gửi video tròn…");
    if (actionType == QStringLiteral("chatActionUploadingPhoto"))
        return QStringLiteral("đang gửi ảnh…");
    if (actionType == QStringLiteral("chatActionUploadingVideo"))
        return QStringLiteral("đang gửi video…");
    if (actionType == QStringLiteral("chatActionUploadingDocument"))
        return QStringLiteral("đang gửi tệp…");
    if (actionType == QStringLiteral("chatActionChoosingSticker"))
        return QStringLiteral("đang chọn nhãn dán…");
    if (actionType == QStringLiteral("chatActionChoosingLocation"))
        return QStringLiteral("đang chọn vị trí…");
    if (actionType == QStringLiteral("chatActionWatchingAnimations"))
        return QStringLiteral("đang xem ảnh động…");
    return QString();
}

QString codeSourceLabel(const QString &codeType)
{
    if (codeType == QStringLiteral("authenticationCodeTypeTelegramMessage"))
        return QStringLiteral("Mã đã gửi vào ứng dụng Telegram trên thiết bị khác");
    if (codeType == QStringLiteral("authenticationCodeTypeSms")
        || codeType == QStringLiteral("authenticationCodeTypeSmsWord")
        || codeType == QStringLiteral("authenticationCodeTypeSmsPhrase"))
        return QStringLiteral("Mã đã gửi bằng tin nhắn SMS");
    if (codeType == QStringLiteral("authenticationCodeTypeCall"))
        return QStringLiteral("Bạn sẽ nhận được cuộc gọi đọc mã");
    if (codeType == QStringLiteral("authenticationCodeTypeFlashCall")
        || codeType == QStringLiteral("authenticationCodeTypeMissedCall"))
        return QStringLiteral("Sẽ có cuộc gọi nhỡ; mã là các số cuối của số gọi đến");
    if (codeType == QStringLiteral("authenticationCodeTypeFragment"))
        return QStringLiteral("Mã được gửi qua Fragment");
    return QStringLiteral("Nhập mã xác thực Telegram vừa gửi");
}

UserEntry::Presence presenceFromStatus(const QString &statusType)
{
    if (statusType == QStringLiteral("userStatusOnline"))       return UserEntry::Presence::Online;
    if (statusType == QStringLiteral("userStatusOffline"))      return UserEntry::Presence::Offline;
    if (statusType == QStringLiteral("userStatusRecently"))     return UserEntry::Presence::Recently;
    if (statusType == QStringLiteral("userStatusLastWeek"))     return UserEntry::Presence::LastWeek;
    if (statusType == QStringLiteral("userStatusLastMonth"))    return UserEntry::Presence::LastMonth;
    return UserEntry::Presence::Unknown;
}

QString initialsOf(const QString &name)
{
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QStringLiteral("?");
    if (parts.size() == 1)
        return parts.first().left(1).toUpper();
    return (parts.first().left(1) + parts.last().left(1)).toUpper();
}

int colorIndexFor(qint64 id)
{
    return static_cast<int>(qAbs(id) % 7);
}

} // namespace

// ---------------------------------------------------------------------------

TdAccount::TdAccount(const QString &slug, QObject *parent)
    : QObject(parent)
    , m_slug(slug)
    , m_accentColor(QStringLiteral("#2ea6ff"))
{
    m_typingTimer = new QTimer(this);
    m_typingTimer->setInterval(4000);
    connect(m_typingTimer, &QTimer::timeout, this, [this] {
        if (m_typingChatId != 0)
            sendChatAction(m_typingChatId, true);
    });
}

TdAccount::~TdAccount()
{
    if (m_clientId >= 0 && m_state != State::Closed)
        send(Json::request(QStringLiteral("close")));
}

QString TdAccount::label() const
{
    return m_customLabel;
}

void TdAccount::setLabel(const QString &label)
{
    if (m_customLabel == label)
        return;
    m_customLabel = label;
    emit profileChanged();
}

void TdAccount::setAccentColor(const QString &hex)
{
    if (m_accentColor == hex)
        return;
    m_accentColor = hex;
    emit profileChanged();
}

UserEntry TdAccount::me() const
{
    return m_users.value(m_myId);
}

QString TdAccount::displayName() const
{
    if (!m_customLabel.isEmpty())
        return m_customLabel;
    const UserEntry self = me();
    if (self.id != 0)
        return self.displayName();
    if (!m_savedPhone.isEmpty())
        return Format::maskPhone(m_savedPhone);
    return QStringLiteral("Tài khoản mới");
}

// --- Vòng đời --------------------------------------------------------------

bool TdAccount::open()
{
    if (m_clientId >= 0 && !m_closeRequested
        && m_state != State::Closed && m_state != State::Failed) {
        return true;
    }

    if (!TdTransport::instance().start()) {
        setError(QStringLiteral("Chưa nạp được thư viện TDLib."));
        setState(State::Failed);
        return false;
    }

    m_clientId = TdLoader::instance().createClientId();
    if (m_clientId < 0) {
        setError(QStringLiteral("Không tạo được client TDLib."));
        setState(State::Failed);
        return false;
    }

    m_bootstrapped = false;
    m_closeRequested = false;
    setState(State::Starting);

    // Gửi một yêu cầu bất kỳ để TDLib bắt đầu bơm update về client mới.
    send(Json::request(QStringLiteral("getAuthorizationState")));
    return true;
}

void TdAccount::close()
{
    if (m_clientId < 0 || m_closeRequested)
        return;

    // Không đặt ngay trạng thái Closed: TDLib còn phải ghi xong cơ sở dữ liệu
    // rồi mới gửi authorizationStateClosed. Thoát trước lúc đó có thể để lại
    // cơ sở dữ liệu ghi dở.
    m_closeRequested = true;
    send(Json::request(QStringLiteral("close")));
}

void TdAccount::logOut()
{
    if (m_clientId < 0)
        return;
    setState(State::LoggingOut);
    send(Json::request(QStringLiteral("logOut")));
}

// --- Đăng nhập -------------------------------------------------------------

void TdAccount::submitPhone(const QString &phone)
{
    const QString normalized = Format::normalizePhone(phone);
    m_savedPhone = normalized;

    QJsonObject payload = Json::request(QStringLiteral("setAuthenticationPhoneNumber"));
    payload.insert(QStringLiteral("phone_number"), normalized);

    QJsonObject settings = Json::request(QStringLiteral("phoneNumberAuthenticationSettings"));
    settings.insert(QStringLiteral("allow_flash_call"), false);
    settings.insert(QStringLiteral("allow_missed_call"), false);
    settings.insert(QStringLiteral("is_current_phone_number"), false);
    settings.insert(QStringLiteral("allow_sms_retriever_api"), false);
    payload.insert(QStringLiteral("settings"), settings);

    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::submitCode(const QString &code)
{
    QJsonObject payload = Json::request(QStringLiteral("checkAuthenticationCode"));
    payload.insert(QStringLiteral("code"), code.trimmed());
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::submitPassword(const QString &password)
{
    QJsonObject payload = Json::request(QStringLiteral("checkAuthenticationPassword"));
    payload.insert(QStringLiteral("password"), password);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::submitRegistration(const QString &firstName, const QString &lastName)
{
    QJsonObject payload = Json::request(QStringLiteral("registerUser"));
    payload.insert(QStringLiteral("first_name"), firstName);
    payload.insert(QStringLiteral("last_name"), lastName);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::requestQrLogin()
{
    QJsonObject payload = Json::request(QStringLiteral("requestQrCodeAuthentication"));
    payload.insert(QStringLiteral("other_user_ids"), QJsonArray());
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::resendCode()
{
    request(Json::request(QStringLiteral("resendAuthenticationCode")),
            [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::requestPasswordRecovery()
{
    request(Json::request(QStringLiteral("requestAuthenticationPasswordRecovery")),
            [this](const QJsonObject &result, bool ok) {
        if (!ok)
            setError(Json::str(result, QStringLiteral("message")));
    });
}

// --- Truy cập dữ liệu ------------------------------------------------------

const UserEntry *TdAccount::user(qint64 userId) const
{
    auto it = m_users.constFind(userId);
    return it == m_users.constEnd() ? nullptr : &it.value();
}

const ChatEntry *TdAccount::chat(qint64 chatId) const
{
    auto it = m_chats.constFind(chatId);
    return it == m_chats.constEnd() ? nullptr : &it.value();
}

QList<qint64> TdAccount::orderedChatIds(bool archived) const
{
    QList<const ChatEntry *> pool;
    pool.reserve(m_chats.size());
    for (auto it = m_chats.constBegin(); it != m_chats.constEnd(); ++it) {
        if (it.value().inArchive != archived)
            continue;
        if (it.value().order == 0 && !it.value().isPinned)
            continue;
        pool.append(&it.value());
    }

    std::sort(pool.begin(), pool.end(), [](const ChatEntry *a, const ChatEntry *b) {
        if (a->isPinned != b->isPinned)
            return a->isPinned;
        if (a->order != b->order)
            return a->order > b->order;
        return a->id > b->id;
    });

    QList<qint64> ids;
    ids.reserve(pool.size());
    for (const ChatEntry *entry : pool)
        ids.append(entry->id);
    return ids;
}

int TdAccount::totalUnreadChats() const
{
    return m_unreadChats;
}

int TdAccount::totalUnreadMessages() const
{
    return m_unreadMessages;
}

QString TdAccount::chatTitle(qint64 chatId) const
{
    if (const ChatEntry *entry = chat(chatId))
        return entry->title;
    return QStringLiteral("Cuộc trò chuyện %1").arg(chatId);
}

QString TdAccount::senderNameFor(const MessageEntry &message) const
{
    if (message.senderUserId != 0) {
        if (const UserEntry *entry = user(message.senderUserId))
            return entry->displayName();
        return QStringLiteral("Người dùng %1").arg(message.senderUserId);
    }
    if (message.senderChatId != 0)
        return chatTitle(message.senderChatId);
    return QString();
}

const MessageEntry *TdAccount::cachedMessage(qint64 chatId, qint64 messageId) const
{
    auto chatIt = m_messages.constFind(chatId);
    if (chatIt == m_messages.constEnd())
        return nullptr;
    auto messageIt = chatIt.value().constFind(messageId);
    return messageIt == chatIt.value().constEnd() ? nullptr : &messageIt.value();
}

QList<MessageEntry> TdAccount::cachedMessages(qint64 chatId, int limit) const
{
    auto chatIt = m_messages.constFind(chatId);
    if (chatIt == m_messages.constEnd())
        return {};

    QList<qint64> ids = chatIt.value().keys();
    std::sort(ids.begin(), ids.end());
    if (limit > 0 && ids.size() > limit)
        ids = ids.mid(ids.size() - limit);

    QList<MessageEntry> result;
    result.reserve(ids.size());
    for (qint64 id : ids)
        result.append(chatIt.value().value(id));
    return result;
}

// --- Gửi yêu cầu -----------------------------------------------------------

void TdAccount::send(const QJsonObject &payload)
{
    if (m_clientId < 0)
        return;
    TdTransport::instance().send(m_clientId, payload);
}

void TdAccount::request(QJsonObject payload, ResultHandler handler)
{
    if (m_clientId < 0)
        return;
    if (handler) {
        const QString extra = QStringLiteral("r%1").arg(m_nextExtra++);
        payload.insert(kExtraKey, extra);
        m_handlers.insert(extra, std::move(handler));
    }
    TdTransport::instance().send(m_clientId, payload);
}

// --- Xử lý dữ liệu về ------------------------------------------------------

void TdAccount::handleIncoming(const QJsonObject &object)
{
    const QString type = Json::type(object);

    // 1. Phản hồi cho một yêu cầu cụ thể.
    const QString extra = Json::str(object, kExtraKey);
    if (!extra.isEmpty()) {
        auto it = m_handlers.find(extra);
        if (it != m_handlers.end()) {
            ResultHandler handler = it.value();
            m_handlers.erase(it);
            if (handler)
                handler(object, type != QStringLiteral("error"));
        }
    }

    if (type == QStringLiteral("error")) {
        const QString message = Json::str(object, QStringLiteral("message"));
        // Lỗi 404 của loadChats chỉ nghĩa là "hết chat", không phải sự cố.
        if (message != QStringLiteral("Have no more chats"))
            qCWarning(logTd) << m_slug << "lỗi TDLib:" << Json::integer(object, QStringLiteral("code")) << message;
        return;
    }

    if (type == QStringLiteral("updateAuthorizationState")) {
        onAuthorizationState(Json::object(object, QStringLiteral("authorization_state")));
        return;
    }
    // getAuthorizationState trả về thẳng đối tượng authorizationState*.
    if (type.startsWith(QStringLiteral("authorizationState"))) {
        onAuthorizationState(object);
        return;
    }

    if (type.startsWith(QStringLiteral("update")))
        onUpdate(type, object);
}

void TdAccount::onAuthorizationState(const QJsonObject &authState)
{
    const QString type = Json::type(authState);
    qCInfo(logTd) << m_slug << "trạng thái đăng nhập:" << type;

    if (type == QStringLiteral("authorizationStateWaitTdlibParameters")) {
        sendTdlibParameters();
    } else if (type == QStringLiteral("authorizationStateWaitEncryptionKey")) {
        QJsonObject payload = Json::request(QStringLiteral("checkDatabaseEncryptionKey"));
        payload.insert(QStringLiteral("encryption_key"), QString());
        send(payload);
    } else if (type == QStringLiteral("authorizationStateWaitPhoneNumber")) {
        m_qrLink.clear();
        setState(State::WaitPhone);
    } else if (type == QStringLiteral("authorizationStateWaitEmailAddress")) {
        setError(QStringLiteral("Tài khoản này yêu cầu xác thực qua email — "
                                "hiện chưa hỗ trợ, hãy đăng nhập bằng mã QR."));
        setState(State::WaitPhone);
    } else if (type == QStringLiteral("authorizationStateWaitCode")) {
        const QJsonObject info = Json::object(authState, QStringLiteral("code_info"));
        const QJsonObject codeType = Json::object(info, QStringLiteral("type"));
        m_prompt.phoneNumber = Json::str(info, QStringLiteral("phone_number"), m_savedPhone);
        m_prompt.codeSource = codeSourceLabel(Json::type(codeType));
        m_prompt.codeLength = qBound(3, Json::integer(codeType, QStringLiteral("length"), 5), 8);
        m_prompt.resendAfterSeconds = Json::integer(info, QStringLiteral("timeout"));
        if (!m_prompt.phoneNumber.isEmpty())
            m_savedPhone = m_prompt.phoneNumber;
        emit authPromptChanged();
        setState(State::WaitCode);
    } else if (type == QStringLiteral("authorizationStateWaitPassword")) {
        m_prompt.passwordHint = Json::str(authState, QStringLiteral("password_hint"));
        m_prompt.hasRecoveryEmail = Json::boolean(authState, QStringLiteral("has_recovery_email_address"));
        m_prompt.recoveryEmailPattern = Json::str(authState, QStringLiteral("recovery_email_address_pattern"));
        emit authPromptChanged();
        setState(State::WaitPassword);
    } else if (type == QStringLiteral("authorizationStateWaitRegistration")) {
        setState(State::WaitRegistration);
    } else if (type == QStringLiteral("authorizationStateWaitOtherDeviceConfirmation")) {
        m_qrLink = Json::str(authState, QStringLiteral("link"));
        emit qrLinkChanged(m_qrLink);
        setState(State::WaitQrScan);
    } else if (type == QStringLiteral("authorizationStateReady")) {
        m_qrLink.clear();
        setState(State::Ready);
        bootstrapAfterLogin();
    } else if (type == QStringLiteral("authorizationStateLoggingOut")) {
        setState(State::LoggingOut);
    } else if (type == QStringLiteral("authorizationStateClosing")) {
        // chờ authorizationStateClosed
    } else if (type == QStringLiteral("authorizationStateClosed")) {
        m_clientId = -1;
        m_closeRequested = false;
        m_users.clear();
        m_chats.clear();
        m_messages.clear();
        setState(State::Closed);
    }
}

void TdAccount::sendTdlibParameters()
{
    const SettingsStore &settings = SettingsStore::instance();
    const QString dbDir = AppPaths::ensureDir(AppPaths::accountDatabaseDir(m_slug));
    const QString filesDir = AppPaths::ensureDir(AppPaths::accountFilesDir(m_slug));

    QJsonObject payload = Json::request(QStringLiteral("setTdlibParameters"));
    payload.insert(QStringLiteral("use_test_dc"), false);
    payload.insert(QStringLiteral("database_directory"), QDir::toNativeSeparators(dbDir));
    payload.insert(QStringLiteral("files_directory"), QDir::toNativeSeparators(filesDir));
    payload.insert(QStringLiteral("database_encryption_key"), QString());
    payload.insert(QStringLiteral("use_file_database"), true);
    payload.insert(QStringLiteral("use_chat_info_database"), true);
    payload.insert(QStringLiteral("use_message_database"), true);
    payload.insert(QStringLiteral("use_secret_chats"), false);
    payload.insert(QStringLiteral("api_id"), settings.apiId());
    payload.insert(QStringLiteral("api_hash"), settings.apiHash());
    payload.insert(QStringLiteral("system_language_code"), QStringLiteral("vi"));
    payload.insert(QStringLiteral("device_model"), QStringLiteral(APP_NAME));
    payload.insert(QStringLiteral("system_version"), QSysInfo::prettyProductName());
    payload.insert(QStringLiteral("application_version"), QStringLiteral(APP_VERSION_STRING));

    request(payload, [this, payload](const QJsonObject &result, bool ok) {
        if (ok)
            return;

        // TDLib cũ (< 1.8.6) nhận tham số trong đối tượng lồng "parameters".
        qCInfo(logTd) << m_slug << "thử lại setTdlibParameters theo định dạng cũ";
        QJsonObject legacyInner = payload;
        legacyInner.remove(QStringLiteral("@type"));
        legacyInner.remove(kExtraKey);
        legacyInner.remove(QStringLiteral("database_encryption_key"));
        legacyInner.insert(QStringLiteral("@type"), QStringLiteral("tdlibParameters"));
        legacyInner.insert(QStringLiteral("enable_storage_optimizer"), true);
        legacyInner.insert(QStringLiteral("ignore_file_names"), false);

        QJsonObject legacy = Json::request(QStringLiteral("setTdlibParameters"));
        legacy.insert(QStringLiteral("parameters"), legacyInner);
        request(legacy, [this](const QJsonObject &innerResult, bool innerOk) {
            if (!innerOk) {
                setError(QStringLiteral("TDLib từ chối tham số khởi tạo: %1")
                             .arg(Json::str(innerResult, QStringLiteral("message"))));
            }
        });
    });
}

void TdAccount::bootstrapAfterLogin()
{
    if (m_bootstrapped)
        return;
    m_bootstrapped = true;

    applyProxySettings();

    request(Json::request(QStringLiteral("getMe")), [this](const QJsonObject &result, bool ok) {
        if (ok) {
            upsertUserFromJson(result);
            m_myId = Json::int64(result, QStringLiteral("id"));
            if (m_savedPhone.isEmpty())
                m_savedPhone = Json::str(result, QStringLiteral("phone_number"));
            emit profileChanged();
        }
    });

    loadMoreChats(false);
    loadMoreChats(true);
    setOnline(true);
}

// --- Cập nhật --------------------------------------------------------------

void TdAccount::onUpdate(const QString &type, const QJsonObject &update)
{
    if (type == QStringLiteral("updateNewChat")) {
        upsertChatFromJson(Json::object(update, QStringLiteral("chat")));
        return;
    }

    if (type == QStringLiteral("updateUser")) {
        upsertUserFromJson(Json::object(update, QStringLiteral("user")));
        return;
    }

    if (type == QStringLiteral("updateUserStatus")) {
        const qint64 userId = Json::int64(update, QStringLiteral("user_id"));
        auto it = m_users.find(userId);
        if (it != m_users.end()) {
            const QJsonObject status = Json::object(update, QStringLiteral("status"));
            it->presence = presenceFromStatus(Json::type(status));
            it->lastSeen = Json::int64(status, QStringLiteral("was_online"));
            // Cập nhật dòng trạng thái của chat riêng tương ứng.
            for (auto chatIt = m_chats.begin(); chatIt != m_chats.end(); ++chatIt) {
                if (chatIt->kind == ChatEntry::Kind::Private && chatIt->relatedUserId == userId) {
                    refreshChatStatusLine(*chatIt);
                    emit chatUpserted(chatIt->id);
                }
            }
        }
        return;
    }

    if (type == QStringLiteral("updateSupergroup")) {
        const QJsonObject group = Json::object(update, QStringLiteral("supergroup"));
        m_supergroups.insert(Json::int64(group, QStringLiteral("id")), group);
        for (auto it = m_chats.begin(); it != m_chats.end(); ++it) {
            if (it->supergroupId == Json::int64(group, QStringLiteral("id"))) {
                it->username = Json::str(group, QStringLiteral("username"));
                it->isVerified = Json::boolean(group, QStringLiteral("is_verified"));
                it->memberCount = Json::integer(group, QStringLiteral("member_count"), it->memberCount);
                refreshChatStatusLine(*it);
                emit chatUpserted(it->id);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateSupergroupFullInfo")) {
        const qint64 id = Json::int64(update, QStringLiteral("supergroup_id"));
        const QJsonObject info = Json::object(update, QStringLiteral("supergroup_full_info"));
        for (auto it = m_chats.begin(); it != m_chats.end(); ++it) {
            if (it->supergroupId == id) {
                it->memberCount = Json::integer(info, QStringLiteral("member_count"), it->memberCount);
                refreshChatStatusLine(*it);
                emit chatUpserted(it->id);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateBasicGroup")) {
        const QJsonObject group = Json::object(update, QStringLiteral("basic_group"));
        const qint64 id = Json::int64(group, QStringLiteral("id"));
        m_basicGroups.insert(id, group);
        for (auto it = m_chats.begin(); it != m_chats.end(); ++it) {
            if (it->basicGroupId == id) {
                it->memberCount = Json::integer(group, QStringLiteral("member_count"), it->memberCount);
                refreshChatStatusLine(*it);
                emit chatUpserted(it->id);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateFile")) {
        onFileUpdate(Json::object(update, QStringLiteral("file")));
        return;
    }

    if (type == QStringLiteral("updateConnectionState")) {
        const QString state = Json::type(Json::object(update, QStringLiteral("state")));
        TdConnectionState next = TdConnectionState::Unknown;
        if (state == QStringLiteral("connectionStateWaitingForNetwork"))
            next = TdConnectionState::WaitingForNetwork;
        else if (state == QStringLiteral("connectionStateConnectingToProxy"))
            next = TdConnectionState::ConnectingToProxy;
        else if (state == QStringLiteral("connectionStateConnecting"))
            next = TdConnectionState::Connecting;
        else if (state == QStringLiteral("connectionStateUpdating"))
            next = TdConnectionState::Updating;
        else if (state == QStringLiteral("connectionStateReady"))
            next = TdConnectionState::Ready;
        if (next != m_connection) {
            m_connection = next;
            emit connectionStateChanged(next);
        }
        return;
    }

    if (type == QStringLiteral("updateOption")) {
        const QString name = Json::str(update, QStringLiteral("name"));
        const QJsonObject value = Json::object(update, QStringLiteral("value"));
        if (name == QStringLiteral("my_id"))
            m_myId = Json::int64(value, QStringLiteral("value"));
        return;
    }

    if (type == QStringLiteral("updateUnreadChatCount")) {
        if (Json::type(Json::object(update, QStringLiteral("chat_list")))
            == QStringLiteral("chatListMain")) {
            m_unreadChats = Json::integer(update, QStringLiteral("unread_unmuted_count"));
            emit unreadCountsChanged();
        }
        return;
    }

    if (type == QStringLiteral("updateUnreadMessageCount")) {
        if (Json::type(Json::object(update, QStringLiteral("chat_list")))
            == QStringLiteral("chatListMain")) {
            m_unreadMessages = Json::integer(update, QStringLiteral("unread_unmuted_count"));
            emit unreadCountsChanged();
        }
        return;
    }

    // --- Cập nhật thuộc tính chat -----------------------------------------
    const qint64 chatId = Json::int64(update, QStringLiteral("chat_id"));
    auto chatIt = chatId != 0 ? m_chats.find(chatId) : m_chats.end();

    if (type == QStringLiteral("updateChatLastMessage")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject lastMessage = Json::object(update, QStringLiteral("last_message"));
        if (!lastMessage.isEmpty()) {
            const MessageEntry entry = parseMessage(lastMessage);
            rememberMessage(entry);
            chatIt->lastMessageId = entry.id;
            chatIt->lastMessageDate = entry.date;
            chatIt->lastMessageOutgoing = entry.isOutgoing;
            chatIt->lastMessagePreview = previewForMessage(entry);
            chatIt->lastMessageSender = entry.senderName;
            chatIt->lastMessageSendState = entry.sendState == MessageEntry::SendState::Sending ? 1
                                        : entry.sendState == MessageEntry::SendState::Failed ? 2 : 0;
        }
        applyChatPositions(*chatIt, Json::array(update, QStringLiteral("positions")));
        emit chatUpserted(chatId);
        emit chatOrderChanged();
        return;
    }

    if (type == QStringLiteral("updateChatPosition")) {
        if (chatIt == m_chats.end())
            return;
        QJsonArray positions;
        positions.append(update.value(QStringLiteral("position")));
        applyChatPositions(*chatIt, positions);
        emit chatUpserted(chatId);
        emit chatOrderChanged();
        return;
    }

    if (type == QStringLiteral("updateChatTitle")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->title = Json::str(update, QStringLiteral("title"));
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatPhoto")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject photo = Json::object(update, QStringLiteral("photo"));
        const QJsonObject small = Json::object(photo, QStringLiteral("small"));
        chatIt->photoFileId = fileIdOf(small);
        chatIt->photoPath = localPathOf(small);
        requestChatPhoto(*chatIt);
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatReadInbox")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->unreadCount = Json::integer(update, QStringLiteral("unread_count"));
        chatIt->lastReadInboxMessageId = Json::int64(update, QStringLiteral("last_read_inbox_message_id"));
        emit chatUpserted(chatId);
        emit unreadCountsChanged();
        return;
    }

    if (type == QStringLiteral("updateChatReadOutbox")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->lastReadOutboxMessageId = Json::int64(update, QStringLiteral("last_read_outbox_message_id"));
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatUnreadMentionCount")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->unreadMentionCount = Json::integer(update, QStringLiteral("unread_mention_count"));
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatIsMarkedAsUnread")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->isMarkedUnread = Json::boolean(update, QStringLiteral("is_marked_as_unread"));
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatNotificationSettings")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject settings = Json::object(update, QStringLiteral("notification_settings"));
        chatIt->isMuted = Json::integer(settings, QStringLiteral("mute_for")) > 0;
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatPermissions")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject permissions = Json::object(update, QStringLiteral("permissions"));
        chatIt->canSendMessages = Json::boolean(permissions, QStringLiteral("can_send_basic_messages"), true)
                               || Json::boolean(permissions, QStringLiteral("can_send_messages"), true);
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatOnlineMemberCount")) {
        if (chatIt == m_chats.end())
            return;
        chatIt->onlineMemberCount = Json::integer(update, QStringLiteral("online_member_count"));
        refreshChatStatusLine(*chatIt);
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatDraftMessage")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject draft = Json::object(update, QStringLiteral("draft_message"));
        const QJsonObject content = Json::object(draft, QStringLiteral("input_message_text"));
        chatIt->draftText = textFromMaybeFormatted(content.value(QStringLiteral("text")));
        applyChatPositions(*chatIt, Json::array(update, QStringLiteral("positions")));
        emit chatUpserted(chatId);
        return;
    }

    if (type == QStringLiteral("updateChatAction") || type == QStringLiteral("updateUserChatAction")) {
        if (chatIt == m_chats.end())
            return;
        const QJsonObject action = Json::object(update, QStringLiteral("action"));
        const QString label = actionLabel(Json::type(action));
        qint64 senderId = Json::int64(update, QStringLiteral("user_id"));
        if (senderId == 0)
            senderId = Json::int64(Json::object(update, QStringLiteral("sender_id")), QStringLiteral("user_id"));
        if (senderId == m_myId)
            return;

        if (label.isEmpty()) {
            chatIt->actionText.clear();
        } else if (chatIt->isGroupLike() && senderId != 0) {
            const UserEntry *sender = user(senderId);
            chatIt->actionText = sender
                ? QStringLiteral("%1 %2").arg(sender->firstName.isEmpty() ? sender->displayName()
                                                                          : sender->firstName, label)
                : label;
        } else {
            chatIt->actionText = label;
        }
        emit chatActionChanged(chatId);
        emit chatUpserted(chatId);
        return;
    }

    // --- Cập nhật tin nhắn -------------------------------------------------
    if (type == QStringLiteral("updateNewMessage")) {
        const MessageEntry entry = parseMessage(Json::object(update, QStringLiteral("message")));
        rememberMessage(entry);
        if (chatIt != m_chats.end() && !chatIt->actionText.isEmpty()) {
            chatIt->actionText.clear();
            emit chatActionChanged(entry.chatId);
        }
        emit newMessageArrived(entry.chatId, entry.id);
        maybeNotify(entry);
        return;
    }

    if (type == QStringLiteral("updateMessageContent")) {
        const qint64 messageId = Json::int64(update, QStringLiteral("message_id"));
        auto chatMessages = m_messages.find(chatId);
        if (chatMessages != m_messages.end()) {
            auto messageIt = chatMessages->find(messageId);
            if (messageIt != chatMessages->end()) {
                QJsonObject rebuilt;
                rebuilt.insert(QStringLiteral("id"), static_cast<double>(messageId));
                rebuilt.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
                rebuilt.insert(QStringLiteral("content"), update.value(QStringLiteral("new_content")));
                MessageEntry updated = *messageIt;
                const MessageEntry parsed = parseMessage(rebuilt);
                updated.kind = parsed.kind;
                updated.text = parsed.text;
                updated.spans = parsed.spans;
                updated.mediaFileId = parsed.mediaFileId;
                updated.mediaPath = parsed.mediaPath;
                updated.thumbFileId = parsed.thumbFileId;
                updated.thumbPath = parsed.thumbPath;
                updated.mediaWidth = parsed.mediaWidth;
                updated.mediaHeight = parsed.mediaHeight;
                updated.fileName = parsed.fileName;
                updated.fileSize = parsed.fileSize;
                updated.durationSeconds = parsed.durationSeconds;
                *messageIt = updated;
                emit messageChanged(chatId, messageId);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateMessageEdited")) {
        const qint64 messageId = Json::int64(update, QStringLiteral("message_id"));
        auto chatMessages = m_messages.find(chatId);
        if (chatMessages != m_messages.end()) {
            auto messageIt = chatMessages->find(messageId);
            if (messageIt != chatMessages->end()) {
                messageIt->editDate = Json::int64(update, QStringLiteral("edit_date"));
                emit messageChanged(chatId, messageId);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateMessageInteractionInfo")) {
        const qint64 messageId = Json::int64(update, QStringLiteral("message_id"));
        auto chatMessages = m_messages.find(chatId);
        if (chatMessages != m_messages.end()) {
            auto messageIt = chatMessages->find(messageId);
            if (messageIt != chatMessages->end()) {
                const QJsonObject info = Json::object(update, QStringLiteral("interaction_info"));
                messageIt->viewCount = Json::integer(info, QStringLiteral("view_count"));
                messageIt->forwardCount = Json::integer(info, QStringLiteral("forward_count"));
                emit messageChanged(chatId, messageId);
            }
        }
        return;
    }

    if (type == QStringLiteral("updateDeleteMessages")) {
        if (!Json::boolean(update, QStringLiteral("is_permanent"), true))
            return;
        const QList<qint64> ids = Json::toInt64List(Json::array(update, QStringLiteral("message_ids")));
        auto chatMessages = m_messages.find(chatId);
        if (chatMessages != m_messages.end()) {
            for (qint64 id : ids)
                chatMessages->remove(id);
        }
        emit messagesDeleted(chatId, ids);
        return;
    }

    if (type == QStringLiteral("updateMessageSendSucceeded")) {
        const QJsonObject message = Json::object(update, QStringLiteral("message"));
        const qint64 oldId = Json::int64(update, QStringLiteral("old_message_id"));
        const MessageEntry entry = parseMessage(message);
        auto chatMessages = m_messages.find(entry.chatId);
        if (chatMessages != m_messages.end())
            chatMessages->remove(oldId);
        rememberMessage(entry);
        emit messageSendSucceeded(entry.chatId, oldId, entry.id);
        return;
    }

    if (type == QStringLiteral("updateMessageSendFailed")) {
        const QJsonObject message = Json::object(update, QStringLiteral("message"));
        const qint64 oldId = Json::int64(update, QStringLiteral("old_message_id"));
        MessageEntry entry = parseMessage(message);
        entry.sendState = MessageEntry::SendState::Failed;
        entry.failureReason = Json::str(update, QStringLiteral("error_message"),
                                        Json::strAt(update, { QStringLiteral("error"),
                                                              QStringLiteral("message") }));
        rememberMessage(entry);
        emit messageSendFailed(entry.chatId, oldId, entry.failureReason);
        return;
    }
}

void TdAccount::onFileUpdate(const QJsonObject &file)
{
    const int fileId = fileIdOf(file);
    if (fileId == 0)
        return;

    m_files.insert(fileId, file);

    const QJsonObject local = Json::object(file, QStringLiteral("local"));
    const qint64 total = Json::int64(file, QStringLiteral("size"),
                                     Json::int64(file, QStringLiteral("expected_size")));
    const qint64 downloaded = Json::int64(local, QStringLiteral("downloaded_size"));
    const bool completed = Json::boolean(local, QStringLiteral("is_downloading_completed"));
    const QString path = Json::str(local, QStringLiteral("path"));

    emit fileProgress(fileId, downloaded, total);

    if (!completed || path.isEmpty())
        return;

    m_photoRequests.remove(fileId);

    // Gắn đường dẫn vừa tải vào các đối tượng đang chờ.
    for (auto it = m_chats.begin(); it != m_chats.end(); ++it) {
        if (it->photoFileId == fileId && it->photoPath != path) {
            it->photoPath = path;
            emit chatUpserted(it->id);
        }
    }
    for (auto it = m_users.begin(); it != m_users.end(); ++it) {
        if (it->photoFileId == fileId && it->photoPath != path) {
            it->photoPath = path;
            if (it->id == m_myId)
                emit profileChanged();
        }
    }
    for (auto chatIt = m_messages.begin(); chatIt != m_messages.end(); ++chatIt) {
        for (auto messageIt = chatIt->begin(); messageIt != chatIt->end(); ++messageIt) {
            bool touched = false;
            if (messageIt->mediaFileId == fileId) {
                messageIt->mediaPath = path;
                messageIt->isDownloaded = true;
                messageIt->isDownloading = false;
                touched = true;
            }
            if (messageIt->thumbFileId == fileId) {
                messageIt->thumbPath = path;
                touched = true;
            }
            if (touched)
                emit messageChanged(chatIt.key(), messageIt->id);
        }
    }

    emit fileReady(fileId, path);
}

// --- Người dùng & chat -----------------------------------------------------

void TdAccount::upsertUserFromJson(const QJsonObject &user)
{
    const qint64 id = Json::int64(user, QStringLiteral("id"));
    if (id == 0)
        return;

    UserEntry entry = m_users.value(id);
    entry.id = id;
    entry.firstName = Json::str(user, QStringLiteral("first_name"), entry.firstName);
    entry.lastName = Json::str(user, QStringLiteral("last_name"), entry.lastName);
    entry.phoneNumber = Json::str(user, QStringLiteral("phone_number"), entry.phoneNumber);
    entry.isVerified = Json::boolean(user, QStringLiteral("is_verified"), entry.isVerified);
    entry.isSupport = Json::boolean(user, QStringLiteral("is_support"), entry.isSupport);
    entry.isContact = Json::boolean(user, QStringLiteral("is_contact"), entry.isContact);

    const QString userType = Json::type(Json::object(user, QStringLiteral("type")));
    if (!userType.isEmpty()) {
        entry.isBot = userType == QStringLiteral("userTypeBot");
        entry.isDeleted = userType == QStringLiteral("userTypeDeleted");
    }

    // "username" (cũ) hoặc "usernames.editable_username" (mới).
    QString username = Json::str(user, QStringLiteral("username"));
    if (username.isEmpty()) {
        const QJsonObject usernames = Json::object(user, QStringLiteral("usernames"));
        username = Json::str(usernames, QStringLiteral("editable_username"));
        if (username.isEmpty()) {
            const QJsonArray active = Json::array(usernames, QStringLiteral("active_usernames"));
            if (!active.isEmpty())
                username = active.first().toString();
        }
    }
    if (!username.isEmpty())
        entry.username = username;

    if (user.contains(QStringLiteral("status"))) {
        const QJsonObject status = Json::object(user, QStringLiteral("status"));
        entry.presence = presenceFromStatus(Json::type(status));
        entry.lastSeen = Json::int64(status, QStringLiteral("was_online"));
    }

    const QJsonObject photo = Json::object(user, QStringLiteral("profile_photo"));
    if (!photo.isEmpty()) {
        const QJsonObject small = Json::object(photo, QStringLiteral("small"));
        entry.photoFileId = fileIdOf(small);
        const QString path = localPathOf(small);
        if (!path.isEmpty())
            entry.photoPath = path;
    }

    m_users.insert(id, entry);
    requestUserPhoto(m_users[id]);

    if (id == m_myId)
        emit profileChanged();
}

void TdAccount::upsertChatFromJson(const QJsonObject &chat)
{
    const qint64 id = Json::int64(chat, QStringLiteral("id"));
    if (id == 0)
        return;

    ChatEntry entry = m_chats.value(id);
    entry.id = id;
    entry.title = Json::str(chat, QStringLiteral("title"), entry.title);

    const QJsonObject chatType = Json::object(chat, QStringLiteral("type"));
    const QString kindName = Json::type(chatType);
    if (kindName == QStringLiteral("chatTypePrivate")) {
        entry.kind = ChatEntry::Kind::Private;
        entry.relatedUserId = Json::int64(chatType, QStringLiteral("user_id"));
    } else if (kindName == QStringLiteral("chatTypeSecret")) {
        entry.kind = ChatEntry::Kind::Secret;
        entry.relatedUserId = Json::int64(chatType, QStringLiteral("user_id"));
    } else if (kindName == QStringLiteral("chatTypeBasicGroup")) {
        entry.kind = ChatEntry::Kind::BasicGroup;
        entry.basicGroupId = Json::int64(chatType, QStringLiteral("basic_group_id"));
    } else if (kindName == QStringLiteral("chatTypeSupergroup")) {
        entry.kind = Json::boolean(chatType, QStringLiteral("is_channel"))
            ? ChatEntry::Kind::Channel : ChatEntry::Kind::Supergroup;
        entry.supergroupId = Json::int64(chatType, QStringLiteral("supergroup_id"));
    }

    if (entry.title.isEmpty() && entry.relatedUserId != 0) {
        if (const UserEntry *peer = user(entry.relatedUserId))
            entry.title = peer->displayName();
    }

    const QJsonObject photo = Json::object(chat, QStringLiteral("photo"));
    if (!photo.isEmpty()) {
        const QJsonObject small = Json::object(photo, QStringLiteral("small"));
        entry.photoFileId = fileIdOf(small);
        const QString path = localPathOf(small);
        if (!path.isEmpty())
            entry.photoPath = path;
    }

    entry.unreadCount = Json::integer(chat, QStringLiteral("unread_count"), entry.unreadCount);
    entry.unreadMentionCount = Json::integer(chat, QStringLiteral("unread_mention_count"), entry.unreadMentionCount);
    entry.isMarkedUnread = Json::boolean(chat, QStringLiteral("is_marked_as_unread"), entry.isMarkedUnread);
    entry.lastReadInboxMessageId = Json::int64(chat, QStringLiteral("last_read_inbox_message_id"), entry.lastReadInboxMessageId);
    entry.lastReadOutboxMessageId = Json::int64(chat, QStringLiteral("last_read_outbox_message_id"), entry.lastReadOutboxMessageId);

    const QJsonObject notifications = Json::object(chat, QStringLiteral("notification_settings"));
    if (!notifications.isEmpty())
        entry.isMuted = Json::integer(notifications, QStringLiteral("mute_for")) > 0;

    const QJsonObject permissions = Json::object(chat, QStringLiteral("permissions"));
    if (!permissions.isEmpty()) {
        entry.canSendMessages = Json::boolean(permissions, QStringLiteral("can_send_basic_messages"), true)
                             || Json::boolean(permissions, QStringLiteral("can_send_messages"), true);
    }

    if (chat.contains(QStringLiteral("is_blocked")))
        entry.isBlocked = Json::boolean(chat, QStringLiteral("is_blocked"));
    else if (chat.contains(QStringLiteral("block_list")))
        entry.isBlocked = !Json::object(chat, QStringLiteral("block_list")).isEmpty();

    entry.isForum = Json::boolean(chat, QStringLiteral("is_forum"), entry.isForum);

    const QJsonObject draft = Json::object(chat, QStringLiteral("draft_message"));
    if (!draft.isEmpty()) {
        const QJsonObject content = Json::object(draft, QStringLiteral("input_message_text"));
        entry.draftText = textFromMaybeFormatted(content.value(QStringLiteral("text")));
    }

    const QJsonObject lastMessage = Json::object(chat, QStringLiteral("last_message"));
    if (!lastMessage.isEmpty()) {
        const MessageEntry message = parseMessage(lastMessage);
        rememberMessage(message);
        entry.lastMessageId = message.id;
        entry.lastMessageDate = message.date;
        entry.lastMessageOutgoing = message.isOutgoing;
        entry.lastMessagePreview = previewForMessage(message);
        entry.lastMessageSender = message.senderName;
    }

    applyChatPositions(entry, Json::array(chat, QStringLiteral("positions")));
    refreshChatStatusLine(entry);

    m_chats.insert(id, entry);
    requestChatPhoto(m_chats[id]);

    emit chatUpserted(id);
    emit chatOrderChanged();
}

void TdAccount::applyChatPositions(ChatEntry &entry, const QJsonArray &positions)
{
    if (positions.isEmpty())
        return;

    for (const QJsonValue &value : positions) {
        const QJsonObject position = value.toObject();
        if (position.isEmpty())
            continue;
        const QString listType = Json::type(Json::object(position, QStringLiteral("list")));
        const qint64 order = Json::int64(position, QStringLiteral("order"));
        const bool pinned = Json::boolean(position, QStringLiteral("is_pinned"));

        if (listType == QStringLiteral("chatListMain")) {
            if (order != 0) {
                entry.order = order;
                entry.isPinned = pinned;
                entry.inArchive = false;
            } else if (!entry.inArchive) {
                // order = 0 ở danh sách chính nghĩa là chat rời khỏi danh sách.
                // Nếu chat đang ở mục lưu trữ thì bỏ qua, kẻo xoá mất thứ tự
                // trong mục lưu trữ (TDLib gửi hai cập nhật vị trí rời nhau).
                entry.order = 0;
                entry.isPinned = false;
            }
        } else if (listType == QStringLiteral("chatListArchive")) {
            if (order != 0) {
                entry.inArchive = true;
                entry.order = order;
                entry.isPinned = pinned;
            }
        }
    }
}

void TdAccount::refreshChatStatusLine(ChatEntry &entry)
{
    switch (entry.kind) {
    case ChatEntry::Kind::Private:
    case ChatEntry::Kind::Secret: {
        const UserEntry *peer = user(entry.relatedUserId);
        if (!peer) {
            entry.statusLine = QStringLiteral("đang tải…");
            break;
        }
        if (peer->isBot) {
            entry.statusLine = QStringLiteral("bot");
        } else {
            switch (peer->presence) {
            case UserEntry::Presence::Online:
                entry.statusLine = QStringLiteral("đang hoạt động");
                break;
            case UserEntry::Presence::Offline:
                entry.statusLine = peer->lastSeen > 0
                    ? QStringLiteral("hoạt động %1").arg(Format::relative(peer->lastSeen))
                    : QStringLiteral("ngoại tuyến");
                break;
            case UserEntry::Presence::Recently:
                entry.statusLine = QStringLiteral("hoạt động gần đây");
                break;
            case UserEntry::Presence::LastWeek:
                entry.statusLine = QStringLiteral("hoạt động trong tuần qua");
                break;
            case UserEntry::Presence::LastMonth:
                entry.statusLine = QStringLiteral("hoạt động trong tháng qua");
                break;
            case UserEntry::Presence::Unknown:
                entry.statusLine = QStringLiteral("ngoại tuyến");
                break;
            }
        }
        break;
    }
    case ChatEntry::Kind::BasicGroup:
    case ChatEntry::Kind::Supergroup:
        if (entry.memberCount > 0) {
            entry.statusLine = Format::countLabel(entry.memberCount, QStringLiteral("thành viên"));
            if (entry.onlineMemberCount > 0) {
                entry.statusLine += QStringLiteral(", %1 đang hoạt động")
                    .arg(entry.onlineMemberCount);
            }
        } else {
            entry.statusLine = QStringLiteral("nhóm");
        }
        break;
    case ChatEntry::Kind::Channel:
        entry.statusLine = entry.memberCount > 0
            ? Format::countLabel(entry.memberCount, QStringLiteral("người theo dõi"))
            : QStringLiteral("kênh");
        break;
    case ChatEntry::Kind::Unknown:
        entry.statusLine.clear();
        break;
    }
}

void TdAccount::requestChatPhoto(ChatEntry &entry)
{
    if (entry.photoFileId == 0 || !entry.photoPath.isEmpty())
        return;
    if (m_photoRequests.contains(entry.photoFileId))
        return;
    m_photoRequests.insert(entry.photoFileId);
    downloadFile(entry.photoFileId, 32);
}

void TdAccount::requestUserPhoto(UserEntry &entry)
{
    if (entry.photoFileId == 0 || !entry.photoPath.isEmpty())
        return;
    if (m_photoRequests.contains(entry.photoFileId))
        return;
    m_photoRequests.insert(entry.photoFileId);
    downloadFile(entry.photoFileId, 32);
}

// --- Phân tích tin nhắn ----------------------------------------------------

MessageEntry TdAccount::parseMessage(const QJsonObject &message) const
{
    MessageEntry entry;
    entry.id = Json::int64(message, QStringLiteral("id"));
    entry.chatId = Json::int64(message, QStringLiteral("chat_id"));
    entry.date = Json::int64(message, QStringLiteral("date"));
    entry.editDate = Json::int64(message, QStringLiteral("edit_date"));
    entry.isOutgoing = Json::boolean(message, QStringLiteral("is_outgoing"));
    entry.isPinned = Json::boolean(message, QStringLiteral("is_pinned"));
    entry.canBeEdited = Json::boolean(message, QStringLiteral("can_be_edited"));
    entry.canBeDeletedForAll = Json::boolean(message, QStringLiteral("can_be_deleted_for_all_users"));
    entry.canBeForwarded = Json::boolean(message, QStringLiteral("can_be_forwarded"), true);
    entry.containsUnreadMention = Json::boolean(message, QStringLiteral("contains_unread_mention"));

    const QJsonObject sender = Json::object(message, QStringLiteral("sender_id"));
    entry.senderUserId = Json::int64(sender, QStringLiteral("user_id"));
    entry.senderChatId = Json::int64(sender, QStringLiteral("chat_id"));

    const QString sendingState = Json::type(Json::object(message, QStringLiteral("sending_state")));
    if (sendingState == QStringLiteral("messageSendingStatePending"))
        entry.sendState = MessageEntry::SendState::Sending;
    else if (sendingState == QStringLiteral("messageSendingStateFailed"))
        entry.sendState = MessageEntry::SendState::Failed;

    // Trả lời: định dạng mới ("reply_to") và cũ ("reply_to_message_id").
    entry.replyToMessageId = Json::int64(message, QStringLiteral("reply_to_message_id"));
    if (entry.replyToMessageId == 0) {
        const QJsonObject replyTo = Json::object(message, QStringLiteral("reply_to"));
        if (Json::type(replyTo) == QStringLiteral("messageReplyToMessage"))
            entry.replyToMessageId = Json::int64(replyTo, QStringLiteral("message_id"));
    }

    const QJsonObject forwardInfo = Json::object(message, QStringLiteral("forward_info"));
    if (!forwardInfo.isEmpty()) {
        const QJsonObject origin = Json::object(forwardInfo, QStringLiteral("origin"));
        const QString originType = Json::type(origin);
        if (originType == QStringLiteral("messageForwardOriginUser")
            || originType == QStringLiteral("messageOriginUser")) {
            const qint64 userId = Json::int64(origin, QStringLiteral("sender_user_id"));
            if (const UserEntry *peer = user(userId))
                entry.forwardFromName = peer->displayName();
            else
                entry.forwardFromName = QStringLiteral("Người dùng %1").arg(userId);
        } else if (originType == QStringLiteral("messageForwardOriginChat")
                   || originType == QStringLiteral("messageOriginChat")) {
            entry.forwardFromName = chatTitle(Json::int64(origin, QStringLiteral("sender_chat_id")));
        } else if (originType == QStringLiteral("messageForwardOriginChannel")
                   || originType == QStringLiteral("messageOriginChannel")) {
            entry.forwardFromName = chatTitle(Json::int64(origin, QStringLiteral("chat_id")));
        } else {
            entry.forwardFromName = Json::str(origin, QStringLiteral("sender_name"),
                                              QStringLiteral("người dùng ẩn"));
        }
    }

    const QJsonObject interaction = Json::object(message, QStringLiteral("interaction_info"));
    if (!interaction.isEmpty()) {
        entry.viewCount = Json::integer(interaction, QStringLiteral("view_count"));
        entry.forwardCount = Json::integer(interaction, QStringLiteral("forward_count"));

        QStringList reactions;
        const QJsonArray list = Json::array(Json::object(interaction, QStringLiteral("reactions")),
                                            QStringLiteral("reactions"));
        const QJsonArray source = list.isEmpty()
            ? Json::array(interaction, QStringLiteral("reactions")) : list;
        for (const QJsonValue &value : source) {
            const QJsonObject reaction = value.toObject();
            QString emoji = Json::str(reaction, QStringLiteral("reaction"));
            if (emoji.isEmpty())
                emoji = Json::strAt(reaction, { QStringLiteral("type"), QStringLiteral("emoji") });
            const int count = Json::integer(reaction, QStringLiteral("total_count"));
            if (!emoji.isEmpty() && count > 0)
                reactions << QStringLiteral("%1 %2").arg(emoji).arg(count);
        }
        entry.reactionSummary = reactions.join(QStringLiteral("   "));
    }

    // --- Nội dung ---------------------------------------------------------
    const QJsonObject content = Json::object(message, QStringLiteral("content"));
    const QString contentType = Json::type(content);

    auto readCaption = [&content, &entry] {
        const QJsonObject caption = Json::object(content, QStringLiteral("caption"));
        entry.text = caption.value(QStringLiteral("text")).toString();
        entry.spans = parseEntities(Json::array(caption, QStringLiteral("entities")), entry.text);
    };

    auto readFile = [&entry](const QJsonObject &fileObject) {
        entry.mediaFileId = fileIdOf(fileObject);
        entry.fileSize = Json::int64(fileObject, QStringLiteral("size"),
                                     Json::int64(fileObject, QStringLiteral("expected_size")));
        const QJsonObject local = Json::object(fileObject, QStringLiteral("local"));
        entry.downloadedSize = Json::int64(local, QStringLiteral("downloaded_size"));
        entry.isDownloading = Json::boolean(local, QStringLiteral("is_downloading_active"));
        entry.isDownloaded = Json::boolean(local, QStringLiteral("is_downloading_completed"));
        if (entry.isDownloaded)
            entry.mediaPath = Json::str(local, QStringLiteral("path"));
    };

    auto readThumb = [&entry](const QJsonObject &owner) {
        QJsonObject thumbnail = Json::object(owner, QStringLiteral("thumbnail"));
        if (thumbnail.isEmpty())
            thumbnail = Json::object(owner, QStringLiteral("album_cover_thumbnail"));
        if (thumbnail.isEmpty())
            return;
        const QJsonObject file = Json::object(thumbnail, QStringLiteral("file"));
        entry.thumbFileId = fileIdOf(file);
        entry.thumbPath = localPathOf(file);
    };

    if (contentType == QStringLiteral("messageText")) {
        entry.kind = MessageEntry::Kind::Text;
        const QJsonObject text = Json::object(content, QStringLiteral("text"));
        entry.text = text.value(QStringLiteral("text")).toString();
        entry.spans = parseEntities(Json::array(text, QStringLiteral("entities")), entry.text);
    } else if (contentType == QStringLiteral("messagePhoto")) {
        entry.kind = MessageEntry::Kind::Photo;
        readCaption();
        const QJsonObject photo = Json::object(content, QStringLiteral("photo"));
        int width = 0;
        int height = 0;
        const QJsonObject size = bestPhotoSize(Json::array(photo, QStringLiteral("sizes")), &width, &height);
        entry.mediaWidth = width;
        entry.mediaHeight = height;
        readFile(Json::object(size, QStringLiteral("photo")));
    } else if (contentType == QStringLiteral("messageVideo")) {
        entry.kind = MessageEntry::Kind::Video;
        readCaption();
        const QJsonObject video = Json::object(content, QStringLiteral("video"));
        entry.mediaWidth = Json::integer(video, QStringLiteral("width"));
        entry.mediaHeight = Json::integer(video, QStringLiteral("height"));
        entry.durationSeconds = Json::integer(video, QStringLiteral("duration"));
        entry.fileName = Json::str(video, QStringLiteral("file_name"));
        entry.mimeType = Json::str(video, QStringLiteral("mime_type"));
        readThumb(video);
        readFile(Json::object(video, QStringLiteral("video")));
    } else if (contentType == QStringLiteral("messageAnimation")) {
        entry.kind = MessageEntry::Kind::Animation;
        readCaption();
        const QJsonObject animation = Json::object(content, QStringLiteral("animation"));
        entry.mediaWidth = Json::integer(animation, QStringLiteral("width"));
        entry.mediaHeight = Json::integer(animation, QStringLiteral("height"));
        entry.durationSeconds = Json::integer(animation, QStringLiteral("duration"));
        entry.fileName = Json::str(animation, QStringLiteral("file_name"));
        readThumb(animation);
        readFile(Json::object(animation, QStringLiteral("animation")));
    } else if (contentType == QStringLiteral("messageAudio")) {
        entry.kind = MessageEntry::Kind::Audio;
        readCaption();
        const QJsonObject audio = Json::object(content, QStringLiteral("audio"));
        entry.durationSeconds = Json::integer(audio, QStringLiteral("duration"));
        entry.audioTitle = Json::str(audio, QStringLiteral("title"));
        entry.performer = Json::str(audio, QStringLiteral("performer"));
        entry.fileName = Json::str(audio, QStringLiteral("file_name"));
        entry.mimeType = Json::str(audio, QStringLiteral("mime_type"));
        readThumb(audio);
        readFile(Json::object(audio, QStringLiteral("audio")));
    } else if (contentType == QStringLiteral("messageVoiceNote")) {
        entry.kind = MessageEntry::Kind::Voice;
        readCaption();
        const QJsonObject voice = Json::object(content, QStringLiteral("voice_note"));
        entry.durationSeconds = Json::integer(voice, QStringLiteral("duration"));
        entry.mimeType = Json::str(voice, QStringLiteral("mime_type"));
        readFile(Json::object(voice, QStringLiteral("voice")));
    } else if (contentType == QStringLiteral("messageDocument")) {
        entry.kind = MessageEntry::Kind::Document;
        readCaption();
        const QJsonObject document = Json::object(content, QStringLiteral("document"));
        entry.fileName = Json::str(document, QStringLiteral("file_name"));
        entry.mimeType = Json::str(document, QStringLiteral("mime_type"));
        readThumb(document);
        readFile(Json::object(document, QStringLiteral("document")));
    } else if (contentType == QStringLiteral("messageSticker")) {
        entry.kind = MessageEntry::Kind::Sticker;
        const QJsonObject sticker = Json::object(content, QStringLiteral("sticker"));
        entry.mediaWidth = Json::integer(sticker, QStringLiteral("width"));
        entry.mediaHeight = Json::integer(sticker, QStringLiteral("height"));
        entry.text = Json::str(sticker, QStringLiteral("emoji"));
        readThumb(sticker);
        readFile(Json::object(sticker, QStringLiteral("sticker")));
    } else if (contentType == QStringLiteral("messageVideoNote")) {
        entry.kind = MessageEntry::Kind::VideoNote;
        const QJsonObject note = Json::object(content, QStringLiteral("video_note"));
        entry.durationSeconds = Json::integer(note, QStringLiteral("duration"));
        entry.mediaWidth = entry.mediaHeight = Json::integer(note, QStringLiteral("length"));
        readThumb(note);
        readFile(Json::object(note, QStringLiteral("video")));
    } else if (contentType == QStringLiteral("messageLocation")) {
        entry.kind = MessageEntry::Kind::Location;
        const QJsonObject location = Json::object(content, QStringLiteral("location"));
        entry.text = QStringLiteral("Vị trí: %1, %2")
            .arg(Json::real(location, QStringLiteral("latitude")), 0, 'f', 5)
            .arg(Json::real(location, QStringLiteral("longitude")), 0, 'f', 5);
    } else if (contentType == QStringLiteral("messageVenue")) {
        entry.kind = MessageEntry::Kind::Venue;
        const QJsonObject venue = Json::object(content, QStringLiteral("venue"));
        entry.text = QStringLiteral("%1\n%2")
            .arg(Json::str(venue, QStringLiteral("title")),
                 Json::str(venue, QStringLiteral("address")));
    } else if (contentType == QStringLiteral("messageContact")) {
        entry.kind = MessageEntry::Kind::Contact;
        const QJsonObject contact = Json::object(content, QStringLiteral("contact"));
        entry.text = QStringLiteral("%1 %2\n%3")
            .arg(Json::str(contact, QStringLiteral("first_name")),
                 Json::str(contact, QStringLiteral("last_name")),
                 Json::str(contact, QStringLiteral("phone_number")))
            .trimmed();
    } else if (contentType == QStringLiteral("messagePoll")) {
        entry.kind = MessageEntry::Kind::Poll;
        const QJsonObject poll = Json::object(content, QStringLiteral("poll"));
        QStringList lines;
        lines << textFromMaybeFormatted(poll.value(QStringLiteral("question")));
        const QJsonArray options = Json::array(poll, QStringLiteral("options"));
        for (const QJsonValue &value : options) {
            const QJsonObject option = value.toObject();
            lines << QStringLiteral("• %1 — %2 phiếu")
                        .arg(textFromMaybeFormatted(option.value(QStringLiteral("text"))))
                        .arg(Json::integer(option, QStringLiteral("voter_count")));
        }
        entry.text = lines.join(QLatin1Char('\n'));
    } else if (contentType.startsWith(QStringLiteral("messageChat"))
               || contentType == QStringLiteral("messagePinMessage")
               || contentType == QStringLiteral("messageBasicGroupChatCreate")
               || contentType == QStringLiteral("messageSupergroupChatCreate")
               || contentType == QStringLiteral("messageCall")
               || contentType == QStringLiteral("messageContactRegistered")
               || contentType == QStringLiteral("messageVideoChatStarted")
               || contentType == QStringLiteral("messageVideoChatEnded")
               || contentType == QStringLiteral("messageScreenshotTaken")) {
        entry.kind = MessageEntry::Kind::Service;
        if (contentType == QStringLiteral("messageChatChangeTitle")) {
            entry.text = QStringLiteral("đã đổi tên nhóm thành “%1”")
                .arg(Json::str(content, QStringLiteral("title")));
        } else if (contentType == QStringLiteral("messageChatAddMembers")) {
            QStringList names;
            for (qint64 userId : Json::toInt64List(Json::array(content, QStringLiteral("member_user_ids")))) {
                if (const UserEntry *peer = user(userId))
                    names << peer->displayName();
            }
            entry.text = names.isEmpty() ? QStringLiteral("đã thêm thành viên mới")
                                         : QStringLiteral("đã thêm %1").arg(names.join(QStringLiteral(", ")));
        } else if (contentType == QStringLiteral("messageChatDeleteMember")) {
            const qint64 userId = Json::int64(content, QStringLiteral("user_id"));
            const UserEntry *peer = user(userId);
            entry.text = QStringLiteral("đã rời nhóm: %1")
                .arg(peer ? peer->displayName() : QStringLiteral("một thành viên"));
        } else if (contentType == QStringLiteral("messageChatJoinByLink")) {
            entry.text = QStringLiteral("đã tham gia nhóm qua liên kết mời");
        } else if (contentType == QStringLiteral("messagePinMessage")) {
            entry.text = QStringLiteral("đã ghim một tin nhắn");
        } else if (contentType == QStringLiteral("messageChatChangePhoto")) {
            entry.text = QStringLiteral("đã đổi ảnh nhóm");
        } else if (contentType == QStringLiteral("messageChatDeletePhoto")) {
            entry.text = QStringLiteral("đã xoá ảnh nhóm");
        } else if (contentType == QStringLiteral("messageCall")) {
            entry.text = QStringLiteral("Cuộc gọi — %1")
                .arg(Format::duration(Json::integer(content, QStringLiteral("duration"))));
        } else if (contentType == QStringLiteral("messageContactRegistered")) {
            entry.text = QStringLiteral("vừa tham gia Telegram");
        } else if (contentType == QStringLiteral("messageBasicGroupChatCreate")
                   || contentType == QStringLiteral("messageSupergroupChatCreate")) {
            entry.text = QStringLiteral("đã tạo cuộc trò chuyện");
        } else {
            entry.text = QStringLiteral("thông báo hệ thống");
        }
    } else {
        entry.kind = MessageEntry::Kind::Unsupported;
        entry.text = QStringLiteral("Nội dung “%1” chưa được hỗ trợ hiển thị.")
            .arg(contentType);
    }

    fillSenderInfo(entry);

    // Xem trước tin nhắn được trả lời nếu đã có trong bộ nhớ.
    if (entry.replyToMessageId != 0) {
        if (const MessageEntry *replied = cachedMessage(entry.chatId, entry.replyToMessageId)) {
            entry.replyPreviewSender = replied->senderName;
            entry.replyPreviewText = Format::oneLine(
                replied->text.isEmpty() ? replied->kindLabel() : replied->text, 90);
        }
    }

    return entry;
}

void TdAccount::fillSenderInfo(MessageEntry &entry) const
{
    entry.senderName = senderNameFor(entry);
    entry.senderInitials = initialsOf(entry.senderName);
    entry.senderColorIndex = colorIndexFor(entry.senderUserId != 0 ? entry.senderUserId
                                                                   : entry.senderChatId);
}

QString TdAccount::previewForMessage(const MessageEntry &entry) const
{
    QString body;
    switch (entry.kind) {
    case MessageEntry::Kind::Text:
        body = entry.text;
        break;
    case MessageEntry::Kind::Service:
        body = entry.text;
        break;
    case MessageEntry::Kind::Sticker:
        body = QStringLiteral("%1 Nhãn dán").arg(entry.text);
        break;
    case MessageEntry::Kind::Photo:
        body = entry.text.isEmpty() ? QStringLiteral("🖼 Ảnh")
                                    : QStringLiteral("🖼 %1").arg(entry.text);
        break;
    case MessageEntry::Kind::Video:
        body = entry.text.isEmpty() ? QStringLiteral("🎬 Video")
                                    : QStringLiteral("🎬 %1").arg(entry.text);
        break;
    case MessageEntry::Kind::Voice:
        body = QStringLiteral("🎤 Tin nhắn thoại");
        break;
    case MessageEntry::Kind::Audio:
        body = QStringLiteral("🎵 %1").arg(entry.audioTitle.isEmpty() ? entry.fileName
                                                                      : entry.audioTitle);
        break;
    case MessageEntry::Kind::Document:
        body = QStringLiteral("📎 %1").arg(entry.fileName.isEmpty() ? QStringLiteral("Tệp")
                                                                    : entry.fileName);
        break;
    case MessageEntry::Kind::Animation:
        body = QStringLiteral("🎞 Ảnh động");
        break;
    case MessageEntry::Kind::VideoNote:
        body = QStringLiteral("📹 Video tròn");
        break;
    default:
        body = entry.text.isEmpty() ? entry.kindLabel() : entry.text;
        break;
    }
    return Format::oneLine(body, 140);
}

void TdAccount::rememberMessage(const MessageEntry &entry)
{
    if (entry.id == 0)
        return;
    m_messages[entry.chatId].insert(entry.id, entry);

    // Giới hạn bộ nhớ: giữ tối đa 600 tin gần nhất cho mỗi chat chưa mở.
    QHash<qint64, MessageEntry> &bucket = m_messages[entry.chatId];
    if (bucket.size() > 900 && !m_openChats.contains(entry.chatId)) {
        QList<qint64> ids = bucket.keys();
        std::sort(ids.begin(), ids.end());
        const int excess = ids.size() - 600;
        for (int i = 0; i < excess; ++i)
            bucket.remove(ids.at(i));
    }
}

void TdAccount::maybeNotify(const MessageEntry &entry)
{
    if (entry.isOutgoing || entry.kind == MessageEntry::Kind::Service)
        return;
    const ChatEntry *chatEntry = chat(entry.chatId);
    if (chatEntry && chatEntry->isMuted)
        return;

    const QString title = chatEntry ? chatEntry->title : chatTitle(entry.chatId);
    QString body = previewForMessage(entry);
    if (chatEntry && chatEntry->isGroupLike() && !entry.senderName.isEmpty())
        body = QStringLiteral("%1: %2").arg(entry.senderName, body);

    emit notificationRequested(entry.chatId, title, body);
}

// --- Hành động -------------------------------------------------------------

void TdAccount::loadMoreChats(bool archived)
{
    QJsonObject payload = Json::request(QStringLiteral("loadChats"));
    payload.insert(QStringLiteral("chat_list"),
                   Json::request(archived ? QStringLiteral("chatListArchive")
                                          : QStringLiteral("chatListMain")));
    payload.insert(QStringLiteral("limit"), kChatPageSize);
    send(payload);
}

void TdAccount::openChatSession(qint64 chatId)
{
    if (m_openChats.contains(chatId))
        return;
    m_openChats.insert(chatId);
    QJsonObject payload = Json::request(QStringLiteral("openChat"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    send(payload);
    requestChatDetails(chatId);
}

void TdAccount::closeChatSession(qint64 chatId)
{
    if (!m_openChats.remove(chatId))
        return;
    QJsonObject payload = Json::request(QStringLiteral("closeChat"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    send(payload);
}

void TdAccount::requestChatDetails(qint64 chatId)
{
    const ChatEntry *entry = chat(chatId);
    if (!entry || m_pendingChatDetails.contains(chatId))
        return;
    m_pendingChatDetails.insert(chatId);

    if (entry->supergroupId != 0) {
        QJsonObject payload = Json::request(QStringLiteral("getSupergroupFullInfo"));
        payload.insert(QStringLiteral("supergroup_id"), static_cast<double>(entry->supergroupId));
        request(payload, [this, chatId](const QJsonObject &, bool) {
            m_pendingChatDetails.remove(chatId);
        });
    } else if (entry->basicGroupId != 0) {
        QJsonObject payload = Json::request(QStringLiteral("getBasicGroupFullInfo"));
        payload.insert(QStringLiteral("basic_group_id"), static_cast<double>(entry->basicGroupId));
        request(payload, [this, chatId](const QJsonObject &, bool) {
            m_pendingChatDetails.remove(chatId);
        });
    } else if (entry->relatedUserId != 0) {
        QJsonObject payload = Json::request(QStringLiteral("getUserFullInfo"));
        payload.insert(QStringLiteral("user_id"), static_cast<double>(entry->relatedUserId));
        request(payload, [this, chatId](const QJsonObject &, bool) {
            m_pendingChatDetails.remove(chatId);
        });
    } else {
        m_pendingChatDetails.remove(chatId);
    }
}

void TdAccount::loadHistory(qint64 chatId, qint64 fromMessageId, int limit)
{
    QJsonObject payload = Json::request(QStringLiteral("getChatHistory"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("from_message_id"), static_cast<double>(fromMessageId));
    payload.insert(QStringLiteral("offset"), 0);
    payload.insert(QStringLiteral("limit"), qBound(1, limit, 100));
    payload.insert(QStringLiteral("only_local"), false);

    request(payload, [this, chatId, fromMessageId, limit](const QJsonObject &result, bool ok) {
        if (!ok) {
            emit historyReady(chatId, {}, false);
            return;
        }

        const QJsonArray messages = Json::array(result, QStringLiteral("messages"));
        QList<MessageEntry> parsed;
        parsed.reserve(messages.size());
        for (const QJsonValue &value : messages) {
            MessageEntry entry = parseMessage(value.toObject());
            rememberMessage(entry);
            parsed.append(entry);
        }

        // TDLib có thể trả về rỗng ở lần đầu vì chưa có gì trong bộ đệm cục bộ;
        // thử lại một lần nữa để nó tải từ máy chủ.
        if (parsed.isEmpty() && Json::integer(result, QStringLiteral("total_count")) == 0
            && !m_historyRetried.contains(chatId)) {
            m_historyRetried.insert(chatId);
            loadHistory(chatId, fromMessageId, limit);
            return;
        }

        // Tin nhắn được trả lời có thể chưa nạp — bổ sung phần xem trước.
        for (MessageEntry &entry : parsed) {
            if (entry.replyToMessageId != 0 && entry.replyPreviewText.isEmpty()) {
                if (const MessageEntry *replied = cachedMessage(chatId, entry.replyToMessageId)) {
                    entry.replyPreviewSender = replied->senderName;
                    entry.replyPreviewText = Format::oneLine(
                        replied->text.isEmpty() ? replied->kindLabel() : replied->text, 90);
                }
            }
        }

        emit historyReady(chatId, parsed, parsed.isEmpty());
    });
}

void TdAccount::sendText(qint64 chatId, const QString &text, qint64 replyToMessageId,
                         bool disableWebPreview)
{
    if (text.trimmed().isEmpty())
        return;

    // Dùng chính TDLib để phân tích Markdown — đúng chuẩn Telegram.
    QJsonObject parseRequest = Json::request(QStringLiteral("parseTextEntities"));
    parseRequest.insert(QStringLiteral("text"), text);
    QJsonObject parseMode = Json::request(QStringLiteral("textParseModeMarkdown"));
    parseMode.insert(QStringLiteral("version"), 2);
    parseRequest.insert(QStringLiteral("parse_mode"), parseMode);

    QJsonObject formatted = TdTransport::instance().executeSync(parseRequest);
    if (Json::type(formatted) != QStringLiteral("formattedText")) {
        formatted = Json::request(QStringLiteral("formattedText"));
        formatted.insert(QStringLiteral("text"), text);
        formatted.insert(QStringLiteral("entities"), QJsonArray());
    }
    formatted.remove(QStringLiteral("@extra"));

    QJsonObject content = Json::request(QStringLiteral("inputMessageText"));
    content.insert(QStringLiteral("text"), formatted);
    content.insert(QStringLiteral("clear_draft"), true);
    content.insert(QStringLiteral("disable_web_page_preview"), disableWebPreview);
    QJsonObject linkPreview = Json::request(QStringLiteral("linkPreviewOptions"));
    linkPreview.insert(QStringLiteral("is_disabled"), disableWebPreview);
    content.insert(QStringLiteral("link_preview_options"), linkPreview);

    QJsonObject payload = Json::request(QStringLiteral("sendMessage"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("input_message_content"), content);
    if (replyToMessageId != 0) {
        payload.insert(QStringLiteral("reply_to_message_id"), static_cast<double>(replyToMessageId));
        QJsonObject replyTo = Json::request(QStringLiteral("inputMessageReplyToMessage"));
        replyTo.insert(QStringLiteral("message_id"), static_cast<double>(replyToMessageId));
        payload.insert(QStringLiteral("reply_to"), replyTo);
    }

    request(payload, [this, chatId](const QJsonObject &result, bool ok) {
        if (!ok) {
            emit errorOccurred(QStringLiteral("Không gửi được tin nhắn: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        const MessageEntry entry = parseMessage(result);
        rememberMessage(entry);
        emit newMessageArrived(chatId, entry.id);
    });
}

void TdAccount::sendFile(qint64 chatId, const QString &filePath, const QString &caption,
                         qint64 replyToMessageId)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        emit errorOccurred(QStringLiteral("Không tìm thấy tệp: %1").arg(filePath));
        return;
    }

    QJsonObject inputFile = Json::request(QStringLiteral("inputFileLocal"));
    inputFile.insert(QStringLiteral("path"), QDir::toNativeSeparators(info.absoluteFilePath()));

    QJsonObject captionObject = Json::request(QStringLiteral("formattedText"));
    captionObject.insert(QStringLiteral("text"), caption);
    captionObject.insert(QStringLiteral("entities"), QJsonArray());

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    const QString mimeName = mime.name();

    QJsonObject content;
    if (mimeName.startsWith(QStringLiteral("image/"))
        && mimeName != QStringLiteral("image/gif")
        && info.size() < 10 * 1024 * 1024) {
        content = Json::request(QStringLiteral("inputMessagePhoto"));
        content.insert(QStringLiteral("photo"), inputFile);
    } else if (mimeName.startsWith(QStringLiteral("video/"))) {
        content = Json::request(QStringLiteral("inputMessageVideo"));
        content.insert(QStringLiteral("video"), inputFile);
        content.insert(QStringLiteral("supports_streaming"), true);
    } else if (mimeName.startsWith(QStringLiteral("audio/"))) {
        content = Json::request(QStringLiteral("inputMessageAudio"));
        content.insert(QStringLiteral("audio"), inputFile);
        content.insert(QStringLiteral("title"), info.completeBaseName());
    } else {
        content = Json::request(QStringLiteral("inputMessageDocument"));
        content.insert(QStringLiteral("document"), inputFile);
        content.insert(QStringLiteral("disable_content_type_detection"), false);
    }
    content.insert(QStringLiteral("caption"), captionObject);

    QJsonObject payload = Json::request(QStringLiteral("sendMessage"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("input_message_content"), content);
    if (replyToMessageId != 0) {
        payload.insert(QStringLiteral("reply_to_message_id"), static_cast<double>(replyToMessageId));
        QJsonObject replyTo = Json::request(QStringLiteral("inputMessageReplyToMessage"));
        replyTo.insert(QStringLiteral("message_id"), static_cast<double>(replyToMessageId));
        payload.insert(QStringLiteral("reply_to"), replyTo);
    }

    request(payload, [this, chatId](const QJsonObject &result, bool ok) {
        if (!ok) {
            emit errorOccurred(QStringLiteral("Không gửi được tệp: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
            return;
        }
        const MessageEntry entry = parseMessage(result);
        rememberMessage(entry);
        emit newMessageArrived(chatId, entry.id);
    });
}

void TdAccount::editMessageText(qint64 chatId, qint64 messageId, const QString &text)
{
    QJsonObject formatted = Json::request(QStringLiteral("formattedText"));
    formatted.insert(QStringLiteral("text"), text);
    formatted.insert(QStringLiteral("entities"), QJsonArray());

    QJsonObject content = Json::request(QStringLiteral("inputMessageText"));
    content.insert(QStringLiteral("text"), formatted);

    QJsonObject payload = Json::request(QStringLiteral("editMessageText"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("message_id"), static_cast<double>(messageId));
    payload.insert(QStringLiteral("input_message_content"), content);

    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(QStringLiteral("Không sửa được tin nhắn: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
    });
}

void TdAccount::deleteMessages(qint64 chatId, const QList<qint64> &messageIds, bool revoke)
{
    if (messageIds.isEmpty())
        return;
    QJsonObject payload = Json::request(QStringLiteral("deleteMessages"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("message_ids"), Json::fromInt64List(messageIds));
    payload.insert(QStringLiteral("revoke"), revoke);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(QStringLiteral("Không xoá được tin nhắn: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
    });
}

void TdAccount::forwardMessages(qint64 toChatId, qint64 fromChatId,
                                const QList<qint64> &messageIds, bool asCopy)
{
    if (messageIds.isEmpty())
        return;
    QJsonObject payload = Json::request(QStringLiteral("forwardMessages"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(toChatId));
    payload.insert(QStringLiteral("from_chat_id"), static_cast<double>(fromChatId));
    payload.insert(QStringLiteral("message_ids"), Json::fromInt64List(messageIds));
    payload.insert(QStringLiteral("send_copy"), asCopy);
    payload.insert(QStringLiteral("remove_caption"), false);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(QStringLiteral("Không chuyển tiếp được: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
    });
}

void TdAccount::viewMessages(qint64 chatId, const QList<qint64> &messageIds)
{
    if (messageIds.isEmpty())
        return;
    QJsonObject payload = Json::request(QStringLiteral("viewMessages"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("message_ids"), Json::fromInt64List(messageIds));
    payload.insert(QStringLiteral("force_read"), true);
    QJsonObject source = Json::request(QStringLiteral("messageSourceChatHistory"));
    payload.insert(QStringLiteral("source"), source);
    send(payload);
}

void TdAccount::pinMessage(qint64 chatId, qint64 messageId, bool pinned)
{
    QJsonObject payload = Json::request(pinned ? QStringLiteral("pinChatMessage")
                                               : QStringLiteral("unpinChatMessage"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("message_id"), static_cast<double>(messageId));
    if (pinned) {
        payload.insert(QStringLiteral("disable_notification"), false);
        payload.insert(QStringLiteral("only_for_self"), false);
    }
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::sendChatAction(qint64 chatId, bool typing)
{
    QJsonObject payload = Json::request(QStringLiteral("sendChatAction"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("action"),
                   Json::request(typing ? QStringLiteral("chatActionTyping")
                                        : QStringLiteral("chatActionCancel")));
    send(payload);

    if (typing) {
        m_typingChatId = chatId;
        if (!m_typingTimer->isActive())
            m_typingTimer->start();
    } else {
        m_typingChatId = 0;
        m_typingTimer->stop();
    }
}

void TdAccount::saveDraft(qint64 chatId, const QString &text, qint64 replyToMessageId)
{
    QJsonObject payload = Json::request(QStringLiteral("setChatDraftMessage"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("message_thread_id"), 0);

    if (text.trimmed().isEmpty()) {
        send(payload); // draft_message rỗng = xoá nháp
        return;
    }

    QJsonObject formatted = Json::request(QStringLiteral("formattedText"));
    formatted.insert(QStringLiteral("text"), text);
    formatted.insert(QStringLiteral("entities"), QJsonArray());

    QJsonObject content = Json::request(QStringLiteral("inputMessageText"));
    content.insert(QStringLiteral("text"), formatted);

    QJsonObject draft = Json::request(QStringLiteral("draftMessage"));
    draft.insert(QStringLiteral("reply_to_message_id"), static_cast<double>(replyToMessageId));
    draft.insert(QStringLiteral("date"), static_cast<double>(QDateTime::currentSecsSinceEpoch()));
    draft.insert(QStringLiteral("input_message_text"), content);

    payload.insert(QStringLiteral("draft_message"), draft);
    send(payload);
}

void TdAccount::setChatMuted(qint64 chatId, bool muted)
{
    QJsonObject settings = Json::request(QStringLiteral("chatNotificationSettings"));
    settings.insert(QStringLiteral("use_default_mute_for"), false);
    settings.insert(QStringLiteral("mute_for"), muted ? 365 * 24 * 3600 : 0);

    QJsonObject payload = Json::request(QStringLiteral("setChatNotificationSettings"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("notification_settings"), settings);
    send(payload);
}

void TdAccount::setChatPinned(qint64 chatId, bool pinned)
{
    const ChatEntry *entry = chat(chatId);
    QJsonObject payload = Json::request(QStringLiteral("toggleChatIsPinned"));
    payload.insert(QStringLiteral("chat_list"),
                   Json::request(entry && entry->inArchive ? QStringLiteral("chatListArchive")
                                                           : QStringLiteral("chatListMain")));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("is_pinned"), pinned);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::setChatArchived(qint64 chatId, bool archived)
{
    QJsonObject payload = Json::request(QStringLiteral("addChatToList"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("chat_list"),
                   Json::request(archived ? QStringLiteral("chatListArchive")
                                          : QStringLiteral("chatListMain")));
    request(payload, [this, chatId, archived](const QJsonObject &result, bool ok) {
        if (!ok) {
            emit errorOccurred(Json::str(result, QStringLiteral("message")));
            return;
        }
        auto it = m_chats.find(chatId);
        if (it != m_chats.end()) {
            it->inArchive = archived;
            emit chatUpserted(chatId);
            emit chatOrderChanged();
        }
    });
}

void TdAccount::setChatMarkedUnread(qint64 chatId, bool unread)
{
    QJsonObject payload = Json::request(QStringLiteral("toggleChatIsMarkedAsUnread"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("is_marked_as_unread"), unread);
    send(payload);
}

void TdAccount::readAllChat(qint64 chatId)
{
    QJsonObject payload = Json::request(QStringLiteral("readAllChatMentions"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    send(payload);

    const ChatEntry *entry = chat(chatId);
    if (entry && entry->lastMessageId != 0)
        viewMessages(chatId, { entry->lastMessageId });
}

void TdAccount::leaveChat(qint64 chatId)
{
    QJsonObject payload = Json::request(QStringLiteral("leaveChat"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(Json::str(result, QStringLiteral("message")));
    });
}

void TdAccount::deleteChatHistory(qint64 chatId, bool removeFromList, bool revoke)
{
    QJsonObject payload = Json::request(QStringLiteral("deleteChatHistory"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("remove_from_chat_list"), removeFromList);
    payload.insert(QStringLiteral("revoke"), revoke);
    request(payload, [this, chatId, removeFromList](const QJsonObject &result, bool ok) {
        if (!ok) {
            emit errorOccurred(Json::str(result, QStringLiteral("message")));
            return;
        }
        m_messages.remove(chatId);
        if (removeFromList) {
            m_chats.remove(chatId);
            emit chatRemoved(chatId);
            emit chatOrderChanged();
        }
    });
}

void TdAccount::setUserBlocked(qint64 userId, bool blocked)
{
    QJsonObject sender = Json::request(QStringLiteral("messageSenderUser"));
    sender.insert(QStringLiteral("user_id"), static_cast<double>(userId));

    // API mới: setMessageSenderBlockList; API cũ: toggleMessageSenderIsBlocked.
    QJsonObject payload = Json::request(QStringLiteral("setMessageSenderBlockList"));
    payload.insert(QStringLiteral("sender_id"), sender);
    if (blocked)
        payload.insert(QStringLiteral("block_list"), Json::request(QStringLiteral("blockListMain")));

    request(payload, [this, sender, blocked](const QJsonObject &, bool ok) {
        if (ok)
            return;
        QJsonObject legacy = Json::request(QStringLiteral("toggleMessageSenderIsBlocked"));
        legacy.insert(QStringLiteral("sender_id"), sender);
        legacy.insert(QStringLiteral("is_blocked"), blocked);
        request(legacy, [this](const QJsonObject &result, bool innerOk) {
            if (!innerOk)
                emit errorOccurred(Json::str(result, QStringLiteral("message")));
        });
    });
}

// --- Tệp -------------------------------------------------------------------

void TdAccount::downloadFile(int fileId, int priority)
{
    if (fileId == 0)
        return;
    QJsonObject payload = Json::request(QStringLiteral("downloadFile"));
    payload.insert(QStringLiteral("file_id"), fileId);
    payload.insert(QStringLiteral("priority"), qBound(1, priority, 32));
    payload.insert(QStringLiteral("offset"), 0);
    payload.insert(QStringLiteral("limit"), 0);
    payload.insert(QStringLiteral("synchronous"), false);
    send(payload);
}

void TdAccount::cancelDownload(int fileId)
{
    QJsonObject payload = Json::request(QStringLiteral("cancelDownloadFile"));
    payload.insert(QStringLiteral("file_id"), fileId);
    payload.insert(QStringLiteral("only_if_pending"), false);
    send(payload);
}

QString TdAccount::localPathForFile(int fileId) const
{
    auto it = m_files.constFind(fileId);
    if (it == m_files.constEnd())
        return QString();
    return localPathOf(it.value());
}

// --- Tìm kiếm / tạo mới ----------------------------------------------------

void TdAccount::searchChatsLocal(const QString &query, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("searchChats"));
    payload.insert(QStringLiteral("query"), query);
    payload.insert(QStringLiteral("limit"), 40);
    request(payload, handler);
}

void TdAccount::searchPublicChats(const QString &query, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("searchPublicChats"));
    payload.insert(QStringLiteral("query"), query);
    request(payload, handler);
}

void TdAccount::searchMessagesInChat(qint64 chatId, const QString &query, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("searchChatMessages"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("query"), query);
    payload.insert(QStringLiteral("from_message_id"), 0);
    payload.insert(QStringLiteral("offset"), 0);
    payload.insert(QStringLiteral("limit"), 50);
    request(payload, handler);
}

void TdAccount::searchMessagesGlobal(const QString &query, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("searchMessages"));
    payload.insert(QStringLiteral("query"), query);
    payload.insert(QStringLiteral("offset"), QString());
    payload.insert(QStringLiteral("limit"), 50);
    request(payload, handler);
}

void TdAccount::fetchContacts(const ResultHandler &handler)
{
    request(Json::request(QStringLiteral("getContacts")), handler);
}

void TdAccount::createPrivateChat(qint64 userId, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("createPrivateChat"));
    payload.insert(QStringLiteral("user_id"), static_cast<double>(userId));
    payload.insert(QStringLiteral("force"), false);
    request(payload, handler);
}

void TdAccount::createGroup(const QString &title, const QList<qint64> &userIds,
                            const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("createNewBasicGroupChat"));
    payload.insert(QStringLiteral("user_ids"), Json::fromInt64List(userIds));
    payload.insert(QStringLiteral("title"), title);
    request(payload, handler);
}

void TdAccount::createChannel(const QString &title, const QString &description, bool megagroup,
                              const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("createNewSupergroupChat"));
    payload.insert(QStringLiteral("title"), title);
    payload.insert(QStringLiteral("is_channel"), !megagroup);
    payload.insert(QStringLiteral("is_forum"), false);
    payload.insert(QStringLiteral("description"), description);
    request(payload, handler);
}

void TdAccount::joinByInviteLink(const QString &link, const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("joinChatByInviteLink"));
    payload.insert(QStringLiteral("invite_link"), link.trimmed());
    request(payload, handler);
}

void TdAccount::searchByUsername(const QString &username, const ResultHandler &handler)
{
    QString cleaned = username.trimmed();
    if (cleaned.startsWith(QLatin1Char('@')))
        cleaned.remove(0, 1);
    QJsonObject payload = Json::request(QStringLiteral("searchPublicChat"));
    payload.insert(QStringLiteral("username"), cleaned);
    request(payload, handler);
}

void TdAccount::fetchChatMembers(qint64 chatId, const ResultHandler &handler)
{
    const ChatEntry *entry = chat(chatId);
    if (!entry) {
        if (handler)
            handler(QJsonObject(), false);
        return;
    }

    if (entry->supergroupId != 0) {
        QJsonObject payload = Json::request(QStringLiteral("getSupergroupMembers"));
        payload.insert(QStringLiteral("supergroup_id"), static_cast<double>(entry->supergroupId));
        payload.insert(QStringLiteral("filter"),
                       Json::request(QStringLiteral("supergroupMembersFilterRecent")));
        payload.insert(QStringLiteral("offset"), 0);
        payload.insert(QStringLiteral("limit"), 200);
        request(payload, handler);
    } else if (entry->basicGroupId != 0) {
        QJsonObject payload = Json::request(QStringLiteral("getBasicGroupFullInfo"));
        payload.insert(QStringLiteral("basic_group_id"), static_cast<double>(entry->basicGroupId));
        request(payload, handler);
    } else if (handler) {
        handler(QJsonObject(), false);
    }
}

void TdAccount::addChatMembers(qint64 chatId, const QList<qint64> &userIds,
                               const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("addChatMembers"));
    payload.insert(QStringLiteral("chat_id"), static_cast<double>(chatId));
    payload.insert(QStringLiteral("user_ids"), Json::fromInt64List(userIds));
    request(payload, handler);
}

void TdAccount::addContact(const QString &phone, const QString &firstName,
                           const QString &lastName, const ResultHandler &handler)
{
    QJsonObject contact = Json::request(QStringLiteral("contact"));
    contact.insert(QStringLiteral("phone_number"), Format::normalizePhone(phone));
    contact.insert(QStringLiteral("first_name"), firstName);
    contact.insert(QStringLiteral("last_name"), lastName);
    contact.insert(QStringLiteral("user_id"), 0);

    QJsonObject payload = Json::request(QStringLiteral("importContacts"));
    QJsonArray contacts;
    contacts.append(contact);
    payload.insert(QStringLiteral("contacts"), contacts);
    request(payload, handler);
}

// --- Bảo trì ---------------------------------------------------------------

void TdAccount::applyProxySettings()
{
    const SettingsStore &settings = SettingsStore::instance();
    const SettingsStore::ProxyKind kind = settings.proxyKind();

    if (kind == SettingsStore::ProxyKind::None) {
        send(Json::request(QStringLiteral("disableProxy")));
        return;
    }
    if (settings.proxyServer().isEmpty())
        return;

    QJsonObject type;
    switch (kind) {
    case SettingsStore::ProxyKind::Socks5:
        type = Json::request(QStringLiteral("proxyTypeSocks5"));
        type.insert(QStringLiteral("username"), settings.proxyUsername());
        type.insert(QStringLiteral("password"), settings.proxyPassword());
        break;
    case SettingsStore::ProxyKind::Http:
        type = Json::request(QStringLiteral("proxyTypeHttp"));
        type.insert(QStringLiteral("username"), settings.proxyUsername());
        type.insert(QStringLiteral("password"), settings.proxyPassword());
        type.insert(QStringLiteral("http_only"), false);
        break;
    case SettingsStore::ProxyKind::MtProto:
        type = Json::request(QStringLiteral("proxyTypeMtproto"));
        type.insert(QStringLiteral("secret"), settings.proxySecret());
        break;
    case SettingsStore::ProxyKind::None:
        return;
    }

    QJsonObject payload = Json::request(QStringLiteral("addProxy"));
    payload.insert(QStringLiteral("server"), settings.proxyServer());
    payload.insert(QStringLiteral("port"), settings.proxyPort());
    payload.insert(QStringLiteral("enable"), true);
    payload.insert(QStringLiteral("type"), type);
    request(payload, [this](const QJsonObject &result, bool ok) {
        if (!ok)
            emit errorOccurred(QStringLiteral("Proxy không dùng được: %1")
                                   .arg(Json::str(result, QStringLiteral("message"))));
    });
}

void TdAccount::setOnline(bool online)
{
    QJsonObject value = Json::request(QStringLiteral("optionValueBoolean"));
    value.insert(QStringLiteral("value"), online);

    QJsonObject payload = Json::request(QStringLiteral("setOption"));
    payload.insert(QStringLiteral("name"), QStringLiteral("online"));
    payload.insert(QStringLiteral("value"), value);
    send(payload);
}

void TdAccount::fetchStorageStatistics(const ResultHandler &handler)
{
    request(Json::request(QStringLiteral("getStorageStatisticsFast")), handler);
}

void TdAccount::optimizeStorage(const ResultHandler &handler)
{
    QJsonObject payload = Json::request(QStringLiteral("optimizeStorage"));
    payload.insert(QStringLiteral("size"), 0);
    payload.insert(QStringLiteral("ttl"), 0);
    payload.insert(QStringLiteral("count"), 0);
    payload.insert(QStringLiteral("immunity_delay"), 0);
    payload.insert(QStringLiteral("chat_limit"), 0);
    payload.insert(QStringLiteral("return_deleted_file_statistics"), false);
    request(payload, handler);
}

// --- Nội bộ ----------------------------------------------------------------

void TdAccount::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void TdAccount::setError(const QString &message)
{
    if (message.isEmpty())
        return;
    m_lastError = message;
    qCWarning(logTd) << m_slug << "lỗi:" << message;
    emit errorOccurred(message);
}
