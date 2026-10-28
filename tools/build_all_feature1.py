import os, sys

base = r"H:\Antigravity Projects\ultimate dualshock"

# 1. save_manager.h
sm_h = """#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Game {

#pragma pack(push, 1)
struct SaveSlotItem {
  uint8_t itemId = 0;
  uint8_t count = 0;
};

struct GameSaveState {
  char magic[8] = {'R', 'E', '1', 'D', 'S', 'A', 'V', '\\0'};
  uint32_t version = 1;
  uint32_t saveIndex = 0;
  uint64_t timestamp = 0;

  // Player state
  uint8_t stage = 1;
  uint8_t room = 0;
  uint8_t cameraCut = 0;
  float posX = 0.0f;
  float posY = 0.0f;
  float posZ = 0.0f;
  float facingAngle = 0.0f;
  int32_t health = 140;
  int32_t maxHealth = 140;
  bool isArmed = false;
  uint8_t equippedSlot = 1;

  // Active Inventory (6 slots)
  SaveSlotItem inventory[6];

  // Item Box (up to 32 items)
  uint32_t boxCount = 0;
  SaveSlotItem boxItems[32];

  // SCD Virtual Machine Flag Banks (32 uint32s = 10 banks)
  uint32_t scdFlags[32];
};
#pragma pack(pop)

class SaveManager {
public:
  static bool SaveToFile(uint32_t slotIndex, const GameSaveState &state);
  static bool LoadFromFile(uint32_t slotIndex, GameSaveState &outState);
  static bool DoesSaveExist(uint32_t slotIndex);
  static std::string GetSavePath(uint32_t slotIndex);
};

} // namespace Game
"""
with open(os.path.join(base, "port", "include", "save_manager.h"), "w", encoding="utf-8") as f:
    f.write(sm_h)

# 2. save_manager.cpp
sm_cpp = """#include "save_manager.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Game {

std::string SaveManager::GetSavePath(uint32_t slotIndex) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "saves/save_%u.dat", slotIndex);
  return std::string(buf);
}

bool SaveManager::DoesSaveExist(uint32_t slotIndex) {
  std::string p = GetSavePath(slotIndex);
  return std::filesystem::exists(p);
}

bool SaveManager::SaveToFile(uint32_t slotIndex, const GameSaveState &state) {
  try {
    std::filesystem::create_directories("saves");
  } catch (...) {
  }

  std::string p = GetSavePath(slotIndex);
  std::ofstream f(p, std::ios::binary);
  if (!f.is_open()) {
    std::cerr << "[SaveManager] Failed to open save file for writing: " << p << std::endl;
    return false;
  }

  GameSaveState s = state;
  if (s.timestamp == 0) {
    s.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
  }

  f.write(reinterpret_cast<const char *>(&s), sizeof(GameSaveState));
  if (!f.good()) {
    std::cerr << "[SaveManager] Error writing save file: " << p << std::endl;
    return false;
  }

  std::printf("[SaveManager] Successfully saved game to %s (Save #%u, Stage %u Room 0x%02X, HP: %d/%d)\\n",
              p.c_str(), s.saveIndex, s.stage, s.room, s.health, s.maxHealth);
  return true;
}

bool SaveManager::LoadFromFile(uint32_t slotIndex, GameSaveState &outState) {
  std::string p = GetSavePath(slotIndex);
  if (!std::filesystem::exists(p)) {
    std::cerr << "[SaveManager] Save file does not exist: " << p << std::endl;
    return false;
  }

  std::ifstream f(p, std::ios::binary);
  if (!f.is_open()) {
    std::cerr << "[SaveManager] Failed to open save file for reading: " << p << std::endl;
    return false;
  }

  GameSaveState s = {};
  f.read(reinterpret_cast<char *>(&s), sizeof(GameSaveState));
  if (f.gcount() < sizeof(GameSaveState)) {
    std::cerr << "[SaveManager] Save file is truncated: " << p << std::endl;
    return false;
  }

  if (std::memcmp(s.magic, "RE1DSAV", 7) != 0) {
    std::cerr << "[SaveManager] Invalid save file magic: " << p << std::endl;
    return false;
  }

  outState = s;
  std::printf("[SaveManager] Successfully loaded save from %s (Save #%u, Stage %u Room 0x%02X, HP: %d/%d)\\n",
              p.c_str(), outState.saveIndex, outState.stage, outState.room, outState.health, outState.maxHealth);
  return true;
}

} // namespace Game
"""
with open(os.path.join(base, "port", "src", "game", "save_manager.cpp"), "w", encoding="utf-8") as f:
    f.write(sm_cpp)

# 3. item_box.h
ib_h = """#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Render {
class PS1Pipeline;
struct PS1Vertex;
}

namespace Game {

class InventorySystem;

struct ItemBoxSlot {
  uint8_t itemId = 0;
  uint8_t count = 0;
};

class ItemBox {
public:
  ItemBox() = default;
  ~ItemBox() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  InventorySystem *inventory);
  void Shutdown();

  void Open();
  void Close();
  bool IsOpen() const { return m_IsOpen; }

  void Update(float deltaTime, uint32_t padButtons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

  const std::vector<ItemBoxSlot> &GetBoxSlots() const { return m_BoxSlots; }
  std::vector<ItemBoxSlot> &GetBoxSlotsMutable() { return m_BoxSlots; }
  void SetBoxSlot(size_t index, uint8_t itemId, uint8_t count);
  bool AddItemToBox(uint8_t itemId, uint8_t count = 1);
  bool RemoveItemFromBox(size_t index, uint8_t count = 1);

private:
  void RebuildVertices();
  void StoreItem(size_t inventorySlotIdx);
  void TakeItem(size_t boxSlotIdx);

  bool m_IsOpen = false;
  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;
  InventorySystem *m_Inventory = nullptr;

  int m_ActiveColumn = 0;
  int m_InventoryCursor = 0;
  int m_BoxCursor = 0;
  int m_BoxScrollOffset = 0;

  bool m_SubmenuActive = false;
  int m_SubmenuSelection = 0;

  std::vector<ItemBoxSlot> m_BoxSlots;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  uint32_t m_VertexCount = 0;

  float m_BlinkTimer = 0.0f;
};

extern ItemBox g_ItemBox;

} // namespace Game
"""
with open(os.path.join(base, "port", "include", "item_box.h"), "w", encoding="utf-8") as f:
    f.write(ib_h)

print("Step 1 done: save_manager and item_box.h written")
