with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# Add --test-itembox
old_arg = 'Game::g_TextBox.ShowMessage("Will you take the\\nCombat Knife?", true);\n\n}'
new_arg = '''Game::g_TextBox.ShowMessage("Will you take the\\nCombat Knife?", true);

} else if (strcmp(argv[argi], "--test-itembox") == 0) {

Game::g_ItemBox.Open();

}'''
assert old_arg in code, "old_arg not found"
code = code.replace(old_arg, new_arg, 1)

# Add O hotkey
old_hotkeys = 'static bool s_PrevT = false, s_PrevY = false, s_PrevU = false;'
new_hotkeys = 'static bool s_PrevT = false, s_PrevY = false, s_PrevU = false, s_PrevO = false;'
code = code.replace(old_hotkeys, new_hotkeys, 1)

old_o = 's_PrevT = tDown; s_PrevY = yDown; s_PrevU = uDown;'
new_o = '''bool oDown = (GetAsyncKeyState('O') & 0x8000) != 0;
if (oDown && !s_PrevO) {
  if (Game::g_ItemBox.IsOpen()) Game::g_ItemBox.Close();
  else Game::g_ItemBox.Open();
}
s_PrevO = oDown;
s_PrevT = tDown; s_PrevY = yDown; s_PrevU = uDown;'''
code = code.replace(old_o, new_o, 1)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Added --test-itembox and 'O' hotkey to main.cpp")
