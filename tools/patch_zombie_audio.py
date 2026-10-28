import re

with open('port/src/audio/audio_engine.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# 1. Initialize m_pZombieVoice
init_target = """  hr = m_pXAudio2->CreateSourceVoice(&m_pGunVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "[Audio] Failed to create Gun SFX source voice (0x%08X)\\n", hr);
  }"""

init_repl = """  hr = m_pXAudio2->CreateSourceVoice(&m_pGunVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "[Audio] Failed to create Gun SFX source voice (0x%08X)\\n", hr);
  }

  hr = m_pXAudio2->CreateSourceVoice(&m_pZombieVoice, &wfxMono);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[Audio] Failed to create Zombie SFX source voice (0x%08X)\\n", hr);
  }"""

if init_target in src:
    src = src.replace(init_target, init_repl)
    print('Init patched.')
else:
    print('Warning: Init target not found.')

# 2. Shutdown m_pZombieVoice
shutdown_target = """  if (m_pGunVoice) {
    m_pGunVoice->DestroyVoice();
    m_pGunVoice = nullptr;
  }"""

shutdown_repl = """  if (m_pGunVoice) {
    m_pGunVoice->DestroyVoice();
    m_pGunVoice = nullptr;
  }
  if (m_pZombieVoice) {
    m_pZombieVoice->DestroyVoice();
    m_pZombieVoice = nullptr;
  }"""

if shutdown_target in src:
    src = src.replace(shutdown_target, shutdown_repl)
    print('Shutdown patched.')
else:
    print('Warning: Shutdown target not found.')

# 3. LoadRoomAudio
load_target = """  std::printf("[Audio] Loaded authentic PS1 wood footsteps from %ls (Left: %zu "
              "samples, Right: %zu samples)\\n",
              rdtPath.c_str(), m_Footstep1Pcm.size(), m_Footstep2Pcm.size());
  return true;"""

load_repl = """  size_t tone3Size = static_cast<size_t>(vagTbl[3]) * 8;
  size_t groanSize = static_cast<size_t>(vagTbl[4]) * 8;
  size_t biteSize  = static_cast<size_t>(vagTbl[5]) * 8;
  size_t hitSize   = static_cast<size_t>(vagTbl[6]) * 8;
  size_t deathSize = static_cast<size_t>(vagTbl[7]) * 8;

  size_t groanOffset = vbOff + vag1Size + vag2Size + tone3Size;
  size_t biteOffset  = groanOffset + groanSize;
  size_t hitOffset   = biteOffset + biteSize;
  size_t deathOffset = hitOffset + hitSize;

  m_ZombieGroanPcm.clear();
  m_ZombieBitePcm.clear();
  m_ZombieHitPcm.clear();
  m_ZombieDeathPcm.clear();

  if (groanSize > 0 && groanOffset + groanSize <= fileSize) {
    DecodeSpuAdpcm(&rdt[groanOffset], groanSize, m_ZombieGroanPcm);
  }
  if (biteSize > 0 && biteOffset + biteSize <= fileSize) {
    DecodeSpuAdpcm(&rdt[biteOffset], biteSize, m_ZombieBitePcm);
  }
  if (hitSize > 0 && hitOffset + hitSize <= fileSize) {
    DecodeSpuAdpcm(&rdt[hitOffset], hitSize, m_ZombieHitPcm);
  }
  if (deathSize > 0 && deathOffset + deathSize <= fileSize) {
    DecodeSpuAdpcm(&rdt[deathOffset], deathSize, m_ZombieDeathPcm);
  }

  std::printf("[Audio] Loaded authentic PS1 wood footsteps from %ls (Left: %zu samples, Right: %zu samples)\\n",
              rdtPath.c_str(), m_Footstep1Pcm.size(), m_Footstep2Pcm.size());
  std::printf("[Audio] Loaded authentic PS1 room audio: Groan: %zu samples, Bite: %zu samples, Hit: %zu samples, Death: %zu samples\\n",
              m_ZombieGroanPcm.size(), m_ZombieBitePcm.size(), m_ZombieHitPcm.size(), m_ZombieDeathPcm.size());
  return true;"""

if load_target in src:
    src = src.replace(load_target, load_repl)
    print('LoadRoomAudio patched.')
else:
    print('Warning: Load target not found.')

# 4. Playback functions
play_target = """void AudioEngine::PlayReload(float volume) {
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
}"""

play_repl = play_target + """

void AudioEngine::PlayZombieGroan(float volume) {
  if (!m_pZombieVoice || m_ZombieGroanPcm.empty())
    return;

  m_pZombieVoice->Stop();
  m_pZombieVoice->FlushSourceBuffers();
  m_pZombieVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = static_cast<UINT32>(m_ZombieGroanPcm.size() * sizeof(int16_t));
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
  buf.AudioBytes = static_cast<UINT32>(m_ZombieBitePcm.size() * sizeof(int16_t));
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
  buf.AudioBytes = static_cast<UINT32>(m_ZombieDeathPcm.size() * sizeof(int16_t));
  buf.pAudioData = reinterpret_cast<const BYTE *>(m_ZombieDeathPcm.data());
  buf.Flags = XAUDIO2_END_OF_STREAM;

  m_pZombieVoice->SubmitSourceBuffer(&buf);
  m_pZombieVoice->Start();
}"""

if play_target in src:
    src = src.replace(play_target, play_repl)
    print('Playback methods patched.')
else:
    print('Warning: Play target not found.')

with open('port/src/audio/audio_engine.cpp', 'w', encoding='utf-8') as f:
    f.write(src)
print('audio_engine.cpp written.')
