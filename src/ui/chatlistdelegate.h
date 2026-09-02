#pragma once

#include <QStyledItemDelegate>

class ChatListModel;

/*!
 * \brief Vẽ một dòng trong danh sách chat.
 *
 * Mỗi dòng gồm: avatar tròn (kèm chấm trực tuyến), tiêu đề, giờ tin cuối, nội
 * dung xem trước (hoặc "đang gõ…"), huy hiệu số tin chưa đọc, biểu tượng ghim
 * và tắt tiếng. Vẽ bằng delegate thay vì widget để danh sách hàng nghìn chat
 * vẫn cuộn mượt.
 */
class ChatListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ChatListDelegate(ChatListModel *model, QObject *parent = nullptr);

    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    ChatListModel *m_model;
    bool m_compact = false;
};
