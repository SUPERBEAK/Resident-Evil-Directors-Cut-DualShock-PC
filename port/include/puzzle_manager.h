#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Game {

class InventorySystem;
class ScdVirtualMachine;

enum class PuzzleType {
    None,
    MansionCrests,
    PianoMoonlight,
    EmblemSwap,
    VJoltMixing,
    VJoltPlant42,
    MoDisks,
    MansionKeys
};

struct PuzzleEventResult {
    bool handled = false;
    bool success = false;
    std::string message;
    uint8_t consumeItemId = 0;
    uint8_t awardItemId = 0;
    uint8_t awardCount = 1;
};

class PuzzleManager {
public:
    PuzzleManager();
    ~PuzzleManager() = default;

    void Initialize(InventorySystem* inventory, ScdVirtualMachine* scd);

    PuzzleEventResult TryUseItem(uint8_t itemId, uint8_t stage, uint8_t room, float playerX, float playerZ);
    std::string CheckPuzzleZone(uint8_t stage, uint8_t room, float playerX, float playerZ);

    bool IsCrestPlaced(int crestIndex) const { return (m_CrestsPlaced & (1 << crestIndex)) != 0; }
    int GetCrestCount() const;
    bool AreAllCrestsPlaced() const { return m_CrestsPlaced == 0x0F; }

    bool IsPianoPlayed() const { return m_PianoPlayed; }
    void SetPianoPlayed(bool val) { m_PianoPlayed = val; }

    bool IsEmblemSwapped() const { return m_EmblemSwapped; }
    void SetEmblemSwapped(bool val) { m_EmblemSwapped = val; }

    bool IsVJoltUsed() const { return m_VJoltUsed; }
    void SetVJoltUsed(bool val) { m_VJoltUsed = val; }

    int GetMoPasscodesEntered() const { return m_MoPasscodesEntered; }
    void SetMoPasscodesEntered(int count) { m_MoPasscodesEntered = count; }

    uint8_t GetSaveCrestsPlaced() const { return m_CrestsPlaced; }
    void SetSaveCrestsPlaced(uint8_t val) { m_CrestsPlaced = val; }

private:
    InventorySystem* m_Inventory = nullptr;
    ScdVirtualMachine* m_Scd = nullptr;

    uint8_t m_CrestsPlaced = 0;
    bool m_PianoPlayed = false;
    bool m_EmblemSwapped = false;
    bool m_VJoltUsed = false;
    int m_MoPasscodesEntered = 0;
};

extern PuzzleManager g_PuzzleManager;

} // namespace Game
