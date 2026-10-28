#include "scd_vm.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Game {

void ScdVirtualMachine::Reset() {
  std::memset(m_FlagBanks, 0, sizeof(m_FlagBanks));
  m_Items.clear();
  m_Enemies.clear();
  m_VoiceClips.clear();
}

bool ScdVirtualMachine::TestBit(uint8_t bank, uint8_t bit) const {
  if (bank >= 10)
    return false;
  uint32_t mask = 0x80000000u >> (bit & 31);
  return (m_FlagBanks[bank] & mask) != 0;
}

void ScdVirtualMachine::SetBit(uint8_t bank, uint8_t bit, uint8_t op) {
  if (bank >= 10)
    return;
  uint32_t mask = 0x80000000u >> (bit & 31);
  if (op == 0) {
    m_FlagBanks[bank] |= mask;
  } else if (op == 1) {
    m_FlagBanks[bank] &= ~mask;
  } else if (op == 2) {
    m_FlagBanks[bank] ^= mask;
  }
}

bool ScdVirtualMachine::ExecuteInitScript(const uint8_t *data, size_t size,
                                          uint32_t initScriptOffset) {
  if (!data || initScriptOffset >= size)
    return false;
  m_Items.clear();
  m_Enemies.clear();
  size_t p = initScriptOffset;
  size_t maxScan = std::min(size, static_cast<size_t>(initScriptOffset + 8192));
  while (p + 1 < maxScan) {
    uint8_t op = data[p];
    if (op == 0x00)
      break;
    else if (op == 0x01)
      p += 2;
    else if (op == 0x04)
      p += 4;
    else if (op == 0x05) {
      if (p + 4 > maxScan)
        break;
      uint16_t b0 = *reinterpret_cast<const uint16_t *>(&data[p]);
      uint16_t b1 = *reinterpret_cast<const uint16_t *>(&data[p + 2]);
      SetBit(b0 >> 8, b1 >> 8, b1 & 0xFF);
      p += 4;
    } else if (op == 0x0C) {
      if (p + 0x1A > maxScan)
        break;
      p += 0x1A;
    } else if (op == 0x0D) {
      if (p + 0x12 > maxScan)
        break;
      p += 0x12;
    } else if (op == 0x18) {
      if (p + 0x1A > maxScan)
        break;
      ScdItemRecord item;
      item.slot = data[p + 1];
      item.x = *reinterpret_cast<const int16_t *>(&data[p + 2]);
      item.y = *reinterpret_cast<const int16_t *>(&data[p + 4]);
      item.z = *reinterpret_cast<const int16_t *>(&data[p + 6]);
      item.flags = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
      item.itemId = data[p + 0x0A];
      item.flagIdx = data[p + 0x16];
      item.isPickedUp = TestBit(7, item.flagIdx);
      if (!item.isPickedUp)
        m_Items.push_back(item);
      p += 0x1A;
    } else if (op == 0x1B) {
      if (p + 0x16 > maxScan)
        break;
      ScdEnemySpawnRecord enemy;
      enemy.slot = data[p + 1] & 0x0F;
      enemy.id = data[p + 2];
      enemy.behavior = data[p + 3];
      enemy.x = *reinterpret_cast<const int16_t *>(&data[p + 4]);
      enemy.y = *reinterpret_cast<const int16_t *>(&data[p + 6]);
      enemy.z = *reinterpret_cast<const int16_t *>(&data[p + 8]);
      enemy.angle = *reinterpret_cast<const int16_t *>(&data[p + 10]);
      enemy.killed = data[p + 14];
      if (enemy.killed == 0xFF || !TestBit(3, enemy.killed))
        m_Enemies.push_back(enemy);
      p += 0x16;
    } else {
      p++;
    }
  }
  return true;
}

bool ScdVirtualMachine::PickupItemAtPosition(float px, float pz,
                                             float pickupRadius,
                                             uint8_t &outItemId) {
  for (auto &item : m_Items) {
    if (item.isPickedUp)
      continue;
    float dx = static_cast<float>(item.x) - px;
    float dz = static_cast<float>(item.z) - pz;
    if (dx * dx + dz * dz <= pickupRadius * pickupRadius) {
      item.isPickedUp = true;
      SetBit(7, item.flagIdx, 0);
      outItemId = item.itemId;
      return true;
    }
  }
  return false;
}


void ScdVirtualMachine::ParseRoomVoiceEvents(const uint8_t *rdtData, size_t rdtSize) {
  m_VoiceClips.clear();
  if (!rdtData || rdtSize < 0x94) return;

  uint32_t offInit = *reinterpret_cast<const uint32_t*>(&rdtData[0x60]);
  uint32_t offMain = *reinterpret_cast<const uint32_t*>(&rdtData[0x64]);
  uint32_t offEv   = *reinterpret_cast<const uint32_t*>(&rdtData[0x68]);

  auto scanRange = [&](size_t start, size_t end, uint8_t evIdx) {
    if (start >= rdtSize || end > rdtSize || start >= end) return;
    for (size_t p = start; p + 4 <= end; ++p) {
      // Direct SCD voice opcode: 0x1E, cmd, clipId
      if (rdtData[p] == 0x1E && (rdtData[p+1] == 1 || rdtData[p+1] == 2)) {
        ScdVoiceRecord rec;
        rec.eventIndex = evIdx;
        rec.cmd = rdtData[p+1];
        rec.clipId = *reinterpret_cast<const uint16_t*>(&rdtData[p+2]);
        m_VoiceClips.push_back(rec);
        p += 3;
      }
      // SCD event exec wrapper: 0x07, 0x06, 0x1E, cmd, clipId
      else if (p + 6 <= end && rdtData[p] == 0x07 && rdtData[p+1] == 0x06 && rdtData[p+2] == 0x1E && (rdtData[p+3] == 1 || rdtData[p+3] == 2)) {
        ScdVoiceRecord rec;
        rec.eventIndex = evIdx;
        rec.cmd = rdtData[p+3];
        rec.clipId = *reinterpret_cast<const uint16_t*>(&rdtData[p+4]);
        m_VoiceClips.push_back(rec);
        p += 5;
      }
    }
  };

  // Scan init and main scripts
  if (offInit > 0 && offInit < rdtSize) {
    size_t end = (offMain > offInit && offMain <= rdtSize) ? offMain : std::min(rdtSize, static_cast<size_t>(offInit + 4096));
    scanRange(offInit, end, 0);
  }
  if (offMain > 0 && offMain < rdtSize) {
    size_t end = (offEv > offMain && offEv <= rdtSize) ? offEv : std::min(rdtSize, static_cast<size_t>(offMain + 4096));
    scanRange(offMain, end, 0);
  }

  // Scan event scripts table if present
  if (offEv > 0 && offEv + 4 <= rdtSize) {
    uint32_t firstRel = *reinterpret_cast<const uint32_t*>(&rdtData[offEv]);
    if (firstRel > 0 && firstRel < 0x400 && offEv + firstRel <= rdtSize) {
      size_t count = firstRel / 4;
      std::vector<uint32_t> evOffsets;
      for (size_t i = 0; i < count; ++i) {
        uint32_t rel = *reinterpret_cast<const uint32_t*>(&rdtData[offEv + i * 4]);
        if (rel > 0 && offEv + rel < rdtSize) {
          evOffsets.push_back(offEv + rel);
        }
      }
      for (size_t i = 0; i < evOffsets.size(); ++i) {
        size_t start = evOffsets[i];
        size_t end = (i + 1 < evOffsets.size()) ? evOffsets[i+1] : std::min(rdtSize, start + 2048);
        scanRange(start, end, static_cast<uint8_t>(i));
      }
    }
  }

  if (!m_VoiceClips.empty()) {
    std::printf("[SCD] Parsed %zu voice clips from room RDT\n", m_VoiceClips.size());
  }
}

} // namespace Game