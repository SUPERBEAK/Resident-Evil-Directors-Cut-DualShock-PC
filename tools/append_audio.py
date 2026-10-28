with open('port/src/audio/audio_engine.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

import re
# remove stub PlayDoorOpen
text = re.sub(r'void AudioEngine::PlayDoorOpen\(float volume\)\s*\{[^}]*\}', '', text)

append_code = """
bool AudioEngine::LoadDoorAudio(const std::wstring &hedPath, const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open()) return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16) return false;
  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);
  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize) return false;
  const uint16_t *vagTbl = reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open()) return false;
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
  std::printf("[Audio] Loaded authentic PS1 door audio: Open: %zu samples, Close: %zu samples\\n",
              m_DoorOpenPcm.size(), m_DoorClosePcm.size());
  return true;
}

bool AudioEngine::LoadSelectAudio(const std::wstring &hedPath, const std::wstring &vbPath) {
  std::ifstream hedFile(hedPath, std::ios::binary | std::ios::ate);
  if (!hedFile.is_open()) return false;
  size_t hedSize = static_cast<size_t>(hedFile.tellg());
  if (hedSize < 16) return false;
  std::vector<uint8_t> hed(hedSize);
  hedFile.seekg(0, std::ios::beg);
  hedFile.read(reinterpret_cast<char *>(hed.data()), hedSize);
  uint32_t vabHeadOff = *reinterpret_cast<const uint32_t *>(&hed[hedSize - 8]);
  if (vabHeadOff + 0xA20 + 32 > hedSize) return false;
  const uint16_t *vagTbl = reinterpret_cast<const uint16_t *>(&hed[vabHeadOff + 0xA20]);

  std::ifstream vbFile(vbPath, std::ios::binary | std::ios::ate);
  if (!vbFile.is_open()) return false;
  size_t vbSize = static_cast<size_t>(vbFile.tellg());
  std::vector<uint8_t> vb(vbSize);
  vbFile.seekg(0, std::ios::beg);
  vbFile.read(reinterpret_cast<char *>(vb.data()), vbSize);

  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
  size_t tone2Size = static_cast<size_t>(vagTbl[2]) * 8;
  size_t tone3Size = static_cast<size_t>(vagTbl[3]) * 8;
  size_t tone4Size = static_cast<size_t>(vagTbl[4]) * 8;

  size_t off3 = tone1Size + tone2Size;
  size_t off4 = off3 + tone3Size;

  m_MenuCursorPcm.clear();
  m_MenuConfirmPcm.clear();
  if (tone3Size > 0 && off3 + tone3Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off3, tone3Size, m_MenuCursorPcm);
  }
  if (tone4Size > 0 && off4 + tone4Size <= vbSize) {
    DecodeSpuAdpcm(vb.data() + off4, tone4Size, m_MenuConfirmPcm);
  }
  std::printf("[Audio] Loaded authentic PS1 UI audio: Cursor: %zu samples, Confirm/Pickup: %zu samples\\n",
              m_MenuCursorPcm.size(), m_MenuConfirmPcm.size());
  return true;
}

void AudioEngine::PlayDoorOpen(float volume) {
  if (!m_pSfxVoice) return;
  const auto &pcm = (!m_DoorOpenPcm.empty()) ? m_DoorOpenPcm : m_Footstep1Pcm;
  if (pcm.empty()) return;
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
  if (!m_pSfxVoice || m_DoorClosePcm.empty()) return;
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
  if (!m_pSfxVoice || m_MenuCursorPcm.empty()) return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_MenuCursorPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_MenuCursorPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayMenuConfirm(float volume) {
  if (!m_pSfxVoice || m_MenuConfirmPcm.empty()) return;
  m_pSfxVoice->Stop();
  m_pSfxVoice->FlushSourceBuffers();
  m_pSfxVoice->SetVolume(volume);
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_MenuConfirmPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_MenuConfirmPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;
  m_pSfxVoice->SubmitSourceBuffer(&buf);
  m_pSfxVoice->Start();
}

void AudioEngine::PlayItemPickup(float volume) {
  PlayMenuConfirm(volume);
}
"""

text = text.rstrip() + '\n' + append_code

with open('port/src/audio/audio_engine.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Updated audio_engine.cpp successfully!")
