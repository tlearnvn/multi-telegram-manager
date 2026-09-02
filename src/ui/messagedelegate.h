#pragma once

#include "model/messageentry.h"

#include <QFont>
#include <QHash>
#include <QRect>
#include <QSharedPointer>
#include <QStyledItemDelegate>

class MessageModel;
class QTextDocument;

/*!
 * \brief Vẽ bong bóng tin nhắn.
 *
 * Tự tính chiều cao theo nội dung (chữ có ngắt dòng, ảnh, thẻ tệp, phần trả
 * lời, phản ứng), vẽ dải phân cách ngày, tên người gửi trong nhóm và dấu
 * đã gửi/đã đọc. Nội dung có định dạng (đậm, nghiêng, mã, liên kết) được dựng
 * bằng QTextDocument nên xuống dòng chuẩn và bấm được vào liên kết.
 */
class MessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    //! Vùng người dùng vừa bấm vào.
    enum class HitKind { None, Bubble, Media, FileAction, ReplyQuote, Link };

    struct Hit
    {
        HitKind kind = HitKind::None;
        QString link;
        qint64 replyToMessageId = 0;
    };

    explicit MessageDelegate(MessageModel *model, QObject *parent = nullptr);
    ~MessageDelegate() override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    //! Xác định người dùng bấm vào phần nào của bong bóng.
    Hit hitTest(const QStyleOptionViewItem &option, const QModelIndex &index,
                const QPoint &viewportPos) const;

    //! Xoá bộ đệm bố cục (gọi khi đổi chủ đề hoặc cỡ chữ).
    void invalidateCache();

    //! Bật/tắt hiển thị avatar người gửi trong nhóm.
    void setShowGroupAvatars(bool show);

signals:
    //! Yêu cầu tải tệp khi người dùng bấm vào ảnh/thẻ tệp chưa tải.
    void downloadRequested(int fileId);

private:
    struct Layout
    {
        QRect dividerRect;
        QRect servicePill;
        QRect avatarRect;
        QRect bubbleRect;
        QRect senderRect;
        QRect forwardRect;
        QRect replyRect;
        QRect mediaRect;
        QRect textRect;
        QRect fileCardRect;
        QRect fileButtonRect;
        QRect reactionRect;
        QRect metaRect;
        int totalHeight = 0;
        bool hasMediaBox = false;
        bool hasFileCard = false;
    };

    Layout computeLayout(const QStyleOptionViewItem &option, const QModelIndex &index,
                         const MessageEntry &entry) const;
    QTextDocument *documentFor(const MessageEntry &entry, int width, bool outgoing,
                               const QFont &font) const;
    QSize mediaDisplaySize(const MessageEntry &entry, int maxWidth) const;
    QString metaTextFor(const MessageEntry &entry) const;

    MessageModel *m_model;
    bool m_showGroupAvatars = true;
    mutable QHash<QString, QSharedPointer<QTextDocument>> m_documents;
    mutable QHash<QString, int> m_heights;
};
