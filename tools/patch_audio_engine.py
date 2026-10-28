with open('port/src/audio/audio_engine.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

old_load_select = """  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
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
  std::printf("[Audio] Loaded authentic PS1 UI audio: Cursor: %zu samples, "
              "Confirm/Pickup: %zu samples\\n",
              m_MenuCursorPcm.size(), m_MenuConfirmPcm.size());
  return true;"""

new_load_select = """  size_t tone1Size = static_cast<size_t>(vagTbl[1]) * 8;
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

  std::printf("[Audio] Loaded authentic PS1 UI audio: ItemBox: %zu samples, Typewriter: %zu samples, Cursor: %zu samples, Confirm: %zu samples, Cancel: %zu samples\\n",
              m_ItemBoxOpenPcm.size(), m_TypewriterPcm.size(), m_MenuCursorPcm.size(), m_MenuConfirmPcm.size(), m_MenuCancelPcm.size());
  return true;"""

assert old_load_select in code, "old_load_select pattern not found"
code = code.replace(old_load_select, new_load_select, 1)

new_methods = """
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
"""

code += new_methods

with open('port/src/audio/audio_engine.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Updated audio_engine.cpp with all save room UI sound methods!")
