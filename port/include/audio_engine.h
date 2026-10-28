#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#include <xaudio2.h>

struct RoomSoundEntry {
  uint16_t vagId = 0;
  uint8_t vol = 127;
  uint8_t pan = 64;
  uint8_t center = 60;
  uint8_t shift = 0;
  uint8_t note = 60;
  float freqRatio44k = 1.0f;
};

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  bool Initialize();
  IXAudio2* GetXAudio2() const { return m_pXAudio2; }
  void Shutdown();

  bool LoadRoomAudio(const std::wstring &rdtPath);
  bool LoadWeaponAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadZombieAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadDoorAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadSelectAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadTitleAudio(const std::wstring &vbPath);

  void PlayFootstep(uint8_t stepType, uint8_t zoneBase, float volume = 0.85f);
  void PlayFootstep(float volume = 0.85f);
  void PlayRoomSoundAttr(uint8_t seIndex, float volumeScale = 1.0f);
  void PlayGunshot(float volume = 1.0f);
  void PlayDryFire(float volume = 0.8f);
  void PlayReload(float volume = 0.9f);
  void PlayWeaponReload(float volume = 0.9f) { PlayReload(volume); }
  void PlayDoorOpen(float volume = 0.9f);
  void PlayDoorClose(float volume = 0.9f);
  void PlayMenuCursor(float volume = 0.85f);
  void PlayMenuConfirm(float volume = 0.9f);
  void PlayMenuCancel(float volume = 0.85f);
  void PlayItemPickup(float volume = 0.95f);
  void PlayItemBoxOpen(float volume = 0.9f);
  void PlayItemBoxClose(float volume = 0.85f);
  void PlayTypewriter(float volume = 0.9f);
  void PlayTitleVoice(float volume = 1.0f);

  void PlayZombieGroan(float volume = 0.9f);
  void PlayZombieHit(float volume = 1.0f);
  void PlayZombieBite(float volume = 1.0f);
  void PlayZombieDeath(float volume = 0.95f);

  // XA Voice Acting Playback (VOICE/*.XAS)
  bool PlayVoiceClip(uint8_t stage, uint16_t clipId, float volume = 1.0f);
  void StopVoiceClip();
  bool IsVoicePlaying() const;
  uint16_t GetCurrentVoiceClip() const { return m_CurrentVoiceClip; }
  void NextVoiceClip(uint8_t stage);
  void PrevVoiceClip(uint8_t stage);

  void PlayAmbience(float volume = 0.25f);
  void StopAmbience();

  // BGM Playback & Authentic PS1 Stage/Room Sequencer
  bool PlayBgm(const std::wstring &hsbPath, uint32_t trackIndex = 0,
               float volume = 0.70f);
  void StopBgm();
  void UpdateRoomBgm(uint8_t stage, uint8_t roomId);
  bool PlayBgmBank(uint8_t bankId, float volume = 0.70f);
  void NextBgm();
  void PrevBgm();
  uint8_t GetCurrentBgmBank() const { return m_CurrentBgmBank; }

  void SubmitData(const uint8_t *data, uint32_t size);
  void Update();

private:
  IXAudio2 *m_pXAudio2 = nullptr;
  IXAudio2MasteringVoice *m_pMasterVoice = nullptr;
  IXAudio2SourceVoice *m_pFootstepVoice = nullptr;
  IXAudio2SourceVoice *m_pSfxVoice = nullptr;
  IXAudio2SourceVoice *m_pGunVoice = nullptr;
  IXAudio2SourceVoice *m_pZombieVoice = nullptr;
  IXAudio2SourceVoice *m_pBgmVoice = nullptr;
  IXAudio2SourceVoice *m_pVoiceLineVoice = nullptr;
  IXAudio2SourceVoice *m_pSourceVoice = nullptr;

  std::vector<int16_t> m_VoiceLinePcm;
  uint16_t m_CurrentVoiceClip = 0;

  std::vector<std::vector<int16_t>> m_RoomVagPcm;
  RoomSoundEntry m_RoomSoundTable[48] = {};
  uint8_t m_DefaultFloorZone = 45;
  std::vector<int16_t> m_Footstep1Pcm;
  std::vector<int16_t> m_Footstep2Pcm;
  std::vector<int16_t> m_GunshotPcm;
  std::vector<int16_t> m_DryFirePcm;
  std::vector<int16_t> m_ReloadPcm;
  std::vector<int16_t> m_ZombieGroanPcm;
  std::vector<int16_t> m_ZombieHitPcm;
  std::vector<int16_t> m_ZombieBitePcm;
  std::vector<int16_t> m_ZombieDeathPcm;
  std::vector<int16_t> m_DoorOpenPcm;
  std::vector<int16_t> m_DoorClosePcm;
  std::vector<int16_t> m_MenuCursorPcm;
  std::vector<int16_t> m_MenuConfirmPcm;
  std::vector<int16_t> m_MenuCancelPcm;
  std::vector<int16_t> m_ItemBoxOpenPcm;
  std::vector<int16_t> m_TypewriterPcm;
  std::vector<int16_t> m_TitleVoicePcm;
  std::vector<int16_t> m_AmbiencePcm;
  std::vector<int16_t> m_BgmPcm; // Active looping BGM stereo PCM
  bool m_FootstepToggle = false;
  uint8_t m_CurrentBgmBank = 0xFF; // 0xFF = no BGM active

  bool m_IsInitialized = false;

  std::vector<uint8_t> m_buffer;
  size_t m_writePos = 0;
  size_t m_readPos = 0;
  std::mutex m_bufferMutex;
  const uint32_t BUFFER_SIZE = 65536;
  WAVEFORMATEX m_wFormat = {};

  void LoadSampleFromVb(const std::wstring &vbPath, size_t startByte,
                        size_t endByte, std::vector<int16_t> &outPcm);
};

extern AudioEngine g_AudioEngine;
