/* computer_arms.c (stage5) - the player's two arms seen in first-person scenes of the lab:
   typing the computer login, pressing keys, pulling the lever, lifting; driven by script commands
   (arm_cmd_* / arm15_cmd_*: right and left arm). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* ComputerArmRightWork: the arm_right's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). ARM_RIGHT(e)->f, ARM_RIGHT_FIELD(e, f) and ARM_RIGHT_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct ComputerArmRightWork {
    /* +0x00 */ s32 vel_x; /* 0x16C: x velocity, 16.16 (arm_begin_reach: delta/15, arm_set_velocity: delta/frames) */
    /* +0x04 */ s32 vel_z; /* 0x170: z velocity, 16.16 (arm_set_velocity, arm_integrate) */
    /* +0x08 */ s32 pos_x; /* 0x174: x position accumulator, 16.16 (arm_state_init, arm_integrate) */
    /* +0x0C */ s32 pos_z; /* 0x178: z position accumulator, 16.16 (arm_state_init, arm_integrate) */
} ComputerArmRightWork;
#define ARM_RIGHT(e) ENEMY_WORK(e, ComputerArmRightWork)
#define ARM_RIGHT_FIELD(e, f) ENEMY_FIELD(e, ComputerArmRightWork, f)
#define ARM_RIGHT_MEMBER(e, f) ENEMY_MEMBER(e, ComputerArmRightWork, f)

/* ComputerArmLeftWork: the arm_left's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). ARM_LEFT(e)->f, ARM_LEFT_FIELD(e, f) and ARM_LEFT_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct ComputerArmLeftWork {
    /* +0x00 */ s32 vel_x;        /* 0x16C: x velocity, 16.16 (arm15_set_velocity: delta/frames) */
    /* +0x04 */ s32 vel_y;        /* 0x170: fall velocity added to pos_y (arm15_cmd7_drop/_fall, gravity +0x28000) */
    /* +0x08 */ s32 vel_z;        /* 0x174: z velocity (arm15_set_velocity, arm15_integrate) */
    /* +0x0C */ s32 pos_x;        /* 0x178: x position accumulator (arm15_state_init, arm15_integrate) */
    /* +0x10 */ s32 pos_y;        /* 0x17C: y position while falling (arm15_cmd7_drop/_fall, stops at scd_pos_y) */
    /* +0x14 */ s32 pos_z;        /* 0x180: z position accumulator (arm15_state_init, arm15_set_velocity, arm15_integrate) */
    /* +0x18 */ s16 slide_frames; /* 0x184: 0xA or 0x12 by player character (arm_step_login15); slide duration for arm15_set_velocity */
} ComputerArmLeftWork;
#define ARM_LEFT(e) ENEMY_WORK(e, ComputerArmLeftWork)
#define ARM_LEFT_FIELD(e, f) ENEMY_FIELD(e, ComputerArmLeftWork, f)
#define ARM_LEFT_MEMBER(e, f) ENEMY_MEMBER(e, ComputerArmLeftWork, f)

/* every frame for each computer arm right (enemies_update_functions_tbl): runs arm_state_tbl[state], then collision checks and
   the shadow / drawing */
void computer_arm_right_update(void) {
    extern s32 (*arm_state_tbl[])();

    arm_state_tbl[g_CurrentEntity->state]();
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->has_enter_switch_zone = (!(g_CurrentEntity->behavior_flags & 0x80 & 0xFF)) ? (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones) : 0;
}

/* state 0: sets up a new arm (health, sca info, animation, work fields), then state 1 */
void arm_state_init(void) {

    g_CurrentEntity->state++;
    g_CurrentEntity->sub_state = 0;
    g_CurrentEntity->health = 1;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 4);
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    g_CurrentEntity->scd_pos_x = (s32) g_CurrentEntity->pos_x;
    g_CurrentEntity->scd_pos_z = (s32) g_CurrentEntity->pos_z;
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) = (s32) (g_CurrentEntity->pos_x << 0x10);
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) = (s32) (g_CurrentEntity->pos_z << 0x10);
}

void arm_state_run(void) {
    extern s32 (*arm_cmd_tbl[])();
    if ((g_CurrentEntity->behavior_flags & 0x80 & 0xFF) || (g_CurrentEntity->behavior_flags & 0x20 & 0xFF)) {
        g_CurrentEntity->sub_state = 0;
    } else {
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xBF);
        }
        arm_cmd_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
    }
}

void arm_cmd_rest(void) {
    s32 arm_set_velocity_a1() __asm__("arm_set_velocity");
    void arm_integrate();

    if (ENTITY_AS(g_CurrentEntity, sub_state, u8) == 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        g_CurrentEntity->blend_counter = (ENTITY_AS(g_CurrentEntity, animation_id, u8) != 0) ? 7 : 0;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = 0U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 8U;
        arm_set_velocity_a1(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x, g_CurrentEntity->scd_pos_z, (s32) g_CurrentEntity->action_ticks_counter);
        if (((__builtin_abs(g_CurrentEntity->pos_x - g_CurrentEntity->scd_pos_x)) + (__builtin_abs(g_CurrentEntity->pos_z - g_CurrentEntity->scd_pos_z))) < 0xC8) {
            g_CurrentEntity->action_ticks_counter = 0U;
        }
    }
    if ((s16) g_CurrentEntity->action_ticks_counter != 0) {
        arm_integrate();
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
}

void arm_cmd_reach_a(void) {
    extern s32 (*arm_cmd_reach_a_tbl[])();
    arm_cmd_reach_a_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_reach_a(void) {
    s32 arm_begin_reach_a1(s32, s32, s32) __asm__("arm_begin_reach");
    g_CurrentEntity->sub_state++;
    arm_begin_reach_a1(-0x28, 0x28, 1);
}

void arm_step_move(void) {
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) = (s32) (ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) + ARM_RIGHT_FIELD(g_CurrentEntity, vel_x));
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) = (s32) (ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) + ARM_RIGHT_FIELD(g_CurrentEntity, vel_z));
    g_CurrentEntity->pos_x = (s32) ((s32) ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) >> 0x10);
    g_CurrentEntity->pos_z = (s32) ((s32) ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) >> 0x10);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
    if (g_PlayerEntity.id & 1 & 0xFF) {
        if ((g_CurrentEntity->animation_frame_id != 3) && (g_CurrentEntity->animation_frame_id != 0xD)) {

        } else {
            (g_ApiCallPtr = api_play_sfx)(2, 0x18, 0);
        }
        return;
    }
    if (g_CurrentEntity->animation_frame_id == 8) {
        (g_ApiCallPtr = api_play_sfx)(2, 0x18, 0);
    }
}

void arm_cmd_reach_b(void) {
    extern s32 (*arm_cmd_reach_b_tbl[])();
    arm_cmd_reach_b_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_reach_b(void) {
    s32 arm_begin_reach_a2(s32, s32, s32) __asm__("arm_begin_reach");

    g_CurrentEntity->sub_state++;
    arm_begin_reach_a2(0x28, 0, ((g_PlayerEntity.id & 1 & 0xFF) ? 2 : 1));
}

void arm_cmd_reach_c(void) {
    extern s32 (*arm_cmd_reach_c_tbl[])();
    arm_cmd_reach_c_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_reach_c(void) {
    s32 arm_begin_reach_a3(s32, s32, s32) __asm__("arm_begin_reach");

    g_CurrentEntity->sub_state++;
    arm_begin_reach_a3(-0x14, -0x28, ((g_PlayerEntity.id & 1 & 0xFF) ? 3 : 2));
}

void arm_cmd_login(void) {
    extern s32 (*arm_cmd_login_tbl[])();
    arm_cmd_login_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_login(void) {
    s32 arm_set_velocity_a2(s32, s32, s32, s32, s32) __asm__("arm_set_velocity");
    g_CurrentEntity->sub_state++;
    g_CurrentEntity->action_behavior = 0;
    g_CurrentEntity->animation_id = 4;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    if (!(g_PlayerEntity.id & 1 & 0xFF)) {
        arm_set_velocity_a2(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->pos_x, g_CurrentEntity->scd_pos_z + 0xC8, 0x12);
        (g_ApiCallPtr = api_voice_command)(1, 0xB8);
        g_GameStateFlags |= GAME_VOICE_PLAYING;
        return;
    }
    (g_ApiCallPtr = api_voice_command)(1, 0xB9);
    g_GameStateFlags |= GAME_VOICE_PLAYING;
}

void arm_step_typing(void) {
    void arm_step_typing_jill();
    extern s32 (*arm_step_typing_tbl[])();
    if (g_PlayerEntity.id & 1 & 0xFF) {
        arm_step_typing_jill();
        return;
    }
    arm_step_typing_tbl[g_CurrentEntity->action_behavior]();
}

void arm_step_press(void) {
    void arm_integrate();
    u8 frame;

    frame = g_CurrentEntity->animation_frame_id;
    if (frame < 0x12U) {
        arm_integrate();
    }
    if ((frame != 0x15) && (frame != 0x22)) {

    } else {
        (g_ApiCallPtr = api_play_sfx)(2, 0x1A, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->action_ticks_counter = 4;
    }
}

void arm_step_wait_voice(void) {
    if (!(g_GameStateFlags & GAME_VOICE_PLAYING)) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
}

void arm_step_typing_jill(void) {
    if (g_CurrentEntity->action_behavior == 0) {
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->action_behavior++;
        }
        return;
    }
    if (!(g_GameStateFlags & GAME_VOICE_PLAYING)) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
}

void arm_cmd_lift(void) {
    extern s32 (*arm_cmd_lift_tbl[])();
    if (g_PlayerEntity.id & 1 & 0xFF) {
        return;
    }
    arm_cmd_lift_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_lift(void) {
    s32 arm_set_velocity_a3(s32, s32, s32, s32, s32) __asm__("arm_set_velocity");
    g_CurrentEntity->sub_state++;
    g_CurrentEntity->action_ticks_counter = 4U;
    arm_set_velocity_a3(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x, g_CurrentEntity->scd_pos_z + 0x50, (s32) g_CurrentEntity->action_ticks_counter);
}

void arm_step_lift_wait(void) {
    void arm_integrate();

    arm_integrate();
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        g_CurrentEntity->animation_id = 5;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
    }
}

void arm_step_withdraw(void) {
    s32 arm_set_velocity_a4() __asm__("arm_set_velocity");
    if (!(g_PlayerEntity.id & 1 & 0xFF) && (g_CurrentEntity->animation_frame_id == 0xF)) {
        (g_ApiCallPtr = api_play_sfx)(2, 0x1C, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->sub_state++;
        g_CurrentEntity->action_ticks_counter = 4U;
        arm_set_velocity_a4(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x, g_CurrentEntity->scd_pos_z, (s32) g_CurrentEntity->action_ticks_counter);
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
    }
}

void arm_step_settle(void) {
    void arm_integrate();

    arm_integrate();
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
}

void arm_cmd_reach_far(void) {
    extern s32 (*arm_cmd_reach_far_tbl[])();
    arm_cmd_reach_far_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_reach_far(void) {
    s32 arm_begin_reach_a4(s32, s32, s32) __asm__("arm_begin_reach");

    g_CurrentEntity->sub_state++;
    arm_begin_reach_a4(-0x14, 0xA0, ((g_PlayerEntity.id & 1 & 0xFF) ? 3 : 2));
}

void arm_cmd_hold(void) {
    extern s32 (*arm_cmd_hold_tbl[])();
    arm_cmd_hold_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_hold(void) {

    g_CurrentEntity->sub_state++;
    g_CurrentEntity->action_ticks_counter = 0x96;
    g_CurrentEntity->blend_counter = (((u8) g_CurrentEntity->animation_id) != 0) ? 7 : 0;
    g_CurrentEntity->animation_id = 0U;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
}

void arm_step_hold_wait(void) {
    s32 arm_set_velocity_a5(s32, s32, s32, s32, s32) __asm__("arm_set_velocity");
    s32 off_z;

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        return;
    }
    ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
    g_CurrentEntity->animation_id = 4;
    ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = (!(g_PlayerEntity.id & 1 & 0xFF)) ? 0x28 : 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x1F;
    g_CurrentEntity->action_ticks_counter = 0x18U;
    off_z = (g_PlayerEntity.id & 1 & 0xFF) ? 0x8C : 0x46;
    arm_set_velocity_a5(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x - 0x64, g_CurrentEntity->scd_pos_z + off_z, (s32) g_CurrentEntity->action_ticks_counter);
}

void arm_step_hold_move(void) {
    void arm_integrate();

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    if (g_PlayerEntity.id & 1 & 0xFF) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 6U) {
            g_CurrentEntity->animation_frame_id = 5U;
        }
    } else {
        g_CurrentEntity->animation_frame_id = 0x28U;
    }
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        arm_integrate();
        return;
    }
    ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
}

void arm_step_idle_end(void) {

}

void arm_begin_reach(s32 off_x, s32 off_z, u8 anim) {
    s32 arm_set_velocity_a6(s32, s32, s32, s32, s32) __asm__("arm_set_velocity");
    u8 cur_anim;

    cur_anim = g_CurrentEntity->animation_id;
    if ((anim == 2) && (cur_anim == 1)) {
        anim = 3;
    } else if ((anim == 3) && (cur_anim == 3)) {
        anim = 2;
    }
    g_CurrentEntity->animation_id = anim;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    ARM_RIGHT_FIELD(g_CurrentEntity, vel_x) = (s32) ((((g_CurrentEntity->scd_pos_x + off_x) - g_CurrentEntity->pos_x) << 0x10) / 15);
    ARM_RIGHT_FIELD(g_CurrentEntity, vel_z) = (s32) ((((g_CurrentEntity->scd_pos_z + off_z) - g_CurrentEntity->pos_z) << 0x10) / 15);
    arm_set_velocity_a6(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x + off_x, g_CurrentEntity->scd_pos_z + off_z, 0xF);
}

void arm_set_velocity(s32 from_x, s32 from_z, s32 to_x, s32 to_z, u16 frames) {
    ARM_RIGHT_FIELD(g_CurrentEntity, vel_x) = ((to_x - from_x) << 0x10) / frames;
    ARM_RIGHT_FIELD(g_CurrentEntity, vel_z) = ((to_z - from_z) << 0x10) / frames;
}

void arm_integrate(void) {
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) = (s32) (ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) + ARM_RIGHT_FIELD(g_CurrentEntity, vel_x));
    ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) = (s32) (ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) + ARM_RIGHT_FIELD(g_CurrentEntity, vel_z));
    g_CurrentEntity->pos_x = (s32) ((s32) ARM_RIGHT_FIELD(g_CurrentEntity, pos_x) >> 0x10);
    g_CurrentEntity->pos_z = (s32) ((s32) ARM_RIGHT_FIELD(g_CurrentEntity, pos_z) >> 0x10);
}

/* every frame for each computer arm left (enemies_update_functions_tbl): runs arm15_state_tbl[state], then collision checks and
   the shadow / drawing */
void computer_arm_left_update(void) {
    extern s32 (*arm15_state_tbl[])();

    arm15_state_tbl[g_CurrentEntity->state]();
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->has_enter_switch_zone = (!(g_CurrentEntity->behavior_flags & 0x80 & 0xFF)) ? (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones) : 0;
}

/* state 0: sets up a new arm15 (health, sca info, animation, work fields), then state 1 */
void arm15_state_init(void) {

    g_CurrentEntity->state++;
    g_CurrentEntity->sub_state = 0;
    g_CurrentEntity->health = 1;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 4);
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    g_CurrentEntity->scd_pos_x = (s32) g_CurrentEntity->pos_x;
    g_CurrentEntity->scd_pos_y = (s32) g_CurrentEntity->pos_y;
    g_CurrentEntity->scd_pos_z = (s32) g_CurrentEntity->pos_z;
    ARM_LEFT_FIELD(g_CurrentEntity, pos_x) = (s32) (g_CurrentEntity->pos_x << 0x10);
    ARM_LEFT_FIELD(g_CurrentEntity, pos_z) = (s32) (g_CurrentEntity->pos_z << 0x10);
}

void arm15_state_run(void) {
    extern s32 (*arm15_cmd_tbl[])();
    if ((g_CurrentEntity->behavior_flags & 0x80 & 0xFF) || (g_CurrentEntity->behavior_flags & 0x20 & 0xFF)) {
        g_CurrentEntity->sub_state = 0;
    } else {
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xBF);
        }
        arm15_cmd_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
    }
}

void arm15_cmd_rest(void) {
    s32 arm15_set_velocity_a1(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    void arm15_integrate();

    if (ENTITY_AS(g_CurrentEntity, sub_state, u8) == 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        g_CurrentEntity->blend_counter = (ENTITY_AS(g_CurrentEntity, animation_id, u8) != 0) ? 7 : 0;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = 0U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 8;
        arm15_set_velocity_a1(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x, g_CurrentEntity->scd_pos_z, (s32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16));
    }
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        arm15_integrate();
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
}

void arm15_cmd_reach_a(void) {
    extern s32 (*arm15_cmd_reach_a_tbl[])();
    arm15_cmd_reach_a_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_step_reach_a(void) {
    s32 arm15_begin_reach_a1(s32, s32, s32) __asm__("arm15_begin_reach");
    g_CurrentEntity->sub_state++;
    arm15_begin_reach_a1(-0x32, -0x32, 1);
}

void arm15_step_move(void) {
    ARM_LEFT_FIELD(g_CurrentEntity, pos_x) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_x) + ARM_LEFT_FIELD(g_CurrentEntity, vel_x));
    ARM_LEFT_FIELD(g_CurrentEntity, pos_z) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_z) + ARM_LEFT_FIELD(g_CurrentEntity, vel_z));
    g_CurrentEntity->pos_x = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_x) >> 0x10);
    g_CurrentEntity->pos_z = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_z) >> 0x10);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
    if (g_PlayerEntity.id & 1 & 0xFF) {
        if ((g_CurrentEntity->animation_frame_id != 3) && (g_CurrentEntity->animation_frame_id != 0xD)) {

        } else {
            (g_ApiCallPtr = api_play_sfx)(2, 0x18, 0);
        }
        return;
    }
    if (g_CurrentEntity->animation_frame_id == 8) {
        (g_ApiCallPtr = api_play_sfx)(2, 0x18, 0);
    }
}

void arm15_cmd_reach_b(void) {
    extern s32 (*arm15_cmd_reach_b_tbl[])();
    arm15_cmd_reach_b_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_step_reach_b(void) {
    s32 arm15_begin_reach_a2(s32, s32, s32) __asm__("arm15_begin_reach");

    g_CurrentEntity->sub_state++;
    arm15_begin_reach_a2(0x32, 0, ((g_PlayerEntity.id & 1 & 0xFF) ? 2 : 1));
}

void arm15_cmd_reach_c(void) {
    extern s32 (*arm15_cmd_reach_c_tbl[])();
    arm15_cmd_reach_c_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_step_reach_c(void) {
    s32 arm15_begin_reach_a3(s32, s32, s32) __asm__("arm15_begin_reach");

    g_CurrentEntity->sub_state++;
    arm15_begin_reach_a3(-0x14, 0x32, ((g_PlayerEntity.id & 1 & 0xFF) ? 3 : 2));
}

void arm15_cmd_login(void) {
    extern s32 (*arm15_cmd_login_tbl[])();
    arm15_cmd_login_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_login15(void) {
    s32 arm15_set_velocity_a2(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    s16 off_z;

    g_CurrentEntity->sub_state++;
    g_CurrentEntity->animation_id = 4;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    if (g_PlayerEntity.id & 1 & 0xFF) {
        ARM_LEFT_FIELD(g_CurrentEntity, slide_frames) = 0xA;
        off_z = 0xC8;
    } else {
        ARM_LEFT_FIELD(g_CurrentEntity, slide_frames) = 0x12;
        off_z = 0x12C;
    }
    arm15_set_velocity_a2(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->pos_x, g_CurrentEntity->scd_pos_z - off_z, (s32) ARM_LEFT_FIELD(g_CurrentEntity, slide_frames));
}

void arm_step_typing15(void) {
    void arm15_integrate();
    if ((s32) g_CurrentEntity->animation_frame_id < ARM_LEFT_FIELD(g_CurrentEntity, slide_frames)) {
        arm15_integrate();
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
    if ((g_PlayerEntity.id & 1 & 0xFF) && (g_CurrentEntity->animation_frame_id == 0xD)) {
        (g_ApiCallPtr = api_play_sfx)(2, 0x1B, 0);
    }
}

void arm_step_settle15(void) {
    void arm15_integrate();

    arm15_integrate();
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
}

void arm15_cmd5(void) {
    extern s32 (*arm15_cmd5_tbl[])();
    arm15_cmd5_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_cmd5_begin(void) {
    g_CurrentEntity->sub_state++;
    g_CurrentEntity->action_behavior = 0;
}

void arm15_cmd5_dispatch(void) {
    extern s32 (*arm15_cmd5_chris_tbl[])();
    extern s32 (*arm15_cmd5_jill_tbl[])();
    if (!(g_PlayerEntity.id & 1 & 0xFF)) {
        arm15_cmd5_chris_tbl[g_CurrentEntity->action_behavior]();
    } else {
        arm15_cmd5_jill_tbl[g_CurrentEntity->action_behavior]();
    }
}

void arm15_cmd5_chris_slide(void) {
    s32 arm15_set_velocity_a3(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->action_ticks_counter = 4;
    arm15_set_velocity_a3(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x, g_CurrentEntity->scd_pos_z - 0x50, (s32) ((s16) g_CurrentEntity->action_ticks_counter));
}

void arm15_cmd5_chris_wait(void) {
    s32 arm15_set_velocity_a4() __asm__("arm15_set_velocity");
    void arm15_integrate();

    arm15_integrate();
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = 5;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->action_ticks_counter = 4U;
        arm15_set_velocity_a4(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x - 0x64, g_CurrentEntity->pos_z, (s32) (s16) g_CurrentEntity->action_ticks_counter);
    }
}

void arm15_cmd5_play(void) {
    void arm15_integrate();
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        arm15_integrate();
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
        g_CurrentEntity->sub_state = 0;
    }
}

void arm15_cmd5_jill_start(void) {
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->animation_id = 5;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
}

void arm15_cmd6(void) {
    extern s32 (*arm15_cmd6_tbl[])();
    arm15_cmd6_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_cmd6_reach(void) {
    s32 arm15_begin_reach_a4(s32, s32, s32) __asm__("arm15_begin_reach");
    s32 arm15_set_velocity_a5(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    g_CurrentEntity->sub_state++;
    arm15_begin_reach_a4(-0x28, 0, 6);
    arm15_set_velocity_a5(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x - 0x28, g_CurrentEntity->scd_pos_z - 0x28, 4);
    g_CurrentEntity->action_ticks_counter = 4;
}

void arm_step_lever_move(void) {

    ARM_LEFT_FIELD(g_CurrentEntity, pos_x) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_x) + ARM_LEFT_FIELD(g_CurrentEntity, vel_x));
    ARM_LEFT_FIELD(g_CurrentEntity, pos_z) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_z) + ARM_LEFT_FIELD(g_CurrentEntity, vel_z));
    g_CurrentEntity->pos_x = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_x) >> 0x10);
    g_CurrentEntity->pos_z = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_z) >> 0x10);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        g_CurrentEntity->animation_id = 6;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
}

void arm_step_lever_pull(void) {
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x20);
    }
    if (g_CurrentEntity->animation_frame_id == 9) {
        (g_ApiCallPtr = api_play_sfx)(2, 0x18, 0);
    }
}

void arm15_cmd7(void) {
    extern s32 (*arm15_cmd7_tbl[])();
    arm15_cmd7_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm15_cmd7_hold(void) {

    g_CurrentEntity->sub_state++;
    g_CurrentEntity->action_ticks_counter = 0xA0;
    g_CurrentEntity->blend_counter = (((u8) g_CurrentEntity->animation_id) != 0) ? 7 : 0;
    g_CurrentEntity->animation_id = 0U;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
}

void arm15_cmd7_drop(void) {
    s32 arm15_set_velocity_a6(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        return;
    }
    ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
    g_CurrentEntity->action_ticks_counter = 0xCU;
    ARM_LEFT_FIELD(g_CurrentEntity, pos_y) = (s32) (g_CurrentEntity->pos_y << 0x10);
    ARM_LEFT_FIELD(g_CurrentEntity, vel_y) = 0xFFF40000;
    arm15_set_velocity_a6(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x - 0x64, g_CurrentEntity->scd_pos_z - 0x190, (s32) (s16) g_CurrentEntity->action_ticks_counter);
}

void arm15_cmd7_fall(void) {
    void arm15_integrate();

    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        arm15_integrate();
        ARM_LEFT_FIELD(g_CurrentEntity, pos_y) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_y) + ARM_LEFT_FIELD(g_CurrentEntity, vel_y));
        ARM_LEFT_FIELD(g_CurrentEntity, vel_y) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, vel_y) + 0x28000);
        if ((g_CurrentEntity->pos_y = (s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_y) >> 0x10) > g_CurrentEntity->scd_pos_y) {
            g_CurrentEntity->pos_y = (s32) g_CurrentEntity->scd_pos_y;
        }
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
        return;
    }
    ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
    g_CurrentEntity->pos_y = (s32) g_CurrentEntity->scd_pos_y;
    g_CurrentEntity->animation_id = 2;
    ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0xE;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0x5A;
}

void arm15_cmd7_loop(void) {
    u8 end_frame;
    u8 is_jill;

    is_jill = g_PlayerEntity.id & 1;
    end_frame = (is_jill != 0) ? 0xA : 0xE;
    if ((u8) g_CurrentEntity->animation_frame_id < end_frame) {
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    }
    if ((s16) --ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->action_ticks_counter = (is_jill == 0) ? ((g_ApiCallPtr = api_rand)() & 0x1F) + 0xA : 0xA;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
    }
}

void arm15_cmd8_lever(void) {
    extern s32 (*arm15_cmd8_lever_tbl[])();
    arm15_cmd8_lever_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void arm_step_lever_reach(void) {
    s32 arm15_begin_reach_a5(s32, s32, s32) __asm__("arm15_begin_reach");
    s32 arm15_set_velocity_a7(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    g_CurrentEntity->sub_state++;
    arm15_begin_reach_a5(-0x28, 0, 6);
    arm15_set_velocity_a7(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x - 0x28, g_CurrentEntity->scd_pos_z - 0x8C, 4);
    g_CurrentEntity->action_ticks_counter = 4;
}

void arm15_begin_reach(s32 off_x, s32 off_z, u8 anim) {
    s32 arm15_set_velocity_a8(s32, s32, s32, s32, s32) __asm__("arm15_set_velocity");
    u8 cur_anim;

    cur_anim = g_CurrentEntity->animation_id;
    if ((anim == 2) && (cur_anim == 1)) {
        anim = 3;
    } else if ((anim == 3) && (cur_anim == 3)) {
        anim = 2;
    }
    g_CurrentEntity->animation_id = anim;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    arm15_set_velocity_a8(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z, g_CurrentEntity->scd_pos_x + off_x, g_CurrentEntity->scd_pos_z + off_z, 0xF);
}

void arm15_set_velocity(s32 from_x, s32 from_z, s32 to_x, s32 to_z, s16 frames) {
    ARM_LEFT_FIELD(g_CurrentEntity, vel_x) = ((to_x - from_x) << 0x10) / frames;
    ARM_LEFT_FIELD(g_CurrentEntity, vel_z) = ((to_z - from_z) << 0x10) / frames;
    ARM_LEFT_FIELD(g_CurrentEntity, pos_x) = g_CurrentEntity->pos_x << 0x10;
    ARM_LEFT_FIELD(g_CurrentEntity, pos_z) = g_CurrentEntity->pos_z << 0x10;
}

void arm15_integrate(void) {
    ARM_LEFT_FIELD(g_CurrentEntity, pos_x) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_x) + ARM_LEFT_FIELD(g_CurrentEntity, vel_x));
    ARM_LEFT_FIELD(g_CurrentEntity, pos_z) = (s32) (ARM_LEFT_FIELD(g_CurrentEntity, pos_z) + ARM_LEFT_FIELD(g_CurrentEntity, vel_z));
    g_CurrentEntity->pos_x = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_x) >> 0x10);
    g_CurrentEntity->pos_z = (s32) ((s32) ARM_LEFT_FIELD(g_CurrentEntity, pos_z) >> 0x10);
}
