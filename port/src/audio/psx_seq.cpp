#include "psx_seq.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

static const int K0[5] = {0, 60, 115, 98, 122};
static const int K1[5] = {0, 0, -52, -55, -60};

PSXSequencer::PSXSequencer() : m_vabHdr{} {}

PSXSequencer::~PSXSequencer() { Clear(); }

void PSXSequencer::Clear() {
  m_programs.clear();
  m_tones.clear();
  m_vagSizes.clear();
  m_decodedVags.clear();
  m_tracks.clear();
  std::memset(&m_vabHdr, 0, sizeof(m_vabHdr));
}

void PSXSequencer::DecodeSpuAdpcm(const uint8_t *adpcm, size_t size,
                                  std::vector<int16_t> &outPcm,
                                  size_t &outLoopStart, size_t &outLoopEnd,
                                  bool &outIsLooping) {
  int32_t s_1 = 0;
  int32_t s_2 = 0;
  size_t numBlocks = size / 16;
  outPcm.reserve(numBlocks * 28);
  outLoopStart = SIZE_MAX;
  outLoopEnd = SIZE_MAX;
  outIsLooping = false;

  for (size_t b = 0; b < numBlocks; ++b) {
    const uint8_t *block = adpcm + b * 16;
    uint8_t flags = block[1];
    if (flags & 0x04) {
      outLoopStart = outPcm.size();
    }
    if (flags & 0x01) {
      outLoopEnd = outPcm.size() + 28;
      outIsLooping = true;
    }
    if (flags == 7) break;

    int shift = block[0] & 0x0f;
    int filter_idx = (block[0] >> 4) & 0x07;
    int k0 = (filter_idx < 5) ? K0[filter_idx] : 0;
    int k1 = (filter_idx < 5) ? K1[filter_idx] : 0;

    for (int i = 2; i < 16; ++i) {
      uint8_t byteVal = block[i];
      int nibbles[2] = {byteVal & 0x0f, (byteVal >> 4) & 0x0f};
      for (int nibble : nibbles) {
        int32_t sample = (nibble < 8) ? nibble : (nibble - 16);
        sample = sample << (12 - shift);
        int32_t pred = (s_1 * k0 + s_2 * k1) / 64;
        sample = sample + pred;
        sample = (std::max)(-32768, (std::min)(32767, sample));
        outPcm.push_back(static_cast<int16_t>(sample));
        s_2 = s_1;
        s_1 = sample;
      }
    }
  }
}

bool PSXSequencer::LoadHSB(const std::string &filePath) {
  std::wstring wpath(filePath.begin(), filePath.end());
  return LoadHSB(wpath);
}

bool PSXSequencer::LoadHSB(const std::wstring &filePath) {
  Clear();

  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    std::wcerr << L"[PSXSequencer] Failed to open HSB file: " << filePath << std::endl;
    return false;
  }

  file.seekg(0, std::ios::end);
  size_t fileSize = file.tellg();
  if (fileSize < 64) return false;

  std::vector<uint8_t> data(fileSize);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(data.data()), fileSize);

  uint32_t vabBodyOff = *reinterpret_cast<const uint32_t *>(&data[fileSize - 12]);
  uint32_t sepOff = *reinterpret_cast<const uint32_t *>(&data[fileSize - 8]);

  if (vabBodyOff >= fileSize || sepOff >= fileSize || sepOff >= vabBodyOff) {
    std::wcerr << L"[PSXSequencer] Invalid trailer offsets in: " << filePath << std::endl;
    return false;
  }

  if (std::memcmp(&data[0], "pBAV", 4) != 0 && std::memcmp(&data[0], "VABp", 4) != 0) {
    std::wcerr << L"[PSXSequencer] Invalid VAB header magic in: " << filePath << std::endl;
    return false;
  }
  std::memcpy(&m_vabHdr, data.data(), sizeof(PSXVabHdr));

  m_programs.resize(128);
  std::memcpy(m_programs.data(), &data[32], 128 * sizeof(PSXProgAtr));

  size_t vagTblOff = sepOff - 512;
  size_t toneBytes = vagTblOff - 2080;
  size_t numTones = toneBytes / sizeof(PSXVagAtr);
  m_tones.resize(numTones);
  std::memcpy(m_tones.data(), &data[2080], numTones * sizeof(PSXVagAtr));

  m_vagSizes.resize(256);
  std::memcpy(m_vagSizes.data(), &data[vagTblOff], 256 * sizeof(uint16_t));

  const uint8_t *bodyPtr = &data[vabBodyOff];
  size_t bodyRemaining = (fileSize >= 12 && fileSize - 12 >= vabBodyOff) ? (fileSize - 12 - vabBodyOff) : 0;
  size_t sampleOffset = 0;

  for (size_t v = 1; v < 256; ++v) {
    size_t vagByteSize = static_cast<size_t>(m_vagSizes[v]) * 8;
    if (vagByteSize > 0 && sampleOffset + vagByteSize <= bodyRemaining) {
      VagInfo info;
      DecodeSpuAdpcm(bodyPtr + sampleOffset, vagByteSize, info.pcm,
                     info.loopStart, info.loopEnd, info.isLooping);
      m_decodedVags[static_cast<uint16_t>(v)] = std::move(info);
      sampleOffset += vagByteSize;
    }
  }

  const uint8_t *sepPtr = &data[sepOff];
  size_t sepTotalSize = vabBodyOff - sepOff;

  if (sepTotalSize >= 19 &&
      (std::memcmp(sepPtr, "pQES", 4) == 0 || std::memcmp(sepPtr, "SEQp", 4) == 0)) {
    PSXSeqTrack track0;
    track0.seqNumber = 0;
    track0.resolution = (static_cast<uint16_t>(sepPtr[8]) << 8) | sepPtr[9];
    track0.tempoUsec = (static_cast<uint32_t>(sepPtr[10]) << 16) |
                       (static_cast<uint32_t>(sepPtr[11]) << 8) | sepPtr[12];
    track0.length = (static_cast<uint32_t>(sepPtr[15]) << 24) |
                    (static_cast<uint32_t>(sepPtr[16]) << 16) |
                    (static_cast<uint32_t>(sepPtr[17]) << 8) | sepPtr[18];

    size_t eventStart = 19;
    size_t eventLen = (std::min)(static_cast<size_t>(track0.length), sepTotalSize - eventStart);
    track0.eventData.assign(sepPtr + eventStart, sepPtr + eventStart + eventLen);
    m_tracks.push_back(std::move(track0));

    size_t nextPos = eventStart + eventLen;
    while (nextPos + 15 <= sepTotalSize) {
      if (sepPtr[nextPos] == 0xFF && sepPtr[nextPos + 1] == 0x2F) {
        nextPos += 3;
      }
      if (nextPos + 15 > sepTotalSize) break;

      PSXSeqTrack subTrack;
      subTrack.seqNumber = (static_cast<uint16_t>(sepPtr[nextPos]) << 8) | sepPtr[nextPos + 1];
      subTrack.resolution = (static_cast<uint16_t>(sepPtr[nextPos + 2]) << 8) | sepPtr[nextPos + 3];
      subTrack.tempoUsec = (static_cast<uint32_t>(sepPtr[nextPos + 4]) << 16) |
                           (static_cast<uint32_t>(sepPtr[nextPos + 5]) << 8) | sepPtr[nextPos + 6];
      subTrack.length = (static_cast<uint32_t>(sepPtr[nextPos + 9]) << 24) |
                        (static_cast<uint32_t>(sepPtr[nextPos + 10]) << 16) |
                        (static_cast<uint32_t>(sepPtr[nextPos + 11]) << 8) | sepPtr[nextPos + 12];

      size_t subEventStart = nextPos + 13;
      if (subEventStart >= sepTotalSize) break;

      size_t subEventLen = (std::min)(static_cast<size_t>(subTrack.length), sepTotalSize - subEventStart);
      subTrack.eventData.assign(sepPtr + subEventStart, sepPtr + subEventStart + subEventLen);
      m_tracks.push_back(std::move(subTrack));
      nextPos = subEventStart + subEventLen;
    }
  }

  std::wprintf(L"[PSXSequencer] Successfully loaded HSB: %ls (Active Progs: %u, VAGs: %zu, Tracks: %zu)\n",
               filePath.c_str(), m_vabHdr.ps, m_decodedVags.size(), m_tracks.size());
  return true;
}

static bool RenderTracksInternal(const std::vector<PSXSeqTrack> &allTracks,
                                 const std::vector<uint32_t> &trackIndices,
                                 const std::vector<PSXVagAtr> &tones,
                                 const std::unordered_map<uint16_t, VagInfo> &decodedVags,
                                 std::vector<int16_t> &outStereoPcm,
                                 uint32_t maxSeconds) {
  const double sampleRate = 44100.0;
  size_t maxSamples = static_cast<size_t>(maxSeconds * sampleRate);
  std::vector<int32_t> mixLeft(maxSamples, 0);
  std::vector<int32_t> mixRight(maxSamples, 0);

  struct NoteInstance {
    size_t startSample = 0;
    size_t releaseStartSample = SIZE_MAX;
    size_t endSample = SIZE_MAX;
    double pitch = 1.0;
    double gainL = 0.0;
    double gainR = 0.0;
    const VagInfo *vagInfo = nullptr;
  };

  std::vector<NoteInstance> activeNotes;
  const size_t releaseDuration = static_cast<size_t>(0.45 * sampleRate); // 450ms smooth release

  for (uint32_t tIdx : trackIndices) {
    if (tIdx >= allTracks.size()) continue;
    const auto &track = allTracks[tIdx];
    if (track.eventData.empty()) continue;

    uint32_t resolution = (track.resolution > 0) ? track.resolution : 480;
    uint32_t tempoUsec = (track.tempoUsec > 0) ? track.tempoUsec : 500000;

    struct ChannelState {
      uint8_t program = 0;
      uint8_t volume = 100;
      uint8_t pan = 64;
      uint8_t expression = 127;
    };
    ChannelState channels[16];
    std::vector<size_t> activeKeyNotes[16][128];

    size_t pos = 0;
    const size_t end = track.eventData.size();
    const uint8_t *s = track.eventData.data();

    uint8_t runningStatus = 0;
    size_t currentSample = 0;

    while (pos < end && currentSample < maxSamples) {
      uint32_t delta = 0;
      while (pos < end) {
        uint8_t b = s[pos++];
        delta = (delta << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
      }

      double deltaSeconds = (static_cast<double>(delta) * (static_cast<double>(tempoUsec) / static_cast<double>(resolution))) / 1000000.0;
      currentSample += static_cast<size_t>(deltaSeconds * sampleRate);
      if (currentSample >= maxSamples) break;
      if (pos >= end) break;

      uint8_t status = s[pos];
      if (status & 0x80) {
        pos++;
        if (status < 0xF0) runningStatus = status;
      } else {
        status = runningStatus;
      }

      uint8_t cmd = status & 0xF0;
      uint8_t ch = status & 0x0F;

      if (status == 0xFF) {
        if (pos >= end) break;
        uint8_t metaType = s[pos++];
        if (metaType == 0x51) {
          if (pos + 3 <= end) {
            tempoUsec = (static_cast<uint32_t>(s[pos]) << 16) |
                        (static_cast<uint32_t>(s[pos + 1]) << 8) | s[pos + 2];
            pos += 3;
          }
        } else if (metaType == 0x2F) {
          if (pos < end) pos++;
          break;
        } else {
          if (pos < end) {
            uint8_t mlen = s[pos++];
            pos += mlen;
          }
        }
      } else if (cmd == 0x90) {
        if (pos + 2 <= end) {
          uint8_t note = s[pos++];
          uint8_t vel = s[pos++];
          if (note < 128) {
            for (size_t idx : activeKeyNotes[ch][note]) {
              auto &prev = activeNotes[idx];
              if (prev.releaseStartSample == SIZE_MAX) {
                prev.releaseStartSample = currentSample;
              }
            }
            activeKeyNotes[ch][note].clear();

            if (vel > 0) {
              uint8_t progId = channels[ch].program;
              size_t progToneBase = static_cast<size_t>(progId) * 16;
              for (size_t t = 0; t < 16 && (progToneBase + t) < tones.size(); ++t) {
                const auto &tone = tones[progToneBase + t];
                if (tone.vag > 0 && note >= tone.min_key && note <= tone.max_key) {
                  auto it = decodedVags.find(static_cast<uint16_t>(tone.vag));
                  if (it != decodedVags.end() && !it->second.pcm.empty()) {
                    const auto &vInfo = it->second;
                    double centerNote = static_cast<double>(tone.center) + static_cast<double>(tone.shift) / 128.0;
                    double pitch = std::pow(2.0, (static_cast<double>(note) - centerNote) / 12.0);
                    double gain = (static_cast<double>(vel) / 127.0) *
                                  (static_cast<double>(tone.vol) / 127.0) *
                                  (static_cast<double>(channels[ch].volume) / 127.0) *
                                  (static_cast<double>(channels[ch].expression) / 127.0) * 0.45;

                    double panNorm = static_cast<double>(channels[ch].pan) / 127.0;
                    double gainL = gain * std::cos(panNorm * 1.5707963);
                    double gainR = gain * std::sin(panNorm * 1.5707963);

                    NoteInstance inst;
                    inst.startSample = currentSample;
                    inst.releaseStartSample = SIZE_MAX;
                    inst.endSample = maxSamples;
                    inst.pitch = pitch;
                    inst.gainL = gainL;
                    inst.gainR = gainR;
                    inst.vagInfo = &vInfo;

                    size_t newIdx = activeNotes.size();
                    activeNotes.push_back(inst);
                    activeKeyNotes[ch][note].push_back(newIdx);
                  }
                }
              }
            }
          }
        }
      } else if (cmd == 0x80) {
        if (pos + 2 <= end) {
          uint8_t note = s[pos++];
          uint8_t vel = s[pos++];
          if (note < 128) {
            for (size_t idx : activeKeyNotes[ch][note]) {
              auto &prev = activeNotes[idx];
              if (prev.releaseStartSample == SIZE_MAX) {
                prev.releaseStartSample = currentSample;
              }
            }
            activeKeyNotes[ch][note].clear();
          }
        }
      } else if (cmd == 0xB0) {
        if (pos + 2 <= end) {
          uint8_t cc = s[pos++];
          uint8_t val = s[pos++];
          if (cc == 7) channels[ch].volume = val;
          else if (cc == 10) channels[ch].pan = val;
          else if (cc == 11) channels[ch].expression = val;
        }
      } else if (cmd == 0xC0) {
        if (pos < end) channels[ch].program = s[pos++];
      } else if (cmd == 0xE0) {
        if (pos + 2 <= end) pos += 2;
      } else {
        pos++;
      }
    }
  }

  size_t lastWrittenSample = 0;

  // Synthesize and mix all collected note instances with sample looping and linear interpolation
  for (const auto &note : activeNotes) {
    if (!note.vagInfo || note.startSample >= maxSamples) continue;

    const auto &wave = note.vagInfo->pcm;
    const size_t waveLen = wave.size();
    if (waveLen < 2) continue;

    bool isLooping = note.vagInfo->isLooping &&
                     note.vagInfo->loopEnd > note.vagInfo->loopStart &&
                     note.vagInfo->loopStart != SIZE_MAX;
    size_t lStart = note.vagInfo->loopStart;
    size_t lEnd = note.vagInfo->loopEnd;
    double lLen = isLooping ? static_cast<double>(lEnd - lStart) : 0.0;

    size_t noteEnd = (std::min)(note.endSample, maxSamples);
    if (note.releaseStartSample != SIZE_MAX) {
      size_t relEnd = note.releaseStartSample + releaseDuration;
      if (relEnd < noteEnd) noteEnd = relEnd;
    }
    if (!isLooping) {
      size_t rawDur = static_cast<size_t>(std::ceil(static_cast<double>(waveLen) / note.pitch));
      if (note.startSample + rawDur < noteEnd) {
        noteEnd = note.startSample + rawDur;
      }
    }

    for (size_t outIdx = note.startSample; outIdx < noteEnd; ++outIdx) {
      double wavePos = static_cast<double>(outIdx - note.startSample) * note.pitch;

      if (isLooping) {
        if (wavePos >= lEnd) {
          wavePos = lStart + std::fmod(wavePos - lStart, lLen);
        }
      } else if (wavePos >= waveLen - 1) {
        break;
      }

      size_t idx0 = static_cast<size_t>(wavePos);
      if (idx0 >= waveLen) break;
      size_t idx1 = (idx0 + 1 < waveLen) ? (idx0 + 1) : idx0;
      double frac = wavePos - static_cast<double>(idx0);
      double val = (1.0 - frac) * wave[idx0] + frac * wave[idx1];

      double sampleGain = 1.0;
      if (outIdx >= note.releaseStartSample) {
        double relFrac = static_cast<double>(outIdx - note.releaseStartSample) / static_cast<double>(releaseDuration);
        double decay = (std::max)(0.0, 1.0 - relFrac);
        sampleGain = decay * decay;
      }

      mixLeft[outIdx] += static_cast<int32_t>(val * note.gainL * sampleGain);
      mixRight[outIdx] += static_cast<int32_t>(val * note.gainR * sampleGain);
    }
    if (noteEnd > lastWrittenSample) {
      lastWrittenSample = noteEnd;
    }
  }

  size_t finalLength = (std::max)(lastWrittenSample, static_cast<size_t>(sampleRate));
  finalLength = (std::min)(finalLength, maxSamples);

  // Peak check for soft limiting
  int32_t maxAmp = 0;
  for (size_t i = 0; i < finalLength; ++i) {
    if (std::abs(mixLeft[i]) > maxAmp) maxAmp = std::abs(mixLeft[i]);
    if (std::abs(mixRight[i]) > maxAmp) maxAmp = std::abs(mixRight[i]);
  }

  double scale = (maxAmp > 30000) ? (30000.0 / static_cast<double>(maxAmp)) : 1.0;

  outStereoPcm.clear();
  outStereoPcm.resize(finalLength * 2);

  for (size_t i = 0; i < finalLength; ++i) {
    int32_t l = static_cast<int32_t>(mixLeft[i] * scale);
    int32_t r = static_cast<int32_t>(mixRight[i] * scale);
    l = (std::max)(-32768, (std::min)(32767, l));
    r = (std::max)(-32768, (std::min)(32767, r));
    outStereoPcm[i * 2 + 0] = static_cast<int16_t>(l);
    outStereoPcm[i * 2 + 1] = static_cast<int16_t>(r);
  }

  std::printf("[PSXSequencer] Synthesized %zu tracks: %zu frames (%.2f seconds, stereo 44.1 kHz, %zu notes)\n",
              trackIndices.size(), finalLength, static_cast<double>(finalLength) / sampleRate, activeNotes.size());
  return true;
}

bool PSXSequencer::RenderTrackToPCM(uint32_t trackIndex,
                                    std::vector<int16_t> &outStereoPcm,
                                    uint32_t maxSeconds) {
  if (trackIndex >= m_tracks.size()) {
    std::cerr << "[PSXSequencer] Track index out of range: " << trackIndex << std::endl;
    return false;
  }
  return RenderTracksInternal(m_tracks, {trackIndex}, m_tones, m_decodedVags, outStereoPcm, maxSeconds);
}

bool PSXSequencer::RenderFullSequenceToPCM(std::vector<int16_t> &outStereoPcm,
                                           uint32_t maxSeconds) {
  if (m_tracks.empty()) {
    std::cerr << "[PSXSequencer] No tracks loaded to render!" << std::endl;
    return false;
  }
  std::vector<uint32_t> allTracks;
  allTracks.reserve(m_tracks.size());
  for (uint32_t i = 0; i < static_cast<uint32_t>(m_tracks.size()); ++i) {
    allTracks.push_back(i);
  }
  return RenderTracksInternal(m_tracks, allTracks, m_tones, m_decodedVags, outStereoPcm, maxSeconds);
}
