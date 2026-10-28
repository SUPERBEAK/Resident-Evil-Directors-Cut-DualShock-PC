with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    txt = f.read()

target = 'if (actionPressed && !s_ActionWasPressed) {'
replacement = '''if (actionPressed && !s_ActionWasPressed) {
        std::printf("[Action] Pressed at (%.1f, %.1f), facing=%.2f rad\\n", playerPos.x, playerPos.z, g_Player.GetFacingAngle());'''

if target in txt:
    txt = txt.replace(target, replacement, 1)
    with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
        f.write(txt)
    print("Successfully patched main.cpp")
else:
    print("Target not found in main.cpp")
