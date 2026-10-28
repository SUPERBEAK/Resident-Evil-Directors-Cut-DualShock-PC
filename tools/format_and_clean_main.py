import re

with open('port/src/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

# Strip any lines that are just \r or empty when stripped
cleaned = []
for line in lines:
    stripped = line.strip()
    if not stripped:
        if cleaned and cleaned[-1] != '':
            cleaned.append('')
    else:
        cleaned.append(stripped)

text = '\n'.join(cleaned)

action_block = """      if (actionPressed && !s_ActionWasPressed) {
        uint8_t pickedItemId = 0;
        if (g_ScdVM.PickupItemAtPosition(playerPos.x, playerPos.z, 600.0f, pickedItemId)) {
          const char *itemName = g_Inventory.GetItemName(pickedItemId);
          if (g_Inventory.AddItem(pickedItemId, 1)) {
            g_AudioEngine.PlayItemPickup();
            char pickupMsg[128];
            snprintf(pickupMsg, sizeof(pickupMsg), "You found a\\n%s.", itemName);
            Game::g_TextBox.ShowMessage(pickupMsg, false);
            std::printf("\\n====================================================\\n");
            std::printf("[Item Pickup] Collected %s (Item ID %u)!\\n", itemName, pickedItemId);
            std::printf("====================================================\\n\\n");
          } else {
            Game::g_TextBox.ShowMessage("You can't carry any more\\nitems.", false);
            std::printf("\\n[Item Pickup] Inventory Full! Cannot collect %s\\n", itemName);
          }
        } else if (g_RoomManager.GetCurrentStage() == 1 && g_RoomManager.GetCurrentRoomId() == 1 &&
                   playerPos.x >= 6500.0f && playerPos.x <= 9500.0f &&
                   playerPos.z >= 4500.0f && playerPos.z <= 5400.0f &&
                   std::cos(g_Player.GetFacingAngle()) < -0.2f) {
          // Authentic Main Hall Staircase Interactivity (1F -> 2F Balcony)
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
          // Authentic Main Hall Staircase Interactivity (2F Balcony -> 1F)
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
                                             hitDoor)) {
            if (hitDoor.isLocked) {
              if (hitDoor.keyItem != 0 && g_Inventory.HasItem(hitDoor.keyItem)) {
                std::printf("\\n[Door Unlock] Unlocked door using %s!\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem));
                hitDoor.isLocked = false;
                g_AudioEngine.PlayDoorOpen();
                Game::g_TextBox.ShowMessage(
                    "\\nYou unlocked it.", false,
                    [=, &hasBg, &hWnd](bool) {
                      float nx = static_cast<float>(hitDoor.nextX);
                      float ny = static_cast<float>(hitDoor.nextY);
                      float nz = static_cast<float>(hitDoor.nextZ);
                      float newAngle = (static_cast<float>(hitDoor.nextDir) / 4096.0f) *
                                       DirectX::XM_2PI;

                      Game::g_DoorTransition.StartTransition(
                          hitDoor.slot, 0, [=, &hasBg, &hWnd]() {
                            if (g_RoomManager.LoadRoomAtPosition(
                                    hitDoor.targetStage, hitDoor.targetRoom,
                                    nx, nz)) {
                              g_Player.SetPosition(nx, ny, nz);
                              g_Player.SetFacingAngle(newAngle);

                              uint32_t activeCut = g_RoomManager.GetActiveCut();
                              hasBg = g_Renderer.LoadRoomBackground(
                                  g_RoomManager.GetCurrentBssPath(), activeCut);
                              g_AudioEngine.LoadRoomAudio(
                                  g_RoomManager.GetCurrentRdtPath());
                              g_AudioEngine.UpdateRoomBgm(
                                  g_RoomManager.GetCurrentStage(),
                                  g_RoomManager.GetCurrentRoomId());

                              const auto &roomRaw =
                                  g_RoomManager.GetCurrentRdtData();
                              if (roomRaw.size() > 0x60) {
                                uint32_t initScdOff =
                                    *reinterpret_cast<const uint32_t *>(
                                        &roomRaw[0x60]);
                                g_ScdVM.ExecuteInitScript(roomRaw.data(),
                                                          roomRaw.size(),
                                                          initScdOff);
                              }

                              g_AudioEngine.PlayDoorClose();
                              UpdateWindowTitle(hWnd);
                              std::printf("[Door Transition Complete] Entered "
                                          "Stage %u Room 0x%02X at Camera Cut %u\\n",
                                          hitDoor.targetStage,
                                          hitDoor.targetRoom, activeCut);
                            }
                          });
                    });
              } else if (hitDoor.keyItem == 0) {
                std::printf("\\n[Door Locked] The door is locked from the other side.\\n");
                g_AudioEngine.PlayDryFire();
                Game::g_TextBox.ShowMessage("\\nIt's locked from inside.", false);
              } else {
                std::printf("\\n[Door Locked] The door is locked. Requires: %s (ID %u)\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem),
                            hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                if (hitDoor.keyItem == 51) {
                  Game::g_TextBox.ShowMessage("\\nIt's locked.\\nA carving of a sword.", false);
                } else if (hitDoor.keyItem == 52) {
                  Game::g_TextBox.ShowMessage("\\nIt's locked.\\nA carving of an armor.", false);
                } else if (hitDoor.keyItem == 53) {
                  Game::g_TextBox.ShowMessage("\\nIt's locked.\\nA carving of a shield.", false);
                } else if (hitDoor.keyItem == 54) {
                  Game::g_TextBox.ShowMessage("\\nIt's locked.\\nA carving of a helmet.", false);
                } else {
                  std::string lockMsg = "\\nIt's locked.\\nRequires the " +
                                        std::string(g_Inventory.GetItemName(hitDoor.keyItem)) + ".";
                  Game::g_TextBox.ShowMessage(lockMsg, false);
                }
              }
            } else {
              // Unlocked door: Start 3D authentic door transition sequence!
              std::printf("\\n====================================================\\n");
              std::printf("[Door Transition] Starting 3D sequence for Door %u!\\n",
                          hitDoor.slot);
              std::printf("  Destination: Stage %u Room 0x%02X\\n",
                          hitDoor.targetStage, hitDoor.targetRoom);
              std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",
                          static_cast<float>(hitDoor.nextX),
                          static_cast<float>(hitDoor.nextY),
                          static_cast<float>(hitDoor.nextZ), hitDoor.nextDir);
              std::printf("====================================================\\n");

              float nx = static_cast<float>(hitDoor.nextX);
              float ny = static_cast<float>(hitDoor.nextY);
              float nz = static_cast<float>(hitDoor.nextZ);
              float newAngle = (static_cast<float>(hitDoor.nextDir) / 4096.0f) *
                               DirectX::XM_2PI;

              Game::g_DoorTransition.StartTransition(
                  hitDoor.slot, 0, [=, &hasBg, &hWnd]() {
                    if (g_RoomManager.LoadRoomAtPosition(
                            hitDoor.targetStage, hitDoor.targetRoom, nx, nz)) {
                      g_Player.SetPosition(nx, ny, nz);
                      g_Player.SetFacingAngle(newAngle);

                      uint32_t activeCut = g_RoomManager.GetActiveCut();
                      hasBg = g_Renderer.LoadRoomBackground(
                          g_RoomManager.GetCurrentBssPath(), activeCut);
                      g_AudioEngine.LoadRoomAudio(
                          g_RoomManager.GetCurrentRdtPath());
                      g_AudioEngine.UpdateRoomBgm(
                          g_RoomManager.GetCurrentStage(),
                          g_RoomManager.GetCurrentRoomId());

                      const auto &roomRaw = g_RoomManager.GetCurrentRdtData();
                      if (roomRaw.size() > 0x60) {
                        uint32_t initScdOff =
                            *reinterpret_cast<const uint32_t *>(
                                &roomRaw[0x60]);
                        g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(),
                                                  initScdOff);
                      }

                      g_AudioEngine.PlayDoorClose();
                      UpdateWindowTitle(hWnd);
                      std::printf("[Door Transition Complete] Entered Stage %u "
                                  "Room 0x%02X at Camera Cut %u (facing %.1f deg)\\n",
                                  hitDoor.targetStage, hitDoor.targetRoom,
                                  activeCut, newAngle * (180.0f / DirectX::XM_PI));
                    }
                  });
            }
          } else {
            // Room Scenery Examination
            if (g_RoomManager.GetCurrentStage() == 1 &&
                g_RoomManager.GetCurrentRoomId() == 0) {
              if (playerPos.x < 1500.0f && playerPos.z > 1800.0f) {
                Game::g_TextBox.ShowMessage(
                    "A grand fireplace.\\nEmbers are still softly glowing.", false);
              } else if (playerPos.z < 1500.0f) {
                Game::g_TextBox.ShowMessage(
                    "A grandfather clock.\\nThe pendulum has stopped.", false);
              } else {
                Game::g_TextBox.ShowMessage(
                    "A long mahogany dining table.\\nDishes and silverware are neatly placed.",
                    false);
              }
            } else {
              const auto &rdt = g_RoomManager.GetCurrentRdtData();
              Game::g_TextBox.ShowRoomMessage(0, rdt);
            }
          }
        }
      }
      s_ActionWasPressed = actionPressed;
    }"""

# Use regex to find from `if (actionPressed && !s_ActionWasPressed) {` to `// 2. Dynamic Camera Switch`
pattern = re.compile(r'if \(actionPressed && !s_ActionWasPressed\) \{.*?(?=// 2\. Dynamic Camera Switch)', re.DOTALL)
text = pattern.sub(action_block + '\n\n    ', text)

with open('port/src/main.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)

print("Formatted port/src/main.cpp successfully!")
