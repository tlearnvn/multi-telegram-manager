#include "core/qrcode.h"

#include <QtGlobal>

namespace {

constexpr int kMinVersion = 1;
constexpr int kMaxVersion = 20;

// Số codeword sửa lỗi trên mỗi khối, tra theo phiên bản (1..20) — bảng chuẩn
// ISO/IEC 18004, chỉ giữ hai mức L và M.
const int kEccPerBlockLow[]    = { -1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18,
                                   20, 24, 26, 30, 22, 24, 28, 30, 28, 28 };
const int kEccPerBlockMedium[] = { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26,
                                   30, 22, 22, 24, 24, 28, 28, 26, 26, 26 };

// Số khối sửa lỗi.
const int kBlocksLow[]    = { -1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,
                              4, 4, 4, 4, 6, 6, 6, 6, 7, 8 };
const int kBlocksMedium[] = { -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 4,
                              4, 8, 9, 9, 10, 10, 11, 13, 14, 16 };

int eccLevelBits(QrCode::Ecc ecc)
{
    // Giá trị dùng trong khối thông tin định dạng: L = 1, M = 0.
    return ecc == QrCode::Ecc::Low ? 1 : 0;
}

//! Nối bit vào cuối chuỗi bit đang dựng.
void appendBits(QVector<bool> &bits, quint32 value, int length)
{
    for (int i = length - 1; i >= 0; --i)
        bits.append(((value >> i) & 1) != 0);
}

bool getBit(int value, int index)
{
    return ((value >> index) & 1) != 0;
}

} // namespace

// ---------------------------------------------------------------------------
//  Bảng tra
// ---------------------------------------------------------------------------

int QrCode::eccCodewordsPerBlock(int version, Ecc ecc)
{
    return ecc == Ecc::Low ? kEccPerBlockLow[version] : kEccPerBlockMedium[version];
}

int QrCode::numErrorCorrectionBlocks(int version, Ecc ecc)
{
    return ecc == Ecc::Low ? kBlocksLow[version] : kBlocksMedium[version];
}

int QrCode::getNumRawDataModules(int version)
{
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int numAlign = version / 7 + 2;
        result -= (25 * numAlign - 10) * numAlign - 55;
        if (version >= 7)
            result -= 36;
    }
    return result;
}

int QrCode::getNumDataCodewords(int version, Ecc ecc)
{
    return getNumRawDataModules(version) / 8
        - eccCodewordsPerBlock(version, ecc) * numErrorCorrectionBlocks(version, ecc);
}

QVector<int> QrCode::alignmentPatternPositions(int version)
{
    if (version == 1)
        return {};

    const int numAlign = version / 7 + 2;
    const int step = (version * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;

    QVector<int> result;
    for (int pos = version * 4 + 10; result.size() < numAlign - 1; pos -= step)
        result.prepend(pos);
    result.prepend(6);
    return result;
}

// ---------------------------------------------------------------------------
//  Reed–Solomon trên GF(256), đa thức nguyên thuỷ 0x11D
// ---------------------------------------------------------------------------

quint8 QrCode::reedSolomonMultiply(quint8 x, quint8 y)
{
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return static_cast<quint8>(z & 0xFF);
}

QVector<quint8> QrCode::reedSolomonComputeDivisor(int degree)
{
    QVector<quint8> result(degree, 0);
    result[degree - 1] = 1;   // đa thức đơn vị

    // Nhân dần với (x - r^i), r = 0x02 là phần tử sinh của GF(256).
    quint8 root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = reedSolomonMultiply(result[j], root);
            if (j + 1 < degree)
                result[j] = static_cast<quint8>(result[j] ^ result[j + 1]);
        }
        root = reedSolomonMultiply(root, 0x02);
    }
    return result;
}

QVector<quint8> QrCode::reedSolomonComputeRemainder(const QByteArray &data,
                                                    const QVector<quint8> &divisor)
{
    QVector<quint8> result(divisor.size(), 0);
    for (char raw : data) {
        const quint8 factor = static_cast<quint8>(static_cast<quint8>(raw) ^ result.first());
        result.removeFirst();
        result.append(0);
        for (int i = 0; i < divisor.size(); ++i)
            result[i] = static_cast<quint8>(result[i] ^ reedSolomonMultiply(divisor[i], factor));
    }
    return result;
}

// ---------------------------------------------------------------------------
//  Mã hoá
// ---------------------------------------------------------------------------

QrCode QrCode::encode(const QByteArray &data, Ecc ecc, int forceMask)
{
    // Chế độ byte: 4 bit chỉ thị + số ký tự (8 bit với v1–9, 16 bit với v10+).
    int version = 0;
    for (int candidate = kMinVersion; candidate <= kMaxVersion; ++candidate) {
        const int capacityBits = getNumDataCodewords(candidate, ecc) * 8;
        const int countBits = candidate <= 9 ? 8 : 16;
        const int neededBits = 4 + countBits + data.size() * 8;
        if (neededBits <= capacityBits) {
            version = candidate;
            break;
        }
    }
    if (version == 0)
        return QrCode();   // dữ liệu quá dài cho phiên bản tối đa

    const int countBits = version <= 9 ? 8 : 16;

    QVector<bool> bits;
    appendBits(bits, 4, 4);                                    // chỉ thị chế độ byte
    appendBits(bits, static_cast<quint32>(data.size()), countBits);
    for (char raw : data)
        appendBits(bits, static_cast<quint8>(raw), 8);

    const int capacityBits = getNumDataCodewords(version, ecc) * 8;

    // Bit kết thúc + đệm cho tròn byte + byte đệm 0xEC/0x11 xen kẽ.
    appendBits(bits, 0, qMin(4, capacityBits - bits.size()));
    appendBits(bits, 0, (8 - bits.size() % 8) % 8);
    for (quint8 pad = 0xEC; bits.size() < capacityBits; pad = pad == 0xEC ? 0x11 : 0xEC)
        appendBits(bits, pad, 8);

    QByteArray codewords(bits.size() / 8, '\0');
    for (int i = 0; i < bits.size(); ++i) {
        if (bits.at(i))
            codewords[i >> 3] = static_cast<char>(codewords[i >> 3] | (1 << (7 - (i & 7))));
    }

    return QrCode(version, ecc, codewords, forceMask);
}

QByteArray QrCode::addEccAndInterleave(int version, Ecc ecc, const QByteArray &data)
{
    const int numBlocks = numErrorCorrectionBlocks(version, ecc);
    const int eccLen = eccCodewordsPerBlock(version, ecc);
    const int rawCodewords = getNumRawDataModules(version) / 8;
    const int numShortBlocks = numBlocks - rawCodewords % numBlocks;
    const int shortBlockLen = rawCodewords / numBlocks;

    QVector<QByteArray> blocks;
    QVector<QVector<quint8>> eccBlocks;
    const QVector<quint8> divisor = reedSolomonComputeDivisor(eccLen);

    int offset = 0;
    for (int i = 0; i < numBlocks; ++i) {
        const int dataLen = shortBlockLen - eccLen + (i < numShortBlocks ? 0 : 1);
        const QByteArray block = data.mid(offset, dataLen);
        offset += dataLen;
        blocks.append(block);
        eccBlocks.append(reedSolomonComputeRemainder(block, divisor));
    }

    // Đan xen: lấy lần lượt codeword thứ i của từng khối.
    QByteArray result;
    result.reserve(rawCodewords);
    for (int i = 0; i < shortBlockLen - eccLen + 1; ++i) {
        for (int j = 0; j < blocks.size(); ++j) {
            if (i < blocks.at(j).size())
                result.append(blocks.at(j).at(i));
        }
    }
    for (int i = 0; i < eccLen; ++i) {
        for (const QVector<quint8> &block : eccBlocks)
            result.append(static_cast<char>(block.at(i)));
    }
    return result;
}

// ---------------------------------------------------------------------------
//  Dựng ma trận
// ---------------------------------------------------------------------------

QrCode::QrCode(int version, Ecc ecc, const QByteArray &dataCodewords, int forceMask)
    : m_version(version)
    , m_size(version * 4 + 17)
    , m_ecc(ecc)
{
    m_modules = QVector<bool>(m_size * m_size, false);
    m_isFunction = QVector<bool>(m_size * m_size, false);

    drawFunctionPatterns();
    drawCodewords(addEccAndInterleave(version, ecc, dataCodewords));

    // Thử cả 8 mặt nạ, giữ lại mặt nạ có điểm phạt nhỏ nhất.
    int bestMask = forceMask >= 0 && forceMask < 8 ? forceMask : 0;
    int bestPenalty = -1;
    for (int mask = 0; forceMask < 0 && mask < 8; ++mask) {
        applyMask(mask);
        drawFormatBits(mask);
        const int penalty = penaltyScore();
        if (bestPenalty < 0 || penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMask = mask;
        }
        applyMask(mask);   // XOR lần hai để hoàn tác
    }

    applyMask(bestMask);
    drawFormatBits(bestMask);
}

bool QrCode::module(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_size || y >= m_size)
        return false;
    return m_modules.at(y * m_size + x);
}

void QrCode::setFunctionModule(int x, int y, bool dark)
{
    if (x < 0 || y < 0 || x >= m_size || y >= m_size)
        return;
    m_modules[y * m_size + x] = dark;
    m_isFunction[y * m_size + x] = true;
}

void QrCode::drawFinderPattern(int x, int y)
{
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int distance = qMax(qAbs(dx), qAbs(dy));   // khoảng cách Chebyshev
            const int xx = x + dx;
            const int yy = y + dy;
            if (xx >= 0 && xx < m_size && yy >= 0 && yy < m_size)
                setFunctionModule(xx, yy, distance != 2 && distance != 4);
        }
    }
}

void QrCode::drawAlignmentPattern(int x, int y)
{
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx)
            setFunctionModule(x + dx, y + dy, qMax(qAbs(dx), qAbs(dy)) != 1);
    }
}

void QrCode::drawFunctionPatterns()
{
    // Hoa văn thời gian (hàng/cột số 6).
    for (int i = 0; i < m_size; ++i) {
        setFunctionModule(6, i, i % 2 == 0);
        setFunctionModule(i, 6, i % 2 == 0);
    }

    // Ba hoa văn định vị ở ba góc.
    drawFinderPattern(3, 3);
    drawFinderPattern(m_size - 4, 3);
    drawFinderPattern(3, m_size - 4);

    // Hoa văn căn chỉnh.
    const QVector<int> positions = alignmentPatternPositions(m_version);
    const int count = positions.size();
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j) {
            // Bỏ ba vị trí trùng với hoa văn định vị.
            const bool skip = (i == 0 && j == 0) || (i == 0 && j == count - 1)
                           || (i == count - 1 && j == 0);
            if (!skip)
                drawAlignmentPattern(positions.at(i), positions.at(j));
        }
    }

    // Đặt chỗ cho thông tin định dạng & phiên bản (giá trị thật vẽ sau).
    drawFormatBits(0);
    drawVersion();
}

void QrCode::drawFormatBits(int mask)
{
    const int data = eccLevelBits(m_ecc) << 3 | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bits = (data << 10 | rem) ^ 0x5412;

    // Bản sao thứ nhất quanh hoa văn định vị góc trên trái.
    for (int i = 0; i <= 5; ++i)
        setFunctionModule(8, i, getBit(bits, i));
    setFunctionModule(8, 7, getBit(bits, 6));
    setFunctionModule(8, 8, getBit(bits, 7));
    setFunctionModule(7, 8, getBit(bits, 8));
    for (int i = 9; i < 15; ++i)
        setFunctionModule(14 - i, 8, getBit(bits, i));

    // Bản sao thứ hai.
    for (int i = 0; i < 8; ++i)
        setFunctionModule(m_size - 1 - i, 8, getBit(bits, i));
    for (int i = 8; i < 15; ++i)
        setFunctionModule(8, m_size - 15 + i, getBit(bits, i));

    setFunctionModule(8, m_size - 8, true);   // ô tối cố định
}

void QrCode::drawVersion()
{
    if (m_version < 7)
        return;

    int rem = m_version;
    for (int i = 0; i < 12; ++i)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const int bits = m_version << 12 | rem;

    for (int i = 0; i < 18; ++i) {
        const bool dark = getBit(bits, i);
        const int a = m_size - 11 + i % 3;
        const int b = i / 3;
        setFunctionModule(a, b, dark);
        setFunctionModule(b, a, dark);
    }
}

void QrCode::drawCodewords(const QByteArray &data)
{
    int bitIndex = 0;
    const int totalBits = data.size() * 8;

    // Đi theo cột đôi từ phải sang trái, đổi chiều lên/xuống mỗi cột.
    for (int right = m_size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;   // bỏ qua cột thời gian
        for (int vert = 0; vert < m_size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? m_size - 1 - vert : vert;
                if (!m_isFunction.at(y * m_size + x) && bitIndex < totalBits) {
                    m_modules[y * m_size + x] =
                        getBit(static_cast<quint8>(data.at(bitIndex >> 3)), 7 - (bitIndex & 7));
                    ++bitIndex;
                }
                // Các bit dư còn lại (nếu có) mặc định là sáng — đúng chuẩn.
            }
        }
    }
}

void QrCode::applyMask(int mask)
{
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            if (m_isFunction.at(y * m_size + x))
                continue;

            bool invert = false;
            switch (mask) {
            case 0: invert = (x + y) % 2 == 0; break;
            case 1: invert = y % 2 == 0; break;
            case 2: invert = x % 3 == 0; break;
            case 3: invert = (x + y) % 3 == 0; break;
            case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
            case 5: invert = x * y % 2 + x * y % 3 == 0; break;
            case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
            case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            default: break;
            }
            if (invert)
                m_modules[y * m_size + x] = !m_modules.at(y * m_size + x);
        }
    }
}

int QrCode::penaltyScore() const
{
    // Bốn quy tắc phạt theo chuẩn: dãy dài cùng màu, khối 2x2, hoa văn giống
    // hoa văn định vị, và tỉ lệ ô tối lệch khỏi 50%.
    constexpr int N1 = 3;
    constexpr int N2 = 3;
    constexpr int N3 = 40;
    constexpr int N4 = 10;

    int result = 0;

    auto addRunPenalty = [&result](int runLength) {
        if (runLength >= 5)
            result += N1 + (runLength - 5);
    };

    // Theo hàng.
    for (int y = 0; y < m_size; ++y) {
        bool runColor = false;
        int runLength = 0;
        int history[7] = { 0, 0, 0, 0, 0, 0, 0 };
        for (int x = 0; x < m_size; ++x) {
            const bool color = module(x, y);
            if (x > 0 && color == runColor) {
                ++runLength;
            } else {
                if (x > 0) {
                    addRunPenalty(runLength);
                    for (int i = 6; i > 0; --i)
                        history[i] = history[i - 1];
                    history[0] = runLength;
                    // Mẫu 1:1:3:1:1 kèm khoảng trắng 4 ô ⇒ giống hoa văn định vị.
                    if (!runColor && history[1] > 0 && history[1] == history[2]
                        && history[2] == history[4] && history[4] == history[5]
                        && history[3] == history[1] * 3
                        && (history[0] >= history[1] * 4 || history[6] >= history[1] * 4)) {
                        result += N3;
                    }
                }
                runColor = color;
                runLength = 1;
            }
        }
        addRunPenalty(runLength);
    }

    // Theo cột.
    for (int x = 0; x < m_size; ++x) {
        bool runColor = false;
        int runLength = 0;
        int history[7] = { 0, 0, 0, 0, 0, 0, 0 };
        for (int y = 0; y < m_size; ++y) {
            const bool color = module(x, y);
            if (y > 0 && color == runColor) {
                ++runLength;
            } else {
                if (y > 0) {
                    addRunPenalty(runLength);
                    for (int i = 6; i > 0; --i)
                        history[i] = history[i - 1];
                    history[0] = runLength;
                    if (!runColor && history[1] > 0 && history[1] == history[2]
                        && history[2] == history[4] && history[4] == history[5]
                        && history[3] == history[1] * 3
                        && (history[0] >= history[1] * 4 || history[6] >= history[1] * 4)) {
                        result += N3;
                    }
                }
                runColor = color;
                runLength = 1;
            }
        }
        addRunPenalty(runLength);
    }

    // Khối 2x2 cùng màu.
    for (int y = 0; y < m_size - 1; ++y) {
        for (int x = 0; x < m_size - 1; ++x) {
            const bool color = module(x, y);
            if (color == module(x + 1, y) && color == module(x, y + 1)
                && color == module(x + 1, y + 1)) {
                result += N2;
            }
        }
    }

    // Tỉ lệ ô tối.
    int dark = 0;
    for (bool value : m_modules) {
        if (value)
            ++dark;
    }
    const int total = m_size * m_size;
    const int k = (qAbs(dark * 20 - total * 10) + total - 1) / total - 1;
    result += k * N4;

    return result;
}
