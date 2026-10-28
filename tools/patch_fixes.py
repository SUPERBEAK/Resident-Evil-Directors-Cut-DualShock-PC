import os

# 1. Patch psx_str.cpp
with open('port/src/hal/psx_str.cpp', 'r', encoding='utf-8') as f:
    str_cpp = f.read()

if '#include <filesystem>' not in str_cpp:
    str_cpp = '#include <filesystem>\n' + str_cpp

target_open = """bool PSXMoviePlayer::Open(const std::string& filePath, float targetFps) {
    Close();
    m_File.open(filePath, std::ios::binary);"""

replace_open = """bool PSXMoviePlayer::Open(const std::string& filePath, float targetFps) {
    Close();
    std::string resolvedPath = filePath;
    if (!std::filesystem::exists(resolvedPath)) {
        std::string cand1 = std::string("../../../") + filePath;
        std::string cand2 = std::string("../../") + filePath;
        if (std::filesystem::exists(cand1)) {
            resolvedPath = cand1;
        } else if (std::filesystem::exists(cand2)) {
            resolvedPath = cand2;
        }
    }
    m_File.open(resolvedPath, std::ios::binary);"""

if target_open in str_cpp:
    str_cpp = str_cpp.replace(target_open, replace_open, 1)
    with open('port/src/hal/psx_str.cpp', 'w', encoding='utf-8') as f:
        f.write(str_cpp)
    print("psx_str.cpp successfully patched with path resolution!")
else:
    print("target_open not found in psx_str.cpp!")

# 2. Patch main.cpp
with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    main_cpp = f.read()

# A. Add --test-puzzle CLI argument
target_args = '} else if (strcmp(argv[argi], "--test-door") == 0) {'
replace_args = """} else if (strcmp(argv[argi], "--test-puzzle") == 0) {
    g_Inventory.AddItem(35, 1); // Wind Crest
    g_Inventory.AddItem(36, 1); // Star Crest
    g_Inventory.AddItem(37, 1); // Sun Crest
    g_Inventory.AddItem(38, 1); // Moon Crest
    g_Inventory.AddItem(46, 1); // Musical Score
    g_Inventory.AddItem(44, 1); // Wooden Emblem
} else if (strcmp(argv[argi], "--test-door") == 0) {"""

if target_args in main_cpp and '--test-puzzle' not in main_cpp:
    main_cpp = main_cpp.replace(target_args, replace_args, 1)
    print("Added --test-puzzle to main.cpp")

# B. Add pending used item handling
target_inv_update = """} else if (g_Inventory.IsOpen()) {

g_Inventory.Update(1.0f / 60.0f, g_Pad.buttons);

} else {"""

replace_inv_update = """} else if (g_Inventory.IsOpen()) {

g_Inventory.Update(1.0f / 60.0f, g_Pad.buttons);

}

if (uint8_t pendingItem = g_Inventory.GetPendingUsedItemId()) {
  DirectX::XMFLOAT3 pPos = g_Player.GetPosition();
  auto res = Game::g_PuzzleManager.TryUseItem(pendingItem, static_cast<uint8_t>(g_RoomManager.GetCurrentStage()), g_RoomManager.GetCurrentRoomId(), pPos.x, pPos.z);
  if (res.handled) {
    if (res.success) {
      g_Inventory.RemoveItem(g_Inventory.GetPendingUsedSlotIdx(), 1);
      if (res.awardItemId > 0) {
        g_Inventory.AddItem(res.awardItemId, res.awardCount);
      }
      g_AudioEngine.PlayMenuConfirm();
    } else {
      g_AudioEngine.PlayMenuCancel();
    }
    Game::g_TextBox.ShowMessage(res.message, false);
  }
  g_Inventory.ClearPendingUsedItem();
}

if (!g_Inventory.IsOpen() && !Game::g_ItemBox.IsOpen() && !Game::g_TextBox.IsActive() && !Game::g_DoorTransition.IsActive()) {"""

if target_inv_update in main_cpp:
    main_cpp = main_cpp.replace(target_inv_update, replace_inv_update, 1)
    print("Added pending used item handler to main.cpp")
else:
    print("target_inv_update not found in main.cpp!")

# C. Add puzzle examination fallback
target_exam = """} else {
              const auto &rdt = g_RoomManager.GetCurrentRdtData();
              Game::g_TextBox.ShowRoomMessage(0, rdt);
            }"""

replace_exam = """} else {
              std::string pDesc = Game::g_PuzzleManager.CheckPuzzleZone(
                  static_cast<uint8_t>(g_RoomManager.GetCurrentStage()),
                  g_RoomManager.GetCurrentRoomId(),
                  playerPos.x, playerPos.z);
              if (!pDesc.empty()) {
                Game::g_TextBox.ShowMessage(pDesc, false);
              } else {
                const auto &rdt = g_RoomManager.GetCurrentRdtData();
                Game::g_TextBox.ShowRoomMessage(0, rdt);
              }
            }"""

if target_exam in main_cpp:
    main_cpp = main_cpp.replace(target_exam, replace_exam, 1)
    print("Added puzzle examination fallback to main.cpp")
else:
    print("target_exam not found in main.cpp!")

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(main_cpp)

print("Fixes applied successfully.")
