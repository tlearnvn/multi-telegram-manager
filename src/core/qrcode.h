#pragma once

#include <QByteArray>
#include <QVector>

/*!
 * \brief Bộ tạo mã QR tự viết (chế độ byte, phiên bản 1–20).
 *
 * Ứng dụng không phụ thuộc thư viện ngoài nào, nên mã QR để đăng nhập bằng
 * điện thoại được sinh tại đây theo đúng chuẩn ISO/IEC 18004: chọn phiên bản
 * nhỏ nhất vừa dữ liệu, thêm sửa lỗi Reed–Solomon, đan xen khối, vẽ các hoa
 * văn định vị, thử cả 8 mặt nạ rồi chọn mặt nạ có điểm phạt thấp nhất.
 */
class QrCode
{
public:
    //! Mức sửa lỗi. Chỉ cần L và M cho nhu cầu hiển thị trên màn hình.
    enum class Ecc { Low, Medium };

    QrCode() = default;

    //! Tạo mã QR từ dữ liệu bất kỳ. Trả về mã rỗng nếu dữ liệu quá dài.
    //! forceMask chỉ dùng cho kiểm thử; -1 = tự chọn mặt nạ tốt nhất.
    static QrCode encode(const QByteArray &data, Ecc ecc = Ecc::Medium, int forceMask = -1);

    bool isValid() const { return m_size > 0; }
    int size() const { return m_size; }
    int version() const { return m_version; }

    //! true = ô tối (vẽ màu mực), false = ô sáng.
    bool module(int x, int y) const;

private:
    QrCode(int version, Ecc ecc, const QByteArray &dataCodewords, int forceMask);

    void drawFunctionPatterns();
    void drawFormatBits(int mask);
    void drawVersion();
    void drawFinderPattern(int x, int y);
    void drawAlignmentPattern(int x, int y);
    void setFunctionModule(int x, int y, bool dark);
    void drawCodewords(const QByteArray &data);
    void applyMask(int mask);
    int penaltyScore() const;

    static QByteArray addEccAndInterleave(int version, Ecc ecc, const QByteArray &data);
    static QVector<quint8> reedSolomonComputeDivisor(int degree);
    static QVector<quint8> reedSolomonComputeRemainder(const QByteArray &data,
                                                       const QVector<quint8> &divisor);
    static quint8 reedSolomonMultiply(quint8 x, quint8 y);
    static int getNumRawDataModules(int version);
    static int getNumDataCodewords(int version, Ecc ecc);
    static int eccCodewordsPerBlock(int version, Ecc ecc);
    static int numErrorCorrectionBlocks(int version, Ecc ecc);
    static QVector<int> alignmentPatternPositions(int version);

    int m_version = 0;
    int m_size = 0;
    Ecc m_ecc = Ecc::Medium;
    QVector<bool> m_modules;      //!< m_size * m_size
    QVector<bool> m_isFunction;
};
