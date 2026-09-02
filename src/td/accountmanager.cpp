#include "td/accountmanager.h"

#include "core/apppaths.h"
#include "core/jsonutil.h"
#include "core/logging.h"
#include "td/tdaccount.h"
#include "td/tdtransport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
{
    connect(&TdTransport::instance(), &TdTransport::received,
            this, &AccountManager::onTransportMessage);
}

AccountManager::~AccountManager()
{
    for (TdAccount *account : m_ordered)
        account->close();
}

void AccountManager::loadFromDisk()
{
    QFile file(AppPaths::accountsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qCInfo(logApp) << "Chưa có accounts.json — bắt đầu với danh sách rỗng";
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray entries = document.isArray()
        ? document.array()
        : Json::array(document.object(), QStringLiteral("accounts"));

    for (const QJsonValue &value : entries) {
        const QJsonObject item = value.toObject();
        const QString slug = Json::str(item, QStringLiteral("slug"));
        if (slug.isEmpty() || m_bySlug.contains(slug))
            continue;

        auto *account = new TdAccount(slug, this);
        account->setLabel(Json::str(item, QStringLiteral("label")));
        account->setSavedPhone(Json::str(item, QStringLiteral("phone")));
        const QString color = Json::str(item, QStringLiteral("color"));
        if (!color.isEmpty())
            account->setAccentColor(color);

        wireAccount(account);
        m_ordered.append(account);
        m_bySlug.insert(slug, account);
    }

    qCInfo(logApp) << "Đã nạp" << m_ordered.size() << "tài khoản từ đĩa";

    for (TdAccount *account : m_ordered)
        account->open();

    if (!m_ordered.isEmpty())
        setActiveAccount(m_ordered.first());

    emit accountsChanged();
}

void AccountManager::saveToDisk() const
{
    QJsonArray entries;
    for (const TdAccount *account : m_ordered) {
        QJsonObject item;
        item.insert(QStringLiteral("slug"), account->slug());
        item.insert(QStringLiteral("label"), account->label());
        item.insert(QStringLiteral("phone"), account->savedPhone());
        item.insert(QStringLiteral("color"), account->accentColor());
        entries.append(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("accounts"), entries);

    QFile file(AppPaths::accountsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(logApp) << "Không ghi được accounts.json:" << file.errorString();
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

TdAccount *AccountManager::accountBySlug(const QString &slug) const
{
    return m_bySlug.value(slug, nullptr);
}

TdAccount *AccountManager::accountAt(int index) const
{
    if (index < 0 || index >= m_ordered.size())
        return nullptr;
    return m_ordered.at(index);
}

int AccountManager::indexOf(TdAccount *account) const
{
    return m_ordered.indexOf(account);
}

TdAccount *AccountManager::createAccount(const QString &label)
{
    const QString slug = allocateSlug();
    auto *account = new TdAccount(slug, this);
    account->setLabel(label);

    // Mỗi tài khoản một màu nhấn khác nhau để dễ phân biệt trên thanh bên.
    static const char *palette[] = {
        "#2ea6ff", "#4dd0a0", "#f5a623", "#e0567c", "#a06bf0", "#38c9d6", "#f2775c"
    };
    account->setAccentColor(QString::fromLatin1(palette[m_ordered.size() % 7]));

    wireAccount(account);
    m_ordered.append(account);
    m_bySlug.insert(slug, account);
    saveToDisk();

    account->open();

    emit accountAdded(account);
    emit accountsChanged();
    setActiveAccount(account);
    return account;
}

void AccountManager::removeAccount(const QString &slug, bool alsoLogOut)
{
    TdAccount *account = m_bySlug.value(slug, nullptr);
    if (!account)
        return;

    if (alsoLogOut && account->state() == TdAccount::State::Ready)
        account->logOut();
    else
        account->close();

    m_ordered.removeAll(account);
    m_bySlug.remove(slug);
    if (m_active == account)
        m_active = m_ordered.isEmpty() ? nullptr : m_ordered.first();

    account->deleteLater();
    saveToDisk();

    // Xoá thư mục dữ liệu cục bộ của tài khoản.
    QDir dir(AppPaths::accountDir(slug));
    if (dir.exists() && !dir.removeRecursively())
        qCWarning(logApp) << "Không xoá được thư mục" << dir.absolutePath();

    emit accountRemoved(slug);
    emit accountsChanged();
    emit activeAccountChanged(m_active);
    emit aggregateUnreadChanged(totalUnread());
}

void AccountManager::setActiveAccount(TdAccount *account)
{
    if (m_active == account)
        return;
    m_active = account;
    emit activeAccountChanged(account);
}

void AccountManager::setActiveIndex(int index)
{
    if (TdAccount *account = accountAt(index))
        setActiveAccount(account);
}

int AccountManager::totalUnread() const
{
    int total = 0;
    for (const TdAccount *account : m_ordered)
        total += account->totalUnreadChats();
    return total;
}

void AccountManager::reopenAll()
{
    for (TdAccount *account : m_ordered)
        account->open();
}

void AccountManager::applyProxyToAll()
{
    for (TdAccount *account : m_ordered) {
        if (account->isReady())
            account->applyProxySettings();
    }
}

void AccountManager::setOnlineAll(bool online)
{
    for (TdAccount *account : m_ordered) {
        if (account->isReady())
            account->setOnline(online);
    }
}

void AccountManager::onTransportMessage(int clientId, const QJsonObject &object)
{
    if (clientId < 0)
        return;
    for (TdAccount *account : m_ordered) {
        if (account->clientId() == clientId) {
            account->handleIncoming(object);
            return;
        }
    }
}

void AccountManager::wireAccount(TdAccount *account)
{
    connect(account, &TdAccount::unreadCountsChanged, this, [this] {
        emit aggregateUnreadChanged(totalUnread());
    });
    connect(account, &TdAccount::profileChanged, this, [this] {
        saveToDisk();
        emit accountsChanged();
    });
    connect(account, &TdAccount::stateChanged, this, [this](TdAccount::State) {
        emit accountsChanged();
    });
}

QString AccountManager::allocateSlug() const
{
    for (int index = 1; index < 1000; ++index) {
        const QString candidate = QStringLiteral("tk%1").arg(index, 2, 10, QLatin1Char('0'));
        if (!m_bySlug.contains(candidate)
            && !QDir(AppPaths::accountDir(candidate)).exists()) {
            return candidate;
        }
    }
    return QStringLiteral("tk-%1").arg(QDateTime::currentSecsSinceEpoch());
}
