#include "room_manager.h"

#include <algorithm>

#include <cstdio>

#include <cstring>

#include <filesystem>

#include <fstream>

#include <iostream>
#include <map>
#include <queue>



namespace fs = std::filesystem;



RoomManager::RoomManager() = default;

RoomManager::~RoomManager() = default;



bool RoomManager::Initialize(const std::wstring &baseExtractionDir) {

  m_baseDir = baseExtractionDir;



  std::vector<std::wstring> candidates = {

      m_baseDir,

      L"extracted/RE1-Dualshock/PSXUSA",

      L"../extracted/RE1-Dualshock/PSXUSA",

      L"../../extracted/RE1-Dualshock/PSXUSA",

      L"../../../extracted/RE1-Dualshock/PSXUSA",

      L"H:/Antigravity Projects/ultimate "

      L"dualshock/extracted/RE1-Dualshock/PSXUSA"};



  bool found = false;

  for (const auto &path : candidates) {

    if (!path.empty() && fs::exists(path) && fs::is_directory(path)) {

      m_baseDir = path;

      found = true;

      std::wprintf(L"[RoomManager] Base extraction directory set to: %ls\n",

                   m_baseDir.c_str());

      break;

    }

  }



  if (!found) {

    std::wprintf(

        L"[RoomManager] Warning: Could not locate extraction folder!\n");

    return false;

  }



  return true;

}



bool RoomManager::LoadRoom(uint16_t stageId, uint8_t roomId,
                           uint32_t initialCut) {
  // Authentic camera.c:82-84: Stage 6 & 7 (Revisited Mansion) share Stage 1 & 2 .BSS backgrounds
  uint16_t bssStage = (stageId == 6 || stageId == 7) ? (stageId - 5) : stageId;
  wchar_t roomHex[16];
  wchar_t bssHex[16];
  swprintf_s(roomHex, L"%X%02X", stageId, roomId);
  swprintf_s(bssHex, L"%X%02X", bssStage, roomId);

  std::wstring stageFolder = m_baseDir + L"/STAGE" + std::to_wstring(stageId);
  std::wstring bssFolder = m_baseDir + L"/STAGE" + std::to_wstring(bssStage);
  std::wstring bssPath = bssFolder + L"/ROOM" + bssHex + L".BSS";

  std::wstring rdtPath0 = stageFolder + L"/ROOM" + roomHex + L"0.RDT";

  std::wstring rdtPath1 = stageFolder + L"/ROOM" + roomHex + L"1.RDT";



  std::wstring targetRdt;

  if (fs::exists(rdtPath0)) {

    targetRdt = rdtPath0;

  } else if (fs::exists(rdtPath1)) {

    targetRdt = rdtPath1;

  } else {

    std::wprintf(

        L"[RoomManager] Error: No RDT found for Stage %u Room 0x%02X (%ls)\n",

        stageId, roomId, rdtPath0.c_str());

    return false;

  }



  if (!fs::exists(bssPath)) {

    std::wprintf(L"[RoomManager] Error: No BSS found at %ls\n",

                 bssPath.c_str());

    return false;

  }



  std::ifstream file(targetRdt, std::ios::binary);

  if (!file.is_open()) {

    std::wprintf(L"[RoomManager] Failed to open RDT: %ls\n", targetRdt.c_str());

    return false;

  }



  std::vector<uint8_t> rdtBytes((std::istreambuf_iterator<char>(file)),

                                std::istreambuf_iterator<char>());

  file.close();



  auto newRoom = std::make_unique<RoomData>();

  newRoom->stageId = stageId;

  newRoom->roomId = roomId;

  newRoom->bssPath = bssPath;

  newRoom->rdtPath = targetRdt;

  newRoom->rdtRaw = std::move(rdtBytes);



  if (!ParseRdt(newRoom->rdtRaw, *newRoom)) {

    std::wprintf(

        L"[RoomManager] Failed to parse RDT header for Stage %u Room 0x%02X\n",

        stageId, roomId);

    return false;

  }



  m_currentRoom = std::move(newRoom);

  m_activeCut = (initialCut < m_currentRoom->numCameras) ? initialCut : 0;



  std::wprintf(L"[RoomManager] Successfully loaded Stage %u Room 0x%02X (%u "

               L"cameras, %zu switch zones, %zu doors)\n",

               stageId, roomId, m_currentRoom->numCameras,

               m_currentRoom->switchZones.size(), m_currentRoom->doors.size());



  return true;

}



bool RoomManager::LoadRoomAtPosition(uint16_t stageId, uint8_t roomId, float playerX, float playerZ) {

  if (!LoadRoom(stageId, roomId, 0)) {

    return false;

  }



  uint32_t bestCut = FindCameraForPosition(playerX, playerZ);

  m_activeCut = bestCut;

  std::wprintf(L"[RoomManager] Position (%.1f, %.1f) selected initial Camera Cut %u\n",

               playerX, playerZ, m_activeCut);

  return true;

}



uint32_t RoomManager::FindCameraForPosition(float playerX, float playerZ) const {
  if (!m_currentRoom || m_currentRoom->cameras.empty()) {
    return 0;
  }

  uint32_t numCams = m_currentRoom->numCameras;

  // Step 1: Authentic PS1 room_init (room.c:232) + check_camera_switch (camera.c:120)
  // Start at Cut 0 and test all switch zones from Cut 0 (camFrom == 0 && camTo < numCams)
  for (const auto &zone : m_currentRoom->switchZones) {
    if (zone.camFrom == 0 && zone.camTo < numCams) {
      if (IsPointInQuad(playerX, playerZ, zone)) {
        return zone.camTo;
      }
    }
  }

  // Step 2: Check if player is inside Cut 0 bounding quad (camFrom == 0 && camTo >= numCams)
  for (const auto &zone : m_currentRoom->switchZones) {
    if (zone.camFrom == 0 && zone.camTo >= numCams) {
      if (IsPointInQuad(playerX, playerZ, zone)) {
        return 0;
      }
    }
  }

  // Step 3: Check all cameras bounding quads (camTo >= numCams)
  for (const auto &zone : m_currentRoom->switchZones) {
    if (zone.camFrom < numCams && zone.camTo >= numCams) {
      if (IsPointInQuad(playerX, playerZ, zone)) {
        return zone.camFrom;
      }
    }
  }

  // Step 4: Fallback sightline heuristic if outside all quads
  uint32_t bestCam = 0;
  float bestScore = -1e9f;
  for (uint32_t i = 0; i < m_currentRoom->cameras.size(); ++i) {
    const auto &cam = m_currentRoom->cameras[i];
    float fx = cam.to.x - cam.from.x;
    float fz = cam.to.z - cam.from.z;
    float flen = std::sqrt(fx * fx + fz * fz);
    if (flen < 1.0f) continue;
    fx /= flen;
    fz /= flen;
    float dx = playerX - cam.from.x;
    float dz = playerZ - cam.from.z;
    float forwardDist = dx * fx + dz * fz;
    if (forwardDist <= 50.0f) continue;
    float totDist = std::sqrt(dx * dx + dz * dz);
    float cosAngle = (totDist > 0.0f) ? (forwardDist / totDist) : 0.0f;
    float tgtDist = std::hypot(playerX - cam.to.x, playerZ - cam.to.z);
    float score = (cosAngle * 1000.0f) - (tgtDist * 0.05f);
    if (score > bestScore) {
      bestScore = score;
      bestCam = i;
    }
  }
  return bestCam;
}





bool RoomManager::ParseRdt(const std::vector<uint8_t> &data, RoomData &room) {

  if (data.size() < 0x94)

    return false;



  room.numCameras = data[1];

  room.ambientR = data[6];

  room.ambientG = data[8];

  room.ambientB = data[10];



  // 1. Parse Camera Records (+0x94, RdtCamera is 0x2C bytes each)

  size_t camBaseOffset = 0x94;

  for (uint8_t i = 0; i < room.numCameras; ++i) {

    size_t off = camBaseOffset + i * 0x2C;

    if (off + 0x2C > data.size())

      break;



    RoomCamera cam;

    int32_t fromX = *reinterpret_cast<const int32_t *>(&data[off + 8]);

    int32_t fromY = *reinterpret_cast<const int32_t *>(&data[off + 12]);

    int32_t fromZ = *reinterpret_cast<const int32_t *>(&data[off + 16]);



    int32_t toX = *reinterpret_cast<const int32_t *>(&data[off + 20]);

    int32_t toY = *reinterpret_cast<const int32_t *>(&data[off + 24]);

    int32_t toZ = *reinterpret_cast<const int32_t *>(&data[off + 28]);



    int32_t roll = *reinterpret_cast<const int32_t *>(&data[off + 32]);

    int32_t fov = *reinterpret_cast<const int32_t *>(&data[off + 40]);



    cam.from =

        DirectX::XMFLOAT3(static_cast<float>(fromX), static_cast<float>(fromY),

                          static_cast<float>(fromZ));

    cam.to = DirectX::XMFLOAT3(static_cast<float>(toX), static_cast<float>(toY),

                               static_cast<float>(toZ));

    cam.fov = (fov > 0) ? static_cast<float>(fov) : 220.0f;

    cam.roll = static_cast<float>(roll);



    room.cameras.push_back(cam);

  }



  // 2. Parse Camera Switch Zones (+0x48 points to CamSwitchZones, 0x14 bytes

  // each)

  uint32_t camSwitchesOff = *reinterpret_cast<const uint32_t *>(&data[0x48]);

  if (camSwitchesOff > 0 && camSwitchesOff + 0x14 <= data.size()) {

    size_t curr = camSwitchesOff;

    while (curr + 0x14 <= data.size()) {

      uint16_t camTo = *reinterpret_cast<const uint16_t *>(&data[curr]);

      uint16_t camFrom = *reinterpret_cast<const uint16_t *>(&data[curr + 2]);



      // PS1 sentinel 0xFFFF marks end of switch table

      if (camFrom == 0xFFFF || camTo == 0xFFFF)

        break;



      RoomCamSwitch z;

      z.camTo = camTo;

      z.camFrom = camFrom;

      z.x0 = *reinterpret_cast<const uint16_t *>(&data[curr + 4]);

      z.z0 = *reinterpret_cast<const uint16_t *>(&data[curr + 6]);

      z.x1 = *reinterpret_cast<const uint16_t *>(&data[curr + 8]);

      z.z1 = *reinterpret_cast<const uint16_t *>(&data[curr + 10]);

      z.x2 = *reinterpret_cast<const uint16_t *>(&data[curr + 12]);

      z.z2 = *reinterpret_cast<const uint16_t *>(&data[curr + 14]);

      z.x3 = *reinterpret_cast<const uint16_t *>(&data[curr + 16]);

      z.z3 = *reinterpret_cast<const uint16_t *>(&data[curr + 18]);



      // Only add zones transitioning to valid cameras in this room

      if (z.camFrom < room.numCameras) {

        room.switchZones.push_back(z);

      }



      curr += 0x14;

    }

  }



  // 3. Parse Door Records from SCD scripts (+0x60 init_script and +0x64 main_script)
  // Full 83-opcode byte length table extracted from dualshock/src/main/scd.c
  auto getOpcodeSize = [&](size_t p, size_t end) -> size_t {
    uint8_t op = data[p];
    if (op == 0x28) { // scd_op_28_enemy_prop_set
      if (p + 4 <= end) {
        uint8_t sub = data[p + 3];
        if (sub == 1) return 8;
        if (sub == 6 || sub == 8) return 4;
      }
      return 6;
    }
    if (op == 0x33) { // scd_op_33_player_prop_set
      if (p + 2 <= end) {
        uint8_t sub = data[p + 1];
        if (sub == 0 || sub == 4 || sub == 6 || sub == 7) return 2;
      }
      return 4;
    }
    switch (op) {
      case 0x00: case 0x26: case 0x2E: return 1;
      case 0x01: case 0x02: case 0x03: case 0x09: case 0x0A: case 0x0E:
      case 0x10: case 0x11: case 0x15: case 0x16: case 0x1A: case 0x1D:
      case 0x23: case 0x27: case 0x29: case 0x2C: case 0x39: case 0x3E:
      case 0x44: case 0x45: case 0x48: case 0x49: case 0x4A: case 0x4B:
      case 0x4D: case 0x52: return 2;
      case 0x04: case 0x05: case 0x06: case 0x08: case 0x0B: case 0x13:
      case 0x14: case 0x19: case 0x1E: case 0x22: case 0x24: case 0x25:
      case 0x2B: case 0x2D: case 0x2F: case 0x31: case 0x32: case 0x35:
      case 0x36: case 0x37: case 0x38: case 0x3A: case 0x41: case 0x42:
      case 0x43: case 0x4C: case 0x4E: return 4;
      case 0x07: case 0x1C: case 0x3B: case 0x3C: case 0x3F: case 0x4F:
      case 0x50: return 6;
      case 0x0F: case 0x34: case 0x51: return 8;
      case 0x12: case 0x17: return 0x0A;
      case 0x2A: case 0x30: case 0x3D: return 0x0C;
      case 0x20: case 0x21: case 0x47: return 0x0E;
      case 0x40: return 0x10;
      case 0x0D: return 0x12;
      case 0x1B: return 0x16;
      case 0x0C: case 0x18: return 0x1A;
      case 0x1F: return 0x1C;
      case 0x46: return 0x2C;
      default: return 2;
    }
  };

  struct RawActionZone {
    uint8_t slot;
    uint16_t x, z, w, d;
    uint8_t type, flags, param;
  };
  struct RawPlayerPos {
    uint16_t x;
    int16_t y;
    uint16_t z;
  };
  std::vector<RoomDoor> rawDoors;
  std::vector<RawActionZone> actionZones;
  std::vector<RawPlayerPos> playerPosSets;

  auto scanScriptDoors = [&](uint32_t scdOff) {
    if (scdOff == 0 || scdOff + 2 >= data.size()) return;
    size_t b = scdOff;
    while (b + 2 <= data.size()) {
      uint16_t blen = *reinterpret_cast<const uint16_t *>(&data[b]);
      if (blen <= 2 || blen > 4096 || b + blen > data.size()) break;
      size_t p = b + 2;
      size_t end = b + blen;
      while (p < end) {
        uint8_t op = data[p];
        if (op == 0x00) {
          p += 1;
          continue;
        } else if (op == 0x0C && p + 0x1A <= end) {
          RoomDoor door;
          door.slot = data[p + 1];
          door.x = *reinterpret_cast<const uint16_t *>(&data[p + 2]);
          door.z = *reinterpret_cast<const uint16_t *>(&data[p + 4]);
          door.w = *reinterpret_cast<const uint16_t *>(&data[p + 6]);
          door.d = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
          door.lock = data[p + 14];

          uint8_t stageRoom = data[p + 15];
          if (stageRoom >= 0x20) {
            door.targetStage = (stageRoom >> 5);
            // Authentic game.c:538 Revisited Mansion (Stage 6/7) mapping
            if (room.stageId >= 6 && (door.targetStage == 1 || door.targetStage == 2)) {
              door.targetStage += 5;
            }
            door.targetRoom = (stageRoom & 0x1F);
          } else {
            door.targetStage = static_cast<uint8_t>(room.stageId);
            door.targetRoom = (stageRoom & 0x1F);
          }

          door.nextX = *reinterpret_cast<const uint16_t *>(&data[p + 16]);
          door.nextY = *reinterpret_cast<const int16_t *>(&data[p + 18]);
          door.nextZ = *reinterpret_cast<const uint16_t *>(&data[p + 20]);
          door.nextDir = *reinterpret_cast<const int16_t *>(&data[p + 22]);
          door.keyItem = data[p + 24];
          door.isLocked = (door.lock & 0x80) != 0;

          // Skip permanently sealed / unrendered bonus doors with zero destination (e.g. ROOM1040 Slot 2)
          if (door.nextX == 0 && door.nextZ == 0 && door.keyItem == 0xFF) {
            p += 0x1A;
            continue;
          }

          // Map STAGE1/STAGE2 Helmet Key / Revisited Mansion rooms that live in STAGE6/STAGE7
          if (door.targetStage == 1 && (door.targetRoom == 0x10 || door.targetRoom == 0x19)) {
            door.targetStage = 6;
          } else if (door.targetStage == 2 && (door.targetRoom == 0x0C || door.targetRoom == 0x14 || door.targetRoom == 0x15)) {
            door.targetStage = 7;
          }
          if (door.targetStage == 7 && door.targetRoom == 0x14 && door.slot == 1) {
            door.nextX = 22800;
            door.nextY = 0;
            door.nextZ = 6300;
          } else if (room.stageId == 3 && room.roomId == 0x03 && stageRoom == 0xBD && door.nextX == 0 && door.nextZ == 0) {
            door.targetStage = 3;
            door.targetRoom = 0x05;
            door.nextX = 3200;
            door.nextY = 0;
            door.nextZ = 17100;
            door.lock = 0;
            door.isLocked = false;
          } else if (door.nextX == 0 && door.nextZ == 0) {
            if (door.targetStage == 3 && door.targetRoom == 0x10) {
              door.nextX = 21300; door.nextY = 4100; door.nextZ = 13200;
            } else if (door.targetStage == 5 && door.targetRoom == 0x15) {
              door.nextX = 5000; door.nextY = 0; door.nextZ = 5500;
            } else if (door.targetStage == 7 && door.targetRoom == 0x00) {
              door.nextX = 2500; door.nextY = 0; door.nextZ = 3300;
            }
          }

          if (door.lock == 0xFF && door.keyItem == 0 && !(room.stageId == 3 && room.roomId == 0x03)) {
            p += 0x1A;
            continue;
          }
          if (door.slot < 16 && door.targetStage >= 1 && door.targetStage <= 7 && door.targetRoom < 32) {
            rawDoors.push_back(door);
          }
          p += 0x1A;
        } else if (op == 0x0D && p + 0x12 <= end) {
          RawActionZone az;
          az.slot = data[p + 1];
          az.x = *reinterpret_cast<const uint16_t *>(&data[p + 2]);
          az.z = *reinterpret_cast<const uint16_t *>(&data[p + 4]);
          az.w = *reinterpret_cast<const uint16_t *>(&data[p + 6]);
          az.d = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
          az.type = data[p + 10];
          az.flags = data[p + 11];
          az.param = data[p + 12];
          if (az.slot < 32 && az.w >= 50 && az.w <= 32767 && az.d >= 50 && az.d <= 32767) {
            actionZones.push_back(az);
          }
          p += 0x12;
        } else if (op == 0x20 && p + 0x0E <= end) {
          RawPlayerPos pp;
          pp.x = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
          pp.y = *reinterpret_cast<const int16_t *>(&data[p + 10]);
          pp.z = *reinterpret_cast<const uint16_t *>(&data[p + 12]);
          playerPosSets.push_back(pp);
          p += 0x0E;
        } else {
          size_t step = getOpcodeSize(p, end);
          p += (step > 0) ? step : 1;
        }
      }
      b += blen;
    }
  };

  uint32_t initScdOff = *reinterpret_cast<const uint32_t *>(&data[0x60]);
  scanScriptDoors(initScdOff);
  uint32_t mainScdOff = *reinterpret_cast<const uint32_t *>(&data[0x64]);
  scanScriptDoors(mainScdOff);

  // Filter out any door slot overwritten by 0x0D type 10 (stairs zone, e.g. ROOM1060/ROOM6060 slot 3)
  std::map<uint8_t, RoomDoor> uniqueDoors;
  for (const auto &d : rawDoors) {
    bool isStairsSlot = false;
    for (const auto &az : actionZones) {
      if (az.slot == d.slot && az.type == 10) {
        isStairsSlot = true;
        break;
      }
    }
    if (isStairsSlot) continue;
    auto it = uniqueDoors.find(d.slot);
    if (it == uniqueDoors.end()) {
      uniqueDoors[d.slot] = d;
    } else if ((it->second.w <= 16 && it->second.d <= 16) && !(d.w <= 16 && d.d <= 16)) {
      uniqueDoors[d.slot] = d;
    }
  }

  std::vector<RawActionZone> candidateActions;
  for (const auto &az : actionZones) {
    if ((az.type == 5 || az.type == 9 || az.type == 14 || az.type == 2 || az.type == 7) &&
        az.w <= 10000 && az.d <= 10000) {
      candidateActions.push_back(az);
    }
  }

  size_t usedActionIdx = 0;
  size_t elevIdx = 0;
  for (auto &kv : uniqueDoors) {
    RoomDoor d = kv.second;
    if (d.targetStage == room.stageId && d.targetRoom == room.roomId && d.nextX == 0 && d.nextZ == 0) {
      continue;
    }
    if (d.w <= 16 && d.d <= 16) {
      if (usedActionIdx < candidateActions.size()) {
        const auto &act = candidateActions[usedActionIdx++];
        d.x = act.x;
        d.z = act.z;
        d.w = act.w;
        d.d = act.d;
      } else if (!playerPosSets.empty()) {
        const auto &pp = playerPosSets[std::min(elevIdx, playerPosSets.size() - 1)];
        int32_t offsetX = (elevIdx % 2 == 0) ? -600 : 600;
        d.x = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(pp.x) + offsetX - 600));
        d.z = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(pp.z) - 600));
        d.w = 1200;
        d.d = 1200;
        elevIdx++;
      }
    }
    if (d.w >= 50 && d.d >= 50) {
      room.doors.push_back(d);
    }
  }

  // 4. Parse SCA Room Collision Boundaries (+0x4C)

  uint32_t scaOff = *reinterpret_cast<const uint32_t *>(&data[0x4C]);

  if (scaOff > 0 && scaOff + 0x18 <= data.size()) {

    room.scaSplitX = *reinterpret_cast<const int16_t *>(&data[scaOff]);

    room.scaSplitZ = *reinterpret_cast<const int16_t *>(&data[scaOff + 2]);

    const uint32_t *counts = reinterpret_cast<const uint32_t *>(&data[scaOff + 4]);

    room.scaCounts[0] = counts[0];

    room.scaCounts[1] = counts[1];

    room.scaCounts[2] = counts[2];

    room.scaCounts[3] = counts[3];



    uint32_t totalBoxes = counts[0] + counts[1] + counts[2] + counts[3];

    size_t boxDataOff = scaOff + 0x18;

    for (uint32_t i = 0; i < totalBoxes; ++i) {

      size_t bOff = boxDataOff + i * 12;

      if (bOff + 12 > data.size())

        break;



      RoomCollisionBox box;

      box.x1 = *reinterpret_cast<const uint16_t *>(&data[bOff + 0]);

      box.z1 = *reinterpret_cast<const uint16_t *>(&data[bOff + 2]);

      box.x2 = *reinterpret_cast<const uint16_t *>(&data[bOff + 4]);

      box.z2 = *reinterpret_cast<const uint16_t *>(&data[bOff + 6]);

      box.type = *reinterpret_cast<const uint16_t *>(&data[bOff + 8]);

      box.floor = *reinterpret_cast<const uint16_t *>(&data[bOff + 10]);



      room.collisionBoxes.push_back(box);

    }

    std::printf("[RoomManager] Loaded %zu collision boundaries (split: %d, %d; counts: %u, %u, %u, %u)\n",
                room.collisionBoxes.size(), room.scaSplitX, room.scaSplitZ,
                room.scaCounts[0], room.scaCounts[1], room.scaCounts[2], room.scaCounts[3]);
  }

  // 5. Parse Floor Sound Zones (+0x5C, .FLR FootstepZone 10 bytes each)
  uint32_t flrOff = *reinterpret_cast<const uint32_t *>(&data[0x5C]);
  if (flrOff > 0 && flrOff + 2 <= data.size()) {
    uint16_t zoneCount = *reinterpret_cast<const uint16_t *>(&data[flrOff]);
    size_t zPos = flrOff + 2;
    for (uint16_t i = 0; i < zoneCount; ++i) {
      if (zPos + 10 > data.size())
        break;
      RoomFloorZone fz;
      fz.x = *reinterpret_cast<const int16_t *>(&data[zPos + 0]);
      fz.z = *reinterpret_cast<const int16_t *>(&data[zPos + 2]);
      fz.w = *reinterpret_cast<const uint16_t *>(&data[zPos + 4]);
      fz.d = *reinterpret_cast<const uint16_t *>(&data[zPos + 6]);
      fz.seBase = data[zPos + 8];
      fz.pad = data[zPos + 9];
      room.floorZones.push_back(fz);
      zPos += 10;
    }
    std::printf("[RoomManager] Loaded %zu floor sound zones (.FLR)\n",
                room.floorZones.size());
  }

  return true;
}

uint8_t RoomManager::LookupFootstepZone(float playerX, float playerZ) const {
  if (!m_currentRoom || m_currentRoom->floorZones.empty()) {
    return 45;
  }
  int16_t px = static_cast<int16_t>(playerX);
  int16_t pz = static_cast<int16_t>(playerZ);
  for (const auto &fz : m_currentRoom->floorZones) {
    if (static_cast<uint32_t>(static_cast<uint16_t>(px - fz.x)) < fz.w &&
        static_cast<uint32_t>(static_cast<uint16_t>(pz - fz.z)) < fz.d) {
      return fz.seBase;
    }
  }
  return m_currentRoom->floorZones.back().seBase;
}



bool RoomManager::IsPointInQuad(float posX, float posZ,

                                const RoomCamSwitch &z) {

  int32_t x0 = z.x0;

  int32_t y0 = z.z0;

  int32_t px = static_cast<int32_t>(posX) - x0;

  int32_t py = static_cast<int32_t>(posZ) - y0;

  int32_t ax = static_cast<int32_t>(z.x1) - x0;

  int32_t ay = static_cast<int32_t>(z.z1) - y0;

  int32_t bx = static_cast<int32_t>(z.x3) - x0;

  int32_t by = static_cast<int32_t>(z.z3) - y0;



  if (static_cast<int64_t>(ax) * py > static_cast<int64_t>(ay) * px)

    return false;

  if (static_cast<int64_t>(bx) * py < static_cast<int64_t>(by) * px)

    return false;



  int32_t cx = static_cast<int32_t>(z.x2) - x0;

  int32_t cy = static_cast<int32_t>(z.z2) - y0;

  py -= cy;

  ax -= cx;

  px -= cx;

  ay -= cy;

  bx -= cx;

  by -= cy;



  if (static_cast<int64_t>(ax) * py < static_cast<int64_t>(ay) * px ||

      static_cast<int64_t>(bx) * py > static_cast<int64_t>(by) * px)

    return false;

  return true;

}



bool RoomManager::UpdateCameraZone(float playerX, float playerZ,

                                   uint32_t &outNewCut) {

  if (!m_currentRoom)

    return false;



  for (const auto &zone : m_currentRoom->switchZones) {

    if (zone.camFrom == m_activeCut && zone.camTo != m_activeCut) {

      if (IsPointInQuad(playerX, playerZ, zone)) {

        if (zone.camTo < m_currentRoom->numCameras) {

          m_activeCut = zone.camTo;

          outNewCut = m_activeCut;

          return true;

        }

      }

    }

  }



  return false;

}



bool RoomManager::CheckDoorTrigger(float playerX, float playerZ,

                                   float facingAngle, RoomDoor &outDoor) const {

  if (!m_currentRoom || m_currentRoom->doors.empty())

    return false;



  // Compute authentic PS1 forward probe point matching check_action_object

  float fx = std::sin(facingAngle);

  float fz = std::cos(facingAngle);

  float probeX = playerX + fx * 600.0f;

  float probeZ = playerZ + fz * 600.0f;



  const float margin = 400.0f; // Friendly interaction margin around trigger box

  float closestDistSq = 1e18f;

  const RoomDoor* closestDoor = nullptr;



  for (const auto &door : m_currentRoom->doors) {

    float minX = static_cast<float>(door.x) - margin;

    float maxX = static_cast<float>(door.x + door.w) + margin;

    float minZ = static_cast<float>(door.z) - margin;

    float maxZ = static_cast<float>(door.z + door.d) + margin;



    // Test 1: Forward probe point inside expanded trigger box

    bool probeInBox = (probeX >= minX && probeX <= maxX && probeZ >= minZ && probeZ <= maxZ);

    // Test 2: Player position inside expanded trigger box

    bool playerInBox = (playerX >= minX && playerX <= maxX && playerZ >= minZ && playerZ <= maxZ);



    float cx = door.x + door.w * 0.5f;

    float cz = door.z + door.d * 0.5f;

    float distPlayerSq = (playerX - cx) * (playerX - cx) + (playerZ - cz) * (playerZ - cz);

    float distProbeSq = (probeX - cx) * (probeX - cx) + (probeZ - cz) * (probeZ - cz);

    float bestDistSq = std::min(distPlayerSq, distProbeSq);



    if (probeInBox || playerInBox || bestDistSq <= (600.0f * 600.0f)) {

      if (bestDistSq < closestDistSq) {

        closestDistSq = bestDistSq;

        closestDoor = &door;

      }

    }

  }



  if (closestDoor) {

    outDoor = *closestDoor;

    return true;

  }



  return false;

}



bool RoomManager::SetActiveCut(uint32_t cutIndex) {

  if (!m_currentRoom || cutIndex >= m_currentRoom->numCameras) {

    return false;

  }

  m_activeCut = cutIndex;

  return true;

}



const RoomCamera *RoomManager::GetActiveCamera() const {

  if (!m_currentRoom || m_activeCut >= m_currentRoom->cameras.size()) {

    return nullptr;

  }

  return &m_currentRoom->cameras[m_activeCut];

}



const RoomCamera *RoomManager::GetCamera(uint32_t index) const {

  if (!m_currentRoom || index >= m_currentRoom->cameras.size()) {

    return nullptr;

  }

  return &m_currentRoom->cameras[index];

}



bool RoomManager::ResolveCollision(float &posX, float &posZ, float oldX, float oldZ, float radius, float posY) const {

  if (!m_currentRoom || m_currentRoom->collisionBoxes.empty())

    return false;



  bool anyCollision = false;



  // Floor height discrimination:

  // In PS1 RE1, ground floor is Y >= -1000 (mask 0x0100), 2nd floor is Y < -1000 (mask 0x0200).

  uint16_t floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;



  // We perform up to 3 passes so corner/junction collisions resolve cleanly without snagging

  for (int pass = 0; pass < 3; ++pass) {

    bool passHit = false;



    for (const auto &box : m_currentRoom->collisionBoxes) {

      uint8_t shapeType = static_cast<uint8_t>(box.type & 0xFF);



      // Skip stairs triggers (4) and climbable step ledges (5) for player

      if (shapeType == 4 || shapeType == 5)

        continue;



      // Authentic PS1 room_collision.c:107-120:
      // All boundaries with floor bits 0x0100, 0x0200 (low obstacle/furniture/railing),
      // or 0x0300 (full-height wall) block player movement!



      float minX = static_cast<float>(std::min(box.x1, box.x2));

      float maxX = static_cast<float>(std::max(box.x1, box.x2));

      float minZ = static_cast<float>(std::min(box.z1, box.z2));

      float maxZ = static_cast<float>(std::max(box.z1, box.z2));



      if (shapeType == 3) {

        // Authentic PS1 Circle Collision

        float obstacleRadius = (maxX - minX) * 0.5f;

        float centerX = (minX + maxX) * 0.5f;

        float centerZ = (minZ + maxZ) * 0.5f;



        float cdx = posX - centerX;

        float cdz = posZ - centerZ;

        float distSq = cdx * cdx + cdz * cdz;

        float minDist = obstacleRadius + radius;



        if (distSq < minDist * minDist) {

          float dist = std::sqrt(distSq);

          if (dist > 0.001f) {

            float overlap = minDist - dist;

            posX += (cdx / dist) * overlap;

            posZ += (cdz / dist) * overlap;

          } else {

            posX += minDist;

          }

          passHit = true;

          anyCollision = true;

        }

      } else {

        // Authentic PS1 AABB Box Collision (shapeType == 1 or 5)

        if (posX < minX - radius || posX > maxX + radius ||

            posZ < minZ - radius || posZ > maxZ + radius) {

          continue;

        }



        constexpr float safety = 18.0f; // Authentic 0x12 PS1 safety push

        float effRadius = radius + safety;



        float dxRight = (maxX + effRadius) - posX;

        float dxLeft = (minX - effRadius) - posX;

        float dx = (-dxLeft < dxRight) ? dxLeft : dxRight;



        float dzTop = (maxZ + effRadius) - posZ;

        float dzBottom = (minZ - effRadius) - posZ;

        float dz = (-dzBottom < dzTop) ? dzBottom : dzTop;



        float delX = posX - oldX;

        float delZ = posZ - oldZ;



        uint32_t side = 0;

        if (delX != 0.0f && ((dx < 0.0f) != (delX < 0.0f))) side |= 1;

        if (delZ != 0.0f && ((dz < 0.0f) != (delZ < 0.0f))) side |= 2;



        switch (side) {

        case 1:

          if (std::abs(dx) <= 800.0f) {

            posX += dx;

          } else {

            if (std::abs(dx) < std::abs(dz)) posX += dx;

            else posZ += dz;

          }

          break;

        case 2:

          if (std::abs(dz) <= 800.0f) {

            posZ += dz;

          } else {

            if (std::abs(dx) < std::abs(dz)) posX += dx;

            else posZ += dz;

          }

          break;

        case 3:

        case 0:

        default:

          if (std::abs(dx) < std::abs(dz)) {

            posX += dx;

          } else {

            posZ += dz;

          }

          break;

        }



        passHit = true;

        anyCollision = true;

      }

    }



    if (!passHit)

      break;

  }



  return anyCollision;

}


void RoomManager::FindPathToDoor(float startX, float startZ, float posY,
                                 const RoomDoor &door,
                                 std::vector<DirectX::XMFLOAT2> &outWaypoints) const {
  outWaypoints.clear();
  float doorCx = door.x + door.w * 0.5f;
  float doorCz = door.z + door.d * 0.5f;
  if (!m_currentRoom) {
    outWaypoints.push_back({doorCx, doorCz});
    return;
  }

  // Pick a collision-free stand-off goal point around the door trigger box
  float goalX = doorCx;
  float goalZ = doorCz;
  {
    float tx = doorCx, tz = doorCz;
    if (ResolveCollision(tx, tz, doorCx, doorCz, 450.0f, posY) &&
        std::hypot(tx - doorCx, tz - doorCz) > 5.0f) {
      bool foundStandoff = false;
      float rx0 = tx, rz0 = tz;
      if (!ResolveCollision(rx0, rz0, tx, tz, 445.0f, posY) ||
          std::hypot(rx0 - tx, rz0 - tz) < 5.0f) {
        float ang0 = std::atan2(doorCx - tx, doorCz - tz);
        RoomDoor hit0;
        if (CheckDoorTrigger(tx, tz, ang0, hit0) && hit0.slot == door.slot) {
          goalX = tx;
          goalZ = tz;
          foundStandoff = true;
        }
      }
      const float radii[] = {350.0f, 550.0f, 750.0f, 950.0f, 1200.0f};
      for (float rOff : radii) {
        if (foundStandoff) break;
        for (int aDeg = 0; aDeg < 360; aDeg += 30) {
          float rad = aDeg * (3.14159265f / 180.0f);
          float candX = doorCx + std::sin(rad) * rOff;
          float candZ = doorCz + std::cos(rad) * rOff;
          float rx = candX, rz = candZ;
          if (!ResolveCollision(rx, rz, candX, candZ, 445.0f, posY) ||
              std::hypot(rx - candX, rz - candZ) < 5.0f) {
            float ang = std::atan2(doorCx - rx, doorCz - rz);
            RoomDoor hit;
            if (CheckDoorTrigger(rx, rz, ang, hit) && hit.slot == door.slot) {
              goalX = rx;
              goalZ = rz;
              foundStandoff = true;
              break;
            }
          }
        }
      }
    }
  }

  constexpr float cell = 350.0f;
  int sx = static_cast<int>(std::lround(startX / cell));
  int sz = static_cast<int>(std::lround(startZ / cell));
  int gx = static_cast<int>(std::lround(goalX / cell));
  int gz = static_cast<int>(std::lround(goalZ / cell));

  struct Node {
    float f, g;
    int x, z;
    bool operator>(const Node &o) const { return f > o.f; }
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openHeap;
  std::map<std::pair<int, int>, std::pair<int, int>> cameFrom;
  std::map<std::pair<int, int>, float> gScore;

  openHeap.push({0.0f, 0.0f, sx, sz});
  gScore[{sx, sz}] = 0.0f;

  const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  const float costs[8] = {1.0f,1.0f,1.0f,1.0f,1.414f,1.414f,1.414f,1.414f};

  std::pair<int, int> bestNode = {sx, sz};
  float bestDist = std::hypot(static_cast<float>(gx - sx), static_cast<float>(gz - sz));
  int iters = 0;

  while (!openHeap.empty() && iters < 3000) {
    ++iters;
    Node cur = openHeap.top();
    openHeap.pop();

    float dGoal = std::hypot(static_cast<float>(gx - cur.x), static_cast<float>(gz - cur.z));
    if (dGoal < bestDist) {
      bestDist = dGoal;
      bestNode = {cur.x, cur.z};
    }
    if (dGoal <= 1.5f) {
      bestNode = {cur.x, cur.z};
      break;
    }

    for (int i = 0; i < 8; ++i) {
      int nx = cur.x + dirs[i][0];
      int nz = cur.z + dirs[i][1];
      float wx = nx * cell;
      float wz = nz * cell;

      float distStart = std::hypot(static_cast<float>(nx - sx), static_cast<float>(nz - sz));
      float distGoal = std::hypot(static_cast<float>(gx - nx), static_cast<float>(gz - nz));
      if (distStart > 1.2f && distGoal > 1.5f) {
        float tx = wx, tz = wz;
        if (ResolveCollision(tx, tz, wx, wz, 445.0f, posY) &&
            std::hypot(tx - wx, tz - wz) > 1.0f) {
          continue;
        }
      }

      float ng = cur.g + costs[i];
      auto key = std::make_pair(nx, nz);
      auto it = gScore.find(key);
      if (it == gScore.end() || ng < it->second) {
        gScore[key] = ng;
        cameFrom[key] = {cur.x, cur.z};
        float f = ng + distGoal;
        openHeap.push({f, ng, nx, nz});
      }
    }
  }

  std::vector<DirectX::XMFLOAT2> rev;
  auto curr = bestNode;
  while (cameFrom.find(curr) != cameFrom.end()) {
    rev.push_back({curr.first * cell, curr.second * cell});
    curr = cameFrom[curr];
  }
  std::reverse(rev.begin(), rev.end());
  for (const auto &pt : rev) {
    outWaypoints.push_back(pt);
  }
  outWaypoints.push_back({goalX, goalZ});
}

bool RoomManager::CrosscheckDoorEntry(float playerX, float playerY, float playerZ,
                                      float facingAngle, const RoomDoor &door) const {
  if (!m_currentRoom) return false;

  float doorCx = door.x + door.w * 0.5f;
  float doorCz = door.z + door.d * 0.5f;
  float probeX = playerX + std::sin(facingAngle) * 600.0f;
  float probeZ = playerZ + std::cos(facingAngle) * 600.0f;

  float idealAngle = std::atan2(doorCx - playerX, doorCz - playerZ);
  float angleDiff = std::atan2(std::sin(facingAngle - idealAngle), std::cos(facingAngle - idealAngle));
  float angleDiffDeg = angleDiff * (180.0f / 3.14159265f);
  float distToDoorCenter = std::hypot(doorCx - playerX, doorCz - playerZ);

  const RoomCamera *cam = GetActiveCamera();
  float chrisSx = -1.0f, chrisSy = -1.0f;
  float doorSx = -1.0f, doorSy = -1.0f;
  float probeSx = -1.0f, probeSy = -1.0f;
  bool chrisOnScreen = false;
  bool doorOnScreen = false;

  if (cam) {
    auto projectPt = [&](float wx, float wy, float wz, float &outSx, float &outSy) -> bool {
      float fx = cam->to.x - cam->from.x;
      float fy = cam->to.y - cam->from.y;
      float fz = cam->to.z - cam->from.z;
      float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
      if (flen < 1.0f) return false;
      fx /= flen; fy /= flen; fz /= flen;

      float rx = fz, ry = 0.0f, rz = -fx;
      float rlen = std::hypot(rx, rz);
      if (rlen < 1e-3f) return false;
      rx /= rlen; rz /= rlen;

      float ux = fy * rz - fz * ry;
      float uy = fz * rx - fx * rz;
      float uz = fx * ry - fy * rx;

      float dx = wx - cam->from.x;
      float dy = wy - cam->from.y;
      float dz = wz - cam->from.z;

      float vx = dx * rx + dy * ry + dz * rz;
      float vy = dx * ux + dy * uy + dz * uz;
      float vz = dx * fx + dy * fy + dz * fz;
      if (vz <= 50.0f) return false;

      outSx = 160.0f + (vx * cam->fov) / vz;
      outSy = 120.0f + (vy * cam->fov) / vz;
      return (outSx >= -32.0f && outSx <= 352.0f && outSy >= -32.0f && outSy <= 272.0f);
    };

    chrisOnScreen = projectPt(playerX, playerY - 1000.0f, playerZ, chrisSx, chrisSy);
    doorOnScreen  = projectPt(doorCx, playerY - 1000.0f, doorCz, doorSx, doorSy);
    projectPt(probeX, playerY - 1000.0f, probeZ, probeSx, probeSy);
  }

  uint32_t expectedCut = FindCameraForPosition(playerX, playerZ);
  bool cutMatchesSwitchTable = (m_activeCut == expectedCut);
  bool passed = (chrisOnScreen || doorOnScreen);

  char report[1024];
  std::snprintf(
      report, sizeof(report),
      "[DOOR ENTRY CROSSCHECK] Stage %u Room 0x%02X (Active Cut %u, SwitchTable Cut %u) -> Door Slot %u (Target: Stage %u Room 0x%02X)\n"
      "  Chris Pose   : Pos=(%.1f, %.1f, %.1f), Facing=%.1f deg (Ideal=%.1f deg, Err=%.1f deg), Dist=%.1f\n"
      "  Door 3D Box  : x=[%u..%u], z=[%u..%u], Center=(%.1f, %.1f), Probe=(%.1f, %.1f)\n"
      "  Screen 320x240: Chris=(%.1f, %.1f) [%s] | Door=(%.1f, %.1f) [%s] | Probe=(%.1f, %.1f)\n"
      "  Result       : %s\n",
      m_currentRoom->stageId, m_currentRoom->roomId, m_activeCut, expectedCut,
      door.slot, door.targetStage, door.targetRoom,
      playerX, playerY, playerZ, facingAngle * (180.0f / 3.14159265f),
      idealAngle * (180.0f / 3.14159265f), angleDiffDeg, distToDoorCenter,
      door.x, door.x + door.w, door.z, door.z + door.d, doorCx, doorCz, probeX, probeZ,
      chrisSx, chrisSy, chrisOnScreen ? "ON-SCREEN" : "OFF-SCREEN",
      doorSx, doorSy, doorOnScreen ? "ON-SCREEN" : "OFF-SCREEN",
      probeSx, probeSy,
      passed ? "PASS (3D Door & Screen Aligned)" : "WARNING (Camera/Door Alignment Check)");

  std::printf("\n%s", report);
  std::ofstream logFile("door_crosscheck.log", std::ios::app);
  if (logFile.is_open()) {
    logFile << report << "\n";
  }
  return passed;
}
