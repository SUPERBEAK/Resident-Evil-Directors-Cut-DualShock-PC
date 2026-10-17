# Game notes

How "Resident Evil Director's Cut Dual Shock Ver." (USA) works inside, as learned while decompiling it.
This page covers the programs and overlays, the startup and main loop, memory use, stages and rooms,
entities, the player, enemies, NPCs, items, menus, movies, sound, game modes and saves, CD loading,
the PS1-only features and the known bugs in the original code.

Names in this document (functions, globals, structs, flags) refer to the decompiled source in
`dualshock/`: sources in `dualshock/src`, headers in `dualshock/include`, symbol and struct configs in
`dualshock/config`. Where a name comes from the RE1 PC decompilation, that is said ("PC name"). Facts
marked "not confirmed" are open questions. "Confirmed in game" means the behaviour was checked by
playing the game.

Related documents (all in `docs/`):

- [decomp_guide.md](decomp_guide.md): how the decompilation project is set up and built.
- [matching_guide.md](matching_guide.md): how to get byte-matching C (includes the build targets and
  their gp values).
- [code_notes.md](code_notes.md): notes on the code and compiler patterns.
- [re1_ram_map.md](re1_ram_map.md) / [re1_ram_map_detailed.md](re1_ram_map_detailed.md): the PS1 RAM map.
- [disc_contents.md](disc_contents.md): what is on the disc.
- [psyq_sdk.md](psyq_sdk.md): the Sony PsyQ libraries the game was built with.

## Contents

1. [Programs and overlays](#programs-and-overlays)
2. [Startup, main loop and tasks](#startup-main-loop-and-tasks)
3. [Memory](#memory)
4. [Stages and rooms](#stages-and-rooms)
5. [Entities](#entities)
6. [The player](#the-player)
7. [Enemies](#enemies)
8. [NPCs](#npcs)
9. [Items and inventory](#items-and-inventory)
10. [Menus](#menus)
11. [Movies](#movies)
12. [Sound](#sound)
13. [Game modes, characters, saves and game cleared](#game-modes-characters-saves-and-game-cleared)
14. [CD loading](#cd-loading)
15. [PS1-only features](#ps1-only-features)
16. [Known bugs in the original code](#known-bugs-in-the-original-code)

---

## Programs and overlays

### The 13 executables

The game consists of a main program and 12 overlays (in `PSXUSA/PROG2/` on the disc):

| Executable | Load address (VRAM) | Capcom functions |
|---|---|---|
| SLUS_007.47 (main program) | 0x80010000 | 962 (327,076 bytes of code) |
| LOGO | 0x800E0000 | 1 |
| PROLOGUE | 0x800E0000 | 1 |
| TITLE | 0x800E0000 | 7 |
| SELECT | 0x800E0000 | 5 |
| ENDING | 0x800E0000 | 20 |
| STAGE1 | 0x80105400 | 208 |
| STAGE2 | 0x80105400 | 114 |
| STAGE3 | 0x80105400 | 245 |
| STAGE4 | 0x80105400 | 249 |
| STAGE5 | 0x80105400 | 203 |
| STAGE6 | 0x80105400 | 228 |
| STAGE7 | 0x80105400 | 187 |

- The main program is compiled at `-O2`; the overlays at `-O0`.
- The menu/screen overlays (LOGO, PROLOGUE, TITLE, SELECT, ENDING) share the load address 0x800E0000,
  which lies over the player/weapon area (no player is on screen while they run). Only STAGE1-7 load at
  0x80105400, the start of the dynamic load area (`g_OverlayArea` / the `g_LoadPointer` heap).
- Each executable has its own gp value (see the build targets table in
  [matching_guide.md](matching_guide.md)), even though the EXE headers say gp = 0. Capcom code itself
  never uses `$gp`.
- The main program is always resident (0x80010000-0x800B2800): text at 0x80010000, size 0xA2800,
  entry point 0x80061888. Build stamp: `CAPCOM-Jul 27 1998-09:43:23-CAPCOM`. It contains only about 387
  strings, and almost all of them come from the Sony libraries.
- The Sony library `$Id` tags are dated Dec 1996 (sys.c 1.129, bios.c 1.81, intr.c 1.74), but signature
  matching shows the libraries are PsyQ 3.7 (LIBSND/LIBSPU 3.3, LIBPAD 4.2): see [psyq_sdk.md](psyq_sdk.md).
- Overlay sizes: LOGO, PROLOGUE, TITLE, SELECT and ENDING are 2-24 KB; STAGE1-STAGE7 are 116-188 KB.
- Capcom's main program was built from several source files: some of its switch jump tables are
  8-aligned and others only 4-aligned (0x800106DC..0x80010804 are packed 4-aligned).

### Main program layout

| Range | Content |
|---|---|
| 0x80010000-0x8001098C | Capcom rodata (jump tables, debug strings such as `"%d Atari!\n"`) |
| 0x8001098C-0x80011ADC | Sony (PsyQ) rodata, starting with printf's `"0123456789ABCDEF"` |
| 0x80011ADC-0x80061880 | Capcom code; the first function is `func_80011ADC` |
| 0x80061880- | Sony library code |
| 0x8008F93C- | data (the Sony library code ends at 0x8008F89C; splat's `data` segment starts at 0x8008F93C, its own estimate) |
| 0x800B2360-0x800BEC10 | BSS (about 51 KB), cleared at startup |
| above 0x800BEC10 | heap, plus Capcom's hand-placed work areas |

Rodata strings include `D_80010130` `"bu00:"` (the memory card path, copied into a 20-byte local buffer
and zero-padded) and `D_80010304` `"_Nothing_"` (`s_Nothing`).

### Overlay loading

- Each overlay starts with a standard PS-X EXE header. The overlay "header" copied into
  `TaskSlot`+0x14 is that header's EXEC struct (`pc0` = `ovl_entry`, `gp0`, `t_addr` = `ovl_load_addr`,
  `t_size`).
- Each overlay contains Sony's `NONE2.OBJ` startup stub (at its entry point - 8). Apart from that stub
  the overlays are pure Capcom code.
- Every overlay entry first calls an empty `jr ra` stub: that overlay's copy of GCC's `__main` hook
  (`title__main`, `stage1__main`, ...).
- How the stage overlay for a room is chosen is described in
  [How the active stage is chosen](#how-the-active-stage-is-chosen).

### Shared code between the stage overlays

- Much of the STAGE code is duplicated between stages: across STAGE1-7 there are only about 577
  distinct function shapes. Many functions exist in near-identical copies, for example the
  `zombie_head_bounce_physics` family (stage1 0x8010F7FC), present in stages 1, 2, 4, 5, 6 and 7.
- Every stage has a 32-byte table at the same address, 0x80105400 (the first bytes of the overlay;
  in stage1 it is `zombie_hit_threshold_hard_tbl`). The `zombie_damaged` (stage1 0x80106F44) and
  `zombie_init` (stage1 0x8010574C) families copy it into a local.
- `yawn_anim_advance` (stage2 0x8011F47C) and `plant42_joint_move` (0x80126B70) return `u8`.

### The overlay engine API table

Stage overlays do not call main-program engine functions directly. They call them through function
pointers in the overlay engine API table at **0x801FE900** (`g_OverlayApiTable`, `void *[0xB7]`, 183
entries, up to 0x801FEBDC). `task_scheduler_init` (0x80037014) copies it from `g_OverlayApiTableSrc`
(0x800945BC). Entry 1 (0x801FE904) is `api_task_sleep`; examples further up are
`api_billboard_reserve` (0x801FE9D8), `api_rand` (0x801FEA18) and `api_entity_add_speed_xz`
(0x801FEA20).

A typical call stores the "current function" pointer in `g_ApiCallPtr` (0x801FEC18) and then calls it:

```c
(g_ApiCallPtr = api_rand)();
(g_ApiCallPtr = api_entity_add_speed_xz)(M2C_PFIELD(p, s16 *, 0x170));
```

`dualshock/tools/api_table.py` decodes the table (run from `dualshock/`; `--md out.md` writes it as a
Markdown table).

### The select overlay

The two big functions of the select overlay use:

- `SelPlayer g_SelectPanels[2]` (0x800E4EE0), stride 0x240 (one select screen panel): a 16-byte header
  (`x0..xD`), `POLY_FT4 a[2][2]` and `POLY_FT4 b[2][5]`. The first index of `a` / `b` is the
  double-buffer index `g_ActiveBuffer` (0x800C3010).
- UV tables of `{u8 x, y, w, h}`: `select_chris_uv_tbl` (0x800E4E44), `select_jill_uv_tbl`
  (0x800E4E4C), `select_backdrop_uv_tbl` (0x800E4E54).
- `SVECTOR g_SelectQuadVerts[4]` (0x800E4EB8).
- The `GsOT` pair at 0x800C88A0 (`g_OrderingTables`).
- The PsyQ macros `setUV4()` and `setRGB0()`.

### Sony library

- `include/psyq_protos.h` holds the prototypes of the 187 Sony (PsyQ) functions the game uses. The
  layouts in `psyq_types.h` follow the real library: GsCOORDINATE2 has `param` (0x50 bytes), GsDOBJ5
  has `packet` (0x14).
- Verified PsyQ struct sizes: MATRIX 0x20, DRAWENV 0x5C, GsCOORDINATE2 0x4C, GsSPRITE 0x24, GsOT 0x14.
- `0x8008C37C` is **SpuWrite** (near-identical to SpuRead, but it calls `_spu_write`; `SsVabTransBody`
  uploads sound banks with it).
- `0x8006D584` is **InitGeom** (sets ZSF3/ZSF4). `init_geom_task` (0x80032F7C) calls `InitGeom(0)`.
- SDK versions and the full list of library functions: [psyq_sdk.md](psyq_sdk.md).

---

## Startup, main loop and tasks

### Program start

Sony's startup code (`2MBYTE.OBJ`) runs in three parts:

- `stup2` clears the BSS, 0x800B2360-0x800BEC10 (about 51 KB).
- `stup1` sets up the stack, sets `$gp`, and calls the heap init (`func_80061944`) with everything
  above 0x800BEC10.
- `stup0` does `jal 0x80031F0C`, so **`main` = 0x80031F0C**. main's first call is `__main`, which GCC
  inserts automatically into a function named `main`; that confirms the name.

### `main` and the main loop

`main` (0xA38 bytes) sets up two 320x240 draw environments (`SetDefDrawEnv`, `SetDrawMode`, with
`GetGraphType` checks), then runs the main loop at 0x80032030:

1. `pad_update()` (0x8003BAF8)
2. `update_demo_timer()` (0x80032DF0)
3. a branch on the menu byte of `g_GameStateFlags` (0x800C3000): `& 0xFF00 == 0x8000`
   (`GAME_MENU_BYTE` == `GAME_MENU_OPEN`, the menu task is running).

The top 2 bits of `g_GameStateFlags` (`GAME_SCREEN_MODE`, 0xC0000000) are the main loop's screen mode
(`GAME_SCREEN_*`), not a fade. The main loop also plays requested movies (`GAME_FMV_REQUEST`) and
resets the graphics (`GAME_GRAPHICS_RESET`); see [Status and game-state flags](#status-and-game-state-flags).

The main loop is the entry point for exploring the engine's call tree.

### Tasks

- The game runs a small thread system: `TaskSlot g_Tasks[3]`, 0x80 bytes per slot (`TaskSlot` has
  `tcb` and `ovl_gp0` fields, and the overlay EXEC struct at +0x14).
- `task_sleep` (0x80037508) saves `$ra` into a global with inline asm. Wait loops call it as
  `task_sleep(1)`, for example `bgm_fade_out_all` (0x80054FB8). Overlays reach it through
  `api_task_sleep`.
- `task.c` reads the PS1 BIOS table of tables at 0x100 (`BiosToT`) and the thread control block
  (`BiosTcb`: +0x94 = saved SR).
- The tasks and their stacks take about 6 KB of RAM (see [re1_ram_map.md](re1_ram_map.md)).

---

## Memory

The overall RAM layout (program, game state, player/weapon area, sound, stage overlay, room heap,
shared screen area, tasks) is in [re1_ram_map.md](re1_ram_map.md); every buffer, load area and task
slot field is in [re1_ram_map_detailed.md](re1_ram_map_detailed.md). In the decompiled code each of
these addresses has a name in `dualshock/config/addrs.txt`. Many areas are shared in turns: the same
memory holds a door animation, then the room background, then a map.

### Fixed work areas

Capcom's big global work areas sit at fixed addresses above the BSS, in RAM the program lays out by
hand (0x800C3000, 0x800C51BC, 0x800C88A0, 0x800CAB0A, 0x800CF86E, and others). Because they are not
declared variables, the code reaches them through address macros (e.g.
`#define WORK ((Work *)0x800C3000)`). The project names them with `LIT(...)` / `LITP(...)` in
`include/re1_addrs.h`.

| Address | Name | Contents |
|---|---|---|
| 0x800C3000 | `g_GameStateFlags` | game-state flag word (start of the work area) |
| 0x800C3004 | `g_StatusFlags` | status flag word (game mode bits and more) |
| 0x800C3010 | `g_ActiveBuffer` | the double-buffer index |
| 0x800C3024 | `g_EffectPool` | `Effect` slots, 0x84 bytes each (`EFFECT_SIZE`) |
| 0x800C5124 | `g_PlayerEntity` | the player entity |
| 0x800C52A4 | `g_EnemiesList` | `Entity[30]`, 0x18C bytes each (`ENTITY_SIZE`); `g_EnemySlotBase1` is the 1-based base; followed by `g_EnemySnapshots` (16 x 0x1C) |
| 0x800C8460 | `g_SaveData` | the save block (PC `g_BioCard`, 0x41C bytes) |
| 0x800C88A0 | `g_OrderingTables` | a pair of `GsOT` (one per buffer), used as `((GsOT *)0x800C88A0)[buf].org + (xC + 4)` |
| 0x800C88C8 / 0x800C88F0 | `g_OtMain` / `g_OtBg` | the ordering tables (`GsOT [2]` each), with their tag stores after them |
| 0x800CAA18 | `g_PlayerAnimFunctions` | the player animation hook table (52 slots) |
| 0x800CAB0A | `g_FadeStep` | |
| 0x800CAB10 | `g_RoomData` | pointer to the loaded RDT header |
| 0x800CAB14 | `g_CurrentEntity` | pointer to the entity being updated |
| 0x800CF86E | `g_ActiveEffectIndex` | the current `Effect` slot (`EFFECT`) |
| 0x800BC5A8 | `g_MenuWork` | `MenuWork`, 0x175 bytes |
| 0x801D10EC | `g_DoorFile` | a 0x25814-byte (150 KB) shared screen work area |
| 0x80096E54 | `g_CdFileTable` | `CdFileEntry g_CdFileTable[]`, the CD file numbers |
| 0x80094898 | `g_MovieTable` | the FMV table (`MovieEntry`) |
| 0x80096338 | `g_ClFrameQuads` | the computer-lab frame quads |

Notes:

- The effect pool is also reached from the 0x800C3000 base: the main program indexes 0x84-byte
  records there through a literal address, e.g.
  `((Rec84 *)0x800C3000)[*(u8 *)0x800CF86E].x32 |= ...` (`effect_behavior_rand_anim`, 0x8001F0AC).
  Capcom re-reads the index byte on each access.
- Objects inside the player's joint array (`g_PlayerJoints`, 0x800C51BC) at +0x744 and +0x6C8 are used
  by `clear_anim_timing` (0x80031A58), `player_clip_eject_start` (0x80031EDC),
  `player_update_detached_joint` (0x80031A74) and `player_clip_state_0_spawn` (0x80031BFC). +0x744 is
  the ejected magazine clip (`ClipJoint`, see [Joint](#joint)).
- `g_RoomData` points to a struct that holds an array of 0x2C-byte records at offset 0x9C (each record:
  a `MATRIX` + an `s32` + 8 pad bytes).
- **`g_DoorFile`** is not just the door file: the area is shared by the room background decode,
  full-screen pictures, the door animation, the item viewer model, file pages, maps, and the menu
  textures at +0x10000.
- **The save block** (`g_SaveData`) holds the SCD flag banks (see [Flag banks](#flag-banks)),
  `g_ItemExaminedFlags`, `g_CountdownTimer` (the lab self-destruct countdown), `g_InventorySlots`,
  `g_RoomBgmState`, and `g_StageId` / `g_RoomId` (copied from 0x800C8660).
- Other named blocks: the OT tag stores, `g_ScdEventTable`, `g_ScdBranchStack`,
  `DoorCmd g_DoorCmdTable[8]`, `s16 g_SndPanVol[3][2]`, `g_MapTabSprites`
  (= `g_MapBrowserSprites[5..8]`).
- `g_DeadMoveValue` (0x801FEC10) is `&g_IdentityMatrix`; its translation (the zero vector) is used to
  clear VECTORs. The scratchpad MATRIX at 0x1F8003E0 is filled from `*g_DeadMoveValue`
  (`zombie_slow_walk`, stage1 0x80108F04) and copied out again elsewhere
  (`M2C_FIELD(base, MATRIX *, off) = *(MATRIX *)0x1F8003E0;`).

### Small tables

- `g_WaveImage` is a 16-byte sprite template (attr, x, y, w, h, u, v, clut_y; +8 w, +0xA h, +0xD v);
  `image_draw_wave` sets w = 1 (1-pixel columns) and h = 0x40.
- A `g_FireVibrationTbl` record is `{dur, ?, rampFrom, rampTo, rampFrames}`.
- Other named tables: `g_KnifeHitEffectOffsets`, `g_EffectTintLightFactors`, `g_WalkSpeedDips`,
  `g_KnifeHitFrameWindows`, `g_DeathFallSpeeds`, `g_ViewEdgeVector`, `g_SideTestVector`,
  `zombie_hit_threshold_hard_tbl`, `zombie_body_probe_neg` / `_pos`.

### The scratchpad

The 1 KB scratchpad at 0x1F800000 is used as a struct of work variables:

| Offset | Name | Use |
|---|---|---|
| 0x00-0x1C | `sp_tmp00`..`sp_tmp1C` (+0x20 / 0x2C / 0x38) | per-function temporaries; their meaning changes per function (enemy AI: `tmp00` = distance to the player, `tmp1C` colour/flag; `tmp00` is also the GsSortObject work base; 0x1C is also used as a loop counter) |
| 0x40 | `sp_text` | sprintf buffer (the memory card list builds each line in it: `sp_text_1..B`) |
| 0x100-0x122 | `sp_path*` | enemy path search |
| 0x180-0x1D0 | | item-viewer light matrices |
| 0x368 | `sp_rect` | RECT (LoadImage / MoveImage) |
| 0x370 | `sp_box` | GsBOXF (filled box) |
| 0x380 | `sp_sprite` | GsSPRITE for every 2D sprite (fields `sp_sprite_x`..`rotate`; PC `TextureDesc` layout) |
| 0x3A4 | `sp_gline` | the status screen ECG line (GsGLINE) |
| 0x3B8 | `sp_line` | GsLINE |
| 0x3C8 | `sp_svec` | SVECTOR |
| 0x3D0 | `sp_vec` | temporary VECTOR, the most used slot (`ScratchVec` view `SCRATCH_VEC`); 0x3D4 / 0x3D8 are its y / z, and 0x1F8003D0 is also passed to functions as an address |
| 0x3E0 | `sp_matrix` | MATRIX (t = `sp_matrix_t0..t2`) |

### Flag banks

The room scripts' flag banks live in the save block:

| Address | Name | Contents |
|---|---|---|
| 0x800C8700 | `g_ScenarioFlags` (bank 0) | scenario/story flags: lockpick 0x7C, radio 0x7F (`g_MenuHasRadio`), 0x7D menu fade latch, 0x7E infinite rocket launcher, 0x5B monster plant, 0x29 Plant 42 dead. Flag 0 selects the revisited mansion. Not "options", as the PC docs say. |
| | `g_ScenarioFlags2` (bank 1) | |
| 0x800C86B4 | `g_LocksFlags` (bank 2) | door/desk lock bits |
| 0x800C86BC | `g_EnemiesFlags` (bank 3) | enemy death flags (an EM_SET of a killed enemy is skipped) |
| 0x800C8710 | `g_RoomFlags` (bank 8) | visited rooms, maps 0x7C+, files 0x82+ |

### Status and game-state flags

Every bit of `g_StatusFlags` (0x800C3004) and `g_GameStateFlags` (0x800C3000) the code uses has a name
in `include/re1_flags.h`. The bits mentioned in this document:

| Flag | Value | Meaning |
|---|---|---|
| `STATUS_IN_WATER` | 0x1 | the player stands in a water zone (`room_action_water_zone`) |
| `STATUS_SHADOW_DEPTH_SORT` | 0x8 | shadows are depth-sorted into the main OT, so they hide behind objects such as the dining-room table |
| `STATUS_BG_LOADING` | 0x10 | a camera background is being read from the CD; rumble pauses meanwhile |
| `STATUS_ADVANCED_HOLD` | 0x10000 | "second variant": Advanced started by holding Right (save tag "AT") |
| `STATUS_ADVANCED` | 0x20000 | Advanced mode (save tag "Ad") |
| `STATUS_TRAINING` | 0x40000 | Training mode (save tag "Tr") |
| `STATUS_SCRIPT_BIT19` | 0x80000 | no code reads or writes it |
| `STATUS_BLOCKING_LOAD` | 0x100000 | `load_file` waits with VSync instead of `task_sleep` |
| `STATUS_SFX_BANK1_SMALL` | 0x200000 | sound bank 1 holds 16 sounds instead of 48 |
| `STATUS_STAIRS_ZONE` | 0x400000 | set by the staircase zones |
| `STATUS_DOOR_WAIT_BGM` | 0x800000 | the door creak waits for the next room's music |
| `STATUS_BOX_CLIMB` | 0x1000000 | set while climbing onto a box |
| `STATUS_SELF_DESTRUCT` | 0x8000000 | the self-destruct countdown is running |
| `STATUS_GAME_CLEARED` | 0x80000000 | the game-over exit goes to the ending (ending FMV, credits, results) |
| `GAME_MIRROR_ON` / `GAME_MIRROR_AXIS` | 0x1 / 0x2 | the room has a mirror; mirror plane axis |
| `GAME_CAMERA_DEFER` | 0x4 | a camera cut redraws on the next frame |
| `GAME_SCRIPT_BIT3` | 0x8 | set by room scripts only; no code reads it |
| `GAME_LADDER` | 0x10 | set by a ladder zone |
| `GAME_PUSHING` | 0x40 | the player has pushed a movable object |
| `GAME_ON_BOX` | 0x80 | the player stands on a climbable box |
| `GAME_MENU_OPEN` / `GAME_MENU_BYTE` | 0x8000 / 0xFF00 | the menu task is running / the whole menu byte (requests + open) |
| `GAME_VOICE_PLAYING` | 0x20000 | an XA voice line is playing |
| `GAME_FMV_REQUEST` | 0x40000 | the main loop plays `g_SelectedFmvId` |
| `GAME_GRAPHICS_RESET` | 0x80000 | the main loop calls `init_graphics(1)` |
| `GAME_UNUSED_21` / `GAME_UNUSED_27` | 0x200000 / 0x8000000 | never set or tested |
| `GAME_FADE_ACTIVE` | 0x20000000 | a screen fade is running |
| `GAME_SCREEN_MODE` | 0xC0000000 | the top 2 bits: the main loop's screen mode (`GAME_SCREEN_*`), not a fade |

`g_StatusFlags` also selects the damage table column (see [Game modes](#game-modes)). Collision flag bits
0x10 / 0x80 are documented in the code.

---

## Stages and rooms

### How the active stage is chosen

- `g_StageId` (0x800C8660, 0..6) selects `STAGE1.EXE`..`STAGE7.EXE`; `g_StageId` 6 means the STAGE7
  files. `g_RoomId` (0x800C8661) is the room within the stage (the room id in `ROOMsrr`). Both are in
  the save block.
- `room_transition_load` reads byte +0xD of the door record (`g_DoorRecord`, `*0x800CAAFC`): the low 5
  bits are the destination room, the high 3 bits the destination stage + 1.
  - 0 = same stage: only `room_init` runs; the overlay stays loaded.
  - Otherwise `g_StageId = (byte >> 5) - 1`, and room setup calls `stage_overlay_load`, which does
    `load_stage_overlay(2, g_StageOverlayFiles[g_StageId])` (`g_StageOverlayFiles` = 0x8009697C; file
    numbers 0x157..0x15D = `STAGE1..7.EXE`). Then the overlay's enemy update table gets the shared
    handlers from the main program: entry 0x13 = `spiderweb_update`, entries 0x17.. =
    `character_npc_update` (NPCs).
- Scenario flag 0 (bank `g_ScenarioFlags`, set only by room scripts) remaps stages 0/1 to 5/6 (+5): the
  revisited mansion (`STAGE6` / `STAGE7`, with hunters). The exact event that sets it is in the room
  SCD data, not in the code.
- Code that special-cases rooms compares stage + room as one `u16` (0x501 = stage 1 room 5).

### Stage contents

From each overlay's enemy data:

| g_StageId | File | Area | Enemies |
|---|---|---|---|
| 0 | STAGE1.EXE | Mansion 1F | zombie, cerberus, monster plant, crow |
| 1 | STAGE2.EXE | Mansion 2F | Yawn (first fight), crow, zombie |
| 2 | STAGE3.EXE | Courtyard / underground caves | black tiger, adders, cerberus, hunters, tyrant |
| 3 | STAGE4.EXE | Guardhouse | web spinner, wasps, Plant 42 (+ roots), Neptune, monster plant, zombie |
| 4 | STAGE5.EXE | Laboratory | tyrant, chimera, computer arms, zombie |
| 5 | STAGE6.EXE | Mansion 1F revisited | hunters, crow, monster plant, web spinner, zombie |
| 6 | STAGE7.EXE | Mansion 2F revisited | hunters, Yawn 2, crow, zombie |

### What a stage overlay installs

Each stage's entry (`stageN_entry`):

- sets `g_EnemyUpdateTable = &enemies_update_functions_tbl` (index = entity id; each entry is that
  enemy's `<enemy>_update`, which dispatches `state_tbl[g_CurrentEntity->state]()`);
- sets the stage's BGM / voice / background-checksum tables (the background table has an
  Advanced-mode variant);
- installs player-animation hooks into the main program's `g_PlayerAnimFunctions` table (0x800CAA18,
  PC `g_playerAnimFunctions`, 52 slots).

The player-side code for a stage's enemies lives in the overlay, for example:

| Slot | Hook | Enemy |
|---|---|---|
| 0 | `player_anim_zombie_grabbed` | zombie |
| 24 | `player_anim_multi_attack` | crow |
| 34 | `player_anim_plant_hold` | plant hold (monster plant) |
| 40 | `player_anim_simple_recovery` | cerberus |
| 44 | `player_anim_hunter_decapitation` | hunter |
| 51 | `player_anim_death_alt` | Yawn (swallow) |

Other hooks: `player_anim_poison_death`, `player_anim_chimera_knockdown`, `player_anim_crawling`,
`player_anim_limb_physics`, `player_anim_limb_bounce`, `player_anim_death_billboard`,
`player_anim_enemy_interact`, `player_anim_set_attacked_flag`.

### Room numbering and room files

- RDT file names: `ROOM<stage><room, 2 hex digits><player: 0 Chris, 1 Jill>.RDT`. For example,
  ROOM71C0/71C1 is room 0x1C of stage 7.
- In Advanced mode, the rooms listed in `g_AdvancedModeRooms` load the "arrange" RDT of stage + 7
  (STAGE8 to STAGEE) instead.
- Stages 6 and 7 (and the Advanced stages 12 and 13) use the BSS background files of stages 1 and 2
  (`load_room_bg_image`: `st -= 5`).
- A `ROOMsrr.BSS` file holds one 320x240 MDEC picture (PS1 "bs" v3 frame) per camera cut, each in its own
  0x8000 bytes (`load_room_bg_image`: lba + cut x 16 sectors).
- `GAME_ALT_RDT` (0x800000) loads the room's alternate RDT file (file index + 1).

### The RDT header

The RDT header (`RoomHeader`, view `ROOM` over `g_RoomData`):

- `ambient_r/g/b`, then 3 lights of 0x14 bytes each (light 0: `light_x/y/z`, `light_r/g/b`,
  `light_type`, `light_radius`; PC `RDT_Light`; one shared `RoomLight` type). The light block is at
  0x0C-0x47.
- `sprite_count`, `object_count`, `item_model_count`.
- 19 section pointers, starting at +0x48. +0x50 `object_models`, +0x54 `item_models`; +0x60 / +0x64 /
  +0x68 = init SCD / main SCD / event scripts; +0x74 the text messages (section 11).
- An init script is a `u16` length followed by bytecode.
- Related records: `BoundaryHeader` (= PC `RDT_BoundaryHeader`), `RoomSpriteRec.depth`, camera records
  (`RdtCamera`), room boundaries, BLK records.

### Room scripts (SCD)

- The script interpreter `scd_run_command_functions` (0x8004B020) runs handlers from a function table
  indexed by the current script byte. It keeps calling handlers until one returns 0, then pops a
  nesting level if there is one:
  `loop: while (fn[*PC]() != 0) {} if (depth) { pop; goto loop; }` (inside a `while`).
- The handlers are in `g_ScdOpcodeTable` (0x80095FE4, 83 handlers, all named). The order is the PC SCD
  opcode list 0x00-0x50 plus 2 PS1 extras. The PS1-only opcodes 0x4F-0x52 are DualShock vibration.
- `g_pScdEventCurrent` (0x800CF688) points to the running event. It sits directly after the SCD branch
  stack (`g_ScdBranchStack`), before the 8-entry event table (`g_ScdEventTable`) at 0x800CF68C. Event
  script commands are `ScdEventCmd`.

Opcodes with known details:

| Opcode | Name / record | Details |
|---|---|---|
| 0x07 | `ScdOpStateWordTest` | state word test |
| 0x0D | `room_action_set` | creates a room-action zone (18 bytes: +1 slot, +2 x, +4 z, +6 w, +8 d, +0xA type, +0xB flags, +0xC arg0, +0xE arg1, +0x10 arg2); see [Room actions](#room-actions-stairs-ladders-and-slopes) |
| 0x0F | `ScdOpMirrorSet` (`scd_op_0f_mirror_set`) | mirror setup (writes `GAME_MIRROR_*`) |
| 0x1B | EM_SET | places an enemy or NPC (see below) |
| 0x2D | | gives an item (`MENU_MODE_GOT_ITEM`) |
| 0x33 | `scd_op_33_player_prop_set` | variable length (2 or 4 bytes depending on the sub-command). Sub-command 8 writes the player's +0xDC flags: `33 08 <value> <mode: 0 set / 1 or / 2 xor>` |
| 0x3F | `ScdOpPlayerDirTest` | player direction test |
| 0x40 | | reads camera records at -8 |
| 0x46 | `ScdOpLight` | 12-byte light operands; the RDT header light block 0x0C-0x47 is restored through it (computer lab) |
| 0x4F-0x52 | | DualShock vibration (PS1 only) |

There are 28 SCD opcode record layouts in the struct configs.

**EM_SET** (opcode 0x1B, 22 bytes) places an enemy or NPC: byte [1] model, [2] behaviour, [3] kill-flag
index, +4 `no_restore`, [0x12] enemy slot; it also has `sound_set`. NPCs are always placed in enemy slot
0 (0x800C52A4). **OM_SET** (object) has `probe1_x/z`, `probe2_x/z`, `radius`, `half_height`, `half_x`,
`half_z`.

`room_event_item_return` performs the item swaps: the broken shotgun on the rack, and the flamethrower
on the cave door holders (Chris).

### Room actions: stairs, ladders and slopes

Room actions are zones created by the room's init script (SCD 0x0D `room_action_set`). Their type is an
index into `g_RoomCheckActions`. The types were identified from the room pictures:

| Type | Kind | Handler | Rooms |
|---|---|---|---|
| 1 | door | | |
| 2 | message | | |
| 5 / 6 | staircase: the player walks the whole flight in one animation | `room_action_stairs_zone_x` / `_z` | see below |
| 12 | ladder | `set_ladder_zone` | stage 3 room 0x01 |
| 17 | slope | `slope_height_update` | stage 4 room 0x0D |

`room_action_water_zone` is the water zone action (`STATUS_IN_WATER`).

**Staircases (types 5 and 6).** `room_action_stairs_zone_x` / `room_action_stairs_zone_z` set
`STATUS_STAIRS_ZONE`. `player_climb_sequence` then walks the whole flight in one animation: anim 0x33
going up, 0x35 going down. The floor height changes by +-0xB45, or +-0x57A in rooms 0x0E and 0x05.
`player_climb_sequence` also climbs boxes. Rooms with staircase zones:

| Room | Where it is |
|---|---|
| Stage 2 room 0x03 | 2F main hall (2 zones) |
| Stage 2 room 0x07 | 2F far-left stairs near the study (where Rebecca is attacked), down to the west wing save room; "Save me from this pain" wall writing |
| Stage 2 room 0x0E | 2F right stairs near the first Yawn fight |
| Stage 3 room 0x05 | Courtyard: eagle/wolf medal hollows; the fountain has stairs once it is opened |
| Stage 6 (1F) room 0x10 | Stairs down to the kitchen ("elevator doesn't stop on this floor") |
| Stage 7 and the Advanced copies | copies of the above |

**Ladders (type 12).** `set_ladder_zone` sets `GAME_LADDER`. Behaviour 0xB is the ladder. The only
ladder zone is in stage 3 room 0x01: the courtyard waterway with the red ladders, right after entering
the guardhouse stage. The ladder in stage 4 room 0x0D (below the guardhouse, near the Neptune lab /
V-JOLT spot) is not a ladder zone: it is a DOOR-type transition, and the RDT has no ladder zone for it.

**Slopes (type 17).** `slope_height_update` is a ramp. Its only known use is stage 4 room 0x0D: the zone
(flag 0x40, automatic) is the sloping corridor, which rises +36 in height per 175 units.

**Boxes and pushing.** `GAME_PUSHING` is set after the player has pushed a movable object (statue,
crate) for 9 frames; `GAME_ON_BOX` while the player stands on a climbable box (only turning, no doors);
`STATUS_BOX_CLIMB` while climbing onto it (climb step 0 with `GAME_ON_BOX`).

### Identified rooms

| Room | What it is |
|---|---|
| STAGE3 (whole stage) | Courtyard / underground caves |
| ROOM3080 / 3081 | Cave room with the Barry choice: "Will you go with him? / Do you want him to go first? / Will you stay here?" |
| ROOM3090 / 3091 | Cave room with the broken power generator |
| ROOM30A0 / 30A1 | The room with Enrico's body ("He couldn't have moved because of this injury... He's holding something"; Jill: "Barry is investigating...") |
| Stage 3 room 0x0C | Cave room with the spider web (enemy type 0x13) |
| ROOM2050 / 2051, ROOM7050 / 7051 | 2F statue-and-vents puzzle (poison gas) |
| ROOM4090 / 4091 | Guardhouse chemical mixing (poison gas) |
| ROOM5070 / 5071 | Lab room with poison gas |
| ROOM7060 (Chris) | The "save Rebecca before the hunter kills her" scene |
| ROOM71C0 / 71C1 | The mansion kitchen (freezer, rusty oven, gas stove, "Dark blood sticks to it. What did they cook here?") |

Staircase, ladder and slope rooms are listed in [Room actions](#room-actions-stairs-ladders-and-slopes).

### Enemy persistence between visits

Enemies keep their positions when you re-enter a room only in ROOM10E0 / 60E0 (zombies) and ROOM7060
(hunters). Everywhere else they respawn at their script position (EM_SET +4 `no_restore`). Confirmed in
game.

### Room-file research scripts (`tools/`)

Run these from the project root. They read the disc dump in `extracted/`. The `rdt_*.py` scripts parse
the "RDT (Resident Evil 1997)" page of the Just Solve the File Format Problem wiki, saved as HTML at
`reference/rdt_format.htm` (or the path in the `RDT_FORMAT_PAGE` environment variable).

| Script | What it does |
|---|---|
| `tools/rdt_em_set.py` | Lists the EM_SET (entity spawn, opcode 0x1B) records in the init script of every room. Opcode lengths come from the RDT page. |
| `tools/rdt_msg.py <ROOM files...>` | Prints a room's text messages (RDT section 11, pointer at +0x74; section pointers start at +0x48). The character table comes from the RDT page. Spaces are shown as `.`, control codes (line break, end, colour, ...) as `{XX}`. Useful for identifying rooms. |
| `tools/rdt_zones.py [types...] [-v]` | Lists the rooms whose init script creates room-action zones of the given `g_RoomCheckActions` types (default 5 6). |
| `tools/emd_tim.py <dir> <EMD names...>` | Finds the TIM texture(s) inside EMD model files and saves them as PNG, to identify a model. |
| `tools/bss_view.py STAGE2/ROOM207 ...` | Decodes a room's camera backgrounds (one MDEC frame per camera, 0x8000 bytes each) into PNGs under `temp/rooms/`, plus one labelled contact sheet per room. Uses jPSXdec (`tools/external/jpsxdec`, needs Java). |

---

## Entities

### Entity layout

Every object (player, enemies, NPCs, room objects) is an `Entity` (`include/re1_types.h`) of 0x18C bytes
(`ENTITY_SIZE`). `g_CurrentEntity` (0x800CAB14) points to the one being updated; code sets fields
through it. The layout matches the RE1 PC decomp's `Entities.h`. The common part ends at 0x16C; the
enemy-specific work starts there and differs per enemy.

| Offset | Field |
|---|---|
| 0x04 | `sca_info` (room objects) |
| 0x08 | `sca_hit_data`: pointer to the hit-centre offset (3 shorts x, y, z from the entity position, stride 6: `HitPoint`) |
| 0x0C | GsDOBJ-style handle |
| 0x18 | `packet` |
| 0x1C | GsCOORDINATE2: `flg`, `coord` 0x20, `workm` 0x40 (`coord_workm`), `param` 0x60, `super` 0x64, `sub` 0x68. `coord_m` is the rotation part of the coordinate matrix; its `t` is `pos_x/y/z` (0x34..). A room object attached to the player has `super` pointing to 0x800C5140 |
| 0x74 | `angle` |
| 0x7E | `prev_angle` |
| 0x80 / 0x82 | `scd_rot_speed_x` / `_y` |
| 0x84 | `state`; 0x84..0x87 = `state`, `sub_state`, `action_behavior`, `action_state`, also written together as one s32. `sub_state` is the second-level state index (PC name `ignore_player_flag`) |
| 0x86 / 0x87 | `action_behavior` / `action_state` (a byte that callers add `u8` results to) |
| 0x8C | `blend_counter` |
| 0x8E | `floor_y` |
| 0x90 / 0x94 | `animHeader` / `animBase` |
| 0x98 | `joints`: pointer to the `Joint` array |
| 0xAC | `mirror_joints` |
| 0xB0 / 0xB4 | `mirror_weapon_tmd` / `mirror_weapon_packet` |
| 0xB8 | `aim_target`: auto-aim lock (the tyrant sets itself; Plant 42 uses it for its body clone; the Yawn stores a joint pointer here) |
| 0xBD / 0xBE / 0xBF | `animationId` / `animation_frame_id` / `timing_control` |
| 0xC1 | `unused_0C1` |
| 0xC2 | `move_speed_current` |
| 0xC4 | `action_ticks_counter` |
| 0xCA | `scale` |
| 0xDC | player status flags (poison, gas; see [Poison and gas](#poison-and-gas)) |
| 0xE4 | `Billboard shadow` (0x78 bytes, to 0x15B) |
| 0x15C | `pool_buffer`: for enemies, the entity's allocation in the load pool (zombie: 2 extra billboards); the player uses 0x15C.. as `PlayerWork` |
| 0x161 | `slot_info` |
| 0x164 | `sight_check` |
| 0x16B | `dc_double_step`: the Director's Cut "fast zombie" |
| 0x16C..0x18B | enemy work area (per-enemy struct, see [Enemy work areas](#enemy-work-areas)) |

Notes on how the code accesses Entity:

- Every byte field is loaded with `lbu`; halfword fields are loaded with mixed signedness, even in
  `-O0` code.
- The sub-structure at 0x16C (`sub16C[k]`) and fields at 0x170+ are typically reached address-first
  (e.g. an `s16` at 0x170 passed to the stages' engine call).

Room objects (`ModelObject`) share some fields with Entity: `sca_info` 0x04, `sca_hit_data` 0x08,
`position_vx/vy/vz` 0x6C-0x70, plus `sca_radius` (`chk_obj_slide` gets two room objects). Room objects
are treated as Entity pointers.

Player-side copies of some fields have their own globals: `g_PlayerFloorY`, `g_PlayerScale`,
`g_PlayerScaHitData`, `g_PlayerCoord2Ptr`, `g_PlayerCoordM00..M22` (see [The player](#the-player)).

### Enemy slots

The enemy entity array (`g_EnemiesList`) starts at 0x800C52A4: stride 0x18C, 30 entries (weapon damage
walks all 30). Slot 0 is where NPCs are placed.

### Joint

Entity `joints` points to an array of `Joint` records of 0x7C bytes (names from the PC decomp's
JointStruct):

- `flags`, `index`, `rotation` (SVECTOR), `anim_slot`, `anim_object`.
- A GsDOBJ5 at +0x0C (attribute, coord2, tmd, id; `dobj_id`).
- A GsCOORDINATE2 at +0x20: `flg` = the joint's scale flag, `coord` = its `transform` (MATRIX,
  parent-relative), `workm` = its `world` matrix (`world.t` = position in the room); also
  `coord_param` / `coord_super` / `coord_sub`.
- `vel_x/y/z` + `rot_delta_x/y/z` for severed/detached parts.
- `fx_frame` / `fx_state`: the head-burst / severed-limb / bouncing-head effect.

`setup_joint_structures` points each joint's coord2 at its own GsCOORDINATE2 and writes `->packet` /
`->id`. The ejected magazine clip is a Joint at joints + 0x744 (`ClipJoint`).

### Billboard: the drop shadow and the blood pool

Entity 0xE4..0x15B is a `Billboard` (`offset_x/y/z`, `POLY_FT4 poly[2]`, `SVECTOR corners[4]`;
functions `billboard_init` / `billboard_draw` / `billboard_set_color` / `billboard_adj_size` /
`billboard_set_size` / `billboard_set_rect`). The PC decomp calls 0xE4.. "pushVelocity"; the
main-program symbols are `g_PlayerShadowOffsetX/Z`.

- It is every entity's round drop shadow, drawn with subtractive blend, grey 0x808080.
- When its owner dies, the death code recolours it (0xFFFF50 -> dark red), shrinks it and then grows
  it each frame: that is the spreading blood pool. Confirmed in game.
- The web spinner tints its shadow.
- `STATUS_SHADOW_DEPTH_SORT` makes shadows depth-sorted, so they are hidden behind objects such as the
  dining-room table.

### Effect

`Effect` is the 0x84-byte record of `g_EffectPool` (0x800C3024; PC struct `Effect`). `EFFECT` is the
current slot (`g_ActiveEffectIndex`, 0x800CF86E).

- 0x03 `scale`, 0x08 `accel_x/y/z`, 0x10 `vel_x/y/z`, 0x18 `disp_x/y/z` (a displacement: the velocity is
  added to it).
- 0x0E `draw_flags`: bit 0 animate, bit 1 recompute, bits 2/3 y snap; `>> 6 << 22` = sprite attribute;
  0x4000 semi-transparent, 0x8000 hidden.
- `pre_behavior` / `post_behavior`, `parent_matrix`, `transform` (a MATRIX, `transform[9]`; its
  translation is `trans_*`), `frame_list` / `frame_cur` (`frame_delay` / `frame_index`), `uv_list` /
  `uv_cur`.
- PC field names also cover: yaw, `rot_speed_x/y/z`, `pos_x/y/z`, `effect_type`, `depth_group`,
  `sprite_offset_*`, `spawn_*`, render pointers, `anim_id`, `update_id`, `type`, `light_factor`.
- An animation step (`EffectAnimStep`): the first 0x18 bytes of a slot are (re)loaded from the step
  list.

Effect behaviours include `effect_behavior_shell_casing` (shotgun shells), `effect_behavior_init_scale`
and `effect_behavior_rand_anim`. Effect frame and sprite tables have their own struct views.

### Other records

| Record | Contents |
|---|---|
| `ModelObject` | 0xA4-byte room-object / item-model records (`g_OmodelTable`, `g_ItemModelTable`, `g_ItemboxObject`): GsDOBJ at 0x0C, GsCOORDINATE2 at 0x1C, billboard at 0x86 |
| `ScaInfo` | the collision record: flags, `ofs_x/y/z`, `half_height`, `radius` |
| `AtkEnt` | the weapon hit record: `fx_x/y/z` (hit effect offset), `damage`, `fx_type`, `fx_variant`, `hit_state` |
| `TaskSlot` | the 0x80-byte scheduler slot |
| `SelPlayer` | a select screen panel (0x240 bytes) |
| `StHEADER` | the STR sector header |
| `InteractiveScreen` | +0x0C `saved_pos_y` |
| `DoorCmd` | `active` |
| `FileScreen` | the files screen: `exit_selected`, `arrow_blink` (+ `blink_timer`, `hold_timer`) |
| `ClWindow` | computer-lab window; 0x16 `keyboard_ramp`: non-zero fades to 0x80 instead of 0xFF |
| `ClLogoSlot` | `active`, `phase_signal`, `parent`, `ot_pri`, `pivot_x`, `pivot_y` |
| `EndingSlot` | `ot_pri`, `vram_x`, `vram_y`, `box_vel_x`, `box_vel_y`, `rotate` |

Other records with struct views: `CdFileEntry`, `VibrationEntry`, `MapScreen`, the EMR header /
armature, `AnimEntry` / `AnimFrame` (animation step tables), `POLY_GT3`, `TmdObject` / `TmdHeader`, the
item viewer block (`ViewerBlock`), and the door record (lock / key / destination).

### Mirror rendering

Rooms with a mirror set `GAME_MIRROR_ON` / `GAME_MIRROR_AXIS` through SCD 0x0F (`scd_op_0f_mirror_set`).
The mirror renderer (`entity_render_mirror`, 0x80030E18, with `mirror_reflect_view`, `mirror_point_visible` and
`mirror_update_parts` in `src/main/model.c`) draws a mirrored GsRVIEW2 view plus a winding-flipped TMD
copy (entity fields `mirror_joints`, `mirror_weapon_tmd`, `mirror_weapon_packet`; globals
`g_MirrorPlane`, `g_MirrorRangeMin`, `g_MirrorRangeMax`). Effects are mirrored by
`effect_draw_mirror_reflection`.

---

## The player

The player is the Entity `g_PlayerEntity` at 0x800C5124. Byte +1 is the player id (`g_PlayerId`,
0x800C5125; `& 1` = Chris/Jill). In the main program many of its fields are also separate symbols:

| Symbol | Address | Entity field |
|---|---|---|
| `g_PlayerAngle` | 0x800C5198 | +0x74 `angle` |
| `g_PlayerHealth` | 0x800C51AC | |
| `g_PlayerHitState` | 0x800C51AE | |
| `g_PlayerJoints` | 0x800C51BC | +0x98 `joints` |
| `g_PlayerAnimationId` | 0x800C51E1 | +0xBD `animationId` |
| `g_PlayerPoisonTimer` | 0x800C5298 | +0x174 `poison_timer` |

Others: `g_PlayerFloorY`, `g_PlayerScale`, `g_PlayerScaHitData`, `g_PlayerCoord2Ptr`,
`g_PlayerCoordM00..M22`, `g_PlayerShadowOffsetX/Z`.

### PlayerWork

`PlayerWork` (entity 0x15C.., `g_PlayerWork`):

| Offset | Field | Meaning |
|---|---|---|
| 0x15C / 0x160 | `weapon_anim_header` / `weapon_anim_base` | animation set of the equipped weapon (`load_equipped_weapon_animation`) |
| 0x164 / 0x168 | `action_anim_header` / `action_anim_base` | ladder / push / door-open animations |
| 0x16C / 0x170 | `grab_anim_header` / `grab_anim_base` | player animations stored in the enemy's EMD (grabs, bites, kills; `load_entity_emd`) |
| 0x174 | `poison_timer` | frames to the next poison damage |
| 0x175 | `max_health` | set by character (`initialize_game`; Chris 140) |
| 0x176 | `aim_flags` | 0x20 up / 0x40 neutral / 0x80 down |
| 0x178 | `aim_state` | result of `player_find_aim_target` (0 = nothing in range) |

### Controls, aiming and weapons

- Standing turns faster than running (`g_PlayerTurnSpeedStand` / `g_PlayerTurnSpeedRun`).
- Aiming up / neutral / down is kept in `aim_flags`; auto-aim locks onto an entity's `aim_target`.
- The equipped weapon's animations are loaded into `weapon_anim_header` / `weapon_anim_base`.
- Reloading ejects the magazine clip as a detached joint (`ClipJoint`, joints + 0x744), driven by
  `player_clip_eject_start`, `player_clip_state_0_spawn` and `player_update_detached_joint`.
- Knife hits use `g_KnifeHitFrameWindows` and `g_KnifeHitEffectOffsets`; shotgun shells are ejected as
  effects (`effect_behavior_shell_casing`).

### Health

- Max HP depends on character and mode (see [Game modes](#game-modes)); the ECG labels of the status
  screen are described in [Status screen](#status-screen).
- The hunter's decapitation kills the player outright: `player_anim_hunter_decapitation` sets health
  to -1.

### Poison and gas

While the player's +0xDC flags have any of the bits 0x62 set, `poison_timer` counts down. At 0 the
player loses 2 HP and the timer restarts at 120 (7 with bit 0x40 = severe). HP cannot drop below 1
unless the poison is severe.

| Bit in +0xDC | Kind | Source | Cure |
|---|---|---|---|
| 0x02 | ordinary poison | a web spinner hit (enemy ids 3/4) poisons 25% of the time (timer 150, flags \|= 2); adders and wasps set 150 | blue-herb type (effect 0x20) |
| 0x20 | Yawn's bite | Yawn | serum type (effect 0x10); clears story flag 0x43 |
| 0x40 | in poison gas (room-local fast/lethal drain) | room scripts | none |

- The first Yawn bite leads to a cutscene: the character passes out and is moved to the medicine room.
- The gas bit 0x40 is set by room scripts with `33 08 40 01` (op 0x33 sub-command 8), cleared on room
  entry (`room.c`), and set by no C code. Rooms: ROOM2050/2051 + ROOM7050/7051 (the 2F
  statue-and-vents puzzle: "Poison gas is escaping", "A weird hole opens"), ROOM4090/4091 (guardhouse
  chemical mixing: "It's poison gas"), ROOM5070/5071 (lab, "Poison gas is escaping"), ROOMC070/C071
  (arrange).
- The poison death has its own player animation hook (`player_anim_poison_death`).

### Stairs, ladders, slopes and climbing

See [Room actions](#room-actions-stairs-ladders-and-slopes). In short: staircases are walked in one
animation (`player_climb_sequence`, anims 0x33 / 0x35), the ladder uses behaviour 0xB and
`GAME_LADDER`, slopes adjust the height through `slope_height_update`, and `player_climb_sequence`
also climbs boxes. The ladder / push / door-open animations come from `action_anim_header` /
`action_anim_base`.

---

## Enemies

### Enemy work areas

Entity bytes 0x16C..0x18B are each enemy's private data, described by a per-enemy struct
(`ZombieWork`, `HunterWork`, ...; specs in `dualshock/config/enemy_work/`). Each enemy's `<enemy>_update`
dispatches `state_tbl[g_CurrentEntity->state]()`. Enemy AI uses `sp_tmp00` as the distance to the player
and the scratchpad `sp_path*` area for path search.

| Enemy | Field | Offset | Meaning |
|---|---|---|---|
| Zombie | `splatter_flag` | 0x174 | |
| | `hit_threshold` | 0x17D | |
| | `move_speed` | 0x180 | = 45 |
| | `turn_speed` | 0x181 | |
| | `stagger_timer` | 0x188 | |
| | `head_detached` | | the kicked-off head |
| Hunter | `npc_target` | 0x174 | the NPC the hunter goes for (see [Hunter](#hunter)) |
| | `decap_chance`, `decap_claw_joint`, `leap_target_x/z`, `decap_cooldown` | | decapitation leap |
| Tyrant | `hit_mask` | 0x16E | |
| | `heart` | 0x170 | pointer to its cloned heart entity |
| | `rocket_flag` | 0x180 | bit 7 is set when the player's byte the PC decomp reads as the equipped weapon is 10 (rocket launcher) |
| Yawn | `form` | 0x16E | 0 first fight, 1 rematch |
| Black tiger | `wound_count` | 0x17C | the next index into `g_BlackTigerWoundedJoints` |
| Chimera | `aggressive` | 0x17E | spawn behaviour 3 unlocks extra attacks |
| Cerberus | `swerve` | +0x14 in `CerberusWork` | the curve after a missed lunge |
| Plant 42 body | scale vector | 0x16C / 0x170 / 0x174 | s32 values for `api_ScaleMatrix` (`Plant42BodyWork`) |
| Plant 42 roots | `sink_tick` | | `plant42_roots` has its own layout |
| Computer arm (right) | `vel_x/z`, `pos_x/z` | | `ComputerArmRightWork` |
| Computer arm (left) | `vel_x/y/z`, `pos_x/y/z`, `slide_frames` | | `ComputerArmLeftWork` |

The zombie names fit the PC decomp's Entity 0x16C.. names. The computer arm values are all 16.16 fixed
point.

### Zombie

- The kitchen (ROOM71C0/71C1) spawns zombie EM1000 (behaviour 3) and zombie EM1011 (behaviour
  0x80/0x8E).
- **Director's Cut fast zombies** use `dc_double_step` (entity +0x16B).
- **Head burst**: a shotgun shot within about 3000 units while aiming up, or a magnum-class weapon while
  aiming up, bursts the head. The head joint's own triangles are tinted dark red and fly apart for 30
  frames. This is a joint effect (`fx_frame` / `fx_state`), not particles. Web spinners (ids 3/4) get the
  same effect in green. In Advanced mode the instant head burst is disabled in the kitchen
  (ROOM71C0/71C1).
- **Crawling zombies** have behaviour flag bit 2: they start on the floor and do not stagger.
- **Jill's kick**: when a crawling zombie grabs Jill from one side (`attack_direction` 2), Jill kicks its
  head off. The head joint detaches, spins and bounces (`blood_splatter_physics`), and the zombie dies
  (`zombie_attack_main` state 8). Confirmed in game.
- A grabbed player plays `player_anim_zombie_grabbed` (slot 0).
- Zombie function names cover the head bounce (`zombie_head_bounce_physics`), leg step speed, bite
  blood tints, the grabbed animation, the headless walk and the SCD walk/action dispatch. Related
  tables: `zombie_hit_threshold_hard_tbl`, `zombie_body_probe_neg` / `_pos`, `g_WalkSpeedDips`.
- Whether zombies can lose their legs is not confirmed.

### Hunter

**Decapitation.** The decapitation leap is behaviour 5 (`hunter_behavior_decap_leap`):
`hunter_decap_leap_start`, `hunter_decap_strike`, `hunter_decap_leap_end`, `hunter_decap_leap_track`,
`hunter_decap_leap_to_hold`, sub-table `hunter_decap_leap_sub_table`.

- `hunter_decap_strike` sets the player's state word to 0x607 directly (no damage calculation), with
  blood, a red claw tint, sound and vibration. That runs `player_anim_hunter_decapitation` (slot 44,
  health = -1). It is instant death. Confirmed in game.
- The scripted version (`hunter_scd_decap_leap_run` / `_tbl`) uses the same strike.
- `decap_chance` is rolled from `hunter_decap_chance_low` / `_mid` only while the player's HP is below
  `g_HunterPlayerHealthGate` (Chris 98 of 140, Jill 67 of 96, i.e. below 70% HP; Advanced 75 / 52).
- The leap starts from any distance only below 16 HP; otherwise only from >= 0x157C away.
- Not the decapitation: `hunter_act_pounce` (intro leap), `hunter_death_pounce` (dies mid-leap).

**Normal hits:** `hunter_atk_swing` -13 HP, `hunter_dodge_swipe` -20 HP.

**NPC targeting.** `npc_target` (entity +0x174) is the NPC a variant 0/1 hunter (behaviour low nibble 0
or 1) goes for: enemy slot 0 (0x800C52A4), or slot 1 if the hunter itself is slot 0. With no active NPC
there it is a normal hunter. The hunter goes for the NPC first, then for the player once the NPC dies
(the NPC's health < 0) or the player gets close. See [NPCs](#npcs).

### Cerberus

- After a missed lunge, the dog runs past in a curve before it turns back (`CerberusWork` +0x14,
  `swerve`).
- The deadly throat attack happens when the player is in Danger (in code: HP < 17 and facing the dog).
  This matches observed play but is not fully confirmed.
- The maul uses DualShock vibration. The player recovers with `player_anim_simple_recovery` (slot 40).
- The PS1 splits the cerberus damage / death / maul / bite helpers that the PC merges.

### Black tiger

- `wound_count` (0x17C) is the next index into `g_BlackTigerWoundedJoints`.
- Stage 3 holds four black tiger behaviours that the PC version lacks (dead code): lunge, single web
  spit, ceiling drop, and an empty one.

### Web spinner and the spider web

- Web spinner hits (enemy ids 3/4) poison 25% of the time (see [Poison and gas](#poison-and-gas)). Their
  heads burst in green, and the web spinner tints its shadow.
- The PS1 has separate fast and slow idle tables for the web spinner (the PC merges them).
- The cave spider web (stage 3 room 0x0C, enemy type 0x13) has to be cut before you can pass. A spider's
  projectile that reaches the web also breaks it (`effect_projectile_hit_check`). The web's update
  handler `spiderweb_update` is in the main program (enemy update table entry 0x13).

### Monster plant

- Scenario flag 0x5B belongs to the monster plant. Its hold on the player runs
  `player_anim_plant_hold` (slot 34), which uses DualShock vibration.
- The PS1 splits the monster plant step table into 3 tables.

### Plant 42

- You damage the bulb hanging from the ceiling. In code this is the cloned body entity (reached through
  Plant 42's +0xB8 `aim_target`; `plant42_body_*` functions), which counts the hits. Its work area is an
  s32 scale vector for `api_ScaleMatrix` (`Plant42BodyWork`).
- Plant 42 can only really die after the V-JOLT has been used, and it flashes / changes colour while
  dying. Scenario flag 0x29 = Plant 42 dead.
- `plant42_roots` has its own layout (`sink_tick`); the PS1 has a separate roots phase table.
- Some Plant 42 functions are empty.

### Yawn

- `form` (0x16E): 0 first fight (stage 2), 1 rematch (stage 7, "Yawn 2").
- The Yawn stores a joint pointer in its `aim_target` (+0xB8).
- Its swallow runs `player_anim_death_alt` (slot 51). Its bite poisons (bit 0x20, see
  [Poison and gas](#poison-and-gas)).
- Whether Yawn takes damage along its body is not confirmed.

### Tyrant

- The tyrant sets itself as its own `aim_target`. `heart` (0x170) points to its cloned heart entity;
  `rocket_flag` (0x180) bit 7 marks the rocket launcher; `hit_mask` at 0x16E.
- **Texture animation** (`tyrant_scroll_body_uvs`, called each frame from `tyrant_update`): scrolls the
  UVs of the tyrant's polygons (GT3 packets of joint 2) every frame. The PC port replaced the call with
  an empty function (the PC decomp guessed "pad rumble").
- The impale uses DualShock vibration.
- A tyrant CLUT semi-transparency function is dead code.
- After the Tyrant is beaten the game is cleared (see [Game cleared](#game-cleared)).

### Chimera

- `aggressive` (0x17E): spawn behaviour 3 unlocks extra attacks.
- The grab uses DualShock vibration; the knockdown runs `player_anim_chimera_knockdown`.

### Computer arms (laboratory)

- Right arm: `ComputerArmRightWork` (`vel_x/z`, `pos_x/z`).
- Left arm: `ComputerArmLeftWork` (`vel_x/y/z`, `pos_x/y/z`, `slide_frames`). The `arm15_*`,
  `arm_step_*15` and lever steps all belong to the left arm's step tables, starting at
  `arm15_cmd_login_tbl` (0x8012DE10).
- All values are 16.16 fixed point. The PS1 has per-command tables that the PC merges.

### Other enemies

- **Crow**: its attacks run `player_anim_multi_attack` (slot 24).
- **Adders** and **wasps**: set the ordinary poison timer to 150.
- **Neptune**: in the guardhouse stage (STAGE4).

---

## NPCs

- NPCs are spawned by EM_SET in enemy slot 0 (0x800C52A4). Their update handler is the main program's
  `character_npc_update` (enemy update table entries 0x17..).
- Hunters with behaviour low nibble 0 or 1 target slot 0 through `npc_target` (see [Hunter](#hunter)).

### The "save Rebecca from the hunter" scene

Chris must reach Rebecca before a hunter kills her. The mechanism is the hunter's `npc_target`
(entity +0x174) plus the EM_SET placements:

- **STAGE7/ROOM7060.RDT** (Chris, revisited mansion 2F): Rebecca (model 0x2C, with the same texture as
  EM1023 = Rebecca) in slot 0 in three alternative branches, plus hunters with behaviour 0x60 in slot 1.
  This is the scene. Hunters here keep their positions between visits.
- Jill counterparts with Barry: STAGE3/ROOM3081 (Barry, model 0x22) and ROOM3091 (Barry, model 0x2D, a
  second Barry model), with variant-0 hunters.
- Chris: ROOM30A0 has Enrico (model 0x28) in slot 0 with behaviour 0x80 hunters.

Other NPC facts:

- Chris's start item list also sets Rebecca's slots. Rebecca's max HP per mode is in
  [Game modes](#game-modes).
- In ROOM3080/3081 Barry offers a choice: "Will you go with him? / Do you want him to go first? / Will
  you stay here?"

---

## Items and inventory

- `g_InventorySlots` (0x800C8784, in the save block) = `{item, qty}[6]`.
- Item 0x41 is the character's special item: the lighter (Chris) or the lockpick (Jill). It goes in the
  special inventory slot. Confirmed in game. Scenario flag 0x7C is the lockpick.
- Start items (`g_StartItemsChris` / `g_StartItemsJill`):
  - Chris: knife + item 0x41.
  - Jill: knife + item 2 x15 + item 0x41. In Advanced mode Jill's list swaps item 2 for item 4.
- Items 0x3E to 0x40 are the Doom Books. When opened in the item viewer, they reveal the crests for the
  lab entrance.
- Item 0x4D is opened by the status screen's RADIO tab. Scenario flag 0x7F = has the radio.
- Scenario flag 0x7E = infinite rocket launcher.
- Training mode doubles ammo and ink ribbon pickups and puts 2 x 30 of item 0x0B in the item box;
  Advanced gives 3 ink ribbons per pickup instead of 2 (see [Game modes](#game-modes)).
- `g_ItemExaminedFlags` (save block) records examined items.
- Item pickups: an item lying in the room opens the menu in mode 3 (`MENU_MODE_PICKUP`); an item given by
  a room script (SCD 0x2D) uses mode 4; `GAME_FILE_PICKUP` opens the menu on the document (file) pickup
  screen. `pickup_fade_update` (0x800297C4) sets a fade colour triple with a chained assignment.
- `room_event_item_return` swaps the broken shotgun on the rack and the flamethrower on the cave door
  holders (Chris). The crank is handed back after its use animation (mode 4).
- Item models are `ModelObject` records (`g_ItemModelTable`, `g_ItemboxObject`); the item viewer uses the
  `ViewerBlock` record, the shared `g_DoorFile` area for its model and the scratchpad 0x180-0x1D0 light
  matrices.

---

## Menus

### MenuWork

`MenuWork` (`include/re1_types.h`, 0x175 bytes at 0x800BC5A8) is one struct for `g_MenuWork`, used by
`main_menu`, `menu_draw_inventory`, `menu_item_submenu` and `item_viewer_update`. Fields:

- `mode` (+0, `MENU_MODE_*`), `state`, `frames_ptr`.
- `g_MenuFlags` (+3) has its own symbol.
- `submenu_state` / `submenu_row` / `submenu_timer` / `move_anim` (+0x11..+0x18).
- `viewer_state` / `viewer_action` / `obj_count` (+0x39..+0x3B).
- `spin_input_x/y/z`, `obj_rot[2]`, `rot`, `obj[2]` (GsDOBJ5), `coord[3]` (GsCOORDINATE2), `timer`.
- The health bar / ECG part is named after its globals (`health_status`, `ekg_*`, `face_anim`).
- Also `char_flags`, `slot_count`, `has_radio` (`g_MenuHasRadio`, 0x800BC5B7), `cursor`.

### Menu modes

`g_MenuWork.mode` says why the menu was opened (PC `DAT_00ae9f10`, `MSF_MENU_MODE_*`; `status_menu.c`
`sel`, `item.c` `mode`; used in `main_menu` and `item_viewer_update`):

| Value | Name | Meaning |
|---|---|---|
| 0 | `MENU_MODE_STATUS` | START: the status screen (Examine) |
| 1 | `MENU_MODE_KEY_USED_UP` | a key item is used up: "drop it?" |
| 2 | `MENU_MODE_ITEMBOX` | item box |
| 3 | `MENU_MODE_PICKUP` | an item lying in the room: model + "Will you take it?" |
| 4 | `MENU_MODE_GOT_ITEM` | given by a room script (SCD 0x2D): model + message, the item is taken automatically, no yes/no. Example: the crank, which the game gives back after its use animation |
| 5 | `MENU_MODE_MAP` | the map display (set by room scripts; used by 7 room scripts) |
| 6 | `MENU_MODE_KEY_USED_UP_VIEW` | Dual Shock only: mode 1 for item ids >= 0x33, showing the used-up item in the viewer (message 0xC7) |

- `MENU_REQUEST(mode)` = `0x4000 >> mode`; requests sit in the menu byte of `g_GameStateFlags`
  (`GAME_MENU_REQUESTS`, 0x7F00).
- `MENU_REQUEST(MENU_MODE_STATUS)` is set by about 14 stage-3 scripts. What it is for in game is not
  confirmed (possibly the radio).
- `GAME_OPTIONS_REQUEST` (0x400000) opens the options menu instead of the status menu.
- Strings: `s_Nothing` "_Nothing_" (`itembox_draw`), `s_YesNo` "Yes  No" (`update_message_display`).
- `g_ItemboxScrollDir` has a second use in the status menu: "selected item is a weapon" (EQUIP vs USE
  row).
- When the status menu opens, a 64x256 VRAM rect at (0x180, 0x100) is saved to the `g_RoomBgBsBuf` area
  and restored by `status_menu` / `file_reader`.

### Status screen

- The RADIO tab (status tab 4, which opens item 0x4D) is greyed out until you have the radio:
  `g_MenuHasRadio` = scenario flag 0x7F (PC `HAS_RADIO`). Confirmed in game.
- The ECG labels are only Fine, Caution, Danger and Poison. In code: 0 = Danger, 1 = Caution, 2/3 = Fine,
  4 = Poison. The ECG line is drawn with the scratchpad GsGLINE `sp_gline`.
- Scenario flag 0x7D is the menu fade latch.

### Map

Rooms you have not visited are hidden unless you have picked up that area's map. Full maps (usually
puzzle rewards) fill the whole area in. This matches `map.c`, which clears CLUT entries per room. The
visited-room and map flags are in bank 8 (`g_RoomFlags`: maps 0x7C+). Map screen records: `MapScreen`,
`g_MapBrowserSprites`, `g_MapTabSprites`.

### Files

Files (documents) are tracked in bank 8 (`g_RoomFlags`, files 0x82+). The files screen is `FileScreen`;
file pages use the shared `g_DoorFile` area.

### Title screen and options

- The title screen is a background (plus the title, possibly separate) and New Game / Load Game / Option
  buttons that change the screen as you select them. This fits the 6 TIMs in `TMOJI.TIM`
  (`g_TitleMojiBuf*`).
- The title menu choices for the game modes are 4, 5 and 6; holding Right on choice 6 changes the menu
  picture (see [Game modes](#game-modes)).
- When the title screen is idle, an attract-mode demo plays (`STATUS_ATTRACT_DEMO`).
- In Options, the controller picture follows the connected pad automatically (digital or Dual Shock).
  The options menu also sets `STATUS_SCREEN_BORDER` (draw the room background with borders).

### Select screen

The character select screen is the SELECT overlay; see [The select overlay](#the-select-overlay).

---

## Movies

`g_MovieTable` (0x80094898) has `MovieEntry` records of 0x14 bytes. Each entry holds a file index, which
goes through `g_CdFileTable` to a disc file. `g_SelectedFmvId` indexes the table; the main loop plays it
when `GAME_FMV_REQUEST` is set.

| Id | Name | File | Content |
|---|---|---|---|
| 0 | `FMV_TITLE` | OJ.STR | the title intro (180 frames) |
| 1 | `FMV_INTRO` | PJ.STR | the live-action intro + CAST roll (3153 frames) |
| 2 | `FMV_SELF_DESTRUCT` | DMF.STR | aerial view + a fireball ending in white: the explosion when the self-destruct countdown runs out. It cannot be skipped |
| 3.. | | DM1-DMB (DM*.STR) | started by room scripts |
| 10 | | file 0 (BIO.TIM) | a placeholder |
| 0xE | `FMV_ENDING_BASE` | DME.STR | + ending id 1..8 = ED1..ED8.STR |

The entries from 3 on are described two ways: as ids 3..14 that are all DM*.STR files started by room
scripts, and with entry 10 as a placeholder pointing at file 0 (BIO.TIM). Which entries are
placeholders is not confirmed.

The disc holds 24 STR files, about 17 minutes in total, at 15 fps, 320x240 or 256x240.

### The prologue cut

`fmv_play` (0x800376DC) skips part of movie 1 (PJ.STR, `FMV_INTRO`) **for Jill only**
(`(g_SavePlayerId & 3) == 1`): when the frame counter reaches 0x97E it fades the CD audio out and seeks
to frame 0xA15, fades the audio back in from 0xA16, and from 0xA1E plays on normally. The skipped part
is the mansion doors / main hall shots just before "CAST", which Chris narrates.

### Movie files

- The `ZMOVIE/*.STR` files are stored with 2336-byte sectors. To play them with ffmpeg (`-f psxstr`),
  rebuild them as 2352-byte sectors (add the sync + MSF header + mode 2).
- The STR sector header is `StHEADER`. The title FMV buffer is carved from the load area
  (`g_FmvBuffer = g_LoadPointer`).
- The number, length and resolution of the movies are in [disc_contents.md](disc_contents.md).

---

## Sound

- The music is sequenced (`SOUND/SEPxx.HSB`: VAB header + SEQ + VAB body), not streamed; see
  [disc_contents.md](disc_contents.md).
- Sound banks are uploaded by `SsVabTransBody` with `SpuWrite` (0x8008C37C). The sound libraries come
  from an older SDK than the rest (see [psyq_sdk.md](psyq_sdk.md)).
- Bank 1 holds 48 sounds, or 16 when `STATUS_SFX_BANK1_SMALL` is set (memory card load, sfx bank 2).
  `sound_load_bank1` (banks 0xC / 0xD / 0xE) stages the `.VB` data in the load area at 0x80105400.
- Each stage overlay installs the stage's BGM and voice tables. `g_RoomBgmState` (save block) keeps the
  room music state.
- `bgm_fade_out_all` (0x80054FB8) fades out the music, waiting with `task_sleep(1)`.
- Character voices are XA audio (`VOICE/`); `GAME_VOICE_PLAYING` (0x20000 in `g_GameStateFlags`) is set
  while an XA voice line plays.
- `STATUS_DOOR_WAIT_BGM` makes the door creak wait for the next room's music.
- `s16 g_SndPanVol[3][2]` holds pan/volume values.

---

## Game modes, characters, saves and game cleared

### Characters

- Player id 0 = Chris, 1 = Jill (`g_PlayerId & 1`). The memory card title picks "Jill" when `id & 1` is
  set.
- `max_health` is set by character in `initialize_game`.
- Start items and the special item 0x41: see [Items and inventory](#items-and-inventory).
- `STATUS_ALT_COSTUME` (0x4000000) makes the player use the alternate costume model (id 0x33).
- `STATUS_NEW_GAME` is set by `init_player_data` (a new game); `GAME_LOADED_SAVE` marks a game started
  from a memory card save.

### Game modes

The Director's Cut modes are stored in `g_StatusFlags` (0x800C3004). The title menu choices are 4, 5 and
6. The save file stores the mode as byte 1/2/3, which restores the mode on load. The names match the PC
decomp (TRAINING, ADVANCED, ADVANCED* = ADVANCED_HOLD). The status flag selects the damage column.

| Mode | How to select | Flag | Save tag | Max HP (Chris / Jill / Rebecca) | Damage table |
|---|---|---|---|---|---|
| Original | choice 4 | none | (`s_MemcardTagNone`) | 140 / 96 / 88 | 0 |
| TRAINING | choice 5 | 0x40000 | "Tr" | 180 / 150 / 140 | 1 |
| ADVANCED | choice 6 | 0x20000 | "Ad" | 100 / 70 / 64 | 2 |
| ADVANCED* | hold Right for 90 frames on choice 6 | 0x20000 + 0x10000 | "AT" | as Advanced | 0 (normal) |

- **TRAINING**: damage table 1 has 42 entries higher than normal and none lower. Ammo and ink ribbon
  pickups are doubled. The item box starts with 2 x 30 of item 0x0B. It uses the normal rooms.
- **ADVANCED**: damage table 2 has 68 entries lower than normal and 3 higher. The rooms in
  `g_AdvancedModeRooms` load the stage + 7 "arrange" RDT. An ink ribbon pickup gives 3 instead of 2.
  Jill's start list swaps item 2 for item 4. The stage background-checksum table has an Advanced
  variant. The hunter decapitation HP gate is 75 / 52.
- **ADVANCED***: holding Right on choice 6 changes the menu picture. This is the PC decomp's "green
  secret Advance mode". It uses the Advanced rooms, HP and ribbons, but the normal damage table and the
  doubled ammo pickups of Training.
- In Advanced mode the instant zombie head burst is disabled in the kitchen (ROOM71C0/71C1).

### Memory card saves

- File names: `s_SaveFilePattern` "BASLUS-00551*" (wildcard), `s_SaveFileName` "BASLUS-005510"; the last
  digit is increased by the slot number. `BASLUS-00551` is the Director's Cut product code.
- The path prefix `s_bu00` ("bu00:", `D_80010130`) is defined in `door.c` and used by `memcard.c`; it is
  copied into a 20-byte local buffer and zero-padded.
- Titles: `s_MemcardTitleJill` / `s_MemcardTitleChris`, Shift-JIS, e.g. "R.E-DC 1. Jill /00/" or
  "R.E-DC 1. Chris/00/" ("R.E-DC" = Director's Cut), built by `memcard_build_file_title`. The mode tag goes
  at +0xE, and the digits are patched in.
- The title tag comes from the status flag: `s_MemcardTagNone` / "Tr" (0x40000) / "Ad" (0x20000) / "AT"
  (0x10000).
- `s_SumError` is printed by `cd.c` but lives in `memcard.o`.
- `McDirInfo` is the memory card listing record; the memory card list builds each line in the scratchpad
  text buffer (`sp_text`).
- `memcard_wait_hw_event` (0x8001A1C8) is a `TestEvent` polling loop that counts down from a constant.
- The save block itself is `g_SaveData` (0x800C8460, 0x41C bytes, PC `g_BioCard`); see
  [Fixed work areas](#fixed-work-areas).

### Game over and the self-destruct

- `GAME_OVER` (0x1000000) starts the game-over fade when the player dies or the countdown runs out.
  The die screen (`game.c`) loads `DIED.TIM` (file 8) into the overlay area and builds its packets
  there.
- `g_CountdownTimer` (save block) is the lab self-destruct countdown; `STATUS_SELF_DESTRUCT` is set
  while it runs. When it runs out, `FMV_SELF_DESTRUCT` (DMF.STR) plays and cannot be skipped.

### Game cleared

After the Tyrant is beaten, the game plays the ending FMV, then the credits, then the results screen
(time, saves). This is the `STATUS_GAME_CLEARED` path: the final escape's script sets the flag, and the
game-over exit of `game_loop` then goes to the ending. The ending FMV is `FMV_ENDING_BASE` + ending id
1..8 (ED1..ED8.STR). The ENDING overlay (20 functions) carves its work buffers from the load area.

---

## CD loading

- `g_CdFileTable` (0x80096E54, `CdFileEntry[]`) maps file numbers to disc files. It decodes against
  `extracted/RE1-Dualshock.xml` (the disc dump listing). Examples: the stage overlays are files
  0x157..0x15D, `DIED.TIM` is file 8, and movie entry 10 points at file 0 (BIO.TIM).
- CD reads use retry loops: `str_cd_start_read` (0x8005D4FC) waits while `CdSync()` returns 5 and retries
  until `CdRead2(...)` succeeds.
- `load_file` normally waits with `task_sleep`; with `STATUS_BLOCKING_LOAD` (`load_file_blocking`) it
  waits with VSync instead.
- `load_room_bg_image` reads a room's camera background from the `ROOMsrr.BSS` file (lba + cut x 16
  sectors) into `g_RoomBgBsBuf` (0x8000 bytes, ending at 0x801FE900), checksums it and decodes it with
  the MDEC. `STATUS_BG_LOADING` is set meanwhile, and rumble pauses.
- `stage_overlay_load` / `load_stage_overlay(2, ...)` loads the stage overlay into task slot 2 at
  0x80105400; the room RDT and other room data follow it in the `g_LoadPointer` heap.
- During boot the load area also holds `FONT.TIM` / `STATUS.TIM`, `CORE00.ETM` and the bank-1 `.VB`
  staging.

---

## PS1-only features

Features of the PS1 Dual Shock version that the PC version does not have:

- **DualShock vibration**: the SCD opcodes 0x4F-0x52 (PS1 only), `VibrationEntry` records and
  `g_FireVibrationTbl` (weapon fire rumble). Enemy attacks with vibration: the cerberus maul, the plant
  hold, the tyrant impale, the chimera grab and the hunter decapitation. Rumble pauses during CD
  background reads (`STATUS_BG_LOADING`).
- **Menu mode 6** (`MENU_MODE_KEY_USED_UP_VIEW`, message 0xC7): shows a used-up key item (ids >= 0x33) in
  the item viewer.
- **Tyrant UV scrolling** (`tyrant_scroll_body_uvs`): the PC port replaced the call with an empty
  function.
- **Options**: the controller picture follows the connected pad (digital or Dual Shock); the PS1 pad
  library (`LIBPAD`) is newer than the rest of the SDK ([psyq_sdk.md](psyq_sdk.md)).
- **Split tables**: the PS1 splits tables that the PC merges: the monster plant step table (3 tables),
  the roots phase table, the web spinner idle fast/slow tables, the computer arm per-command tables,
  and the cerberus damage / death / maul / bite helpers.
- **Dead code**: four black tiger behaviours in stage 3 (lunge, single web spit, ceiling drop, empty)
  that are absent from PC; a tyrant CLUT semi-transparency function; empty Plant 42 functions.

---

## Known bugs in the original code

- **`pad_update`** (0x8003BAF8): `mode2` is read uninitialised on the reset path. The matched C
  reproduces it.
