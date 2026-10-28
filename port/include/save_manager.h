#pragma once
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
  char magic[8] = {'R', 'E', '1', 'D', 'S', 'A', 'V', '\0'};
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
