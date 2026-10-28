# 1. Patch player_character.h
with open('port/include/player_character.h', 'r', encoding='utf-8') as f:
    pch = f.read()

pch_old = '''  int GetAmmo() const { return m_AmmoCount; }
  int GetMaxAmmo() const { return m_MaxAmmo; }
  void Reload();'''

pch_new = '''  int GetAmmo() const { return m_AmmoCount; }
  int GetMaxAmmo() const { return m_MaxAmmo; }
  bool JustFiredBullet() {
    bool fired = m_JustFiredBullet;
    m_JustFiredBullet = false;
    return fired;
  }
  void Reload();'''

if pch_old in pch:
    pch = pch.replace(pch_old, pch_new)
    print('player_character.h getter patched.')

mem_old = '''  float m_RumbleTimer = 0.0f;

  XMMATRIX m_PrevWorldViewProj = XMMatrixIdentity();'''

mem_new = '''  float m_RumbleTimer = 0.0f;
  bool m_JustFiredBullet = false;

  XMMATRIX m_PrevWorldViewProj = XMMatrixIdentity();'''

if mem_old in pch:
    pch = pch.replace(mem_old, mem_new)
    print('player_character.h member patched.')

with open('port/include/player_character.h', 'w', encoding='utf-8') as f:
    f.write(pch)

# 2. Patch enemy_zombie.cpp
with open('port/src/game/enemy_zombie.cpp', 'r', encoding='utf-8') as f:
    ez = f.read()

ez = ez.replace('PSX::TimLoader::LoadFromMemory', 'PSX::TimDecoder::LoadFromMemory')

with open('port/src/game/enemy_zombie.cpp', 'w', encoding='utf-8') as f:
    f.write(ez)
print('enemy_zombie.cpp TimDecoder patched.')
