with open('port/src/audio/audio_engine.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# 1. Constructor
ctor_old = 'm_pGunVoice(nullptr), m_pBgmVoice(nullptr)'
ctor_new = 'm_pGunVoice(nullptr), m_pZombieVoice(nullptr), m_pBgmVoice(nullptr)'
if ctor_old in src:
    src = src.replace(ctor_old, ctor_new)
    print('Constructor patched.')

# 2. Shutdown
shut_old = '''  if (m_pGunVoice) {
    m_pGunVoice->Stop(0);
    m_pGunVoice->DestroyVoice();
    m_pGunVoice = nullptr;
  }'''

shut_new = '''  if (m_pGunVoice) {
    m_pGunVoice->Stop(0);
    m_pGunVoice->DestroyVoice();
    m_pGunVoice = nullptr;
  }
  if (m_pZombieVoice) {
    m_pZombieVoice->Stop(0);
    m_pZombieVoice->DestroyVoice();
    m_pZombieVoice = nullptr;
  }'''

if shut_old in src:
    src = src.replace(shut_old, shut_new)
    print('Shutdown patched.')

with open('port/src/audio/audio_engine.cpp', 'w', encoding='utf-8') as f:
    f.write(src)
print('audio_engine.cpp fully updated.')
