#include "audio_engine.h"

#include "door_transition.h"

#include "dx12_renderer.h"

#include "enemy_zombie.h"

#include "inventory_system.h"

#include "player_character.h"

#include "psx_gte.h"

#include "psx_memory.h"

#include "psx_pad.h"

#include "room_manager.h"

#include "scd_vm.h"

#include "stage_manager.h"

#include "text_box.h"
#include "item_box.h"
#include "save_manager.h"
#include "psx_str.h"
#include "puzzle_manager.h"
#include "title_screen.h"

#include <cstdio>

#include <filesystem>

#include <iostream>

#include <windows.h>

static Render::DX12Renderer g_Renderer;

static StageManager g_StageManager;

static RoomManager g_RoomManager;
// --- F11 In-Game Automated Door Walker Bot State ---
static bool g_AutoDoorWalker = false;
static int g_AutoTargetDoorIdx = -1;
static std::vector<DirectX::XMFLOAT2> g_AutoWaypoints;
static size_t g_AutoWaypointIdx = 0;
static int g_AutoStuckFrames = 0;
static DirectX::XMFLOAT3 g_AutoLastPos = {0.0f, 0.0f, 0.0f};
static std::vector<uint32_t> g_AutoVisitedDoors; // (stage << 16) | (room << 8) | slot
static std::vector<uint16_t> g_AutoVisitedRooms; // (stage << 8) | room
static uint16_t g_AutoPrevStage = 0;
static uint8_t g_AutoPrevRoom = 0xFF;


static Game::PlayerCharacter g_Player;

static Game::EnemyZombie g_Zombie;

static Game::InventorySystem g_Inventory;

static Game::ScdVirtualMachine g_ScdVM;

static bool g_IsRunning = true;

static PAD g_Pad;


static void UpdateWindowTitle(HWND hWnd);
static uint32_t s_SaveCount = 0;

static void PerformSaveGame() {
  Game::GameSaveState state = {};
  state.saveIndex = ++s_SaveCount;
  state.stage = static_cast<uint8_t>(g_RoomManager.GetCurrentStage());
  state.room = g_RoomManager.GetCurrentRoomId();
  state.cameraCut = static_cast<uint8_t>(g_RoomManager.GetActiveCut());

  DirectX::XMFLOAT3 pos = g_Player.GetPosition();
  state.posX = pos.x;
  state.posY = pos.y;
  state.posZ = pos.z;
  state.facingAngle = g_Player.GetFacingAngle();
  state.health = g_Player.GetHealth();
  state.maxHealth = g_Player.GetMaxHealth();
  state.isArmed = g_Player.IsWeaponEquipped();
  state.equippedSlot = g_Inventory.GetEquippedSlot();

  const auto &invSlots = g_Inventory.GetSlots();
  for (size_t i = 0; i < 6 && i < invSlots.size(); ++i) {
    state.inventory[i].itemId = invSlots[i].itemId;
    state.inventory[i].count = invSlots[i].count;
  }

  const auto &boxSlots = Game::g_ItemBox.GetBoxSlots();
  state.boxCount = static_cast<uint32_t>(boxSlots.size());
  for (size_t i = 0; i < 32 && i < boxSlots.size(); ++i) {
    state.boxItems[i].itemId = boxSlots[i].itemId;
    state.boxItems[i].count = boxSlots[i].count;
  }

  for (size_t b = 0; b < 10; ++b) {
    state.scdFlags[b] = g_ScdVM.GetFlagBank(b);
  }

  if (Game::SaveManager::SaveToFile(0, state)) {
    Game::g_TextBox.ShowMessage("Game saved successfully.", false);
  } else {
    Game::g_TextBox.ShowMessage("Error recording save data.", false);
  }
}

static void PerformLoadGame(HWND hWnd, bool &hasBg) {
  if (!Game::SaveManager::DoesSaveExist(0)) {
    Game::g_TextBox.ShowMessage("No save data found.", false);
    g_AudioEngine.PlayMenuCancel();
    return;
  }

  Game::GameSaveState state;
  if (!Game::SaveManager::LoadFromFile(0, state)) {
    Game::g_TextBox.ShowMessage("Corrupt save data file.", false);
    g_AudioEngine.PlayMenuCancel();
    return;
  }

  g_AudioEngine.PlayMenuConfirm();

  // Restore Player
  g_Player.SetPosition(state.posX, state.posY, state.posZ);
  g_Player.SetFacingAngle(state.facingAngle);
  g_Player.SetHealth(state.health);
  g_Player.SetMaxHealth(state.maxHealth);
  g_Player.SetArmed(state.isArmed);

  // Restore Inventory
  for (size_t i = 0; i < 6; ++i) {
    g_Inventory.SetSlot(i, state.inventory[i].itemId, state.inventory[i].count);
  }
  g_Inventory.SetEquippedSlot(state.equippedSlot);

  // Restore Item Box
  for (size_t i = 0; i < 32; ++i) {
    Game::g_ItemBox.SetBoxSlot(i, state.boxItems[i].itemId, state.boxItems[i].count);
  }

  // Restore SCD Flags
  for (size_t b = 0; b < 10; ++b) {
    g_ScdVM.SetFlagBank(b, state.scdFlags[b]);
  }

  // Restore Room & Background
  if (g_RoomManager.LoadRoom(state.stage, state.room, state.cameraCut)) {
    hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), state.cameraCut);
    g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
    g_AudioEngine.UpdateRoomBgm(state.stage, state.room);
    const auto &roomRaw = g_RoomManager.GetCurrentRdtData();
    if (roomRaw.size() > 0x60) {
      uint32_t initScdOff = *reinterpret_cast<const uint32_t *>(&roomRaw[0x60]);
      g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(), initScdOff);
    }
    UpdateWindowTitle(hWnd);
  }

  Game::g_TextBox.ShowMessage("Game loaded successfully.", false);
}

static void UpdateWindowTitle(HWND hWnd) {

wchar_t title[384];

uint8_t bgmBank = g_AudioEngine.GetCurrentBgmBank();

const wchar_t *aimStr = L"";

if (g_Player.IsAiming()) {

aimStr = (g_Player.GetAimPitch() == Game::AimPitch::Up)

? L" [AIM: UP]"

: (g_Player.GetAimPitch() == Game::AimPitch::Down

? L" [AIM: DOWN]"

: L" [AIM: STRAIGHT]");

}

static wchar_t zombieStr[64] = L"";

zombieStr[0] = L'\0';

if (g_RoomManager.GetCurrentStage() == 1 &&

g_RoomManager.GetCurrentRoomId() == 0 &&

g_Zombie.IsLoaded()) {

if (g_Zombie.IsAlive()) {

const wchar_t *st =

(g_Zombie.GetState() == Game::ZombieAnimState::Shamble)

? L"Shamble"

: ((g_Zombie.GetState() == Game::ZombieAnimState::Lunge)

? L"Lunge"

: ((g_Zombie.GetState() == Game::ZombieAnimState::Bite)

? L"BITE"

: ((g_Zombie.GetState() ==

Game::ZombieAnimState::HitRecoil)

? L"STAGGER"

: L"Idle")));

swprintf_s(zombieStr, L" [Zombie: %d HP | %ls]", g_Zombie.GetHealth(),

st);

} else {

wcscpy_s(zombieStr, L" [Zombie: Defeated]");

}

}

if (bgmBank != 0xFF) {

swprintf_s(title,

L"Resident Evil Dual Shock (DX12) - Stage %u Room 0x%02X "

L"[Beretta 92FS: %d/%d]%ls%ls "

L"[BGM: SEP%02X.HSB (%u/57)] [Upscaler: %ls] (F/R-Click: Aim | "

L"Enter/L-Click: Shoot | R: Reload)",

g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId(), g_Player.GetAmmo(),

g_Player.GetMaxAmmo(), aimStr, zombieStr, bgmBank, bgmBank + 1,

g_Renderer.GetUpscalerModeName());

} else {

swprintf_s(title,

L"Resident Evil Dual Shock (DX12) - Stage %u Room 0x%02X "

L"[Beretta 92FS: %d/%d]%ls%ls "

L"[BGM: Mute] [Upscaler: %ls] (F/R-Click: Aim | Enter/L-Click: "

L"Shoot | R: Reload)",

g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId(), g_Player.GetAmmo(),

g_Player.GetMaxAmmo(), aimStr, zombieStr,

g_Renderer.GetUpscalerModeName());

}

SetWindowTextW(hWnd, title);

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,

LPARAM lParam) {

switch (message) {

case WM_SIZE:

if (wParam != SIZE_MINIMIZED) {

g_Renderer.Resize(LOWORD(lParam), HIWORD(lParam));

}

return 0;

case WM_KEYDOWN:

if (wParam == VK_ESCAPE) {

g_IsRunning = false;

PostQuitMessage(0);

} else if (wParam == VK_F1) {

int nextMode = ((int)g_Renderer.GetUpscalerMode() + 1) % 6;

g_Renderer.SetUpscalerMode((Render::UpscalerMode)nextMode);

UpdateWindowTitle(hWnd);

std::printf("\n[Upscaler] Switched mode to: %ls\n",

g_Renderer.GetUpscalerModeName());

}

return 0;

case WM_DESTROY:

g_IsRunning = false;

PostQuitMessage(0);

return 0;

default:

return DefWindowProc(hWnd, message, wParam, lParam);

}

}

int main(int argc, char *argv[]) {

std::printf("====================================================\n");

std::printf(" Resident Evil Director's Cut: Dual Shock (PC Port) \n");

std::printf(" DirectX 12 Native Engine and Authentic Room Manager\n");

std::printf("====================================================\n\n");

// 1. Initialize PS1 Memory Arena

if (!PSX::Memory::Initialize()) {

std::fprintf(stderr, "Failed to initialize PS1 virtual memory arena!\n");

return 1;

}

// 2. Initialize GTE and Input

InitPAD();

InitGeom();

// 3. Register Win32 Window Class

HINSTANCE hInstance = GetModuleHandle(nullptr);

WNDCLASSEXW wcex = {};

wcex.cbSize = sizeof(WNDCLASSEXW);

wcex.style = CS_HREDRAW | CS_VREDRAW;

wcex.lpfnWndProc = WndProc;

wcex.hInstance = hInstance;

wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);

wcex.lpszClassName = L"ResidentEvilDualShockPortWndClass";

RegisterClassExW(&wcex);

const uint32_t windowWidth = 1280;

const uint32_t windowHeight = 960; // 4:3 authentic aspect ratio

RECT rc = {0, 0, static_cast<LONG>(windowWidth),

static_cast<LONG>(windowHeight)};

AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

HWND hWnd = CreateWindowW(

wcex.lpszClassName,

L"Resident Evil Director's Cut: Dual Shock Ver. (DirectX 12 PC Port)",

WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,

rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

if (!hWnd) {

std::fprintf(stderr, "Failed to create Win32 window!\n");

return 1;

}

ShowWindow(hWnd, SW_SHOW);

UpdateWindow(hWnd);

// 4. Initialize DirectX 12 Renderer with DLSS Hook and Motion Vector Target

if (!g_Renderer.Initialize(hWnd, windowWidth, windowHeight)) {

std::fprintf(stderr, "Failed to initialize DX12 Renderer!\n");

return 1;

}

// Parse optional command line flags for starting room and position

uint16_t startStage = 1;

uint8_t startRoom = 0;

float startX = 5274.0f, startY = 0.0f, startZ = 5652.0f;

float startFacing = 3.14159f;

for (int i = 1; i < argc; ++i) {

if (strcmp(argv[i], "--room") == 0 && i + 2 < argc) {

startStage = static_cast<uint16_t>(atoi(argv[i + 1]));

startRoom = static_cast<uint8_t>(atoi(argv[i + 2]));

if (startStage == 1 && startRoom == 1) {

// Main Hall double doors spawn

startX = 8700.0f; startY = 0.0f; startZ = 7900.0f;

startFacing = 3.14159f;

}

} else if (strcmp(argv[i], "--pos") == 0 && i + 4 < argc) {

startX = static_cast<float>(atof(argv[i + 1]));

startY = static_cast<float>(atof(argv[i + 2]));

startZ = static_cast<float>(atof(argv[i + 3]));

startFacing = static_cast<float>(atof(argv[i + 4]));

}

}

// 5. Initialize Room Manager and Load Initial Room

bool hasBg = false;

if (g_RoomManager.Initialize(L"extracted/RE1-Dualshock/PSXUSA")) {

if (g_RoomManager.LoadRoom(startStage, startRoom, 0)) {

hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(),

g_RoomManager.GetActiveCut());

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(startStage, startRoom);

// Parse SCD room voice dialogue events
{
  std::ifstream rdtFile(g_RoomManager.GetCurrentRdtPath(), std::ios::binary | std::ios::ate);
  if (rdtFile.is_open()) {
    size_t sz = static_cast<size_t>(rdtFile.tellg());
    std::vector<uint8_t> rdtBytes(sz);
    rdtFile.seekg(0, std::ios::beg);
    rdtFile.read(reinterpret_cast<char*>(rdtBytes.data()), sz);
    g_ScdVM.ParseRoomVoiceEvents(rdtBytes.data(), sz);
  }
}

std::printf(

"[Engine] Initialized Room Manager: Stage %u Room 0x%02X\n",

startStage, startRoom);

}

}

UpdateWindowTitle(hWnd);

// 6. Load Authentic 3D Weapon Model (WS212.TMD - Handgun)

std::wstring tmdCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/PLAYERS/WS212.TMD",

L"../extracted/RE1-Dualshock/PSXUSA/PLAYERS/WS212.TMD",

L"../../extracted/RE1-Dualshock/PSXUSA/PLAYERS/WS212.TMD",

L"../../../extracted/RE1-Dualshock/PSXUSA/PLAYERS/WS212.TMD",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/PLAYERS/WS212.TMD"};

bool hasTmd = false;

for (const auto &path : tmdCandidates) {

if (std::filesystem::exists(path)) {

hasTmd = g_Renderer.LoadTmdModel(path);

if (hasTmd) {

std::wprintf(L"[Engine] Loaded 3D weapon model from: %ls\n",

path.c_str());

break;

}

}

}

// 6b. Load Authentic 3D Player Character (CHAR10.EMD - Chris Redfield)

std::wstring emdCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/ENEMY/CHAR10.EMD",

L"../extracted/RE1-Dualshock/PSXUSA/ENEMY/CHAR10.EMD",

L"../../extracted/RE1-Dualshock/PSXUSA/ENEMY/CHAR10.EMD",

L"../../../extracted/RE1-Dualshock/PSXUSA/ENEMY/CHAR10.EMD",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/ENEMY/CHAR10.EMD"};

bool hasPlayer = false;

for (const auto &path : emdCandidates) {

if (std::filesystem::exists(path)) {

hasPlayer = g_Player.Initialize(g_Renderer.GetDevice(),

g_Renderer.GetCommandQueue(), path);

if (hasPlayer) {

std::wprintf(L"[Engine] Loaded 3D player character from: %ls\n",

path.c_str());

// Position Chris on the room floor

g_Player.SetPosition(startX, startY, startZ);

g_Player.SetFacingAngle(startFacing);

break;

}

}

}

// 6c. Load Authentic Equipped Weapon (W02.EMW & WS212.TMD - Handgun)

std::wstring emwCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/PLAYERS/W02.EMW",

L"../extracted/RE1-Dualshock/PSXUSA/PLAYERS/W02.EMW",

L"../../extracted/RE1-Dualshock/PSXUSA/PLAYERS/W02.EMW",

L"../../../extracted/RE1-Dualshock/PSXUSA/PLAYERS/W02.EMW",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/PLAYERS/W02.EMW"};

std::wstring tmdWeaponPath = L"";

for (const auto &path : tmdCandidates) {

if (std::filesystem::exists(path)) {

tmdWeaponPath = path;

break;

}

}

for (const auto &path : emwCandidates) {

if (std::filesystem::exists(path)) {

if (g_Player.LoadEquippedWeapon(path, tmdWeaponPath)) {

std::wprintf(

L"[Engine] Loaded authentic weapon animations & model from: %ls\n",

path.c_str());

break;

}

}

}

// 6d. Load Weapon Sound Bank (WP002.HED / WP002.VB)

std::wstring hedCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.HED",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.HED",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.HED",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.HED",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.HED"};

std::wstring vbCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.VB",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.VB",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.VB",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.VB",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/WP002.VB"};

std::wstring hedPath = L"", vbPath = L"";

for (const auto &p : hedCandidates) {

if (std::filesystem::exists(p)) {

hedPath = p;

break;

}

}

for (const auto &p : vbCandidates) {

if (std::filesystem::exists(p)) {

vbPath = p;

break;

}

}

if (!hedPath.empty() && !vbPath.empty()) {

g_AudioEngine.LoadWeaponAudio(hedPath, vbPath);

}

// Authentic PS1 room audio (footsteps & zombie SFX) is loaded

// from the room VAB (ROOM1000.RDT) in AudioEngine::LoadRoomAudio.

// CHAR00.VB is Chris's voice/exhale sfx and must not be used for zombie audio.

// 6e. Load Authentic 3D Enemy Zombie (EM1000.EMD - Dining Room Zombie)

std::wstring emdZombieCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",

L"../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",

L"../../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",

L"../../../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD"};

bool hasZombie = false;

for (const auto &path : emdZombieCandidates) {

if (std::filesystem::exists(path)) {

hasZombie = g_Zombie.Initialize(g_Renderer.GetDevice(),

g_Renderer.GetCommandQueue(), path);

if (hasZombie) {

std::wprintf(L"[Engine] Loaded 3D enemy zombie from: %ls\n",

path.c_str());

// Position Zombie in Mansion Dining Room (standing/shambling across

// room)

g_Zombie.SetPosition(2600.0f, 0.0f, 5652.0f);

g_Zombie.SetFacingAngle(3.14159f / 2.0f);

break;

}

}

}

// Initialize Authentic Inventory and Status Screen UI

std::wstring statusTimCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/DATA/STATUS.TIM",

L"../extracted/RE1-Dualshock/PSXUSA/DATA/STATUS.TIM",

L"../../extracted/RE1-Dualshock/PSXUSA/DATA/STATUS.TIM",

L"../../../extracted/RE1-Dualshock/PSXUSA/DATA/STATUS.TIM",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/DATA/STATUS.TIM"};

std::wstring itemPixCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/DATA/ITEM_ALL.PIX",

L"../extracted/RE1-Dualshock/PSXUSA/DATA/ITEM_ALL.PIX",

L"../../extracted/RE1-Dualshock/PSXUSA/DATA/ITEM_ALL.PIX",

L"../../../extracted/RE1-Dualshock/PSXUSA/DATA/ITEM_ALL.PIX",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/DATA/ITEM_ALL.PIX"};

std::wstring statusTim = L"";

for (const auto &p : statusTimCandidates) {

if (std::filesystem::exists(p)) {

statusTim = p;

break;

}

}

std::wstring itemAllPix = L"";

for (const auto &p : itemPixCandidates) {

if (std::filesystem::exists(p)) {

itemAllPix = p;

break;

}

}

if (!statusTim.empty() && !itemAllPix.empty()) {

if (g_Inventory.Initialize(g_Renderer.GetDevice(),

g_Renderer.GetCommandQueue(), statusTim,

itemAllPix)) {

std::printf("[Engine] Initialized authentic PS1 inventory & status "

"screen (STATUS.TIM + ITEM_ALL.PIX)\n");
Game::g_ItemBox.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), &g_Inventory);
PSX::g_MoviePlayer.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), g_AudioEngine.GetXAudio2());
Game::g_PuzzleManager.Initialize(&g_Inventory, &g_ScdVM);

}

}

// Initialize Authentic In-Game Text Box and Examine Subtitles (FONT.TIM + SLUS_007.47)

std::wstring fontTimCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/DATA/FONT.TIM",

L"../extracted/RE1-Dualshock/PSXUSA/DATA/FONT.TIM",

L"../../extracted/RE1-Dualshock/PSXUSA/DATA/FONT.TIM",

L"../../../extracted/RE1-Dualshock/PSXUSA/DATA/FONT.TIM",

L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/DATA/FONT.TIM"};

std::wstring slusCandidates[] = {

L"extracted/RE1-Dualshock/SLUS_007.47",

L"../extracted/RE1-Dualshock/SLUS_007.47",

L"../../extracted/RE1-Dualshock/SLUS_007.47",

L"../../../extracted/RE1-Dualshock/SLUS_007.47",

L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/SLUS_007.47"};

std::wstring fontTim = L"", slusPath = L"";

for (const auto &p : fontTimCandidates) {

if (std::filesystem::exists(p)) { fontTim = p; break; }

}

for (const auto &p : slusCandidates) {

if (std::filesystem::exists(p)) { slusPath = p; break; }

}

if (!fontTim.empty() && !slusPath.empty()) {

if (Game::g_TextBox.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), fontTim, slusPath)) {

std::printf("[Engine] Initialized authentic PS1 text box & examine subtitles (FONT.TIM + SLUS_007.47)\n");

}

}

// Initialize Iconic 3D Door Transition Animation Sequences (ITEM_M1)

std::wstring itemM1Candidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/ITEM_M1",

L"../extracted/RE1-Dualshock/PSXUSA/ITEM_M1",

L"../../extracted/RE1-Dualshock/PSXUSA/ITEM_M1",

L"../../../extracted/RE1-Dualshock/PSXUSA/ITEM_M1",

L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/ITEM_M1"};

std::wstring itemM1Dir = L"";

for (const auto &p : itemM1Candidates) {

if (std::filesystem::exists(p)) { itemM1Dir = p; break; }

}

if (!itemM1Dir.empty()) {

if (Game::g_DoorTransition.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), itemM1Dir)) {

std::printf("[Engine] Initialized authentic 3D door transition sequences (ITEM_M1/DOORxx.DOR)\n");

}

}

// Initialize SCD Room Script Virtual Machine for initial room

if (g_RoomManager.HasActiveRoom()) {

const auto &roomRaw = g_RoomManager.GetCurrentRdtData();

if (roomRaw.size() > 0x60) {

uint32_t initScdOff = *reinterpret_cast<const uint32_t *>(&roomRaw[0x60]);

g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(), initScdOff);

}

}

// 7. Initialize Audio Engine (XAudio2 and Authentic PS1 Footsteps)

if (g_AudioEngine.Initialize()) {

std::printf("[Engine] Initialized XAudio2 audio engine with authentic PS1 "

"sound effects\n");

if (g_RoomManager.HasActiveRoom()) {

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

}

std::wstring doorHedCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.HED"};

std::wstring doorVbCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/DOORWOD.VB"};

std::wstring dHed = L"", dVb = L"";

for (const auto &p : doorHedCandidates) {

if (std::filesystem::exists(p)) {

dHed = p;

break;

}

}

for (const auto &p : doorVbCandidates) {

if (std::filesystem::exists(p)) {

dVb = p;

break;

}

}

if (!dHed.empty() && !dVb.empty()) {

g_AudioEngine.LoadDoorAudio(dHed, dVb);

}

std::wstring selectHedCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.HED"};

std::wstring selectVbCandidates[] = {

L"extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",

L"../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",

L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",

L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB",

L"H:/Antigravity Projects/ultimate "

L"dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/SELECT.VB"};

std::wstring sHed = L"", sVb = L"";

for (const auto &p : selectHedCandidates) {

if (std::filesystem::exists(p)) {

sHed = p;

break;

}

}

for (const auto &p : selectVbCandidates) {

if (std::filesystem::exists(p)) {

sVb = p;

break;

}

}

if (!sHed.empty() && !sVb.empty()) {

g_AudioEngine.LoadSelectAudio(sHed, sVb);

}

std::wstring evilVbCandidates[] = {
    L"extracted/RE1-Dualshock/PSXUSA/SOUND/EVIL.VB",
    L"../extracted/RE1-Dualshock/PSXUSA/SOUND/EVIL.VB",
    L"../../extracted/RE1-Dualshock/PSXUSA/SOUND/EVIL.VB",
    L"../../../extracted/RE1-Dualshock/PSXUSA/SOUND/EVIL.VB",
    L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/SOUND/EVIL.VB"};
for (const auto &p : evilVbCandidates) {
  if (std::filesystem::exists(p)) {
    g_AudioEngine.LoadTitleAudio(p);
    break;
  }
}

}

std::wstring dataDirCandidates[] = {
    L"extracted/RE1-Dualshock/PSXUSA/DATA",
    L"../extracted/RE1-Dualshock/PSXUSA/DATA",
    L"../../extracted/RE1-Dualshock/PSXUSA/DATA",
    L"../../../extracted/RE1-Dualshock/PSXUSA/DATA",
    L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/DATA"};
for (const auto &p : dataDirCandidates) {
  if (std::filesystem::exists(p)) {
    if (Game::g_TitleScreen.Initialize(g_Renderer.GetDevice(), g_Renderer.GetCommandQueue(), p)) {
      std::printf("[Engine] Initialized authentic PS1 Startup Warning & Title Screen (GWARNIN1.TIM + TITLE.PIX + TMOJI.TIM)\n");
    }
    break;
  }
}

std::printf("\n[Engine] Entering main render loop with authentic PS1 shaders "

"and DLSS hook\n");

std::printf("[Controls] W/S: Walk/Backstep | A/D: Turn | Shift/X: Run\n");

std::printf("[Combat] Hold F / R-Mouse / R1: Aim | Enter / L-Mouse / Cross: "

"Shoot | R: Reload\n");

std::printf(

"[Combat] While Aiming: W = Aim Up | S = Aim Down | A/D = Turn Aim\n");

std::printf(

"[Controls] Space / Enter / Cross (when not aiming): Open Doors\n");

std::printf("[Controls] 1..6: Switch Camera Cut | F1: Upscaler | F2: Next "

"Room | M/N: Jukebox\n\n");

// 8. Engine / Render Loop

MSG msg = {};

uint32_t frameCount = 0;

float rotationY = 0.0f;

float rotationX = 0.2f;

static bool s_ActionWasPressed = false;

static bool s_F2WasPressed = false;

static bool s_F3WasPressed = false;

static bool s_F4WasPressed = false;

static bool s_F5WasPressed = false;

static bool s_F6WasPressed = false;

static bool s_F7WasPressed = false;

for (int argi = 1; argi < argc; ++argi) {

if (strcmp(argv[argi], "--test-textbox") == 0) {

Game::g_TextBox.ShowMessage("A grand fireplace.\nEmbers are still softly glowing.", false);

} else if (strcmp(argv[argi], "--test-prompt") == 0) {

Game::g_TextBox.ShowMessage("Will you take the\nCombat Knife?", true);

} else if (strcmp(argv[argi], "--play-movie") == 0 || strcmp(argv[argi], "--movie") == 0) {
int mId = (argi + 1 < argc) ? atoi(argv[++argi]) : 0;
PSX::g_MoviePlayer.OpenMovie(static_cast<PSX::MovieId>(mId));
} else if (strcmp(argv[argi], "--test-itembox") == 0) {

Game::g_ItemBox.Open();

} else if (strcmp(argv[argi], "--test-puzzle") == 0) {
    g_Inventory.AddItem(35, 1); // Wind Crest
    g_Inventory.AddItem(36, 1); // Star Crest
    g_Inventory.AddItem(37, 1); // Sun Crest
    g_Inventory.AddItem(38, 1); // Moon Crest
    g_Inventory.AddItem(46, 1); // Musical Score
    g_Inventory.AddItem(44, 1); // Wooden Emblem
} else if (strcmp(argv[argi], "--test-inventory") == 0) {
    g_Inventory.ToggleOpen();
} else if (strcmp(argv[argi], "--test-use-piano") == 0) {
    auto res = Game::g_PuzzleManager.TryUseItem(46, 1, 5, 0, 0);
    if (res.handled) {
        if (res.awardItemId > 0) g_Inventory.AddItem(res.awardItemId, res.awardCount);
        Game::g_TextBox.ShowMessage(res.message, false);
    }
} else if (strcmp(argv[argi], "--test-use-crests") == 0) {
    Game::g_PuzzleManager.TryUseItem(35, 1, 0x0E, 0, 0);
    Game::g_PuzzleManager.TryUseItem(36, 1, 0x0E, 0, 0);
    Game::g_PuzzleManager.TryUseItem(37, 1, 0x0E, 0, 0);
    auto res = Game::g_PuzzleManager.TryUseItem(38, 1, 0x0E, 0, 0);
    if (res.handled) {
        Game::g_TextBox.ShowMessage(res.message, false);
    }
} else if (strcmp(argv[argi], "--test-door") == 0) {

Game::g_DoorTransition.StartTransition(0, 0, nullptr);

}

}

bool startAtTitle = (argc <= 1);
for (int argi = 1; argi < argc; ++argi) {
  if (strcmp(argv[argi], "--title") == 0) startAtTitle = true;
  if (strcmp(argv[argi], "--skip-title") == 0) startAtTitle = false;
}
if (startAtTitle) {
  Game::g_TitleScreen.StartSequence(
      [&]() {
        if (g_RoomManager.HasActiveRoom()) {
          g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());
          g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),
                                      g_RoomManager.GetCurrentRoomId());
        }
        UpdateWindowTitle(hWnd);
      },
      [&]() {
        PerformLoadGame(hWnd, hasBg);
      });
}

while (g_IsRunning) {

while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

TranslateMessage(&msg);

DispatchMessage(&msg);

if (msg.message == WM_QUIT) {

g_IsRunning = false;

}

}

if (!g_IsRunning)

break;

ReadPAD(&g_Pad);

// Turntable rotation for preview mode if no player active

if (!hasPlayer) {

if (g_Pad.buttons & (1 << 6))

rotationY -= 0.03f;

if (g_Pad.buttons & (1 << 7))

rotationY += 0.03f;

if (g_Pad.buttons & (1 << 4))

rotationX -= 0.02f;

if (g_Pad.buttons & (1 << 5))

rotationX += 0.02f;

rotationY += 0.015f;

}

// Inventory Toggle Key (Tab / I / Gamepad Y/Triangle)

static bool s_PrevTabDown = false;

bool tabRaw = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0 ||

(GetAsyncKeyState('I') & 0x8000) != 0 ||

(g_Pad.buttons & (1 << 12)) != 0; // Triangle

if (tabRaw && !s_PrevTabDown && !Game::g_TitleScreen.IsActive() && !PSX::g_MoviePlayer.IsPlaying()) {

g_Inventory.ToggleOpen();

}

s_PrevTabDown = tabRaw;

// Interactive Preview Hotkeys: T (Text Box), Y (Yes/No Prompt), U (Door Transition)

static bool s_PrevT = false, s_PrevY = false, s_PrevU = false, s_PrevO = false;

bool tDown = (GetAsyncKeyState('T') & 0x8000) != 0;

bool yDown = (GetAsyncKeyState('Y') & 0x8000) != 0;

bool uDown = (GetAsyncKeyState('U') & 0x8000) != 0;

if (tDown && !s_PrevT) {

Game::g_TextBox.ShowMessage("A grand fireplace.\nEmbers are still softly glowing.", false);

}

if (yDown && !s_PrevY) {

Game::g_TextBox.ShowMessage("Will you take the\nCombat Knife?", true);

}

if (uDown && !s_PrevU) {

Game::g_DoorTransition.StartTransition(0, 0, nullptr);

}

bool oDown = (GetAsyncKeyState('O') & 0x8000) != 0;
if (oDown && !s_PrevO) {
  if (Game::g_ItemBox.IsOpen()) Game::g_ItemBox.Close();
  else Game::g_ItemBox.Open();
}
s_PrevO = oDown;

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
s_PrevP = pDown;
s_PrevT = tDown; s_PrevY = yDown; s_PrevU = uDown;

// Quick Save (F5) and Quick Load (F6) Hotkeys
static bool s_PrevF5 = false, s_PrevF6 = false;
bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
if (f5Down && !s_PrevF5) {
  PerformSaveGame();
}
if (f6Down && !s_PrevF6) {
  PerformLoadGame(hWnd, hasBg);
}
s_PrevF5 = f5Down; s_PrevF6 = f6Down;

bool textBoxWasActive = Game::g_TextBox.IsActive();
if (PSX::g_MoviePlayer.IsPlaying()) {
  PSX::g_MoviePlayer.Update(1.0f / 60.0f, g_Pad.buttons);
  if (!PSX::g_MoviePlayer.IsPlaying() && Game::g_TitleScreen.IsActive()) {
    Game::g_TitleScreen.Update(1.0f / 60.0f, g_Pad.buttons);
  }
} else if (Game::g_TitleScreen.IsActive()) {
  Game::g_TitleScreen.Update(1.0f / 60.0f, g_Pad.buttons);
} else if (Game::g_DoorTransition.IsActive()) {
  Game::g_DoorTransition.Update(1.0f / 60.0f, g_Pad.buttons);
} else if (Game::g_TextBox.IsActive()) {
  Game::g_TextBox.Update(1.0f / 60.0f, g_Pad.buttons);
} else if (Game::g_ItemBox.IsOpen()) {
  Game::g_ItemBox.Update(1.0f / 60.0f, g_Pad.buttons);
} else if (g_Inventory.IsOpen()) {
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

// F11 Hotkey: Toggle Automated Door Walker Bot
static bool s_F11_WasPressed = false;
bool f11Pressed = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
if (f11Pressed && !s_F11_WasPressed) {
  g_AutoDoorWalker = !g_AutoDoorWalker;
  g_AutoTargetDoorIdx = -1;
  g_AutoWaypoints.clear();
  g_AutoWaypointIdx = 0;
  g_AutoStuckFrames = 0;
  std::printf("\n[AUTO-WALKER] %s (F11 to toggle)\n",
              g_AutoDoorWalker ? "ENABLED - Walking Chris to every door automatically!" : "DISABLED");
  UpdateWindowTitle(hWnd);
}
s_F11_WasPressed = f11Pressed;

bool autoTriggerDoorNow = false;
if (!Game::g_TitleScreen.IsActive() && !PSX::g_MoviePlayer.IsPlaying() && !g_Inventory.IsOpen() && !Game::g_ItemBox.IsOpen() && !Game::g_TextBox.IsActive() && !Game::g_DoorTransition.IsActive()) {
  uint32_t effectiveButtons = g_Pad.buttons;

  if (g_AutoDoorWalker && hasPlayer && g_RoomManager.HasActiveRoom()) {
    const auto &doors = g_RoomManager.GetDoors();
    uint16_t curStage = g_RoomManager.GetCurrentStage();
    uint8_t curRoom = g_RoomManager.GetCurrentRoomId();
    DirectX::XMFLOAT3 ppos = g_Player.GetPosition();

    if (doors.empty()) {
      g_AutoTargetDoorIdx = -1;
    } else if (g_AutoTargetDoorIdx < 0 || g_AutoTargetDoorIdx >= static_cast<int>(doors.size())) {
      uint16_t curRoomKey = (curStage << 8) | curRoom;
      if (std::find(g_AutoVisitedRooms.begin(), g_AutoVisitedRooms.end(), curRoomKey) == g_AutoVisitedRooms.end()) {
        g_AutoVisitedRooms.push_back(curRoomKey);
      }

      // Smart door selection:
      // 1) Unvisited door to an UNVISITED room (not previous room)
      // 2) Unvisited door to any room other than g_AutoPrevRoom
      // 3) Any unvisited door
      // 4) Any door not leading back to g_AutoPrevRoom (unless 1-door dead-end room)
      int chosen = -1;
      int bestPriority = 999;
      for (size_t i = 0; i < doors.size(); ++i) {
        uint32_t dKey = (static_cast<uint32_t>(curStage) << 16) |
                        (static_cast<uint32_t>(curRoom) << 8) | doors[i].slot;
        uint16_t tRoomKey = (static_cast<uint16_t>(doors[i].targetStage) << 8) | doors[i].targetRoom;
        bool dVisited = (std::find(g_AutoVisitedDoors.begin(), g_AutoVisitedDoors.end(), dKey) != g_AutoVisitedDoors.end());
        bool rVisited = (std::find(g_AutoVisitedRooms.begin(), g_AutoVisitedRooms.end(), tRoomKey) != g_AutoVisitedRooms.end());
        bool isBacktrack = (doors[i].targetStage == g_AutoPrevStage && doors[i].targetRoom == g_AutoPrevRoom);

        int prio = 5;
        if (!dVisited && !rVisited && !isBacktrack) prio = 1;
        else if (!dVisited && !isBacktrack) prio = 2;
        else if (!dVisited) prio = 3;
        else if (!isBacktrack) prio = 4;

        if (prio < bestPriority) {
          bestPriority = prio;
          chosen = static_cast<int>(i);
        }
      }
      if (chosen < 0) chosen = 0;
      g_AutoTargetDoorIdx = chosen;
      g_RoomManager.FindPathToDoor(ppos.x, ppos.z, ppos.y, doors[chosen], g_AutoWaypoints);
      g_AutoWaypointIdx = 0;
      g_AutoStuckFrames = 0;
      g_AutoLastPos = ppos;
      std::printf("[AUTO-WALKER] Routing Stage %u Room 0x%02X -> Door Slot %u (Target: Stage %u Room 0x%02X, %zu waypoints)\n",
                  curStage, curRoom, doors[chosen].slot, doors[chosen].targetStage,
                  doors[chosen].targetRoom, g_AutoWaypoints.size());
    }

    if (g_AutoTargetDoorIdx >= 0 && g_AutoTargetDoorIdx < static_cast<int>(doors.size())) {
      const RoomDoor &td = doors[g_AutoTargetDoorIdx];
      float doorCx = td.x + td.w * 0.5f;
      float doorCz = td.z + td.d * 0.5f;
      float angToDoor = std::atan2(doorCx - ppos.x, doorCz - ppos.z);

      RoomDoor probeHit;
      if (g_RoomManager.CheckDoorTrigger(ppos.x, ppos.z, angToDoor, probeHit) &&
          probeHit.slot == td.slot) {
        g_Player.SetFacingAngle(angToDoor);
        autoTriggerDoorNow = true;
        uint32_t key = (static_cast<uint32_t>(curStage) << 16) |
                       (static_cast<uint32_t>(curRoom) << 8) | td.slot;
        g_AutoVisitedDoors.push_back(key);
        g_AutoTargetDoorIdx = -1;
        g_AutoWaypoints.clear();
      } else {
        while (g_AutoWaypointIdx + 1 < g_AutoWaypoints.size()) {
          float wx = g_AutoWaypoints[g_AutoWaypointIdx].x;
          float wz = g_AutoWaypoints[g_AutoWaypointIdx].y;
          if (std::hypot(wx - ppos.x, wz - ppos.z) < 280.0f) {
            g_AutoWaypointIdx++;
          } else {
            break;
          }
        }
        float tx = (g_AutoWaypointIdx < g_AutoWaypoints.size()) ? g_AutoWaypoints[g_AutoWaypointIdx].x : doorCx;
        float tz = (g_AutoWaypointIdx < g_AutoWaypoints.size()) ? g_AutoWaypoints[g_AutoWaypointIdx].y : doorCz;
        float moveAng = std::atan2(tx - ppos.x, tz - ppos.z);
        g_Player.SetFacingAngle(moveAng);
        effectiveButtons |= (1 << 4) | (1 << 15); // Forward + Sprint

        float moved = std::hypot(ppos.x - g_AutoLastPos.x, ppos.z - g_AutoLastPos.z);
        g_AutoLastPos = ppos;
        if (moved < 2.0f) {
          g_AutoStuckFrames++;
          // Steer perpendicular for a few frames and re-plan A* path without teleporting through walls
          if (g_AutoStuckFrames > 20) {
            float escapeSign = ((g_AutoStuckFrames / 20) % 2 == 0) ? 1.0f : -1.0f;
            g_Player.SetFacingAngle(moveAng + escapeSign * 1.35f);
          }
          if (g_AutoStuckFrames > 75) {
            g_RoomManager.FindPathToDoor(ppos.x, ppos.z, ppos.y, td, g_AutoWaypoints);
            g_AutoWaypointIdx = (g_AutoWaypoints.size() > 1) ? 1 : 0;
            g_AutoStuckFrames = 0;
          }
        } else {
          g_AutoStuckFrames = 0;
        }
      }
    }
  }

  // Update Player Tank Controls and Skeletal Animation
  if (hasPlayer) {
    g_Player.Update(1.0f / 60.0f, effectiveButtons, &g_RoomManager);
  }

// Update Enemy Zombie AI, Hitscan Combat, and Skeletal Animation

if (hasZombie && g_Zombie.IsLoaded() &&

g_RoomManager.GetCurrentStage() == 1 &&

g_RoomManager.GetCurrentRoomId() == 0) {

bool bulletFired = g_Player.JustFiredBullet();

g_Zombie.Update(1.0f / 60.0f, g_Player.GetPosition(), bulletFired,

&g_RoomManager);

if (bulletFired) {

g_Zombie.CheckBulletHit(g_Player.GetPosition(),

g_Player.GetFacingAngle(),

(int)g_Player.GetAimPitch(), 10);

}

}

}

// Update Audio Engine

g_AudioEngine.Update();

// 1. Door & Scenery Interaction Trigger Check

bool rawWorldAction = ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                      ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                      ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                      ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) ||
                      ((g_Pad.buttons & ((1 << 0) | (1 << 2) | (1 << 14))) != 0);
if (hasPlayer && g_RoomManager.HasActiveRoom() &&
    !Game::g_TitleScreen.IsActive() && !PSX::g_MoviePlayer.IsPlaying() &&
    !Game::g_DoorTransition.IsActive() && !Game::g_TextBox.IsActive() && !textBoxWasActive &&
    !g_Inventory.IsOpen() && !Game::g_ItemBox.IsOpen()) {

DirectX::XMFLOAT3 playerPos = g_Player.GetPosition();

// Dual-channel keyboard + gamepad action button check (E, Enter, Space,

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
      if (autoTriggerDoorNow) {
        actionPressed = true;
        s_ActionWasPressed = false;
      }


      if (actionPressed && !s_ActionWasPressed) {
        uint8_t pickedItemId = 0;
        if (g_ScdVM.PickupItemAtPosition(playerPos.x, playerPos.z, 600.0f, pickedItemId)) {
          const char *itemName = g_Inventory.GetItemName(pickedItemId);
          if (g_Inventory.AddItem(pickedItemId, 1)) {
            g_AudioEngine.PlayItemPickup();
            char pickupMsg[128];
            snprintf(pickupMsg, sizeof(pickupMsg), "You found a\n%s.", itemName);
            Game::g_TextBox.ShowMessage(pickupMsg, false);
            std::printf("\n====================================================\n");
            std::printf("[Item Pickup] Collected %s (Item ID %u)!\n", itemName, pickedItemId);
            std::printf("====================================================\n\n");
          } else {
            Game::g_TextBox.ShowMessage("You can't carry any more\nitems.", false);
            std::printf("\n[Item Pickup] Inventory Full! Cannot collect %s\n", itemName);
          }
        } else {
          RoomDoor hitDoor;
          if (g_RoomManager.CheckDoorTrigger(playerPos.x, playerPos.z,
                                             g_Player.GetFacingAngle(),
                                             hitDoor)) {
            g_RoomManager.CrosscheckDoorEntry(playerPos.x, playerPos.y, playerPos.z,
                                              g_Player.GetFacingAngle(), hitDoor);
            g_AutoPrevStage = g_RoomManager.GetCurrentStage();
            g_AutoPrevRoom = g_RoomManager.GetCurrentRoomId();
            if (g_AutoDoorWalker) { hitDoor.isLocked = false; }
            if (hitDoor.isLocked) {
              if (hitDoor.keyItem == 0xFE || (hitDoor.keyItem != 0 && hitDoor.keyItem != 0xFF && g_Inventory.HasItem(hitDoor.keyItem))) {
                std::printf("\n[Door Unlock] Unlocked door using %s!\n",
                            g_Inventory.GetItemName(hitDoor.keyItem));
                hitDoor.isLocked = false;
                g_AudioEngine.PlayDoorOpen();
                Game::g_TextBox.ShowMessage("\nYou unlocked it.", false,
                    [=, &hasBg, &hWnd](bool) {
                      float nx = static_cast<float>(hitDoor.nextX);
                      float ny = static_cast<float>(hitDoor.nextY);
                      float nz = static_cast<float>(hitDoor.nextZ);
                      float newAngle = DirectX::XM_PIDIV2 + (static_cast<float>(hitDoor.nextDir) / 4096.0f) * DirectX::XM_2PI;

                      Game::g_DoorTransition.StartTransition(
                          hitDoor.slot, 0, [=, &hasBg, &hWnd]() {
                            if (g_RoomManager.LoadRoomAtPosition(
                                    hitDoor.targetStage, hitDoor.targetRoom,
                                    nx, nz)) {
                              g_Player.SetPosition(nx, ny, nz);
                              g_Player.SetFacingAngle(newAngle);

                              uint32_t activeCut = g_RoomManager.GetActiveCut();
                              hasBg = g_Renderer.LoadRoomBackground(
                                  g_RoomManager.GetCurrentBssPath(), activeCut);
                              g_AudioEngine.LoadRoomAudio(
                                  g_RoomManager.GetCurrentRdtPath());
                              g_AudioEngine.UpdateRoomBgm(
                                  g_RoomManager.GetCurrentStage(),
                                  g_RoomManager.GetCurrentRoomId());

                              const auto &roomRaw =
                                  g_RoomManager.GetCurrentRdtData();
                              if (roomRaw.size() > 0x60) {
                                uint32_t initScdOff =
                                    *reinterpret_cast<const uint32_t *>(
                                        &roomRaw[0x60]);
                                g_ScdVM.ExecuteInitScript(roomRaw.data(),
                                                          roomRaw.size(),
                                                          initScdOff);
                              }

                              g_AudioEngine.PlayDoorClose();
                              UpdateWindowTitle(hWnd);
                              std::printf("[Door Transition Complete] Entered "
                                          "Stage %u Room 0x%02X at Camera Cut %u\n",
                                          hitDoor.targetStage,
                                          hitDoor.targetRoom, activeCut);
                            }
                          });
                    });
              } else if (hitDoor.keyItem == 0 || hitDoor.keyItem == 0xFF) {
                std::printf("\n[Door Locked] The door is locked from the other side.\n");
                g_AudioEngine.PlayDryFire();
                Game::g_TextBox.ShowMessage("\nIt's locked from inside.", false);
              } else {
                std::printf("\n[Door Locked] The door is locked. Requires: %s (ID %u)\n",
                            g_Inventory.GetItemName(hitDoor.keyItem),
                            hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                if (hitDoor.keyItem == 51) {
                  Game::g_TextBox.ShowMessage("\nIt's locked.\nA carving of a sword.", false);
                } else if (hitDoor.keyItem == 52) {
                  Game::g_TextBox.ShowMessage("\nIt's locked.\nA carving of an armor.", false);
                } else if (hitDoor.keyItem == 53) {
                  Game::g_TextBox.ShowMessage("\nIt's locked.\nA carving of a shield.", false);
                } else if (hitDoor.keyItem == 54) {
                  Game::g_TextBox.ShowMessage("\nIt's locked.\nA carving of a helmet.", false);
                } else {
                  std::string lockMsg = "\nIt's locked.\nRequires the " +
                                        std::string(g_Inventory.GetItemName(hitDoor.keyItem)) + ".";
                  Game::g_TextBox.ShowMessage(lockMsg, false);
                }
              }
            } else {
              // Unlocked door: Start 3D authentic door transition sequence!
              std::printf("\n====================================================\n");
              std::printf("[Door Transition] Starting 3D sequence for Door %u!\n",
                          hitDoor.slot);
              std::printf("  Destination: Stage %u Room 0x%02X\n",
                          hitDoor.targetStage, hitDoor.targetRoom);
              std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\n",
                          static_cast<float>(hitDoor.nextX),
                          static_cast<float>(hitDoor.nextY),
                          static_cast<float>(hitDoor.nextZ), hitDoor.nextDir);
              std::printf("====================================================\n");

              float nx = static_cast<float>(hitDoor.nextX);
              float ny = static_cast<float>(hitDoor.nextY);
              float nz = static_cast<float>(hitDoor.nextZ);
              float newAngle = DirectX::XM_PIDIV2 + (static_cast<float>(hitDoor.nextDir) / 4096.0f) * DirectX::XM_2PI;

              Game::g_DoorTransition.StartTransition(
                  hitDoor.slot, 0, [=, &hasBg, &hWnd]() {
                    if (g_RoomManager.LoadRoomAtPosition(
                            hitDoor.targetStage, hitDoor.targetRoom, nx, nz)) {
                      g_Player.SetPosition(nx, ny, nz);
                      g_Player.SetFacingAngle(newAngle);

                      uint32_t activeCut = g_RoomManager.GetActiveCut();
                      hasBg = g_Renderer.LoadRoomBackground(
                          g_RoomManager.GetCurrentBssPath(), activeCut);
                      g_AudioEngine.LoadRoomAudio(
                          g_RoomManager.GetCurrentRdtPath());
                      g_AudioEngine.UpdateRoomBgm(
                          g_RoomManager.GetCurrentStage(),
                          g_RoomManager.GetCurrentRoomId());

                      const auto &roomRaw = g_RoomManager.GetCurrentRdtData();
                      if (roomRaw.size() > 0x60) {
                        uint32_t initScdOff =
                            *reinterpret_cast<const uint32_t *>(
                                &roomRaw[0x60]);
                        g_ScdVM.ExecuteInitScript(roomRaw.data(), roomRaw.size(),
                                                  initScdOff);
                      }

                      g_AudioEngine.PlayDoorClose();
                      UpdateWindowTitle(hWnd);
                      std::printf("[Door Transition Complete] Entered Stage %u "
                                  "Room 0x%02X at Camera Cut %u (facing %.1f deg)\n",
                                  hitDoor.targetStage, hitDoor.targetRoom,
                                  activeCut, newAngle * (180.0f / DirectX::XM_PI));
                    }
                  });
            }
          } else {
                        // Room Scenery Examination
            if (g_RoomManager.GetCurrentStage() == 1 &&
                g_RoomManager.GetCurrentRoomId() == 0) {
              // Stage 1 Room 0: 1F West Medical Save Room (where Chris meets Rebecca)
              if (playerPos.z > 6500.0f) {
  // Foreground desk with typewriter
  if (!g_Inventory.HasItem(47)) {
    Game::g_TextBox.ShowMessage(
        "It\'s a typewriter.\nYou need an ink ribbon to record your game.", false);
  } else {
    Game::g_TextBox.ShowMessage(
        "It\'s a typewriter.\nWould you like to use an ink ribbon to save?", true,
        [](bool yes) {
          if (yes) {
            g_Inventory.RemoveItemByItemId(47, 1);
            g_AudioEngine.PlayTypewriter();
            PerformSaveGame();
          }
        });
  }
} else if (playerPos.x < 4200.0f && playerPos.z < 4500.0f) {
  // Red metal item storage chest beside the door
  Game::g_TextBox.ShowMessage(
      "An Item Box.\nWould you like to open it?", true,
      [](bool yes) {
        if (yes) {
          Game::g_ItemBox.Open();
        }
      });
              } else if (playerPos.x >= 4200.0f && playerPos.z < 4500.0f) {
                // Wooden shelf with medicine bottles (authentic Capcom text)
                Game::g_TextBox.ShowMessage(
                    "Vitamins and serums.\nOne dose would do.", false);
              } else {
                // Medical treatment bed
                Game::g_TextBox.ShowMessage(
                    "A simple medical bed.\nIt looks clean and prepared for emergencies.", false);
              }
            } else if (g_RoomManager.GetCurrentStage() == 1 &&
                       g_RoomManager.GetCurrentRoomId() == 5) {
              // Stage 1 Room 5: 1F Dining Room
              if (playerPos.x < 1500.0f && playerPos.z > 1800.0f) {
                Game::g_TextBox.ShowMessage(
                    "A grand fireplace.\nEmbers are still softly glowing.\nIt's the hollow that an emblem was in.", false);
              } else if (playerPos.z < 1500.0f) {
                Game::g_TextBox.ShowMessage(
                    "A dusty looking Grandfather Clock.\nIt is not running any more.", false);
              } else {
                Game::g_TextBox.ShowMessage(
                    "A long mahogany dining table.\nDinner utensils are neatly placed.", false);
              }
            } else {
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
            }
          }
        }
      }
      s_ActionWasPressed = actionPressed;
    } else {
      s_ActionWasPressed = rawWorldAction;
    }

    // 2. Dynamic Camera Switch Triggers (Authentic RE1 RDT Zones)

if (hasPlayer && g_RoomManager.HasActiveRoom()) {

DirectX::XMFLOAT3 playerPos = g_Player.GetPosition();

uint32_t newCut = 0;

if (g_RoomManager.UpdateCameraZone(playerPos.x, playerPos.z, newCut)) {

std::printf("\n[Camera Switch] Transitioned to Cut %u (Player Pos: "

"%.1f, %.1f, %.1f)\n",

newCut, playerPos.x, playerPos.y, playerPos.z);

hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(),

newCut);

}

}

// 3. Manual Camera Cut Selection Keys (1..9)

if (g_RoomManager.HasActiveRoom()) {

uint8_t numCams = g_RoomManager.GetNumCameras();

for (uint8_t k = 0; k < numCams && k < 9; ++k) {

if ((GetAsyncKeyState('1' + k) & 0x8000) != 0 &&

g_RoomManager.GetActiveCut() != k) {

if (g_RoomManager.SetActiveCut(k)) {

std::printf("\n[Manual Camera Select] Active Cut %u\n", k);

hasBg = g_Renderer.LoadRoomBackground(

g_RoomManager.GetCurrentBssPath(), k);

}

}

}

}

// 4. Debug Room Transition Hotkeys: F2 (Cycle Room), F3 (Dining Room), F4

// (Main Hall)

bool f2Pressed = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;

if (f2Pressed && !s_F2WasPressed) {
uint16_t curStage = g_RoomManager.GetCurrentStage();
uint8_t nextRoom = (g_RoomManager.GetCurrentRoomId() + 1) % 0x1D;
std::printf("\n[Debug Transition] Cycling to Stage %u Room 0x%02X...\n", curStage, nextRoom);
if (g_RoomManager.LoadRoom(curStage, nextRoom, 0)) {
  const auto &rmDoors = g_RoomManager.GetDoors();
  if (!rmDoors.empty()) {
    std::vector<DirectX::XMFLOAT2> wps;
    float dcx = rmDoors[0].x + rmDoors[0].w * 0.5f;
    float dcz = rmDoors[0].z + rmDoors[0].d * 0.5f;
    g_RoomManager.FindPathToDoor(dcx, dcz, 0.0f, rmDoors[0], wps);
    float spawnX = wps.empty() ? dcx : wps.back().x;
    float spawnZ = wps.empty() ? dcz : wps.back().y;
    g_RoomManager.LoadRoomAtPosition(curStage, nextRoom, spawnX, spawnZ);
    g_Player.SetPosition(spawnX, 0.0f, spawnZ);
  }
  uint32_t activeCut = g_RoomManager.GetActiveCut();
  hasBg = g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), activeCut);

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F2WasPressed = f2Pressed;

bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;

if (f3Pressed && !s_F3WasPressed) {

std::printf(

"\n[Debug Transition] Jumping to Dining Room (Room 0x00)...\n");

if (g_RoomManager.LoadRoom(1, 0, 0)) {

g_Player.SetPosition(5274.0f, 0.0f, 5652.0f);

hasBg =

g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), 0);

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F3WasPressed = f3Pressed;

bool f4Pressed = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;

if (f4Pressed && !s_F4WasPressed) {

std::printf("\n[Debug Transition] Jumping to Main Hall (Room 0x01)...\n");

if (g_RoomManager.LoadRoomAtPosition(1, 1, 8700.0f, 7900.0f)) {
g_Player.SetPosition(8700.0f, 0.0f, 7900.0f);
hasBg =
g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), g_RoomManager.GetActiveCut());

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F4WasPressed = f4Pressed;

// F5: Jump to Underground Laboratory (Stage 5, Room 0x00)

bool f5Pressed = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;

if (f5Pressed && !s_F5WasPressed) {

std::printf("\n[Debug Transition] Jumping to Underground Laboratory "

"(Stage 5 Room 0x00)...\n");

if (g_RoomManager.LoadRoom(5, 0, 0)) {

g_Player.SetPosition(10170.0f, 0.0f, 3276.0f);

hasBg =

g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), 0);

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F5WasPressed = f5Pressed;

// F6: Jump to Underground Caves / Basement (Stage 3, Room 0x00)

bool f6Pressed = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;

if (f6Pressed && !s_F6WasPressed) {

std::printf("\n[Debug Transition] Jumping to Underground Caves / "

"Basement (Stage 3 Room 0x00)...\n");

if (g_RoomManager.LoadRoom(3, 0, 0)) {

g_Player.SetPosition(25992.0f, 0.0f, 11016.0f);

hasBg =

g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), 0);

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F6WasPressed = f6Pressed;

// F7: Jump to Guardhouse / Dormitory (Stage 4, Room 0x00)

bool f7Pressed = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;

if (f7Pressed && !s_F7WasPressed) {

std::printf("\n[Debug Transition] Jumping to Guardhouse / Dormitory "

"(Stage 4 Room 0x00)...\n");

if (g_RoomManager.LoadRoom(4, 0, 0)) {

const RoomCamera *cam = g_RoomManager.GetActiveCamera();

if (cam)

g_Player.SetPosition(cam->to.x, 0.0f, cam->to.z);

hasBg =

g_Renderer.LoadRoomBackground(g_RoomManager.GetCurrentBssPath(), 0);

g_AudioEngine.LoadRoomAudio(g_RoomManager.GetCurrentRdtPath());

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

UpdateWindowTitle(hWnd);

}

}

s_F7WasPressed = f7Pressed;

// Music Jukebox Hotkeys: M / F8 (Next Song), N / F9 (Prev Song), B (Mute /

// Resume BGM)

static bool s_M_WasPressed = false;

bool mPressed = (GetAsyncKeyState('M') & 0x8000) != 0 ||

(GetAsyncKeyState(VK_F8) & 0x8000) != 0;

if (mPressed && !s_M_WasPressed) {

g_AudioEngine.NextBgm();

UpdateWindowTitle(hWnd);

}

s_M_WasPressed = mPressed;

static bool s_N_WasPressed = false;

bool nPressed = (GetAsyncKeyState('N') & 0x8000) != 0 ||

(GetAsyncKeyState(VK_F9) & 0x8000) != 0;

if (nPressed && !s_N_WasPressed) {

g_AudioEngine.PrevBgm();

UpdateWindowTitle(hWnd);

}

s_N_WasPressed = nPressed;

static bool s_B_WasPressed = false;

bool bPressed = (GetAsyncKeyState('B') & 0x8000) != 0;

if (bPressed && !s_B_WasPressed) {

if (g_AudioEngine.GetCurrentBgmBank() != 0xFF) {

std::printf("\n[Audio] Muting BGM\n");

g_AudioEngine.StopBgm();

} else {

g_AudioEngine.UpdateRoomBgm(g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId());

}

UpdateWindowTitle(hWnd);

}

s_B_WasPressed = bPressed;

// --- Voice Line Controls (V: Play/Trigger Room Voice Acting Clip, F10: Next Voice Line) ---
static bool s_V_WasPressed = false;
bool vPressed = (GetAsyncKeyState('V') & 0x8000) != 0;
if (vPressed && !s_V_WasPressed) {
  uint8_t currentStage = g_RoomManager.GetCurrentStage();
  const auto &clips = g_ScdVM.GetRoomVoiceClips();
  if (!clips.empty()) {
    static size_t s_roomClipIdx = 0;
    uint16_t clipToPlay = clips[s_roomClipIdx % clips.size()].clipId;
    std::printf("\n[Voice] Triggering Room Dialogue Clip: 0x%02X (%u) [entry %zu/%zu]\n",
                clipToPlay, clipToPlay, (s_roomClipIdx % clips.size()) + 1, clips.size());
    g_AudioEngine.PlayVoiceClip(currentStage, clipToPlay);
    s_roomClipIdx++;
  } else {
    std::printf("\n[Voice] Cycling Stage %u Voice Line...\n", currentStage);
    g_AudioEngine.NextVoiceClip(currentStage);
  }
}
s_V_WasPressed = vPressed;

static bool s_F10_WasPressed = false;
bool f10Pressed = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
if (f10Pressed && !s_F10_WasPressed) {
  g_AudioEngine.NextVoiceClip(g_RoomManager.GetCurrentStage());
}
s_F10_WasPressed = f10Pressed;

// 5. Dynamic Camera View and Projection Matrix from Active Room Camera

DirectX::XMMATRIX viewProj = DirectX::XMMatrixIdentity();

const RoomCamera *activeCam = g_RoomManager.GetActiveCamera();

if (activeCam) {

DirectX::XMVECTOR camEye = DirectX::XMVectorSet(

activeCam->from.x, activeCam->from.y, activeCam->from.z, 1.0f);

DirectX::XMVECTOR camTarget = DirectX::XMVectorSet(

activeCam->to.x, activeCam->to.y, activeCam->to.z, 1.0f);

DirectX::XMVECTOR camUp = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

DirectX::XMMATRIX viewMatrix =

DirectX::XMMatrixLookAtLH(camEye, camTarget, camUp);

float fovY = 2.0f * std::atan2(120.0f, activeCam->fov);

float aspect = 320.0f / 240.0f; // 4:3 PS1 aspect ratio

DirectX::XMMATRIX projMatrix =
    DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, 100.0f, 100000.0f);
// Authentic PS1 GsSetRefView2 Right-Handed (+X Right, +Y Down, +Z Forward):
// XMMatrixLookAtLH with camUp=(0,-1,0) negates the camera Right (+X) axis,
// horizontally mirroring 3D geometry against the 2D .BSS background.
projMatrix.r[0].m128_f32[0] = -projMatrix.r[0].m128_f32[0];
viewProj = viewMatrix * projMatrix;

}

// Render Frame

g_Renderer.BeginFrame();

if (PSX::g_MoviePlayer.IsPlaying()) {
  PSX::g_MoviePlayer.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline());
  g_Renderer.EndFrame();
  frameCount++;
  continue;
}
if (Game::g_TitleScreen.IsActive()) {
  Game::g_TitleScreen.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline());
  g_Renderer.EndFrame();
  frameCount++;
  continue;
}

// 1. Composite Pre-rendered Background into offscreen target

if (hasBg && !Game::g_DoorTransition.IsActive()) {

g_Renderer.RenderBackground();

}

// 2. Render 3D Scene Geometry

if (!Game::g_DoorTransition.IsActive()) {

if (hasPlayer) {

g_Player.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline(),

viewProj);

} else if (hasTmd) {

g_Renderer.RenderTmdMesh(rotationX, rotationY, 1.0f);

} else {

g_Renderer.RenderTestTriangle(rotationY);

}

// Render 3D Enemy Zombie

if (hasZombie && g_Zombie.IsLoaded() &&

g_RoomManager.GetCurrentStage() == 1 &&

g_RoomManager.GetCurrentRoomId() == 0) {

g_Zombie.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline(),

viewProj);

}

} else {

// Render Iconic 3D Door Transition Animation Sequence!

Game::g_DoorTransition.Render(g_Renderer.GetCommandList(),

g_Renderer.GetPS1Pipeline());

}

// 2.5. Render 2D Authentic Status / Inventory Screen overlay

if (g_Inventory.IsOpen()) {

g_Inventory.Render(g_Renderer.GetCommandList(),

g_Renderer.GetPS1Pipeline());

}

// 2.6. Render In-Game Text Box / Subtitle / Examine overlay

if (Game::g_ItemBox.IsOpen()) {
Game::g_ItemBox.Render(g_Renderer.GetCommandList(),
g_Renderer.GetPS1Pipeline());
}

if (Game::g_TextBox.IsActive()) {

Game::g_TextBox.Render(g_Renderer.GetCommandList(),

g_Renderer.GetPS1Pipeline());

}

// 3. Upscale and Resolve pass to swapchain (DLSS / Bicubic Sharpening)

g_Renderer.EndFrame();

if (frameCount % 15 == 0) {

UpdateWindowTitle(hWnd);

}

frameCount++;

if (frameCount % 60 == 0) {

std::printf("\r[Engine] DX12 PS1 + DLSS Pipeline (Frames: %u | Stage: %u "

"| Room: 0x%02X | Cut: %u | Mode: %ls)",

frameCount, g_RoomManager.GetCurrentStage(),

g_RoomManager.GetCurrentRoomId(),

g_RoomManager.GetActiveCut(),

g_Renderer.GetUpscalerModeName());

std::fflush(stdout);

}

}

std::printf("\n\n[Engine] Shutting down renderer and hardware layer...\n");

Game::g_TitleScreen.Shutdown();
Game::g_ItemBox.Shutdown();
g_StageManager.Shutdown();

g_Renderer.Shutdown();

PSX::Memory::Shutdown();

DestroyWindow(hWnd);

std::printf("[Engine] Clean shutdown complete.\n");

return 0;

}