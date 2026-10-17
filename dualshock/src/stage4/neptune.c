/* neptune.c (stage4) - Neptune (shark): swim, lunge, bite and devour, circle, retreat, deaths
   (incl. the boss death in the aquarium), and two player death animations. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* neptune data */
extern s32 neptune_sca_info_big;
extern s32 neptune_sca_info_small;
extern s32 g_NeptuneCaptureMatrix;
extern s32 g_NeptuneCaptureMatrix_tx;
extern s32 g_NeptuneCaptureMatrix_ty;

/* NeptuneWork: the neptune's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). NEPTUNE(e)->f, NEPTUNE_FIELD(e, f) and NEPTUNE_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct NeptuneWork {
    /* +0x00 */ s16 grab_struggle_timer; /* 0x16C: 0x5A when the bite seizes the player (neptune_bite_seize); input subtracts 8, else 1 per frame; releases when < 0 */
    /* +0x02 */ u16 wander_angle;        /* 0x16E: copied from scratch 0x1F800018 after api_check_room_collision (neptune_update); first arg of api_entity_update_wander_turn */
    /* +0x04 */ s8 wander_turn_state;    /* 0x170: cleared in neptune_state_init; given by address to api_entity_update_wander_turn */
    /* +0x05 */ s8 effect_timer;         /* 0x171: counts down; at 0 spawns effect 0x17 billboards on joints and reloads (4 swimming, 7 dying, 0xB biting/recovering) */
    /* +0x06 */ s8 swim_anim_count;      /* 0x172: adds api_joint_move results while swimming; at 3 plays swim SE 0 and resets (3 in neptune_swim_begin) */
    /* +0x07 */ s8 unused_173;           /* 0x173: only set to 5 in neptune_state_init */
    /* +0x08 */ u8 dir_control_flags;    /* 0x174: OR-ed api_check_room_collision bits (neptune_update), cleared in init; given by address to the wander turn */
    /* +0x09 */ s8 unused_175;           /* 0x175: only set to 0x20 in neptune_circle_begin */
} NeptuneWork;
#define NEPTUNE(e) ENEMY_WORK(e, NeptuneWork)
#define NEPTUNE_AS(e, f, T) ENEMY_WORK_AS(e, NeptuneWork, f, T)
#define NEPTUNE_FIELD(e, f) ENEMY_FIELD(e, NeptuneWork, f)
#define NEPTUNE_MEMBER(e, f) ENEMY_MEMBER(e, NeptuneWork, f)

/* every frame for each neptune (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void neptune_update(void) {
    extern s32 (*neptune_state_tbl[])();
    s32 joints;
    VECTOR probe;

    if ((g_CurrentEntity->state != 0) && (g_CurrentEntity->behavior_flags & 0x80 & 0xFF)) {
        return;
    }
    if (g_UpdateFlags & 4 & 0xFFFF) {
        neptune_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        (g_ApiCallPtr = api_check_entity_collision)(LITP(Entity, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
        probe = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        probe.vx = 0x320;
        (g_ApiCallPtr = api_RotMatrix)((Entity *) &g_CurrentEntity->rot_x, LITP(Entity, sp_matrix));
        (g_ApiCallPtr = api_ApplyMatrixLV)(LITP(Entity, sp_matrix), (Entity *) &probe, &probe);
        g_CurrentEntity->pos_x = g_CurrentEntity->pos_x + probe.vx;
        g_CurrentEntity->pos_z = g_CurrentEntity->pos_z + probe.vz;
        NEPTUNE(g_CurrentEntity)->dir_control_flags |= (g_ApiCallPtr = api_check_room_collision)((Entity *) &g_CurrentEntity->pos_x, (Entity *)0x258);
        NEPTUNE(g_CurrentEntity)->wander_angle = LIT(u16, sp_tmp18);
        g_CurrentEntity->pos_x = g_CurrentEntity->pos_x - probe.vx;
        g_CurrentEntity->pos_z = g_CurrentEntity->pos_z - probe.vz;
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)((Entity *) &g_CurrentEntity->pos_x, g_CameraSwitchZones);
    joints = g_CurrentEntity->joints;
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)((Entity *) &g_CurrentEntity->pos_x, (Entity *) &g_CurrentEntity->shadow, NULL, (s16) g_CurrentEntity->angle);
    }
}

/* state 0: sets up a new neptune (health, sca info, animation, work fields), then state 1 */
void neptune_state_init(void) {

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0xF;
    g_CurrentEntity->hit_state = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x404040;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        (g_ApiCallPtr = api_billboard_init)(LITP(void, sp_svec), &g_CurrentEntity->shadow, 0x3E8, 0x96);
        g_CurrentEntity->scale = 0x800;
        g_CurrentEntity->sca_info = (s32)&neptune_sca_info_small;
    } else {
        (g_ApiCallPtr = api_billboard_init)(LITP(void, sp_svec), &g_CurrentEntity->shadow, 0x7D0, 0x12C);
        g_CurrentEntity->sca_info = (s32)&neptune_sca_info_big;
    }
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (s16) (0x28 + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7));
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
    if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 1, 0);
    }
    (g_ApiCallPtr = api_joint_move)(NULL, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x40);
    NEPTUNE(g_CurrentEntity)->effect_timer = 7;
    NEPTUNE(g_CurrentEntity)->unused_173 = 5;
    NEPTUNE(g_CurrentEntity)->dir_control_flags = 0;
    NEPTUNE(g_CurrentEntity)->wander_turn_state = 0;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = 0x3E8;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, LITP(void, sp_matrix));
    (g_ApiCallPtr = api_ApplyMatrixLV)(LITP(void, sp_matrix), LITP(void, sp_vec), ADDR_sp_vec);
    g_CurrentEntity->position_vx = (u16) (g_CurrentEntity->position_vx + (u16) SCRATCH_VEC->x);
    g_CurrentEntity->position_vz = (u16) (g_CurrentEntity->position_vz + LIT(u16, sp_vec_vz));
    if (g_CurrentEntity->behavior_flags == 2) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    }
}

void neptune_state_run(void) {
    void neptune_choose_attack();
    void neptune_behavior_dispatch();

    (g_ApiCallPtr = api_zone_path_find)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, &g_CurrentEntity->waypoint_x, &g_CurrentEntity->waypoint_z);
    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF) && (LIT(s32, sp_tmp00) < 0x2710)) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    }
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        neptune_choose_attack();
    }
    neptune_behavior_dispatch();
}

void neptune_choose_attack(void) {
    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    (g_ApiCallPtr = api_entity_update_wander_turn)(NEPTUNE(g_CurrentEntity)->wander_angle, &NEPTUNE(g_CurrentEntity)->dir_control_flags, &NEPTUNE(g_CurrentEntity)->wander_turn_state, 0x20, 0x3C);
    if ((((u8) g_PlayerEntity.hit_state) == 0) && !(g_CurrentEntity->behavior_flags & 2 & 0xFF) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x300) == 0)) {
        if (g_PlayerEntity.health < 0x1A) {
            if (LIT(s32, sp_tmp00) < 0xDAC) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_S16(g_CurrentEntity) = 4;
            }
            return;
        }
        if (LIT(s32, sp_tmp00) < 0xBB8) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 5;
        }
    }
}

void neptune_behavior_dispatch(void) {
    extern s32 (*neptune_behavior_tbl[])();
    neptune_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void neptune_behavior_swim(void) {
    extern s32 (*neptune_swim_actions[])();
    s32 joints;

    neptune_swim_actions[g_CurrentEntity->action_state]();
    NEPTUNE(g_CurrentEntity)->swim_anim_count = (s8) (NEPTUNE_AS(g_CurrentEntity, swim_anim_count, u8) + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40));
    if (NEPTUNE_AS(g_CurrentEntity, swim_anim_count, u8) == 3) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
        NEPTUNE(g_CurrentEntity)->swim_anim_count = 0;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if (!(g_CurrentEntity->behavior_flags & 2 & 0xFF)) {
        if (--NEPTUNE_AS(g_CurrentEntity, effect_timer, u8) == 0) {
            joints = g_CurrentEntity->joints;
            LIT(s32, sp_vec_vx) = 0x1F4;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
            if (!((g_ApiCallPtr = api_rand)() & 1)) {
                LIT(s32, sp_vec_vx) = 0x190;
                (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            }
            LIT(s32, sp_vec_vx) = -0x64;
            LIT(s32, sp_vec_vy) = -0x190;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
            if (!((g_ApiCallPtr = api_rand)() & 3)) {
                LIT(s32, sp_vec_vy) = 0x1F4;
                (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
            }
            NEPTUNE(g_CurrentEntity)->effect_timer = 4;
        }
    }
}

void neptune_swim_begin(void) {
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
    g_CurrentEntity->action_state++;
    g_CurrentEntity->move_speed_current = 0x78;
    NEPTUNE(g_CurrentEntity)->swim_anim_count = 3;
}

void neptune_swim_far(void) {
    if ((LIT(s32, sp_tmp00) >= 0x2711) && (g_PlayerEntity.action_behavior == 0xD)) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->move_speed_current = 0x82;
    }
}

void neptune_swim_near(void) {
    if ((LIT(s32, sp_tmp00) >= 0x1770) && (g_PlayerEntity.action_behavior == 0xD)) {
        return;
    }
    g_CurrentEntity->action_state--;
    g_CurrentEntity->move_speed_current = 0x64;
}

void neptune_behavior_lunge(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->move_speed_current = 0x32;
        g_CurrentEntity->animation_id = (u8) (((g_ApiCallPtr = api_rand_lfsr_next)() & 1) + 2);
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        (g_ApiCallPtr = api_sound_play_entity_se)(4U, 0);
        /* fallthrough */
    case 1:
        if ((((u8) g_CurrentEntity->animation_id) == 2) && (g_CurrentEntity->animation_frame_id == 0x19)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(5U, 0);
        }
        if ((g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40) != 0) {
            g_CurrentEntity->action_state--;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(((g_ApiCallPtr = api_rand)() & 0xFFF));
        g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 4);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
    }
}

void neptune_behavior_retreat(void) {
    extern s32 (*neptune_retreat_actions[])();
    neptune_retreat_actions[g_CurrentEntity->action_state]();
}

void neptune_retreat_begin(void) {
    g_CurrentEntity->action_state++;
    g_CurrentEntity->move_speed_current = 0x96;
    g_CurrentEntity->animation_id = 6;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
}

void neptune_retreat_run(void) {
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
    if (g_CurrentEntity->animation_frame_id == 0x17) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void neptune_behavior_circle(void) {
    extern s32 (*neptune_circle_actions[])();
    neptune_circle_actions[g_CurrentEntity->action_state]();
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void neptune_circle_begin(void) {

    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
    g_CurrentEntity->action_state++;
    g_CurrentEntity->move_speed_current = 0x64;
    g_CurrentEntity->action_ticks_counter = (s16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x5A);
    NEPTUNE(g_CurrentEntity)->unused_175 = 0x20;
}

void neptune_circle_left(void) {

    (g_ApiCallPtr = api_entity_update_wander_turn)(NEPTUNE(g_CurrentEntity)->wander_angle, &NEPTUNE(g_CurrentEntity)->dir_control_flags, &NEPTUNE(g_CurrentEntity)->wander_turn_state, 0x20, 0x3C);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x1E);
    }
}

void neptune_circle_right(void) {

    (g_ApiCallPtr = api_entity_update_wander_turn)(NEPTUNE(g_CurrentEntity)->wander_angle, &NEPTUNE(g_CurrentEntity)->dir_control_flags, &NEPTUNE(g_CurrentEntity)->wander_turn_state, -0x20, 0x3C);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->action_state--;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x5A);
    }
}

void neptune_behavior_devour(void) {
    extern s32 (*neptune_devour_actions[])();
    s32 old_x;
    s32 old_z;

    neptune_devour_actions[g_CurrentEntity->action_state]();
    old_x = g_CurrentEntity->pos_x;
    old_z = g_CurrentEntity->pos_z;
    (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, 0x1F4);
    LIT(s32, sp_tmp00) = g_CurrentEntity->pos_x - old_x;
    LIT(s32, sp_tmp04) = g_CurrentEntity->pos_z - old_z;
    g_CurrentEntity->anim_base_x = (u16) (g_CurrentEntity->anim_base_x + (u16) LIT(s32, sp_tmp00));
    g_CurrentEntity->anim_base_z = (u16) (g_CurrentEntity->anim_base_z + (u16) LIT(s32, sp_tmp04));
    g_PlayerEntity.anim_base_x += (u16) LIT(s32, sp_tmp00);
    g_PlayerEntity.anim_base_z += (u16) LIT(s32, sp_tmp04);
    g_CurrentEntity->position_vx = (u16) (g_CurrentEntity->position_vx + (u16) LIT(s32, sp_tmp00));
    g_CurrentEntity->position_vz = (u16) (g_CurrentEntity->position_vz + (u16) LIT(s32, sp_tmp04));
    g_PlayerEntity.position_vx += (u16) LIT(s32, sp_tmp00);
    g_PlayerEntity.position_vz += (u16) LIT(s32, sp_tmp04);
}

void neptune_devour_seize(void) {
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x100);
    g_CurrentEntity->animation_id = 6;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
    g_CurrentEntity->action_state++;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    g_CurrentEntity->move_speed_current = 0x96;
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    g_PlayerEntity.status_flags |= 6;
    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 0xB, 0, 0);
    g_PlayerEntity.angle = g_CurrentEntity->angle;
}

void neptune_devour_carry(void) {
    s32 joints;

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x40);
    joints = g_CurrentEntity->joints;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = 0xC8;
    if (g_CurrentEntity->animation_frame_id == 0xF) {
        if ((g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0x3E8, LITP(void, g_PlayerPosX)) == 0) {
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 1);
            (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
            g_PlayerEntity.timing_control = 0;
            g_PlayerEntity.animation_id = 0;
            g_PlayerEntity.blend_counter = 0;
            (*(s16 *)&g_PlayerEntity.action_ticks_counter) = 0x64;
            g_PlayerEntity.hit_state = 0;
            g_PlayerEntity.move_speed_current = 0;
            (*(s16 *)&g_PlayerEntity.angle) -= 0x800;
            g_CurrentEntity->action_state = 5U;
            g_PlayerEntity.status_flags &= 0xF9;
            g_PlayerEntity.hit_state = 0;
            return;
        }
        g_PlayerEntity.pos_x = g_CurrentEntity->pos_x;
        g_PlayerEntity.pos_z = g_CurrentEntity->pos_z;
        g_CurrentEntity->action_state++;
        (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0xFA, 0, (void *)5);
        (g_ApiCallPtr = api_pad_vibrate_small)(5, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)(8, 0xA);
        (g_ApiCallPtr = api_pad_vibrate_small)(0xA, 0x15);
        (g_ApiCallPtr = api_pad_vibrate_small)(0xA, 0x23);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0xFA, 0xF);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0xFA, 0x1A);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0xC8, 0x28);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x2D, 0xC8, 0, (void *)0x32);
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 3U) {
        SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x12C);
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xA, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x5AA);
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 9, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 4U) && ((u8) g_CurrentEntity->animation_frame_id < 5U)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = 0x1F4;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 10), ADDR_sp_vec, 0);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void neptune_devour_bite(void) {
    void *joints;

    (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, ((s32 *) g_CurrentEntity->anim_base), 0x40);
    if (g_CurrentEntity->animation_frame_id == 0x13) {
        g_CurrentEntity->action_state++;
        joints = ((void *) g_CurrentEntity->joints);
        (g_ApiCallPtr = api_matrix_relative)(JOINT_N_WORLD(joints, 1), ADDR_g_PlayerCoordM00, &g_NeptuneCaptureMatrix);
        g_PlayerEntity.action_state++;
        g_PlayerEntity.scd_entity_flags |= 0x40;
        joints = ((void *) g_PlayerEntity.joints);
        FIELD_AT(joints, Joint, [1].flags, s8) = (s8) (FIELD_AT(joints, Joint, [1].flags, u8) & 0xFE);
        JOINT(joints)->flags = (s8) (JOINT(joints)->flags & 0xFE);
        FIELD_AT(joints, Joint, [9].flags, s8) = (s8) (FIELD_AT(joints, Joint, [9].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [12].flags, s8) = (s8) (FIELD_AT(joints, Joint, [12].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [10].flags, s8) = (s8) (FIELD_AT(joints, Joint, [10].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [13].flags, s8) = (s8) (FIELD_AT(joints, Joint, [13].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [11].flags, s8) = (s8) (FIELD_AT(joints, Joint, [11].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [14].flags, s8) = (s8) (FIELD_AT(joints, Joint, [14].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [2].flags, s8) = (s8) (FIELD_AT(joints, Joint, [2].flags, u8) & 0xFE);
    }
}

void neptune_devour_swallow(void) {
    void *joints;

    g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x40);
    g_PlayerEntity.health = -1;
    g_NeptuneCaptureMatrix_tx -= 0x41;
    g_NeptuneCaptureMatrix_ty += 0xD;
    joints = ((void *) g_PlayerEntity.joints);
    if (g_CurrentEntity->animation_frame_id == 0x29) {
        FIELD_AT(joints, Joint, [3].flags, u8) = (u8) (FIELD_AT(joints, Joint, [3].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [6].flags, u8) = FIELD_AT(joints, Joint, [6].flags, u8) & 0xFE;
    }
    if (g_CurrentEntity->animation_frame_id == 0x3B) {
        FIELD_AT(joints, Joint, [4].flags, u8) = (u8) (FIELD_AT(joints, Joint, [4].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [7].flags, u8) = (u8) (FIELD_AT(joints, Joint, [7].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [5].flags, u8) = (u8) (FIELD_AT(joints, Joint, [5].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [8].flags, u8) = FIELD_AT(joints, Joint, [8].flags, u8) & 0xFE;
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0x38U) {
        if (((u8) g_CurrentEntity->animation_frame_id >= 3U) && ((u8) g_CurrentEntity->animation_frame_id < 5U)) {
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x12C);
            LIT(s32, sp_vec_vy) = -0x5DC;
            LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 0xA, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x5AA);
            LIT(s32, sp_vec_vy) = -0x5DC;
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE);
            (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 9, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        }
        if (!(g_CurrentEntity->animation_frame_id & 7 & 0xFF) && ((u8) g_CurrentEntity->animation_frame_id >= 8U)) {
            joints = ((void *) g_CurrentEntity->joints);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0xC8;
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100);
            (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 0xA, 0, JOINT_N_WORLD(joints, 5), ADDR_sp_vec, 0);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 9, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        }
        if (!(g_CurrentEntity->animation_frame_id & 7 & 0xFF) && ((u8) g_CurrentEntity->animation_frame_id >= 0xBU)) {
            joints = ((void *) g_CurrentEntity->joints);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0xC8;
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100);
            (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 0xA, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        }
    } else if (!(g_CurrentEntity->animation_frame_id & 3 & 0xFF)) {
        joints = ((void *) g_CurrentEntity->joints);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17U, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
    }
    if (!((g_CurrentEntity->animation_frame_id % 5) & 0xFF) && ((u8) g_CurrentEntity->animation_frame_id >= 2U)) {
        joints = ((void *) g_CurrentEntity->joints);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(0U, 0, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
    }
}

void neptune_devour_finish(void) {
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
}

void neptune_devour_spit(void) {
    s32 joints;

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x40);
    if (g_CurrentEntity->animation_frame_id == 0x17) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->action_ticks_counter = 0x1E;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 3U) && ((u8) g_CurrentEntity->animation_frame_id < 5U)) {
        SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x12C);
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xA, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x5AA);
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x2EE);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 9, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 5U) && !(g_CurrentEntity->animation_frame_id & 3 & 0xFF)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = 0xC8;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        SCRATCH_VEC->x = (s32) (0x320 - ((g_ApiCallPtr = api_rand)() & 0x3FF));
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
    }
    if (!(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        SCRATCH_VEC->x = 0;
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, LITP(void, g_PlayerCoordM00), ADDR_sp_vec, 0);
    }
}

void neptune_devour_recover(void) {
    s32 joints;

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 1);
        g_CurrentEntity->move_speed_current = 0x78;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if (--NEPTUNE_AS(g_CurrentEntity, effect_timer, u8) == 0) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        NEPTUNE(g_CurrentEntity)->effect_timer = 0xB;
    }
}

void neptune_behavior_bite(void) {
    extern s32 (*neptune_bite_actions[])();
    neptune_bite_actions[g_CurrentEntity->action_state]();
}

void neptune_bite_begin(void) {
    g_CurrentEntity->action_state++;
    g_CurrentEntity->move_speed_current = 0x64;
    g_CurrentEntity->animation_id = 7;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x1F;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
}

void neptune_bite_close(void) {
    s32 joints;

    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->action_ticks_counter = 0x1E;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        return;
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0x1EU) {
        if (!(g_CurrentEntity->animation_frame_id & 1 & 0xFF)) {
            joints = g_CurrentEntity->joints;
            SCRATCH_VEC->x = 0x1F4;
            LIT(s32, sp_vec_vy) = 0xC8;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        }
    } else if (!(g_CurrentEntity->animation_frame_id & 1 & 0xFF)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
    }
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x20);
    joints = g_CurrentEntity->joints;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = 0xC8;
    if ((g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0x12C, ADDR_g_PlayerPosX) != 0) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->anim_base_x = (u16) ENTITY_AS(g_CurrentEntity, pos_x, u16);
        g_CurrentEntity->anim_base_z = (u16) ENTITY_AS(g_CurrentEntity, pos_z, u16);
        g_PlayerEntity.anim_base_x = ENTITY_AS(g_CurrentEntity, pos_x, u16);
        g_PlayerEntity.anim_base_z = ENTITY_AS(g_CurrentEntity, pos_z, u16);
        g_PlayerEntity.angle = g_CurrentEntity->angle;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xB, 0, 0);
        g_PlayerEntity.status_flags |= 2;
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0, 0xC8, 0);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void neptune_bite_seize(void) {
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
        g_CurrentEntity->action_state++;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        g_CurrentEntity->animation_id = 9;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_PlayerEntity.animation_id = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        NEPTUNE_FIELD(g_CurrentEntity, grab_struggle_timer) = 0x5A;
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0, 0xC8, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)(6, 5);
    }
    if (!(g_CurrentEntity->animation_frame_id & 3 & 0xFF)) {
        LIT(s32, sp_vec_vx) = 0;
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
    }
}

void neptune_bite_shake(void) {
    s32 joints;

    NEPTUNE(g_CurrentEntity)->grab_struggle_timer -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 8 : 1;
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x80) != 0) {
        g_PlayerEntity.health -= 0x1E;
        if (!(g_CurrentEntity->animation_frame_id & 3 & 0xFF)) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = -0x64;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xA, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vy) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
            joints = g_PlayerEntity.joints;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 11), ADDR_sp_vec, 0);
        }
        if ((s16) g_PlayerEntity.health < 0) {
            g_PlayerEntity.health = 1;
        }
    }
    if (!(g_CurrentEntity->animation_frame_id & 0xF & 0xFF)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        joints = g_PlayerEntity.joints;
        LIT(s32, sp_vec_vy) = 0;
        SCRATCH_VEC->x = 0xC8;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        SCRATCH_VEC->x = 0;
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, LITP(void, g_PlayerCoordM00), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xF, 0x82, 0xB4, NULL);
        (g_ApiCallPtr = api_pad_vibrate_small)(4, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)(4, 0xA);
    }
    if (--NEPTUNE_AS(g_CurrentEntity, effect_timer, u8) == 0) {
        SCRATCH_VEC->x = 0x258;
        LIT(s32, sp_vec_vy) = -0xC8;
        LIT(s32, sp_vec_vz) = -0x514;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        NEPTUNE(g_CurrentEntity)->effect_timer = 0xB;
    }
    if (NEPTUNE_FIELD(g_CurrentEntity, grab_struggle_timer) < 0) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->move_speed_current = 0x64;
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 0;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        (g_ApiCallPtr = api_pad_vibrate_large)(0x1E, 0x50, 0);
    }
}

void neptune_bite_release(void) {
    s32 joints;

    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -0x10);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->action_ticks_counter = 0x1E;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
        g_PlayerEntity.hit_state = 0;
        g_PlayerEntity.status_flags &= 0xFD;
    }
    if (!(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        SCRATCH_VEC->x = 0;
        LIT(s32, sp_vec_vy) = -0x5DC;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void neptune_bite_recover(void) {
    s32 joints;

    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -0x20);
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 1);
        g_CurrentEntity->move_speed_current = 0x78;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if (--NEPTUNE_AS(g_CurrentEntity, effect_timer, u8) == 0) {
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x64;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        NEPTUNE(g_CurrentEntity)->effect_timer = 0xF;
    }
}

void neptune_state_death(void) {
    extern s32 (*neptune_death_kind_tbl[])();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->sub_state++;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
    neptune_death_kind_tbl[(g_CurrentEntity->behavior_flags & 1 & 0xFF)]();
}

void neptune_death_normal(void) {
    extern s32 (*neptune_death_actions[])();
    s32 joints;

    neptune_death_actions[g_CurrentEntity->action_behavior]();
    if (!(g_CurrentEntity->behavior_flags & 2 & 0xFF)) {
        if (--NEPTUNE_AS(g_CurrentEntity, effect_timer, u8) == 0) {
            NEPTUNE(g_CurrentEntity)->effect_timer = 7;
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = -0x64;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vy) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        }
    }
}

void neptune_death_turn(void) {
    void neptune_death_thrash();
    s32 joints;

    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->move_speed_current = 0x50;
    g_CurrentEntity->action_ticks_counter = 0x1E;
    if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) != 0) {
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->move_speed_current = 0x78;
        return;
    }
    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
    NEPTUNE(g_CurrentEntity)->effect_timer = 7;
    joints = g_CurrentEntity->joints;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    LIT(s32, sp_vec_vy) = -0x64;
    (g_ApiCallPtr = api_effect_create_billboard)(0x17, 0xD, 0, JOINT_N_WORLD(joints, 12), ADDR_sp_vec, 0x1E);
    LIT(s32, sp_vec_vy) = 0;
    (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0x1E);
    (g_ApiCallPtr = api_effect_create_billboard)(0x17, 8, 0, JOINT_N_WORLD(joints, 9), ADDR_sp_vec, 0x1E);
    neptune_death_thrash();
}

void neptune_death_thrash(void) {
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->move_speed_current = 0x78;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle + 0x800) & 0xFFF);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
}

void neptune_death_sink(void) {

    LIT(s32, sp_vec_vx) = (s32) ENTITY_AS(g_CurrentEntity, waypoint_x, s16);
    LIT(s32, sp_vec_vz) = (s32) ENTITY_AS(g_CurrentEntity, waypoint_z, s16);
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -0x20);
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->hit_state = 0;
    }
}

void neptune_death_boss(void) {
    extern s32 (*neptune_boss_death_actions[])();
    neptune_boss_death_actions[g_CurrentEntity->action_behavior]();
}

void neptune_boss_death_begin(void) {
    void neptune_boss_death_charge();
    s32 joints;

    (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    joints = g_CurrentEntity->joints;
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->move_speed_current = 0x96;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = -0x64;
    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 3, JOINT_N_WORLD(joints, 7), ADDR_sp_vec, 0);
    g_CurrentEntity->action_ticks_counter = 0x2D;
    g_CurrentEntity->animation_id = 2;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x3F;
    (g_ApiCallPtr = api_sound_play_entity_se)(4, 0U);
    neptune_boss_death_charge();
}

void neptune_boss_death_charge(void) {
    s16 turning;

    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -8);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->move_speed_current = 0x64;
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 0xDU) && ((u8) g_CurrentEntity->animation_frame_id < 0x19U)) {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
        turning = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400);
        g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
        if (turning != 0) {
            (g_ApiCallPtr = api_entity_add_speed_xz)(0x400);
            return;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0xC00);
    }
}

void neptune_boss_death_charge2(void) {
    s16 turning;

    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -8);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_ticks_counter = 0x2D;
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
    turning = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400);
    g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
    if (turning != 0) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x400);
        return;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0xC00);
}

void neptune_boss_death_beach(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        if (g_CurrentEntity->animation_frame_id == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)4, 0);
        }
        if ((--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) < 0) && (g_CurrentEntity->animation_frame_id == 8)) {
            g_CurrentEntity->action_state++;
            g_CurrentEntity->animation_id = 5;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 0x3F;
            g_CurrentEntity->action_ticks_counter = 0x46U;
        }
        break;
    case 1:
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->action_state = 0U;
        g_CurrentEntity->animation_id = 5;
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x64, -0x64);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->action_ticks_counter = 0x5AU;
        break;
    }
    (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
}

void neptune_boss_death_fade(void) {

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->health = -1;
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 6, 6);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state++;
        }
    }
    (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
}

void neptune_state3_nop(void) {

}

void neptune_state4_nop(void) {

}

void neptune_state5_nop(void) {

}

void player_anim_poison_death(void) {
    if (g_PlayerEntity.action_state == 0) {
        g_PlayerEntity.action_state++;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0;
        g_PlayerEntity.hit_state = 1;
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
    }
    (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
}

void player_anim_death_billboard(void) {
    s32 joints;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state++;
        g_PlayerEntity.animation_id = 3;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 0;
        g_PlayerEntity.hit_state = 1;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, ((s32 *) g_PlayerWork.grab_anim_header), g_PlayerWork.grab_anim_base, 0x400);
        if (g_PlayerEntity.animation_frame_id == 0xE) {
            g_PlayerEntity.animation_frame_id--;
        }
        return;
    case 2:
        joints = g_EnemySlot0_Joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_RotMatrix)(ADDR_g_EnemySlot0_RotX, LITP(s32, g_EnemySlot0_CoordMatrix));
        (g_ApiCallPtr = api_GsMulCoord0)(ADDR_g_EnemySlot0_CoordMatrix, JOINT_N_TRANSFORM(joints, 0), ADDR_sp_matrix);
        (g_ApiCallPtr = api_GsMulCoord0)(ADDR_sp_matrix, &g_NeptuneCaptureMatrix, ADDR_g_PlayerCoordM00);
        return;
    }
}
