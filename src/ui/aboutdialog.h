#pragma once

#include <QDialog>

/*!
 * \brief Hộp thoại "Giới thiệu": phiên bản, nơi lưu dữ liệu, trạng thái TDLib
 *        và bản ghi log gần đây để tiện chẩn đoán sự cố.
 */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
