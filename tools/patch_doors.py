import re

# 1. Patch main.cpp
with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

target_re = re.compile(
    r'      // Dual-channel keyboard \+ gamepad action button check.*?'
    r'            if \(hitDoor\.isLocked && hitDoor\.keyItem != 0\) \{'
    r'.*?'
    r'            \}',
    re.DOTALL
)

new_block = '''      // Dual-channel keyboard + gamepad action button check (E, Enter, Space,
      // Z, Pad Cross/Square)
      bool actionPressed = ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                           ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) ||
                           (g_Pad.buttons & (1 << 0)) ||
                           (g_Pad.buttons & (1 << 2)) ||
                           (g_Pad.buttons & (1 << 14));

      // Suppress action interaction while aiming / shooting weapon
      if (g_Player.IsAiming()) {
        actionPressed = false;
      }

      if (actionPressed && !s_ActionWasPressed) {
        uint8_t pickedItemId = 0;
        if (g_ScdVM.PickupItemAtPosition(playerPos.x, playerPos.z, 600.0f,
                                         pickedItemId)) {
          const char *itemName = g_Inventory.GetItemName(pickedItemId);
          if (g_Inventory.AddItem(pickedItemId, 1)) {
            g_AudioEngine.PlayItemPickup();
            std::printf(
                "\\n====================================================\\n");
            std::printf("[Item Pickup] Collected %s (Item ID %u)!\\n", itemName,
                        pickedItemId);
            std::printf(
                "====================================================\\n\\n");
          } else {
            std::printf("\\n[Item Pickup] Inventory Full! Cannot collect %s\\n",
                        itemName);
          }
        } else {
          RoomDoor hitDoor;
          if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z,
                                             g_Player.GetFacingAngle(),
                                             hitDoor)) {
            bool canEnter = true;
            if (hitDoor.isLocked) {
              if (hitDoor.keyItem != 0 && g_Inventory.HasItem(hitDoor.keyItem)) {
                std::printf("\\n[Door Unlock] Unlocked door using %s!\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem));
                hitDoor.isLocked = false;
                g_AudioEngine.PlayDoorOpen();
              } else if (hitDoor.keyItem == 0) {
                std::printf("\\n[Door Locked] The door is locked from the other side.\\n");
                g_AudioEngine.PlayDryFire();
                canEnter = false;
              } else {
                std::printf("\\n[Door Locked] The door is locked. Requires: %s "
                            "(ID %u)\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem),
                            hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                canEnter = false;
              }
            }'''

assert target_re.search(content), 'target_re did not match in main.cpp'
content = target_re.sub(new_block, content)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Successfully patched main.cpp for door triggers!')
