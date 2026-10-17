/* wasp.c (stage4) - Wasp: nest, take-off, hover, sting, damage / death, and the player's crawling
   animation. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* wasp data */
extern s32 wasp_sca_info_ptr;

/* WaspWork: the wasp's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). WASP(e)->f, WASP_FIELD(e, f) and WASP_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct WaspWork {
    /* +0x00 */ s16 chase_frames;    /* 0x16C: frames flying in wasp_state_run (+1 each); from 0x1D on it steers toward the player; cleared at init and when the hover attack starts */
    /* +0x02 */ u16 path_flags;      /* 0x16E: bit 0 = api_entity_pathfind_update result bit 0 (wasp_behavior_dispatch); needed for the sting, gates turning in victory */
    /* +0x04 */ s16 player_touch;    /* 0x170: api_check_entity_collision result (wasp_update); starts the hover attack */
    /* +0x06 */ u8 pad06[4];
    /* +0x0A */ u16 bob_speed;       /* 0x176: vertical bob step added/subtracted to pos_y; 0x23, +1 per frame, wraps back to 0x23 at 0x97 */
    /* +0x0C */ u16 player_distance; /* 0x178: |dx|+|dz| to the player; sets speed (dist/100 + 0xC8) and the attack/nest ranges */
    /* +0x0E */ s16 room_hit;        /* 0x17A: api_check_room_collision result (wasp_update), tested in wasp_state_run */
    /* +0x10 */ u16 climbing;        /* 0x17C: 1 = pos_y -= bob_speed (rise), 0 = sink; flipped at height limits in hover/victory */
    /* +0x12 */ s16 timer;           /* 0x17E: general countdown: nest spawn delay (0x5A), on-floor time when downed (0x1E; 0x14 marks a big wasp), death fade (0x14) */
    /* +0x14 */ s16 anim_done;       /* 0x180: api_joint_move result (wasp_animate); the sting waits for it */
    /* +0x16 */ u8 is_big;           /* 0x182: big-wasp flag (wasp_state_init): bigger billboard/shadow, more health, sting damage (is_big+1)*10, gibs on death */
    /* +0x17 */ u8 pad17[1];
    /* +0x18 */ u8 respawn_count;    /* 0x184: +1 each respawn from the nest (wasp_state_death); the 10th comes back as behavior 0x12, wraps at 11 */
} WaspWork;
#define WASP(e) ENEMY_WORK(e, WaspWork)
#define WASP_FIELD(e, f) ENEMY_FIELD(e, WaspWork, f)
#define WASP_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, WaspWork, f, T)
#define WASP_MEMBER(e, f) ENEMY_MEMBER(e, WaspWork, f)

/* state 0: sets up a new wasp (health, sca info, animation, work fields), then state 1 */
void wasp_state_init(void) {

    ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 0x101;
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->status_flags = 1;
    WASP_FIELD(g_CurrentEntity, player_touch) = 0;
    WASP_FIELD(g_CurrentEntity, chase_frames) = 0;
    WASP_FIELD(g_CurrentEntity, timer) = 0x5A;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    g_CurrentEntity->sca_info = (s32) wasp_sca_info_ptr;
    g_CurrentEntity->scale = 0;
    if (g_CurrentEntity->pos_y == 0) {
        g_CurrentEntity->pos_y = -0xFA0;
    }
    WASP_FIELD(g_CurrentEntity, is_big) = 0U;
    if (((u8) g_CurrentEntity->behavior_flags >> 1) & 1 & 0xFF) {
        g_CurrentEntity->scale = 0x2000;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags + 0xFE);
        WASP_FIELD(g_CurrentEntity, is_big) = 1U;
    }
    if (g_CurrentEntity->behavior_flags == 0x10) {
        g_CurrentEntity->action_ticks_counter = (s16) (0x320 - (((g_ApiCallPtr = api_rand)() & 9) * 0x1E));
    }
    LIT(s32, sp_tmp1C) = 0x606060;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, (WASP_FIELD(g_CurrentEntity, is_big) * 0x64) + 0x64, (WASP_FIELD(g_CurrentEntity, is_big) * 0x64) + 0x64);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (s16) ((0xA + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7) + (WASP_FIELD(g_CurrentEntity, is_big) * 0xA)) * 2);
}

/* every frame for each wasp (enemies_update_functions_tbl): runs wasp_state_tbl[state], then collision checks and
   the shadow / drawing */
void wasp_update(void) {
    extern s32 (*wasp_state_tbl[])();

    if (g_UpdateFlags & 4 & 0xFFFF) {
        wasp_state_tbl[g_CurrentEntity->state]();
        if ((g_CurrentEntity->pos_y != 0) && (g_CurrentEntity->action_behavior != 0) && (g_CurrentEntity->action_behavior != 6)) {
            (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
            WASP_FIELD(g_CurrentEntity, player_touch) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
            (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            WASP_FIELD(g_CurrentEntity, room_hit) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
        }
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void wasp_state_run(void) {
    void wasp_behavior_dispatch();
    void *joints;

    joints = ((void *) g_CurrentEntity->joints);
    if ((g_PlayerEntity.health < 0) && (g_CurrentEntity->action_behavior != 4)) {
        WASP_FIELD_AS(g_CurrentEntity, bob_speed, s16) = 0x23;
        WASP_FIELD_AS(g_CurrentEntity, climbing, s16) = 1;
        g_CurrentEntity->move_speed_current = (u16) ((WASP_FIELD(g_CurrentEntity, player_distance) / 100) + 0xC8);
        g_CurrentEntity->sub_state = 0U;
        ENTITY_ACTION_U16(g_CurrentEntity) = 4;
    }
    if (WASP_FIELD_AS(g_CurrentEntity, room_hit, u16) != 0) {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x80));
    }
    WASP_FIELD(g_CurrentEntity, player_distance) = (u16) ((__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + (__builtin_abs(g_PlayerEntity.pos_z - (g_CurrentEntity->pos_z))));
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        WASP_FIELD_AS(g_CurrentEntity, chase_frames, u16)++;
        g_CurrentEntity->move_speed_current = (u16) (((u16) g_CurrentEntity->move_speed_current) + 2);
        if ((s16) WASP_FIELD_AS(g_CurrentEntity, chase_frames, u16) >= 0x1D) {
            if ((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x100) != 0) {
                g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x80));
                g_CurrentEntity->move_speed_current = (u16) (g_CurrentEntity->move_speed_current - 8);
            }
        }
        if ((s16) ((u16) g_CurrentEntity->move_speed_current) < 0x1E) {
            g_CurrentEntity->move_speed_current = (u16) ((WASP_FIELD(g_CurrentEntity, player_distance) / 100) + 0xC8);
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle - (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x200));
        }
        if ((s16) ((u16) g_CurrentEntity->move_speed_current) >= 0x191) {
            g_CurrentEntity->move_speed_current = 0x190U;
        }
        /* fallthrough */
    case 1:
        wasp_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    if (g_CurrentEntity->pos_y < -0xFA0) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_alert_range)((void *)0xFA0);
    }
    if (g_CurrentEntity->pos_y >= -0x3E7) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_visual_range)((void *)0xFA0);
    }
    LIT(s32, sp_tmp0C) = ((s32) JOINT(joints)->world.t[1] >> 4) + 0x12C + (WASP_FIELD(g_CurrentEntity, is_big) * 0x64);
    if (LIT(s32, sp_tmp0C) < 0) {
        LIT(s32, sp_tmp0C) = (s32) ((WASP_FIELD(g_CurrentEntity, is_big) * 0x64) + 0x64);
    }
    (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, LIT(s32, sp_tmp0C), LIT(s32, sp_tmp0C));
}

void wasp_behavior_dispatch(void) {
    extern s32 (*wasp_behavior_tbl[])();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        WASP_FIELD(g_CurrentEntity, path_flags) = (u16) (((s16) WASP_FIELD(g_CurrentEntity, path_flags)) & ~1);
        WASP_FIELD(g_CurrentEntity, path_flags) = (u16) (WASP_FIELD(g_CurrentEntity, path_flags) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    LIT(s32, sp_tmp18) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100);
    LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200);
    wasp_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void wasp_behavior_nest(void) {
    void wasp_animate();

    g_CurrentEntity->hit_state = 1;
    g_CurrentEntity->animation_id = 0;
    wasp_animate();
    if (g_CurrentEntity->behavior_flags & 0xF0 & 0xFF) {
        if ((u8) g_CurrentEntity->behavior_flags >= 0x20U) {
            g_CurrentEntity->hit_state = 0;
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 2;
            WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23;
            WASP_FIELD(g_CurrentEntity, climbing) = 1;
            g_CurrentEntity->move_speed_current = (s16) ((WASP_FIELD(g_CurrentEntity, player_distance) / 100) + 0xC8);
            return;
        }
        if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
            g_CurrentEntity->move_speed_current = 0x1E;
            g_CurrentEntity->action_ticks_counter = 0U;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
            return;
        }
                if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) && ((u16) WASP_FIELD(g_CurrentEntity, player_distance) >= 0xFA0U)) {

        } else {
            g_CurrentEntity->behavior_flags = 0x11U;
        }
        return;
    }
    if (((g_CurrentEntity->behavior_flags == 0) && ((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x1388U)) || ((g_CurrentEntity->behavior_flags != 0) && (--WASP_FIELD(g_CurrentEntity, timer) != -1))) {
        g_CurrentEntity->hit_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        WASP_FIELD(g_CurrentEntity, timer) = 0x5AU;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->move_speed_current = 0x32;
    }
}

void wasp_behavior_takeoff(void) {
    void wasp_animate();

    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->move_speed_current = (u16) ((g_CurrentEntity->move_speed_current) + 1);
    wasp_animate();
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x55);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800 - ((g_CurrentEntity->animation_frame_id) * 0x55));
    if (g_CurrentEntity->animation_frame_id == 0x18) {
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23;
        WASP_FIELD(g_CurrentEntity, climbing) = 1;
    }
}

void wasp_behavior_hover(void) {
    void wasp_animate();
    void *joints;

    joints = ((void *) g_CurrentEntity->joints);
    FIELD_AT(joints, Joint, [1].flags, s8) = 0;
    FIELD_AT(joints, Joint, [2].flags, s8) = 0;
    FIELD_AT(joints, Joint, [3].flags, s8) = 0;
    FIELD_AT(joints, Joint, [4].flags, s8) = 0;
    g_CurrentEntity->animation_id = 2;
    wasp_animate();
    if (!((g_CurrentEntity->animation_frame_id % 7) & 0xFF)) {
        FIELD_AT(joints, Joint, [1].flags, s8) = 3;
        FIELD_AT(joints, Joint, [2].flags, s8) = 3;
        FIELD_AT(joints, Joint, [3].flags, s8) = 3;
        FIELD_AT(joints, Joint, [4].flags, s8) = 3;
        if ((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x3E8U) {
            (g_ApiCallPtr = api_sound_play_entity_se)(0U, 0);
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
    FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->bob_speed, u16) = (FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->bob_speed, u16) + 1);
    if ((s16) WASP_FIELD(g_CurrentEntity, bob_speed) >= 0x97) {
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23U;
    }
    if ((u32) g_CurrentEntity->pos_y < (u32) (-0xDAC - (((g_ApiCallPtr = api_rand)() & 1) * 0x1F4))) {
        WASP_FIELD(g_CurrentEntity, climbing) = 0U;
    }
    if ((u32) g_CurrentEntity->pos_y > (u32) (-0x5DC - (((g_ApiCallPtr = api_rand)() & 1) * 0x1F4))) {
        WASP_FIELD(g_CurrentEntity, climbing) = 1U;
    }
    if (WASP_FIELD(g_CurrentEntity, climbing) != 0) {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - (s16) WASP_FIELD(g_CurrentEntity, bob_speed));
    } else {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (s16) WASP_FIELD(g_CurrentEntity, bob_speed));
    }
    if ((WASP_FIELD(g_CurrentEntity, player_touch) != 0) && ((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x3E8U) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && ((u32) (-g_CurrentEntity->pos_y - 0x8FC) < 0x258U) && (WASP_FIELD(g_CurrentEntity, chase_frames) = 0, FIELD_AT(joints, Joint, [1].flags, s8) = 3, FIELD_AT(joints, Joint, [2].flags, s8) = 3, FIELD_AT(joints, Joint, [3].flags, s8) = 3, FIELD_AT(joints, Joint, [4].flags, s8) = 3, g_CurrentEntity->sub_state = 0, ENTITY_ACTION_S16(g_CurrentEntity) = 2, WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23U, WASP_FIELD(g_CurrentEntity, climbing) = 1U, g_CurrentEntity->move_speed_current = (s16) ((WASP_FIELD(g_CurrentEntity, player_distance) / 100) + 0xC8), (((u8) g_PlayerEntity.hit_state) == 0)) && ((s16) (((s16) WASP_FIELD(g_CurrentEntity, path_flags)) & 1) != 0) && (g_CurrentEntity->sub_state = 1, ENTITY_ACTION_S16(g_CurrentEntity) = 3, ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0)) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0xC0) == 0)) {
        if (FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->is_big, u8) != 0) {
            return;
        }
        ENTITY_ACTION_S16(g_CurrentEntity) = 5;
        g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
        g_PlayerEntity.hit_state = 1;
        g_CurrentEntity->hit_state = 1;
        return;
    }
}

void wasp_behavior_sting(void) {
    void wasp_animate();

    g_CurrentEntity->animation_id = 3;
    g_CurrentEntity->angle = ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x30));
    wasp_animate();
    if (WASP_FIELD(g_CurrentEntity, anim_done) != 0) {
        if ((((u8) g_PlayerEntity.hit_state) == 0) && ((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x5DCU)) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && ((s16) (((s16) WASP_FIELD(g_CurrentEntity, path_flags)) & 1) != 0)) {
                g_PlayerEntity.hit_state = 1;
                g_PlayerEntity.action_behavior = 0x64;
                (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
                g_PlayerEntity.health -= (WASP_FIELD(g_CurrentEntity, is_big) + 1) * 0xA;
                if ((s16) g_PlayerEntity.health < 0) {
                    g_PlayerEntity.health = 1;
                    if (WASP_FIELD(g_CurrentEntity, is_big) != 0) {
                        (*(s16 *)&g_PlayerEntity.state) = 3;
                        *(s16 *)&g_PlayerEntity.action_behavior = 0xC8;
                        g_PlayerEntity.health = -1;
                        return;
                    }
                }
            }
        }
        g_CurrentEntity->move_speed_current = 0x64;
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
}

void wasp_behavior_victory(void) {
    void wasp_animate();
    void *joints;

    joints = ((void *) g_CurrentEntity->joints);
    FIELD_AT(joints, Joint, [1].flags, s8) = 0;
    FIELD_AT(joints, Joint, [2].flags, s8) = 0;
    FIELD_AT(joints, Joint, [3].flags, s8) = 0;
    FIELD_AT(joints, Joint, [4].flags, s8) = 0;
    g_CurrentEntity->animation_id = 2;
    g_CurrentEntity->angle += (((FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->path_flags, s16) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10U)) * 0x2);
    wasp_animate();
    if (!((g_CurrentEntity->animation_frame_id % 7) & 0xFF)) {
        FIELD_AT(joints, Joint, [1].flags, s8) = 3;
        FIELD_AT(joints, Joint, [2].flags, s8) = 3;
        FIELD_AT(joints, Joint, [3].flags, s8) = 3;
        FIELD_AT(joints, Joint, [4].flags, s8) = 3;
    }
    FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->bob_speed, u16) = (FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->bob_speed, u16) + 1);
    if ((s16) WASP_FIELD(g_CurrentEntity, bob_speed) >= 0x97) {
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23U;
    }
    if ((u32) ((u32) g_CurrentEntity->pos_y) < (u32) (-0xDAC - (((g_ApiCallPtr = api_rand)() & 1) * 0x1F4))) {
        WASP_FIELD(g_CurrentEntity, climbing) = 0U;
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23U;
    }
    if ((u32) g_CurrentEntity->pos_y > (u32) (-0x5DC - (((g_ApiCallPtr = api_rand)() & 1) * 0x1F4))) {
        WASP_FIELD(g_CurrentEntity, climbing) = 1U;
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23U;
    }
    if (WASP_FIELD(g_CurrentEntity, climbing) != 0) {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - (s16) WASP_FIELD(g_CurrentEntity, bob_speed));
    } else {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (s16) WASP_FIELD(g_CurrentEntity, bob_speed));
    }
    if ((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x7D0U) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x400U);
        return;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
}

void wasp_behavior_grab_thunk(void) {
    void wasp_behavior_grab();
    wasp_behavior_grab();
}

void wasp_behavior_emerge(void) {
    void wasp_animate();

    g_CurrentEntity->animation_id = 2;
    g_CurrentEntity->move_speed_current = (u16) ((g_CurrentEntity->move_speed_current) + 1);
    wasp_animate();
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0x14);
    if (g_CurrentEntity->pos_y >= -0xAEF) {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0x14);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        g_CurrentEntity->action_ticks_counter++;
        if ((s16) g_CurrentEntity->action_ticks_counter >= 0x15) {
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x3C);
        }
    }
    if ((s16) g_CurrentEntity->action_ticks_counter >= 0x1A) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
        WASP_FIELD(g_CurrentEntity, bob_speed) = 0x23;
        WASP_FIELD(g_CurrentEntity, climbing) = 1;
    }
}

void wasp_animate(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        WASP_FIELD(g_CurrentEntity, anim_done) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    }
}

void wasp_behavior_grab(void) {
    s32 unused;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->angle = (u16) g_PlayerEntity.angle;
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->animation_id = (s8) ((g_PlayerEntity.id & 1) + 4);
        g_CurrentEntity->anim_base_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->anim_base_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_PlayerEntity.anim_base_x = (*(u16 *)&g_PlayerEntity.pos_x);
        g_PlayerEntity.anim_base_z = (*(u16 *)&g_PlayerEntity.pos_z);
        g_PlayerEntity.hit_state = 1;
        (*(s16 *)&g_PlayerEntity.state) = 0x705;
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        g_CurrentEntity->death_timer++;
        if (g_CurrentEntity->animation_frame_id == 4) {
            (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, 0, 0, ADDR_g_PlayerPosX);
        }
        if (g_CurrentEntity->animation_frame_id == (((g_PlayerEntity.id & 1 & 0xFF) * 2) + 0x22)) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        g_CurrentEntity->health = -0x2C;
        if ((g_ApiCallPtr = api_rand)() & 1) {
            g_PlayerEntity.collision_flags |= 2;
            g_PlayerWork.poison_timer = 0x96;
        }
        g_PlayerEntity.health -= 0xF;
        if ((s16) g_PlayerEntity.health < 0) {
            g_PlayerEntity.health = 1;
        }
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 3;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        return;
    }
}

void wasp_state_damaged(void) {
    void *joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0x1E;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0, 0);
        joints = (void *)g_CurrentEntity->joints;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x11, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        if (WASP_FIELD(g_CurrentEntity, is_big) != 0) {
            LIT(s32, sp_tmp1C) = 4;
            while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
                (g_ApiCallPtr = api_joint_setup_attack_effect)((LIT(s32, sp_tmp1C) * 0x7C) + 0x7C + joints, 0x1E, 0xA, 3);
            }
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)2, 0);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0xC8);
        if (g_CurrentEntity->pos_y >= -0x63) {
            g_CurrentEntity->pos_y = 0;
            g_CurrentEntity->action_state = 2U;
            WASP_FIELD(g_CurrentEntity, timer) = 0x1EU;
            (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, 0xFF3030);
        }
        return;
    case 2:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 8, 8);
        if (--FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->timer, s16) == -1) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            g_CurrentEntity->hit_state = 0;
            if (WASP_FIELD(g_CurrentEntity, is_big) != 0) {
                WASP_FIELD(g_CurrentEntity, timer) = 0x14U;
            }
        }
        return;
    case 3:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_visual_range)((void *)0xFA0);
        WASP_FIELD(g_CurrentEntity, player_distance) = (u16) ((__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + (__builtin_abs(g_PlayerEntity.pos_z - (g_CurrentEntity->pos_z))));
        if (((u16) WASP_FIELD(g_CurrentEntity, player_distance) < 0x190U) && (g_PlayerEntity.move_speed_current != 0)) {
            g_CurrentEntity->hit_state = 1;
            g_CurrentEntity->health = -4;
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)2, 0);
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 3;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
            (g_ApiCallPtr = api_pad_vibrate_small)((void *)1, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)((void *)4, 0x5A, 0);
        }
    }
}

void wasp_state_death(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 4U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0x1E;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)1, 0U);
        joints = g_CurrentEntity->joints;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x11U, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x11U, 0, JOINT_N_WORLD(joints, 9), ADDR_sp_vec, 0);
        if ((WASP_FIELD(g_CurrentEntity, is_big) != 0) || ((u32) (((u8) g_CurrentEntity->hit_state >> 3) & 0xFF) >= 7U)) {
            LIT(s32, sp_tmp1C) = 0xA;
            while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
                (g_ApiCallPtr = api_joint_setup_attack_effect)((LIT(s32, sp_tmp1C) * 0x7C) + joints, 0x1EU, 0xA, 3);
            }
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)2, 0U);
        }
        if (WASP_FIELD(g_CurrentEntity, timer) == 0x14) {
            g_CurrentEntity->action_state = 1U;
            return;
        }
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0U, 0);
        if ((WASP_FIELD(g_CurrentEntity, is_big) != 0) || ((u32) (((u8) g_CurrentEntity->hit_state >> 3) & 0xFF) >= 7U)) {
            g_CurrentEntity->action_state = 4U;
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
            return;
        }
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x11U, 0, JOINT_N_WORLD(joints, 7), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x11U, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
        g_CurrentEntity->action_state = 2U;
        FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->timer, s16) = 0x14;
        return;
    case 1:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 8U, 8);
        if (--FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->timer, s16) == -1) {
            g_CurrentEntity->action_state = 4U;
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        }
        return;
    case 2:
        g_CurrentEntity->scale = 1;
        if (--FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->timer, s16) == -1) {
            g_CurrentEntity->behavior_flags = 0x10;
            if ((u8) WASP_FIELD(g_CurrentEntity, respawn_count) >= 0xBU) {
                WASP_FIELD(g_CurrentEntity, respawn_count) = 0U;
            }
            if (WASP_FIELD(g_CurrentEntity, respawn_count) == 0xA) {
                g_CurrentEntity->behavior_flags = 0x12;
            }
            FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->respawn_count, u8) = (FIELD_AT((u8 *)g_CurrentEntity + 0x16C, WaspWork, ->respawn_count, u8) + 1);
            g_CurrentEntity->angle = 0x400;
            g_CurrentEntity->pos_x = (s32) (0x4650 - (((g_ApiCallPtr = api_rand)() & 1) * 0x64));
            g_CurrentEntity->pos_y = (s32) (-0xFA0 - (((g_ApiCallPtr = api_rand)() & 1) * 0x64));
            g_CurrentEntity->pos_z = (s32) (0x5974 - (((g_ApiCallPtr = api_rand)() & 1) * 0x64));
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(0, 0, 0, 0);
        }
        return;
    }
}

void player_anim_crawling(void) {
    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0;
        (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        if ((g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            (*(s16 *)&g_PlayerEntity.state) = 1;
            ENTITY_ACTION_S16(&g_PlayerEntity) = 0;
            g_PlayerEntity.hit_state = 0;
        }
        return;
    }
}
