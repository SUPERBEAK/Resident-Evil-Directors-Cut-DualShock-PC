/* entry.c (stage5) - stage5_entry, the first code of STAGE5.EXE: installs the stage's enemy update table,
   the player animations its enemies override, and the BGM / voice / background tables, then ends
   its task. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

void stage5_entry(void) {
    s32 stage5__main();  /* PSY-Q __main stub (asm/stage5/stage5_sony.s): empty */
    void player_anim_zombie_grabbed(void);
    void player_anim_chimera_knockdown(void);
    void player_anim_enemy_interact(void);
    void player_anim_limb_physics(void);
    stage5__main();
    g_EnemyUpdateTable = &enemies_update_functions_tbl;
    g_PlayerAnimFunctions[0] = player_anim_zombie_grabbed;
    g_PlayerAnimFunctions[28] = player_anim_chimera_knockdown;
    g_PlayerAnimFunctions[31] = player_anim_limb_physics;
    g_PlayerAnimFunctions[50] = player_anim_enemy_interact;
    g_StageRoomBgmTable = &stage_bgm_tbl;
    g_StageVoiceOffsetTable = &stage_voice_offset_tbl;
    if (!(g_StatusFlags & STATUS_ADVANCED)) {
        g_StageBgChecksumTable = &stage_bg_checksum_tbl;
    } else {
        g_StageBgChecksumTable = &stage_bg_checksum_tbl_adv;
    }
    (g_ApiCallPtr = api_task_exit)();
}
