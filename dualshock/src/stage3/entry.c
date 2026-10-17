/* entry.c (stage3) - stage3_entry, the first code of STAGE3.EXE: installs the stage's enemy update table,
   the player animations its enemies override, and the BGM / voice / background tables, then ends
   its task. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

void stage3_entry(void) {
    s32 stage3__main();  /* PSY-Q __main stub (asm/stage3/stage3_sony.s): empty */
    void player_anim_simple_recovery(void);
    void player_anim_hunter_decapitation(void);
    void player_anim_enemy_interact(void);
    void player_anim_limb_physics(void);
    stage3__main();
    g_EnemyUpdateTable = &enemies_update_functions_tbl;
    g_PlayerAnimFunctions[40] = player_anim_simple_recovery;
    g_PlayerAnimFunctions[44] = player_anim_hunter_decapitation;
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
