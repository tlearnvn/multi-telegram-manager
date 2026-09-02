#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class TdAccount;
class QJsonObject;
class QStackedWidget;
class QTabBar;

/*!
 * \brief Một ô nhãn dán trong bảng chọn.
 *
 * Vẽ ảnh thu nhỏ nếu đã tải được, chưa có thì hiện emoji tương ứng — nhờ vậy
 * bảng dùng được ngay chứ không phải chờ tải hết ảnh.
 */
class StickerButton : public QWidget
{
    Q_OBJECT

public:
    struct Info
    {
        int fileId = 0;        //!< mã tệp của chính nhãn dán (để gửi lại)
        int thumbFileId = 0;   //!< mã tệp ảnh thu nhỏ (để hiển thị)
        QString thumbPath;     //!< đường dẫn cục bộ khi đã tải xong
        QString emoji;
        int width = 512;
        int height = 512;
    };

    explicit StickerButton(const Info &info, QWidget *parent = nullptr);

    const Info &info() const { return m_info; }
    void setThumbPath(const QString &path);

signals:
    void chosen(const StickerButton::Info &info);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Info m_info;
    bool m_hovered = false;
};

/*!
 * \brief Bảng chọn nhãn dán để gửi.
 *
 * Lấy danh sách nhãn dán dùng gần đây và nhãn dán yêu thích từ Telegram, tải
 * ảnh thu nhỏ rồi cho bấm để gửi. Nhãn dán động (.tgs) không phát được hoạt
 * ảnh nên hiển thị bằng ảnh thu nhỏ tĩnh.
 */
class StickerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StickerPanel(QWidget *parent = nullptr);

    void setAccount(TdAccount *account);

    //! Hiện bảng phía trên widget neo và tải lại danh sách.
    void popupAbove(QWidget *anchor);

signals:
    void stickerChosen(int fileId, const QString &emoji, int width, int height);

private slots:
    void applyTheme();
    void onFileReady(int fileId, const QString &localPath);

private:
    QWidget *buildGrid(const QList<StickerButton::Info> &stickers, const QString &emptyText);
    void reload();
    void fillPage(int pageIndex, const QList<StickerButton::Info> &stickers,
                  const QString &emptyText);
    QList<StickerButton::Info> parseStickers(const QJsonObject &result) const;

    TdAccount *m_account = nullptr;
    QTabBar *m_tabs = nullptr;
    QStackedWidget *m_pages = nullptr;
    QList<StickerButton *> m_buttons;
};
