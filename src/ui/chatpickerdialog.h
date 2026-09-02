#pragma once

#include <QDialog>
#include <QList>

class AccountManager;
class TdAccount;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

/*!
 * \brief Hộp thoại chọn cuộc trò chuyện — dùng cho chuyển tiếp và gửi hàng loạt.
 *
 * Có thể chọn trong một tài khoản hoặc gộp mọi tài khoản (mỗi dòng ghi rõ
 * thuộc tài khoản nào), một hoặc nhiều lựa chọn.
 */
class ChatPickerDialog : public QDialog
{
    Q_OBJECT

public:
    struct Selection
    {
        TdAccount *account = nullptr;
        qint64 chatId = 0;
        QString title;
    };

    //! \a account khác nullptr = chỉ chọn trong tài khoản đó.
    ChatPickerDialog(AccountManager *manager, TdAccount *account,
                     bool multiSelect, QWidget *parent = nullptr);

    QList<Selection> selections() const;

    void setHeaderText(const QString &text);

private slots:
    void reload();

private:
    void addAccountChats(TdAccount *account, const QString &filter);

    AccountManager *m_manager;
    TdAccount *m_account;
    bool m_multiSelect;

    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    class QLabel *m_header = nullptr;
};
