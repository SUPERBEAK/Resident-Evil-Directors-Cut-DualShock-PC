#include "puzzle_manager.h"
#include "inventory_system.h"
#include "scd_vm.h"
#include <iostream>

namespace Game {

PuzzleManager g_PuzzleManager;

PuzzleManager::PuzzleManager() {}

void PuzzleManager::Initialize(InventorySystem* inventory, ScdVirtualMachine* scd) {
    m_Inventory = inventory;
    m_Scd = scd;
}

int PuzzleManager::GetCrestCount() const {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_CrestsPlaced & (1 << i)) count++;
    }
    return count;
}

PuzzleEventResult PuzzleManager::TryUseItem(uint8_t itemId, uint8_t stage, uint8_t room, float playerX, float playerZ) {
    PuzzleEventResult res;

    if (itemId >= 35 && itemId <= 38) {
        if (stage == 1 && (room == 0x0E || room == 0x0D || room == 0x0C || playerZ > 4500.0f)) {
            int crestIdx = itemId - 35;
            const char* crestNames[4] = { "Wind", "Star", "Sun", "Moon" };

            if (IsCrestPlaced(crestIdx)) {
                res.handled = true;
                res.success = false;
                res.message = std::string("The ") + crestNames[crestIdx] + " Crest is already placed in the relief.";
                return res;
            }

            m_CrestsPlaced |= (1 << crestIdx);
            res.handled = true;
            res.success = true;
            res.consumeItemId = itemId;

            if (AreAllCrestsPlaced()) {
                res.message = "All four crests are firmly placed in the stone relief!\nA heavy mechanical clunk echoes as the garden door unlocks!";
                if (m_Scd) {
                    m_Scd->SetBit(0, 7, 1);
                    m_Scd->SetBit(2, 14, 0);
                }
            } else {
                int left = 4 - GetCrestCount();
                res.message = std::string("You placed the ") + crestNames[crestIdx] + " Crest into the stone relief.\n" +
                              std::to_string(left) + " more crest slots remain empty.";
            }
            return res;
        }
    }

    if (itemId == 46) {
        if (stage == 1 && (room == 0x05 || room == 0x06 || room == 0x0B)) {
            if (m_PianoPlayed) {
                res.handled = true;
                res.success = false;
                res.message = "The piano music has already been performed.";
                return res;
            }

            m_PianoPlayed = true;
            res.handled = true;
            res.success = true;
            res.consumeItemId = 46;
            res.awardItemId = 45;
            res.message = "You placed the sheet music on the piano and played Moonlight Sonata.\nThe secret wall sliding panel opened, revealing the Gold Emblem!";

            if (m_Scd) {
                m_Scd->SetBit(0, 5, 1);
            }
            return res;
        }
    }

    if (itemId == 44) {
        if (stage == 1 && (room == 0x00 || room == 0x05)) {
            if (m_EmblemSwapped) {
                res.handled = true;
                res.success = false;
                res.message = "The Wooden Emblem is already fitted into the mantle relief.";
                return res;
            }

            m_EmblemSwapped = true;
            res.handled = true;
            res.success = true;
            res.consumeItemId = 44;
            res.message = "You placed the Wooden Emblem into the fireplace mantle relief.\nThe clock mechanism remains disarmed and the exit door stays unlocked!";

            if (m_Scd) {
                m_Scd->SetBit(0, 0, 1);
            }
            return res;
        }
    }

    if (itemId == 76 || (itemId >= 39 && itemId <= 43)) {
        if (stage == 3 && room == 0x0F) {
            m_VJoltUsed = true;
            res.handled = true;
            res.success = true;
            res.consumeItemId = itemId;
            res.message = "You poured the V-JOLT solution directly onto the giant roots!\nThe monstrous tendrils writhe and completely wither away!";

            if (m_Scd) {
                m_Scd->SetBit(0, 12, 1);
            }
            return res;
        }
    }

    if (itemId == 50) {
        if (stage == 5) {
            if (m_MoPasscodesEntered < 3) {
                m_MoPasscodesEntered++;
                res.handled = true;
                res.success = true;
                res.consumeItemId = 50;

                if (m_MoPasscodesEntered >= 3) {
                    res.message = "Terminal verified the third MO Disk passcode!\nAll three security locks released! The prison cell door is now unlocked!";
                    if (m_Scd) {
                        m_Scd->SetBit(0, 15, 1);
                    }
                } else {
                    res.message = "MO Disk passcode accepted and authenticated!\nSecurity clearance verified: " +
                                  std::to_string(m_MoPasscodesEntered) + " of 3 passcode terminals completed.";
                }
                return res;
            } else {
                res.handled = true;
                res.success = false;
                res.message = "All laboratory passcode terminals are already active.";
                return res;
            }
        }
    }

    res.handled = true;
    res.success = false;
    res.message = "Using that here will have no effect.";
    return res;
}

std::string PuzzleManager::CheckPuzzleZone(uint8_t stage, uint8_t room, float playerX, float playerZ) {
    if (stage == 1 && (room == 0x0E || room == 0x0D || room == 0x0C)) {
        if (AreAllCrestsPlaced()) {
            return "The stone relief holding the four crests is unlocked. The garden door is open.";
        } else {
            return "A heavy stone relief with four shaped indentations for Wind, Star, Sun, and Moon.";
        }
    }

    if (stage == 1 && (room == 0x05 || room == 0x0B || room == 0x06)) {
        if (m_PianoPlayed) {
            return "A grand piano with the Moonlight Sonata sheet music neatly arranged on the stand.";
        } else {
            return "It is a grand piano.\nA page of sheet music seems to be missing from the stand.";
        }
    }

    if (stage == 1 && (room == 0x00 || room == 0x05)) {
        if (m_EmblemSwapped) {
            return "The Wooden Emblem rests quietly inside the mantle relief.";
        } else {
            return "A fireplace mantle with an emblem-shaped indentation.";
        }
    }

    return "";
}

} // namespace Game
