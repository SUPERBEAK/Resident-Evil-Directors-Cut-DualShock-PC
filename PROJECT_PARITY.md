# RESIDENT EVIL: DUAL SHOCK VER. (PC DX12 PORT) - PARITY & PROGRESS TRACKER

This tracker maintains parity status against the original PS1 game (`SLUS_007.47` + 12 overlays) and monitors active tasks between **Antigravity** and **Gemma (Cline)**.

---

## 1. High-Level Parity Matrix

| Subsystem | Target Fidelity / Feature Set | Status | Owner | Notes |
|---|---|---|---|---|
| **Asset Extraction** | Disc ISO/BIN extraction (DATA, SOUND, STAGE, MOVIE, VOICE) | 🟢 DONE | Antigravity | 987 files extracted with dumpsxiso |
| **Memory Map & HAL** | Virtual RAM arena (fixed PS1 addrs & scratchpad), PsyQ shims | 🟢 DONE | Antigravity | 2MB RAM + Scratchpad mapped & verified |
| **Renderer (DX12)** | Modern DX12 backend, command list / PSO pipeline | 🟢 DONE | Antigravity | Device, queue, swapchain, render targets & loop |
| **PS1 Shader Emulation** | Fixed-point vertex jitter, affine texturing, 15-bit color, dither | 🟢 DONE | Antigravity | HLSL shader with embedded CSOs & live rendering |
| **Background / MDEC** | 320x240 MDEC background decode, depth ordering (GsOT) | 🟢 DONE | Antigravity | BSS loader & MDEC target quad composite |
| **Upscaling / DLSS** | DLSS 5 / OptiScaler / RenoDX neural rendering, motion vectors, F1 mode toggle | 🟢 DONE | Antigravity | 320x240 offscreen targets, NVSDK NGX evaluate, DLSS 5 feed |
| **Audio (SPU / XA)** | SPU ADPCM voices, HSB (VAB+SEQ) music synth, XA streaming | 🟢 DONE | Antigravity / Gemma | Authentic sound & music replay |
| **Input & Vibration** | DualShock analog response, deadzones, dual-motor force feedback | ⚪ PLANNED | Gemma | XInput / SDL gamepad backend |
# RESIDENT EVIL: DUAL SHOCK VER. (PC DX12 PORT) - PARITY & PROGRESS TRACKER

This tracker maintains parity status against the original PS1 game (`SLUS_007.47` + 12 overlays) and monitors active tasks between **Antigravity** and **Gemma (Cline)**.

---

## 1. High-Level Parity Matrix

| Subsystem | Target Fidelity / Feature Set | Status | Owner | Notes |
|---|---|---|---|---|
| **Asset Extraction** | Disc ISO/BIN extraction (DATA, SOUND, STAGE, MOVIE, VOICE) | 🟢 DONE | Antigravity | 987 files extracted with dumpsxiso |
| **Memory Map & HAL** | Virtual RAM arena (fixed PS1 addrs & scratchpad), PsyQ shims | 🟢 DONE | Antigravity | 2MB RAM + Scratchpad mapped & verified |
| **Renderer (DX12)** | Modern DX12 backend, command list / PSO pipeline | 🟢 DONE | Antigravity | Device, queue, swapchain, render targets & loop |
| **PS1 Shader Emulation** | Fixed-point vertex jitter, affine texturing, 15-bit color, dither | 🟢 DONE | Antigravity | HLSL shader with embedded CSOs & live rendering |
| **Background / MDEC** | 320x240 MDEC background decode, depth ordering (GsOT) | 🟢 DONE | Antigravity | BSS loader & MDEC target quad composite |
| **Upscaling / DLSS** | DLSS 5 / OptiScaler / RenoDX neural rendering, motion vectors, F1 mode toggle | 🟢 DONE | Antigravity | 320x240 offscreen targets, NVSDK NGX evaluate, DLSS 5 feed |
| **Audio (SPU / XA)** | SPU ADPCM voices, HSB (VAB+SEQ) music synth, XA streaming | 🟢 DONE | Antigravity / Gemma | Authentic sound & music replay |
| **Input & Vibration** | DualShock analog response, deadzones, dual-motor force feedback | ⚪ PLANNED | Gemma | XInput / SDL gamepad backend |
| **Game Logic & Overlays**| SLUS_007.47 engine + STAGE1-7 & screen overlays | 🟡 IN PROGRESS: Gemma | 100% decompiled C port |
| **Title Screen Overlay** | Title state machine, menu navigation, sprite loading | 🟡 IN PROGRESS: Gemma | Based on title_init / title_menu_update |
| **FMV Player** | STR playback with stereo XA audio sync & narration skips | ⚪ PLANNED | Gemma | Intro and ending movies |
| **Title Screen Overlay** | Title state machine, menu navigation, sprite loading | 🟡 IN PROGRESS: Gemma | Based on title_init / title_menu_update |
| **FMV Player** | STR playback with stereo XA audio sync & narration skips | ⚪ PLANNED | Gemma | Intro and ending movies |
| **Title Screen Overlay** | Title state machine, menu navigation, sprite loading | 🟡 IN PROGRESS: Antigravity | Based on `title_init` / `title_menu_update` |
| **FMV Player** | STR playback with stereo XA audio sync & narration skips | 🟢 DONE | Antigravity | Intro, Capcom logo, and ending movies |
## 2. Granular Task Board

### Legend:
- ⚪ `TODO`
- 🟡 `IN PROGRESS: <Agent>`
- 🟢 `DONE: <Agent>`
- 🔴 `BLOCKED: <Reason>`

---

### [Phase 1: Tooling & Disc Extraction]
- [x] **Task 1.1**: Set up `AGENTS.md` and `PROJECT_PARITY.md` tracking files. `[DONE: Antigravity]`
- [x] **Task 1.2**: Receive disc dump (ISO / CUE+BIN) from user into `game_files/`. `[DONE: Antigravity]` (Verified SHA1 `08fbc01ae9c3d47aa93d2a99d69fa67a54e4982b`)
- [x] **Task 1.3**: Extract all disc files to `extracted/RE1-Dualshock`. `[DONE: Antigravity]` (987 files extracted with `dumpsxiso`, XML + SHA1 catalogue generated)
- [x] **Task 1.4**: Configure CMake / MSVC solution for 64-bit Windows build in `port/`. `[DONE: Antigravity]`

---

### [Phase 2: PS1 Hardware Abstraction Layer (HAL)]
- [x] **Task 2.1**: Implement memory arena allocator (mapping 0x80000000-0x80200000 and 0x1F800000 Scratchpad). `[DONE: Antigravity]`
- [x] **Task 2.2**: Implement PsyQ GTE vector / matrix math library (`RotMatrix`, `TransMatrix`, etc.). `[DONE: Antigravity]`
- [x] **Task 2.3**: Implement cooperative task scheduler (`task_sleep` coroutines / fibers). `[DONE: Antigravity]`
- [x] **Task 2.4**: Implement PsyQ CD-ROM and memory card file shims. `[DONE: Antigravity]`

---

### [Phase 3: DirectX 12 Graphics Engine & Shaders]
- [x] **Task 3.1**: DX12 Device, Command Queue, Swapchain, and Descriptor Heaps initialization. `[DONE: Antigravity]`
- [x] **Task 3.2**: HLSL shader for PS1 authentic pipeline: `[DONE: Antigravity]`
  - Coordinate truncation / vertex snapping (emulating GTE integer math).
  - Affine texture mapping vs perspective-correct toggle.
  - 15-bit color reduction and 4x4 Bayer dithering matrix.
  - CLUT 4-bit / 8-bit palette lookups.
  - PS1 semi-transparency blending modes (50%/50%, Add, Subtract, 25%/100%).
- [x] **Task 3.3**: Pre-rendered camera background quad drawing and depth masking. `[DONE: Antigravity]`
- [x] **Task 3.5**: Authentic PS1 TMD 3D Model parser & TIM texture loader composited over pre-rendered room backgrounds with interactive pad rotation. `[DONE: Antigravity]`
- [x] **Task 3.6**: Authentic 3D Player Character (Chris Redfield), Forward Kinematics, EDD Motion Sequencer, Multi-CLUT decoding, and Tank Controls. `[DONE: Antigravity]`
- [x] **Task 3.4**: Motion vector generation and DLSS / upscaler pass integration (Halton jitter, `SV_Target1` screen velocity, dynamic DLSS/Streamline DLL detector & Bicubic CAS fallback). `[DONE: Antigravity]`


---

### [Phase 4: Audio Engine (SPU, Sequence & XA)]
- [x] **Task 4.1**: WASAPI / XAudio2 streaming output buffer. [DONE: Gemma]
- [x] **Task 4.2**: PS1 ADPCM sample decoder and SPU envelope/reverb emulator. [DONE: Gemma]
- [x] **Task 4.3**: `SEPxx.HSB` sequencer and VAB soundfont playback. `[DONE: Antigravity]`
  - Upgraded synthesis to native 44.1 kHz stereo PCM with high-precision linear interpolation.
  - Implemented authentic SPU ADPCM sample looping (`block[1] & 0x04` loop start, `block[1] & 0x01` loop end).
  - Implemented multi-track concurrent playback for SEP sequence packages (up to 3 tracks simultaneously).
  - Added 450ms smooth exponential ADSR release envelope and CC 11 Expression volume dynamics.
- [x] **Task 4.4**: XA audio stream decoding (`VOICE/*.XAS`). [DONE: Gemma]

---

### [Phase 5: Controller, Input & Haptics]
- [x] **Task 5.1**: Gamepad input mapping with original PS1 DualShock analog sensitivity curves. [DONE: Gemma]
- [x] **Task 5.2**: DualShock vibration motor emulation to XInput triggers/rumble. [DONE: Gemma]

---

### [Phase 6: Gameplay Logic Integration]
- [x] **Task 6.1**: Port `src/main/` engine and main game loop (Memory Card Shim & Save Subsystem Infrastructure). `[DONE: Gemma]`
- [x] **Task 6.2**: Link and load stage overlays (`src/stage1/` through `src/stage7/`). `[DONE: Gemma]`
- [x] **Task 6.3**: Stage & Room Manager: dynamic camera switches, authentic GTE right-handed camera projection, incoming door camera targeting, and full-game room wiring across STAGE1-8. `[DONE: Antigravity / Gemma]`
- [x] **Task 6.4**: Room Collision (SCA boundaries): Circle-box and circle-circle authentic PS1 sliding collision across all 109 playable rooms and 1,811 boundaries. `[DONE: Antigravity]`
- [x] **Task 6.5**: Combat, Enemy AI, Health System, and Item Inventory:
  - [x] **Task 6.5a (Milestone 1)**: Authentic Combat, Weapons & Aiming Subsystem: `[DONE: Antigravity]`
    - Attached 3D handgun model (`WS212.TMD`, 138 render vertices) to Chris Redfield's Right Hand (Joint 11).
    - Loaded authentic weapon animation table from `W02.EMW` (all 203 frames / 15 EDD clips): Relaxed Idle, Walk/Run Locomotion with weapon, Gun Raise, Straight/Up/Down Aim Stances and Transitions, 3-Angle Muzzle Recoil Kicks, and Magazine Reload cycle.
    - Decoded authentic PS1 SPU weapon audio directly from `WP002.HED` and `WP002.VB` (Gunshot: 23,408 samples, Dry-Fire Click: 5,880 samples, Reload: 9,212 samples).
    - Integrated dual-channel controls (XInput R1 / Keyboard `F` / Right-Click Aim, Enter / Left-Click / Pad Cross Shoot, `R` Reload) with authentic DualShock haptic rumble on gunshot kick.
  - [x] **Task 6.5b (Milestone 2)**: Enemy AI, Spawning, Hit Detection & Damage Reactions (Zombies, Cerberus): `[DONE: Antigravity]`
    - Loaded 3D enemy zombie model (`EM1000.EMD`): 15 joint meshes (2,022 render vertices), 256x256 8-bit paletted TIM texture, and 225 EMR animation frames with forward kinematics.
    - Implemented full zombie finite state machine: Idle Groan sway (Clip 2: frames 0..18), Shamble walk tracking player (Clip 0: frames 19..39), Arms forward lunge (Clip 3: frames 40..60), Neck bite grapple (Clip 4: frames 61..75), Bullet impact stagger/knockback (Clip 5: frames 76..102), Death collapse (Clip 7: frames 103..127), and static floor corpse.
    - Implemented ballistics / hitscan raycast from Chris's handgun aim vector to zombie bounding cylinder with damage application (40 HP / 4 handgun shots to kill), knockback recoil, and death collapse.
    - Decoded authentic PS1 SPU zombie audio directly from `SOUND/CHAR00.HED` and `CHAR00.VB`: Vocal Groan (Tone 2), Bite (Tone 5), Bullet Hit squelch (Tone 7), and Death howl (Tone 4).
    - Integrated zombie drop shadow quad and real-time state display in window title bar (`[Zombie: 40 HP | Shamble]` / `[Zombie: Defeated]`).
  - [x] **Task 6.5c (Milestone 3)**: Inventory, Item Interaction & Status Screen UI (TIM 2D menus): `[DONE: Antigravity]`
    - Decoded authentic PS1 8-bit TIM status screen (`DATA/STATUS.TIM`, 256x256) and multi-CLUT palettes (Palette 0 UI border, Palette 1 poison/danger, Palette 2 item icon 256-color palette).
    - Extracted all 76 uncompressed item icons (40x30 8-bit pixels each = 91,200 bytes) from `DATA/ITEM_ALL.PIX` and built a unified 512x512 GPU texture atlas.
    - Decoded all 77 authentic item names directly from `SLUS_007.47` string table (`0x8009503C`).
    - Implemented authentic 6-slot inventory screen with yellow cursor navigation, EKG heartbeat monitor, and action submenus (Equip, Use, Check, Combine).
    - Added herb and ammo combining rules (Handgun Clip + Beretta, Green + Red Herb, Green + Green Herb) with sound cues and inventory reordering.
  - [x] **Task 6.5d (Milestone 4)**: Room Script Bytecode (SCD / Script VM) Execution & Event Triggers: `[DONE: Antigravity]`
    - Decoded authentic PS1 SCD bytecode engine matching `dualshock/src/main/scd.c` and `SLUS_007.47` symbol tables.
    - Implemented all 10 flag banks (Scenario, Scenario2, Locks, Enemies, g_SysFlags, g_GameStateFlags, g_UpdateFlags, RoomItems, Room, ItemUse) with MSB-first bit addressing (`0x80000000 >> bit`).
    - Implemented 83-entry table-driven opcode parser decoding item spawns (`0x18 item_model_set`), enemy spawns (`0x1B enemy_set`), door triggers (`0x0C door_set`), and room actions (`0x0D room_action_set`).
    - Verified live execution across RDT scripts (Stage 1 Room 00 parsed 3 item spawns in the Mansion Dining Room).
  - [x] **Task 6.5e (Milestone 5)**: Interactive World Item Pickups & Inventory Integration: `[DONE: Antigravity]`
    - Integrated real-time proximity detection for SCD spawned world items (`PickupItemAtPosition`) with 600-unit interaction sphere.
    - Added item collection on action key (`E`, Space, Enter, Gamepad Cross): transfers item to active 6-slot inventory, marks permanent collection bit in SCD flag bank 7 (RoomItems), and updates HUD.
    - Decoded authentic PS1 UI audio directly from `SOUND/SELECT.HED` and `SELECT.VB` (Menu Cursor blip: 756 samples, Confirm chime / Item pickup: 3,052 samples).
  - [x] **Task 6.5f (Milestone 6)**: Door Transition Sequences, Key Lock Verification & Authentic SPU Creak Audio: `[DONE: Antigravity]`
    - Implemented door lock verification: queries inventory with `g_Inventory.HasItem(keyId)` (Sword Key, Armor Key, Shield Key, Helmet Key); plays authentic dry-fire locked rattle if key is missing.
    - Decoded authentic PS1 wooden door sound effects directly from `SOUND/DOORWOD.HED` and `DOORWOD.VB` (Door Creak Open: 22,764 samples, Door Slam Shut: 14,784 samples).
    - Seamless room transitions with camera perspective handover and automatic execution of destination room's SCD script bytecode.
  - [x] **Task 6.5g (Milestone 7)**: Iconic 3D Door Transition Animation Sequences (ITEM_M1/DOORxx.DOR): `[DONE: Antigravity]`
    - Loaded 3D door transition assets from `ITEM_M1/DOORxx.DOR` (`DOOR00.DOR` through `DOOR15.DOR`).
    - Decoded 3D TMD door models (12 objects: door frame and swinging door leaf/handle/latch) and 8-bit paletted TIM textures (`0xafd4`).
    - Implemented authentic cinematic transition timeline: unlatching handle down (0.0s to 0.35s), smooth door swing ease-in-out open (0.35s to 2.05s) with camera dolly glide forward into darkness, alpha fade to black (2.05s to 2.45s), and room handover.
    - Synchronized with authentic PS1 door audio (`PlayDoorOpen()`, `PlayDoorClose()`) and interactive preview hotkey (`U`).
  - [x] **Task 6.5h (Milestone 8)**: In-Game Text Box, Examine System, Subtitles & Yes/No Prompts: `[DONE: Antigravity]`
    - Decoded authentic 4-bit indexed `DATA/FONT.TIM` font atlas and embedded crisp fallback bitmap glyphs with drop shadows.
    - Implemented lower-third semi-transparent dark banner (320x240 coordinates) with authentic silver border lines and blinking prompt cursor.
    - Implemented typewriter text reveal cadence (~120 chars/sec) with instant button skip and multi-page advancing.
    - Implemented interactive Yes/No selection prompts with red cursor navigation (`>` indicator) and audio feedback (`PlayMenuCursor()`).
    - Pre-cached all 64 global messages from `SLUS_007.47` string table (`0x800944BC`) and decoded room-specific MSG sections from RDT header offset `0x74`.
    - Integrated with room scenery examination (fireplace, grandfather clock, dining table) and interactive preview hotkeys (`T` for text, `Y` for prompt).

  - [x] **Task 6.5i (Milestone 9)**: Room Collision Multi-Floor Isolation, 16-Bit Coordinate Overflow Resolution, Authentic Opcode Stepper & Main Hall Staircase: `[DONE: Antigravity]`
    - Diagnosed and eliminated periodic "shhhh" noise: fixed VAG size table offset in VAB parser (`vbOff - 512`), isolated zombie voice groans strictly to rooms with active enemies, and eliminated `CHAR00.VB` groan loading.
    - Resolved staircase blockage and hallway clipping: evaluated `floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;` in `ResolveCollision` so 2nd-floor balcony railings no longer act as invisible barriers on 1st-floor staircases and corridors.
    - Fixed 16-bit coordinate overflow: upgraded `RoomDoor` and `RoomCamSwitch` coordinate fields from `int16_t` to `uint16_t`, fixing 990 doors and 308 camera switch quads that wrapped negative.
    - Implemented authentic opcode-aware door scanner in `RoomManager::ParseRdt`: eliminated all false ghost doors that blocked hallways.
    - Implemented authentic Main Hall staircase interactivity: seamlessly transitions between 1F (`Stage 1 Room 0x01`) and 2F Balcony (`Stage 2 Room 0x03`).
  - [x] **Task 6.5j (Milestone 10)**: PSX Audio Sequencer Synthesis Overhaul & 57-Track Complete Music Extraction: `[DONE: Antigravity]`
    - Diagnosed audio rendering discrepancies matching OST reference ("Meeting Rebecca" / `SEP0B.HSB`): multi-track dropping, missing sample looping, nearest-neighbor aliasing, and 25ms choke-off clicks.
    - Implemented authentic sample looping, linear interpolation, 450ms natural ADSR release curves, CC 11 Expression support, and multi-track sequence mixing in [`psx_seq.cpp`](port/src/audio/psx_seq.cpp).
    - Built and executed standalone native music extraction tool [`extract_music.exe`](port/src/extract_music.cpp), batch-rendering all 57 authentic DualShock music tracks (`SEP00.HSB` .. `SEP38.HSB`) to 44.1 kHz 16-bit stereo `.WAV` files in `extracted/music_wav/`.
  - [x] **Task 6.5k (Milestone 11)**: Item Box (Magic Storage Chest) & Typewriter Save/Load System: [DONE: Antigravity]
    - Created binary save state serialization engine in [save_manager.h](port/include/save_manager.h) and [save_manager.cpp](port/src/game/save_manager.cpp) (RE1DSAV format, version 1, serializing player XYZ, facing angle, 140 HP, equipped weapon, 6 player inventory slots, 32 storage chest slots, and 10 SCD VM flag banks).
    - Implemented dual-column Item Box screen in [item_box.h](port/include/item_box.h) and [item_box.cpp](port/src/game/item_box.cpp) with 6-slot player inventory, 32-slot scrollable chest, item icons, quantities, deep royal blue beveled panels, item description display, and interactive [STORE] / [TAKE] command submenus.
    - Decoded authentic Save Room UI audio from SELECT.HED and SELECT.VB: Tone 0 / VAG 1 (Item Box Open/Close, 13,104 samples), Tone 1 / VAG 2 (Typewriter Keystrokes & Bell, 305,928 samples), and Tone 4 / VAG 5 (Menu Cancel/Buzz, 840 samples).
    - Wired authentic Save Room Typewriter interaction in [main.cpp](port/src/main.cpp): checks for Ink Ribbon (Item 47), displays authentic prompt, decrements ribbon on Yes, plays typewriter SFX, and writes saves/save_0.dat.
    - Implemented Quick Save (F5) and Quick Load (F6): instantly saves and restores player position, health, inventory, item box, room, camera cut, background slice, and room BGM.
    - Verified 100% clean Release build (
e1_dualshock_port.exe) and visual verification via itembox_captured.png running at 60 FPS on GPU.

  - [x] **Task 6.5l (Milestone 12)**: Authentic PS1 FMV / Movie Player (`.STR` Streaming & MDEC V2): `[DONE: Antigravity]`
    - Created `PSXMoviePlayer` subsystem in [`psx_str.h`](port/include/psx_str.h) and [`psx_str.cpp`](port/src/hal/psx_str.cpp) parsing 2,336-byte CD-ROM XA Mode 2 Form 2 sectors across all 24 authentic game movies (`extracted/RE1-Dualshock/PSXUSA/ZMOVIE/*.STR`).
    - Upgraded MDEC Decoder in [`psx_mdec.cpp`](port/src/hal/psx_mdec.cpp) to support Version 2 bitstreams (`0x0002` magic) with 10-bit signed direct DC decoding, MPEG-1 run-length AC codebooks, and column-major macroblock iteration (20 cols x 15 rows).
    - Decoded 37.8 kHz stereo 4-bit CD-ROM XA ADPCM audio via dual-history filter coefficients ($K_0, K_1$) streaming directly into dedicated XAudio2 voice queues.
    - Implemented dynamic D3D12 Texture2D upload buffer, fullscreen quad rasterization with PS1 pipeline shaders, skip hotkeys (Space, Enter, Escape, Gamepad Cross/Start), hotkey cycling (`P`), and CLI flag `--play-movie <id>`. Verified with `capcom_5s.png`.

  - [x] **Task 6.5m (Milestone 13)**: Full Game Progression & Puzzle Items (Crests, Emblems, Sheet Music, V-JOLT, MO Disks): `[DONE: Antigravity]`
    - Implemented `PuzzleManager` in [`puzzle_manager.h`](port/include/puzzle_manager.h) and [`puzzle_manager.cpp`](port/src/game/puzzle_manager.cpp) tracking progression states and SCD scenario/lock bits.
    - 4 Crests (Wind 35, Star 36, Sun 37, Moon 38) placed into Rear Garden door stone relief (Stage 1 Room 0x0E) with incremental countdown and crypt unlock (`crests_puzzle_confirmed.png`).
    - Musical Score (46) performed on Grand Piano (Stage 1 Room 0x05) to trigger Moonlight Sonata, open secret sliding panel, and award Gold Emblem (45) (`piano_puzzle_confirmed.png`).
    - Wooden Emblem (44) swapped into Dining Room fireplace mantle relief (Stage 1 Room 0x00/0x05) to disarm clock trap and unlock doors.
    - V-JOLT (76 / 39..43) applied to Plant 42 giant root system in Guardhouse B2 (Stage 3 Room 0x0F) to wither tendrils.
    - 3 MO Disks (50) authenticated at laboratory passcode terminals (Stage 5) to unlock partner holding cell.
    - Wired inventory `[USE]` action to `PuzzleManager::TryUseItem`, consuming used items, granting awards, playing confirm/cancel audio, and displaying feedback in authentic PS1 text boxes.
    - Added test CLI flags `--test-puzzle`, `--test-inventory`, `--test-use-piano`, and `--test-use-crests` for automated verification.

  - [x] **Task 6.5n (Milestone 14)**: Authentic PS1 XA Voice Acting (`VOICE/*.XAS`) & SCD Dialogue Triggering: `[DONE: Antigravity]`
    - Replace placeholder IMA ADPCM logic in [`xas_decoder.cpp`](port/src/audio/xas_decoder.cpp) with authentic 2,336-byte CD-ROM XA Mode 2 Form 2 sector parsing (16-channel interleaved 37.8 kHz mono 4-bit Sony XA ADPCM with filter coefficients $K_0, K_1$).
    - Extract `g_StageVoiceFileIndex` and `stage_voice_offset_tbl` across all 7 stage overlays (`STAGE1.EXE` .. `STAGE7.EXE`) to map voice clip IDs to exact `(VOICEn.XAS, channel, startSector, endSector)` ranges matching `voice_play_clip` in `cd.c`.
    - Integrate XAudio2 37.8 kHz mono voice streaming channel into [`AudioEngine`](port/include/audio_engine.h) and wire SCD opcode `0x1E` (`scd_op_1e_voice_play`) and interactive dialogue triggers.
  - [x] **Task 6.5o (Milestone 15)**: Automated Door Walker, Full 83-Opcode SCD Door Parser & Action Zone Linker: `[DONE: Antigravity]`
    - Replaced 7-opcode door scanner in [`room_manager.cpp`](port/src/room_manager.cpp) with complete 83-opcode length table and multi-block SCD iterator (`while ((len = *(u16*)b) != 0)`), linked `0x0D` (`room_action_set`) interactive boxes and `0x20` (`player_pos_set`) elevator cabin positions to `box=(0,0,1,1)` elevator/ladder doors, supported large outdoor exit bounds (`d > 6500`), mapped `STAGE6`/`STAGE7` cross-floor and Helmet Key rooms (`+5`), and restored authentic 600-unit (`0x258`) forward probe.
    - Built zero-token offline physics & 2D A* door walker ([`tools/verify_all_doors_walker.py`](tools/verify_all_doors_walker.py), 345/345 doors passing across 160 rooms) and in-game `F11` Door Walker Autopilot in [`main.cpp`](port/src/main.cpp).
| **Title Screen Overlay** | Title state machine, menu navigation, sprite loading | 🟡 IN PROGRESS: Antigravity | Based on `title_init` / `title_menu_update` |
| **FMV Player** | STR playback with stereo XA audio sync & narration skips | 🟢 DONE | Antigravity | Intro, Capcom logo, and ending movies |
## 2. Granular Task Board

### Legend:
- ⚪ `TODO`
- 🟡 `IN PROGRESS: <Agent>`
- 🟢 `DONE: <Agent>`
- 🔴 `BLOCKED: <Reason>`

---

### [Phase 1: Tooling & Disc Extraction]
- [x] **Task 1.1**: Set up `AGENTS.md` and `PROJECT_PARITY.md` tracking files. `[DONE: Antigravity]`
- [x] **Task 1.2**: Receive disc dump (ISO / CUE+BIN) from user into `game_files/`. `[DONE: Antigravity]` (Verified SHA1 `08fbc01ae9c3d47aa93d2a99d69fa67a54e4982b`)
- [x] **Task 1.3**: Extract all disc files to `extracted/RE1-Dualshock`. `[DONE: Antigravity]` (987 files extracted with `dumpsxiso`, XML + SHA1 catalogue generated)
- [x] **Task 1.4**: Configure CMake / MSVC solution for 64-bit Windows build in `port/`. `[DONE: Antigravity]`

---

### [Phase 2: PS1 Hardware Abstraction Layer (HAL)]
- [x] **Task 2.1**: Implement memory arena allocator (mapping 0x80000000-0x80200000 and 0x1F800000 Scratchpad). `[DONE: Antigravity]`
- [x] **Task 2.2**: Implement PsyQ GTE vector / matrix math library (`RotMatrix`, `TransMatrix`, etc.). `[DONE: Antigravity]`
- [x] **Task 2.3**: Implement cooperative task scheduler (`task_sleep` coroutines / fibers). `[DONE: Antigravity]`
- [x] **Task 2.4**: Implement PsyQ CD-ROM and memory card file shims. `[DONE: Antigravity]`

---

### [Phase 3: DirectX 12 Graphics Engine & Shaders]
- [x] **Task 3.1**: DX12 Device, Command Queue, Swapchain, and Descriptor Heaps initialization. `[DONE: Antigravity]`
- [x] **Task 3.2**: HLSL shader for PS1 authentic pipeline: `[DONE: Antigravity]`
  - Coordinate truncation / vertex snapping (emulating GTE integer math).
  - Affine texture mapping vs perspective-correct toggle.
  - 15-bit color reduction and 4x4 Bayer dithering matrix.
  - CLUT 4-bit / 8-bit palette lookups.
  - PS1 semi-transparency blending modes (50%/50%, Add, Subtract, 25%/100%).
- [x] **Task 3.3**: Pre-rendered camera background quad drawing and depth masking. `[DONE: Antigravity]`
- [x] **Task 3.5**: Authentic PS1 TMD 3D Model parser & TIM texture loader composited over pre-rendered room backgrounds with interactive pad rotation. `[DONE: Antigravity]`
- [x] **Task 3.6**: Authentic 3D Player Character (Chris Redfield), Forward Kinematics, EDD Motion Sequencer, Multi-CLUT decoding, and Tank Controls. `[DONE: Antigravity]`
- [x] **Task 3.4**: Motion vector generation and DLSS / upscaler pass integration (Halton jitter, `SV_Target1` screen velocity, dynamic DLSS/Streamline DLL detector & Bicubic CAS fallback). `[DONE: Antigravity]`


---

### [Phase 4: Audio Engine (SPU, Sequence & XA)]
- [x] **Task 4.1**: WASAPI / XAudio2 streaming output buffer. [DONE: Gemma]
- [x] **Task 4.2**: PS1 ADPCM sample decoder and SPU envelope/reverb emulator. [DONE: Gemma]
- [x] **Task 4.3**: `SEPxx.HSB` sequencer and VAB soundfont playback. `[DONE: Antigravity]`
  - Upgraded synthesis to native 44.1 kHz stereo PCM with high-precision linear interpolation.
  - Implemented authentic SPU ADPCM sample looping (`block[1] & 0x04` loop start, `block[1] & 0x01` loop end).
  - Implemented multi-track concurrent playback for SEP sequence packages (up to 3 tracks simultaneously).
  - Added 450ms smooth exponential ADSR release envelope and CC 11 Expression volume dynamics.
- [x] **Task 4.4**: XA audio stream decoding (`VOICE/*.XAS`). [DONE: Gemma]

---

### [Phase 5: Controller, Input & Haptics]
- [x] **Task 5.1**: Gamepad input mapping with original PS1 DualShock analog sensitivity curves. [DONE: Gemma]
- [x] **Task 5.2**: DualShock vibration motor emulation to XInput triggers/rumble. [DONE: Gemma]

---

### [Phase 6: Gameplay Logic Integration]
- [x] **Task 6.1**: Port `src/main/` engine and main game loop (Memory Card Shim & Save Subsystem Infrastructure). `[DONE: Gemma]`
- [x] **Task 6.2**: Link and load stage overlays (`src/stage1/` through `src/stage7/`). `[DONE: Gemma]`
- [x] **Task 6.3**: Stage & Room Manager: dynamic camera switches, authentic GTE right-handed camera projection, incoming door camera targeting, and full-game room wiring across STAGE1-8. `[DONE: Antigravity / Gemma]`
- [x] **Task 6.4**: Room Collision (SCA boundaries): Circle-box and circle-circle authentic PS1 sliding collision across all 109 playable rooms and 1,811 boundaries. `[DONE: Antigravity]`
- [x] **Task 6.5**: Combat, Enemy AI, Health System, and Item Inventory:
  - [x] **Task 6.5a (Milestone 1)**: Authentic Combat, Weapons & Aiming Subsystem: `[DONE: Antigravity]`
    - Attached 3D handgun model (`WS212.TMD`, 138 render vertices) to Chris Redfield's Right Hand (Joint 11).
    - Loaded authentic weapon animation table from `W02.EMW` (all 203 frames / 15 EDD clips): Relaxed Idle, Walk/Run Locomotion with weapon, Gun Raise, Straight/Up/Down Aim Stances and Transitions, 3-Angle Muzzle Recoil Kicks, and Magazine Reload cycle.
    - Decoded authentic PS1 SPU weapon audio directly from `WP002.HED` and `WP002.VB` (Gunshot: 23,408 samples, Dry-Fire Click: 5,880 samples, Reload: 9,212 samples).
    - Integrated dual-channel controls (XInput R1 / Keyboard `F` / Right-Click Aim, Enter / Left-Click / Pad Cross Shoot, `R` Reload) with authentic DualShock haptic rumble on gunshot kick.
  - [x] **Task 6.5b (Milestone 2)**: Enemy AI, Spawning, Hit Detection & Damage Reactions (Zombies, Cerberus): `[DONE: Antigravity]`
    - Loaded 3D enemy zombie model (`EM1000.EMD`): 15 joint meshes (2,022 render vertices), 256x256 8-bit paletted TIM texture, and 225 EMR animation frames with forward kinematics.
    - Implemented full zombie finite state machine: Idle Groan sway (Clip 2: frames 0..18), Shamble walk tracking player (Clip 0: frames 19..39), Arms forward lunge (Clip 3: frames 40..60), Neck bite grapple (Clip 4: frames 61..75), Bullet impact stagger/knockback (Clip 5: frames 76..102), Death collapse (Clip 7: frames 103..127), and static floor corpse.
    - Implemented ballistics / hitscan raycast from Chris's handgun aim vector to zombie bounding cylinder with damage application (40 HP / 4 handgun shots to kill), knockback recoil, and death collapse.
    - Decoded authentic PS1 SPU zombie audio directly from `SOUND/CHAR00.HED` and `CHAR00.VB`: Vocal Groan (Tone 2), Bite (Tone 5), Bullet Hit squelch (Tone 7), and Death howl (Tone 4).
    - Integrated zombie drop shadow quad and real-time state display in window title bar (`[Zombie: 40 HP | Shamble]` / `[Zombie: Defeated]`).
  - [x] **Task 6.5c (Milestone 3)**: Inventory, Item Interaction & Status Screen UI (TIM 2D menus): `[DONE: Antigravity]`
    - Decoded authentic PS1 8-bit TIM status screen (`DATA/STATUS.TIM`, 256x256) and multi-CLUT palettes (Palette 0 UI border, Palette 1 poison/danger, Palette 2 item icon 256-color palette).
    - Extracted all 76 uncompressed item icons (40x30 8-bit pixels each = 91,200 bytes) from `DATA/ITEM_ALL.PIX` and built a unified 512x512 GPU texture atlas.
    - Decoded all 77 authentic item names directly from `SLUS_007.47` string table (`0x8009503C`).
    - Implemented authentic 6-slot inventory screen with yellow cursor navigation, EKG heartbeat monitor, and action submenus (Equip, Use, Check, Combine).
    - Added herb and ammo combining rules (Handgun Clip + Beretta, Green + Red Herb, Green + Green Herb) with sound cues and inventory reordering.
  - [x] **Task 6.5d (Milestone 4)**: Room Script Bytecode (SCD / Script VM) Execution & Event Triggers: `[DONE: Antigravity]`
    - Decoded authentic PS1 SCD bytecode engine matching `dualshock/src/main/scd.c` and `SLUS_007.47` symbol tables.
    - Implemented all 10 flag banks (Scenario, Scenario2, Locks, Enemies, g_SysFlags, g_GameStateFlags, g_UpdateFlags, RoomItems, Room, ItemUse) with MSB-first bit addressing (`0x80000000 >> bit`).
    - Implemented 83-entry table-driven opcode parser decoding item spawns (`0x18 item_model_set`), enemy spawns (`0x1B enemy_set`), door triggers (`0x0C door_set`), and room actions (`0x0D room_action_set`).
    - Verified live execution across RDT scripts (Stage 1 Room 00 parsed 3 item spawns in the Mansion Dining Room).
  - [x] **Task 6.5e (Milestone 5)**: Interactive World Item Pickups & Inventory Integration: `[DONE: Antigravity]`
    - Integrated real-time proximity detection for SCD spawned world items (`PickupItemAtPosition`) with 600-unit interaction sphere.
    - Added item collection on action key (`E`, Space, Enter, Gamepad Cross): transfers item to active 6-slot inventory, marks permanent collection bit in SCD flag bank 7 (RoomItems), and updates HUD.
    - Decoded authentic PS1 UI audio directly from `SOUND/SELECT.HED` and `SELECT.VB` (Menu Cursor blip: 756 samples, Confirm chime / Item pickup: 3,052 samples).
  - [x] **Task 6.5f (Milestone 6)**: Door Transition Sequences, Key Lock Verification & Authentic SPU Creak Audio: `[DONE: Antigravity]`
    - Implemented door lock verification: queries inventory with `g_Inventory.HasItem(keyId)` (Sword Key, Armor Key, Shield Key, Helmet Key); plays authentic dry-fire locked rattle if key is missing.
    - Decoded authentic PS1 wooden door sound effects directly from `SOUND/DOORWOD.HED` and `DOORWOD.VB` (Door Creak Open: 22,764 samples, Door Slam Shut: 14,784 samples).
    - Seamless room transitions with camera perspective handover and automatic execution of destination room's SCD script bytecode.
  - [x] **Task 6.5g (Milestone 7)**: Iconic 3D Door Transition Animation Sequences (ITEM_M1/DOORxx.DOR): `[DONE: Antigravity]`
    - Loaded 3D door transition assets from `ITEM_M1/DOORxx.DOR` (`DOOR00.DOR` through `DOOR15.DOR`).
    - Decoded 3D TMD door models (12 objects: door frame and swinging door leaf/handle/latch) and 8-bit paletted TIM textures (`0xafd4`).
    - Implemented authentic cinematic transition timeline: unlatching handle down (0.0s to 0.35s), smooth door swing ease-in-out open (0.35s to 2.05s) with camera dolly glide forward into darkness, alpha fade to black (2.05s to 2.45s), and room handover.
    - Synchronized with authentic PS1 door audio (`PlayDoorOpen()`, `PlayDoorClose()`) and interactive preview hotkey (`U`).
  - [x] **Task 6.5h (Milestone 8)**: In-Game Text Box, Examine System, Subtitles & Yes/No Prompts: `[DONE: Antigravity]`
    - Decoded authentic 4-bit indexed `DATA/FONT.TIM` font atlas and embedded crisp fallback bitmap glyphs with drop shadows.
    - Implemented lower-third semi-transparent dark banner (320x240 coordinates) with authentic silver border lines and blinking prompt cursor.
    - Implemented typewriter text reveal cadence (~120 chars/sec) with instant button skip and multi-page advancing.
    - Implemented interactive Yes/No selection prompts with red cursor navigation (`>` indicator) and audio feedback (`PlayMenuCursor()`).
    - Pre-cached all 64 global messages from `SLUS_007.47` string table (`0x800944BC`) and decoded room-specific MSG sections from RDT header offset `0x74`.
    - Integrated with room scenery examination (fireplace, grandfather clock, dining table) and interactive preview hotkeys (`T` for text, `Y` for prompt).

  - [x] **Task 6.5i (Milestone 9)**: Room Collision Multi-Floor Isolation, 16-Bit Coordinate Overflow Resolution, Authentic Opcode Stepper & Main Hall Staircase: `[DONE: Antigravity]`
    - Diagnosed and eliminated periodic "shhhh" noise: fixed VAG size table offset in VAB parser (`vbOff - 512`), isolated zombie voice groans strictly to rooms with active enemies, and eliminated `CHAR00.VB` groan loading.
    - Resolved staircase blockage and hallway clipping: evaluated `floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;` in `ResolveCollision` so 2nd-floor balcony railings no longer act as invisible barriers on 1st-floor staircases and corridors.
    - Fixed 16-bit coordinate overflow: upgraded `RoomDoor` and `RoomCamSwitch` coordinate fields from `int16_t` to `uint16_t`, fixing 990 doors and 308 camera switch quads that wrapped negative.
    - Implemented authentic opcode-aware door scanner in `RoomManager::ParseRdt`: eliminated all false ghost doors that blocked hallways.
    - Implemented authentic Main Hall staircase interactivity: seamlessly transitions between 1F (`Stage 1 Room 0x01`) and 2F Balcony (`Stage 2 Room 0x03`).
  - [x] **Task 6.5j (Milestone 10)**: PSX Audio Sequencer Synthesis Overhaul & 57-Track Complete Music Extraction: `[DONE: Antigravity]`
    - Diagnosed audio rendering discrepancies matching OST reference ("Meeting Rebecca" / `SEP0B.HSB`): multi-track dropping, missing sample looping, nearest-neighbor aliasing, and 25ms choke-off clicks.
    - Implemented authentic sample looping, linear interpolation, 450ms natural ADSR release curves, CC 11 Expression support, and multi-track sequence mixing in [`psx_seq.cpp`](port/src/audio/psx_seq.cpp).
    - Built and executed standalone native music extraction tool [`extract_music.exe`](port/src/extract_music.cpp), batch-rendering all 57 authentic DualShock music tracks (`SEP00.HSB` .. `SEP38.HSB`) to 44.1 kHz 16-bit stereo `.WAV` files in `extracted/music_wav/`.
  - [x] **Task 6.5k (Milestone 11)**: Item Box (Magic Storage Chest) & Typewriter Save/Load System: [DONE: Antigravity]
    - Created binary save state serialization engine in [save_manager.h](port/include/save_manager.h) and [save_manager.cpp](port/src/game/save_manager.cpp) (RE1DSAV format, version 1, serializing player XYZ, facing angle, 140 HP, equipped weapon, 6 player inventory slots, 32 storage chest slots, and 10 SCD VM flag banks).
    - Implemented dual-column Item Box screen in [item_box.h](port/include/item_box.h) and [item_box.cpp](port/src/game/item_box.cpp) with 6-slot player inventory, 32-slot scrollable chest, item icons, quantities, deep royal blue beveled panels, item description display, and interactive [STORE] / [TAKE] command submenus.
    - Decoded authentic Save Room UI audio from SELECT.HED and SELECT.VB: Tone 0 / VAG 1 (Item Box Open/Close, 13,104 samples), Tone 1 / VAG 2 (Typewriter Keystrokes & Bell, 305,928 samples), and Tone 4 / VAG 5 (Menu Cancel/Buzz, 840 samples).
    - Wired authentic Save Room Typewriter interaction in [main.cpp](port/src/main.cpp): checks for Ink Ribbon (Item 47), displays authentic prompt, decrements ribbon on Yes, plays typewriter SFX, and writes saves/save_0.dat.
    - Implemented Quick Save (F5) and Quick Load (F6): instantly saves and restores player position, health, inventory, item box, room, camera cut, background slice, and room BGM.
    - Verified 100% clean Release build (
e1_dualshock_port.exe) and visual verification via itembox_captured.png running at 60 FPS on GPU.

  - [x] **Task 6.5l (Milestone 12)**: Authentic PS1 FMV / Movie Player (`.STR` Streaming & MDEC V2): `[DONE: Antigravity]`
    - Created `PSXMoviePlayer` subsystem in [`psx_str.h`](port/include/psx_str.h) and [`psx_str.cpp`](port/src/hal/psx_str.cpp) parsing 2,336-byte CD-ROM XA Mode 2 Form 2 sectors across all 24 authentic game movies (`extracted/RE1-Dualshock/PSXUSA/ZMOVIE/*.STR`).
    - Upgraded MDEC Decoder in [`psx_mdec.cpp`](port/src/hal/psx_mdec.cpp) to support Version 2 bitstreams (`0x0002` magic) with 10-bit signed direct DC decoding, MPEG-1 run-length AC codebooks, and column-major macroblock iteration (20 cols x 15 rows).
    - Decoded 37.8 kHz stereo 4-bit CD-ROM XA ADPCM audio via dual-history filter coefficients ($K_0, K_1$) streaming directly into dedicated XAudio2 voice queues.
    - Implemented dynamic D3D12 Texture2D upload buffer, fullscreen quad rasterization with PS1 pipeline shaders, skip hotkeys (Space, Enter, Escape, Gamepad Cross/Start), hotkey cycling (`P`), and CLI flag `--play-movie <id>`. Verified with `capcom_5s.png`.

  - [x] **Task 6.5m (Milestone 13)**: Full Game Progression & Puzzle Items (Crests, Emblems, Sheet Music, V-JOLT, MO Disks): `[DONE: Antigravity]`
    - Implemented `PuzzleManager` in [`puzzle_manager.h`](port/include/puzzle_manager.h) and [`puzzle_manager.cpp`](port/src/game/puzzle_manager.cpp) tracking progression states and SCD scenario/lock bits.
    - 4 Crests (Wind 35, Star 36, Sun 37, Moon 38) placed into Rear Garden door stone relief (Stage 1 Room 0x0E) with incremental countdown and crypt unlock (`crests_puzzle_confirmed.png`).
    - Musical Score (46) performed on Grand Piano (Stage 1 Room 0x05) to trigger Moonlight Sonata, open secret sliding panel, and award Gold Emblem (45) (`piano_puzzle_confirmed.png`).
    - Wooden Emblem (44) swapped into Dining Room fireplace mantle relief (Stage 1 Room 0x00/0x05) to disarm clock trap and unlock doors.
    - V-JOLT (76 / 39..43) applied to Plant 42 giant root system in Guardhouse B2 (Stage 3 Room 0x0F) to wither tendrils.
    - 3 MO Disks (50) authenticated at laboratory passcode terminals (Stage 5) to unlock partner holding cell.
    - Wired inventory `[USE]` action to `PuzzleManager::TryUseItem`, consuming used items, granting awards, playing confirm/cancel audio, and displaying feedback in authentic PS1 text boxes.
    - Added test CLI flags `--test-puzzle`, `--test-inventory`, `--test-use-piano`, and `--test-use-crests` for automated verification.

  - [x] **Task 6.5n (Milestone 14)**: Authentic PS1 XA Voice Acting (`VOICE/*.XAS`) & SCD Dialogue Triggering: `[DONE: Antigravity]`
    - Replace placeholder IMA ADPCM logic in [`xas_decoder.cpp`](port/src/audio/xas_decoder.cpp) with authentic 2,336-byte CD-ROM XA Mode 2 Form 2 sector parsing (16-channel interleaved 37.8 kHz mono 4-bit Sony XA ADPCM with filter coefficients $K_0, K_1$).
    - Extract `g_StageVoiceFileIndex` and `stage_voice_offset_tbl` across all 7 stage overlays (`STAGE1.EXE` .. `STAGE7.EXE`) to map voice clip IDs to exact `(VOICEn.XAS, channel, startSector, endSector)` ranges matching `voice_play_clip` in `cd.c`.
    - Integrate XAudio2 37.8 kHz mono voice streaming channel into [`AudioEngine`](port/include/audio_engine.h) and wire SCD opcode `0x1E` (`scd_op_1e_voice_play`) and interactive dialogue triggers.
