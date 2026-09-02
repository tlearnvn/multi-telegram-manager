#include "td/tdtransport.h"

#include "core/jsonutil.h"
#include "core/logging.h"
#include "td/tdloader.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace {
constexpr double kReceiveTimeoutSeconds = 1.0;
}

TdTransport &TdTransport::instance()
{
    static TdTransport transport;
    return transport;
}

TdTransport::TdTransport(QObject *parent)
    : QObject(parent)
{
}

TdTransport::~TdTransport()
{
    stop();
}

bool TdTransport::start()
{
    if (m_worker && m_worker->isRunning())
        return true;

    if (!TdLoader::instance().load())
        return false;

    if (!m_worker)
        m_worker = new Worker(this);
    m_worker->start();
    qCInfo(logTd) << "Luồng nhận TDLib đã khởi động";
    return true;
}

void TdTransport::stop()
{
    if (!m_worker)
        return;

    m_worker->requestStop();
    // Luồng thức dậy sau tối đa kReceiveTimeoutSeconds giây.
    if (!m_worker->wait(5000))
        m_worker->terminate();
    delete m_worker;
    m_worker = nullptr;
    qCInfo(logTd) << "Luồng nhận TDLib đã dừng";
}

bool TdTransport::isRunning() const
{
    return m_worker && m_worker->isRunning();
}

void TdTransport::send(int clientId, const QJsonObject &request)
{
    TdLoader::instance().send(clientId, Json::compact(request));
}

QJsonObject TdTransport::executeSync(const QJsonObject &request)
{
    const QByteArray raw = TdLoader::instance().execute(Json::compact(request));
    if (raw.isEmpty())
        return QJsonObject();
    return QJsonDocument::fromJson(raw).object();
}

// --- Worker ----------------------------------------------------------------

TdTransport::Worker::Worker(TdTransport *owner)
    : QThread(nullptr)
    , m_owner(owner)
    , m_stop(0)
{
    setObjectName(QStringLiteral("TdReceive"));
}

void TdTransport::Worker::requestStop()
{
    m_stop.storeRelease(1);
}

void TdTransport::Worker::run()
{
    TdLoader &loader = TdLoader::instance();

    while (m_stop.loadAcquire() == 0) {
        const QByteArray raw = loader.receive(kReceiveTimeoutSeconds);
        if (raw.isEmpty())
            continue;

        QJsonParseError error {};
        const QJsonDocument document = QJsonDocument::fromJson(raw, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qCWarning(logTd) << "JSON không hợp lệ từ TDLib:" << error.errorString();
            continue;
        }

        const QJsonObject object = document.object();
        int clientId = -1;
        const QJsonValue clientValue = object.value(QStringLiteral("@client_id"));
        if (clientValue.isDouble())
            clientId = static_cast<int>(clientValue.toDouble());

        emit m_owner->received(clientId, object);
    }
}
