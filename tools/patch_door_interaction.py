import re

# 1. Patch room_manager.cpp
with open('port/src/room_manager.cpp', 'r', encoding='utf-8') as f:
    rm_content = f.read()

door_trigger_pattern = r'bool RoomManager::CheckDoorTrigger\(float playerX, float playerZ,\s*RoomDoor &outDoor\) const \{.*?return false;\s*\}'

new_door_trigger = '''bool RoomManager::CheckDoorTrigger(float playerX, float playerZ,
                                   RoomDoor &outDoor) const {
  if (!m_currentRoom || m_currentRoom->doors.empty())
    return false;

  const float margin = 500.0f; // Friendly interaction margin around trigger box (RE1 probes 600 units ahead)

  float closestDistSq = 1e18f;
  const RoomDoor* closestDoor = nullptr;

  for (const auto &door : m_currentRoom->doors) {
    float minX = static_cast<float>(
                     std::min(door.x, static_cast<int16_t>(door.x + door.w))) -
                 margin;
    float maxX = static_cast<float>(
                     std::max(door.x, static_cast<int16_t>(door.x + door.w))) +
                 margin;
    float minZ = static_cast<float>(
                     std::min(door.z, static_cast<int16_t>(door.z + door.d))) -
                 margin;
    float maxZ = static_cast<float>(
                     std::max(door.z, static_cast<int16_t>(door.z + door.d))) +
                 margin;

    if (playerX >= minX && playerX <= maxX && playerZ >= minZ && playerZ <= maxZ) {
      outDoor = door;
      return true;
    }

    float cx = door.x + door.w * 0.5f;
    float cz = door.z + door.d * 0.5f;
    float distSq = (playerX - cx) * (playerX - cx) + (playerZ - cz) * (playerZ - cz);
    if (distSq < closestDistSq) {
      closestDistSq = distSq;
      closestDoor = &door;
    }
  }

  // Also match if player is within 600 units of door center
  if (closestDoor && closestDistSq <= (600.0f * 600.0f)) {
    outDoor = *closestDoor;
    return true;
  }

  return false;
}'''

rm_new, count_rm = re.subn(door_trigger_pattern, new_door_trigger, rm_content, flags=re.DOTALL)
if count_rm > 0:
    with open('port/src/room_manager.cpp', 'w', encoding='utf-8') as f:
        f.write(rm_new)
    print('SUCCESS: Patched room_manager.cpp CheckDoorTrigger')
else:
    print('WARNING: Could not find CheckDoorTrigger pattern in room_manager.cpp')

# 2. Patch main.cpp
with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    main_content = f.read()

door_section_pattern = r'// 1\. Door Interaction Trigger Check.*?s_ActionWasPressed = actionPressed;\s*\}'

new_door_section = '''// 1. Door Interaction Trigger Check
    if (hasPlayer && g_RoomManager.HasActiveRoom()) {
      DirectX::XMFLOAT3 playerPos = g_Player.GetPosition();

      // Dual-channel keyboard + gamepad action button check (E, Enter, Space, Z, Pad Cross/Square)
      bool actionPressed = ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                           ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) ||
                           (g_Pad.buttons & (1 << 0)) ||
                           (g_Pad.buttons & (1 << 2)) ||
                           (g_Pad.buttons & (1 << 14));

      if (actionPressed && !s_ActionWasPressed) {
        std::printf("[Action] Button pressed at player pos (%.1f, %.1f, %.1f)... checking %zu doors in room\\n",
                    playerPos.x, playerPos.y, playerPos.z, g_RoomManager.GetDoors().size());

        RoomDoor hitDoor;
        if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z, hitDoor)) {
          std::printf("\\n====================================================\\n");
          std::printf("[Door Trigger] Interacting with Door slot %u!\\n", hitDoor.slot);
          std::printf("  Destination: Stage %u Room 0x%02X\\n", hitDoor.targetStage, hitDoor.targetRoom);
          std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",
                      static_cast<float>(hitDoor.nextX),
                      static_cast<float>(hitDoor.nextY),
                      static_cast<float>(hitDoor.nextZ), hitDoor.nextDir);
          std::printf("====================================================\\n");

          float nx = static_cast<float>(hitDoor.nextX);
          float ny = static_cast<float>(hitDoor.nextY);
          float nz = static_cast<float>(hitDoor.nextZ);
          float newAngle = (static_cast<float>(hitDoor.nextDir) / 4096.0f) * DirectX::XM_2PI;

          if (g_RoomManager.LoadRoomAtPosition(hitDoor.targetStage, hitDoor.targetRoom, nx, nz)) {
            // Reposition player
            g_Player.SetPosition(nx, ny, nz);
            g_Player.SetFacingAngle(newAngle);

            // Update Renderer Background for the selected camera cut pointing at the door
            uint32_t activeCut = g_RoomManager.GetActiveCut();
            hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), activeCut);
            g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
            UpdateWindowTitle(hWnd);
            std::printf("[Door Transition Complete] Entered Stage %u Room 0x%02X at Camera Cut %u (facing %.1f deg)\\n",
                        hitDoor.targetStage, hitDoor.targetRoom, activeCut,
                        newAngle * (180.0f / DirectX::XM_PI));
          }
        } else {
          std::printf("[Action] No door in interaction range.\\n");
        }
      }
      s_ActionWasPressed = actionPressed;
    }'''

main_new, count_main = re.subn(door_section_pattern, new_door_section, main_content, flags=re.DOTALL)
if count_main > 0:
    with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
        f.write(main_new)
    print('SUCCESS: Patched main.cpp door section')
else:
    print('WARNING: Could not find door section in main.cpp')
