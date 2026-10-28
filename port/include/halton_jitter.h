#pragma once
#include <cstdint>

namespace Render {

/**
 * Computes radical inverse for Halton sequence.
 */
inline float HaltonSequence(uint32_t index, uint32_t base) {
    float result = 0.0f;
    float invBase = 1.0f / (float)base;
    float fraction = invBase;
    while (index > 0) {
        result += (float)(index % base) * fraction;
        index /= base;
        fraction *= invBase;
    }
    return result;
}

/**
 * Standard 8-phase Halton(2, 3) sub-pixel camera jitter generator
 * required by DLSS, FSR 2+, and modern temporal upscalers.
 * Returns subpixel offset in range [-0.5, +0.5] pixels.
 */
inline void GetHaltonJitter(uint32_t frameIndex, uint32_t phaseCount, float& outJitterX, float& outJitterY) {
    uint32_t index = (frameIndex % phaseCount) + 1;
    outJitterX = HaltonSequence(index, 2) - 0.5f;
    outJitterY = HaltonSequence(index, 3) - 0.5f;
}

} // namespace Render
