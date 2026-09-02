#pragma once

#include <QAtomicInt>
#include <QJsonObject>
#include <QThread>

/*!
 * \brief Luồng nhận dữ liệu duy nhất cho toàn bộ tiến trình.
 *
 * tdjson chỉ có một hàng đợi nhận chung: td_receive() trả về phản hồi của mọi
 * client, phân biệt nhau bằng trường "@client_id". Hơn nữa con trỏ trả về chỉ
 * hợp lệ tới lần gọi kế tiếp *trong cùng luồng*, nên bắt buộc phải có đúng một
 * luồng gọi td_receive(). Lớp này giữ luồng đó, phân tích JSON rồi phát tín
 * hiệu về luồng giao diện (kết nối kiểu queued tự động).
 *
 * Việc gửi (td_send) an toàn từ mọi luồng nên send() gọi trực tiếp.
 */
class TdTransport : public QObject
{
    Q_OBJECT

public:
    static TdTransport &instance();

    //! Khởi động luồng nhận. Trả về false nếu chưa nạp được tdjson.
    bool start();

    //! Dừng luồng nhận và chờ nó kết thúc.
    void stop();

    bool isRunning() const;

    //! Gửi yêu cầu tới một client TDLib.
    void send(int clientId, const QJsonObject &request);

    //! Chạy hàm đồng bộ của TDLib (không cần client).
    QJsonObject executeSync(const QJsonObject &request);

signals:
    //! Phát cho mỗi đối tượng TDLib nhận được. clientId = -1 nếu không rõ.
    void received(int clientId, const QJsonObject &object);

private:
    explicit TdTransport(QObject *parent = nullptr);
    ~TdTransport() override;
    Q_DISABLE_COPY_MOVE(TdTransport)

    class Worker : public QThread
    {
    public:
        explicit Worker(TdTransport *owner);
        void requestStop();

    protected:
        void run() override;

    private:
        TdTransport *m_owner;
        QAtomicInt m_stop;
    };

    Worker *m_worker = nullptr;
};
