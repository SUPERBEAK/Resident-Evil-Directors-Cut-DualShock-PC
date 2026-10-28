import os

# 1. Patch scanScriptDoors in port/src/room_manager.cpp
rm_path = "port/src/room_manager.cpp"
with open(rm_path, "r", encoding="utf-8") as f:
    rm_content = f.read()

old_scan = """  // 3. Parse Door Records from SCD scripts (+0x60 init_script and +0x64 main_script)
  auto scanScriptDoors = [&](uint32_t scdOff) {
    if (scdOff == 0 || scdOff + 2 >= data.size()) return;
    uint16_t scriptSize = *reinterpret_cast<const uint16_t *>(&data[scdOff]);
    if (scriptSize == 0 || scdOff + 2 + scriptSize > data.size()) {
      scriptSize = static_cast<uint16_t>(std::min(data.size() - scdOff - 2, size_t(4096)));
    }
    size_t p = scdOff + 2;
    size_t end = p + scriptSize;
    while (p + 0x1A <= end) {
      if (data[p] == 0x0C) { // door_set opcode
        RoomDoor door;
        door.slot = data[p + 1];
        door.x = *reinterpret_cast<const uint16_t *>(&data[p + 2]);
        door.z = *reinterpret_cast<const uint16_t *>(&data[p + 4]);
        door.w = *reinterpret_cast<const uint16_t *>(&data[p + 6]);
        door.d = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
        door.lock = data[p + 14];

        uint8_t stageRoom = data[p + 15];
        door.targetStage = (stageRoom >> 5);
        if (door.targetStage == 0) {
          door.targetStage = room.stageId; // 0 implies same stage
        }
        door.targetRoom = (stageRoom & 0x1F);

        door.nextX = *reinterpret_cast<const uint16_t *>(&data[p + 16]);
        door.nextY = *reinterpret_cast<const int16_t *>(&data[p + 18]);
        door.nextZ = *reinterpret_cast<const uint16_t *>(&data[p + 20]);
        door.nextDir = *reinterpret_cast<const int16_t *>(&data[p + 22]);
        door.keyItem = data[p + 24];
        door.isLocked = (door.lock & 0x80) != 0; // Authentic PS1 lock mask (bit 7)

        // Filter valid doors with sensible dimensions connecting to another room
        if (door.slot < 16 && door.targetStage <= 7 && door.targetRoom < 32 &&
            (door.targetStage != room.stageId || door.targetRoom != room.roomId) &&
            door.w >= 200 && door.w <= 10000 &&
            door.d >= 200 && door.d <= 10000) {
          bool exists = false;
          for (const auto &d : room.doors) {
            if (d.slot == door.slot && d.x == door.x && d.z == door.z) {
              exists = true;
              break;
            }
          }
          if (!exists) {
            room.doors.push_back(door);
          }
        }
        p += 0x1A;
      } else {
        p++;
      }
    }
  };"""

new_scan = """  // 3. Parse Door Records from SCD scripts (+0x60 init_script and +0x64 main_script)
  auto scanScriptDoors = [&](uint32_t scdOff) {
    if (scdOff == 0 || scdOff + 2 >= data.size()) return;
    uint16_t scriptSize = *reinterpret_cast<const uint16_t *>(&data[scdOff]);
    if (scriptSize == 0 || scdOff + 2 + scriptSize > data.size()) {
      scriptSize = static_cast<uint16_t>(std::min(data.size() - scdOff - 2, size_t(4096)));
    }
    size_t p = scdOff + 2;
    size_t end = p + scriptSize;
    while (p + 1 < end) {
      uint8_t op = data[p];
      if (op == 0x00) {
        break; // block end
      } else if (op == 0x0C) { // door_set opcode (0x1A bytes)
        if (p + 0x1A > end) break;
        RoomDoor door;
        door.slot = data[p + 1];
        door.x = *reinterpret_cast<const uint16_t *>(&data[p + 2]);
        door.z = *reinterpret_cast<const uint16_t *>(&data[p + 4]);
        door.w = *reinterpret_cast<const uint16_t *>(&data[p + 6]);
        door.d = *reinterpret_cast<const uint16_t *>(&data[p + 8]);
        door.lock = data[p + 14];

        uint8_t stageRoom = data[p + 15];
        door.targetStage = (stageRoom >> 5);
        if (door.targetStage == 0) {
          door.targetStage = room.stageId; // 0 implies same stage
        }
        door.targetRoom = (stageRoom & 0x1F);

        door.nextX = *reinterpret_cast<const uint16_t *>(&data[p + 16]);
        door.nextY = *reinterpret_cast<const int16_t *>(&data[p + 18]);
        door.nextZ = *reinterpret_cast<const uint16_t *>(&data[p + 20]);
        door.nextDir = *reinterpret_cast<const int16_t *>(&data[p + 22]);
        door.keyItem = data[p + 24];
        door.isLocked = (door.lock & 0x80) != 0; // Authentic PS1 lock mask (bit 7)

        // Filter valid doors with sensible dimensions connecting to another room
        if (door.slot < 16 && door.targetStage <= 7 && door.targetRoom < 32 &&
            (door.targetStage != room.stageId || door.targetRoom != room.roomId) &&
            door.w >= 200 && door.w <= 6500 &&
            door.d >= 200 && door.d <= 6500) {
          bool exists = false;
          for (const auto &d : room.doors) {
            if (d.slot == door.slot && d.x == door.x && d.z == door.z) {
              exists = true;
              break;
            }
          }
          if (!exists) {
            room.doors.push_back(door);
          }
        }
        p += 0x1A;
      } else if (op == 0x0D) {
        p += 0x12;
      } else if (op == 0x18) {
        p += 0x1A;
      } else if (op == 0x1B) {
        p += 0x16;
      } else if (op == 0x05) {
        p += 4;
      } else if (op == 0x01) {
        p += 2;
      } else if (op == 0x04) {
        p += 4;
      } else {
        p++;
      }
    }
  };"""

rm_content = rm_content.replace(old_scan.replace("\r\n", "\n"), new_scan.replace("\r\n", "\n"))
rm_content = rm_content.replace("\r\n", "\n").replace("\n", "\r\n")

with open(rm_path, "w", encoding="utf-8") as f:
    f.write(rm_content)

print("Patched room_manager.cpp scanScriptDoors with opcode-aware parser.")

# 2. Patch main.cpp with stairs transitions
main_path = "port/src/main.cpp"
with open(main_path, "r", encoding="utf-8") as f:
    main_content = f.read()

old_door_block = """          RoomDoor hitDoor;
          if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z,
                                             g_Player.GetFacingAngle(),
                                             hitDoor)) {"""

new_door_block = """          // Authentic Main Hall Staircase Interactivity (1F <-> 2F Balcony)
          if (g_RoomManager.GetCurrentStage() == 1 && g_RoomManager.GetCurrentRoomId() == 1 &&
              playerPos.x >= 6500.0f && playerPos.x <= 9500.0f &&
              playerPos.z >= 4500.0f && playerPos.z <= 5400.0f &&
              std::cos(g_Player.GetFacingAngle()) < -0.2f) {
            std::printf("\\n[Staircase] Climbing Main Hall stairs to 2F Balcony (Stage 2 Room 0x03)...\\n");
            g_AudioEngine.PlayFootstep(0.85f);
            if (g_RoomManager.LoadRoomAtPosition(2, 3, 16800.0f, 19500.0f)) {
              g_Player.SetPosition(16800.0f, 0.0f, 19500.0f);
              g_Player.SetFacingAngle(3.14159f);
              uint32_t activeCut = g_RoomManager.GetActiveCut();
              hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), activeCut);
              g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
              g_AudioEngine.UpdateRoomBgm(2, 3);
              UpdateWindowTitle(hWnd);
              std::printf("[Staircase Complete] Arrived at Stage 2 Room 0x03 Cut %u\\n", activeCut);
            }
          } else if (g_RoomManager.GetCurrentStage() == 2 && g_RoomManager.GetCurrentRoomId() == 3 &&
                     playerPos.x >= 14000.0f && playerPos.x <= 19500.0f &&
                     playerPos.z >= 17000.0f && playerPos.z <= 21000.0f) {
            std::printf("\\n[Staircase] Descending Main Hall stairs to 1F (Stage 1 Room 0x01)...\\n");
            g_AudioEngine.PlayFootstep(0.85f);
            if (g_RoomManager.LoadRoomAtPosition(1, 1, 8200.0f, 5200.0f)) {
              g_Player.SetPosition(8200.0f, 0.0f, 5200.0f);
              g_Player.SetFacingAngle(0.0f);
              uint32_t activeCut = g_RoomManager.GetActiveCut();
              hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), activeCut);
              g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
              g_AudioEngine.UpdateRoomBgm(1, 1);
              UpdateWindowTitle(hWnd);
              std::printf("[Staircase Complete] Arrived at Stage 1 Room 0x01 Cut %u\\n", activeCut);
            }
          } else {
            RoomDoor hitDoor;
            if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z,
                                               g_Player.GetFacingAngle(),
                                               hitDoor)) {"""

# Need to balance the extra else block
old_tail = """            }
          } else {
            // Room Scenery Examination"""

new_tail = """              }
            }
          } else {
            // Room Scenery Examination"""

main_content = main_content.replace(old_door_block.replace("\r\n", "\n"), new_door_block.replace("\r\n", "\n"))
main_content = main_content.replace(old_tail.replace("\r\n", "\n"), new_tail.replace("\r\n", "\n"))
main_content = main_content.replace("\r\n", "\n").replace("\n", "\r\n")

with open(main_path, "w", encoding="utf-8") as f:
    f.write(main_content)

print("Patched main.cpp with authentic Main Hall staircase transitions.")
