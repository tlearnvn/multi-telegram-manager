#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

/*!
 * \brief Tiện ích đọc JSON của TDLib.
 *
 * TDLib biểu diễn số 64 bit (như "order" của chatPosition) bằng chuỗi, còn
 * int53 (chat_id, message_id, user_id) bằng số. Các hàm dưới đây chấp nhận cả
 * hai dạng nên phần còn lại của mã không phải quan tâm.
 */
namespace Json {

QString type(const QJsonObject &object);

QString str(const QJsonObject &object, const QString &key, const QString &fallback = QString());
bool boolean(const QJsonObject &object, const QString &key, bool fallback = false);
int integer(const QJsonObject &object, const QString &key, int fallback = 0);
qint64 int64(const QJsonObject &object, const QString &key, qint64 fallback = 0);
double real(const QJsonObject &object, const QString &key, double fallback = 0.0);

QJsonObject object(const QJsonObject &parent, const QString &key);
QJsonArray array(const QJsonObject &parent, const QString &key);

//! Đi sâu nhiều tầng: path(o, {"content","text","text"}).
QJsonValue path(const QJsonObject &root, const QStringList &keys);
QString strAt(const QJsonObject &root, const QStringList &keys, const QString &fallback = QString());

//! Tạo yêu cầu TDLib với "@type" cho sẵn.
QJsonObject request(const QString &type);

//! Mảng số nguyên -> QJsonArray.
QJsonArray fromInt64List(const QList<qint64> &values);
QList<qint64> toInt64List(const QJsonArray &array);

QByteArray compact(const QJsonObject &object);
QString pretty(const QJsonObject &object);

} // namespace Json
