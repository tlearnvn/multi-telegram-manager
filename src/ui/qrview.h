#pragma once

#include "core/qrcode.h"

#include <QString>
#include <QWidget>

/*!
 * \brief Widget hiển thị mã QR (vẽ trực tiếp, luôn nét ở mọi kích cỡ).
 */
class QrView : public QWidget
{
    Q_OBJECT

public:
    explicit QrView(QWidget *parent = nullptr);

    void setData(const QString &text);
    void clear();
    bool hasCode() const { return m_code.isValid(); }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QrCode m_code;
    QString m_text;
};
