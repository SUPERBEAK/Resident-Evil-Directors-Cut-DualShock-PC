with open('AGENTS.md', 'r', encoding='utf-8') as f:
    agents = f.read()

entry = """- **2026-10-25 [Antigravity]**: Completed Milestone 2: Enemy AI, Spawning, Hit Detection & Damage Reactions (Task 6.5b):
  1. Enemy 3D Model & Skeletal Hierarchy: Decoded Capcom EMD footer offsets in `EM1000.EMD` (Mansion dining room zombie), parsed 15 joint TMD meshes (2,022 render vertices), 256x256 8-bit paletted TIM texture, and 225 EMR animation frames with forward kinematic bone transformations.
  2. Authentic Zombie State Machine: Implemented comprehensive state transitions in [`enemy_zombie.cpp`](port/src/game/enemy_zombie.cpp):
     - Idle Standing / Groan Sway (Clip 2: frames 0..18).
     - Shamble Walk tracking player (Clip 0: frames 19..39) with angular lerp steering.
     - Arms Forward Lunge attack (Clip 3: frames 40..60).
     - Neck Bite Grapple (Clip 4: frames 61..75).
     - Bullet Impact Stagger / Knockback (Clip 5: frames 76..102).
     - Backward Death Collapse (Clip 7: frames 103..127) and static floor corpse.
  3. Combat Ballistics & Hitscan: Implemented `EnemyZombie::CheckBulletHit` with weapon angle cone detection, damage application (40 HP, ~4 handgun shots to kill), backward knockback displacement, collision sliding with room walls, and death collapse.
  4. Room VAB SPU Audio: Decoded authentic PS1 SPU audio directly from `STAGE1/ROOM1000.RDT` VAB body: Zombie Groan (35,420 samples), Bite (38,668 samples), Bullet Hit squelch (13,160 samples), and Death collapse thud (6,972 samples).
  5. UI Status & Drop Shadow: Added ground-contact drop shadow quad beneath zombie's boots and real-time state display in window title bar (`[Zombie: 40 HP | Shamble]` / `[Zombie: Defeated]`).
"""

agents = agents.rstrip() + "\n" + entry + "\n"
with open('AGENTS.md', 'w', encoding='utf-8') as f:
    f.write(agents)
print('AGENTS.md updated.')
