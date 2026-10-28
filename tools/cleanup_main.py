with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

import re

# Clean up broken string literals first
# Replace multi-line std::printf strings with clean single line ones
text = re.sub(r'std::printf\(\s*"\s*\n=+\s*\n"\);', 'std::printf("\\n====================================================\\n");', text)
text = re.sub(r'std::printf\("\[Item Pickup\] Collected %s \(Item ID %u\)![\r\n]+",', 'std::printf("[Item Pickup] Collected %s (Item ID %u)!\\n",', text)
text = re.sub(r'std::printf\("[\r\n]+\[Item Pickup\] Inventory Full! Cannot collect %s[\r\n]+",', 'std::printf("\\n[Item Pickup] Inventory Full! Cannot collect %s\\n",', text)
text = re.sub(r'std::printf\("[\r\n]+\[Door Unlock\] Unlocked door using %s![\r\n]+",', 'std::printf("\\n[Door Unlock] Unlocked door using %s!\\n",', text)
text = re.sub(r'std::printf\("[\r\n]+\[Door Locked\] The door is locked. Requires: %s \(ID %u\)[\r\n]+",', 'std::printf("\\n[Door Locked] The door is locked. Requires: %s (ID %u)\\n",', text)
text = re.sub(r'std::printf\("\[Door Trigger\] Interacting with Door slot %u![\r\n]+",', 'std::printf("[Door Trigger] Interacting with Door slot %u!\\n",', text)
text = re.sub(r'std::printf\("  Destination: Stage %u Room 0x%02X[\r\n]+",', 'std::printf("  Destination: Stage %u Room 0x%02X\\n",', text)
text = re.sub(r'std::printf\("  Spawn Point: \(\%\.1f, \%\.1f, \%\.1f\), Dir: \%d[\r\n]+",', 'std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",', text)
text = re.sub(r'std::printf\("\[Door Transition Complete\] Entered Stage %u Room "\s*"0x%02X at Camera Cut %u \(facing %\.1f deg\)[\r\n]+",', 'std::printf("[Door Transition Complete] Entered Stage %u Room 0x%02X at Camera Cut %u (facing %.1f deg)\\n",', text)
text = re.sub(r'std::printf\("\[Action\] No item or door in interaction range\.[\r\n]+"\);', 'std::printf("[Action] No item or door in interaction range.\\n");', text)

# Remove the bad 'continue;' and fix the extra brace
text = text.replace('s_ActionWasPressed = actionPressed;\n                continue;', '/* locked */')
text = text.replace('s_ActionWasPressed = actionPressed;\r\n                continue;', '/* locked */')

# Find the action block and ensure proper bracing
action_pattern = re.compile(r'if \(actionPressed && !s_ActionWasPressed\) \{.*?s_ActionWasPressed = actionPressed;', re.DOTALL)

clean_action_block = """if (actionPressed && !s_ActionWasPressed) {
        uint8_t pickedItemId = 0;
        if (g_ScdVM.PickupItemAtPosition(playerPos.x, playerPos.z, 600.0f, pickedItemId)) {
          const char *itemName = g_Inventory.GetItemName(pickedItemId);
          if (g_Inventory.AddItem(pickedItemId, 1)) {
            g_AudioEngine.PlayItemPickup();
            std::printf("\\n====================================================\\n");
            std::printf("[Item Pickup] Collected %s (Item ID %u)!\\n", itemName, pickedItemId);
            std::printf("====================================================\\n\\n");
          } else {
            std::printf("\\n[Item Pickup] Inventory Full! Cannot collect %s\\n", itemName);
          }
        } else {
          RoomDoor hitDoor;
          if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z, hitDoor)) {
            bool canEnter = true;
            if (hitDoor.isLocked && hitDoor.keyItem != 0) {
              if (g_Inventory.HasItem(hitDoor.keyItem)) {
                std::printf("\\n[Door Unlock] Unlocked door using %s!\\n", g_Inventory.GetItemName(hitDoor.keyItem));
                hitDoor.isLocked = false;
                g_AudioEngine.PlayDoorOpen();
              } else {
                std::printf("\\n[Door Locked] The door is locked. Requires: %s (ID %u)\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem), hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                canEnter = false;
              }
            }

            if (canEnter) {
              std::printf("\\n====================================================\\n");
              std::printf("[Door Trigger] Interacting with Door slot %u!\\n", hitDoor.slot);
              std::printf("  Destination: Stage %u Room 0x%02X\\n", hitDoor.targetStage, hitDoor.targetRoom);
              std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",
                          static_cast<float>(hitDoor.nextX),
                          static_cast<float>(hitDoor.nextY),
                          static_cast<float>(hitDoor.nextZ), hitDoor.nextDir);
              std::printf("====================================================\\n");

              g_AudioEngine.PlayDoorOpen();

              float nx = static_cast<float>(hitDoor.nextX);
              float ny = static_cast<float>(hitDoor.nextY);
              float nz = static_cast<float>(hitDoor.nextZ);
              float newAngle = (static_cast<float>(hitDoor.nextDir) / 4096.0f) * DirectX::XM_2PI;

              if (g_RoomManager.LoadRoomAtPosition(hitDoor.targetStage, hitDoor.targetRoom, nx, nz)) {
                g_Player.SetPosition(nx, ny, nz);
                g_Player.SetFacingAngle(newAngle);

                uint32_t activeCut = g_RoomManager.GetActiveCut();
                hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), activeCut);
                g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
                g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(), g_RoomManager.GetCurrentRoomId());

                const auto &roomRaw = g_RoomManager.GetCurrentRdtData();
                if (roomRaw.size() > 0x60) {
                  uint32_t initScdOff = *reinterpret_cast<const uint32_t *>(&roomRaw[0x60]);
                  g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(), initScdOff);
                }

                g_AudioEngine.PlayDoorClose();
                UpdateWindowTitle(hWnd);
                std::printf("[Door Transition Complete] Entered Stage %u Room 0x%02X at Camera Cut %u (facing %.1f deg)\\n",
                            hitDoor.targetStage, hitDoor.targetRoom, activeCut, newAngle * (180.0f / DirectX::XM_PI));
              }
            }
          } else {
            std::printf("[Action] No item or door in interaction range.\\n");
          }
        }
      }
      s_ActionWasPressed = actionPressed;"""

text = action_pattern.sub(clean_action_block, text)

# Remove any stray closing braces before section 2
text = re.sub(r's_ActionWasPressed = actionPressed;\s*\}\s*// 2\. Dynamic Camera', 's_ActionWasPressed = actionPressed;\n    }\n\n    // 2. Dynamic Camera', text)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Cleaned up main.cpp completely!")
