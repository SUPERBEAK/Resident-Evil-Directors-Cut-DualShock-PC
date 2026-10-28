with open('PROJECT_PARITY.md', 'r', encoding='utf-8') as f:
    parity = f.read()

target = '- [ ] **Task 6.5b (Milestone 2)**: Enemy AI, Spawning, Hit Detection & Damage Reactions (Zombies, Cerberus). `[TODO]`'
replacement = """- [x] **Task 6.5b (Milestone 2)**: Enemy AI, Spawning, Hit Detection & Damage Reactions (Zombies, Cerberus): `[DONE: Antigravity]`
    - Loaded 3D enemy zombie model (`EM1000.EMD`): 15 joint meshes (2,022 render vertices), 256x256 8-bit paletted TIM texture, and 225 EMR animation frames with forward kinematics.
    - Implemented full zombie finite state machine: Idle Groan sway (Clip 2: frames 0..18), Shamble walk tracking player (Clip 0: frames 19..39), Arms forward lunge (Clip 3: frames 40..60), Neck bite grapple (Clip 4: frames 61..75), Bullet impact stagger/knockback (Clip 5: frames 76..102), Death collapse (Clip 7: frames 103..127), and static floor corpse.
    - Implemented ballistics / hitscan raycast from Chris's handgun aim vector to zombie bounding cylinder with damage application (40 HP / 4 handgun shots to kill), knockback recoil, and death collapse.
    - Decoded authentic PS1 SPU room audio directly from `STAGE1/ROOM1000.RDT` VAB body: Zombie Groan (35,420 samples), Bite (38,668 samples), Bullet Hit squelch (13,160 samples), and Death collapse thud (6,972 samples).
    - Integrated zombie drop shadow quad and real-time state display in window title bar (`[Zombie: 40 HP | Shamble]` / `[Zombie: Defeated]`)."""

if target in parity:
    parity = parity.replace(target, replacement)
    with open('PROJECT_PARITY.md', 'w', encoding='utf-8') as f:
        f.write(parity)
    print('PROJECT_PARITY.md updated.')
else:
    print('Warning: target not found in PROJECT_PARITY.md.')
