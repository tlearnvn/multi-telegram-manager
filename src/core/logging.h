#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logTd)
Q_DECLARE_LOGGING_CATEGORY(logUi)

/*!
 * \brief Ghi log ra data/logs/tuan-multitele.log, tự cắt bớt khi quá lớn.
 */
class Logging
{
public:
    //! Gắn message handler của Qt vào tệp log. Gọi sau AppPaths::initialize().
    static void install();

    //! Đóng tệp log (gọi khi thoát).
    static void shutdown();

    static QString logFilePath();

    //! Nội dung log gần đây, dùng để hiển thị trong hộp thoại chẩn đoán.
    static QString tail(int maxLines = 400);
};
