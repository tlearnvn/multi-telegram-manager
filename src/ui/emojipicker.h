#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QStackedWidget;
class QTabBar;

/*!
 * \brief Bảng chọn biểu tượng cảm xúc.
 *
 * Cửa sổ bật lên gọn nhẹ, chia theo nhóm, có ô tìm kiếm theo tên tiếng Việt và
 * ghi nhớ những emoji dùng gần đây (lưu trong cấu hình).
 */
class EmojiPicker : public QWidget
{
    Q_OBJECT

public:
    explicit EmojiPicker(QWidget *parent = nullptr);

    //! Hiện bảng ngay phía trên widget neo.
    void popupAbove(QWidget *anchor);

signals:
    void emojiChosen(const QString &emoji);

private:
    void buildCategories();
    QWidget *buildGrid(const QStringList &emojis);
    void applyTheme();
    void rememberRecent(const QString &emoji);
    QStringList recentEmojis() const;
    void refreshRecentGrid();

    QTabBar *m_tabs = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLineEdit *m_search = nullptr;
    QWidget *m_recentPage = nullptr;
};
