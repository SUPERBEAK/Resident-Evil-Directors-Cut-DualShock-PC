/* stage_load.c - stage start: music setup, loading the stage overlay and its enemy code */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void spiderweb_update(void);

void load_stage_overlay(s32 slot, s32 file);

void task_sleep(s16 frames);

void room_init(void);

/* Defined earlier in the same file before it was split (tools/splitfile.py): what these
   functions saw when they were one file. */
void bgm_fade_out_all(void);

void stage_init(void) {
    s32 room_init_a2() __asm__("room_init");
    s32 bgm_fade_out_all_a2() __asm__("bgm_fade_out_all");
    void stage_overlay_load(void);
    extern u8 g_RoomId_u8 __asm__("g_RoomId");
    int LoadImage(RECT *rect, u_long *p);
    s16 vram_rect[4];

    g_PrevRoomId = 0x1F;
    LIT(s32, g_RoomBgmConfig) = (g_StageId << 5) + ADDR_g_RoomBgmState;
    if (g_SepAccessNum >= 0) {
        bgm_fade_out_all_a2();
    }
    g_BgmState = 0xFF;
    LIT(u32 *, g_LoadPointer) = LITP(u32, g_OverlayArea);
    stage_overlay_load();
    g_RoomLoadBase = LIT(u32 *, g_LoadPointer);
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        *(u8 *)(LIT(s32, g_RoomBgmConfig) + g_RoomId_u8) = 0xFF;
    }
    vram_rect[0] = 0x3C0;
    vram_rect[1] = 0x1C4;
    vram_rect[2] = 0x40;
    vram_rect[3] = 0x38;
    LoadImage((RECT *) vram_rect, LIT(u_long *, g_LoadPointer));
    room_init_a2();
}

void stage_overlay_load(void) {
    s32 load_stage_overlay_c1(s32, u16) __asm__("load_stage_overlay");
    s32 task_sleep_a10(s32) __asm__("task_sleep");
    extern void (**g_EnemyUpdateTable)();  /* the stage's update function per entity type id */
    void spiderweb_update();
    void character_npc_update();
    load_stage_overlay_c1(2, ((s16 *)&g_StageOverlayFiles)[g_StageId]);
    g_LoadPointer = g_Tasks[2].ovl_text_size + ADDR_g_OverlayArea;
    task_sleep_a10(1);
    g_EnemyUpdateTable[0x13] = spiderweb_update;
    g_EnemyUpdateTable[0x17] = character_npc_update;
    g_EnemyUpdateTable[0x18] = character_npc_update;
    g_EnemyUpdateTable[0x19] = character_npc_update;
    g_EnemyUpdateTable[0x1A] = character_npc_update;
    g_EnemyUpdateTable[0x1B] = character_npc_update;
    g_EnemyUpdateTable[0x1C] = character_npc_update;
    g_EnemyUpdateTable[0x1D] = character_npc_update;
    g_EnemyUpdateTable[0x1E] = character_npc_update;
    g_EnemyUpdateTable[0x1F] = character_npc_update;
    g_EnemyUpdateTable[0x20] = character_npc_update;
    g_EnemyUpdateTable[0x21] = character_npc_update;
    g_EnemyUpdateTable[0x22] = character_npc_update;
    g_EnemyUpdateTable[0x23] = character_npc_update;
    g_EnemyUpdateTable[0x24] = character_npc_update;
    g_EnemyUpdateTable[0x25] = character_npc_update;
    g_EnemyUpdateTable[0x26] = character_npc_update;
    g_EnemyUpdateTable[0x27] = character_npc_update;
    g_EnemyUpdateTable[0x28] = character_npc_update;
    g_EnemyUpdateTable[0x29] = character_npc_update;
    g_EnemyUpdateTable[0x2A] = character_npc_update;
    g_EnemyUpdateTable[0x2B] = character_npc_update;
    g_EnemyUpdateTable[0x2C] = character_npc_update;
    g_EnemyUpdateTable[0x2D] = character_npc_update;
    g_EnemyUpdateTable[0x2E] = character_npc_update;
    g_EnemyUpdateTable[0x2F] = character_npc_update;
}
