with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    txt = f.read()

txt = txt.replace('''static int s_FrameCount = 0;
      s_FrameCount++;
      bool actionPressed = ((GetAsyncKeyState('E') & 0x8000) != 0) ||''',
'''bool actionPressed = ((GetAsyncKeyState('E') & 0x8000) != 0) ||''')

txt = txt.replace('''      if (actionPressed) {
        std::printf("[ActionDebug] Frame %d: actionPressed=1, wasPressed=%d, pad=0x%X\\n",
                    s_FrameCount, (int)s_ActionWasPressed, g_Pad.buttons);
      }''', '')

txt = txt.replace('''if (actionPressed && !s_ActionWasPressed) {
        std::printf("[Action] Pressed at (%.1f, %.1f), facing=%.2f rad\\n", playerPos.x, playerPos.z, g_Player.GetFacingAngle());''',
'''if (actionPressed && !s_ActionWasPressed) {''')

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(txt)

with open('port/src/room_manager.cpp', 'r', encoding='utf-8') as f:
    rtxt = f.read()

rtxt = rtxt.replace('''  std::printf("[CheckDoor] Player (%.1f, %.1f), Probe (%.1f, %.1f) vs %zu doors in room\\n", playerX, playerZ, probeX, probeZ, m_currentRoom->doors.size());
  for (const auto &door : m_currentRoom->doors) {''',
'''  for (const auto &door : m_currentRoom->doors) {''')

rtxt = rtxt.replace('''    std::printf("  Door slot %u: [%.0f..%.0f, %.0f..%.0f], probeIn=%d, playerIn=%d, bestDist=%.1f\\n",
                door.slot, minX, maxX, minZ, maxZ, probeInBox, playerInBox, std::sqrt(bestDistSq));
    if (probeInBox || playerInBox || bestDistSq <= (600.0f * 600.0f)) {''',
'''    if (probeInBox || playerInBox || bestDistSq <= (600.0f * 600.0f)) {''')

with open('port/src/room_manager.cpp', 'w', encoding='utf-8') as f:
    f.write(rtxt)

print("Cleaned debug prints from main.cpp and room_manager.cpp")
