#pragma once
#include <cstdint>
#include <vector>

namespace PSX {

// Authentic Sony PS1 MDEC (Macroblock Discrete Cosine Transform) Decoder
// Decodes 320x240 v3 bitstream frames (ROOMsrr.BSS background slices and .STR FMVs)
class MdecDecoder {
public:
    static constexpr uint32_t WIDTH = 320;
    static constexpr uint32_t HEIGHT = 240;

    // Decodes a raw PS1 BS chunk (0x8000 bytes) into 320x240 RGBA32 pixels (for DX12 upload)
    static bool DecodeBsFrame(const uint8_t* bsData, size_t bsSize, std::vector<uint32_t>& outRgba);

private:
    static void IdctBlock(int16_t* block, int16_t* out);
    static void YuvToRgb(int32_t y, int32_t u, int32_t v, uint8_t& r, uint8_t& g, uint8_t& b);
};

} // namespace PSX
