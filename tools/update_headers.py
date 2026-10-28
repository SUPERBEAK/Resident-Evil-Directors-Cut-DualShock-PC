# 1. Update audio_engine.h
with open('port/include/audio_engine.h', 'r', encoding='utf-8') as f:
    text = f.read()

target = '  bool LoadZombieAudio(const std::wstring &hedPath, const std::wstring &vbPath);'
replacement = '''  bool LoadZombieAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadDoorAudio(const std::wstring &hedPath, const std::wstring &vbPath);
  bool LoadSelectAudio(const std::wstring &hedPath, const std::wstring &vbPath);'''
text = text.replace(target, replacement, 1)

target2 = '  void PlayDoorOpen(float volume = 0.85f);'
replacement2 = '''  void PlayDoorOpen(float volume = 0.9f);
  void PlayDoorClose(float volume = 0.9f);
  void PlayMenuCursor(float volume = 0.85f);
  void PlayMenuConfirm(float volume = 0.9f);
  void PlayItemPickup(float volume = 0.95f);'''
text = text.replace(target2, replacement2, 1)

target3 = '  std::vector<int16_t> m_ZombieDeathPcm;'
replacement3 = '''  std::vector<int16_t> m_ZombieDeathPcm;
  std::vector<int16_t> m_DoorOpenPcm;
  std::vector<int16_t> m_DoorClosePcm;
  std::vector<int16_t> m_MenuCursorPcm;
  std::vector<int16_t> m_MenuConfirmPcm;'''
text = text.replace(target3, replacement3, 1)

with open('port/include/audio_engine.h', 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated audio_engine.h successfully")

# 2. Update inventory_system.h
with open('port/include/inventory_system.h', 'r', encoding='utf-8') as f:
    text = f.read()

target4 = '  bool RemoveItem(uint8_t slotIdx, uint8_t count = 1);'
replacement4 = '''  bool RemoveItem(uint8_t slotIdx, uint8_t count = 1);
  bool HasItem(uint8_t itemId) const {
    for (const auto &slot : m_Slots) {
      if (slot.itemId == itemId) return true;
    }
    return false;
  }'''
text = text.replace(target4, replacement4, 1)

with open('port/include/inventory_system.h', 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated inventory_system.h successfully")
