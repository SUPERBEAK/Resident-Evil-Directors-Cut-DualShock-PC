with open('port/src/main.cpp', 'rb') as fp:
    src = fp.read().decode('utf-8').replace('\r\n', '\n')

old_load_initial = '''  // 5. Initialize Room Manager and Load Initial Room (Stage 1 Room 0 - Mansion
  // Dining Room)
  bool hasBg = false;
  if (g_RoomManager.Initialize(L"extracted/RE1-Dualshock/PSXUSA")) {
    if (g_RoomManager.LoadRoom(1, 0, 0)) {
      hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(),
                                            g_RoomManager.GetActiveCut());
      std::printf(
          "[Engine] Initialized Room Manager: Stage 1 Room 0 (Dining Room)\\n");
    }
  }

  UpdateWindowTitle(hWnd);'''

new_load_initial = '''  // Parse optional command line flags for starting room and position
  uint16_t startStage = 1;
  uint8_t startRoom = 0;
  float startX = 5274.0f, startY = 0.0f, startZ = 5652.0f;
  float startFacing = 3.14159f;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--room") == 0 && i + 2 < argc) {
      startStage = static_cast<uint16_t>(atoi(argv[i + 1]));
      startRoom = static_cast<uint8_t>(atoi(argv[i + 2]));
      if (startStage == 1 && startRoom == 1) {
        // Main Hall double doors spawn
        startX = 8700.0f; startY = 0.0f; startZ = 7900.0f;
        startFacing = 3.14159f;
      }
    } else if (strcmp(argv[i], "--pos") == 0 && i + 4 < argc) {
      startX = static_cast<float>(atof(argv[i + 1]));
      startY = static_cast<float>(atof(argv[i + 2]));
      startZ = static_cast<float>(atof(argv[i + 3]));
      startFacing = static_cast<float>(atof(argv[i + 4]));
    }
  }

  // 5. Initialize Room Manager and Load Initial Room
  bool hasBg = false;
  if (g_RoomManager.Initialize(L"extracted/RE1-Dualshock/PSXUSA")) {
    if (g_RoomManager.LoadRoom(startStage, startRoom, 0)) {
      hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(),
                                            g_RoomManager.GetActiveCut());
      g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
      g_AudioEngine.UpdateRoomBgm(startStage, startRoom);
      std::printf(
          "[Engine] Initialized Room Manager: Stage %u Room 0x%02X\\n",
          startStage, startRoom);
    }
  }

  UpdateWindowTitle(hWnd);'''

assert old_load_initial in src, 'old_load_initial not found'
src = src.replace(old_load_initial, new_load_initial)

old_player_spawn = '''        // Position Chris on the Dining Room floor
        g_Player.SetPosition(5274.0f, 0.0f, 5652.0f);
        g_Player.SetFacingAngle(3.14159f);'''

new_player_spawn = '''        // Position Chris on the room floor
        g_Player.SetPosition(startX, startY, startZ);
        g_Player.SetFacingAngle(startFacing);'''

assert old_player_spawn in src, 'old_player_spawn not found'
src = src.replace(old_player_spawn, new_player_spawn)

with open('port/src/main.cpp', 'wb') as fp:
    fp.write(src.replace('\n', '\r\n').encode('utf-8'))

print('Patched main.cpp with command line room support successfully')
