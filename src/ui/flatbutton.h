#pragma once

#include "ui/iconfactory.h"

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

/*!
 * \brief Nút chỉ có biểu tượng, tự đổi màu theo chủ đề.
 *
 * Dùng cho thanh công cụ của khung chat và hộp soạn tin: gọn, không viền,
 * sáng lên khi trỏ chuột vào.
 */
class IconButton : public QPushButton
{
    Q_OBJECT

public:
    explicit IconButton(Icons::Name name, const QString &tooltip,
                        int iconSize = 20, QWidget *parent = nullptr);

    void setIconName(Icons::Name name);
    void setAccented(bool accented);
    void setDanger(bool danger);
    void setBadgeCount(int count);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void refreshIcon();

private:
    Icons::Name m_name;
    int m_iconSize;
    bool m_accented = false;
    bool m_danger = false;
    bool m_hovered = false;
    int m_badge = 0;
};

/*!
 * \brief Nhãn tiêu đề mục trong hộp thoại cài đặt.
 */
class SectionLabel : public QLabel
{
    Q_OBJECT

public:
    explicit SectionLabel(const QString &text, QWidget *parent = nullptr);
};

/*!
 * \brief Đường kẻ phân cách 1px theo màu chủ đề.
 */
class Separator : public QWidget
{
    Q_OBJECT

public:
    explicit Separator(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Orientation m_orientation;
};
