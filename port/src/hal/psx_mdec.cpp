#include "psx_mdec.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace PSX {

// PS1 MDEC Standard ZigZag scan order
static const uint8_t s_ZigZag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

// PS1 Default Luminance Quantization Matrix
static const uint8_t s_DefaultQuantLuma[64] = {
     2, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};
// Total entries: 111
struct VlcEntry { uint16_t code; uint8_t len; uint8_t zeros; int16_t val; };
static const VlcEntry s_VlcTable[] = {
    { 0b11, 2, 0, 1 },
    { 0b011, 3, 1, 1 },
    { 0b0100, 4, 0, 2 },
    { 0b0101, 4, 2, 1 },
    { 0b00101, 5, 0, 3 },
    { 0b00110, 5, 4, 1 },
    { 0b00111, 5, 3, 1 },
    { 0b000100, 6, 7, 1 },
    { 0b000101, 6, 6, 1 },
    { 0b000110, 6, 1, 2 },
    { 0b000111, 6, 5, 1 },
    { 0b0000100, 7, 2, 2 },
    { 0b0000101, 7, 9, 1 },
    { 0b0000110, 7, 0, 4 },
    { 0b0000111, 7, 8, 1 },
    { 0b00100000, 8, 13, 1 },
    { 0b00100001, 8, 0, 6 },
    { 0b00100010, 8, 12, 1 },
    { 0b00100011, 8, 11, 1 },
    { 0b00100100, 8, 3, 2 },
    { 0b00100101, 8, 1, 3 },
    { 0b00100110, 8, 0, 5 },
    { 0b00100111, 8, 10, 1 },
    { 0b0000001000, 10, 16, 1 },
    { 0b0000001001, 10, 5, 2 },
    { 0b0000001010, 10, 0, 7 },
    { 0b0000001011, 10, 2, 3 },
    { 0b0000001100, 10, 1, 4 },
    { 0b0000001101, 10, 15, 1 },
    { 0b0000001110, 10, 14, 1 },
    { 0b0000001111, 10, 4, 2 },
    { 0b000000010000, 12, 0, 11 },
    { 0b000000010001, 12, 8, 2 },
    { 0b000000010010, 12, 4, 3 },
    { 0b000000010011, 12, 0, 10 },
    { 0b000000010100, 12, 2, 4 },
    { 0b000000010101, 12, 7, 2 },
    { 0b000000010110, 12, 21, 1 },
    { 0b000000010111, 12, 20, 1 },
    { 0b000000011000, 12, 0, 9 },
    { 0b000000011001, 12, 19, 1 },
    { 0b000000011010, 12, 18, 1 },
    { 0b000000011011, 12, 1, 5 },
    { 0b000000011100, 12, 3, 3 },
    { 0b000000011101, 12, 0, 8 },
    { 0b000000011110, 12, 6, 2 },
    { 0b000000011111, 12, 17, 1 },
    { 0b0000000010000, 13, 10, 2 },
    { 0b0000000010001, 13, 9, 2 },
    { 0b0000000010010, 13, 5, 3 },
    { 0b0000000010011, 13, 3, 4 },
    { 0b0000000010100, 13, 2, 5 },
    { 0b0000000010101, 13, 1, 7 },
    { 0b0000000010110, 13, 1, 6 },
    { 0b0000000010111, 13, 0, 15 },
    { 0b0000000011000, 13, 0, 14 },
    { 0b0000000011001, 13, 0, 13 },
    { 0b0000000011010, 13, 0, 12 },
    { 0b0000000011011, 13, 26, 1 },
    { 0b0000000011100, 13, 25, 1 },
    { 0b0000000011101, 13, 24, 1 },
    { 0b0000000011110, 13, 23, 1 },
    { 0b0000000011111, 13, 22, 1 },
    { 0b00000000010000, 14, 0, 31 },
    { 0b00000000010001, 14, 0, 30 },
    { 0b00000000010010, 14, 0, 29 },
    { 0b00000000010011, 14, 0, 28 },
    { 0b00000000010100, 14, 0, 27 },
    { 0b00000000010101, 14, 0, 26 },
    { 0b00000000010110, 14, 0, 25 },
    { 0b00000000010111, 14, 0, 24 },
    { 0b00000000011000, 14, 0, 23 },
    { 0b00000000011001, 14, 0, 22 },
    { 0b00000000011010, 14, 0, 21 },
    { 0b00000000011011, 14, 0, 20 },
    { 0b00000000011100, 14, 0, 19 },
    { 0b00000000011101, 14, 0, 18 },
    { 0b00000000011110, 14, 0, 17 },
    { 0b00000000011111, 14, 0, 16 },
    { 0b000000000010000, 15, 0, 40 },
    { 0b000000000010001, 15, 0, 39 },
    { 0b000000000010010, 15, 0, 38 },
    { 0b000000000010011, 15, 0, 37 },
    { 0b000000000010100, 15, 0, 36 },
    { 0b000000000010101, 15, 0, 35 },
    { 0b000000000010110, 15, 0, 34 },
    { 0b000000000010111, 15, 0, 33 },
    { 0b000000000011000, 15, 0, 32 },
    { 0b000000000011001, 15, 1, 14 },
    { 0b000000000011010, 15, 1, 13 },
    { 0b000000000011011, 15, 1, 12 },
    { 0b000000000011100, 15, 1, 11 },
    { 0b000000000011101, 15, 1, 10 },
    { 0b000000000011110, 15, 1, 9 },
    { 0b000000000011111, 15, 1, 8 },
    { 0b0000000000010000, 16, 1, 18 },
    { 0b0000000000010001, 16, 1, 17 },
    { 0b0000000000010010, 16, 1, 16 },
    { 0b0000000000010011, 16, 1, 15 },
    { 0b0000000000010100, 16, 6, 3 },
    { 0b0000000000010101, 16, 16, 2 },
    { 0b0000000000010110, 16, 15, 2 },
    { 0b0000000000010111, 16, 14, 2 },
    { 0b0000000000011000, 16, 13, 2 },
    { 0b0000000000011001, 16, 12, 2 },
    { 0b0000000000011010, 16, 11, 2 },
    { 0b0000000000011011, 16, 31, 1 },
    { 0b0000000000011100, 16, 30, 1 },
    { 0b0000000000011101, 16, 29, 1 },
    { 0b0000000000011110, 16, 28, 1 },
    { 0b0000000000011111, 16, 27, 1 },
};

class BitStreamReader {
public:
    BitStreamReader(const uint8_t* data, size_t size)
        : m_Data(data), m_Size(size), m_Offset(0), m_BitBuf(0), m_BitsInBuf(0) {}

    void Refill() {
        while (m_BitsInBuf <= 16 && m_Offset + 2 <= m_Size) {
            uint16_t val = static_cast<uint16_t>(m_Data[m_Offset]) |
                           (static_cast<uint16_t>(m_Data[m_Offset + 1]) << 8);
            m_Offset += 2;
            m_BitBuf = (m_BitBuf << 16) | val;
            m_BitsInBuf += 16;
        }
    }

    uint32_t Peek(uint32_t n) {
        Refill();
        if (m_BitsInBuf < n) return 0;
        return (m_BitBuf >> (m_BitsInBuf - n)) & ((1u << n) - 1u);
    }

    void Skip(uint32_t n) {
        if (m_BitsInBuf >= n) {
            m_BitsInBuf -= n;
        } else {
            m_BitsInBuf = 0;
        }
    }

    uint32_t Read(uint32_t n) {
        uint32_t val = Peek(n);
        Skip(n);
        return val;
    }

    int32_t ReadSigned(uint32_t n) {
        uint32_t val = Read(n);
        if (val & (1u << (n - 1))) {
            return static_cast<int32_t>(val) - static_cast<int32_t>(1u << n);
        }
        return static_cast<int32_t>(val);
    }

    bool HasBits(uint32_t n) {
        Refill();
        return m_BitsInBuf >= n;
    }

private:
    const uint8_t* m_Data;
    size_t m_Size;
    size_t m_Offset;
    uint32_t m_BitBuf;
    uint32_t m_BitsInBuf;
};

static bool DecodeCrCbDc(BitStreamReader& bs, int32_t& outDiff) {
    struct Prefix { uint8_t len; uint8_t val; uint8_t bitLen; int16_t baseNeg; int16_t basePos; };
    static const Prefix prefixes[] = {
        { 8, 0b11111110, 8, -255, 128 },
        { 7, 0b1111110,  7, -127,  64 },
        { 6, 0b111110,   6,  -63,  32 },
        { 5, 0b11110,    5,  -31,  16 },
        { 4, 0b1110,     4,  -15,   8 },
        { 3, 0b110,      3,   -7,   4 },
        { 2, 0b10,       2,   -3,   2 },
        { 2, 0b01,       1,   -1,   1 },
        { 2, 0b00,       0,    0,   0 },
    };

    for (const auto& p : prefixes) {
        if (bs.Peek(p.len) == p.val) {
            bs.Skip(p.len);
            if (p.bitLen == 0) {
                outDiff = 0;
                return true;
            }
            uint32_t sign = bs.Read(1);
            uint32_t rem = (p.bitLen > 1) ? bs.Read(p.bitLen - 1) : 0;
            int32_t diff = static_cast<int32_t>(rem) + (sign == 0 ? p.baseNeg : p.basePos);
            outDiff = diff * 4;
            return true;
        }
    }
    return false;
}

static bool DecodeYDc(BitStreamReader& bs, int32_t& outDiff) {
    struct Prefix { uint8_t len; uint8_t val; uint8_t bitLen; int16_t baseNeg; int16_t basePos; };
    static const Prefix prefixes[] = {
        { 7, 0b1111110, 8, -255, 128 },
        { 6, 0b111110,  7, -127,  64 },
        { 5, 0b11110,   6,  -63,  32 },
        { 4, 0b1110,    5,  -31,  16 },
        { 3, 0b110,     4,  -15,   8 },
        { 3, 0b101,     3,   -7,   4 },
        { 2, 0b01,      2,   -3,   2 },
        { 2, 0b00,      1,   -1,   1 },
        { 3, 0b100,     0,    0,   0 },
    };

    for (const auto& p : prefixes) {
        if (bs.Peek(p.len) == p.val) {
            bs.Skip(p.len);
            if (p.bitLen == 0) {
                outDiff = 0;
                return true;
            }
            uint32_t sign = bs.Read(1);
            uint32_t rem = (p.bitLen > 1) ? bs.Read(p.bitLen - 1) : 0;
            int32_t diff = static_cast<int32_t>(rem) + (sign == 0 ? p.baseNeg : p.basePos);
            outDiff = diff * 4;
            return true;
        }
    }
    return false;
}

static bool DecodeAcBlock(BitStreamReader& bs, std::vector<std::pair<uint8_t, int16_t>>& outCoeffs) {
    outCoeffs.clear();
    while (true) {
        if (bs.Peek(2) == 0b10) {
            bs.Skip(2);
            break;
        }
        if (bs.Peek(6) == 0b000001) {
            bs.Skip(6);
            uint8_t zeros = static_cast<uint8_t>(bs.Read(6));
            uint32_t rawCoeff = bs.Read(10);
            int16_t coeff = static_cast<int16_t>(rawCoeff);
            if (coeff & 0x200) coeff -= 0x400;
            outCoeffs.emplace_back(zeros, coeff);
            continue;
        }

        bool matched = false;
        for (size_t i = 0; i < sizeof(s_VlcTable) / sizeof(s_VlcTable[0]); ++i) {
            const auto& entry = s_VlcTable[i];
            if (bs.Peek(entry.len) == entry.code) {
                bs.Skip(entry.len);
                uint32_t sign = bs.Read(1);
                int16_t coeff = (sign == 1) ? -entry.val : entry.val;
                outCoeffs.emplace_back(entry.zeros, coeff);
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}

static void DequantAndIdct(int32_t dcVal, const std::vector<std::pair<uint8_t, int16_t>>& acList, uint16_t qscale, float outBlock[8][8]) {
    int16_t blk[64] = {0};
    blk[0] = static_cast<int16_t>(dcVal * s_DefaultQuantLuma[0]);
    int idx = 0;
    for (const auto& pair : acList) {
        idx += pair.first + 1;
        if (idx >= 64) break;
        uint8_t zIdx = s_ZigZag[idx];
        blk[zIdx] = static_cast<int16_t>((2 * pair.second * qscale * s_DefaultQuantLuma[zIdx]) / 16);
    }

    // 8x8 IDCT
    const float pi = 3.14159265358979323846f;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            float sum = 0.0f;
            for (int u = 0; u < 8; ++u) {
                float cu = (u == 0) ? 0.7071067811865475f : 1.0f;
                for (int v = 0; v < 8; ++v) {
                    float cv = (v == 0) ? 0.7071067811865475f : 1.0f;
                    sum += cu * cv * blk[v * 8 + u] *
                           std::cos((2.0f * x + 1.0f) * u * pi / 16.0f) *
                           std::cos((2.0f * y + 1.0f) * v * pi / 16.0f);
                }
            }
            outBlock[y][x] = sum * 0.25f;
        }
    }
}

void MdecDecoder::YuvToRgb(int32_t y, int32_t u, int32_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float Y = static_cast<float>(y);
    float Cb = static_cast<float>(u - 128);
    float Cr = static_cast<float>(v - 128);

    float R = Y + 1.402f * Cr;
    float G = Y - 0.34414f * Cb - 0.71414f * Cr;
    float B = Y + 1.772f * Cb;

    r = static_cast<uint8_t>(std::clamp(static_cast<int>(R + 0.5f), 0, 255));
    g = static_cast<uint8_t>(std::clamp(static_cast<int>(G + 0.5f), 0, 255));
    b = static_cast<uint8_t>(std::clamp(static_cast<int>(B + 0.5f), 0, 255));
}

void MdecDecoder::IdctBlock(int16_t* in, int16_t* out) {
    // Retained for compatibility
}

bool MdecDecoder::DecodeBsFrame(const uint8_t* bsData, size_t bsSize, std::vector<uint32_t>& outRgba) {
    if (!bsData || bsSize < 8) {
        return false;
    }

    uint16_t magic = static_cast<uint16_t>(bsData[2]) | (static_cast<uint16_t>(bsData[3]) << 8);
    if (magic != 0x3800) {
        return false;
    }

    uint16_t qscale = static_cast<uint16_t>(bsData[4]) | (static_cast<uint16_t>(bsData[5]) << 8);
    if (qscale == 0) qscale = 1;

    outRgba.resize(WIDTH * HEIGHT);

    BitStreamReader bs(bsData + 8, bsSize - 8);

    int32_t prevCr = 0;
    int32_t prevCb = 0;
    int32_t prevY = 0;

    std::vector<std::pair<uint8_t, int16_t>> acList;
    float crBlock[8][8];
    float cbBlock[8][8];
    float yBlocks[4][8][8];

    uint16_t version = static_cast<uint16_t>(bsData[6]) | (static_cast<uint16_t>(bsData[7]) << 8);

    if (version <= 2) {
        // PlayStation MDEC Version 2 (FMV .STR movie streams: 20 columns of 15 rows, direct 10-bit signed DC)
        for (int col = 0; col < 20; ++col) {
            for (int row = 0; row < 15; ++row) {
                // 1. Cr block
                int32_t crDc = bs.ReadSigned(10);
                if (!DecodeAcBlock(bs, acList)) return false;
                DequantAndIdct(crDc, acList, qscale, crBlock);

                // 2. Cb block
                int32_t cbDc = bs.ReadSigned(10);
                if (!DecodeAcBlock(bs, acList)) return false;
                DequantAndIdct(cbDc, acList, qscale, cbBlock);

                // 3. Y1..Y4 blocks
                for (int yIdx = 0; yIdx < 4; ++yIdx) {
                    int32_t yDc = bs.ReadSigned(10);
                    if (!DecodeAcBlock(bs, acList)) return false;
                    DequantAndIdct(yDc, acList, qscale, yBlocks[yIdx]);
                }

                // Assemble 16x16 macroblock into image
                int startX = col * 16;
                int startY = row * 16;
                for (int subY = 0; subY < 16; ++subY) {
                    int blockY = subY / 8;
                    int innerY = subY % 8;
                    int chromaY = subY / 2;
                    for (int subX = 0; subX < 16; ++subX) {
                        int blockX = subX / 8;
                        int innerX = subX % 8;
                        int chromaX = subX / 2;
                        int yBlockIdx = blockY * 2 + blockX;
                        float luma = yBlocks[yBlockIdx][innerY][innerX] + 128.0f;
                        float cb = cbBlock[chromaY][chromaX];
                        float cr = crBlock[chromaY][chromaX];

                        float rVal = luma + 1.402f * cr;
                        float gVal = luma - 0.34414f * cb - 0.71414f * cr;
                        float bVal = luma + 1.772f * cb;

                        uint8_t r = static_cast<uint8_t>(std::clamp(static_cast<int>(rVal + 0.5f), 0, 255));
                        uint8_t g = static_cast<uint8_t>(std::clamp(static_cast<int>(gVal + 0.5f), 0, 255));
                        uint8_t b = static_cast<uint8_t>(std::clamp(static_cast<int>(bVal + 0.5f), 0, 255));

                        int px = startX + subX;
                        int py = startY + subY;
                        outRgba[py * WIDTH + px] = (0xFFu << 24) | (static_cast<uint32_t>(b) << 16) |
                                                   (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
                    }
                }
            }
        }
        return true;
    }

    // PlayStation MDEC streams are ordered by vertical columns: 20 columns of 15 macroblocks (320x240)
    for (int col = 0; col < 20; ++col) {
        for (int row = 0; row < 15; ++row) {
            // 1. Cr block
            int32_t crDiff = 0;
            if (!DecodeCrCbDc(bs, crDiff)) { std::printf("MDEC Error: CrCb failed\n"); return false; }
            prevCr += crDiff;
            if (!DecodeAcBlock(bs, acList)) { std::printf("MDEC Error: AC failed\n"); return false; }
            DequantAndIdct(prevCr, acList, qscale, crBlock);

            // 2. Cb block
            int32_t cbDiff = 0;
            if (!DecodeCrCbDc(bs, cbDiff)) return false;
            prevCb += cbDiff;
            if (!DecodeAcBlock(bs, acList)) { std::printf("MDEC Error: AC failed\n"); return false; }
            DequantAndIdct(prevCb, acList, qscale, cbBlock);

            // 3. Y1..Y4 blocks
            for (int yIdx = 0; yIdx < 4; ++yIdx) {
                int32_t yDiff = 0;
                if (!DecodeYDc(bs, yDiff)) return false;
                prevY += yDiff;
                if (!DecodeAcBlock(bs, acList)) { std::printf("MDEC Error: AC failed\n"); return false; }
                DequantAndIdct(prevY, acList, qscale, yBlocks[yIdx]);
            }

            // Assemble 16x16 macroblock into image
            int startX = col * 16;
            int startY = row * 16;

            for (int subY = 0; subY < 16; ++subY) {
                int blockY = subY / 8;
                int innerY = subY % 8;
                int chromaY = subY / 2;

                for (int subX = 0; subX < 16; ++subX) {
                    int blockX = subX / 8;
                    int innerX = subX % 8;
                    int chromaX = subX / 2;

                    int yBlockIdx = blockY * 2 + blockX;
                    float luma = yBlocks[yBlockIdx][innerY][innerX] + 128.0f;
                    float cb = cbBlock[chromaY][chromaX];
                    float cr = crBlock[chromaY][chromaX];

                    float rVal = luma + 1.402f * cr;
                    float gVal = luma - 0.34414f * cb - 0.71414f * cr;
                    float bVal = luma + 1.772f * cb;

                    uint8_t r = static_cast<uint8_t>(std::clamp(static_cast<int>(rVal + 0.5f), 0, 255));
                    uint8_t g = static_cast<uint8_t>(std::clamp(static_cast<int>(gVal + 0.5f), 0, 255));
                    uint8_t b = static_cast<uint8_t>(std::clamp(static_cast<int>(bVal + 0.5f), 0, 255));

                    int px = startX + subX;
                    int py = startY + subY;
                    outRgba[py * WIDTH + px] = (0xFFu << 24) | (static_cast<uint32_t>(b) << 16) |
                                               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
                }
            }
        }
    }

    std::printf("[MDEC] Decoded room background cut successfully (320x240 RGB)\n");
    return true;
}

} // namespace PSX
