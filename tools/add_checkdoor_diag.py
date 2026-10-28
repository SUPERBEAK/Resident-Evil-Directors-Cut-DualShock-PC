with open('port/src/room_manager.cpp', 'r', encoding='utf-8') as f:
    txt = f.read()

target = '  for (const auto &door : m_currentRoom->doors) {'
replacement = '''  std::printf("[CheckDoor] Player (%.1f, %.1f), Probe (%.1f, %.1f) vs %zu doors in room\\n", playerX, playerZ, probeX, probeZ, m_currentRoom->doors.size());
  for (const auto &door : m_currentRoom->doors) {'''

if target in txt:
    txt = txt.replace(target, replacement, 1)

target2 = '''    if (probeInBox || playerInBox || bestDistSq <= (600.0f * 600.0f)) {'''
replacement2 = '''    std::printf("  Door slot %u: [%.0f..%.0f, %.0f..%.0f], probeIn=%d, playerIn=%d, bestDist=%.1f\\n",
                door.slot, minX, maxX, minZ, maxZ, probeInBox, playerInBox, std::sqrt(bestDistSq));
    if (probeInBox || playerInBox || bestDistSq <= (600.0f * 600.0f)) {'''

if target2 in txt:
    txt = txt.replace(target2, replacement2, 1)

with open('port/src/room_manager.cpp', 'w', encoding='utf-8') as f:
    f.write(txt)

print("Added CheckDoor diagnostics to room_manager.cpp")
