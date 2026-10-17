/* effect.c - 2D effects in g_EffectPool (sprites drawn as billboards): the behaviour functions (effect_behavior_*,
   run through g_EffectBehaviorTable), effect creation (effect_create_billboard) and drawing, mirror copies. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 effect_projectile_hit_check(s32 radius, s16 x, s16 z);
u8 apply_weapon_damage(s32 weapon);
s32 is_point_in_quad(s32 *pos, u16 *quad);


void effect_behavior_unused(void) {
}

void effect_behavior_idle(void) {
}

void effect_behavior_hold_frame(void) {
    STATE_BLOCK[g_ActiveEffectIndex].frame_delay = 0;
}

void effect_behavior_count_phase(void) {
    s32 effect_behavior_next_phase_a1() __asm__("effect_behavior_next_phase");
    s32 off;
    u8 timer;

    off = EFFECT_CUR_OFF;
    timer = EFFECT_AT(off, hdr06, s8);
    EFFECT_AT(off, hdr06, s8) = timer + 0xFF;
    if (timer == 0) {
        effect_behavior_next_phase_a1();
    }
}

void effect_behavior_blink(void) {
    s32 off;

    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, draw_flags, s16) = EFFECT_AT(off, draw_flags, u16) ^ 0x8000;
}

void effect_behavior_phase_drop(void) {
    s32 effect_behavior_next_phase_a2() __asm__("effect_behavior_next_phase");
    s32 off;

    effect_behavior_next_phase_a2();
    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, vel_y, u16) = EFFECT_AT(off, vel_y, u16) - 0x18;
}

void effect_behavior_next_frame(void) {
    s32 off;

    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, frame_cur, s32) = EFFECT_AT(off, frame_cur, s32) + 4;
    EFFECT_AT(EFFECT_CUR_OFF, frame_delay, s8) = 0;
}

/* the 0x18-byte head of an effect (behaviour, frame, ...) as stored per phase in its animation data: copied
   as raw bytes (unaligned word copies, as compiled) */
typedef BYTES(0x18) EffectPhaseBytes;

void effect_behavior_next_phase(void) {
    void effect_behavior_kill(void);
    u16 save16;
    u8 save3;

    EFFECT->anim_data_frame += 0x18;
    save16 = ((u16) EFFECT->yaw);
    save3 = EFFECT->scale;
    *(EffectPhaseBytes *)EFFECT = *((EffectPhaseBytes *) EFFECT->anim_data_frame);
    if (EFFECT->pre_behavior == 0) {
        effect_behavior_kill();
    }
    EFFECT->yaw = save16;
    EFFECT->scale = save3;
}

void effect_behavior_rand_frame(void) {
    int rand(void);

    EFFECT->frame_cur = ((rand() % 3) * 4) + EFFECT->frame_cur;
    EFFECT->frame_delay = EFF_FRAME(EFFECT->frame_cur)->delay;
    EFFECT->uv_cur = (*((u8 *) EFFECT->frame_cur) * 4) + EFFECT->uv_list;
}

void effect_behavior_set_frame(void) {
    s32 off;
    s32 off2;
    s32 off3;
    s32 off4;

    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, frame_cur, s32) = EFFECT_AT(off, frame_list, s32);
    off2 = EFFECT_CUR_OFF;
    EFFECT_AT(off2, frame_cur, s32) = (EFFECT_AT(off2, hdr04, u8) * 4) + EFFECT_AT(off2, frame_cur, s32);
    off3 = EFFECT_CUR_OFF;
    EFFECT_AT(off3, frame_delay, s8) = EFF_FRAME(EFFECT_AT(off3, frame_cur, s32))->delay;
    off4 = EFFECT_CUR_OFF;
    EFFECT_AT(off4, uv_cur, s32) = (*EFFECT_AT(off4, frame_cur, u8 *) * 4) + EFFECT_AT(off4, uv_list, s32);
}

void effect_behavior_spawner(void) {
    s32 effect_behavior_next_phase_a3() __asm__("effect_behavior_next_phase");
    s32 effect_create_billboard_a7() __asm__("effect_create_billboard");
    s32 i84;
    u8 n;

    i84 = EFFECT_CUR_OFF;
    n = EFFECT_AT(i84, hdr06, u8);
    EFFECT_AT(i84, hdr06, u8) = n + 0xFF;
    if (n == 0) {
        LIT(s32, sp_vec_vx) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_x, s16);
        LIT(s32, sp_vec_vy) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_y, s16);
        LIT(s32, sp_vec_vz) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_z, s16);
        effect_create_billboard_a7(EFFECT_AT(EFFECT_CUR_OFF, effect_type, u8),
                      EFFECT_AT(EFFECT_CUR_OFF, depth_group, u8),
                      EFFECT_AT(EFFECT_CUR_OFF, yaw, s16),
                      EFFECT_AT(EFFECT_CUR_OFF, parent_matrix, s32), ADDR_sp_vec, 0);
        effect_behavior_next_phase_a3();
    }
}

/* An Effect slot as effect_behavior_clone / effect_create_billboard's original code saw it: the first 0x18
   bytes are one animation step (EffectStep: the slot's header is (re)loaded from the step list at
   anim_data_frame), the matrix as nine shorts, typed pointers. Same bytes as Effect (re1_types.h). */
typedef struct {
    u8 pre_behavior, post_behavior, type, scale, hdr04, hdr05, hdr06, hdr07;
    u16 accel_x, accel_y, accel_z, draw_flags;
    u16 vel_x, vel_y, vel_z, yaw;
} EffectStep;
typedef struct {
    /* 0x00 */ EffectStep h;
    /* 0x18 */ s16 disp_x, disp_y, disp_z;
    /* 0x1E */ u8 frame_delay, frame_index;
    /* 0x20 */ s16 pos_x, pos_y, pos_z;
    /* 0x26 */ u8 effect_type, depth_group;
    /* 0x28 */ s16 local_offset_x, local_offset_y, local_offset_z, depth_scaled;
    /* 0x30 */ s16 m00, m01, m02, m10, m11, m12, m20, m21, m22, pad42;
    /* 0x44 */ s32 trans_x, trans_y, trans_z, proj_depth;
    /* 0x54 */ VECTOR spawn;
    /* 0x64 */ void *parent_matrix;
    /* 0x68 */ u8 *clut_info;
    /* 0x6C */ u8 *frame_list, *frame_cur, *uv_list, *uv_cur;
    /* 0x7C */ EffectStep *anim_data_base, *anim_data_frame;
} EffectSlot;

void effect_behavior_clone(void) {
    u8 c;
    u8 cnt;
    u8 i;

    c = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].h.hdr04--;
    if (c == 0) {
        if (LIT(u8, g_FreeEffectSlots) == 0) {
            LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].h.pre_behavior = 1;
            return;
        }
        cnt = LIT(u8, g_FreeEffectSlots);
        i = 0x40;
        do {
            i--;
            if (LITP(EffectSlot, g_EffectPool)[i].h.pre_behavior == 0) {
                LIT(u8, g_FreeEffectSlots)--;
            }
        } while (LIT(u8, g_FreeEffectSlots) == cnt);
        LITP(EffectSlot, g_EffectPool)[i].disp_x = LITP(EffectSlot, g_EffectPool)[i].disp_y = LITP(EffectSlot, g_EffectPool)[i].disp_z = 0;
        LITP(EffectSlot, g_EffectPool)[i].pos_x = LITP(EffectSlot, g_EffectPool)[i].pos_y = LITP(EffectSlot, g_EffectPool)[i].pos_z = 0;
        LITP(EffectSlot, g_EffectPool)[i].m00 = LITP(EffectSlot, g_EffectPool)[i].m01 = LITP(EffectSlot, g_EffectPool)[i].m02 = 0;
        LITP(EffectSlot, g_EffectPool)[i].m10 = LITP(EffectSlot, g_EffectPool)[i].m11 = LITP(EffectSlot, g_EffectPool)[i].m12 = 0;
        LITP(EffectSlot, g_EffectPool)[i].m20 = LITP(EffectSlot, g_EffectPool)[i].m21 = LITP(EffectSlot, g_EffectPool)[i].m22 = 0;
        LITP(EffectSlot, g_EffectPool)[i].trans_x = LITP(EffectSlot, g_EffectPool)[i].trans_y = LITP(EffectSlot, g_EffectPool)[i].trans_z = 0;
        LITP(EffectSlot, g_EffectPool)[i].proj_depth = 0;
        LITP(EffectSlot, g_EffectPool)[i].depth_scaled = 0;
        LITP(EffectSlot, g_EffectPool)[i].effect_type = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].effect_type;
        LITP(EffectSlot, g_EffectPool)[i].depth_group = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].depth_group;
        LITP(EffectSlot, g_EffectPool)[i].parent_matrix = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].parent_matrix;
        LITP(EffectSlot, g_EffectPool)[i].spawn = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].spawn;
        LITP(EffectSlot, g_EffectPool)[i].clut_info = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].clut_info;
        LITP(EffectSlot, g_EffectPool)[i].frame_list = LITP(EffectSlot, g_EffectPool)[i].frame_cur = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].frame_list;
        LITP(EffectSlot, g_EffectPool)[i].uv_list = LITP(EffectSlot, g_EffectPool)[i].uv_cur = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].uv_list;
        LITP(EffectSlot, g_EffectPool)[i].anim_data_base = LITP(EffectSlot, g_EffectPool)[i].anim_data_frame = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].anim_data_base;
        LITP(EffectSlot, g_EffectPool)[i].local_offset_x = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].spawn.vx;
        LITP(EffectSlot, g_EffectPool)[i].local_offset_y = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].spawn.vy;
        LITP(EffectSlot, g_EffectPool)[i].local_offset_z = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].spawn.vz;
        LITP(EffectSlot, g_EffectPool)[i].anim_data_base = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].anim_data_base;
        LITP(EffectSlot, g_EffectPool)[i].anim_data_frame = LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].anim_data_frame + 1;
        LITP(EffectSlot, g_EffectPool)[i].h = *LITP(EffectSlot, g_EffectPool)[i].anim_data_frame;
        LITP(EffectSlot, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].h.pre_behavior = 1;
    }
}

void effect_behavior_spawn_from_header(void) {
    s32 effect_create_billboard_a8(u8, u8, s16, s32, s32, s32) __asm__("effect_create_billboard");
    s32 off;

    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    off = EFFECT_CUR_OFF;
    LIT(s32, sp_tmp00) = effect_create_billboard_a8(EFFECT_AT(off, hdr04, u8), EFFECT_AT(off, hdr05, u8), EFFECT_AT(off, yaw, s16), 0, ADDR_sp_vec, 0);
}

void effect_behavior_spawn_advance(void) {
    s32 effect_behavior_next_phase_a4() __asm__("effect_behavior_next_phase");
    s32 effect_create_billboard_a9(u8, u8, s16, s32, s32, s32) __asm__("effect_create_billboard");
    s32 off;

    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    off = EFFECT_CUR_OFF;
    effect_create_billboard_a9(EFFECT_AT(off, hdr04, u8), EFFECT_AT(off, hdr05, u8), EFFECT_AT(off, yaw, s16), 0, ADDR_sp_vec, 0);
    effect_behavior_next_phase_a4();
}

typedef struct {
    u8 pad0[7];
    u8 hdr07;
    u8 pad8[0x28];
    MATRIX transform;
    u8 pad50[0x14];
    MATRIX *parent_matrix;
    u8 pad68[0x1C];
} EffectTimerView;

void effect_behavior_timer_refresh(void) {
    u8 *idx;
    s32 v;

    idx = LITP(u8, g_ActiveEffectIndex);
    v = LITP(EffectTimerView, g_EffectPool)[*idx].hdr07;
    if (v == 0) {
        effect_behavior_next_phase();
        LITP(EffectTimerView, g_EffectPool)[*idx].transform = *LITP(EffectTimerView, g_EffectPool)[*idx].parent_matrix;
    } else {
        LITP(EffectTimerView, g_EffectPool)[*idx].hdr07 = v - 1;
    }
}

void effect_behavior_rand_phase(void) {
    u16 save16;
    u8 save7;
    u8 save3;

    effect_behavior_set_frame();
    EFFECT->anim_data_frame += 0x18;
    save16 = ((u16) EFFECT->yaw);
    save7 = EFFECT->hdr07;
    save3 = EFFECT->scale;
    if (g_RandSeed & 1) {
        EFFECT->anim_data_frame += 0x18;
        *(EffectStep *)EFFECT = *(EffectStep *)EFFECT->anim_data_frame;
    } else {
        *(EffectStep *)EFFECT = *(EffectStep *)EFFECT->anim_data_frame;
        EFFECT->anim_data_frame += 0x18;
    }
    EFFECT->yaw = save16 + ((u16) EFFECT->yaw);
    EFFECT->hdr07 = save7;
    EFFECT->scale = save3;
    EFFECT->transform = *(MATRIX *)EFFECT->parent_matrix;
}

void effect_behavior_init_scale(void) {
    u8 t[10] = { 7, 13, 8, 8, 15, 15, 15, 17, 17, 17 }; /* Effect.scale per effect hdr04 */

    EFFECT->post_behavior = 1;
    EFFECT->type = 2;
    EFFECT->transform = **(MATRIX **)&EFFECT->parent_matrix;
    EFFECT->scale = t[EFFECT->hdr04];
}

void effect_behavior_timer_flags(void) {
    s32 effect_behavior_timer_refresh_a1() __asm__("effect_behavior_timer_refresh");
    effect_behavior_timer_refresh_a1();
    EFFECT_AT(g_ActiveEffectIndex * EFFECT_SIZE, hdr06, s8) = g_PlayerEntity.status_flags;
}

void effect_behavior_ember(void) {
    s32 effect_behavior_next_phase_a5() __asm__("effect_behavior_next_phase");
    s32 off;
    s32 off2;
    u8 timer;

    off = EFFECT_CUR_OFF;
    timer = EFFECT_AT(off, hdr07, s8);
    EFFECT_AT(off, hdr07, s8) = timer + 0xFF;
    if (timer == 0) {
        effect_behavior_next_phase_a5();
        EFFECT_AT(EFFECT_CUR_OFF, scale, s8) = 0x1E;
        off2 = EFFECT_CUR_OFF;
        EFFECT_AT(off2, local_offset_x, s16) = EFFECT_AT(off2, local_offset_x, u16) - 0xFA;
    }
}

void effect_behavior_timer_phase(void) {
    s32 effect_behavior_next_phase_a6() __asm__("effect_behavior_next_phase");
    s32 off;
    u8 timer;

    off = EFFECT_CUR_OFF;
    timer = EFFECT_AT(off, hdr07, s8);
    EFFECT_AT(off, hdr07, s8) = timer + 0xFF;
    if (timer == 0) {
        effect_behavior_next_phase_a6();
    }
}

void effect_behavior_gravity_settle(void) {
    s32 off;
    u8 *pool;

    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    pool = LITP(u8, g_EffectPool);
    LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) != 0) {
        off = EFFECT_CUR_OFF;
        EFFECT_AT(off, vel_z, s16) = 0;
        EFFECT_AT(off, vel_y, s16) = 0;
        EFFECT_AT(off, vel_x, s16) = 0;
        pool[LIT(u8, g_ActiveEffectIndex) * EFFECT_SIZE] = 1;   /* pre_behavior = 1 */
    }
}

void effect_behavior_gravity_impact(void) {
    s32 wt;

    LIT(s32, sp_vec_vx) = (s32) EFFECT->pos_x;
    SCRATCH->vec_y = (s32) EFFECT->pos_y;
    SCRATCH->vec_z = (s32) EFFECT->pos_z;
    SCRATCH->sv_x = 0;
    SCRATCH->sv_y = 0;
    SCRATCH->sv_z = 0;
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) != 0) {
        EFFECT->pre_behavior = 1;
        EFFECT->hdr04 = 2;
        EFFECT->hdr06 = 1;
        wt = -((s16) EFFECT->vel_x);
        EFFECT->vel_x = wt;
        if ((g_PlayerEntity.behavior_flags - 2) == EFFECT->hdr07) {
            LIT(s32, sp_vec_vx) = (s32) EFFECT->pos_x;
            SCRATCH->vec_y = (s32) EFFECT->pos_y;
            SCRATCH->vec_z = (s32) EFFECT->pos_z;
            sound_play_se_3d(1, 0xA, 0, ADDR_sp_vec);
        }
    }
}

void effect_behavior_bounce(void) {
    if (EFFECT->pos_y + EFFECT->vel_y >= 0) {
        EFFECT->pre_behavior = 1;
        EFFECT->post_behavior = 1;
        EFFECT->draw_flags |= 0x4007;
        EFFECT->accel_x = EFFECT->accel_y = EFFECT->accel_z = 0;
        EFFECT->vel_x = EFFECT->vel_y = EFFECT->vel_z = 0;
        EFFECT->disp_y = -({ EFFECT->local_offset_y + EFFECT->transform.t[1]; });
    }
}

void effect_behavior_gravity_rise(void) {
    EFFECT->draw_flags = 0x4003;
    if (g_PlayerFloorY < EFFECT->pos_y + EFFECT->vel_y) {
        EFFECT->draw_flags |= 0x400B;
        EFFECT->pre_behavior = 0x2F;
        EFFECT->post_behavior = 0;
        EFFECT->disp_y = g_PlayerFloorY - ({ EFFECT->local_offset_y + EFFECT->transform.t[1]; });
    }
}

/* The current Effect slot as effect_behavior_shell_casing's original code typed it (signed velocities, a byte
   pointer for the animation frame): CASING.f */
typedef struct {
    u8 pre_behavior, post_behavior, type, scale, hdr04, hdr05, hdr06, hdr07;
    u16 accel_x, accel_y, accel_z, draw_flags;
    s16 vel_x, vel_y, vel_z, yaw, disp_x, disp_y, disp_z, frame_delay;
    s16 pos_x, pos_y, pos_z, effect_type, local_offset_x, local_offset_y;
    u8 pad2C[0x1C];
    s32 trans_y;
    u8 pad4C[0x34];
    u8 *anim_data_frame;
} EffectCasing;

void effect_behavior_shell_casing(void) {
    int rand(void);
    s8 effect_create_billboard_l(s32, s32, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
#define CASING (LITP(EffectCasing, g_EffectPool))[LIT(u8, g_ActiveEffectIndex)]

    if (g_StatusFlags & STATUS_IN_WATER) {
        if (CASING.pos_y + CASING.vel_y > MODEL_OBJ(g_OmodelTable)->coord.t[1]) {
            CASING.draw_flags |= 0x400B;
            CASING.pre_behavior = 0x2F;
            CASING.post_behavior = 0;
            CASING.disp_y = g_PlayerEntity.floor_y - ({ CASING.local_offset_y + CASING.trans_y; });
            LIT(s32, sp_vec_vx) = CASING.pos_x;
            SCRATCH->vec_y = CASING.pos_y;
            SCRATCH->vec_z = CASING.pos_z;
            effect_create_billboard_l(0x17, 8, CASING.yaw, 0, LITP(s32, sp_vec), 5);
        }
    } else {
        if (g_PlayerEntity.floor_y < CASING.pos_y + CASING.vel_y) {
            CASING.draw_flags |= 0x400B;
            CASING.anim_data_frame += 0x18;
            CASING.anim_data_frame += (rand() & LIT(u16, g_RandSeed)) % CASING.hdr05 * 0x18;
            CASING.post_behavior = CASING.anim_data_frame[1];
            CASING.vel_x = FIELD_AT(CASING.anim_data_frame, EffectAnimStep, ->vel_x, u16);
            CASING.vel_y = FIELD_AT(CASING.anim_data_frame, EffectAnimStep, ->vel_y, u16);
            CASING.vel_z = FIELD_AT(CASING.anim_data_frame, EffectAnimStep, ->vel_z, u16) + (rand() & LIT(u16, g_RandSeed)) % CASING.hdr05 * 10;
            CASING.yaw += FIELD_AT(CASING.anim_data_frame, EffectAnimStep, ->yaw, u16);
            CASING.disp_y = g_PlayerFloorY - ({ CASING.local_offset_y + CASING.trans_y; });
            if (CASING.hdr06 != 0) {
                s32 t = CASING.vel_x;
                CASING.vel_x = -t;
            }
            if (g_PlayerEntity.behavior_flags - 2 == CASING.hdr07) {
                SCRATCH->vec_x = CASING.pos_x;
                SCRATCH->vec_y = CASING.pos_y;
                SCRATCH->vec_z = CASING.pos_z;
                sound_play_se_3d(1, 10, 0, LITP(s32, sp_vec));
            }
        }
    }
#undef CASING
}

void effect_behavior_floor_splash(void) {
    void effect_behavior_spawn_from_header();
    if (EFFECT->pos_y + EFFECT->vel_y >= 0) {
        EFFECT->pre_behavior = 0x2F;
        EFFECT->post_behavior = 0;
        EFFECT->disp_y = -(({ EFFECT->local_offset_y + EFFECT->transform.t[1]; }));
        effect_behavior_spawn_from_header();
        LITP(Effect, g_EffectPool)[LIT(s32, sp_tmp00)].scale = LITP(Effect, g_EffectPool)[LIT(s32, sp_tmp00)].scale + ({ -3; }) + EFFECT->scale;
    }
}

void effect_behavior_fall_stop(void) {
    if (EFFECT->pos_y + EFFECT->vel_y >= 0) {
        EFFECT->pre_behavior = 0x2F;
        EFFECT->draw_flags = 0x4007;
        EFFECT->accel_x = EFFECT->accel_y = EFFECT->accel_z = 0;
        EFFECT->vel_x = EFFECT->vel_y = EFFECT->vel_z = 0;
        EFFECT->disp_y = -({ EFFECT->local_offset_y + EFFECT->transform.t[1]; });
    }
}

void effect_behavior_floor_phase(void) {
    s32 effect_behavior_next_phase_a7() __asm__("effect_behavior_next_phase");
    s32 off;
    s32 off2;
    s32 off3;

    off = EFFECT_CUR_OFF;
    if ((EFFECT_AT(off, pos_y, s16) + EFFECT_AT(off, vel_y, s16)) >= 0) {
        EFFECT_AT(off, accel_z, u16) = 0;
        EFFECT_AT(off, accel_y, u16) = 0;
        EFFECT_AT(off, accel_x, s16) = 0;
        off2 = EFFECT_CUR_OFF;
        EFFECT_AT(off2, vel_z, s16) = 0;
        EFFECT_AT(off2, vel_y, s16) = 0;
        EFFECT_AT(off2, vel_x, s16) = 0;
        off3 = EFFECT_CUR_OFF;
        { s32 wt = -(EFFECT_AT(off3, local_offset_y, s16) + (EFFECT_AT(off3, transform.t[1], s32))); EFFECT_AT(off3, disp_y, s16) = wt; }
        effect_behavior_set_frame();
        effect_behavior_next_phase_a7();
    }
}

void effect_behavior_floor_stop(void) {
    if (EFFECT->pos_y + EFFECT->vel_y >= 0) {
        EFFECT->pre_behavior = 0x2F;
        EFFECT->post_behavior = 0;
        EFFECT->draw_flags = 6;
        EFFECT->accel_x = EFFECT->accel_y = EFFECT->accel_z = 0;
        EFFECT->vel_x = EFFECT->vel_y = EFFECT->vel_z = 0;
        EFFECT->disp_y = -({ EFFECT->local_offset_y + EFFECT->transform.t[1]; });
    }
}

void effect_behavior_wobble(void) {
    int rand(void);
    u8 t;

    t = EFFECT->hdr07;
    EFFECT->hdr07 = t - 1;
    if (t != 0) {
        EFFECT->draw_flags |= (rand() % 2) << 7;
        EFFECT->vel_x =
            (120 - (rand() % 6) * 50) * ((s8) EFFECT->hdr06);
        EFFECT->accel_y = -8 - rand() % 6;
        EFFECT->scale -= rand() % 2;
        EFFECT->yaw = (rand() % 6) * 60 * 4096 / 360;
        EFFECT->hdr04 = rand() % 6;
        effect_behavior_set_frame();
    } else {
        EFFECT->pre_behavior = 0x11;
        EFFECT->hdr07 = 0;
        EFFECT->type = 2;
        EFFECT->transform = *(MATRIX *)EFFECT->parent_matrix;
    }
}

void effect_behavior_bounce_debris(void) {
    int rand(void);

    LIT(s32, sp_vec_vx) = EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    if (EFFECT_AT(EFFECT_CUR_OFF, vel_y, s16) <= 0
        || room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 0x3C) != 0) {
        EFFECT_AS(accel_x, s16) *= -1;
    }
    if (EFFECT->vel_y
        < EFFECT_AS(hdr07, s8)) {
        EFFECT_AS(vel_x, s16) =
            EFFECT_AS(hdr06, s8) * (30 - rand() % 4);
        EFFECT_AS(hdr07, s8) =
            (EFFECT->vel_y = 70 - (rand() % 4) * 2) / -3;
        EFFECT_AS(vel_z, s16) +=
            EFFECT_AS(hdr06, s8) * (rand() % 5);
        EFFECT_AS(accel_x, s16) =
            EFFECT_AS(hdr06, s8) * (rand() % 3);
        EFFECT_AS(accel_y, s16) = -2 - rand() % 2;
        EFFECT_AS(hdr06, s8) *= -1;
    }
}

void effect_behavior_fade_fall(void) {
    void effect_behavior_drop_splat();
    if (EFFECT->hdr04-- == 0) {
        EFFECT->pre_behavior = 0x16;
        EFFECT->vel_y = 0;
        EFFECT->accel_y = EFFECT->hdr06;
    }
    effect_behavior_drop_splat();
}

void effect_behavior_drop_splat(void) {
    void effect_ground_splat();
    s32 off;

    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) != 0) {
        effect_ground_splat();
    }
    off = EFFECT_CUR_OFF;
    if (effect_projectile_hit_check(0x258, EFFECT_AT(off, pos_x, s32), EFFECT_AT(off, pos_z, s32)) != 0) {
        effect_ground_splat();
    }
}

void effect_behavior_ground_splat(void) {
    void effect_ground_splat();
    s32 off;
    s32 off2;

    off = EFFECT_CUR_OFF;
    if ((EFFECT_AT(off, pos_y, s16) + EFFECT_AT(off, vel_y, s16)) >= 0) {
        effect_ground_splat();
        off2 = EFFECT_CUR_OFF;
        EFFECT_AT(off2, draw_flags, s16) = EFFECT_AT(off2, draw_flags, u16) | 0x4007;
    }
}

void effect_ground_splat() {
    s32 effect_create_billboard_a10() __asm__("effect_create_billboard");
    s32 off2;
    s32 off3;
    s32 off4;
    s32 off5;
    s32 off;
    s32 off6;

    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, frame_cur, s32) = EFFECT_AT(off, frame_list, s32) + 0x14;
    off2 = EFFECT_CUR_OFF;
    EFFECT_AT(off2, frame_index, u8) = *(u8 *)EFFECT_AT(off2, frame_cur, s32);
    off3 = EFFECT_CUR_OFF;
    EFFECT_AT(off3, frame_delay, u8) = EFF_FRAME(EFFECT_AT(off3, frame_cur, s32))->delay;
    off4 = EFFECT_CUR_OFF;
    EFFECT_AT(off4, uv_cur, s32) = (EFFECT_AT(off4, frame_index, u8) * 4) + EFFECT_AT(off4, uv_list, s32);
    EFFECT_AT(EFFECT_CUR_OFF, pre_behavior, u8) = 1;
    EFFECT_AT(EFFECT_CUR_OFF, post_behavior, u8) = 1;
    off5 = EFFECT_CUR_OFF;
    EFFECT_AT(off5, accel_y, u16) = 0;
    EFFECT_AT(off5, accel_x, u16) = 0;
    EFFECT_AT(off5, vel_y, u16) = 0;
    EFFECT_AT(off5, vel_x, u16) = 0;
    off6 = EFFECT_CUR_OFF;
    if (EFFECT_AT(off6, hdr05, u8) != 0) {
        LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(off6, pos_x, s16);
        LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
        LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
        effect_create_billboard_a10(9, 1, 0, 0, ADDR_sp_vec, (s32) EFFECT_AT(EFFECT_CUR_OFF, hdr07, u8));
        sound_play_se_3d(2, 8, 0, ADDR_sp_vec);
    }
}

void effect_behavior_jitter(void) {
    int rand(void);

    if (EFFECT->hdr07 & 1) {
        EFFECT->vel_x = ((s16) EFFECT->vel_x) + (rand() % 5);
    }
    if (EFFECT->hdr07 & 2) {
        EFFECT->vel_y = EFFECT->vel_y + (rand() % 5);
    }
    if (EFFECT->hdr07 & 4) {
        EFFECT->vel_z = ((s16) EFFECT->vel_z) + (rand() % 5);
    }
    if (EFFECT->hdr07 & 8) {
        EFFECT->accel_x = ((s16) EFFECT->accel_x) + (rand() % 5);
    }
    if (EFFECT->hdr07 & 0x10) {
        EFFECT->accel_y = ((s16) EFFECT->accel_y) + (rand() % 5);
    }
    if (EFFECT->hdr07 & 0x20) {
        EFFECT->accel_z = ((s16) EFFECT->accel_z) + (rand() % 5);
    }
}

void effect_behavior_rand_anim(void) {
    typedef struct { u8 pad0[0x24]; u8 pre_behavior; u8 pad25[5]; u8 hdr06; u8 pad2B[7]; u16 draw_flags; u8 pad34[0x50]; } EffectFromBlock;
    int rand(void);
    LITP(EffectFromBlock, g_GameStateFlags)[LIT(u8, g_ActiveEffectIndex)].draw_flags |= (rand() % 3) << 6;
    LITP(EffectFromBlock, g_GameStateFlags)[LIT(u8, g_ActiveEffectIndex)].pre_behavior = LITP(EffectFromBlock, g_GameStateFlags)[LIT(u8, g_ActiveEffectIndex)].hdr06;
}

void effect_behavior_random_spin(void) {
    int rand(void);
    Effect *p;
    s16 v;

    p = &LITP(Effect, g_EffectPool)[g_ActiveEffectIndex];
    v = 15;
    if (rand() & 1) {
        v = 5;
    }
    EFF(p)->disp_x = (rand() & 1) ? v : -v;
    p = &LITP(Effect, g_EffectPool)[g_ActiveEffectIndex];
    v = 15;
    if (rand() & 1) {
        v = 5;
    }
    EFF(p)->disp_y = (rand() & 1) ? v : -v;
    p = &LITP(Effect, g_EffectPool)[g_ActiveEffectIndex];
    v = 15;
    if (rand() & 1) {
        v = 5;
    }
    EFF(p)->disp_z = (rand() & 1) ? v : -v;
}

void effect_behavior_drift(void) {
    s32 effect_behavior_next_phase_a8() __asm__("effect_behavior_next_phase");
    s16 new_z;
    s32 off;
    s32 off2;
    s32 off3;

    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, local_offset_x, u16) = EFFECT_AT(off, local_offset_x, u16) + (EFFECT_AT(off, hdr04, s8) * 2);
    off2 = EFFECT_CUR_OFF;
    EFFECT_AT(off2, local_offset_y, u16) = EFFECT_AT(off2, local_offset_y, u16) + (EFFECT_AT(off2, hdr05, s8) * 2);
    off3 = EFFECT_CUR_OFF;
    new_z = EFFECT_AT(off3, local_offset_z, s32) + (EFFECT_AT(off3, hdr06, s8) * 2);
    EFFECT_AT(off3, local_offset_z, s16) = new_z;
    effect_behavior_next_phase_a8();
}

void effect_behavior_frame_phase(void) {
    s32 effect_behavior_next_phase_a9() __asm__("effect_behavior_next_phase");
    void effect_behavior_set_frame();
    effect_behavior_set_frame();
    effect_behavior_next_phase_a9();
}

void effect_behavior_burn(void) {
    s32 effect_behavior_spawn_from_header_p() __asm__("effect_behavior_spawn_from_header");
    void effect_behavior_kill(void);
    u8 saved_status;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    saved_status = LIT(u8, g_PlayerEntity);
    LIT(u8, g_PlayerEntity) = EFFECT->hdr06;
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) != 0 && EFFECT->hdr06 == 0x21) {
        effect_behavior_spawn_from_header_p();
        effect_behavior_kill();
        LIT(u8, g_PlayerEntity) = saved_status;
        return;
    }
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) == 1) {
        effect_behavior_spawn_from_header_p();
        effect_behavior_kill();
    }
    g_PlayerEntity.status_flags = saved_status;
}

void effect_behavior_flamethrower(void) {
    s32 apply_weapon_damage_a1(s32) __asm__("apply_weapon_damage");
    void effect_behavior_spawn_from_header();
    void effect_behavior_kill(void);
    u8 saved_status;

    saved_status = LIT(u8, g_PlayerEntity);
    LIT(u8, g_PlayerEntity) = EFFECT_AT(EFFECT_CUR_OFF, hdr06, s8);
    LIT(s32, sp_vec_vx) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vz) = (s32) EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    if (apply_weapon_damage_a1(6) != 0) {
        effect_behavior_spawn_from_header();
        effect_behavior_kill();
    }
    LIT(u8, g_PlayerEntity) = saved_status;
}

void effect_behavior_fire_wobble(void) {
    int rand(void);
    u8 t;

    t = EFFECT->hdr07;
    EFFECT->hdr07 = t - 1;
    if (t != 0) {
        EFFECT->draw_flags |= (rand() % 2) << 7;
        EFFECT->vel_x =
            (60 - (rand() % 6) * 25) * ((s8) EFFECT->hdr06);
        EFFECT->accel_y = -10 - rand() % 6;
        EFFECT->scale -= rand() % 2;
        EFFECT->yaw = (rand() % 6) * 60 * 4096 / 360;
        EFFECT->hdr04 = rand() % 6;
        effect_behavior_set_frame();
    } else {
        EFFECT->pre_behavior = 0x11;
        EFFECT->hdr07 = 0;
        EFFECT->type = 2;
        EFFECT->transform = *(MATRIX *)EFFECT->parent_matrix;
    }
}

void effect_behavior_fire_bounce(void) {
    int rand(void);

    LIT(s32, sp_vec_vx) = EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    if (EFFECT_AT(EFFECT_CUR_OFF, vel_y, s16) <= 0
        || room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 0x3C) != 0) {
        EFFECT_AS(accel_x, s16) *= -1;
    }
    if (EFFECT->vel_y
        < EFFECT_AS(hdr07, s8)) {
        EFFECT_AS(vel_x, s16) =
            EFFECT_AS(hdr06, s8) * (10 - rand() % 4);
        EFFECT_AS(hdr07, s8) =
            (EFFECT->vel_y = 70 - (rand() % 4) * 2) / -3;
        EFFECT_AS(vel_z, s16) +=
            EFFECT_AS(hdr06, s8) * (rand() % 5);
        EFFECT_AS(accel_x, s16) =
            EFFECT_AS(hdr06, s8) * (rand() % 3);
        EFFECT_AS(accel_y, s16) = -10 - rand() % 2;
        EFFECT_AS(hdr06, s8) *= -1;
    }
}

void effect_behavior_pair_timer(void) {
    void effect_behavior_kill(void);
    s32 effect_create_billboard_a11() __asm__("effect_create_billboard");

    EFFECT->hdr06 = ((s8) EFFECT->hdr06) - 1;
    EFFECT->hdr07 = ((s8) EFFECT->hdr07) - 1;
    if (EFFECT_AT(EFFECT_CUR_OFF, hdr07, u8) == 0) {
        effect_behavior_kill();
        return;
    }
    if (EFFECT_AT(EFFECT_CUR_OFF, hdr06, u8) == 0) {
        EFFECT->hdr06 = 4;
        effect_create_billboard_a11(EFFECT->hdr04, EFFECT->hdr05, EFFECT->yaw, EFFECT_AT(EFFECT_CUR_OFF, parent_matrix, s32), (s32) &EFFECT->spawn_x, (s32) EFFECT_AT(EFFECT_CUR_OFF, scale, u8));
    }
}

void effect_behavior_charge(void) {
    s8 effect_create_billboard_l(s32, s32, s32, s32, s32 *, s32) __asm__("effect_create_billboard");

    switch (EFFECT->hdr07) {
    case 0:
        EFFECT->hdr04 = g_PlayerEntity.status_flags;
        EFFECT->hdr05 = g_PlayerEntity.behavior_flags;
        EFFECT->hdr07++;
        break;
    case 1:
        EFFECT->type = 2;
        EFFECT->transform = *(MATRIX *)EFFECT->parent_matrix;
        EFFECT->hdr07++;
        break;
    case 2:
        LIT(s32, sp_vec_vx) = EFFECT->pos_x;
        SCRATCH->vec_y = EFFECT->pos_y;
        SCRATCH->vec_z = EFFECT->pos_z;
        effect_create_billboard_l(8, 1, EFFECT->yaw, 0,
                        LITP(s32, sp_vec), 0);
        break;
    }
}

void effect_behavior_bullet(void) {
    s32 apply_weapon_damage_l() __asm__("apply_weapon_damage");
    s32 effect_shot_impact_l() __asm__("effect_shot_impact");
    s32 unused;
    s32 unused2;
    s32 off;
    s32 next_y;
    s32 off2;
    u8 status_low;
    u8 saved_status;
    u8 ticks;
    u8 *player_status = LITP(u8, g_PlayerEntity);

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_vec_vx) = EFFECT_AT(EFFECT_CUR_OFF, pos_x, s16);
    LIT(s32, sp_vec_vy) = EFFECT_AT(EFFECT_CUR_OFF, pos_y, s16);
    LIT(s32, sp_vec_vz) = EFFECT_AT(EFFECT_CUR_OFF, pos_z, s16);
    if (g_PlayerEntity.floor_y < EFFECT->pos_y + EFFECT->vel_y) {
        effect_shot_impact_l(1);
        return;
    }
    saved_status = *player_status;
    status_low = saved_status & 0x1F;
    *player_status = status_low;
    next_y = EFFECT->pos_y + EFFECT->vel_y;
    if (g_PlayerEntity.floor_y - 0xAF0 < next_y) {
        if (g_PlayerEntity.floor_y - 0x1F4 < next_y) {
            *player_status = status_low | 0x20;
        } else {
            *player_status = status_low | 0x40;
        }
    } else {
        *player_status = status_low | 0x80;
    }
    if (room_collision_check_point_radius(ADDR_sp_vec, ADDR_sp_svec, 4) == 1) {
        g_PlayerEntity.status_flags = saved_status;
        effect_shot_impact_l(1);
        return;
    }
    if (apply_weapon_damage_l(EFFECT_AT(EFFECT_CUR_OFF, hdr05, u8)) != 0) {
        g_PlayerEntity.status_flags = saved_status;
        effect_shot_impact_l(0);
        return;
    }
    g_PlayerEntity.status_flags = saved_status;
    off = EFFECT_CUR_OFF;
    ticks = EFFECT_AT(off, hdr06, u8);
    EFFECT_AT(off, hdr06, u8) = ticks + 1;
    if (ticks >= 9U) {
        off2 = EFFECT_CUR_OFF;
        EFFECT_AT(off2, frame_cur, s32) = EFFECT_AT(off2, frame_list, s32);
    }
}

void effect_behavior_fire_burst(void) {
    s32 effect_behavior_kill_h6() __asm__("effect_behavior_kill");
    s32 effect_create_billboard_h6() __asm__("effect_create_billboard");
    u8 *cur;
    u8 state;

    LIT(s32, sp_vec_vx) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_x, s16);
    LIT(s32, sp_vec_vy) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_y, s16);
    LIT(s32, sp_vec_vz) = EFFECT_AT(EFFECT_CUR_OFF, local_offset_z, s16);
    state = EFFECT_AT(EFFECT_CUR_OFF, hdr04, u8);
    switch (state) {
    case 0:
        effect_create_billboard_h6(1, EFFECT_AT(EFFECT_CUR_OFF, depth_group, u8) + 1,
                         EFFECT_AT(EFFECT_CUR_OFF, yaw, s16),
                         EFFECT_AT(EFFECT_CUR_OFF, parent_matrix, s32), ADDR_sp_vec, 0);
        EFFECT->hdr04 = 1;
        return;
    case 1:
        effect_create_billboard_h6(1, EFFECT_AT(EFFECT_CUR_OFF, depth_group, u8) + 2,
                         EFFECT_AT(EFFECT_CUR_OFF, yaw, s16),
                         EFFECT_AT(EFFECT_CUR_OFF, parent_matrix, s32), ADDR_sp_vec, 0);
        cur = LITP(u8, g_EffectPool) + LIT(u8, g_ActiveEffectIndex) * EFFECT_SIZE;
        effect_create_billboard_h6(1, *(&EFF(cur)->depth_group) + 3, EFF(cur)->yaw, EFF(cur)->parent_matrix, ADDR_sp_vec, 0);
        EFFECT->hdr04 = 2;
        return;
    case 2:
        effect_create_billboard_h6(1, EFFECT_AT(EFFECT_CUR_OFF, depth_group, u8) + 4,
                         EFFECT_AT(EFFECT_CUR_OFF, yaw, s16),
                         EFFECT_AT(EFFECT_CUR_OFF, parent_matrix, s32), ADDR_sp_vec, 0);
        effect_behavior_kill_h6();
        return;
    }
}

void effect_behavior_rocket(void) {
    s32 apply_weapon_damage_l(s32) __asm__("apply_weapon_damage");
    void effect_rocket_explode(void);
    void effect_free_matching_l(s32, s32, s32, s32, s32) __asm__("effect_free_matching");
    s32 x;
    u32 z;
    u8 save;

    effect_behavior_spawn_from_header();
    SCRATCH->sv_x = 0;
    SCRATCH->sv_y = 0;
    SCRATCH->sv_z = 0;
    SCRATCH->vec_x = x = EFFECT->pos_x;
    SCRATCH->vec_y = EFFECT->pos_y;
    SCRATCH->vec_z = z = EFFECT->pos_z;
    if ((x & 0x80000000) || (z & 0x80000000)) {
        effect_free_matching_l(7, EFFECT->effect_type,
                        EFFECT->depth_group,
                        EFFECT->hdr06, 0);
    }
    if (room_collision_check_point_radius(LITP(void, sp_vec), LITP(void, sp_svec), 4) == 1) {
        SCRATCH->vec_x = EFFECT->pos_x;
        SCRATCH->vec_y = EFFECT->pos_y;
        SCRATCH->vec_z = EFFECT->pos_z;
        effect_rocket_explode();
        return;
    }
    save = LIT(u8, g_PlayerEntity);
    LIT(u8, g_PlayerEntity) = (save & 0x1F) | 0x40;
    if (apply_weapon_damage_l(0xA) != 0) {
        LIT(u8, g_PlayerEntity) = save;
        SCRATCH->vec_x = EFFECT->pos_x;
        SCRATCH->vec_y = EFFECT->pos_y;
        SCRATCH->vec_z = EFFECT->pos_z;
        sound_play_se_3d(1, 0xF, 4, LITP(void, sp_vec));
        sound_play_se_3d(1, 0xF, 0, LITP(void, sp_vec));
        effect_free_matching_l(7, EFFECT->effect_type,
                        EFFECT->depth_group,
                        EFFECT->hdr06, 0);
        return;
    }
    LIT(u8, g_PlayerEntity) = save;
}

void effect_behavior_countdown_kill(void) {
    void effect_behavior_kill(void);
    s32 off;
    u8 timer;

    off = EFFECT_CUR_OFF;
    timer = EFFECT_AT(off, hdr06, s8);
    EFFECT_AT(off, hdr06, s8) = timer + 0xFF;
    if (timer == 0) {
        effect_behavior_kill();
    }
}

void effect_behavior_kill(void) {
    s32 off;

    LIT(u8, g_FreeEffectSlots) += 1;
    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, post_behavior, s8) = 0;
    *(off + &LIT(s8, g_EffectPool)) = 0;
}

typedef struct { u8 pre_behavior[5]; u8 hdr05; u8 pad6[8]; u16 draw_flags; u8 pad10[0x74]; } EffectFlashView;

void effect_behavior_flash_marker(void) {
    void effect_behavior_kill(void);
    if (LITP(EffectFlashView, g_EffectPool)[g_ActiveEffectIndex].draw_flags == 6) {
        LITP(EffectFlashView, g_EffectPool)[g_ActiveEffectIndex].draw_flags = 0x8006;
    } else {
        LITP(EffectFlashView, g_EffectPool)[g_ActiveEffectIndex].draw_flags = 6;
    }
    if (LITP(EffectFlashView, g_EffectPool)[g_ActiveEffectIndex].hdr05-- == 0) {
        effect_behavior_kill();
    }
}

void effect_shot_impact(u8 hit_scenery) {
    s8 effect_create_billboard_l(s32, s32, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    s8 idx;

    LIT(s32, sp_vec_vx) = EFFECT->pos_x;
    SCRATCH->vec_y = EFFECT->pos_y;
    SCRATCH->vec_z = EFFECT->pos_z;
    switch (EFFECT->hdr05 - 7) {
    case 0:
        sound_play_se_3d(1, 0xC, 4, LITP(s32, sp_vec));
        sound_play_se_3d(1, 0xC, 0, LITP(s32, sp_vec));
        if (hit_scenery) {
            SCRATCH->vec_y -= 0x96;
            effect_create_billboard_l(0xE, 3, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x19);
            SCRATCH->vec_y += 0x96;
            LIT(s32, sp_vec_vx) -= 0xB4;
            SCRATCH->vec_z += 0xB4;
            effect_create_billboard_l(0xE, 3, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x1E);
            SCRATCH->vec_y += 0x96;
            LIT(s32, sp_vec_vx) += 0x154;
            SCRATCH->vec_z -= 0x168;
            effect_create_billboard_l(0xE, 3, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x1C);
            LIT(s32, sp_vec_vx) = EFFECT->pos_x;
            SCRATCH->vec_y = EFFECT->pos_y;
            SCRATCH->vec_z = EFFECT->pos_z;
            SCRATCH->vec_y -= 0xC8;
            idx = effect_create_billboard_l(9, 0xD, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x14);
            if (idx != -1) {
                LITP(Effect, g_EffectPool)[idx].pre_behavior = 1;
                LITP(Effect, g_EffectPool)[idx].draw_flags = 0x4003;
                LITP(Effect, g_EffectPool)[idx].post_behavior = 0;
                *(s16 *)&LITP(Effect, g_EffectPool)[idx].accel_y = -1;
                LITP(Effect, g_EffectPool)[idx].vel_y = 0;
                *(s16 *)&LITP(Effect, g_EffectPool)[idx].vel_x = 0;
                LITP(Effect, g_EffectPool)[idx].accel_x = 0;
            }
        }
        break;
    case 1:
        sound_play_se_3d(1, 0xD, 4, LITP(s32, sp_vec));
        sound_play_se_3d(1, 0xD, 0, LITP(s32, sp_vec));
        if (hit_scenery) {
            effect_create_billboard_l(9, 0, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x10);
            effect_create_billboard_l(9, 0, EFFECT->yaw + 0x555, 0, LITP(s32, sp_vec), 0x10);
            effect_create_billboard_l(9, 0, EFFECT->yaw + 0xAAA, 0, LITP(s32, sp_vec), 0x10);
            idx = effect_create_billboard_l(9, 0, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x14);
            if (idx != -1) {
                LITP(Effect, g_EffectPool)[idx].accel_x = 0;
                *(s16 *)&LITP(Effect, g_EffectPool)[idx].vel_x = 0;
                LITP(Effect, g_EffectPool)[idx].vel_y = -0x32;
            }
        }
        break;
    case 2:
        sound_play_se_3d(1, 0xE, 4, LITP(s32, sp_vec));
        sound_play_se_3d(1, 0xE, 0, LITP(s32, sp_vec));
        if (hit_scenery) {
            effect_create_billboard_l(0xE, 5, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x1B);
            LIT(s32, sp_vec_vx) -= 0xB4;
            effect_create_billboard_l(0xE, 4, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x17);
            LIT(s32, sp_vec_vx) += 0x140;
            SCRATCH->vec_z += 0x8C;
            effect_create_billboard_l(0xE, 4, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x16);
            SCRATCH->vec_z -= 0x118;
            effect_create_billboard_l(0xE, 4, EFFECT->yaw, 0, LITP(s32, sp_vec), 0x15);
        }
        break;
    }
    effect_behavior_kill();
}

void effect_behavior_sysflag(void) {
    void effect_behavior_set_frame();
    if (Flg_ck(ADDR_g_SysFlags, 0) != 0) {
        *((g_ActiveEffectIndex * EFFECT_SIZE) + &LIT(s8, g_EffectPool)) = 1;
        effect_behavior_set_frame();
    }
}

void effect_behavior_fade(void) {
    s32 off;
    u8 scale;

    EFFECT_AT(EFFECT_CUR_OFF, frame_delay, s8) = 0;
    off = EFFECT_CUR_OFF;
    scale = EFFECT_AT(off, scale, s8);
    if (scale != 0) {
        EFFECT_AT(off, scale, s8) = scale + 0xFE;
    }
}

typedef struct {
    u8 pre_behavior;
    u8 post_behavior;
    u8 type;
    u8 pad3[0x2D];
    MATRIX transform;
    u8 pad50[0x14];
    MATRIX *parent_matrix;
    u8 pad68[0x1C];
} EffectSettleView;

void effect_behavior_to_settle(void) {
    u8 *idx;

    idx = LITP(u8, g_ActiveEffectIndex);
    LITP(EffectSettleView, g_EffectPool)[*idx].transform = *LITP(EffectSettleView, g_EffectPool)[*idx].parent_matrix;
    LITP(EffectSettleView, g_EffectPool)[*idx].type = 2;
    LITP(EffectSettleView, g_EffectPool)[*idx].pre_behavior = 5;
    effect_behavior_gravity_settle();
}

void effect_behavior_floor_stop1(void) {
    s32 t;

    if (EFFECT->pos_y +
        EFFECT->vel_y >= 0) {
        EFFECT->pre_behavior = 1;
        EFFECT->post_behavior = 0;
        EFFECT->accel_x =
            EFFECT->accel_y =
            EFFECT->accel_z = 0;
        EFFECT->vel_x =
            EFFECT->vel_y =
            EFFECT->vel_z = 0;
        t = -(EFFECT->local_offset_y +
              EFFECT->transform.t[1]);
        EFFECT->disp_y = t;
    }
}

void effect_rocket_explode(void) {
    s8 effect_create_billboard_l(s32, s32, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    void effect_free_matching_l(s32, s32, s32, s32, s32) __asm__("effect_free_matching");
    s8 r;

    LIT(s32, sp_vec_vx) = EFFECT->pos_x;
    SCRATCH->vec_y = EFFECT->pos_y;
    SCRATCH->vec_z = EFFECT->pos_z;
    sound_play_se_3d(1, 0xF, 4, LITP(void, sp_vec));
    sound_play_se_3d(1, 0xF, 0, LITP(void, sp_vec));
    effect_create_billboard_l(0xE, 3, EFFECT->yaw, 0, LITP(void, sp_vec), 0x1C);
    effect_create_billboard_l(0xE, 3, EFFECT->yaw, 0, LITP(void, sp_vec), 0x19);
    SCRATCH->vec_y -= 200;
    r = effect_create_billboard_l(9, 0xD, EFFECT->yaw, 0, LITP(void, sp_vec), 0x13);
    if (r != -1) {
        LITP(Effect, g_EffectPool)[r].pre_behavior = 1;
        LITP(Effect, g_EffectPool)[r].post_behavior = 0;
        LITP(Effect, g_EffectPool)[r].draw_flags = 0x4003;
        LITP(Effect, g_EffectPool)[r].vel_y = 0;
        LITP(Effect, g_EffectPool)[r].vel_x = 0;
        LITP(Effect, g_EffectPool)[r].accel_x = 0;
    }
    effect_free_matching_l(7, EFFECT->effect_type,
                    EFFECT->depth_group,
                    EFFECT->hdr06, 0);
}

void effect_behavior_splash_timer(s32 unused_arg) {
    s32 effect_projectile_hit_check_a2() __asm__("effect_projectile_hit_check");
    void effect_ground_splat();
    s32 off4;
    s32 off5;
    s32 off6;
    s32 off7;
    s32 off9;
    s32 off;
    s32 off2;
    s32 off3;
    s32 next_y;
    s32 off8;
    s32 uv_base;

    uv_base = unused_arg;
    off = EFFECT_CUR_OFF;
    EFFECT_AT(off, hdr04, s8) = EFFECT_AT(off, hdr04, s8) - 1;
    off2 = EFFECT_CUR_OFF;
    if (EFFECT_AT(off2, hdr04, u8) == 0) {
        EFFECT_AT(off2, hdr04, s8) = 4;
        off3 = EFFECT_CUR_OFF;
        EFFECT_AT(off3, frame_cur, s32) = EFFECT_AT(off3, frame_list, s32) + 0x1C;
        off4 = EFFECT_CUR_OFF;
        EFFECT_AT(off4, frame_index, s8) = *EFFECT_AT(off4, frame_cur, s32 *);
        off5 = EFFECT_CUR_OFF;
        EFFECT_AT(off5, frame_delay, s8) = EFF_FRAME(EFFECT_AT(off5, frame_cur, s32))->delay;
        off6 = EFFECT_CUR_OFF;
        uv_base = EFFECT_AT(off6, uv_list, s32);
        EFFECT_AT(off6, uv_cur, s32) = (EFFECT_AT(off6, frame_index, u8) * 4) + uv_base;
    }
    off7 = EFFECT_CUR_OFF;
    next_y = EFFECT_AT(off7, pos_y, s16) + EFFECT_AT(off7, vel_y, s16);
    if (next_y >= -0x960) {
        if (next_y >= 0) {
            effect_ground_splat(uv_base);
            off8 = EFFECT_CUR_OFF;
            EFFECT_AT(off8, draw_flags, s16) = EFFECT_AT(off8, draw_flags, u16) | 0x4007;
        }
        if (g_PlayerEntity.state == 1) {
            off9 = EFFECT_CUR_OFF;
            if (effect_projectile_hit_check_a2(0x384, EFFECT_AT(off9, pos_x, s16), EFFECT_AT(off9, pos_z, s16)) != 0) {
                effect_ground_splat();
            }
        }
    }
}

void effect_behavior_fire_phases(void) {
    int rand(void);
    switch (EFFECT->hdr04) {
    case 0:
        EFFECT->frame_cur = EFFECT->frame_list;
        EFFECT->frame_cur += 0x14;
        EFFECT->frame_delay = EFF_FRAME(EFFECT->frame_cur)->delay;
        EFFECT->uv_cur = (s32) (((u32 *) EFFECT->uv_list) + EFF_FRAME(EFFECT->frame_cur)->uv_index);
        EFFECT->hdr05 = 0xB;
        EFFECT->hdr04 = 1;
    case 1:
        EFFECT->draw_flags = 0x4003;
        EFFECT->scale = EFFECT->hdr06;
        EFFECT->hdr05--;
        if (EFFECT->hdr05 != 0) {
            break;
        }
        EFFECT->hdr04 = 2;
        EFFECT->draw_flags = 0;
        EFFECT->scale = 0;
        break;
    case 2:
        EFFECT->frame_cur = EFFECT->frame_list;
        EFFECT->frame_cur += 0x14;
        EFFECT->frame_delay = EFF_FRAME(EFFECT->frame_cur)->delay;
        EFFECT->uv_cur = (s32) (((u32 *) EFFECT->uv_list) + EFF_FRAME(EFFECT->frame_cur)->uv_index);
        EFFECT->hdr05 = rand() % 5 * 10 + 100;
        EFFECT->hdr04 = 3;
        EFFECT->draw_flags = 0;
        EFFECT->scale = 0;
        break;
    case 3:
        EFFECT->draw_flags = 0;
        EFFECT->scale = 0;
        EFFECT->hdr05--;
        if (EFFECT->hdr05 != 0) {
            break;
        }
        EFFECT->hdr04 = 0;
        EFFECT->hdr05 = 0xB;
        break;
    }
}

void effect_behavior_muzzle_phases(void) {
    switch (EFFECT->hdr04) {
    case 0:
        EFFECT->hdr05 = 1;
        EFFECT->hdr04 = 1;
        break;
    case 1:
        EFFECT->transform = *(MATRIX *)EFFECT->parent_matrix;
        EFFECT->type = 2;
        EFFECT->hdr05 = 2;
        EFFECT->hdr04 = 2;
        break;
    case 2:
        EFFECT->hdr05++;
        if (EFFECT->hdr05 == 9) {
            EFFECT->hdr06--;
            EFFECT->hdr05 = 4;
            EFFECT->frame_cur = EFFECT->frame_list;
            EFFECT->frame_cur += 0x10;
            EFFECT->frame_delay = EFF_FRAME(EFFECT->frame_cur)->delay;
            EFFECT->uv_cur = (s32) (((s32 *) EFFECT->uv_list) + *((u8 *) EFFECT->frame_cur));
        }
        if (EFFECT->hdr06 == 0) {
            EFFECT->hdr04 = 3;
        }
        break;
    }
}

void effect_behavior_autoaim_flash(void) {
    s32 effect_behavior_timer_refresh_a2() __asm__("effect_behavior_timer_refresh");
    s32 weapon_get_ammo();
    effect_behavior_timer_refresh_a2();
    EFFECT_AT(g_ActiveEffectIndex * EFFECT_SIZE, hdr06, s8) = weapon_get_ammo() & 3;
}

void effect_behavior_dual_shot(void) {
    s32 effect_create_billboard_k() __asm__("effect_create_billboard");
    void effect_behavior_next_phase_l() __asm__("effect_behavior_next_phase");
    s32 r;
    u8 t;

    t = EFFECT->hdr04;
    EFFECT->hdr04 = t - 1;
    if (t == 0) {
        if (EFFECT->hdr05 != 0) {
            LIT(s32, sp_vec_vx) = EFFECT->local_offset_x - EFFECT_AS(hdr06, s8);
            SCRATCH->vec_y = EFFECT->local_offset_y + 10;
            SCRATCH->vec_z = EFFECT->local_offset_z + EFFECT_AS(hdr07, s8);
            r = effect_create_billboard_k(5, 3, EFFECT->yaw, EFFECT->parent_matrix, LITP(s32, sp_vec), EFFECT->scale);
            SCRATCH->tmp00 = r;
            LITP(Effect, g_EffectPool)[r].hdr07 = g_PlayerEntity.behavior_flags - 2;
        }
        LIT(s32, sp_vec_vx) = EFFECT->local_offset_x + EFFECT_AS(hdr06, s8);
        SCRATCH->vec_y = EFFECT->local_offset_y;
        SCRATCH->vec_z = EFFECT->local_offset_z + EFFECT_AS(hdr07, s8);
        r = effect_create_billboard_k(5, 3, EFFECT->yaw, EFFECT->parent_matrix, LITP(s32, sp_vec), EFFECT->scale);
        SCRATCH->tmp00 = r;
        LITP(Effect, g_EffectPool)[r].hdr07 = g_PlayerEntity.behavior_flags - 2;
        effect_behavior_next_phase_l();
    }
}

void stage_init_eff_sprite(void) {
    u8 load_effect_sprite_data_l() __asm__("load_effect_sprite_data");
    void setup_effect_sprite_textures_l() __asm__("setup_effect_sprite_textures");
    void memclr_l() __asm__("memclr");
    u8 i;
    u8 c;
    u8 *p;
    u8 *w;
    u8 *q;
    u8 *r;
    s32 t;
    s32 o;

    g_FreeEffectSlots = 0x40;
    memclr_l(ADDR_g_EffectPool, 0x840);
    i = 0;
    w = LITP(u8, g_EffectTexPage);
    while (i < 8) {
        c = LITP(u8, g_EffectRoomSpriteIds)[i];
        q = (u8 *)(i + (s32)w);
        if (c == 0xFF) {
            i = 8;
        } else {
            i++;
            EFF_SPR(w)->sprite_info[c] = -1;
            EFF_SPR(w)->sprite_anim[c] = -1;
            FIELD_AT(q, EffectSpriteTable, ->sprite_ids[8], u8) = 0xFF;   /* q = table + i: the room slot sprite_ids[8 + i] */
        }
    }
    p = g_RoomData;
    load_effect_sprite_data_l(ROOM_HDR(p)->effect_index, ROOM_HDR(p)->effect_data, p, 8);
    for (i = 0; i < 8; i++) {
        r = LIT(u8 *, g_RoomData);
        o = i;
        o <<= 2;
        t = *(s32 *)(((u8 *) ROOM_HDR(r)->effect_textures) - o);
        o += (s32)g_EffectSpriteTimPtrs;
        *(s32 *)o = t + (s32)r;
    }
    setup_effect_sprite_textures_l(8);
}

void load_shoot_direction_data(void) {
    u8 load_effect_sprite_data_l() __asm__("load_effect_sprite_data");
    void setup_effect_sprite_textures_l() __asm__("setup_effect_sprite_textures");
    u8 i;
    u8 *w;
    u8 *w2;
    u32 base;
    u32 t;
    u8 n;
    s32 *q;
    s32 v;

    g_FreeEffectSlots = 0x40;
    i = 0;
    w = LITP(u8, g_EffectTexPage);
    do {
        *(i + EFF_SPR(w)->sprite_ids) = 0xFF;   /* sprite_ids[i]; this operand order matches */
        i++;
    } while (i < 16);
    i = 0;
    w2 = LITP(u8, g_EffectTexPage);
    do {
        EFF_SPR(w2)->sprite_info[i] = -1;
        EFF_SPR(w2)->sprite_anim[i] = -1;
        i++;
    } while (i < 50);
    base = LIT(u32, g_LoadPointer);
    load_file(6, base);
    t = ((u16)g_CdFileTable[6].size & 0xFFFC) + LIT(u32, g_LoadPointer);
    LIT(u32, g_LoadPointer) = t;
    if ((u16)g_CdFileTable[6].size & 3) {
        LIT(u32, g_LoadPointer) = t + 4;
    }
    LIT(u32, g_LoadPointer) -= 4;
    n = load_effect_sprite_data_l(base, LIT(u32, g_LoadPointer), base, 0);
    LIT(u32, g_LoadPointer) = ADDR_g_OverlayArea;
    q = LITP(s32, g_OverlayArea);
    load_file(7, ADDR_g_OverlayArea);
    q = (s32 *)(((u16)g_CdFileTable[7].size & 0xFFFC) + (u32)q);
    if ((u16)g_CdFileTable[7].size & 3) {
        q++;
    }
    i = 0;
    do {
        q--;
        v = LIT(s32, g_LoadPointer);
        v += *q;
        g_EffectSpriteTimPtrs[i] = v;
        i++;
    } while (i < n);
    setup_effect_sprite_textures_l(0);
}

u8 load_effect_sprite_data(u8 *ids, s32 *offsets, s32 base, u8 first) {
    u8 ret;
    u8 i;
    u8 c;
    u8 *p;
    u8 *q;
    s32 v;
    s32 k;
    s32 w;
    s32 tbl;

    ret = 8;
    i = 0;
    tbl = ADDR_g_EffectTexPage;
    do {
        c = ids[i];
        k = i + first;
        LITP(u8, g_EffectSpriteIds)[k] = c;
        if (c == 0xFF) {
            ret = i;
            i = 8;
        } else {
            v = offsets[-i];
            v += base;
            p = (u8 *)(c * 4 + tbl);
            EFF_SPR(p)->sprite_info[0] = v;
            EFF_SPR(p)->sprite_anim[0] = v;
            q = ((u8 *) EFF_SPR(p)->sprite_info[0]);
            i++;
            w = (q[2] + q[0]) * 4 + 8;
            EFF_SPR(p)->sprite_anim[0] = w + v;
        }
    } while (i < 8);
    return ret;
}

void setup_effect_sprite_textures(s32 start) {
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    void GsGetTimInfo(u_long *im, GsIMAGE *tim);
    GsIMAGE tim;
    RECT rect;
    u8 id;
    u8 ch;
    u8 i;
    u8 n;
    u8 page;
    u16 x;
    u16 y;
    u16 cx;
    u16 cy;
    u16 *p;
    u8 *q;
    s32 k;
    u16 c;

    if ((start & 0xFF) == 8) {
        page = g_EffectTexPage;
        x = g_EffectTexPos[0];
        y = g_EffectTexPos[1];
        cx = g_EffectClutPos[0];
        cy = g_EffectClutPos[1];
    } else {
        g_EffectTexPage = 0x18;
        g_EffectTexPos[0] = 0;
        g_EffectTexPos[1] = 0;
        g_EffectClutPos[0] = 0;
        g_EffectClutPos[1] = 4;
        page = 0x18;
        x = 0;
        y = 0;
        cx = 0;
        cy = 4;
    }
    i = 0;
    k = start & 0xFF;
    goto test;
    do {
        id = LITP(u8, g_EffectSpriteIds)[i + (start & 0xFF)];
        GsGetTimInfo((u_long *)(g_EffectSpriteTimPtrs[i] + 4), &tim);
        if (y + tim.ph > 0x100) {
            x += 0x40;
            y = 3;
            page++;
        }
        if (cy + tim.ch >= 0x20) {
            cx++;
            cy = 4;
        }
        rect.x = x + 0x200;
        rect.y = y + 0x100;
        rect.w = tim.pw;
        rect.h = tim.ph;
        ch = tim.ch;
        LoadImage(&rect, tim.pixel);
        DrawSync(0);
        if ((tim.pmode >> 3) & 1) {
            rect.x = cx * 16 + 0x100;
            rect.y = cy + 0x1E0;
            rect.w = tim.cw;
            rect.h = tim.ch;
            LoadImage(&rect, tim.clut);
            DrawSync(0);
        }
        c = cx + (((cy + 0x1E0) << 6) + 0x10);
        p = LITP(u16 *, g_EffectSpriteInfo)[id];
        n = 0;
        p[2] = c;
        FIELD_AT(p, EspSpriteInfo, ->tpage, u8) = page;
        q = (u8 *)(p[1] * 4 + (u32)p + 9);
        do {
            n++;
            *q += y;
            q += 4;
        } while (n < p[0]);
        y += tim.ph;
        cy = ch + cy;
        i++;
        if (i >= 8) {
            break;
        }
        k = i + (start & 0xFF);
    test:;
    } while (LITP(u8, g_EffectSpriteIds)[k] != 0xFF);
    if ((start & 0xFF) == 0) {
        g_EffectTexPage = page;
        g_EffectTexPos[0] = x;
        g_EffectTexPos[1] = y;
        g_EffectClutPos[0] = cx;
        g_EffectClutPos[1] = cy;
    }
}

s8 effect_create_billboard_api(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale) {
    s8 effect_create_billboard(u8, u8, s32, s32, s32, u8);
    return effect_create_billboard(type, group, yaw, parent_mat, offset, scale);
}

/* Creates effect `type` (sprite set; `group` & 7 picks its animation) at `offset` (VECTOR) relative to
   `parent_mat` (0 = identity): a chain of n effects, one free slot each, as listed in the animation data.
   Returns the last slot used; -1 when the pool runs out (nothing when it is full from the start). */
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale) {
    s32 first;
    s32 found;
    s32 i;
    u32 j;
    u32 n;
    u8 *data;
    u8 *q;
    u8 *p;
    s32 off;

    first = 1;
    if (g_FreeEffectSlots != 0) {
        do {
            found = 1;
            i = 0x40;
            do {
                i--;
                if (LITP(EffectSlot, g_EffectPool)[i].h.pre_behavior == 0) {
                    found = 0;
                    LIT(u8, g_FreeEffectSlots)--;
                }
            } while (i != 0 && found != 0);
            if (found != 0) {
                return -1;
            }
            LITP(EffectSlot, g_EffectPool)[i].disp_x = LITP(EffectSlot, g_EffectPool)[i].disp_y = LITP(EffectSlot, g_EffectPool)[i].disp_z = 0;
            LITP(EffectSlot, g_EffectPool)[i].pos_x = LITP(EffectSlot, g_EffectPool)[i].pos_y = LITP(EffectSlot, g_EffectPool)[i].pos_z = 0;
            LITP(EffectSlot, g_EffectPool)[i].m00 = LITP(EffectSlot, g_EffectPool)[i].m01 = LITP(EffectSlot, g_EffectPool)[i].m02 = 0;
            LITP(EffectSlot, g_EffectPool)[i].m10 = LITP(EffectSlot, g_EffectPool)[i].m11 = LITP(EffectSlot, g_EffectPool)[i].m12 = 0;
            LITP(EffectSlot, g_EffectPool)[i].m20 = LITP(EffectSlot, g_EffectPool)[i].m21 = LITP(EffectSlot, g_EffectPool)[i].m22 = 0;
            LITP(EffectSlot, g_EffectPool)[i].trans_x = LITP(EffectSlot, g_EffectPool)[i].trans_y = LITP(EffectSlot, g_EffectPool)[i].trans_z = 0;
            LITP(EffectSlot, g_EffectPool)[i].depth_scaled = 0;
            LITP(EffectSlot, g_EffectPool)[i].proj_depth = 0;
            LITP(EffectSlot, g_EffectPool)[i].effect_type = type;
            LITP(EffectSlot, g_EffectPool)[i].depth_group = group;
            LITP(EffectSlot, g_EffectPool)[i].local_offset_x = ((VECTOR *)offset)->vx;
            LITP(EffectSlot, g_EffectPool)[i].local_offset_y = ((VECTOR *)offset)->vy;
            LITP(EffectSlot, g_EffectPool)[i].local_offset_z = ((VECTOR *)offset)->vz;
            LITP(EffectSlot, g_EffectPool)[i].parent_matrix = (void *)parent_mat;
            LITP(EffectSlot, g_EffectPool)[i].spawn = *(VECTOR *)offset;
            off = type * 4;
            LITP(EffectSlot, g_EffectPool)[i].clut_info = *(u8 **)(LITP(u8, g_EffectSpriteInfo) + off);
            LITP(EffectSlot, g_EffectPool)[i].frame_list = LITP(EffectSlot, g_EffectPool)[i].frame_cur = *(u8 **)(LITP(u8, g_EffectSpriteInfo) + off) + 8;
            p = *(u8 **)(LITP(u8, g_EffectSpriteInfo) + off);
            LITP(EffectSlot, g_EffectPool)[i].uv_list = LITP(EffectSlot, g_EffectPool)[i].uv_cur = (u8 *)(ESP_INFO(p)->frame_count * 4 + ({ 8; }) + (s32)p);
            LITP(EffectSlot, g_EffectPool)[i].frame_delay = LITP(EffectSlot, g_EffectPool)[i].frame_list[1];
            LITP(EffectSlot, g_EffectPool)[i].frame_index = LITP(EffectSlot, g_EffectPool)[i].frame_list[0];
            if (first != 0) {
                data = *(u8 **)(LITP(u8, g_EffectSpriteAnim) + off);
                data += data[group & 7] * 4;
                first = 0;
                n = *(s32 *)data;
            }
            q = data + 4;
            for (j = 1; j < n; j++) {
                q += *(s32 *)q * 24 + 4;
            }
            LITP(EffectSlot, g_EffectPool)[i].anim_data_base = LITP(EffectSlot, g_EffectPool)[i].anim_data_frame = (EffectStep *)(q + 4);
            LITP(EffectSlot, g_EffectPool)[i].h = *LITP(EffectSlot, g_EffectPool)[i].anim_data_frame;
            if (scale != 0) {
                LITP(EffectSlot, g_EffectPool)[i].h.scale = scale;
            }
            LITP(EffectSlot, g_EffectPool)[i].h.type = 1;
            LITP(EffectSlot, g_EffectPool)[i].h.yaw += yaw;
            n--;
            if (parent_mat == 0) {
                LITP(EffectSlot, g_EffectPool)[i].parent_matrix = &g_IdentityMatrix;
            }
        } while (n != 0);
        return i;
    }
    /* g_FreeEffectSlots == 0: falls off the end, v0 still holds 0 */
}

void update_2d_effects(void) {
    void GsMulCoord2(MATRIX *m1, MATRIX *m2);
    int GsSetRefView2(GsRVIEW2 *pv);
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    void effect_actor_update_and_render(void);
    void effect_draw_mirror_reflection(void);
    GsRVIEW2 view;

    g_ActiveEffectIndex = 0x40;
    do {
        LIT(s8, g_ActiveEffectIndex) -= 1;
        if (EFFECT_AT(EFFECT_CUR_OFF, pre_behavior, u8) != 0) {
            LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
            effect_actor_update_and_render();
            if (g_GameStateFlags & GAME_MIRROR_ON) {
                mirror_reflect_view(ROOM_CAMERA_VIEW_ADDR(LIT(u8, g_CutId)), &view,
                                (u32)(g_GameStateFlags & GAME_MIRROR_AXIS) >> 1, g_MirrorPlane);
                GsSetRefView2(&view);
                LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
                LITP(MATRIX, sp_matrix)->m[0][0] *= -1;
                GsMulCoord2(LITP(MATRIX, sp_matrix), &g_WorldScreenMatrix);
                effect_draw_mirror_reflection();
                GsSetRefView2((GsRVIEW2 *)ROOM_CAMERA_VIEW_ADDR(LIT(u8, g_CutId)));
            }
        }
    } while (g_ActiveEffectIndex != 0);
}

typedef struct {
    /* 0x00 */ u8 pre_behavior;
    /* 0x01 */ u8 post_behavior;
    /* 0x02 */ u8 type;
    /* 0x03 */ u8 scale;
    /* 0x04 */ u8 pad4[4];
    /* 0x08 */ u16 accel_x;
    /* 0x0A */ u16 accel_y;
    /* 0x0C */ u16 accel_z;
    /* 0x0E */ u16 draw_flags;
    /* 0x10 */ u16 vel_x;
    /* 0x12 */ u16 vel_y;
    /* 0x14 */ u16 vel_z;
    /* 0x16 */ s16 yaw;
    /* 0x18 */ u16 disp_x;
    /* 0x1A */ u16 disp_y;
    /* 0x1C */ u16 disp_z;
    /* 0x1E */ u16 pad1E;
    /* 0x20 */ u16 pos_x;
    /* 0x22 */ u16 pos_y;
    /* 0x24 */ u16 pos_z;
    /* 0x26 */ u8 pad26;
    /* 0x27 */ u8 depth_group;
    /* 0x28 */ u16 local_offset_x;
    /* 0x2A */ u16 local_offset_y;
    /* 0x2C */ u16 local_offset_z;
    /* 0x2E */ u16 depth_scaled;
    /* 0x30 */ MATRIX matrix;
    /* 0x50 */ s32 proj_depth;
    /* 0x54 */ u8 pad54[0x10];
    /* 0x64 */ MATRIX *parent_matrix;
    /* 0x68 */ u16 *clut_info;
    /* 0x6C */ u8 pad6C[4];
    /* 0x70 */ u8 *frame_cur;
    /* 0x74 */ u8 pad74[4];
    /* 0x78 */ u8 *uv_cur;
    /* 0x7C */ u8 pad7C[8];
} EffectActor;
typedef struct {
    u8 pad0[0x9C];
    struct {
        MATRIX m;
        s32 fov;
        u8 pad24[8];
    } arr[1];
} RoomCameraView;

void effect_actor_update_and_render(void) {
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    void SetRotMatrix(MATRIX *m);
    void SetTransMatrix(MATRIX *m);
    long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
    extern void (*g_EffectBehaviorTable_l[])(void) __asm__("g_EffectBehaviorTable");
    extern RoomCameraView *g_RoomData_l __asm__("g_RoomData");
    void GsSortSprite_l(GsSPRITE *, GsOT *, u16) __asm__("GsSortSprite");
    void effect_animate_sprite(void);
    s32 is_point_in_quad_l(VECTOR *, s32) __asm__("is_point_in_quad");
    SVECTOR sxy;
    VECTOR pos;
    u8 *spr;
    u16 t;
    s32 q;
    u32 z;

    if (g_UpdateFlags & 8) {
        g_EffectBehaviorTable_l[LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].pre_behavior]();
    }
    if (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags & 2) {
        RotMatrixY(LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].yaw, LITP(MATRIX, sp_matrix));
        ApplyMatrixSV(LITP(MATRIX, sp_matrix), (SVECTOR *)&LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].disp_x, LITP(SVECTOR, sp_svec));
        LITP(SVECTOR, sp_svec)->vx += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].local_offset_x;
        LITP(SVECTOR, sp_svec)->vy += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].local_offset_y;
        LITP(SVECTOR, sp_svec)->vz += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].local_offset_z;
        if (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].type == 1) {
            LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].matrix = *LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].parent_matrix;
        }
        ApplyMatrix(&LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].matrix, LITP(SVECTOR, sp_svec), LITP(VECTOR, sp_vec));
        LITP(SVECTOR, sp_svec)->vx = LITP(VECTOR, sp_vec)->vx + LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].matrix.t[0];
        LITP(SVECTOR, sp_svec)->vy = LITP(VECTOR, sp_vec)->vy + LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].matrix.t[1];
        LITP(SVECTOR, sp_svec)->vz = LITP(VECTOR, sp_vec)->vz + LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].matrix.t[2];
        if (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags & 4) {
            LITP(SVECTOR, sp_svec)->vy = 0;
        }
        if (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags & 8) {
            LITP(SVECTOR, sp_svec)->vy = g_PlayerEntity.floor_y;
        }
        LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].pos_x = LITP(SVECTOR, sp_svec)->vx;
        LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].pos_y = LITP(SVECTOR, sp_svec)->vy;
        LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].pos_z = LITP(SVECTOR, sp_svec)->vz;
        pos.vx = LITP(SVECTOR, sp_svec)->vx;
        pos.vy = LITP(SVECTOR, sp_svec)->vy;
        pos.vz = LITP(SVECTOR, sp_svec)->vz;
        SetTransMatrix(&g_WorldScreenMatrix);
        SetRotMatrix(&g_WorldScreenMatrix);
        LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].proj_depth = RotTransPers(LITP(SVECTOR, sp_svec), (long *)&sxy, LITP(long, sp_tmp00), LITP(long, sp_tmp00)) & 0xFFFF;
        LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].depth_scaled = *(u16 *)&LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].proj_depth * 4;
        sxy.vx -= 0xA0;
        sxy.vy -= (g_ActiveBuffer == 0) ? 0x168 : 0x78;
        if (g_UpdateFlags & 8) {
            EFFECT_AT(EFFECT_CUR_OFF, vel_x, u16) += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].accel_x;
            LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].vel_y += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].accel_y;
            LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].vel_z += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].accel_z;
            EFFECT_AT(EFFECT_CUR_OFF, disp_x, u16) += EFFECT_AT(EFFECT_CUR_OFF, vel_x, u16);
            LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].disp_y += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].vel_y;
            LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].disp_z += LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].vel_z;
        }
    }
    if (g_UpdateFlags & 8) {
        g_EffectBehaviorTable_l[LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].post_behavior]();
    }
    if ((LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags & 1) && (g_UpdateFlags & 8)) {
        effect_animate_sprite();
    }
    if (!(LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags & 0x8000) && is_point_in_quad_l(&pos, (s32)g_CameraSwitchZones) != 0) {
        spr = LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].uv_cur;
        t = LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].clut_info[2];
        LITP(GsSPRITE, sp_sprite)->attribute = ((LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].draw_flags >> 6) << 22) | 0x40;
        LITP(GsSPRITE, sp_sprite)->w = LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].frame_cur[2];
        LITP(GsSPRITE, sp_sprite)->h = LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].frame_cur[3];
        LITP(GsSPRITE, sp_sprite)->mx = 0x80 - spr[2];
        LITP(GsSPRITE, sp_sprite)->my = 0x80 - spr[3];
        LITP(GsSPRITE, sp_sprite)->x = sxy.vx;
        LITP(GsSPRITE, sp_sprite)->y = sxy.vy;
        LITP(GsSPRITE, sp_sprite)->tpage = LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].clut_info[3];
        LITP(GsSPRITE, sp_sprite)->u = spr[0];
        LITP(GsSPRITE, sp_sprite)->v = spr[1];
        LITP(GsSPRITE, sp_sprite)->cx = (t & 0x3F) * 16;
        LITP(GsSPRITE, sp_sprite)->cy = (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].depth_group >> 3) + (t >> 6);
        q = (LITP(GsSPRITE, sp_sprite)->w << 8) * (g_RoomData_l->arr[g_CutId].fov * LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].scale) / (LITP(EffectActor, g_EffectPool)[LIT(u8, g_ActiveEffectIndex)].depth_scaled + 1);
        if ((u32)q > 0x7FFF) {
            LITP(GsSPRITE, sp_sprite)->scalex = LITP(GsSPRITE, sp_sprite)->scaley = -0x8000;
        } else {
            LITP(GsSPRITE, sp_sprite)->scalex = LITP(GsSPRITE, sp_sprite)->scaley = q;
        }
        z = (u32)LITP(EffectActor, g_EffectPool)[g_ActiveEffectIndex].proj_depth >> 4;
        if (z < 0x400) {
            GsSortSprite_l(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OtMain), z);
        }
    }
}

void effect_draw_mirror_reflection(void) {
    void SetRotMatrix(MATRIX *m);
    void SetTransMatrix(MATRIX *m);
    long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 is_point_in_quad_l() __asm__("is_point_in_quad");
    DVECTOR sxy[2];
    VECTOR v;
    u32 z;
    u16 zz;
    u8 *a2;
    u16 a3;
    s32 scale;
    u32 otz;

    if (EFFECT->draw_flags & 2) {
        v.vx = EFFECT->pos_x;
        v.vy = EFFECT->pos_y;
        v.vz = EFFECT->pos_z;
        SetTransMatrix(&g_WorldScreenMatrix);
        SetRotMatrix(&g_WorldScreenMatrix);
        z = 0xFFFF & RotTransPers((SVECTOR *)&*(u8 *)&EFFECT->pos_x, (long *)&sxy[0], LITP(long, sp_tmp00), LITP(long, sp_tmp00));
        sxy[0].vx -= 0xA0;
        zz = z * 4;
        sxy[0].vy -= (g_ActiveBuffer == 0) ? 0x168 : 0x78;
    }
    if (!(EFFECT->draw_flags & 0x8000)
        && mirror_point_visible(ROOM_CAMERA_VIEW_ADDR(LIT(u8, g_CutId)), (u32)(g_GameStateFlags & GAME_MIRROR_AXIS) >> 1, &v) != 0
        && is_point_in_quad_l(&v, g_CameraSwitchZones) != 0) {
        a2 = ((u8 *) EFFECT->uv_cur);
        a3 = ESP_INFO(EFFECT->clut_info)->clut;
        LITP(GsSPRITE, sp_sprite)->attribute = ((EFFECT->draw_flags >> 6) << 22) | 0x40;
        LITP(GsSPRITE, sp_sprite)->w = EFF_FRAME(EFFECT->frame_cur)->w;
        LITP(GsSPRITE, sp_sprite)->h = EFF_FRAME(EFFECT->frame_cur)->h;
        LITP(GsSPRITE, sp_sprite)->mx = 0x80 - a2[2];
        LITP(GsSPRITE, sp_sprite)->my = 0x80 - a2[3];
        LITP(GsSPRITE, sp_sprite)->x = sxy[0].vx;
        LITP(GsSPRITE, sp_sprite)->y = sxy[0].vy;
        LITP(GsSPRITE, sp_sprite)->tpage = ESP_INFO(EFFECT->clut_info)->tpage;
        LITP(GsSPRITE, sp_sprite)->u = a2[0];
        LITP(GsSPRITE, sp_sprite)->v = a2[1];
        LITP(GsSPRITE, sp_sprite)->cx = (a3 & 0x3F) << 4;
        LITP(GsSPRITE, sp_sprite)->cy = (EFFECT->depth_group >> 3) + (a3 >> 6);
        scale = (LITP(GsSPRITE, sp_sprite)->w << 8)
              * (RDT_CAM(LIT(s32, g_RoomData) + ({ LIT(u8, g_CutId) * 0x2C; }) + 0x94)->fov
                 * EFFECT->scale)
              / (zz + 1);
        if ((u32)scale > 0x7FFF) {
            LITP(GsSPRITE, sp_sprite)->scalex = LITP(GsSPRITE, sp_sprite)->scaley = -0x8000;
        } else {
            LITP(GsSPRITE, sp_sprite)->scalex = LITP(GsSPRITE, sp_sprite)->scaley = scale;
        }
        otz = z >> 4;
        if (otz < 0x400) {
            GsSortSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OtMain) + g_ActiveBuffer, otz);
        }
    }
}



void effect_animate_sprite(void) {
    u8 *q;
    u8 a;
    u8 b;

    if (EFFECT->frame_delay == 0) {
        q = ((u8 *) EFFECT->frame_cur);
        q += 4;
        EFFECT->frame_index++;
        a = q[0];
        b = q[1];
        if ((a == 0) & (b == 0)) {
            if (EFFECT_AS(pre_behavior, u16) != 0) {
                LIT(u8, g_FreeEffectSlots)++;
                EFFECT->pre_behavior = EFFECT->post_behavior = 0;
            }
            return;
        }
        if (b == 0xFF) {
            q = ((u8 *) EFFECT->frame_list) + a * 4;
            EFFECT->frame_index = q[0];
        }
        EFFECT->frame_delay = q[1];
        EFFECT->uv_cur =
            (s32) (((u8 *) EFFECT->uv_list) + q[0] * 4);
        EFFECT->frame_cur = (s32) q;
    }
    LITP(Effect, g_EffectPool)[g_ActiveEffectIndex].frame_delay--;
}

void memclr(u32 *p, s32 n) {
    while (n) {
        *p++ = 0;
        n--;
    }
}

void effect_free_matching(u8 mask, u8 a, u8 b, u8 c, s32 d) {
    s32 i;
    u8 m;

    i = 64;
    do {
        i--;
        m = 0;
        if (mask & 1) {
            m = LITP(Effect, g_EffectPool)[i].effect_type == a;
        }
        if ((mask & 2) && LITP(Effect, g_EffectPool)[i].depth_group == b) {
            m |= 2;
        }
        if ((mask & 4) && LITP(Effect, g_EffectPool)[i].hdr06 == c) {
            m |= 4;
        }
        if ((mask & 8) && LITP(Effect, g_EffectPool)[i].parent_matrix == d) {
            m |= 8;
        }
        if (m == mask) {
            LIT(u8, g_FreeEffectSlots)++;
            LITP(Effect, g_EffectPool)[i].post_behavior = 0;
            LITP(Effect, g_EffectPool)[i].pre_behavior = 0;
        }
    } while (i != 0);
}
