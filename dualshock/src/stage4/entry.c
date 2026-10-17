/* entry.c (stage4) - stage4_entry, the first code of STAGE4.EXE: installs the stage's enemy update table,
   the player animations its enemies override, and the BGM / voice / background tables, then ends
   its task. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

void stage4_entry(void) {
    s32 stage4__main();  /* PSY-Q __main stub (asm/stage4/stage4_sony.s): empty */
    void player_anim_zombie_grabbed(void);
    void player_anim_crawling(void);
    void player_anim_limb_bounce(void);
    void player_anim_set_attacked_flag(void);
    void player_anim_poison_death(void);
    void player_anim_death_billboard(void);
    void player_anim_plant_hold(void);
    stage4__main();
    g_EnemyUpdateTable = &enemies_update_functions_tbl;
    g_PlayerAnimFunctions[0] = player_anim_zombie_grabbed;
    g_PlayerAnimFunctions[7] = player_anim_crawling;
    g_PlayerAnimFunctions[27] = player_anim_set_attacked_flag;
    g_PlayerAnimFunctions[46] = player_anim_limb_bounce;
    g_PlayerAnimFunctions[30] = player_anim_poison_death;
    g_PlayerAnimFunctions[49] = player_anim_death_billboard;
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
