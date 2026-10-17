/* adder.c (stage3) - Adder (small snake): ground and ceiling ambush, chase, bite, damage and death. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* adder data */
extern s32 adder_sca_info_ptr;

/* AdderWork: the adder's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). ADDER(e)->f, ADDER_FIELD(e, f) and ADDER_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct AdderWork {
    /* +0x00 */ u16 path_flags;        /* 0x16C: bit 0 = api_entity_pathfind_update result bit 0 (clear line to player); gates turning in chase and the bite */
    /* +0x02 */ s16 player_touch;      /* 0x16E: api_check_entity_collision result (touching the player); triggers behavior 3 in adder_decide_ground, then cleared */
    /* +0x04 */ u16 player_distance;   /* 0x170: |dx|+|dz| to the player (adder_decide_action), 0x1387 when the player is on another floor */
    /* +0x06 */ s16 room_hit;          /* 0x172: api_check_room_collision result (adder_update); non-zero counts up wall_stuck_frames */
    /* +0x08 */ s16 collision_enabled; /* 0x174: 1 = adder_update runs the collision block; cleared for hidden/ceiling snakes, set again on emerge */
    /* +0x0A */ s16 wall_stuck_frames; /* 0x176: frames pinned against walls (adder_state_run); >= 61 forces behavior 2 and resets */
} AdderWork;
#define ADDER(e) ENEMY_WORK(e, AdderWork)
#define ADDER_FIELD(e, f) ENEMY_FIELD(e, AdderWork, f)
#define ADDER_MEMBER(e, f) ENEMY_MEMBER(e, AdderWork, f)

/* state 0: sets up a new adder (health, sca info, animation, work fields), then state 1 */
void adder_state_init(void) {

    ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->status_flags = 1;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x202020;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0, 0);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (5 + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7)) * 2;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->animation_id = 0;
    (g_ApiCallPtr = api_joint_move)(0, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x40);
    g_CurrentEntity->sca_info = (s32) adder_sca_info_ptr;
    g_CurrentEntity->scale = 0x3000;
    ADDER(g_CurrentEntity)->wall_stuck_frames = 0;
    ADDER(g_CurrentEntity)->collision_enabled = 1;
    if (g_CurrentEntity->behavior_flags != 0) {
        g_CurrentEntity->action_ticks_counter = (s16) (0xF0 - (((g_ApiCallPtr = api_rand)() & 6) * 0xA));
        g_CurrentEntity->rot_x = 0;
        g_CurrentEntity->angle_z = 0x800;
        if (g_CurrentEntity->pos_y == 0) {
            g_CurrentEntity->pos_y = -0x1FB8;
        }
    }
    if (g_CurrentEntity->behavior_flags == 3) {
        g_CurrentEntity->angle_z = 0;
        g_CurrentEntity->pos_y = 0;
        ADDER(g_CurrentEntity)->collision_enabled = 0;
        g_CurrentEntity->hit_state = 1;
    }
    return;
}

/* every frame for each adder (enemies_update_functions_tbl): runs adder_state_tbl[state], then collision checks and
   the shadow / drawing */
void adder_update(void) {
    extern s32 (*adder_state_tbl[])();

    if (g_UpdateFlags & 4 & 0xFFFF) {
        adder_state_tbl[g_CurrentEntity->state]();
        if (((u16) ADDER(g_CurrentEntity)->collision_enabled) != 0) {
            (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
            MEMBER_AT((u8 *)g_CurrentEntity + 0x16C, AdderWork, ->player_touch, s16) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
            (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            ADDER(g_CurrentEntity)->room_hit = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
        }
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->behavior_flags != 0) {
        g_CurrentEntity->has_enter_switch_zone = 0U;
    }
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void adder_state_run(void) {
    void adder_behavior_dispatch();
    void adder_decide_action();
    void *joints;

    joints = ((void *) g_CurrentEntity->joints);
    LIT(s32, sp_tmp18) = (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x100);
    LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x200);
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        adder_decide_action();
        /* fallthrough */
    case 1:
        adder_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if ((g_CurrentEntity->pos_y == 0) && (g_PlayerEntity.pos_y == 0)) {
        (g_ApiCallPtr = api_entity_check_visual_range)((void *)0x1388);
    }
    if (g_CurrentEntity->pos_y >= -0xF9F) {
        LIT(s32, sp_tmp0C) = ((s32) JOINT(joints)->world.t[1] >> 4) + 0x1F4;
        if (LIT(s32, sp_tmp0C) < 0) {
            LIT(s32, sp_tmp0C) = 0x12C;
        }
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, LIT(s32, sp_tmp0C) + 0x1F4, LIT(s32, sp_tmp0C));
    }
    if (((u16) ADDER(g_CurrentEntity)->room_hit) != 0) {
        ADDER(g_CurrentEntity)->wall_stuck_frames++;
        return;
    }
    ADDER(g_CurrentEntity)->wall_stuck_frames = 0;
}

void adder_state_damaged(void) {
    void adder_damaged_flinch();
    s32 unused;
    void *joints;

    joints = ((void *) g_CurrentEntity->joints);
    switch (g_CurrentEntity->action_behavior) {
    case 0:
        adder_damaged_flinch();
        break;
    }
    LIT(s32, sp_tmp0C) = ((s32) JOINT(joints)->world.t[1] >> 4) + 0x190;
    if (LIT(s32, sp_tmp0C) < 0) {
        LIT(s32, sp_tmp0C) = 0xC8;
    }
    (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, LIT(s32, sp_tmp0C) + 0x190, LIT(s32, sp_tmp0C));
}

void adder_state_death(void) {
    void adder_death_writhe();
    void adder_death_retreat();
    s32 unused;
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        adder_death_writhe();
        return;
    case 3:
        adder_death_retreat();
    }
}

void adder_behavior_dispatch(void) {
    extern s32 (*adder_behavior_tbl[])();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        ADDER_FIELD(g_CurrentEntity, path_flags) = (u16) (((s16) ADDER_FIELD(g_CurrentEntity, path_flags)) & ~1);
        ADDER_FIELD(g_CurrentEntity, path_flags) = (u16) (ADDER_FIELD(g_CurrentEntity, path_flags) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    adder_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void adder_decide_action() {
    extern s32 (*adder_decide_tbl[])();
    if (g_PlayerEntity.pos_y == 0) {
        ADDER(g_CurrentEntity)->player_distance = (s16) (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - (g_CurrentEntity->pos_z)));
    } else {
        ADDER(g_CurrentEntity)->player_distance = 0x1387;
    }
    adder_decide_tbl[g_CurrentEntity->behavior_flags]();
}

void adder_decide_ground(void) {
    if (((u16) ADDER(g_CurrentEntity)->player_distance < 0x2710U) && ((s16) (((s16) ADDER_FIELD(g_CurrentEntity, path_flags)) & 1) == 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
    }
    if ((((u16) ADDER(g_CurrentEntity)->player_distance < 0x7D0U) && (LIT(s32, sp_tmp18) == 0)) || (ADDER(g_CurrentEntity)->player_touch != 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 3;
        ADDER(g_CurrentEntity)->player_touch = 0;
    } else {
        if ((u16) ((u16) ADDER(g_CurrentEntity)->wall_stuck_frames) >= 0x3DU) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 2;
            ADDER(g_CurrentEntity)->wall_stuck_frames = 0;
        }
        if (g_PlayerEntity.health < 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
        }
    }
}

void adder_decide_ceiling_near(void) {
    ENTITY_ACTION_S16(g_CurrentEntity) = 5;
    if ((u16) ADDER(g_CurrentEntity)->player_distance < 0xBB8U) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
}

void adder_decide_ceiling_ambush() {
    ENTITY_ACTION_S16(g_CurrentEntity) = 5;
    if (((g_PlayerEntity.action_behavior == 0x14) && (g_PlayerEntity.action_state == 0) && ((u16) ADDER(g_CurrentEntity)->player_distance < 0x1388U)) || ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0)) {
        g_CurrentEntity->pos_x = (s32) (g_PlayerEntity.pos_x + (((g_ApiCallPtr = api_rand)() & 1) * 0x64));
        g_CurrentEntity->pos_z = (s32) (g_PlayerEntity.pos_z + (((g_ApiCallPtr = api_rand)() & 1) * 0x64));
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
}

void adder_decide_hidden(void) {
    ENTITY_ACTION_S16(g_CurrentEntity) = 5;
    if ((u16) ADDER(g_CurrentEntity)->player_distance < 0x1194U) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 6;
    }
}

void adder_behavior_chase(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->move_speed_current = (s16) (0x32 - ((g_ApiCallPtr = api_rand)() & 0xF));
        g_CurrentEntity->action_ticks_counter = (s16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0x10);
        /* fallthrough */
    case 1:
        if ((u16) ADDER(g_CurrentEntity)->player_distance < 0x1F40U) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((s16) (ADDER_FIELD(g_CurrentEntity, path_flags) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, ((s16) g_CurrentEntity->action_ticks_counter))));
        } else {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x40);
        }
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((((g_ApiCallPtr = api_rand)()) & 0xF) + ((g_ApiCallPtr = api_rand)() & 0xF)));
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    }
}

void adder_behavior_drop(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0;
        if (g_CurrentEntity->pos_y < -0x1FB8) {
            g_CurrentEntity->pos_y = -0x1FB8;
        }
        ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 0;
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = g_CurrentEntity->pos_y + (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) * ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)++ << 3);
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x55);
        if ((s16) g_CurrentEntity->angle_z & 0x80000000) {
            g_CurrentEntity->angle_z = 0U;
        }
        if (!(g_CurrentEntity->pos_y & 0x80000000)) {
            g_CurrentEntity->pos_y = 0;
            g_CurrentEntity->angle_z = 0U;
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        break;
    case 2:
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void adder_behavior_turn_away(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = (((g_ApiCallPtr = api_rand)() & 1) + 1);
        g_CurrentEntity->action_ticks_counter = (s16) ((g_ApiCallPtr = api_rand)() & 0x3F);
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        return;
    }
}

void adder_behavior_bite(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0x32;
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
        /* fallthrough */
    case 1:
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_tmp10) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0x384, ADDR_g_PlayerPosX);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if ((g_CurrentEntity->animation_frame_id == 0xB) && (LIT(s32, sp_tmp14) == 0) && (LIT(s32, sp_tmp10) != 0) && (((u8) g_PlayerEntity.hit_state) == 0) && ((s16) (((s16) ADDER_FIELD(g_CurrentEntity, path_flags)) & 1) != 0)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)(5, 0);
            if (((((g_ApiCallPtr = api_rand)()) & 1) * ((g_ApiCallPtr = api_rand)() & 1)) != 0) {
                g_PlayerEntity.collision_flags |= 2;
                g_PlayerWork.poison_timer = 0x96;
            }
            g_PlayerEntity.health -= 6;
            g_PlayerEntity.hit_state = 1;
            g_PlayerEntity.action_behavior = 0x64;
            (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
            SCRATCH_VEC->x = 0;
            LIT(s32, sp_vec_vy) = -0x208;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 1, (s32) ((s16) g_PlayerEntity.angle), ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        break;
    case 2:
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
        joints;
    }
}

void adder_behavior_victory(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0x32;
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    }
}

void adder_behavior_idle(void) {

    return;
}

void adder_behavior_emerge(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->move_speed_current = 0x1E;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 0;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->hit_state = 0;
            ADDER(g_CurrentEntity)->collision_enabled = 1;
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

void adder_damaged_flinch(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0x10;
        joints = g_CurrentEntity->joints;
        LIT(s32, sp_tmp1C) = 4;
        while (--LIT(s32, sp_tmp1C) != -1) {
            *(s8 *)(JOINT_N(joints, 11) - (LIT(s32, sp_tmp1C) * 0x7C)) = 0;
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 2:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
            g_CurrentEntity->hit_state = 0;
        }
        return;
    }
}

void adder_death_writhe(void) {
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_id = 4;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0x1E;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 8);
        g_CurrentEntity->move_speed_current = 0x28;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0x10);
        if ((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) == 7) {
            LIT(s32, sp_tmp1C) = 0xB;
            while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
                (g_ApiCallPtr = api_joint_setup_attack_effect)((LIT(s32, sp_tmp1C) * 0x7C) + joints, 8U, 5, 3);
            }
            (g_ApiCallPtr = api_billboard_set_size)((&g_CurrentEntity->shadow), 0U, 0);
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 4;
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
            return;
        }
        if ((u32) (((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) >= 2U) {
            (g_ApiCallPtr = api_billboard_set_size)((&g_CurrentEntity->shadow), 0U, 0);
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 3;
            return;
        }
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            (g_ApiCallPtr = api_billboard_set_color)((&g_CurrentEntity->shadow), 1U, 2, 0xFFFF50);
            (g_ApiCallPtr = api_billboard_set_size)((&g_CurrentEntity->shadow), 0x14U, 0xA);
            g_CurrentEntity->action_state = 2U;
        }
        if ((u8) ENTITY_AS(g_CurrentEntity, timing_control, u8) < 0x23U) {
            (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
        }
        return;
    case 2:
        (g_ApiCallPtr = api_billboard_adj_size)((&g_CurrentEntity->shadow), 0x16U, 0xB);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 4;
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
        }
        return;
    }
}

void adder_death_retreat(void) {
    Joint *joints;

    joints = ENTITY_AS(g_CurrentEntity, joints, Joint *);
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->animation_id = 4;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0x10;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0, 7, (((s16) g_PlayerEntity.angle) - ENTITY_AS(g_CurrentEntity, angle, s16)) + 0x800, &joints[0].world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0, 7, (((s16) g_PlayerEntity.angle) - ENTITY_AS(g_CurrentEntity, angle, s16)) + 0x800, &joints[5].world, ADDR_sp_vec, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->scale -= 0xF5;
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        break;
    case 2:
        LIT(s32, sp_tmp1C) = 0xB;
        while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
            joints[LIT(s32, sp_tmp1C)].flags = 0;
        }
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0xD2;
        break;
    case 3:
        g_CurrentEntity->pos_y = -0x4E20;
        g_CurrentEntity->action_state = 4;
        /* fallthrough */
    case 4:
        if (!({ (s32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--; })) {
            LIT(s32, sp_tmp1C) = 0xB;
            while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
                joints[LIT(s32, sp_tmp1C)].flags = 3;
            }
            ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 2;
            if (g_RoomId == 1) {
                ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 1;
            }
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(0, 0, 0, 0);
        }
        break;
    }
}
