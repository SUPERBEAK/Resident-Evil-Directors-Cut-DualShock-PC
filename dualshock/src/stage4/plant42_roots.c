/* plant42_roots.c (stage4) - the roots entity of the Plant 42 room: rise, shake and sink phases
   with CLUT fades. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Plant 42 roots data */
extern s32 plant42_roots_sca_info;

/* Plant42RootsWork: the plant42_roots's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). PLANT42_ROOTS(e)->f, PLANT42_ROOTS_FIELD(e, f) and PLANT42_ROOTS_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct Plant42RootsWork {
    /* +0x00 */ s16 shake_velocity;  /* 0x16C: pos_y += it >> 8 each frame, then flips to -/+ shake_amplitude (plant42_roots_move_a); cleared in plant42_roots_phase_start */
    /* +0x02 */ u16 shake_amplitude; /* 0x16E: 0 at start, +0x200/frame up to 0x1600 (plant42_roots_phase_rise), -0x70/frame in plant42_roots_phase_sink until <= 0 */
    /* +0x04 */ s16 fade_count;      /* 0x170: CLUT fade steps left (6 at start, 1 when shrinking ends); each step calls api_clut_fade_add_delta and decrements */
    /* +0x06 */ s16 unused_172;      /* 0x172: only cleared in plant42_roots_init */
    /* +0x08 */ s32 home_x;          /* 0x174: pos_x saved in plant42_roots_init; plant42_roots_update forces pos_x back to it when behavior_flags != 0 */
    /* +0x0C */ s32 home_z;          /* 0x178: pos_z saved in plant42_roots_init; restored like home_x */
    /* +0x10 */ s16 groan_timer;     /* 0x17C: 0x28 at init; counts down in plant42_roots_move_a, at 0 plays 3D SE 0x19 and reloads 12..26 */
    /* +0x12 */ u16 sink_tick;       /* 0x17E: 2 at init, +1 every 4th frame in plant42_roots_phase_sink; never read in this file */
} Plant42RootsWork;
#define PLANT42_ROOTS(e) ENEMY_WORK(e, Plant42RootsWork)
#define PLANT42_ROOTS_FIELD(e, f) ENEMY_FIELD(e, Plant42RootsWork, f)
#define PLANT42_ROOTS_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, Plant42RootsWork, f, T)
#define PLANT42_ROOTS_MEMBER(e, f) ENEMY_MEMBER(e, Plant42RootsWork, f)

/* every frame for each plant42 roots (enemies_update_functions_tbl): runs plant42_roots_state_tbl[state], then collision checks and
   the shadow / drawing */
void plant42_roots_update(void) {
    extern s32 (*plant42_roots_state_tbl[])();
    if (g_UpdateFlags & 4 & 0xFFFF) {
        plant42_roots_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
    }
    if (g_CurrentEntity->behavior_flags != 0) {
        g_CurrentEntity->pos_x = (s32) PLANT42_ROOTS_FIELD(g_CurrentEntity, home_x);
        g_CurrentEntity->pos_z = (s32) PLANT42_ROOTS_FIELD(g_CurrentEntity, home_z);
    }
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0, ((s16) g_CurrentEntity->angle));
    }
}

/* state 0: sets up a new plant42 roots (health, sca info, animation, work fields), then state 1 */
void plant42_roots_init(void) {

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->health = 1;
    g_CurrentEntity->sca_info = (s32)&plant42_roots_sca_info;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 4);
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    g_CurrentEntity->scale = 0x1000;
    g_CurrentEntity->hit_state = 1;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, home_x) = (s32) g_CurrentEntity->pos_x;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, home_z) = (s32) g_CurrentEntity->pos_z;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, unused_172) = 0;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, sink_tick) = 2;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, groan_timer) = 0x28;
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x404040;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0xC00, 0xC00);
}

void plant42_roots_idle(void) {
    void plant42_roots_move_a();
    void plant42_roots_move_b();
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 0xE0);
    if (g_CurrentEntity->behavior_flags != 0x80) {
        if (g_CurrentEntity->behavior_flags == 0) {
            plant42_roots_move_a();
        } else {
            plant42_roots_move_b();
        }
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
}

void plant42_roots_move_a(void) {
    extern s32 (*plant42_roots_phase_tbl[])();
    s32 unused[10];

    if ((u8) ((u8) g_CurrentEntity->sub_state) >= 4U) {
        return;
    }
    plant42_roots_phase_tbl[((u8) g_CurrentEntity->sub_state)]();
    g_CurrentEntity->pos_y += (s16) (PLANT42_ROOTS(g_CurrentEntity)->shake_velocity >> 8);
    PLANT42_ROOTS(g_CurrentEntity)->shake_velocity = (PLANT42_ROOTS(g_CurrentEntity)->shake_velocity > 0) ? -PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) : PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude);
    if (--PLANT42_ROOTS_FIELD(g_CurrentEntity, groan_timer) == 0) {
        (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x19U, 0, &g_CurrentEntity->pos_x);
        PLANT42_ROOTS_FIELD_AS(g_CurrentEntity, groan_timer, u16) = ((g_ApiCallPtr = api_rand)() & 7) + 0xC + ((g_ApiCallPtr = api_rand)() & 7);
    }
    if (((g_ApiCallPtr = api_rand)() & 0x1F) == 1) {
        (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x19U, 0, &g_CurrentEntity->pos_x);
    }
}

void plant42_roots_phase_start(void) {
    g_CurrentEntity->sub_state++;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_velocity) = 0;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) = 0;
    g_CurrentEntity->action_ticks_counter = 8;
    PLANT42_ROOTS_FIELD(g_CurrentEntity, fade_count) = 6;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    g_CurrentEntity->health = -1;
    (g_ApiCallPtr = api_sound_play_entity_se)(0x18, 0);
}

void plant42_roots_phase_rise(void) {
    PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) = (u16) (PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) + 0x200);
    if ((s16) PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) >= 0x1600) {
        PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) = 0x1600U;
        g_CurrentEntity->sub_state++;
    }
}

void plant42_roots_phase_shrink(void) {

    if ((u16) g_CurrentEntity->scale < 0x9C5U) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        g_CurrentEntity->action_ticks_counter = 1U;
        PLANT42_ROOTS_FIELD(g_CurrentEntity, fade_count) = 1;
        return;
    }
    (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0xA, -0xA);
    g_CurrentEntity->scale = (u16) (g_CurrentEntity->scale + 0xFFF8);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) {
        return;
    }
    if (PLANT42_ROOTS_FIELD(g_CurrentEntity, fade_count) != 0) {
        (g_ApiCallPtr = api_clut_fade_add_delta)(NULL, -1, -2, 0, 0x100, (s32) g_CurrentEntity->id);
        g_CurrentEntity->action_ticks_counter = 8U;
        PLANT42_ROOTS_FIELD(g_CurrentEntity, fade_count)--;
    }
}

void plant42_roots_phase_sink(void) {

    PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) = (u16) (((s16) PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude)) - 0x70);
    if ((s16) (++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 3) == 0) {
        PLANT42_ROOTS_FIELD(g_CurrentEntity, sink_tick) = (u16) (PLANT42_ROOTS_FIELD(g_CurrentEntity, sink_tick) + 1);
    }
    if ((s16) PLANT42_ROOTS_FIELD(g_CurrentEntity, shake_amplitude) <= 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
        return;
    }
}

void plant42_roots_move_b(void) {
    if (((u8) g_CurrentEntity->sub_state) != 0) {
        return;
    }
    g_CurrentEntity->sub_state++;
    g_CurrentEntity->scale = 0x9C4;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    g_CurrentEntity->health = -1;
    (g_ApiCallPtr = api_clut_fade_set)(NULL, -6, -0xC, 0, 0x100, (s32) g_CurrentEntity->id);
    (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x7D0, -0x7D0);
}

void plant42_roots_hit_reset(void) {
    g_CurrentEntity->state = 1;
    g_CurrentEntity->sub_state = 0;
    g_CurrentEntity->hit_state = 0;
}

void plant42_roots_state3_nop(void) {

}

void plant42_roots_state4_nop(void) {

    return;
}

void plant42_roots_state5_nop(void) {

}
