#include "save_manager.h"
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

  std::printf("[SaveManager] Successfully saved game to %s (Save #%u, Stage %u Room 0x%02X, HP: %d/%d)\n",
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
  std::printf("[SaveManager] Successfully loaded save from %s (Save #%u, Stage %u Room 0x%02X, HP: %d/%d)\n",
              p.c_str(), outState.saveIndex, outState.stage, outState.room, outState.health, outState.maxHealth);
  return true;
}

} // namespace Game
