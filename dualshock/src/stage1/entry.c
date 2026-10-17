/* entry.c (stage1) - stage1_entry, the first code of STAGE1.EXE: installs the stage's enemy update table,
   the player animations its enemies override, and the BGM / voice / background tables, then ends
   its task. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

void stage1_entry(void) {
    s32 stage1__main();  /* PSY-Q __main stub (asm/stage1/stage1_sony.s): empty */
    void player_anim_zombie_grabbed(void);
    void player_anim_simple_recovery(void);
    void player_anim_multi_attack(void);
    void player_anim_plant_hold(void);
    stage1__main();
    g_EnemyUpdateTable = &enemies_update_functions_tbl;
    g_PlayerAnimFunctions[0] = player_anim_zombie_grabbed;
    g_PlayerAnimFunctions[40] = player_anim_simple_recovery;
    g_PlayerAnimFunctions[24] = player_anim_multi_attack;
    g_PlayerAnimFunctions[34] = player_anim_plant_hold;
    g_StageRoomBgmTable = &stage_bgm_tbl;
    g_StageVoiceOffsetTable = &stage_voice_offset_tbl;
    if (!(g_StatusFlags & STATUS_ADVANCED)) {
        g_StageBgChecksumTable = &stage_bg_checksum_tbl;
    } else {
        g_StageBgChecksumTable = &stage_bg_checksum_tbl_adv;
    }
    (g_ApiCallPtr = api_task_exit)();
}
