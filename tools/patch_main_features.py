import os

with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Includes
target_inc = '#include "save_manager.h"'
replace_inc = '#include "save_manager.h"\n#include "psx_str.h"\n#include "puzzle_manager.h"'
if target_inc in text and 'psx_str.h' not in text:
    text = text.replace(target_inc, replace_inc, 1)

# 2. Subsystem initialization
target_init = 'Game::g_ItemBox.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), &g_Inventory);'
replace_init = '''Game::g_ItemBox.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), &g_Inventory);
PSX::g_MoviePlayer.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), g_AudioEngine.GetXAudio2());
Game::g_PuzzleManager.Initialize(&g_Inventory, nullptr);'''
if target_init in text and 'g_MoviePlayer.Initialize' not in text:
    text = text.replace(target_init, replace_init, 1)

# 3. Command-line args for movie playback
target_arg = '} else if (strcmp(argv[argi], "--test-itembox") == 0) {'
replace_arg = '''} else if (strcmp(argv[argi], "--play-movie") == 0 || strcmp(argv[argi], "--movie") == 0) {
int mId = (argi + 1 < argc) ? atoi(argv[++argi]) : 0;
PSX::g_MoviePlayer.OpenMovie(static_cast<PSX::MovieId>(mId));
} else if (strcmp(argv[argi], "--test-itembox") == 0) {'''
if target_arg in text and '--play-movie' not in text:
    text = text.replace(target_arg, replace_arg, 1)

# 4. Movie hotkey 'P'
target_hotkey = 's_PrevO = oDown;'
replace_hotkey = '''s_PrevO = oDown;

static bool s_PrevP = false;
bool pDown = (GetAsyncKeyState('P') & 0x8000) != 0;
if (pDown && !s_PrevP) {
  if (PSX::g_MoviePlayer.IsPlaying()) {
    PSX::g_MoviePlayer.Close();
  } else {
    static int s_MovieToggle = 0;
    if (s_MovieToggle == 0) {
      PSX::g_MoviePlayer.OpenMovie(PSX::MOVIE_CAPCOM);
      s_MovieToggle = 1;
    } else {
      PSX::g_MoviePlayer.OpenMovie(PSX::MOVIE_INTRO);
      s_MovieToggle = 0;
    }
  }
}
s_PrevP = pDown;'''
if target_hotkey in text and 's_PrevP' not in text:
    text = text.replace(target_hotkey, replace_hotkey, 1)

# 5. Update loop movie
target_update = 'if (Game::g_DoorTransition.IsActive()) {'
replace_update = '''if (PSX::g_MoviePlayer.IsPlaying()) {
Game::g_MoviePlayer.Update(1.0f / 60.0f, g_Pad.buttons);
} else if (Game::g_DoorTransition.IsActive()) {'''
if target_update in text and 'g_MoviePlayer.Update' not in text:
    text = text.replace(target_update, replace_update, 1)

# 6. Render loop movie
target_render = 'g_Renderer.BeginFrame();'
replace_render = '''g_Renderer.BeginFrame();

if (PSX::g_MoviePlayer.IsPlaying()) {
  PSX::g_MoviePlayer.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline());
  g_Renderer.EndFrame();
  frameCount++;
  continue;
}'''
if target_render in text and 'g_MoviePlayer.Render' not in text:
    text = text.replace(target_render, replace_render, 1)

# 7. Inventory pending item usage check
target_inv = '''g_Inventory.Update(1.0f / 60.0f, g_Pad.buttons);
} else {'''
replace_inv = '''g_Inventory.Update(1.0f / 60.0f, g_Pad.buttons);
if (uint8_t pendingItem = g_Inventory.GetPendingUsedItemId()) {
  DirectX::XMFLOAT3 pPos = g_Player.GetPosition();
  auto res = Game::g_PuzzleManager.TryUseItem(pendingItem, g_RoomManager.GetCurrentStage(), g_RoomManager.GetCurrentRoomId(), pPos.x, pPos.z);
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
} else {'''
if target_inv in text and 'GetPendingUsedItemId' not in text:
    text = text.replace(target_inv, replace_inv, 1)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print('tools/patch_main_features.py successfully executed!')
