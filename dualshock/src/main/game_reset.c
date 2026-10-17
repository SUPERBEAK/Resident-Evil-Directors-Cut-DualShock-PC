/* game_reset.c - Clearing the game state for a new game / room: state flags, sound and fade state, the
   game work area, room state; mem_clear_words. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 is_point_in_quad(s32 *pos, u16 *quad);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
s16 get_angle_towards_target(s16 x, s16 z);
void entity_extract_anim_vertex(u8 *ent, u8 *src, u8 *anim_tbl, u8 reverse);
s32 check_entity_collision(u8 *a, u8 *b);
void pad_vibrate_small(s32 duration, s32 delay);
void pad_vibrate_large(s32 duration, s32 vol, s32 delay);
void billboard_init(SVECTOR *pos, u8 *bb, s16 half_w, s16 half_d);
void billboard_draw(VECTOR *pos, u8 *obj, s16 y, s16 ang);
u32 boundary_get_cell(void *pos, void *ofs, s32 split_x, s32 split_z);
s32 check_room_collision(VECTOR *pos, u16 radius);
s32 room_check_sight_blocked(VECTOR *dir, s32 group);
void sound_play_entity_se(u8 se);


void clear_game_state_flags(void) {
    s32 *p;

    p = LITP(s32, g_GameStateFlags);
    do {
        *p = 0;
        p += 1;
    } while ((u32) p <= ADDR_g_GameStateFlagsBlockEnd);
    g_MessageStatus = 0;
}

void init_sound_and_fade_state(void) {
    g_VSyncMode = 2;
    g_SndFadeActive = 0;
    g_FadeLevel = -1;
    g_Fade2Level = -1;
}

void clear_game_work_area(void) {
    s32 *p;

    p = LITP(s32, g_DefaultItemSlot);
    do {
        *p = 0;
        p += 1;
    } while ((u32) p <= ADDR_g_Fade2MaskB);
}

void room_state_reset(void) {
    g_PickedItemId = 0;
    g_UsedItemId = 0;
    g_SysFlags[1] = 0;
    g_FwdPosActionId = 0;
}

void mem_clear_words(s32 *p, s32 *end) {
    do {
        *p++ = 0;
    } while (p < end);
}
