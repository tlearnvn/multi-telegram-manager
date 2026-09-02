#pragma once

#include <QList>
#include <QWidget>

class Composer;
class EmojiPicker;
class IconButton;
class MessageDelegate;
class MessageModel;
class TdAccount;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QPushButton;
class QTimer;

/*!
 * \brief Khung hội thoại: thanh tiêu đề, danh sách tin nhắn, ô soạn tin.
 *
 * Đảm nhiệm mọi tương tác với tin nhắn: trả lời, sửa, xoá, chuyển tiếp, ghim,
 * chép, tải tệp, mở liên kết, chọn nhiều tin, cuộn để nạp thêm lịch sử và tự
 * đánh dấu đã đọc phần đang xem.
 */
class ChatView : public QWidget
{
    Q_OBJECT

public:
    explicit ChatView(QWidget *parent = nullptr);

    void setAccount(TdAccount *account);
    void setChat(qint64 chatId);
    qint64 chatId() const { return m_chatId; }

    void focusComposer();
    void openSearchBar();
    void jumpToMessage(qint64 messageId);

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void forwardRequested(qint64 fromChatId, const QList<qint64> &messageIds);
    void chatInfoRequested(qint64 chatId);
    void statusMessage(const QString &text);

private slots:
    void onSend(const QString &text);
    void onEditSubmitted(qint64 messageId, const QString &text);
    void onFiles(const QStringList &paths);
    void onListClicked(const QModelIndex &index);
    void onListDoubleClicked(const QModelIndex &index);
    void showMessageMenu(const QPoint &position);
    void onScrolled(int value);
    void onNewestAppended();
    void onOlderInserted(int count);
    void refreshHeader();
    void applyTheme();
    void runInChatSearch();
    void markVisibleAsRead();

private:
    void buildUi();
    void updateComposerState();
    void positionScrollDownButton();
    void openMedia(const class MessageEntry &entry);
    void copySelection();
    void deleteSelection();
    QList<qint64> selectedMessageIds() const;
    bool isNearBottom() const;

    TdAccount *m_account = nullptr;
    qint64 m_chatId = 0;

    MessageModel *m_model = nullptr;
    MessageDelegate *m_delegate = nullptr;
    QListView *m_list = nullptr;
    Composer *m_composer = nullptr;
    EmojiPicker *m_emoji = nullptr;

    QWidget *m_header = nullptr;
    QWidget *m_avatarBox = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    IconButton *m_searchButton = nullptr;
    IconButton *m_infoButton = nullptr;
    IconButton *m_menuButton = nullptr;

    QWidget *m_searchBar = nullptr;
    QLineEdit *m_searchInput = nullptr;
    QLabel *m_searchStatus = nullptr;
    IconButton *m_searchPrev = nullptr;
    IconButton *m_searchNext = nullptr;
    QList<qint64> m_searchHits;
    int m_searchCursor = -1;

    QWidget *m_selectionBar = nullptr;
    QLabel *m_selectionLabel = nullptr;

    QLabel *m_placeholder = nullptr;
    QWidget *m_body = nullptr;

    //! Tin nhắn đang ở đỉnh khung ngay trước khi nạp thêm lịch sử, dùng để
    //! cuộn về đúng chỗ cũ sau khi chèn (nếu không màn hình sẽ nhảy).
    qint64 m_anchorMessageId = 0;
    int m_anchorOffset = 0;

    QPushButton *m_scrollDownButton = nullptr;
    QTimer *m_readTimer = nullptr;
};
