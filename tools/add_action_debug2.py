with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    txt = f.read()

target = 'bool actionPressed = ((GetAsyncKeyState(\'E\') & 0x8000) != 0) ||'
replacement = '''static int s_FrameCount = 0;
      s_FrameCount++;
      bool actionPressed = ((GetAsyncKeyState('E') & 0x8000) != 0) ||'''

txt = txt.replace(target, replacement, 1)

target2 = 'if (g_Player.IsAiming()) {\n actionPressed = false;\n }'
replacement2 = '''if (g_Player.IsAiming()) {
        actionPressed = false;
      }
      if (actionPressed) {
        std::printf("[ActionDebug] Frame %d: actionPressed=1, wasPressed=%d, pad=0x%X\\n",
                    s_FrameCount, (int)s_ActionWasPressed, g_Pad.buttons);
      }'''

txt = txt.replace('if (g_Player.IsAiming()) {\n\nactionPressed = false;\n\n}', replacement2)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(txt)

print("Added ActionDebug to main.cpp")
