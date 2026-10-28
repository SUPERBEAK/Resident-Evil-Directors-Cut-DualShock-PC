#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Game {

struct ScdItemRecord {
  uint8_t slot = 0;
  uint8_t itemId = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  uint16_t flags = 0;
  uint8_t flagIdx = 0;
  bool isPickedUp = false;
};

struct ScdEnemySpawnRecord {
  uint8_t slot = 0;
  uint8_t id = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  int16_t angle = 0;
  uint8_t killed = 0;
  uint8_t behavior = 0;
};

struct ScdVoiceRecord {
  uint8_t eventIndex = 0;
  uint8_t cmd = 1;
  uint16_t clipId = 0;
};

class ScdVirtualMachine {
public:
  ScdVirtualMachine() = default;
  ~ScdVirtualMachine() = default;

  void Reset();
  bool ExecuteInitScript(const uint8_t *data, size_t size,
                         uint32_t initScriptOffset);

  bool TestBit(uint8_t bank, uint8_t bit) const;
  void SetBit(uint8_t bank, uint8_t bit, uint8_t op);

  uint32_t GetFlagBank(size_t bank) const { return (bank < 10) ? m_FlagBanks[bank] : 0; }
  void SetFlagBank(size_t bank, uint32_t val) { if (bank < 10) m_FlagBanks[bank] = val; }

  const std::vector<ScdItemRecord> &GetSpawnedItems() const { return m_Items; }
  const std::vector<ScdEnemySpawnRecord> &GetSpawnedEnemies() const {
    return m_Enemies;
  }
  const std::vector<ScdVoiceRecord> &GetRoomVoiceClips() const {
    return m_VoiceClips;
  }

  void ParseRoomVoiceEvents(const uint8_t *rdtData, size_t rdtSize);

  bool PickupItemAtPosition(float px, float pz, float pickupRadius,
                            uint8_t &outItemId);

private:
  uint32_t m_FlagBanks[10] = {};
  std::vector<ScdItemRecord> m_Items;
  std::vector<ScdEnemySpawnRecord> m_Enemies;
  std::vector<ScdVoiceRecord> m_VoiceClips;
};

} // namespace Game
