with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# 1. Include & Global
inc_old = '#include "player_character.h"'
inc_new = '#include "enemy_zombie.h"\n#include "player_character.h"'
if inc_old in src and '#include "enemy_zombie.h"' not in src:
    src = src.replace(inc_old, inc_new, 1)
    print('Include patched.')

glob_old = 'static Game::PlayerCharacter g_Player;'
glob_new = 'static Game::PlayerCharacter g_Player;\nstatic Game::EnemyZombie g_Zombie;'
if glob_old in src and 'static Game::EnemyZombie g_Zombie;' not in src:
    src = src.replace(glob_old, glob_new, 1)
    print('Global patched.')

# 2. UpdateWindowTitle
title_old = '''  const wchar_t *aimStr = L"";
  if (g_Player.IsAiming()) {
    aimStr = (g_Player.GetAimPitch() == Game::AimPitch::Up)
                 ? L" [AIM: UP]"
                 : (g_Player.GetAimPitch() == Game::AimPitch::Down
                        ? L" [AIM: DOWN]"
                        : L" [AIM: STRAIGHT]");
  }'''

title_new = '''  const wchar_t *aimStr = L"";
  if (g_Player.IsAiming()) {
    aimStr = (g_Player.GetAimPitch() == Game::AimPitch::Up)
                 ? L" [AIM: UP]"
                 : (g_Player.GetAimPitch() == Game::AimPitch::Down
                        ? L" [AIM: DOWN]"
                        : L" [AIM: STRAIGHT]");
  }

  static wchar_t zombieStr[64] = L"";
  if (g_Zombie.IsLoaded()) {
    if (g_Zombie.IsAlive()) {
      const wchar_t *st = (g_Zombie.GetState() == Game::ZombieAnimState::Shamble)
                              ? L"Shamble"
                              : ((g_Zombie.GetState() == Game::ZombieAnimState::Lunge)
                                     ? L"Lunge"
                                     : ((g_Zombie.GetState() == Game::ZombieAnimState::Bite)
                                            ? L"BITE"
                                            : ((g_Zombie.GetState() == Game::ZombieAnimState::HitRecoil)
                                                   ? L"STAGGER"
                                                   : L"Idle")));
      swprintf_s(zombieStr, L" [Zombie: %d HP | %ls]", g_Zombie.GetHealth(), st);
    } else {
      wcscpy_s(zombieStr, L" [Zombie: Defeated]");
    }
  }'''

if title_old in src:
    src = src.replace(title_old, title_new, 1)
    print('Title strings patched.')

title_fmt1 = 'L"[Beretta 92FS: %d/%d]%ls "'
title_repl1 = 'L"[Beretta 92FS: %d/%d]%ls%ls "'
if title_fmt1 in src:
    src = src.replace(title_fmt1, title_repl1)
    print('Title format patched.')

title_call1 = 'g_Player.GetMaxAmmo(), aimStr, bgmBank, bgmBank + 1,'
title_call_repl1 = 'g_Player.GetMaxAmmo(), aimStr, zombieStr, bgmBank, bgmBank + 1,'
if title_call1 in src:
    src = src.replace(title_call1, title_call_repl1)
    print('Title call 1 patched.')

title_call2 = 'g_Player.GetMaxAmmo(), aimStr, g_Renderer.GetUpscalerModeName());'
title_call_repl2 = 'g_Player.GetMaxAmmo(), aimStr, zombieStr, g_Renderer.GetUpscalerModeName());'
if title_call2 in src:
    src = src.replace(title_call2, title_call_repl2)
    print('Title call 2 patched.')

# 3. Model Loading
load_old = '''  if (!hedPath.empty() && !vbPath.empty()) {
    g_AudioEngine.LoadWeaponAudio(hedPath, vbPath);
  }'''

load_new = '''  if (!hedPath.empty() && !vbPath.empty()) {
    g_AudioEngine.LoadWeaponAudio(hedPath, vbPath);
  }

  // 6e. Load Authentic 3D Enemy Zombie (EM1000.EMD - Dining Room Zombie)
  std::wstring emdZombieCandidates[] = {
      L"extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",
      L"../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",
      L"../../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",
      L"../../../extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD",
      L"H:/Antigravity Projects/ultimate dualshock/extracted/RE1-Dualshock/PSXUSA/ENEMY/EM1000.EMD"};

  bool hasZombie = false;
  for (const auto &path : emdZombieCandidates) {
    if (std::filesystem::exists(path)) {
      hasZombie = g_Zombie.Initialize(g_Renderer.GetDevice(),
                                      g_Renderer.GetCommandQueue(), path);
      if (hasZombie) {
        std::wprintf(L"[Engine] Loaded 3D enemy zombie from: %ls\\n",
                     path.c_str());
        // Position Zombie in Mansion Dining Room (standing/shambling across room)
        g_Zombie.SetPosition(2600.0f, 0.0f, 5652.0f);
        g_Zombie.SetFacingAngle(3.14159f / 2.0f);
        break;
      }
    }
  }'''

if load_old in src and 'emdZombieCandidates' not in src:
    src = src.replace(load_old, load_new, 1)
    print('Model loading patched.')

# 4. Update Loop
update_old = '''    // Update Player Tank Controls and Skeletal Animation
    if (hasPlayer) {
      g_Player.Update(1.0f / 60.0f, g_Pad.buttons, &g_RoomManager);
    }'''

update_new = '''    // Update Player Tank Controls and Skeletal Animation
    if (hasPlayer) {
      g_Player.Update(1.0f / 60.0f, g_Pad.buttons, &g_RoomManager);
    }

    // Update Enemy Zombie AI, Hitscan Combat, and Skeletal Animation
    if (hasZombie && g_Zombie.IsLoaded()) {
      bool bulletFired = g_Player.JustFiredBullet();
      g_Zombie.Update(1.0f / 60.0f, g_Player.GetPosition(), bulletFired, &g_RoomManager);
      if (bulletFired) {
        g_Zombie.CheckBulletHit(g_Player.GetPosition(), g_Player.GetFacingAngle(),
                                (int)g_Player.GetAimPitch(), 10);
      }
    }'''

if update_old in src and 'g_Zombie.Update' not in src:
    src = src.replace(update_old, update_new, 1)
    print('Update loop patched.')

# 5. Render Pass
render_old = '''    // 2. Render 3D Player Character on floor of room, or standalone weapon
    if (hasPlayer) {
      g_Player.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline(),
                      viewProj);
    } else if (hasTmd) {
      g_Renderer.RenderTmdMesh(rotationX, rotationY, 1.0f);
    } else {
      g_Renderer.RenderTestTriangle(rotationY);
    }'''

render_new = '''    // 2. Render 3D Player Character on floor of room, or standalone weapon
    if (hasPlayer) {
      g_Player.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline(),
                      viewProj);
    } else if (hasTmd) {
      g_Renderer.RenderTmdMesh(rotationX, rotationY, 1.0f);
    } else {
      g_Renderer.RenderTestTriangle(rotationY);
    }

    // Render 3D Enemy Zombie
    if (hasZombie && g_Zombie.IsLoaded()) {
      g_Zombie.Render(g_Renderer.GetCommandList(), g_Renderer.GetPS1Pipeline(),
                      viewProj);
    }'''

if render_old in src and 'g_Zombie.Render' not in src:
    src = src.replace(render_old, render_new, 1)
    print('Render pass patched.')

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(src)
print('main.cpp updated successfully.')
