#include "xas_decoder.h"
#include "audio_engine.h"
#include "psx_seq.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

AudioEngine g_AudioEngine;

static const int K0[5] = {0, 60, 115, 98, 122};
static const int K1[5] = {0, 0, -52, -55, -60};

static const uint8_t g_StageBgmTables[8][32] = {
    {0xFF},
    {11,  255, 255, 0,   0, 0, 0,   0, 0,   255, 255, 0, 30,  28,  0,   13,
     255, 255, 255, 255, 2, 3, 255, 5, 255, 2,   2,   2, 255, 255, 255, 255},
    {55, 7, 7, 9, 7, 31,  255, 255, 7,   7,   17,  4,   12,  16,  20,  255,
     6,  8, 8, 7, 7, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {36,  36,  36,  37,  36,  36,  12,  38,  32,  33,  255,
     34,  35,  33,  15,  33,  55,  255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {43,  12,  12,  15,  43,  45,  12,  12,  43,  31,  12,
     12,  39,  41,  41,  12,  41,  41,  255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {54, 53, 54, 54, 54, 47, 47,  47,  47,  47,  47,  48,  47,  53,  15,  49,
     49, 49, 48, 52, 50, 53, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {15,  19,  255, 24,  24, 24, 24,  24, 24,  2, 2, 24, 30,  28,  24,  255,
     255, 255, 255, 255, 2,  3,  255, 5,  255, 2, 2, 2,  255, 255, 255, 255},
    {55, 255, 255, 255, 255, 31,  19, 255, 255, 255, 255,
     4,  12,  255, 255, 255, 255, 8,  8,   255, 255, 255,
     12, 12,  29,  255, 26,  26,  26, 255, 255, 255},
};

static void DecodeSpuAdpcm(const uint8_t *adpcm, size_t size,
                           std::vector<int16_t> &outPcm) {
  int32_t s_1 = 0;
  int32_t s_2 = 0;
  size_t numBlocks = size / 16;
  outPcm.reserve(outPcm.size() + numBlocks * 28);

  for (size_t b = 0; b < numBlocks; ++b) {
    const uint8_t *block = adpcm + b * 16;
    uint8_t flags = block[1];
    if (flags == 7)
      break;

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

AudioEngine::AudioEngine()
    : m_pXAudio2(nullptr), m_pMasterVoice(nullptr), m_pSfxVoice(nullptr),
      m_pGunVoice(nullptr), m_pZombieVoice(nullptr), m_pBgmVoice(nullptr),
      m_pSourceVoice(nullptr), m_writePos(0), m_readPos(0), m_wFormat{} {
  m_buffer.resize(BUFFER_SIZE);
}

AudioEngine::~AudioEngine() { Shutdown(); }

void AudioEngine::LoadSampleFromVb(const std::wstring &vbPath, size_t startByte,
                                   size_t endByte,
                                   std::vector<int16_t> &outPcm) {
  std::ifstream file(vbPath, std::ios::binary);
  if (!file.is_open())
    return;

  file.seekg(startByte, std::ios::beg);
  size_t readSize = endByte - startByte;
  std::vector<uint8_t> buffer(readSize);
  file.read(reinterpret_cast<char *>(buffer.data()), readSize);

  DecodeSpuAdpcm(buffer.data(), buffer.size(), outPcm);
}

bool AudioEngine::LoadRoomAudio(const std::wstring &rdtPath) {
  std::ifstream file(rdtPath, std::ios::binary);
  if (!file.is_open())
    return false;

  file.seekg(0, std::ios::end);
  size_t fileSize = file.tellg();
  if (fileSize < 0x94)
    return false;

  std::vector<uint8_t> rdt(fileSize);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(rdt.data()), fileSize);

  uint32_t flrOff = *reinterpret_cast<const uint32_t *>(&rdt[0x5C]);
  uint32_t sndOff = *reinterpret_cast<const uint32_t *>(&rdt[0x88]);
  uint32_t vhOff = *reinterpret_cast<const uint32_t *>(&rdt[0x8C]);
  uint32_t vbOff = *reinterpret_cast<const uint32_t *>(&rdt[0x90]);

  if (vhOff == 0 || vbOff == 0 || vhOff >= fileSize || vbOff >= fileSize ||
      vhOff >= vbOff)
    return false;

  if (std::memcmp(&rdt[vhOff], "pBAV", 4) != 0 &&
      std::memcmp(&rdt[vhOff], "VABp", 4) != 0)
    return false;

  if (vbOff < vhOff + 512)
    return false;

  // Stop active footstep voice before replacing room VAG buffers
  if (m_pFootstepVoice) {
    m_pFootstepVoice->Stop(0);
    m_pFootstepVoice->FlushSourceBuffers();
  }

  // Parse default floor zone from RDT +0x5C (.FLR)
  m_DefaultFloorZone = 45;
  if (flrOff > 0 && flrOff + 12 <= fileSize) {
    uint16_t zoneCount = *reinterpret_cast<const uint16_t *>(&rdt[flrOff]);
    if (zoneCount > 0 && flrOff + 2 + static_cast<size_t>(zoneCount) * 10 <= fileSize) {
      m_DefaultFloorZone = rdt[flrOff + 2 + (zoneCount - 1) * 10 + 8];
    }
  }

  // In PsyQ VAB, the VAG size table (uint16_t vag_size[256]) is always the last
  // 512 bytes of the VAB header (vbOff - 512)
  size_t vagTblOff = vbOff - 512;
  const uint16_t *vagTbl = reinterpret_cast<const uint16_t *>(&rdt[vagTblOff]);

  // Decode all room VAG samples (1..255) from vab_body
  m_RoomVagPcm.assign(256, std::vector<int16_t>());
  size_t curVbOff = vbOff;
  size_t decodedVagCount = 0;
  for (size_t v = 1; v < 256; ++v) {
    size_t vSize = static_cast<size_t>(vagTbl[v]) * 8;
    if (vSize == 0)
      continue;
    if (curVbOff + vSize > fileSize)
      break;
    DecodeSpuAdpcm(&rdt[curVbOff], vSize, m_RoomVagPcm[v]);
    curVbOff += vSize;
    decodedVagCount++;
  }

  // Parse all 48 ROOM->sound_attributes entries (+0x88, .SND) and VagAtr tone records (+0x820)
  std::memset(m_RoomSoundTable, 0, sizeof(m_RoomSoundTable));
  if (sndOff > 0 && sndOff + 48 * 4 <= fileSize) {
    for (size_t se = 0; se < 48; ++se) {
      int8_t prog = static_cast<int8_t>(rdt[sndOff + se * 4 + 1]);
      int8_t tone = static_cast<int8_t>(rdt[sndOff + se * 4 + 2]);
      if (prog < 0 || tone < 0)
        continue;
      size_t atrOff = vhOff + 0x820 + (static_cast<size_t>(prog) << 9) +
                      (static_cast<size_t>(tone) << 5);
      if (atrOff + 32 > vbOff)
        continue;
      const uint8_t *data = &rdt[atrOff];
      RoomSoundEntry &entry = m_RoomSoundTable[se];
      entry.vol = data[2];
      entry.pan = data[3];
      entry.center = data[4];
      entry.shift = data[5];
      entry.note = data[6]; // Passed as note to SsUtKeyOn(vab, prog, tone, data[6], data[5], ...)
      entry.vagId = *reinterpret_cast<const uint16_t *>(&data[0x16]);
      // Authentic PsyQ SsUtKeyOn / SsPitchFromNote pitch multiplier relative to 44.1 kHz
      float semitoneDiff = static_cast<float>(static_cast<int>(entry.note) -
                                              static_cast<int>(entry.center));
      entry.freqRatio44k = std::pow(2.0f, semitoneDiff / 12.0f);
    }
  }

  // Keep legacy m_Footstep1Pcm / m_Footstep2Pcm populated as fallback
  m_Footstep1Pcm.clear();
  m_Footstep2Pcm.clear();
  uint8_t walkSe = (m_DefaultFloorZone < 48) ? m_DefaultFloorZone : 45;
  uint16_t walkVag = m_RoomSoundTable[walkSe].vagId;
  if (walkVag > 0 && walkVag < 256 && !m_RoomVagPcm[walkVag].empty()) {
    m_Footstep1Pcm = m_RoomVagPcm[walkVag];
    m_Footstep2Pcm = m_RoomVagPcm[walkVag];
  } else if (!m_RoomVagPcm[1].empty()) {
    m_Footstep1Pcm = m_RoomVagPcm[1];
    m_Footstep2Pcm = m_RoomVagPcm[1];
  }

  m_ZombieGroanPcm.clear();
  m_ZombieBitePcm.clear();
  m_ZombieHitPcm.clear();
  m_ZombieDeathPcm.clear();

  // Only load zombie audio in rooms with zombies (Stage 1 Room 0 - Mansion
  // Dining Room) In other rooms (like Main Hall Room 1), keep buffers empty so
  // no zombie sounds can play!
  if (rdtPath.find(L"ROOM100") != std::wstring::npos) {
    if (!m_RoomVagPcm[4].empty()) m_ZombieGroanPcm = m_RoomVagPcm[4];
    if (!m_RoomVagPcm[6].empty()) m_ZombieBitePcm = m_RoomVagPcm[6];
    if (!m_RoomVagPcm[7].empty()) m_ZombieHitPcm = m_RoomVagPcm[7];
    if (!m_RoomVagPcm[8].empty()) m_ZombieDeathPcm = m_RoomVagPcm[8];
  }

  const auto &walkEntry = m_RoomSoundTable[walkSe];
  const auto &runEntry = m_RoomSoundTable[(walkSe + 1 < 48) ? (walkSe + 1) : walkSe];
  std::printf("[Audio] Loaded room VAB (%zu VAGs, default floor SE=%u -> Walk VAG=%u @ %.0f Hz, Run VAG=%u @ %.0f Hz) from %ls\n",
              decodedVagCount, walkSe,
              walkEntry.vagId, 44100.0f * walkEntry.freqRatio44k,
              runEntry.vagId, 44100.0f * runEntry.freqRatio44k,
              rdtPath.c_str());
  return true;
}

bool AudioEngine::LoadWeaponAudio(const std::wstring &hedPath,
                                  const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open())
    return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16)
    return false;

  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);

  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize)
    return false;

  const uint16_t *vagTbl =
      reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);
  size_t vag1Size = static_cast<size_t>(vagTbl[1]) * 8; // Gunshot (13392)
  size_t vag2Size = static_cast<size_t>(vagTbl[2]) * 8; // Slide click (3376)
  size_t vag3Size = static_cast<size_t>(vagTbl[3]) * 8; // Reload 1 (5280)
  size_t vag4Size = static_cast<size_t>(vagTbl[4]) * 8; // Reload 2 (7888)

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open())
    return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  m_GunshotPcm.clear();
  m_DryFirePcm.clear();
  m_ReloadPcm.clear();

  if (vag1Size > 0 && vag1Size <= vbSize) {
    DecodeSpuAdpcm(vb.data(), vag1Size, m_GunshotPcm);
  }
  if (vag2Size > 0 && vag1Size + vag2Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + vag1Size, vag2Size, m_DryFirePcm);
  }
  size_t reloadOffset = vag1Size + vag2Size;
  size_t reloadSize = vag3Size + vag4Size;
  if (reloadSize > 0 && reloadOffset + reloadSize <= vbSize) {
    DecodeSpuAdpcm(vb.data() + reloadOffset, reloadSize, m_ReloadPcm);
  }

  std::printf("[Audio] Loaded authentic PS1 weapon audio: Gunshot: %zu "
              "samples, DryFire: %zu samples, Reload: %zu samples\n",
              m_GunshotPcm.size(), m_DryFirePcm.size(), m_ReloadPcm.size());
  return true;
}

bool AudioEngine::LoadZombieAudio(const std::wstring &hedPath,
                                  const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open())
    return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16)
    return false;

  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);

  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize)
    return false;

  const uint16_t *vagTbl =
      reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open())
    return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
  size_t groanSize = static_cast<size_t>(vagTbl[2]) * 8;
  size_t groan2Size = static_cast<size_t>(vagTbl[3]) * 8;
  size_t deathSize = static_cast<size_t>(vagTbl[4]) * 8;
  size_t biteSize = static_cast<size_t>(vagTbl[5]) * 8;
  size_t hitSize = static_cast<size_t>(vagTbl[7]) * 8;

  size_t groanOffset = tone1Size;
  size_t groan2Offset = groanOffset + groanSize;
  size_t deathOffset = groan2Offset + groan2Size;
  size_t biteOffset = deathOffset + deathSize;
  size_t hitOffset = biteOffset + biteSize + static_cast<size_t>(vagTbl[6]) * 8;

  m_ZombieGroanPcm.clear();
  m_ZombieBitePcm.clear();
  m_ZombieHitPcm.clear();
  m_ZombieDeathPcm.clear();

  if (groanSize > 0 && groanOffset + groanSize <= vbSize) {
    DecodeSpuAdpcm(vb.data() + groanOffset, groanSize, m_ZombieGroanPcm);
  }
  if (biteSize > 0 && biteOffset + biteSize <= vbSize) {
    DecodeSpuAdpcm(vb.data() + biteOffset, biteSize, m_ZombieBitePcm);
  }
  if (hitSize > 0 && hitOffset + hitSize <= vbSize) {
    DecodeSpuAdpcm(vb.data() + hitOffset, hitSize, m_ZombieHitPcm);
  }
  if (deathSize > 0 && deathOffset + deathSize <= vbSize) {
    DecodeSpuAdpcm(vb.data() + deathOffset, deathSize, m_ZombieDeathPcm);
  }

  std::printf(
      "[Audio] Loaded authentic PS1 zombie audio from %ls: Groan: %zu samples, "
      "Bite: %zu samples, Hit: %zu samples, Death: %zu samples\n",
      hedPath.c_str(), m_ZombieGroanPcm.size(), m_ZombieBitePcm.size(),
      m_ZombieHitPcm.size(), m_ZombieDeathPcm.size());
  return true;
}

bool AudioEngine::Initialize() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  HRESULT hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create XAudio2 instance (0x%08X)\n",
                 hr);
    return false;
  }

  hr = m_pXAudio2->CreateMasteringVoice(&m_pMasterVoice);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create Mastering Voice (0x%08X)\n",
                 hr);
    return false;
  }

  WAVEFORMATEX wfxFootstep = {};
  wfxFootstep.wFormatTag = WAVE_FORMAT_PCM;
  wfxFootstep.nChannels = 1;
  wfxFootstep.nSamplesPerSec = 44100;
  wfxFootstep.wBitsPerSample = 16;
  wfxFootstep.nBlockAlign = (wfxFootstep.nChannels * wfxFootstep.wBitsPerSample) / 8;
  wfxFootstep.nAvgBytesPerSec = wfxFootstep.nSamplesPerSec * wfxFootstep.nBlockAlign;

  hr = m_pXAudio2->CreateSourceVoice(&m_pFootstepVoice, &wfxFootstep, 0, 4.0f);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create Footstep source voice (0x%08X)\n",
                 hr);
  }

  WAVEFORMATEX wfxMono = {};
  wfxMono.wFormatTag = WAVE_FORMAT_PCM;
  wfxMono.nChannels = 1;
  wfxMono.nSamplesPerSec = 22050;
  wfxMono.wBitsPerSample = 16;
  wfxMono.nBlockAlign = (wfxMono.nChannels * wfxMono.wBitsPerSample) / 8;
  wfxMono.nAvgBytesPerSec = wfxMono.nSamplesPerSec * wfxMono.nBlockAlign;

  hr = m_pXAudio2->CreateSourceVoice(&m_pSfxVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create SFX source voice (0x%08X)\n",
                 hr);
  }

  hr = m_pXAudio2->CreateSourceVoice(&m_pGunVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "[Audio] Failed to create Gun SFX source voice (0x%08X)\n", hr);
  }

  hr = m_pXAudio2->CreateSourceVoice(&m_pZombieVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "[Audio] Failed to create Zombie SFX source voice (0x%08X)\n",
                 hr);
  }

  WAVEFORMATEX wfxStereo = {};
  wfxStereo.wFormatTag = WAVE_FORMAT_PCM;
  wfxStereo.nChannels = 2;
  wfxStereo.nSamplesPerSec = 44100;
  wfxStereo.wBitsPerSample = 16;
  wfxStereo.nBlockAlign = (wfxStereo.nChannels * wfxStereo.wBitsPerSample) / 8;
  wfxStereo.nAvgBytesPerSec = wfxStereo.nSamplesPerSec * wfxStereo.nBlockAlign;

  hr = m_pXAudio2->CreateSourceVoice(&m_pBgmVoice, &wfxStereo);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create BGM source voice (0x%08X)\n",
                 hr);
  }

  WAVEFORMATEX wfxVoice = {};
  wfxVoice.wFormatTag = WAVE_FORMAT_PCM;
  wfxVoice.nChannels = 1;
  wfxVoice.nSamplesPerSec = 37800;
  wfxVoice.wBitsPerSample = 16;
  wfxVoice.nBlockAlign = (wfxVoice.nChannels * wfxVoice.wBitsPerSample) / 8;
  wfxVoice.nAvgBytesPerSec = wfxVoice.nSamplesPerSec * wfxVoice.nBlockAlign;
  hr = m_pXAudio2->CreateSourceVoice(&m_pVoiceLineVoice, &wfxVoice);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create Voice Line source voice (0x%08X)\n", hr);
  }

  std::memset(&m_wFormat, 0, sizeof(m_wFormat));
  m_wFormat.wFormatTag = WAVE_FORMAT_PCM;
  m_wFormat.nChannels = 2;
  m_wFormat.nSamplesPerSec = 48000;
  m_wFormat.wBitsPerSample = 16;
  m_wFormat.nBlockAlign = (m_wFormat.wBitsPerSample / 8) * m_wFormat.nChannels;
  m_wFormat.nAvgBytesPerSec = m_wFormat.nSamplesPerSec * m_wFormat.nBlockAlign;
  m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &m_wFormat, 0, 0, nullptr,
                                nullptr, nullptr);

  m_IsInitialized = true;
  return true;
}

void AudioEngine::PlayRoomSoundAttr(uint8_t seIndex, float volumeScale) {
  if (!m_pFootstepVoice || seIndex >= 48)
    return;

  const RoomSoundEntry &entry = m_RoomSoundTable[seIndex];
  if (entry.vagId == 0 || entry.vagId >= m_RoomVagPcm.size())
    return;

  const auto &pcm = m_RoomVagPcm[entry.vagId];
  if (pcm.empty())
    return;

  float finalVol = (static_cast<float>(entry.vol) / 127.0f) * volumeScale;
  float freqRatio = std::clamp(entry.freqRatio44k, 0.05f, 4.0f);

  m_pFootstepVoice->Stop(0);
  m_pFootstepVoice->FlushSourceBuffers();
  m_pFootstepVoice->SetFrequencyRatio(freqRatio);
  m_pFootstepVoice->SetVolume(finalVol);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(pcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(pcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pFootstepVoice->SubmitSourceBuffer(&buf);
  m_pFootstepVoice->Start(0);
}

void AudioEngine::PlayFootstep(uint8_t stepType, uint8_t zoneBase, float volume) {
  if (stepType >= 3)
    return;
  uint8_t se = zoneBase + stepType;
  if (se < 48 && m_RoomSoundTable[se].vagId > 0) {
    PlayRoomSoundAttr(se, volume);
    return;
  }
  // Fallback to room default floor zone if specific zone entry is empty
  uint8_t fallbackSe = m_DefaultFloorZone + stepType;
  if (fallbackSe < 48 && m_RoomSoundTable[fallbackSe].vagId > 0) {
    PlayRoomSoundAttr(fallbackSe, volume);
  }
}

void AudioEngine::PlayFootstep(float volume) {
  PlayFootstep(0, m_DefaultFloorZone, volume);
}

void AudioEngine::PlayGunshot(float volume) {
  if (!m_pGunVoice || m_GunshotPcm.empty())
    return;

  m_pGunVoice->Stop();
  m_pGunVoice->FlushSourceBuffers();
  m_pGunVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_GunshotPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_GunshotPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pGunVoice->SubmitSourceBuffer(&buf);
  m_pGunVoice->Start();
}

void AudioEngine::PlayDryFire(float volume) {
  if (!m_pGunVoice || m_DryFirePcm.empty())
    return;

  m_pGunVoice->Stop();
  m_pGunVoice->FlushSourceBuffers();
  m_pGunVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_DryFirePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_DryFirePcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pGunVoice->SubmitSourceBuffer(&buf);
  m_pGunVoice->Start();
}

void AudioEngine::PlayReload(float volume) {
  if (!m_pGunVoice || m_ReloadPcm.empty())
    return;

  m_pGunVoice->Stop();
  m_pGunVoice->FlushSourceBuffers();
  m_pGunVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_ReloadPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ReloadPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pGunVoice->SubmitSourceBuffer(&buf);
  m_pGunVoice->Start();
}

void AudioEngine::PlayZombieGroan(float volume) {
  if (!m_pZombieVoice || m_ZombieGroanPcm.empty())
    return;

  m_pZombieVoice->Stop();
  m_pZombieVoice->FlushSourceBuffers();
  m_pZombieVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes =
      static_cast<UINT32>(m_ZombieGroanPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ZombieGroanPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pZombieVoice->SubmitSourceBuffer(&buf);
  m_pZombieVoice->Start();
}

void AudioEngine::PlayZombieHit(float volume) {
  if (!m_pZombieVoice || m_ZombieHitPcm.empty())
    return;

  m_pZombieVoice->Stop();
  m_pZombieVoice->FlushSourceBuffers();
  m_pZombieVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_ZombieHitPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ZombieHitPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pZombieVoice->SubmitSourceBuffer(&buf);
  m_pZombieVoice->Start();
}

void AudioEngine::PlayZombieBite(float volume) {
  if (!m_pZombieVoice || m_ZombieBitePcm.empty())
    return;

  m_pZombieVoice->Stop();
  m_pZombieVoice->FlushSourceBuffers();
  m_pZombieVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes =
      static_cast<UINT32>(m_ZombieBitePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ZombieBitePcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pZombieVoice->SubmitSourceBuffer(&buf);
  m_pZombieVoice->Start();
}

void AudioEngine::PlayZombieDeath(float volume) {
  if (!m_pZombieVoice || m_ZombieDeathPcm.empty())
    return;

  m_pZombieVoice->Stop();
  m_pZombieVoice->FlushSourceBuffers();
  m_pZombieVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes =
      static_cast<UINT32>(m_ZombieDeathPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ZombieDeathPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pZombieVoice->SubmitSourceBuffer(&buf);
  m_pZombieVoice->Start();
}

bool AudioEngine::PlayBgm(const std::wstring &hsbPath, uint32_t trackIndex,
                          float volume) {
  if (!m_pXAudio2)
    return false;

  PSXSequencer seq;
  if (!seq.LoadHSB(hsbPath))
    return false;

  std::vector<int16_t> newPcm;
  if (!seq.RenderFullSequenceToPCM(newPcm, 180))
    return false;
  if (newPcm.empty())
    return false;

  // 1. Destroy previous voice to safely and synchronously unbind old audio
  // buffer from XAudio2
  if (m_pBgmVoice) {
    m_pBgmVoice->Stop(0);
    m_pBgmVoice->FlushSourceBuffers();
    m_pBgmVoice->DestroyVoice();
    m_pBgmVoice = nullptr;
  }

  // 2. Re-assign buffer safely
  m_BgmPcm = std::move(newPcm);

  // 3. Create fresh 44.1 kHz stereo voice
  WAVEFORMATEX wfxStereo = {};
  wfxStereo.wFormatTag = WAVE_FORMAT_PCM;
  wfxStereo.nChannels = 2;
  wfxStereo.nSamplesPerSec = 44100;
  wfxStereo.wBitsPerSample = 16;
  wfxStereo.nBlockAlign = (wfxStereo.nChannels * wfxStereo.wBitsPerSample) / 8;
  wfxStereo.nAvgBytesPerSec = wfxStereo.nSamplesPerSec * wfxStereo.nBlockAlign;

  HRESULT hr = m_pXAudio2->CreateSourceVoice(&m_pBgmVoice, &wfxStereo);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "[Audio] Failed to recreate BGM source voice (0x%08X)\n", hr);
    return false;
  }

  m_pBgmVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_BgmPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_BgmPcm.data());
  buf.LoopCount = XAUDIO2_LOOP_INFINITE;

  hr = m_pBgmVoice->SubmitSourceBuffer(&buf);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "[Audio] Failed to submit BGM source buffer (0x%08X)\n", hr);
    return false;
  }

  m_pBgmVoice->Start(0);
  return true;
}

void AudioEngine::StopBgm() {
  if (m_pBgmVoice) {
    m_pBgmVoice->Stop();
    m_pBgmVoice->FlushSourceBuffers();
  }
  m_CurrentBgmBank = 0xFF;
}

bool AudioEngine::PlayBgmBank(uint8_t bankId, float volume) {
  if (bankId > 56)
    return false;

  wchar_t hsbName[64];
  swprintf_s(hsbName, L"SEP%02X.HSB", bankId);

  std::wstring candidates[] = {
      std::wstring(L"extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName,
      std::wstring(L"../extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName,
      std::wstring(L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName,
      std::wstring(L"H:/Antigravity Projects/ultimate "
                   L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/") +
          hsbName,
  };

  for (const auto &path : candidates) {
    if (std::filesystem::exists(path)) {
      std::printf("\n[Audio] Playing BGM Track: Bank %u / 0x%02X (%ls, track "
                  "%u of 57)...\n",
                  bankId, bankId, hsbName, bankId + 1);
      if (PlayBgm(path, 0, volume)) {
        m_CurrentBgmBank = bankId;
        return true;
      }
    }
  }

  std::printf("[Audio] Failed to find or load BGM bank %u (%ls)\n", bankId,
              hsbName);
  return false;
}

void AudioEngine::NextBgm() {
  uint8_t nextBank =
      (m_CurrentBgmBank == 0xFF) ? 0 : (m_CurrentBgmBank + 1) % 57;
  PlayBgmBank(nextBank);
}

void AudioEngine::PrevBgm() {
  uint8_t prevBank = (m_CurrentBgmBank == 0xFF || m_CurrentBgmBank == 0)
                         ? 56
                         : (m_CurrentBgmBank - 1);
  PlayBgmBank(prevBank);
}

void AudioEngine::UpdateRoomBgm(uint8_t stage, uint8_t roomId) {
  if (stage >= 8 || roomId >= 32)
    return;

  uint8_t targetBank = g_StageBgmTables[stage][roomId];
  if (targetBank == m_CurrentBgmBank)
    return;

  if (targetBank == 0xFF) {
    std::printf("[Audio] Stage %u Room 0x%02X has no BGM (silence)\n", stage,
                roomId);
    StopBgm();
    return;
  }

  PlayBgmBank(targetBank);
}

void AudioEngine::PlayAmbience(float volume) {
  if (!m_pBgmVoice || m_AmbiencePcm.empty())
    return;

  m_pBgmVoice->Stop();
  m_pBgmVoice->FlushSourceBuffers();
  m_pBgmVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_AmbiencePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_AmbiencePcm.data());
  buf.LoopCount = XAUDIO2_LOOP_INFINITE;

  m_pBgmVoice->SubmitSourceBuffer(&buf);
  m_pBgmVoice->Start();
}

void AudioEngine::StopAmbience() { StopBgm(); }

void AudioEngine::SubmitData(const uint8_t *data, uint32_t size) {
  std::lock_guard<std::mutex> lock(m_bufferMutex);
  for (uint32_t i = 0; i < size; ++i) {
    m_buffer[m_writePos] = data[i];
    m_writePos = (m_writePos + 1) % BUFFER_SIZE;
  }
}

void AudioEngine::Update() {}

void AudioEngine::Shutdown() {
  if (!m_IsInitialized)
    return;
  StopBgm();

  if (m_pSfxVoice) {
    m_pSfxVoice->Stop(0);
    m_pSfxVoice->DestroyVoice();
    m_pSfxVoice = nullptr;
  }
  if (m_pGunVoice) {
    m_pGunVoice->Stop(0);
    m_pGunVoice->DestroyVoice();
    m_pGunVoice = nullptr;
  }
  if (m_pFootstepVoice) {
    m_pFootstepVoice->Stop(0);
    m_pFootstepVoice->DestroyVoice();
    m_pFootstepVoice = nullptr;
  }
  if (m_pZombieVoice) {
    m_pZombieVoice->Stop(0);
    m_pZombieVoice->DestroyVoice();
    m_pZombieVoice = nullptr;
  }
  if (m_pBgmVoice) {
    m_pBgmVoice->Stop(0);
    m_pBgmVoice->DestroyVoice();
    m_pBgmVoice = nullptr;
  }
  if (m_pVoiceLineVoice) {
    m_pVoiceLineVoice->Stop(0);
    m_pVoiceLineVoice->DestroyVoice();
    m_pVoiceLineVoice = nullptr;
  }
  if (m_pSourceVoice) {
    m_pSourceVoice->Stop(0);
    m_pSourceVoice->DestroyVoice();
    m_pSourceVoice = nullptr;
  }
  if (m_pMasterVoice) {
    m_pMasterVoice->DestroyVoice();
    m_pMasterVoice = nullptr;
  }
  if (m_pXAudio2) {
    m_pXAudio2->Release();
    m_pXAudio2 = nullptr;
  }

  CoUninitialize();
  m_IsInitialized = false;
}

bool AudioEngine::LoadDoorAudio(const std::wstring &hedPath,
                                const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open())
    return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16)
    return false;
  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);
  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize)
    return false;
  const uint16_t *vagTbl =
      reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open())
    return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
  size_t tone2Size = static_cast<size_t>(vagTbl[2]) * 8;

  m_DoorOpenPcm.clear();
  m_DoorClosePcm.clear();
  if (tone1Size > 0 && tone1Size <= vbSize) {
    DecodeSpuAdpcm(vb.data(), tone1Size, m_DoorOpenPcm);
  }
  if (tone2Size > 0 && tone1Size + tone2Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + tone1Size, tone2Size, m_DoorClosePcm);
  }
  std::printf("[Audio] Loaded authentic PS1 door audio: Open: %zu samples, "
              "Close: %zu samples\n",
              m_DoorOpenPcm.size(), m_DoorClosePcm.size());
  return true;
}

bool AudioEngine::LoadSelectAudio(const std::wstring &hedPath,
                                  const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open())
    return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16)
    return false;
  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);
  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize)
    return false;
  const uint16_t *vagTbl =
      reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open())
    return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
  size_t tone2Size = static_cast<size_t>(vagTbl[2]) * 8;
  size_t tone3Size = static_cast<size_t>(vagTbl[3]) * 8;
  size_t tone4Size = static_cast<size_t>(vagTbl[4]) * 8;
  size_t tone5Size = static_cast<size_t>(vagTbl[5]) * 8;

  size_t off1 = 0;
  size_t off2 = tone1Size;
  size_t off3 = tone1Size + tone2Size;
  size_t off4 = off3 + tone3Size;
  size_t off5 = off4 + tone4Size;

  m_ItemBoxOpenPcm.clear();
  m_TypewriterPcm.clear();
  m_MenuCursorPcm.clear();
  m_MenuConfirmPcm.clear();
  m_MenuCancelPcm.clear();

  if (tone1Size > 0 && off1 + tone1Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off1, tone1Size, m_ItemBoxOpenPcm);
  }
  if (tone2Size > 0 && off2 + tone2Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off2, tone2Size, m_TypewriterPcm);
  }
  if (tone3Size > 0 && off3 + tone3Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off3, tone3Size, m_MenuCursorPcm);
  }
  if (tone4Size > 0 && off4 + tone4Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off4, tone4Size, m_MenuConfirmPcm);
  }
  if (tone5Size > 0 && off5 + tone5Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off5, tone5Size, m_MenuCancelPcm);
  }

  std::printf("[Audio] Loaded authentic PS1 UI audio: ItemBox: %zu samples, Typewriter: %zu samples, Cursor: %zu samples, Confirm: %zu samples, Cancel: %zu samples\n",
              m_ItemBoxOpenPcm.size(), m_TypewriterPcm.size(), m_MenuCursorPcm.size(), m_MenuConfirmPcm.size(), m_MenuCancelPcm.size());
  return true;
}

void AudioEngine::PlayDoorOpen(float volume) {
  if (!m_pSfxVoice)
    return;
  const auto &pcm = (!m_DoorOpenPcm.empty()) ? m_DoorOpenPcm : m_Footstep1Pcm;
  if (pcm.empty())
    return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(pcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(pcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayDoorClose(float volume) {
  if (!m_pSfxVoice || m_DoorClosePcm.empty())
    return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_DoorClosePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_DoorClosePcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayMenuCursor(float volume) {
  if (!m_pSfxVoice || m_MenuCursorPcm.empty())
    return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes =
      static_cast<UINT32>(m_MenuCursorPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_MenuCursorPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayMenuConfirm(float volume) {
  if (!m_pSfxVoice || m_MenuConfirmPcm.empty())
    return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes =
      static_cast<UINT32>(m_MenuConfirmPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_MenuConfirmPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayItemPickup(float volume) { PlayMenuConfirm(volume); }

void AudioEngine::PlayMenuCancel(float volume) {
  if (!m_pSfxVoice || m_MenuCancelPcm.empty()) return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_MenuCancelPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_MenuCancelPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayItemBoxOpen(float volume) {
  if (!m_pSfxVoice || m_ItemBoxOpenPcm.empty()) return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_ItemBoxOpenPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ItemBoxOpenPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayItemBoxClose(float volume) {
  PlayItemBoxOpen(volume * 0.85f);
}

void AudioEngine::PlayTypewriter(float volume) {
  if (!m_pSfxVoice || m_TypewriterPcm.empty()) return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_TypewriterPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_TypewriterPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

bool AudioEngine::LoadTitleAudio(const std::wstring &vbPath) {
  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open()) return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  if (vbSize < 16) return false;
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  // Sample 0 in EVIL.VB (0..65200 bytes) is the "Resident Evil!" announcer voice
  size_t sampleSize = std::min<size_t>(65200, vbSize);
  m_TitleVoicePcm.clear();
  DecodeSpuAdpcm(vb.data(), sampleSize, m_TitleVoicePcm);
  std::printf("[Audio] Loaded authentic PS1 Title Call (EVIL.VB): %zu samples\n",
              m_TitleVoicePcm.size());
  return !m_TitleVoicePcm.empty();
}

void AudioEngine::PlayTitleVoice(float volume) {
  if (!m_pGunVoice || m_TitleVoicePcm.empty()) return;
  m_pGunVoice->Stop();
  m_pGunVoice->FlushSourceBuffers();
  m_pGunVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_TitleVoicePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_TitleVoicePcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pGunVoice->SubmitSourceBuffer(&buf);
  m_pGunVoice->Start();
}

bool AudioEngine::PlayVoiceClip(uint8_t stage, uint16_t clipId, float volume) {
  if (!m_pXAudio2) return false;

  XasClipInfo info;
  std::vector<int16_t> pcm;
  if (!XASDecoder::DecodeStageClip(stage, clipId, pcm, &info)) {
    std::printf("[Audio] Failed to decode voice clip: Stage %u, Clip 0x%02X (%u)\n", stage, clipId, clipId);
    return false;
  }

  if (pcm.empty()) return false;

  StopVoiceClip();

  m_VoiceLinePcm = std::move(pcm);
  m_CurrentVoiceClip = clipId;

  if (!m_pVoiceLineVoice) {
    WAVEFORMATEX wfxVoice = {};
    wfxVoice.wFormatTag = WAVE_FORMAT_PCM;
    wfxVoice.nChannels = 1;
    wfxVoice.nSamplesPerSec = 37800;
    wfxVoice.wBitsPerSample = 16;
    wfxVoice.nBlockAlign = (wfxVoice.nChannels * wfxVoice.wBitsPerSample) / 8;
    wfxVoice.nAvgBytesPerSec = wfxVoice.nSamplesPerSec * wfxVoice.nBlockAlign;
    HRESULT hr = m_pXAudio2->CreateSourceVoice(&m_pVoiceLineVoice, &wfxVoice);
    if (FAILED(hr)) return false;
  }

  m_pVoiceLineVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_VoiceLinePcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE*>(m_VoiceLinePcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  HRESULT hr = m_pVoiceLineVoice->SubmitSourceBuffer(&buf);
  if (FAILED(hr)) return false;

  hr = m_pVoiceLineVoice->Start(0);
  if (SUCCEEDED(hr)) {
    std::printf("[Audio] Playing Voice Clip: Stage %u, Clip 0x%02X (%u) [%s ch %u, %.2fs, %zu samples]\n",
                stage, clipId, clipId, info.xasFileName.c_str(), info.channel, info.durationSeconds, m_VoiceLinePcm.size());
    return true;
  }
  return false;
}

void AudioEngine::StopVoiceClip() {
  if (m_pVoiceLineVoice) {
    m_pVoiceLineVoice->Stop(0);
    m_pVoiceLineVoice->FlushSourceBuffers();
  }
}

bool AudioEngine::IsVoicePlaying() const {
  if (!m_pVoiceLineVoice) return false;
  XAUDIO2_VOICE_STATE state = {};
  m_pVoiceLineVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
  return state.BuffersQueued > 0;
}

void AudioEngine::NextVoiceClip(uint8_t stage) {
  uint16_t maxClips = XASDecoder::GetStageClipCount(stage);
  if (maxClips == 0) return;
  uint16_t nextClip = (m_CurrentVoiceClip + 1) % maxClips;
  PlayVoiceClip(stage, nextClip);
}

void AudioEngine::PrevVoiceClip(uint8_t stage) {
  uint16_t maxClips = XASDecoder::GetStageClipCount(stage);
  if (maxClips == 0) return;
  uint16_t prevClip = (m_CurrentVoiceClip == 0) ? (maxClips - 1) : (m_CurrentVoiceClip - 1);
  PlayVoiceClip(stage, prevClip);
}
