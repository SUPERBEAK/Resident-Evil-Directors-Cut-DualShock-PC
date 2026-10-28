import os, re

base = r"H:\Antigravity Projects\ultimate dualshock"

# 1. audio_engine.h
ae_h_path = os.path.join(base, "port", "include", "audio_engine.h")
with open(ae_h_path, "r", encoding="utf-8") as f:
    ae_h = f.read()

if "PlayItemBoxOpen" not in ae_h:
    ae_h = ae_h.replace("void PlayItemPickup(float volume = 0.95f);",
                        """void PlayMenuCancel(float volume = 0.85f);
  void PlayItemPickup(float volume = 0.95f);
  void PlayItemBoxOpen(float volume = 0.9f);
  void PlayItemBoxClose(float volume = 0.85f);
  void PlayTypewriter(float volume = 0.9f);""")

if "m_ItemBoxOpenPcm" not in ae_h:
    ae_h = ae_h.replace("std::vector<int16_t> m_MenuConfirmPcm;",
                        """std::vector<int16_t> m_MenuConfirmPcm;
  std::vector<int16_t> m_MenuCancelPcm;
  std::vector<int16_t> m_ItemBoxOpenPcm;
  std::vector<int16_t> m_TypewriterPcm;""")

with open(ae_h_path, "w", encoding="utf-8") as f:
    f.write(ae_h)

# 2. inventory_system.h
inv_h_path = os.path.join(base, "port", "include", "inventory_system.h")
with open(inv_h_path, "r", encoding="utf-8") as f:
    inv_h = f.read()

if "GetSlotsMutable" not in inv_h:
    target = "const std::vector<InventorySlot> &GetSlots() const { return m_Slots; }"
    replacement = """const std::vector<InventorySlot> &GetSlots() const { return m_Slots; }
  std::vector<InventorySlot> &GetSlotsMutable() { return m_Slots; }
  void SetSlot(size_t index, uint8_t itemId, uint8_t count) {
    if (index < m_Slots.size()) {
      m_Slots[index].itemId = itemId;
      m_Slots[index].count = count;
    }
  }
  void SetEquippedSlot(uint8_t slot) { m_EquippedSlot = slot; }
  bool RemoveItemByItemId(uint8_t itemId, uint8_t count = 1);
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return m_SrvGpuHandle; }
  ID3D12DescriptorHeap *GetSrvHeap() const { return m_SrvHeap.Get(); }"""
    inv_h = inv_h.replace(target, replacement)

with open(inv_h_path, "w", encoding="utf-8") as f:
    f.write(inv_h)

# 3. inventory_system.cpp
inv_cpp_path = os.path.join(base, "port", "src", "game", "inventory_system.cpp")
with open(inv_cpp_path, "r", encoding="utf-8") as f:
    inv_cpp = f.read()

if "RemoveItemByItemId" not in inv_cpp:
    target = "bool InventorySystem::RemoveItem(uint8_t slotIdx, uint8_t count) {"
    replacement = """bool InventorySystem::RemoveItemByItemId(uint8_t itemId, uint8_t count) {
  for (size_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].itemId == itemId) {
      return RemoveItem(static_cast<uint8_t>(i), count);
    }
  }
  return false;
}

bool InventorySystem::RemoveItem(uint8_t slotIdx, uint8_t count) {"""
    inv_cpp = inv_cpp.replace(target, replacement)
    inv_cpp = inv_cpp.replace("if (itemId == 11 || itemId == 12 || itemId == 14) {",
                              "if (itemId == 11 || itemId == 12 || itemId == 14 || itemId == 47) {")

with open(inv_cpp_path, "w", encoding="utf-8") as f:
    f.write(inv_cpp)

# 4. player_character.h
pc_h_path = os.path.join(base, "port", "include", "player_character.h")
with open(pc_h_path, "r", encoding="utf-8") as f:
    pc_h = f.read()

if "GetHealth" not in pc_h:
    target = "bool IsLoaded() const { return m_IsLoaded; }"
    replacement = """bool IsLoaded() const { return m_IsLoaded; }

  int GetHealth() const { return m_Health; }
  int GetMaxHealth() const { return m_MaxHealth; }
  void SetHealth(int hp) { m_Health = hp; }
  void SetMaxHealth(int maxHp) { m_MaxHealth = maxHp; }
  void SetArmed(bool armed) { m_WeaponEquipped = armed; }"""
    pc_h = pc_h.replace(target, replacement)
    pc_h = pc_h.replace("bool m_WeaponEquipped = false;",
                        """bool m_WeaponEquipped = false;
  int m_Health = 140;
  int m_MaxHealth = 140;""")

with open(pc_h_path, "w", encoding="utf-8") as f:
    f.write(pc_h)

# 5. scd_vm.h
vm_h_path = os.path.join(base, "port", "include", "scd_vm.h")
with open(vm_h_path, "r", encoding="utf-8") as f:
    vm_h = f.read()

if "GetFlagBank" not in vm_h:
    target = "void SetBit(uint8_t bank, uint8_t bit, uint8_t op);"
    replacement = """void SetBit(uint8_t bank, uint8_t bit, uint8_t op);

  uint32_t GetFlagBank(size_t bank) const { return (bank < 10) ? m_FlagBanks[bank] : 0; }
  void SetFlagBank(size_t bank, uint32_t val) { if (bank < 10) m_FlagBanks[bank] = val; }"""
    vm_h = vm_h.replace(target, replacement)

with open(vm_h_path, "w", encoding="utf-8") as f:
    f.write(vm_h)

# 6. CMakeLists.txt
cm_path = os.path.join(base, "port", "CMakeLists.txt")
with open(cm_path, "r", encoding="utf-8") as f:
    cm = f.read()

if "save_manager.cpp" not in cm:
    cm = cm.replace("src/game/door_transition.cpp",
                    """src/game/door_transition.cpp
    src/game/save_manager.cpp
    src/game/item_box.cpp""")

with open(cm_path, "w", encoding="utf-8") as f:
    f.write(cm)

# 7. main.cpp string literal fixes
main_path = os.path.join(base, "port", "src", "main.cpp")
with open(main_path, "r", encoding="utf-8") as f:
    main_code = f.read()

# Fix any unescaped multiline strings in main_code
main_code = re.sub(r'"It\'s a typewriter\.\s+You need an ink ribbon',
                   r'"It\'s a typewriter.\\nYou need an ink ribbon', main_code)
main_code = re.sub(r'"It\'s a typewriter\.\s+Would you like to use an ink ribbon',
                   r'"It\'s a typewriter.\\nWould you like to use an ink ribbon', main_code)
main_code = re.sub(r'"An Item Box\.\s+Would you like to open it\?"',
                   r'"An Item Box.\\nWould you like to open it?"', main_code)

with open(main_path, "w", encoding="utf-8") as f:
    f.write(main_code)

print("Step 3 done: all headers and main.cpp updated")
