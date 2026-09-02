#include "core/jsonutil.h"

#include <QJsonDocument>
#include <QStringList>

namespace Json {

QString type(const QJsonObject &object)
{
    return object.value(QStringLiteral("@type")).toString();
}

QString str(const QJsonObject &object, const QString &key, const QString &fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble(), 'f', 0);
    return fallback;
}

bool boolean(const QJsonObject &object, const QString &key, bool fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isBool())
        return value.toBool();
    if (value.isDouble())
        return value.toDouble() != 0.0;
    return fallback;
}

int integer(const QJsonObject &object, const QString &key, int fallback)
{
    return static_cast<int>(int64(object, key, fallback));
}

qint64 int64(const QJsonObject &object, const QString &key, qint64 fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

double real(const QJsonObject &object, const QString &key, double fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

QJsonObject object(const QJsonObject &parent, const QString &key)
{
    return parent.value(key).toObject();
}

QJsonArray array(const QJsonObject &parent, const QString &key)
{
    return parent.value(key).toArray();
}

QJsonValue path(const QJsonObject &root, const QStringList &keys)
{
    QJsonValue current(root);
    for (const QString &key : keys) {
        if (!current.isObject())
            return QJsonValue();
        current = current.toObject().value(key);
    }
    return current;
}

QString strAt(const QJsonObject &root, const QStringList &keys, const QString &fallback)
{
    const QJsonValue value = path(root, keys);
    return value.isString() ? value.toString() : fallback;
}

QJsonObject request(const QString &type)
{
    QJsonObject object;
    object.insert(QStringLiteral("@type"), type);
    return object;
}

QJsonArray fromInt64List(const QList<qint64> &values)
{
    QJsonArray array;
    for (qint64 value : values)
        array.append(static_cast<double>(value));
    return array;
}

QList<qint64> toInt64List(const QJsonArray &array)
{
    QList<qint64> result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isString()) {
            bool ok = false;
            const qint64 parsed = value.toString().toLongLong(&ok);
            if (ok)
                result.append(parsed);
        } else {
            result.append(static_cast<qint64>(value.toDouble()));
        }
    }
    return result;
}

QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString pretty(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace Json
