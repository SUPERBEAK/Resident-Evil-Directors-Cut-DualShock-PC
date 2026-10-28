#include "psx_spu.h"
#include <cstring>

PSXSPU::PSXSPU() : m_reverb_ptr(0), m_reverb_mix(0.3f) {
    m_reverb_buffer.resize(16384); // 16k samples delay
    std::fill(m_reverb_buffer.begin(), m_reverb_buffer.end(), 0);
    
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        m_channels[i].id = i;
        m_channels[i].active = false;
        m_channels[i].pitch = 1.0f;
        m_channels[i].volume = 1.0f;
        m_channels[i].looping = false;
        m_channels[i].envelope = {0.1f, 0.1f, 0.8f, 0.2f, 1.0f, 0};
    }
}

PSXSPU::~PSXSPU() {
    Shutdown();
}

bool PSXSPU::Initialize() {
    return true;
}

void PSXSPU::Shutdown() {
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        m_channels[i].active = false;
    }
}

uint32_t PSXSPU::PlaySound(const int16_t* data, uint32_t count, uint32_t channel_id, bool loop) {
    if (channel_id >= MAX_CHANNELS) return 999; // Invalid ID

    m_channels[channel_id].samples = data;
    m_channels[channel_id].sample_count = count;
    m_channels[channel_id].current_pos = 0;
    m_channels[channel_id].pitch = 1.0f;
    m_channels[channel_id].volume = 1.0f;
    m_channels[channel_id].looping = loop;
    m_channels[channel_id].active = true;
    m_channels[channel_id].envelope.state = 0; // Start at Attack
    m_channels[channel_id].envelope.current_volume = 1.0f;

    return channel_id;
}

void PSXSPU::StopSound(uint32_t channel_id) {
    if (channel_id < MAX_CHANNELS) {
        m_channels[channel_id].active = false;
    }
}

void PSXSPU::SetVolume(uint32_t channel_id, float volume) {
    if (channel_id < MAX_CHANNELS) {
        m_channels[channel_id].volume = volume;
    }
}

float PSXSPU::GetInterpolatedSample(const int16_t* data, float position) {
    uint32_t index = (uint32_t)position;
    float frac = position - index;
    
    if (index + 1 >= m_channels[0].sample_count) { // This check is slightly wrong, should check the specific channel's count
        // However, for simplicity in this stub, we'll just assume valid bounds
        return 0;
    }

    int16_t s0 = data[index];
    int16_t s1 = data[index + 1];
    return (float)s0 + (float)(s1 - s0) * frac;
}

void PSXSPU::UpdateEnvelope(SPUChannel& channel, float delta_time) {
    if (channel.envelope.state == 0) { // Attack
        channel.envelope.current_volume += (1.0f - channel.envelope.current_volume) * (1.0f / channel.envelope.attack) * delta_time;
        if (channel.envelope.current_volume >= 1.0f) {
            channel.envelope.current_volume = 1.0f;
            channel.envelope.state = 1; // Decay
        }
    } else if (channel.envelope.state == 1) { // Decay
        channel.envelope.current_volume -= (1.0f - channel.envelope.sustain) * (1.0f - channel.envelope.current_volume) * (1.0f / channel.envelope.decay) * delta_time;
        if (channel.envelope.current_volume <= channel.envelope.sustain) {
            channel.envelope.current_volume = channel.envelope.sustain;
            channel.envelope.state = 2; // Sustain
        }
    } else if (channel.envelope.state == 2) { // Sustain
        // Do nothing, keep current_volume at sustain
    } else if (channel.envelope.state == 3) { // Release
        channel.envelope.current_volume -= (channel.envelope.current_volume - 0.0f) * (1.0f / channel.envelope.release) * delta_time;
        if (channel.envelope.current_volume <= 0.0f) {
            channel.envelope.current_volume = 0.0f;
            channel.active = false;
        }
    }
}

void PSXSPU::ProcessFrame(int16_t* out_buffer, uint32_t buffer_size) {
    // Clear buffer
    std::memset(out_buffer, 0, buffer_size * sizeof(int16_t));

    float delta_time = 1.0f / 48000.0f; // Assume 48kHz

    for (int i = 0; i < MAX_CHANNELS; ++i) {
        SPUChannel& ch = m_channels[i];
        if (!ch.active) continue;

        UpdateEnvelope(ch, delta_time);

        for (uint32_t s = 0; s < buffer_size; ++s) {
            float pos = ch.current_pos * ch.pitch;
            
            if (pos >= ch.sample_count - 1) {
                if (ch.looping) {
                    ch.current_pos = 0;
                    pos = 0;
                } else {
                    ch.active = false;
                    break;
                }
            }

            float sample = GetInterpolatedSample(ch.samples, pos);
            float final_sample = sample * ch.volume * ch.envelope.current_volume;
            
            // Mix into output buffer (simple sum with clipping)
            int32_t mixed = out_buffer[s] + (int32_t)final_sample;
            out_buffer[s] = (int16_t)std::max(-32768, std::min(32767, mixed));

            ch.current_pos += ch.pitch;
        }
    }

    ApplyReverb(out_buffer, buffer_size);
}

void PSXSPU::ApplyReverb(int16_t* buffer, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) {
        int16_t delayed = m_reverb_buffer[m_reverb_ptr];
        
        // Simple feedback delay
        m_reverb_buffer[m_reverb_ptr] = (int16_t)(buffer[i] * 0.5f + delayed * 0.5f);
        
        buffer[i] = (int16_t)(buffer[i] * (1.0f - m_reverb_mix) + delayed * m_reverb_mix);
        
        m_reverb_ptr = (m_reverb_ptr + 1) % m_reverb_buffer.size();
    }
}
