#include "core/formatting.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>

namespace {

const char *kWeekdays[] = {
    "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy", "Chủ Nhật"
};

QString weekdayName(const QDate &date)
{
    const int index = date.dayOfWeek(); // 1 = Thứ Hai ... 7 = Chủ Nhật
    if (index >= 1 && index <= 7)
        return QString::fromUtf8(kWeekdays[index - 1]);
    return QString();
}

} // namespace

namespace Format {

QString clock(qint64 unixSeconds)
{
    if (unixSeconds <= 0)
        return QString();
    return QDateTime::fromSecsSinceEpoch(unixSeconds).toString(QStringLiteral("HH:mm"));
}

QString chatListTime(qint64 unixSeconds)
{
    if (unixSeconds <= 0)
        return QString();

    const QDateTime moment = QDateTime::fromSecsSinceEpoch(unixSeconds);
    const QDate today = QDate::currentDate();
    const QDate day = moment.date();

    if (day == today)
        return moment.toString(QStringLiteral("HH:mm"));
    if (day == today.addDays(-1))
        return QCoreApplication::translate("Format", "Hôm qua");
    if (day > today.addDays(-7))
        return weekdayName(day);
    if (day.year() == today.year())
        return moment.toString(QStringLiteral("dd/MM"));
    return moment.toString(QStringLiteral("dd/MM/yyyy"));
}

QString dayDivider(qint64 unixSeconds)
{
    if (unixSeconds <= 0)
        return QString();

    const QDateTime moment = QDateTime::fromSecsSinceEpoch(unixSeconds);
    const QDate today = QDate::currentDate();
    const QDate day = moment.date();

    if (day == today)
        return QCoreApplication::translate("Format", "Hôm nay");
    if (day == today.addDays(-1))
        return QCoreApplication::translate("Format", "Hôm qua");

    const QString base = QCoreApplication::translate("Format", "%1, %2 tháng %3")
        .arg(weekdayName(day))
        .arg(day.day())
        .arg(day.month());

    if (day.year() == today.year())
        return base;
    return QCoreApplication::translate("Format", "%1, %2").arg(base).arg(day.year());
}

QString fullDateTime(qint64 unixSeconds)
{
    if (unixSeconds <= 0)
        return QString();
    return QDateTime::fromSecsSinceEpoch(unixSeconds).toString(QStringLiteral("dd/MM/yyyy HH:mm"));
}

QString relative(qint64 unixSeconds)
{
    if (unixSeconds <= 0)
        return QCoreApplication::translate("Format", "chưa rõ");

    const QDateTime moment = QDateTime::fromSecsSinceEpoch(unixSeconds);
    const qint64 diff = moment.secsTo(QDateTime::currentDateTime());

    if (diff < 0)
        return clock(unixSeconds);
    if (diff < 60)
        return QCoreApplication::translate("Format", "vừa xong");
    if (diff < 3600) {
        return QCoreApplication::translate("Format", "%1 phút trước").arg(diff / 60);
    }
    if (diff < 86400)
        return QCoreApplication::translate("Format", "%1 giờ trước").arg(diff / 3600);
    if (diff < 172800)
        return QCoreApplication::translate("Format", "hôm qua");
    if (diff < 604800)
        return QCoreApplication::translate("Format", "%1 ngày trước").arg(diff / 86400);
    return moment.toString(QStringLiteral("dd/MM/yyyy"));
}

QString fileSize(qint64 bytes)
{
    if (bytes < 0)
        bytes = 0;
    if (bytes < 1024)
        return QCoreApplication::translate("Format", "%1 B").arg(bytes);

    static const char *units[] = { "KB", "MB", "GB", "TB" };
    double value = static_cast<double>(bytes) / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }

    QLocale vi(QLocale::Vietnamese, QLocale::Vietnam);
    const QString number = vi.toString(value, 'f', value < 10.0 ? 1 : 0);
    return QStringLiteral("%1 %2").arg(number, QString::fromLatin1(units[unit]));
}

QString duration(int seconds)
{
    if (seconds < 0)
        seconds = 0;
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString oneLine(const QString &text, int maxChars)
{
    QString flat = text;
    flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\r'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\t'), QLatin1Char(' '));
    static const QRegularExpression spaces(QStringLiteral("\\s{2,}"));
    flat.replace(spaces, QStringLiteral(" "));
    flat = flat.trimmed();
    if (maxChars > 0 && flat.size() > maxChars)
        flat = flat.left(maxChars - 1) + QChar(0x2026); // …
    return flat;
}

QString maskPhone(const QString &phone)
{
    const QString digits = normalizePhone(phone);
    if (digits.size() < 7)
        return digits;
    const int keepFront = digits.startsWith(QLatin1Char('+')) ? 4 : 3;
    const int keepBack = 2;
    QString masked = digits.left(keepFront);
    masked += QString(digits.size() - keepFront - keepBack, QLatin1Char('*'));
    masked += digits.right(keepBack);
    return masked;
}

QString normalizePhone(const QString &phone)
{
    QString result;
    for (const QChar ch : phone) {
        if (ch.isDigit())
            result.append(ch);
        else if (ch == QLatin1Char('+') && result.isEmpty())
            result.append(ch);
    }
    return result;
}

QString countLabel(int count, const QString &noun)
{
    // Tiếng Việt không biến đổi danh từ theo số, nên chỉ cần ghép.
    QLocale vi(QLocale::Vietnamese, QLocale::Vietnam);
    return QStringLiteral("%1 %2").arg(vi.toString(count), noun);
}

} // namespace Format
