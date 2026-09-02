#include "core/logging.h"

#include "core/apppaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>

Q_LOGGING_CATEGORY(logApp, "tuan.app")
Q_LOGGING_CATEGORY(logTd, "tuan.td")
Q_LOGGING_CATEGORY(logUi, "tuan.ui")

namespace {

constexpr qint64 kMaxLogBytes = 4 * 1024 * 1024; // 4 MB rồi xoay vòng

QFile *g_logFile = nullptr;
QMutex g_logMutex;
QtMessageHandler g_previousHandler = nullptr;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    }
    return "?????";
}

void rotateIfNeeded()
{
    const QString path = Logging::logFilePath();
    if (QFileInfo(path).size() < kMaxLogBytes)
        return;

    const QString backup = path + QStringLiteral(".1");
    QFile::remove(backup);
    QFile::rename(path, backup);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString line = QStringLiteral("%1 [%2] %3: %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             QString::fromLatin1(levelName(type)),
             QString::fromLatin1(context.category ? context.category : "default"),
             message);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile && g_logFile->isOpen()) {
            QTextStream stream(g_logFile);
            stream << line << Qt::endl;
        }
    }

    // Vẫn in ra console để tiện chạy từ terminal.
    std::fputs(qPrintable(line), type == QtDebugMsg || type == QtInfoMsg ? stdout : stderr);
    std::fputc('\n', type == QtDebugMsg || type == QtInfoMsg ? stdout : stderr);

    if (type == QtFatalMsg) {
        Logging::shutdown();
        std::abort();
    }
}

} // namespace

void Logging::install()
{
    AppPaths::ensureDir(AppPaths::logDir());
    rotateIfNeeded();

    QMutexLocker locker(&g_logMutex);
    if (g_logFile)
        return;

    g_logFile = new QFile(logFilePath());
    if (!g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
        return;
    }

    g_previousHandler = qInstallMessageHandler(messageHandler);
}

void Logging::shutdown()
{
    QtMessageHandler previous = g_previousHandler;
    g_previousHandler = nullptr;
    if (previous)
        qInstallMessageHandler(previous);
    else
        qInstallMessageHandler(nullptr);

    QMutexLocker locker(&g_logMutex);
    if (g_logFile) {
        g_logFile->flush();
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
}

QString Logging::logFilePath()
{
    return QDir(AppPaths::logDir()).filePath(QStringLiteral("tuan-multitele.log"));
}

QString Logging::tail(int maxLines)
{
    QFile file(logFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    // Chỉ đọc phần cuối tệp để không ngốn RAM.
    constexpr qint64 kWindow = 512 * 1024;
    const qint64 size = file.size();
    if (size > kWindow)
        file.seek(size - kWindow);

    const QString text = QString::fromUtf8(file.readAll());
    QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.size() > maxLines)
        lines = lines.mid(lines.size() - maxLines);
    return lines.join(QLatin1Char('\n'));
}
