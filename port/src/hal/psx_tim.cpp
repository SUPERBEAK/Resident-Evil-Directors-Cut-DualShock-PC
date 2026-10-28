#include "psx_tim.h"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace PSX {

uint32_t TimDecoder::C15ToRgba(uint16_t c15) {
  uint32_t r = ((c15 & 0x1F) * 255) / 31;
  uint32_t g = (((c15 >> 5) & 0x1F) * 255) / 31;
  uint32_t b = (((c15 >> 10) & 0x1F) * 255) / 31;
  uint32_t a = 0xFF; // Full opacity: 3D character models are solid opaque
  return (a << 24) | (b << 16) | (g << 8) | r;
}

bool TimDecoder::LoadFromFile(const std::wstring &filePath,
                              TimImage &outImage) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);
  return LoadFromMemory(buffer.data(), size, outImage);
}

bool TimDecoder::LoadFromMemory(const uint8_t *data, size_t size,
                                TimImage &outImage) {
  if (!data || size < 16)
    return false;

  uint32_t magic = *reinterpret_cast<const uint32_t *>(data);
  if ((magic & 0xFF) != 0x10)
    return false; // PS1 TIM ID

  uint32_t flags = *reinterpret_cast<const uint32_t *>(data + 4);
  uint32_t pmode = flags & 0x07;
  bool hasClut = (flags & 0x08) != 0;

  size_t offset = 8;
  std::vector<uint32_t> palette;

  if (hasClut) {
    if (offset + 12 > size)
      return false;
    uint32_t clutBsize = *reinterpret_cast<const uint32_t *>(data + offset);
    uint16_t cnum = *reinterpret_cast<const uint16_t *>(data + offset + 8);
    uint16_t ncolors = *reinterpret_cast<const uint16_t *>(data + offset + 10);
    uint32_t totalColors = cnum * ncolors;

    offset += 12;
    palette.resize(totalColors);
    const uint16_t *clutRaw = reinterpret_cast<const uint16_t *>(data + offset);
    for (uint32_t i = 0; i < totalColors; ++i) {
      palette[i] = C15ToRgba(clutRaw[i]);
    }
    offset += clutBsize - 12;
  }

  if (offset + 12 > size)
    return false;
  uint32_t imgBsize = *reinterpret_cast<const uint32_t *>(data + offset);
  uint16_t iw = *reinterpret_cast<const uint16_t *>(data + offset + 8);
  uint16_t ih = *reinterpret_cast<const uint16_t *>(data + offset + 10);
  offset += 12;

  uint32_t pixelWidth = 0;
  if (pmode == 0) { // 4-bit CLUT
    pixelWidth = iw * 4;
    outImage.rgbaPixels.resize(pixelWidth * ih);
    const uint8_t *srcPix = data + offset;
    for (uint32_t y = 0; y < ih; ++y) {
      for (uint32_t x = 0; x < pixelWidth; x += 2) {
        uint8_t b = srcPix[y * (iw * 2) + (x / 2)];
        uint8_t idx0 = b & 0x0F;
        uint8_t idx1 = (b >> 4) & 0x0F;
        outImage.rgbaPixels[y * pixelWidth + x] =
            (idx0 < palette.size()) ? palette[idx0] : 0;
        outImage.rgbaPixels[y * pixelWidth + x + 1] =
            (idx1 < palette.size()) ? palette[idx1] : 0;
      }
    }
  } else if (pmode == 1) { // 8-bit CLUT
    pixelWidth = iw * 2;
    outImage.rgbaPixels.resize(pixelWidth * ih);
    const uint8_t *srcPix = data + offset;
    for (uint32_t y = 0; y < ih; ++y) {
      for (uint32_t x = 0; x < pixelWidth; ++x) {
        uint8_t idx = srcPix[y * pixelWidth + x];
        uint32_t palOffset = (x / 128) * 256;
        uint32_t palIdx =
            (palOffset + idx < palette.size()) ? (palOffset + idx) : idx;
        outImage.rgbaPixels[y * pixelWidth + x] =
            (palIdx < palette.size()) ? palette[palIdx] : 0;
      }
    }
  } else if (pmode == 2) { // 16-bit direct RGB555
    pixelWidth = iw;
    outImage.rgbaPixels.resize(pixelWidth * ih);
    const uint16_t *srcPix = reinterpret_cast<const uint16_t *>(data + offset);
    for (uint32_t y = 0; y < ih; ++y) {
      for (uint32_t x = 0; x < pixelWidth; ++x) {
        outImage.rgbaPixels[y * pixelWidth + x] =
            C15ToRgba(srcPix[y * pixelWidth + x]);
      }
    }
  }

  outImage.width = pixelWidth;
  outImage.height = ih;
  outImage.bpp = (pmode == 0) ? 4 : (pmode == 1 ? 8 : 16);
  std::printf("[TIM] Loaded %ux%u (%u-bit TIM)\n", outImage.width,
              outImage.height, outImage.bpp);
  return true;
}

} // namespace PSX
