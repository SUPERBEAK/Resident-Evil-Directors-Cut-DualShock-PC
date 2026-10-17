/* black_tiger.c (stage3) - Black Tiger (the big spider boss): the web spinner's code
   (src/shared/web_spinner.c) with its own behaviors (bite, spit) and a third death (writhes). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* black tiger data */
extern s32 black_tiger_sca_info_ptr;
extern s32 black_tiger_sca_info_ptr_walk;
extern s32 black_tiger_sca_info_ptr_dead;
extern u8 black_tiger_step_se_tbl[];
extern s32 black_tiger_wound_joints;
extern s32 g_BlackTigerWoundedJoints;

/* BlackTigerWork: the black_tiger's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). BLACK_TIGER(e)->f, BLACK_TIGER_FIELD(e, f) and BLACK_TIGER_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct BlackTigerWork {
    /* +0x00 */ s16 turn_step;         /* 0x16C: turn rate / heading step: 0x80 for approach, +-0x18 added to angle each frame while strafing, random direction for api_entity_add_speed_xz in circle */
    /* +0x02 */ s16 path_flags;        /* 0x16E: bit 0 = api_entity_pathfind_update result bit 0 (clear line to player); gates turning, spit and circle exit */
    /* +0x04 */ s16 player_touch;      /* 0x170: api_check_entity_collision result (black_tiger_update); lunge hits when set */
    /* +0x06 */ s16 attack_cooldown;   /* 0x172: frames before the spit (behavior 0xD) may be chosen; 0x1E/0x14 after attacks, counted down in black_tiger_state_run */
    /* +0x08 */ s16 death_behavior;    /* 0x174: dying variant picked in black_tiger_state_death (1 or 6); restored with action_state 3 when the dying state restarts */
    /* +0x0A */ s16 room_hit;          /* 0x176: api_check_room_collision result (black_tiger_update) */
    /* +0x0C */ s16 wall_stuck_frames; /* 0x178: frames touching walls (black_tiger_state_run); >= 21 makes black_tiger_pick_behavior choose strafe/approach and reset it */
    /* +0x0E */ s16 unused_17A;        /* 0x17A: set 0 in black_tiger_state_init and 6 each frame of behavior 1 (walk); never read in this file */
    /* +0x10 */ s16 wound_count;       /* 0x17C: next index into g_BlackTigerWoundedJoints (black_tiger_hit_react adds one per hit); black_tiger_die_writhe checks < 6 */
    /* +0x12 */ s16 pick_cooldown;     /* 0x17E: frames black_tiger_pick_behavior waits before choosing a new behavior (0x2D at init), counted down there */
} BlackTigerWork;
#define BLACK_TIGER(e) ENEMY_WORK(e, BlackTigerWork)
#define BLACK_TIGER_FIELD(e, f) ENEMY_FIELD(e, BlackTigerWork, f)
#define BLACK_TIGER_MEMBER(e, f) ENEMY_MEMBER(e, BlackTigerWork, f)

/* state 0: sets up a new black tiger (health, sca info, animation, work fields), then state 1 */
void black_tiger_state_init(void) {
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    g_CurrentEntity->animation_id = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400);
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    LIT(s32, sp_tmp1C) = 0x404040;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0x9C4, 0x9C4);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = 0xCC;
    g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr;
    g_CurrentEntity->scale = 0x1B33;
    BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) = 0;
    BLACK_TIGER_FIELD(g_CurrentEntity, death_behavior) = 0;
    BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    BLACK_TIGER_FIELD(g_CurrentEntity, unused_17A) = 0;
    BLACK_TIGER_FIELD(g_CurrentEntity, wound_count) = 0;
    BLACK_TIGER_FIELD(g_CurrentEntity, pick_cooldown) = 0x2D;
}

/* every frame for each black tiger (enemies_update_functions_tbl): runs black_tiger_state_tbl[state], then collision checks and
   the shadow / drawing */
void black_tiger_update(void) {
    extern s32 (*black_tiger_state_tbl[])();
    u16 unused;
    s32 joints;

    if (g_UpdateFlags & 4 & 0xFFFF) {
        black_tiger_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        BLACK_TIGER_FIELD(g_CurrentEntity, player_touch) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
        BLACK_TIGER_FIELD(g_CurrentEntity, room_hit) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    joints = g_CurrentEntity->joints;
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void black_tiger_state_run(void) {
    void black_tiger_behavior_dispatch();
    void black_tiger_pick_behavior();
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        black_tiger_pick_behavior();
        /* fallthrough */
    case 1:
        black_tiger_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    if (BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
        BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown)--;
    }
    if (BLACK_TIGER_FIELD(g_CurrentEntity, room_hit) != 0) {
        BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames)++;
        return;
    }
    BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0U;
}

void black_tiger_state_damaged(void) {
    void black_tiger_hit_react();
    if (g_CurrentEntity->action_behavior != 0) {
        return;
    }
    g_CurrentEntity->animation_id = 1;
    black_tiger_hit_react();
    return;
}

void black_tiger_state_death(void) {
    void black_tiger_die_burst();
    void black_tiger_die_flip();
    void black_tiger_die_writhe();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->action_behavior = ((u8) g_CurrentEntity->hit_state) >> 3;
        g_CurrentEntity->sub_state = 1;
        if (BLACK_TIGER_FIELD(g_CurrentEntity, death_behavior) != 0) {
            g_CurrentEntity->action_state = 3;
            g_CurrentEntity->action_behavior = BLACK_TIGER_FIELD(g_CurrentEntity, death_behavior);
        }
    }
    switch (g_CurrentEntity->action_behavior) {
    case 1:
    case 2:
        BLACK_TIGER_FIELD(g_CurrentEntity, death_behavior) = 1;
        black_tiger_die_writhe();
        break;
    case 6:
    case 8:
    case 9:
        BLACK_TIGER_FIELD(g_CurrentEntity, death_behavior) = 6;
        black_tiger_die_flip();
        break;
    default:
        g_CurrentEntity->animation_id = 1;
        black_tiger_die_burst();
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void black_tiger_behavior_dispatch(void) {
    void black_tiger_behavior_idle();
    void black_tiger_behavior_walk();
    void black_tiger_behavior_approach();
    void black_tiger_behavior_strafe();
    void black_tiger_behavior_circle();
    void black_tiger_behavior_bite();
    void black_tiger_behavior_spit();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) = (u16) (BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & ~1);
        BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) = (u16) (((u16) BLACK_TIGER_FIELD(g_CurrentEntity, path_flags)) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        black_tiger_behavior_idle();
        return;
    case 1:
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->angle = (g_CurrentEntity->angle + ((BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10)));
        BLACK_TIGER_FIELD(g_CurrentEntity, unused_17A) = 6;
        black_tiger_behavior_walk();
        return;
    case 2:
        black_tiger_behavior_strafe();
        return;
    case 3:
        g_CurrentEntity->animation_id = 2;
        BLACK_TIGER_FIELD(g_CurrentEntity, turn_step) = 0x80;
        black_tiger_behavior_approach();
        return;
    case 10:
        black_tiger_behavior_circle();
        return;
    case 12:
        black_tiger_behavior_bite();
        return;
    case 13:
        black_tiger_behavior_spit();
        break;
    }
}

void black_tiger_pick_behavior(void) {
    s16 saved_state;

    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    saved_state = (s16) g_CurrentEntity->action_state;
    if (BLACK_TIGER_FIELD(g_CurrentEntity, pick_cooldown) != 0) {
        if (LIT(s32, sp_tmp00) >= 0x7D1) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->action_behavior = 1;
            g_CurrentEntity->action_state = (u8) saved_state;
        }
        BLACK_TIGER_FIELD(g_CurrentEntity, pick_cooldown)--;
        return;
    }
    if ((BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames) >= 0x15) || ((s16) (BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & 1) == 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = ((g_ApiCallPtr = api_rand)() & 1) + 2;
        BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    } else {
        if (LIT(s32, sp_tmp00) < 0x1388) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xC;
        }
        if (BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) == 0) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_S16(g_CurrentEntity) = 0xD;
            }
        }
        if (LIT(s32, sp_tmp00) >= 0x2329) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->action_behavior = 1;
            g_CurrentEntity->action_state = (u8) saved_state;
        }
    }
}

void black_tiger_behavior_idle(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    }
}

void black_tiger_behavior_walk(void) {
    void black_tiger_leg_step_speed(u8 leg, s32 scale);
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) * g_CurrentEntity->action_behavior);
        if (!(((u8) g_CurrentEntity->action_behavior >> 1) & 0xFF)) {
            g_CurrentEntity->action_ticks_counter = (u16) (g_CurrentEntity->action_ticks_counter + 0x50);
        }
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_walk;
        /* fallthrough */
    case 1:
        if (black_tiger_step_se_tbl[g_CurrentEntity->animation_frame_id] != 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(black_tiger_step_se_tbl[g_CurrentEntity->animation_frame_id], 0);
        }
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if ((g_CurrentEntity->animation_frame_id != 0) && ((u8) g_CurrentEntity->animation_frame_id < 9U)) {
            black_tiger_leg_step_speed(1, g_CurrentEntity->scale);
        } else {
            black_tiger_leg_step_speed(0, g_CurrentEntity->scale);
        }
        if (g_CurrentEntity->move_speed_current >= 0x51) {
            g_CurrentEntity->move_speed_current = 0x50;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_U16(g_CurrentEntity) = 0;
            g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr;
            return;
        }
    }
}

void black_tiger_behavior_approach(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x50);
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_walk;
        /* fallthrough */
    case 1:
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, BLACK_TIGER_FIELD(g_CurrentEntity, turn_step));
                if (!((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) || (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C));
        return;
    case 2:
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr;
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
        return;
    }
}

void black_tiger_behavior_strafe(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_ApiCallPtr = api_rand)() & 0x3F);
        g_CurrentEntity->blend_counter = 3;
        BLACK_TIGER_FIELD(g_CurrentEntity, turn_step) = 0x18U;
        if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 1) != 0) {
            BLACK_TIGER_FIELD(g_CurrentEntity, turn_step) = -0x18;
        }
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_walk;
        /* fallthrough */
    case 1:
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) BLACK_TIGER_FIELD(g_CurrentEntity, turn_step)));
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if ((s16) (BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, BLACK_TIGER(g_CurrentEntity)->turn_step) == 0) && (BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) == 0)) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_S16(g_CurrentEntity) = 0xD;
                return;
            }
        }
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
            g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr;
        }
    }
}

void black_tiger_behavior_lunge_unused(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 8;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (g_CurrentEntity->action_state) + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->move_speed_current = 0x64;
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        return;
    case 2:
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0xB;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0x14;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), 0x40);
        g_CurrentEntity->angle = g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C);
        if (LIT(s32, sp_tmp1C) == 0) {
            g_CurrentEntity->action_state = 4;
        }
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 6;
        }
        return;
    case 4:
        g_CurrentEntity->action_state = 5;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0xB;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0xA;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), 0x10);
        g_CurrentEntity->move_speed_current = 0x12C;
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
                if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) && ((g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), 0x200) == 0)) {

        } else {
            g_CurrentEntity->action_state = 6;
        }
        LIT(s32, sp_tmp00) = (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + (__builtin_abs(g_PlayerEntity.pos_z - (g_CurrentEntity->pos_z)));
        if ((BLACK_TIGER_FIELD(g_CurrentEntity, player_touch) != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_is_facing_toward_entity)(LITP(s32, g_PlayerEntity));
            g_CurrentEntity->action_state = 6;
            g_PlayerEntity.health -= 0x14;
            if ((s16) g_PlayerEntity.health > 0) {
                g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp1C) + 1;
                g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp1C) + 0x66;
            }
            (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)8, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xA, 0xFA, 0);
        }
        return;
    case 6:
        g_CurrentEntity->action_state = 7;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 9;
        /* fallthrough */
    case 7:
        g_CurrentEntity->action_state = (g_CurrentEntity->action_state) + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->move_speed_current = 0x64;
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        return;
    case 8:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) = 0x1E;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void black_tiger_behavior_spit_single_unused(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xC;
        LIT(s32, sp_vec_vx) = 0x3E8;
        LIT(s32, sp_vec_vy) = -0x258;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xB;
        BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) = 0x1E;
    }
}

void black_tiger_behavior_drop_unused(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0;
        /* fallthrough */
    case 1:
                g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + ((ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) * ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)++) << 3));
        if (!(g_CurrentEntity->pos_y & 0x80000000)) {
            g_CurrentEntity->pos_y = 0;
            g_CurrentEntity->action_state = 2U;
        }
        g_CurrentEntity->rot_x = (u16) (ENTITY_AS(g_CurrentEntity, rot_x, s16) - 0x100);
        if ((s16) g_CurrentEntity->rot_x & 0x80000000) {
            g_CurrentEntity->rot_x = 0U;
            g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xFD);
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 2:
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 7;
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        return;
    case 4:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void black_tiger_behavior_nop(void) {

}

void black_tiger_behavior_circle(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_id = 6;
        BLACK_TIGER_FIELD(g_CurrentEntity, turn_step) = (s16) (0x32C - (((g_ApiCallPtr = api_rand)() & 1) * 0x658));
        BLACK_TIGER_FIELD(g_CurrentEntity, turn_step) += ((g_ApiCallPtr = api_rand)() & 0xFFF) * !(s16) (BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & 1);
        g_CurrentEntity->move_speed_current = (s16) (((g_ApiCallPtr = api_rand)() & 0x3F) + 0xFA);
        (g_ApiCallPtr = api_entity_add_speed_xz)(BLACK_TIGER_FIELD(g_CurrentEntity, turn_step));
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40));
        g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x + g_CurrentEntity->speed_vx);
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + g_CurrentEntity->speed_vy);
        g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z + g_CurrentEntity->speed_vz);
        break;
    case 2:
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = (s16) ((s16) (((u16) BLACK_TIGER_FIELD(g_CurrentEntity, path_flags)) & 1) * 4);
        BLACK_TIGER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    }
}

void black_tiger_behavior_bite(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 8;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 2:
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0xB;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0x1F) + 0x14;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), 0x40);
        g_CurrentEntity->angle = g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C);
        if (LIT(s32, sp_tmp1C) == 0) {
            g_CurrentEntity->action_state = 4;
        }
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4;
        }
        return;
    case 4:
        g_CurrentEntity->action_state = 5;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 9;
        /* fallthrough */
    case 5:
        if ((g_CurrentEntity->animation_frame_id == 0xC) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0x514, ADDR_g_PlayerPosX);
            LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0x514, ADDR_g_PlayerPosX);
            if ((LIT(s32, sp_tmp04) == 0) && (LIT(s32, sp_tmp08) == 0)) {

            } else {
                LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_is_facing_toward_entity)(LITP(s32, g_PlayerEntity));
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)4, 0);
                g_PlayerEntity.health -= 0x14;
                if ((s16) g_PlayerEntity.health < 0) {
                    g_PlayerEntity.health = 1;
                }
                if ((s16) g_PlayerEntity.health >= 0) {
                    g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp1C) + 1;
                    g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp1C) + 0x66;
                }
            }
        }
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 6:
        (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)3, 0);
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        if ((s16) g_PlayerEntity.health < 0) {
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 0x101;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xC;
        }
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void black_tiger_behavior_spit(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xC;
        g_CurrentEntity->action_ticks_counter = 0;
        LIT(s32, sp_vec_vx) = 0x3E8;
        LIT(s32, sp_vec_vy) = -0x258;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_tmp0C) = 0x100 - ((((g_ApiCallPtr = api_rand)() & 1) * 0x80) * ((g_ApiCallPtr = api_rand)() & 1));
        (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 0, -LIT(s32, sp_tmp0C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 0, LIT(s32, sp_tmp0C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 0x101;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xC;
        BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) = 0x14;
    }
}

void black_tiger_hit_react(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        if (!(((u8) g_CurrentEntity->hit_state) & 1 & 0xFF)) {
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_rand)() & 7;
            LIT(s32, sp_tmp18) = 8;
            LIT(s32, sp_tmp14) = 0;
            while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
                if (*(u8 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C)) == *(u8 *)((u8 *)&g_BlackTigerWoundedJoints + LIT(s32, sp_tmp18))) {
                    LIT(s32, sp_tmp14) = 1;
                }
            }
            if (LIT(s32, sp_tmp14) == 0) {
                joints = g_CurrentEntity->joints;
                *(u8 *)((u8 *)&g_BlackTigerWoundedJoints + (BLACK_TIGER_FIELD(g_CurrentEntity, wound_count))) = *(s32 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C));
                (g_ApiCallPtr = api_joint_setup_attack_effect)((*(u8 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C)) * 0x7C) + joints, 0x1E, 0x14, 3);
                (g_ApiCallPtr = api_joint_setup_attack_effect)((*(u8 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C)) * 0x7C) + joints + 0x7C, 0x1E, 0x14, 3);
                LIT(s32, sp_vec_vz) = 0;
                LIT(s32, sp_vec_vy) = 0;
                LIT(s32, sp_vec_vx) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 8, 0, (*(u8 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C)) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(0, 8, 0, (*(u8 *)((u8 *)&black_tiger_wound_joints + LIT(s32, sp_tmp1C)) * 0x7C) + joints + 0xC0, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
                BLACK_TIGER_FIELD(g_CurrentEntity, wound_count)++;
            }
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        return;
    case 2:
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 0x101;
        ENTITY_ACTION_S16(g_CurrentEntity) = 3;
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
        if ((s16) (BLACK_TIGER_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 0x101;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xA;
        }
        BLACK_TIGER_FIELD(g_CurrentEntity, attack_cooldown) = 0;
        return;
    }
}

void black_tiger_die_burst(void) {
    void *joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        joints = ENTITY_AS(g_CurrentEntity, joints, void *);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0, 0);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(joints, 0x1E, 0x14, (void *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1E, 0x14, (void *)3);
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, JOINT_N_WORLD(joints, 19), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        LIT(s32, sp_tmp1C) = 9;
        while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != 0) {
            if ((FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) == 0) || (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) & 0x20 & 0xFF)) {
                continue;
            }
            FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, u8) | 0xC);
            FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [3].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [3].flags, u8) | 0x10);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, (void *)0xDF809F);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0x1F4, 0x1F4);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_ticks_counter = 0x5AU;
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_dead;
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 9, 9);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
            g_CurrentEntity->action_state = 4U;
            (g_ApiCallPtr = api_entity_clone)((void *)0x33, 0xF2, (s32) g_CurrentEntity->joint_count, &g_CurrentEntity->aim_target);
        }
        return;
    case 4:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_web_spinner_update_webs)((void *)0x33);
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void black_tiger_die_flip(void) {
    void *joints;

    if ((g_CurrentEntity->action_state == 1) || (g_CurrentEntity->action_state == 3)) {
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 2 & 0xFF) {
            joints = ENTITY_AS(g_CurrentEntity, joints, void *);
            (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0, 0);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(joints, 0x1E, 0x1E, (void *)3);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1E, 0x1E, (void *)3);
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, JOINT_N_WORLD(joints, 19), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
            LIT(s32, sp_tmp1C) = 9;
            while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != 0) {
                if ((FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) == 0) || (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) & 0x20 & 0xFF)) {
                    continue;
                }
                FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) | 0xC);
                FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, u8) | 0x10);
            }
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 3;
            g_CurrentEntity->action_state = 2U;
            return;
        }
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 1 & 0xFF) {
            g_CurrentEntity->action_state = 4U;
        }
    }
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        g_CurrentEntity->animation_id = 4;
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0x708, 0x708);
        /* fallthrough */
    case 1:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        if ((u8) g_CurrentEntity->animation_frame_id >= 0xEU) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        g_CurrentEntity->action_ticks_counter = 0xB4U;
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        /* fallthrough */
    case 3:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
            ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0x11U;
        }
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        return;
    case 4:
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, (void *)0xDF809F);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0x7D0, 0x7D0);
        g_CurrentEntity->action_ticks_counter = 0x64U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_dead;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 4, 4);
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state = 6U;
            (g_ApiCallPtr = api_entity_clone)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1, 0xF2, (s32) g_CurrentEntity->joint_count, &g_CurrentEntity->aim_target);
        }
        return;
    case 6:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1);
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void black_tiger_die_writhe(void) {
    extern u8 black_tiger_wound_joints_u8[] __asm__("black_tiger_wound_joints");
    void *joints;

    if ((g_CurrentEntity->action_state == 1) || (g_CurrentEntity->action_state == 3)) {
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 2 & 0xFF) {
            joints = ENTITY_AS(g_CurrentEntity, joints, void *);
            (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0, 0);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(joints, 0x1E, 0x1E, (void *)3);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1E, 0x1E, (void *)3);
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, JOINT_N_WORLD(joints, 19), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
            LIT(s32, sp_tmp1C) = 9;
            while (--LIT(s32, sp_tmp1C) != 0) {
                if ((FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) == 0) || (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) & 0x20 & 0xFF)) {
                    continue;
                }
                FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) | 0xC);
                FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, u8) | 0x10);
            }
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 3;
            g_CurrentEntity->action_state = 2U;
            return;
        }
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 1 & 0xFF) {
            g_CurrentEntity->action_state = 4U;
        }
    }
    switch (g_CurrentEntity->action_state) {
    case 0:
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0U;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        g_CurrentEntity->animation_id = 1;
        joints = ENTITY_AS(g_CurrentEntity, joints, void *);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0x5DC, 0x5DC);
        if (BLACK_TIGER_FIELD(g_CurrentEntity, wound_count) < 6) {
            (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1E, 0x1E, (void *)3);
        }
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0xB, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0xB, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        g_CurrentEntity->animation_id = 7;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0xD2U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        g_CurrentEntity->action_state = 3U;
        /* fallthrough */
    case 3:
        if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- & 1) != 0) {
            (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        }
        if (g_CurrentEntity->animation_frame_id == 0) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            joints = ENTITY_AS(g_CurrentEntity, joints, void *);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0xB, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0xB, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
        }
        if ((s16) g_CurrentEntity->action_ticks_counter == 0) {
            g_CurrentEntity->action_state = 4U;
            ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0x11U;
        }
        return;
    case 4:
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        g_CurrentEntity->action_ticks_counter = 0x64U;
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x78U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->sca_info = (s32) black_tiger_sca_info_ptr_dead;
        (g_ApiCallPtr = api_entity_clone)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1, 0xF2, (s32) g_CurrentEntity->joint_count, &g_CurrentEntity->aim_target);
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1);
        --g_CurrentEntity->move_speed_current;
        LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 7);
        (g_ApiCallPtr = api_entity_add_speed_xz)((black_tiger_wound_joints_u8[LIT(s32, sp_tmp1C)] * ((((g_ApiCallPtr = api_rand)() & 1) + 1) * ({ 0x64; }))));
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state = 6U;
        }
        return;
    case 6:
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0U;
        g_CurrentEntity->action_state = 7U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 4;
        /* fallthrough */
    case 7:
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1);
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            g_CurrentEntity->action_state = 8U;
            g_CurrentEntity->action_ticks_counter = 0x3CU;
        }
        return;
    case 8:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 5) + 1);
        break;
    }
}

void black_tiger_leg_step_speed(u8 leg, s32 scale) {
    void *joint;
    u8 i;
    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_CompMatrix)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    if (scale != 0) {
        LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = scale;
        (g_ApiCallPtr = api_ScaleMatrix)(LITP(void, sp_matrix), LITP(void, sp_vec));
    }
    joint += (leg * 0x3E0) + 0x1F0;
    if (!(JOINT(joint)->flags & 1 & 0xFF)) {
        g_CurrentEntity->move_speed_current = 0;
        return;
    }
    i = 2;
    while (i != 0) {
        (g_ApiCallPtr = api_CompMatrix)(LITP(void, sp_matrix), ((i * 0x7C) + joint) - 0x58, ADDR_sp_matrix);
        i--;
    }
    LIT(s32, sp_matrix_t0) = (s32) (LIT(s32, sp_matrix_t0) - JOINT(joint)->world.t[0]);
    LIT(s32, sp_matrix_t1) = 0;
    LIT(s32, sp_matrix_t2) = (s32) (LIT(s32, sp_matrix_t2) - JOINT(joint)->world.t[2]);
    (g_ApiCallPtr = api_Square0)(LITP(void, sp_matrix_t0), LITP(void, sp_vec));
    g_CurrentEntity->move_speed_current = (g_ApiCallPtr = api_SquareRoot0)(LIT(s32, sp_vec_vx) + LIT(s32, sp_vec_vz));
}
