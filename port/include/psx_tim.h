#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace PSX {

struct TimImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bpp = 0; // 4, 8, 16, or 24
    std::vector<uint32_t> rgbaPixels; // De-palettized 32-bit RGBA pixels
};

class TimDecoder {
public:
    static bool LoadFromFile(const std::wstring& filePath, TimImage& outImage);
    static bool LoadFromMemory(const uint8_t* data, size_t size, TimImage& outImage);

private:
    static uint32_t C15ToRgba(uint16_t c15);
};

} // namespace PSX
