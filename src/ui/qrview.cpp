#include "ui/qrview.h"

#include "ui/iconfactory.h"
#include "ui/theme.h"

#include <QPainter>

namespace {
constexpr int kQuietZone = 2;   // viền trắng bắt buộc quanh mã (đơn vị: ô)
}

QrView::QrView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void QrView::setData(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_code = text.isEmpty() ? QrCode() : QrCode::encode(text.toUtf8(), QrCode::Ecc::Medium);
    update();
}

void QrView::clear()
{
    m_text.clear();
    m_code = QrCode();
    update();
}

QSize QrView::sizeHint() const
{
    return QSize(240, 240);
}

void QrView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Mã QR luôn cần nền sáng và mực tối để máy quét đọc được, kể cả ở chủ đề tối.
    const int side = qMin(width(), height());
    const QRect box((width() - side) / 2, (height() - side) / 2, side, side);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(box, 10, 10);

    if (!m_code.isValid()) {
        painter.setPen(QColor(0x88, 0x88, 0x88));
        painter.drawText(box, Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Đang lấy mã QR…"));
        return;
    }

    const int modules = m_code.size() + kQuietZone * 2;
    const int scale = qMax(1, (side - 12) / modules);
    const int drawSize = scale * modules;
    const int originX = box.left() + (side - drawSize) / 2;
    const int originY = box.top() + (side - drawSize) / 2;

    painter.setBrush(QColor(0x10, 0x14, 0x1a));
    for (int y = 0; y < m_code.size(); ++y) {
        for (int x = 0; x < m_code.size(); ++x) {
            if (!m_code.module(x, y))
                continue;
            painter.drawRect(originX + (x + kQuietZone) * scale,
                             originY + (y + kQuietZone) * scale,
                             scale, scale);
        }
    }

    // Logo nhỏ ở giữa: mức sửa lỗi M cho phép che tới ~15% diện tích.
    const int logoSide = qMax(24, drawSize / 6);
    const QRect logoBox(originX + (drawSize - logoSide) / 2,
                        originY + (drawSize - logoSide) / 2,
                        logoSide, logoSide);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(logoBox.adjusted(-4, -4, 4, 4), 6, 6);
    painter.drawPixmap(logoBox, Icons::appLogo(logoSide * 2,
                                               Theme::instance().colors().accent));
}
