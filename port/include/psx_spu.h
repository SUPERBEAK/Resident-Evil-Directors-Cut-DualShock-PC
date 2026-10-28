#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <stdint.h>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * PS1 SPU (Sound Processing Unit) Emulator.
 * Handles multi-channel mixing, ADSR envelopes, and basic reverb.
 */

struct ADSR {
    float attack;    // seconds
    float decay;     // seconds
    float sustain;   // volume (0.0 - 1.0)
    float release;   // seconds
    
    float current_volume;
    float state;     // 0: Attack, 1: Decay, 2: Sustain, 3: Release
};

struct SPUChannel {
    uint32_t id;
    const int16_t* samples;
    uint32_t sample_count;
    uint32_t current_pos;
    
    float pitch;       // 1.0 = normal, 2.0 = octave up, etc.
    float volume;      // 0.0 to 1.0
    
    ADSR envelope;
    bool active;
    bool looping;
};

class PSXSPU {
public:
    PSXSPU();
    ~PSXSPU();

    bool Initialize();
    void Shutdown();

    // Channel Management
    uint32_t PlaySound(const int16_t* data, uint32_t count, uint32_t channel_id, bool loop = false);
    void StopSound(uint32_t channel_id);
    void SetVolume(uint32_t channel_id, float volume);

    // Core Engine
    /**
     * Processes the SPU for one audio frame.
     * @param out_buffer Output buffer for mixed PCM data
     * @param buffer_size Number of samples in the buffer
     */
    void ProcessFrame(int16_t* out_buffer, uint32_t buffer_size);

    /**
     * Simple Reverb/Echo effect on the master mix.
     */
    void ApplyReverb(int16_t* buffer, uint32_t size);

private:
    static const int MAX_CHANNELS = 32;
    SPUChannel m_channels[MAX_CHANNELS];
    
    // Reverb buffer (Simple delay line)
    std::vector<int16_t> m_reverb_buffer;
    uint32_t m_reverb_ptr;
    float m_reverb_mix;

    void UpdateEnvelope(SPUChannel& channel, float delta_time);
    float GetInterpolatedSample(const int16_t* data, float position);
};

#endif // PSX_SPU_H
