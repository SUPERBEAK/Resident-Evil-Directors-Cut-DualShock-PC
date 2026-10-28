import re

with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Patch Audio Init
audio_target = re.compile(r'g_AudioEngine\.UpdateRoomBgm\(g_RoomManager\.GetCurrentStage\(\),\s*g_RoomManager\.GetCurrentRoomId\(\)\);\s*\}', re.DOTALL)

audio_replacement = """g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),
                                  g_RoomManager.GetCurrentRoomId());
    }

    std::wstring doorHedCandidates[] = {
        L"extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",
        L"../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",
        L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",
        L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",
        L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED"
    };
    std::wstring doorVbCandidates[] = {
        L"extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",
        L"../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",
        L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",
        L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",
        L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB"
    };
    std::wstring dHed = L"", dVb = L"";
    for (const auto &p : doorHedCandidates) { if (std::filesystem::exists(p)) { dHed = p; break; } }
    for (const auto &p : doorVbCandidates) { if (std::filesystem::exists(p)) { dVb = p; break; } }
    if (!dHed.empty() && !dVb.empty()) {
      g_AudioEngine.LoadDoorAudio(dHed, dVb);
    }

    std::wstring selectHedCandidates[] = {
        L"extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",
        L"../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",
        L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",
        L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",
        L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED"
    };
    std::wstring selectVbCandidates[] = {
        L"extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",
        L"../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",
        L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",
        L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",
        L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB"
    };
    std::wstring sHed = L"", sVb = L"";
    for (const auto &p : selectHedCandidates) { if (std::filesystem::exists(p)) { sHed = p; break; } }
    for (const auto &p : selectVbCandidates) { if (std::filesystem::exists(p)) { sVb = p; break; } }
    if (!sHed.empty() && !sVb.empty()) {
      g_AudioEngine.LoadSelectAudio(sHed, sVb);
    }
  }"""

text, c1 = audio_target.subn(audio_replacement, text, count=1)
print(f"Patched audio init: {c1}")

# 2. Patch Action Button Handler
action_target = re.compile(r'if \(actionPressed && !s_ActionWasPressed\) \{.*?s_ActionWasPressed = actionPressed;', re.DOTALL)

action_replacement = """if (actionPressed && !s_ActionWasPressed) {
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
            if (hitDoor.isLocked && hitDoor.keyItem != 0) {
              if (g_Inventory.HasItem(hitDoor.keyItem)) {
                std::printf("\\n[Door Unlock] Unlocked door using %s!\\n", g_Inventory.GetItemName(hitDoor.keyItem));
                hitDoor.isLocked = false;
                g_AudioEngine.PlayDoorOpen();
              } else {
                std::printf("\\n[Door Locked] The door is locked. Requires: %s (ID %u)\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem), hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                s_ActionWasPressed = actionPressed;
                continue;
              }
            }

            std::printf(
                "\\n====================================================\\n");
            std::printf("[Door Trigger] Interacting with Door slot %u!\\n",
                        hitDoor.slot);
            std::printf("  Destination: Stage %u Room 0x%02X\\n",
                        hitDoor.targetStage, hitDoor.targetRoom);
            std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",
                        static_cast<float>(hitDoor.nextX),
                        static_cast<float>(hitDoor.nextY),
                        static_cast<float>(hitDoor.nextZ), hitDoor.nextDir);
            std::printf("====================================================\\n");

            g_AudioEngine.PlayDoorOpen();

            float nx = static_cast<float>(hitDoor.nextX);
            float ny = static_cast<float>(hitDoor.nextY);
            float nz = static_cast<float>(hitDoor.nextZ);
            float newAngle =
                (static_cast<float>(hitDoor.nextDir) / 4096.0f) * DirectX::XM_2PI;

            if (g_RoomManager.LoadRoomAtPosition(hitDoor.targetStage,
                                                 hitDoor.targetRoom, nx, nz)) {
              g_Player.SetPosition(nx, ny, nz);
              g_Player.SetFacingAngle(newAngle);

              uint32_t activeCut = g_RoomManager.GetActiveCut();
              hasBg = g_Renderer.LoadRoomBackground(
                  g_RoomManager.GetCurrentBssPath(), activeCut);
              g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
              g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),
                                          g_RoomManager.GetCurrentRoomId());

              const auto &roomRaw = g_RoomManager.GetCurrentRdtData();
              if (roomRaw.size() > 0x60) {
                uint32_t initScdOff = *reinterpret_cast<const uint32_t *>(&roomRaw[0x60]);
                g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(), initScdOff);
              }

              g_AudioEngine.PlayDoorClose();
              UpdateWindowTitle(hWnd);
              std::printf("[Door Transition Complete] Entered Stage %u Room "
                          "0x%02X at Camera Cut %u (facing %.1f deg)\\n",
                          hitDoor.targetStage, hitDoor.targetRoom, activeCut,
                          newAngle * (180.0f / DirectX::XM_PI));
            }
          } else {
            std::printf("[Action] No item or door in interaction range.\\n");
          }
        }
      }
      s_ActionWasPressed = actionPressed;"""

text, c2 = action_target.subn(action_replacement, text, count=1)
print(f"Patched action handler: {c2}")

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("Saved patched main.cpp")
