# RE1 Dual Shock (SLUS-00747) - PS1 RAM map (detailed)

Built from the fixed addresses in the decompiled C (`dualshock/config/addrs.txt`), the
symbol files, the overlay PS-X EXE headers and the CD file table (g_CdFileTable 0x80096E54, decoded
against extracted/RE1-Dualshock.xml). "?" = not proven yet.

PS1 main RAM is 2 MB (0x80000000-0x801FFFFF). Many areas are **time-shared**: the same bytes
hold different things depending on what the game is doing (room, door, menu, title, ending).
That is the main trick to remember when reading the code.

## Overview (main program, in-game)

| Start - End | Size | What lives there |
|---|---|---|
| 0x80000000 - 0x8000FFFF | 64 KB | BIOS / kernel work area (not the game's) |
| 0x80010000 - 0x800B27FF | 650 KB | SLUS_007.47 code + data (PsyQ libs 0x80061880-0x8008F7FC) |
| 0x800B2800 - ~0x800CFA24 | ~117 KB | main BSS: globals, effect pool, player + 30 enemy entities, save block (0x800C8460), ordering tables, GPU packet buffers 0/1 (0x800CAB28 / 0x800CD0A8, 0x2580 each), sound / SCD / message state, effect sprite table |
| ~0x800CFA24 - 0x800CFFFF | ~1.5 KB | free? |
| 0x800D0000 - 0x800DCBFF | 51 KB | player EMD model (g_PlayerEmdBuf) |
| 0x800DCC00 - 0x800EA7FF | 55 KB | player joint GsPresetObject packets (g_PlayerObjPacketBuf); item viewer reuses it |
| 0x800EA800 - 0x800F21FF | 30.5 KB | equipped weapon file Wxx.EMW (g_WeaponAnimBuf) |
| 0x800F2200 - 0x800F3BFF | 6.5 KB | weapon object packets (g_WeaponObjPacketBuf) |
| 0x800F3C00 - 0x800F53FF | 6 KB | CORE00.ESP global effect sprites (g_CoreEspBuf) |
| 0x800F5400 - 0x800F83FF | 3 x 4 KB | sound bank .HED files: bank 1 (weapon), bank 0 (doors), bank 3 (character) |
| 0x800F8400 - 0x800FFFFF | 31 KB | BGM VAB header + SEP data (g_BgmHeadBuf) |
| 0x80100000 - 0x801053FF | 21 KB | ? (no fixed use found in main) |
| 0x80105400 - ~0x80134400 | up to 188 KB | stage overlay STAGEn.EXE (enemy AI; largest STAGE4 0x2F000) |
| after the overlay - 0x801C10EB | ~560 KB | g_LoadPointer heap: room file (RDT: camera, collision, SCD, enemy models...), bank-3 .VB staging, in-game FMV buffers (g_FmvBuffer = g_LoadPointer) |
| 0x801C10EC - 0x801D08EB | 64 KB | bank 0 (door sounds) .VB staging (g_SndBank0VbBuf) |
| 0x801D08EC - 0x801D10EB | 2 KB | overlay EXE header sector (g_ExeHeaderBuf) |
| 0x801D10EC - 0x801F68FF | 150 KB | **big shared work area** (g_DoorFile), see below |
| 0x801F6900 - 0x801FE8FF | 32 KB | room background MDEC bitstream (g_RoomBgBsBuf); VRAM save while the status menu is open |
| 0x801FE900 - 0x801FEBDB | 732 B | overlay API table, 183 function pointers (g_OverlayApiTable) |
| 0x801FEC00 - 0x801FEC23 | 36 B | globals shared with overlays (g_StageRoomBgmTable, g_ApiCallPtr, g_EnemyUpdateTable, ...) |
| 0x801FEC24 - 0x801FEDA3 | 384 B | 3 task (thread) slots, 0x80 each (g_Tasks) |
| 0x801FEDA4 - 0x801FFA7F | 3.3 KB | task stacks: slot 2 (0x2DC), slot 1 (0x300), slot 0 (0x700) |
| 0x801FFA80 - 0x801FFFF0 | 1.4 KB | main thread stack (EXE header s_addr 0x801FFFF0) |

Scratchpad (1 KB data cache at 0x1F800000) is used for temporary RECTs, sprites and matrices.

### The big shared work area 0x801D10EC - 0x801F68FF (0x25814 bytes = 20-byte TIM header + 320x240x16bpp)

| Use | Layout |
|---|---|
| Room background | MDEC: bitstream at 0x801F6900 -> run-level data at +0x2000 (0x801D30EC) -> 16-px RGB slices at +0, then into VRAM |
| Full-screen pictures | raw .PIX (TITLE.PIX, SEL_BACK.PIX) at +0 (g_BgImageMode 0); 16bpp TIM (GWARNIN1.TIM) at +0 -> pixels at +0x14 (mode 1); TYPE00.TIM loaded 0x14 *before* so its pixels land on +0 |
| Door animation | door file at +0 (up to 0xC00C), door model packets +0xC00C (0x9C40), door GPU packet buffers 0x801E6D38 / 0x801EEB1C (0x7DE4 each, the second ends exactly at 0x801F6900) |
| Status menu | item viewer model .IVM (8bpp 256x256 TIM 0x10220 + TMD at +0x10220); file pages / maps at +0x10000 (g_MenuTexBuf) |
| Title / select | title BG, select screen TIMs |

## Small overlays (logo, title, select, prologue, ending)

They load at **0x800E0000** (not 0x80105400), on top of the player/weapon area (the player model is
not needed on those screens). Sizes: LOGO 0x800, PROLOGUE 0x800, TITLE 0x2000, SELECT 0x5000,
ENDING 0x6000 (+BSS ~0x800E66D6). Stage overlays STAGE1-7 load at 0x80105400.
They all reset g_LoadPointer to 0x80105400 and use the space above as their heap:

| Program | 0x80105400 and up |
|---|---|
| logo | STR movie: slice 0x80100000 (0x2D00), CD ring 0x80102D00 (24 sectors), VLC 0x8010ED00 / 0x8011ED00 (64 KB each) |
| title | bank-1 .VB staging (EVIL.VB), title FMV buffer; TMOJI.TIM (6 small TIMs) at 0x80155400; memcard work buffer 0x80180000; title picture at 0x801D10EC |
| select | bank-1 .VB staging (SELECT.VB); SELECT.TIM / SEL_BACK.PIX at 0x801D10EC |
| ending | ENDING.VB staging, then carved: work 0x80115400 (256 KB), small 0x80155400 (4 KB), player picture 0x80156400 (150 KB), load base 0x8017BC00 (64 KB), STR 0x8018BC00 (192 KB), model 0x801BBC00 (192 KB), packets 0x801EBC00.. |
