/* plant42.c (stage4) - Plant 42 (boss): the bulb body that takes the damage, vines that grab and
   throw the player, the roots clone it draws, script (SCD) states and the player's grab / throw
   animations. It can only die after the V-JOLT (sets SCN_FLAG_PLANT42_DEAD). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Plant 42 data */
extern s32 plant42_sca_info_ptr_vine;
extern s32 plant42_sca_info_ptr_main;
extern s32 g_Plant42CaptureMatrix;
extern s32 g_Plant42CaptureMatrix_tx;
extern s32 g_Plant42CaptureMatrix_ty;
extern s32 g_Plant42CaptureMatrix_tz;
extern s16 plant42_effect_x[];
extern s16 plant42_effect_z[];
extern u8 plant42_idle_anim_tbl[];
extern s32 plant42_chris_hold_matrix;
extern u8 plant42_pulse_tbl[];

/* Plant42Work: the plant42's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). PLANT42(e)->f, PLANT42_FIELD(e, f) and PLANT42_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct Plant42Work {
    /* +0x00 */ s8 yaw_step;              /* 0x16C: signed yaw step: random 0..15 added to angle (plant42_behavior_idle), +-1 sway decay (suspend), +-0x20 spin (death) */
    /* +0x01 */ s8 tilt_step;             /* 0x16D: signed tilt step: random 0..7 for angle_z (idle), +-1 decay of tilt_swing (suspend), +-0x40 for rot_x (death) */
    /* +0x02 */ u16 yaw_swing;            /* 0x16E: suspend: yaw swing subtracted from angle (starts 3); death: target yaw (0 or 0x800) the spin stops at */
    /* +0x04 */ u16 tilt_swing;           /* 0x170: suspend: tilt swing subtracted from angle_z (starts 6); death: target rot_x (0x400/0xC00) */
    /* +0x06 */ s16 rot_speed;            /* 0x172: per-behavior rotation speed: sweep ramp (+4, then +0x1C), spit/move/hold/release angle_z step, kill/recoil turn; suspend frame countdown */
    /* +0x08 */ s8 sweep_dir;             /* 0x174: sweep direction 0/1, index into the sweep table (plant42_behavior_sweep); checked in plant42_sweep_hit */
    /* +0x09 */ s8 grab_joint;            /* 0x175: joint index (15) the captured player is held at (move/hold matrices); recoil reuses it as a +-8 swing step */
    /* +0x0A */ s8 unused_176;            /* 0x176: only cleared in plant42_init */
    /* +0x0B */ s8 scd_effect_timer;      /* 0x177: script-driven countdown (10 on flinch/recoil); at 1 spawns debris effects + SE at the body, re-arms 13 (plant42_scd_state) */
    /* +0x0C */ void *roots;              /* 0x178: roots clone entity made by api_entity_clone (plant42_init); plant42_render_copies runs plant42_roots_clone_state_tbl on it */
    /* +0x10 */ u32 player_distance;      /* 0x17C: api_SquareRoot0 of dx^2+dz^2 to the player (plant42_state_check_body); also handed to plant42_action_dispatch */
    /* +0x14 */ s16 ambient_effect_timer; /* 0x180: random 0x32.. countdown; near 1/<0x1E (and every 50) spawns falling effects around the player when far away */
    /* +0x16 */ u8 death_wilt_timer;      /* 0x182: plant42_behavior_death phase 1 length (15..28 frames of tilting) before it falls */
    /* +0x17 */ u8 death_bounce_count;    /* 0x183: 5 at death; one per floor bounce while falling, plays the thud SE/effects at 4, ends the fall at 0 */
} Plant42Work;
#define PLANT42(e) ENEMY_WORK(e, Plant42Work)
#define PLANT42_AS(e, f, T) ENEMY_WORK_AS(e, Plant42Work, f, T)
#define PLANT42_FIELD(e, f) ENEMY_FIELD(e, Plant42Work, f)
#define PLANT42_MEMBER(e, f) ENEMY_MEMBER(e, Plant42Work, f)

/* Plant42BodyWork: the plant42_body's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). PLANT42_BODY(e)->f, PLANT42_BODY_FIELD(e, f) and PLANT42_BODY_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct Plant42BodyWork {
    /* +0x00 */ s32 scale_x; /* 0x16C: x scale (0x1000 = 1.0; pulse/shrivel/fall) */
    /* +0x04 */ s32 scale_y; /* 0x170: y scale */
    /* +0x08 */ s32 scale_z; /* 0x174: z scale */
} Plant42BodyWork;
#define PLANT42_BODY(e) ENEMY_WORK(e, Plant42BodyWork)
#define PLANT42_BODY_FIELD(e, f) ENEMY_FIELD(e, Plant42BodyWork, f)
#define PLANT42_BODY_MEMBER(e, f) ENEMY_MEMBER(e, Plant42BodyWork, f)

/* state 0: sets up a new plant42 (health, sca info, animation, work fields), then state 1 */
void plant42_init(void) {
    void plant42_behavior_suspend();
    void *clone;
    Entity *enemy;

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->has_enter_switch_zone = 1;
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->hit_state = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x404040;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0x7D0, (void *)0x12C);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = 0x28U;
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        g_CurrentEntity->health = (u16) (g_CurrentEntity->health + 0x64);
    }
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->animation_id = 2;
    (g_ApiCallPtr = api_joint_move)(0, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, (void *)0x40);
    g_CurrentEntity->sca_info = (s32) plant42_sca_info_ptr_vine;
    PLANT42(g_CurrentEntity)->rot_speed = 0;
    PLANT42(g_CurrentEntity)->unused_176 = 0;
    PLANT42(g_CurrentEntity)->scd_effect_timer = 0;
    PLANT42(g_CurrentEntity)->ambient_effect_timer = (s16) (((g_ApiCallPtr = api_rand)() & 0xFF) + 0x32);
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->sca_info = (s32) plant42_sca_info_ptr_main;
        FIELD_AT(((void *) g_CurrentEntity->sca_hit_data), HitPoint, [1].x, s16) = 0;
        HIT_POINT((((void *) g_CurrentEntity->sca_hit_data) + 6))->y = 0;
        HIT_POINT((((void *) g_CurrentEntity->sca_hit_data) + 6))->z = 0;
        (g_ApiCallPtr = api_entity_clone)(1, (void *)0x384, g_CurrentEntity->joint_count + 1, &g_CurrentEntity->aim_target);
        clone = ((void *) g_CurrentEntity->aim_target);
        ENT(clone)->behavior_flags = 0;
        ENT(clone)->hit_state = 0;
        ENT(clone)->scale = 0x1000;
        ENT(clone)->speed_vy = 0;
        ENT(clone)->speed_vz = 1;
        ENT(clone)->position_vx = 0;
        ENT(clone)->position_vy = 1;
        ENT(clone)->position_vz = 4;
        ENT(clone)->prev_angle = 0;
        ENT(clone)->rot_x = 0;
        ENT(clone)->angle = 0;
        ENT(clone)->angle_z = 0;
        ENT(clone)->action_ticks_counter = 0x59;
        ENT(clone)->speed_vx = 0;
        ENT(clone)->blend_counter = 7;
        ENT(clone)->floor_y = 0;
        PLANT42_BODY_FIELD(clone, scale_x) = 0x1000;
        PLANT42_BODY(clone)->scale_y = 0x1000;
        PLANT42_BODY(clone)->scale_z = 0x1000;
        if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 8) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(-1, (void *)-2, 0, NULL, 0x200, 8);
            ENT(clone)->position_vz = 0;
        }
        FIELD_AT(clone, Entity, ->state, s32) = 0;
        if (g_CurrentEntity->behavior_flags == 4) {
            ENT(clone)->health = -1;
            ENT(clone)->state = 2;
            ENT(clone)->action_state = 3;
            ENT(clone)->floor_y = 0xA20;
            PLANT42_BODY_FIELD(clone, scale_x) = 0x19DC;
            PLANT42_BODY(clone)->scale_y = 0x818;
            PLANT42_BODY(clone)->scale_z = 0x19DC;
            ENT(clone)->blend_counter = 1;
        }
        LIT(s32, sp_tmp1C) = 0x606060;
        LIT(s16, sp_svec_vz) = 0;
        LIT(s16, sp_svec_vy) = 0;
        LIT(s16, sp_svec_vx) = 0;
        (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &ENT(clone)->shadow, 0xBB8, (void *)0xBB8);
        enemy = LITP(void, g_EnemiesList);
        LIT(s32, sp_tmp00) = (s32) g_EnemyCount;
        while (LIT(s32, sp_tmp00) != 0) {
            enemy->aim_target = (s32)clone;
            enemy++;
            LIT(s32, sp_tmp00)--;
        }
        (g_ApiCallPtr = api_entity_clone)(1, (void *)0xDC, (s32) g_CurrentEntity->joint_count, &PLANT42(g_CurrentEntity)->roots);
        clone = PLANT42(g_CurrentEntity)->roots;
        ENT(clone)->rot_x = 0;
        ENT(clone)->angle = 0;
        ENT(clone)->angle_z = 0;
        ENT(clone)->action_ticks_counter = 0x59;
        ENT(clone)->behavior_flags = 0;
        ENT(clone)->hit_state = 0;
        ENT(clone)->speed_vy = 0;
        ENT(clone)->speed_vz = 1;
        ENT(clone)->position_vx = 0;
        ENT(clone)->position_vy = 1;
        ENT(clone)->scale = 0x1000;
        ENT(clone)->speed_vx = 0;
        if (g_CurrentEntity->behavior_flags == 4) {
            ENT(clone)->scale = 0x3E8;
            ENT(clone)->pos_y = -0x514;
            ENT(clone)->state = 2;
            ENT(clone)->action_state = 2;
        }
    }
    if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
        g_CurrentEntity->state = 8;
    }
    if (g_CurrentEntity->behavior_flags == 4) {
        g_CurrentEntity->state = 4;
        (g_ApiCallPtr = api_clut_fade_add_delta)(-4, (void *)-5, 0, NULL, 0x200, 8);
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 6) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 1, 0xB, 0);
        plant42_behavior_suspend();
    }
    if (g_CurrentEntity->behavior_flags == 5) {
        g_CurrentEntity->scale = 0x12C0;
    }
}

/* every frame for Plant 42 (enemies_update_functions_tbl): runs plant42_state_tbl[state] (script states
   take over while behavior_flags & 0x40), collision, and draws the body / roots copies */
void plant42_update() {
    void plant42_render_copies();
    extern s32 (*plant42_state_tbl[])();
    s32 joints;
    s32 unused;
    u8 state_m1;
    if (g_UpdateFlags & 4 & 0xFFFF) {
        state_m1 = g_CurrentEntity->state + 0xFF;
        if ((g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && (state_m1 < 2U)) {
            if (ENTITY_ACTION_U16(g_CurrentEntity) == 0x309) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0xF, 3);
            } else {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0, 0);
            }
        }
        plant42_state_tbl[g_CurrentEntity->state]();
        joints = g_CurrentEntity->joints;
        *((s16 *) g_CurrentEntity->sca_hit_data) = MEMBER_AT((JOINT_N(joints, 14)), Joint, ->world.t[0], u16) - ENTITY_AS(g_CurrentEntity, pos_x, u16);
        HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->y = (s16) (MEMBER_AT((JOINT_N(joints, 14)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16));
        HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->z = MEMBER_AT((JOINT_N(joints, 14)), Joint, ->world.t[2], u16) - ENTITY_AS(g_CurrentEntity, pos_z, u16);
    }
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        plant42_render_copies();
        joints = g_CurrentEntity->joints;
    }
    if (g_CurrentEntity->behavior_flags == 4) {
        g_CurrentEntity->has_enter_switch_zone = 0;
        return;
    }
    joints = g_CurrentEntity->joints;
    if (!(((u8) g_CurrentEntity->hit_state) & 0x80 & 0xFF)) {
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, (JOINT(JOINT_N(joints, 11))->world.t[1] / 7) + 0x7D0, (JOINT(JOINT_N(joints, 11))->world.t[1] / 70) + 0x12C);
    }
    (g_ApiCallPtr = api_billboard_draw)(JOINT_N_POS(joints, 11), &g_CurrentEntity->shadow, 0, (s32) ((s16) g_CurrentEntity->angle));
}

void plant42_state_check(void) {
    void plant42_state_check_body();
    plant42_state_check_body();
}

void plant42_state_check_body(void) {
    void plant42_run_action();
    void plant42_action_dispatch();
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    PLANT42(g_CurrentEntity)->player_distance = (g_ApiCallPtr = api_SquareRoot0)(((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) * (g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + ((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z) * (g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
    if ((u32) PLANT42(g_CurrentEntity)->player_distance < 0x2710U) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x80);
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        if (((u8) ENT(((void *) g_CurrentEntity->aim_target))->hit_state) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 1, 0, 0);
            if (!((g_ApiCallPtr = api_rand)() & 3)) {
                g_CurrentEntity->action_behavior = 1U;
            }
            if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
                if ((g_ApiCallPtr = api_rand)() & 3) {
                    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 1, 0);
                    g_CurrentEntity->action_behavior += !((g_ApiCallPtr = api_rand)() & 3);
                }
            }
            return;
        }
        plant42_action_dispatch();
    }
    plant42_run_action();
    if (--PLANT42(g_CurrentEntity)->ambient_effect_timer == 0) {
        PLANT42(g_CurrentEntity)->ambient_effect_timer = ((g_ApiCallPtr = api_rand)() & 0x3F) + 0x32;
    }
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        if ((PLANT42(g_CurrentEntity)->ambient_effect_timer == 1) && ((u32) PLANT42(g_CurrentEntity)->player_distance >= 0x1965U)) {
            LIT(s32, sp_vec_vy) = -0x2710;
            LIT(s32, sp_vec_vx) = (((g_ApiCallPtr = api_rand)() & 0x1FF) + g_PlayerEntity.pos_x) - 0x100;
            LIT(s32, sp_vec_vz) = (((g_ApiCallPtr = api_rand)() & 0x1FF) + g_PlayerEntity.pos_z) - 0x100;
            if (!((g_ApiCallPtr = api_rand)() & 1)) {
                (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 1, 0, 0, ADDR_sp_vec, 0);
            }
        }
        if ((PLANT42(g_CurrentEntity)->ambient_effect_timer < 0x1E) && ((u32) PLANT42(g_CurrentEntity)->player_distance >= 0x1965U) && !(((u16) PLANT42(g_CurrentEntity)->ambient_effect_timer) & 3 & 0xFFFF)) {
            LIT(s32, sp_vec_vy) = -0x2710;
            LIT(s32, sp_vec_vx) = g_PlayerEntity.pos_x - ({ ((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100; });
            LIT(s32, sp_vec_vz) = g_PlayerEntity.pos_z - ({ ((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100; });
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0x1C, 0, 0, ADDR_sp_vec, 0);
        }
    }
    if (PLANT42(g_CurrentEntity)->ambient_effect_timer % 50 == 0) {
        LIT(s32, sp_vec_vy) = -0x2710;
        LIT(s32, sp_vec_vx) = (plant42_effect_x[(g_ApiCallPtr = api_rand)() & 1] + (({ -0x400; }) + ((g_ApiCallPtr = api_rand)() & 0x7FF)));
        LIT(s32, sp_vec_vz) = (plant42_effect_z[(g_ApiCallPtr = api_rand)() & 1] + (({ -0x400; }) + ((g_ApiCallPtr = api_rand)() & 0x7FF)));
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0x1C, 0, 0, ADDR_sp_vec, 0);
    }
}

void plant42_damaged(void) {
    void plant42_damage_react();

    plant42_damage_react();
    if (--PLANT42(g_CurrentEntity)->ambient_effect_timer == 0) {
        PLANT42(g_CurrentEntity)->ambient_effect_timer = ((g_ApiCallPtr = api_rand)() & 0x3F) + 0x32;
    }
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        if ((PLANT42(g_CurrentEntity)->ambient_effect_timer == 1) && ((u32) PLANT42(g_CurrentEntity)->player_distance >= 0x1965U)) {
            LIT(s32, sp_vec_vy) = -0x2710;
            LIT(s32, sp_vec_vx) = (((g_ApiCallPtr = api_rand)() & 0x1FF) + g_PlayerEntity.pos_x) - 0x100;
            LIT(s32, sp_vec_vz) = (((g_ApiCallPtr = api_rand)() & 0x1FF) + g_PlayerEntity.pos_z) - 0x100;
            if (!((g_ApiCallPtr = api_rand)() & 1)) {
                (g_ApiCallPtr = api_effect_create_billboard)(0x1E, 1, 0, 0, ADDR_sp_vec, 0);
            }
        }
        if ((PLANT42(g_CurrentEntity)->ambient_effect_timer < 0x1E) && ((u32) PLANT42(g_CurrentEntity)->player_distance >= 0x1965U) && !(((u16) PLANT42(g_CurrentEntity)->ambient_effect_timer) & 3 & 0xFFFF)) {
            LIT(s32, sp_vec_vy) = -0x2710;
            LIT(s32, sp_vec_vx) = g_PlayerEntity.pos_x - ({ ((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100; });
            LIT(s32, sp_vec_vz) = g_PlayerEntity.pos_z - ({ ((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x100; });
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0x1C, 0, 0, ADDR_sp_vec, 0);
        }
    }
    if (PLANT42(g_CurrentEntity)->ambient_effect_timer % 50 == 0) {
        LIT(s32, sp_vec_vy) = -0x2710;
        LIT(s32, sp_vec_vx) = (plant42_effect_x[(g_ApiCallPtr = api_rand)() & 1] + (({ -0x400; }) + ((g_ApiCallPtr = api_rand)() & 0x7FF)));
        LIT(s32, sp_vec_vz) = (plant42_effect_z[(g_ApiCallPtr = api_rand)() & 1] + (({ -0x400; }) + ((g_ApiCallPtr = api_rand)() & 0x7FF)));
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0x1C, 0, 0, ADDR_sp_vec, 0);
    }
}

void plant42_damage_react(void) {
    void plant42_behavior_recoil();
    void plant42_behavior_scd_flinch();
    void *body;
    void *joints;

    body = ((void *) g_CurrentEntity->aim_target);
    if (ENT(((void *) g_CurrentEntity->aim_target))->health & 0x8000) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->health = -1;
        return;
    }
    if (((u8) g_CurrentEntity->sub_state == 0) && ((u8) g_CurrentEntity->hit_state & 7 & 0xFF)) {
        g_CurrentEntity->hit_state = (u8) g_CurrentEntity->hit_state & 0xF8;
        g_CurrentEntity->hit_state = ((u8) g_CurrentEntity->hit_state) | 1;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 1, 0, 0);
        if ((g_ApiCallPtr = api_rand)() & 1) {
            g_CurrentEntity->action_behavior = 1;
        }
        if (((u8) g_CurrentEntity->hit_state & 0x78 & 0xFF) == 8) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0xC8;
            joints = (void *)g_PlayerEntity.joints;
            (g_ApiCallPtr = api_effect_create_billboard)(0U, 0x18, 0, JOINT_N_WORLD(joints, 14), ADDR_sp_vec, 0);
        } else if (!(MEMBER_AT(body, Entity, ->hit_state, u8) & 7 & 0xFF)) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            joints = (void *)g_CurrentEntity->joints;
            LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_rand)() & 7;
            (g_ApiCallPtr = api_effect_create_billboard)(0U, 0x1B, 0, (LIT(s32, sp_tmp00) * 0x7C) + 0x364 + joints + 0x44, ADDR_sp_vec, 0);
            SCRATCH_VEC->x = (s32) (g_PlayerEntity.pos_x - g_CurrentEntity->pos_x);
            LIT(s32, sp_vec_vy) = (s32) g_CurrentEntity->pos_y;
            LIT(s32, sp_vec_vz) = g_PlayerEntity.pos_z - g_CurrentEntity->pos_z;
            (g_ApiCallPtr = api_VectorNormal)(ADDR_sp_vec, ADDR_sp_vec);
            SCRATCH_VEC->x = (s32) ((s32) SCRATCH_VEC->x >> 1);
            LIT(s32, sp_vec_vy) = (s32) ((s32) LIT(s32, sp_vec_vy) >> 1);
            LIT(s32, sp_vec_vz) = (s32) ((s32) LIT(s32, sp_vec_vz) >> 1);
            SCRATCH_VEC->x = (s32) (SCRATCH_VEC->x + g_CurrentEntity->pos_x);
            LIT(s32, sp_vec_vy) = (s32) (g_CurrentEntity->pos_y + 0x1194);
            LIT(s32, sp_vec_vz) = (s32) (LIT(s32, sp_vec_vz) + g_CurrentEntity->pos_z);
            (g_ApiCallPtr = api_effect_create_billboard)(0U, 0x18, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
        }
        if (((g_ApiCallPtr = api_rand)() & 1) && (g_CurrentEntity->behavior_flags & 1 & 0xFF) && (g_CurrentEntity->health < 0xA)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
            g_CurrentEntity->health = -1;
            MEMBER_AT(body, Entity, ->position_vz, s16)--;
            /* `body` = the bulb hanging from the ceiling, the part that takes the damage (seen in game); flag 0x29 =
               Plant 42 dead (it can only die after the V-JOLT, and flashes while dying - seen in game) */
            if ((s16) ENT(body)->position_vz == 1) {
                (g_ApiCallPtr = api_Flg_on)(ADDR_g_ScenarioFlags, SCN_FLAG_PLANT42_DEAD);
                if (g_PlayerEntity.id & 1 & 0xFF) {
                    g_UpdateFlags |= 0x100;
                }
                LIT(s32, sp_tmp00) = 6;
                while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                    if (!(ENT(((LIT(s32, sp_tmp00) * ENTITY_SIZE) + ADDR_g_EnemiesList))->health & 0x8000)) {
                        *(s32 *)(ADDR_g_EnemySlot0_State + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = STATE4(1, 1, 9, 0);
                        ENT(((LIT(s32, sp_tmp00) * ENTITY_SIZE) + ADDR_g_EnemiesList))->hit_state = 1;
                    }
                }
            }
            return;
        }
        if ((s16) ENT(body)->position_vz < 2) {
            g_CurrentEntity->hit_state = 0;
        }
        MEMBER_AT(body, Entity, ->hit_state, u8) = 1U;
        MEMBER_AT(body, Entity, ->behavior_flags, s8) = 1;
        return;
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        plant42_behavior_recoil();
        return;
    case 1:
        plant42_behavior_scd_flinch();
    }
}

void plant42_nop_unused(void) {

}

void plant42_die(void) {
    void plant42_behavior_death();
    if ((u8) g_CurrentEntity->sub_state == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        LIT(s32, sp_tmp04) = --MEMBER_AT(((void *) g_CurrentEntity->aim_target), Entity, ->position_vz, s16);
        if (LIT(s32, sp_tmp04) == 1) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_ScenarioFlags, SCN_FLAG_PLANT42_DEAD);
            if (g_PlayerEntity.id & 1 & 0xFF) {
                g_UpdateFlags |= 0x100;
            }
            LIT(s32, sp_tmp00) = 6;
            while (--LIT(s32, sp_tmp00) != -1) {
                if (!(ENT(((LIT(s32, sp_tmp00) * ENTITY_SIZE) + ADDR_g_EnemiesList))->health & 0x8000)) {
                    *(s32 *)(ADDR_g_EnemySlot0_State + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = STATE4(1, 1, 9, 0);
                    ENT(((LIT(s32, sp_tmp00) * ENTITY_SIZE) + ADDR_g_EnemiesList))->hit_state = 1;
                }
            }
        }
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(((void *) g_CurrentEntity->aim_target))->health = -1;
            FIELD_AT(((void *) g_CurrentEntity->aim_target), Entity, ->state, s32) = 2;
            FIELD_AT(PLANT42(g_CurrentEntity)->roots, Entity, ->state, s32) = 2;
        }
    }
    if (g_CurrentEntity->action_behavior != 0) {
        return;
    }
    plant42_behavior_death();
    return;
}

void plant42_attack(void) {

}

void plant42_run_action(void) {
    extern s32 (*plant42_action_tbl[])();
    if (ENT(((void *) g_CurrentEntity->aim_target))->health & 0x8000) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->health = -1;
        return;
    }
    plant42_action_tbl[g_CurrentEntity->action_behavior]();
}

void plant42_action_dispatch(void) {
    extern s32 (*plant42_action_select_tbl[])();
    LIT(s32, sp_tmp00) = ((s32) PLANT42(g_CurrentEntity)->player_distance);
    plant42_action_select_tbl[(g_CurrentEntity->behavior_flags & 7 & 0xFF)]();
}

void plant42_action_select(void) {
    u16 tilt;
    s32 pad14;
    u8 actions[8] = { 1, 2, 3, 1, 3, 1, 1, 2 };

    if (((u8) g_PlayerEntity.hit_state) != 0) {
        return;
    }
    if (LIT(s32, sp_tmp00) < 0x2AF8) {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 4));
    }
    if (LIT(s32, sp_tmp00) < 0x2710) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 1, 0);
            g_CurrentEntity->action_behavior = (u8) (g_CurrentEntity->action_behavior + ((g_ApiCallPtr = api_rand)() & 1));
        }
    }
    tilt = (((s16) g_CurrentEntity->angle_z) - 0xE80) & 0xFFF;
    if (LIT(s32, sp_tmp00) < 0x2328) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->action_behavior = (u8) actions[((g_ApiCallPtr = api_rand)() & 7)];
        }
    }
}

void plant42_action_nop_unused(void) {

}

void plant42_action_select_nop(void) {

}

void plant42_action_select_flags5(void) {
    s32 pad10;
    s32 pad14;
    u8 actions[8] = { 1, 2, 1, 1, 2, 1, 1, 2 };

    if (((u8) g_PlayerEntity.hit_state) != 0) {
        return;
    }
    if (LIT(s32, sp_tmp00) < 0x2AF8) {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 4));
    }
    if (LIT(s32, sp_tmp00) < 0x2710) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 1, 0);
            g_CurrentEntity->action_behavior = (u8) (g_CurrentEntity->action_behavior + ((g_ApiCallPtr = api_rand)() & 1));
        }
    }
    if (LIT(s32, sp_tmp00) < 0x2328) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->action_behavior = (u8) actions[(g_ApiCallPtr = api_rand)() & 7];
        }
    }
}

void plant42_behavior_idle(void) {
    u8 plant42_joint_move_a1() __asm__("plant42_joint_move");
    u16 tilt;
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 0xF) + 0xF;
        g_CurrentEntity->blend_counter = 0x3F;
        PLANT42(g_CurrentEntity)->yaw_step = (g_ApiCallPtr = api_rand)() & 0xF;
        PLANT42(g_CurrentEntity)->tilt_step = (g_ApiCallPtr = api_rand)() & 7;
        PLANT42(g_CurrentEntity)->yaw_step -= PLANT42(g_CurrentEntity)->yaw_step * ((g_ApiCallPtr = api_rand)() & 2);
        PLANT42(g_CurrentEntity)->tilt_step -= PLANT42(g_CurrentEntity)->tilt_step * ((g_ApiCallPtr = api_rand)() & 2);
        if (ENTITY_AS(g_CurrentEntity, animation_id, u8) == 8) {
            ENTITY_AS(g_CurrentEntity, animation_id, u8) = 9U;
        }
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle - (s8) (PLANT42(g_CurrentEntity)->yaw_step / 2)) & 0xFFF);
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - (s8) (PLANT42(g_CurrentEntity)->tilt_step / 2));
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + plant42_joint_move_a1(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40));
        break;
    case 2:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + plant42_joint_move_a1(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40));
        break;
    case 3:
        g_CurrentEntity->action_state = 0U;
        break;
    }
    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle + PLANT42(g_CurrentEntity)->yaw_step) & 0xFFF);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 8U);
    }
    LIT(s32, sp_tmp00) = (s32) (s16) g_CurrentEntity->angle_z;
    g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + PLANT42(g_CurrentEntity)->tilt_step);
    joints = g_CurrentEntity->joints;
    tilt = (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x3E) & 0xFFF;
    if (tilt < 0xE32U) {
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - PLANT42(g_CurrentEntity)->tilt_step);
    }
    if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x257) {
        g_CurrentEntity->angle_z = LIT(s32, sp_tmp00) - 0x10;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = plant42_idle_anim_tbl[API_AS(u32, api_rand)() % 10];
        g_CurrentEntity->blend_counter = 0x3F;
    }
    if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = plant42_idle_anim_tbl[API_AS(u32, api_rand)() % 10];
        g_CurrentEntity->blend_counter = 0x3F;
    }
}

void plant42_behavior_sweep(void) {
    void plant42_sweep_hit();
    s32 plant42_joint_move_a2() __asm__("plant42_joint_move");
    struct { u8 a; u8 b; s8 c; } sweep[2] = { { 0, 1, 1 }, { 1, 0, -1 } };
    void *joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xC;
        g_CurrentEntity->action_ticks_counter = 0x32U;
        PLANT42(g_CurrentEntity)->sweep_dir = 1;
        if ((u32) (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 1) >= 0U) {
        } else {
            PLANT42(g_CurrentEntity)->sweep_dir = 0;
        }
        /* fallthrough */
    case 1:
        plant42_joint_move_a2(sweep[PLANT42(g_CurrentEntity)->sweep_dir].b, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->angle += (sweep[PLANT42(g_CurrentEntity)->sweep_dir].c * 0x10);
        joints = (void *)g_CurrentEntity->joints;
        if (JOINT(JOINT_N(joints, 14))->world.t[1] < -0x9C4) {
            g_CurrentEntity->angle_z += 8;
        }
        if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x3E7) {
            g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 8);
        }
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        g_CurrentEntity->action_ticks_counter = 0xFU;
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        /* fallthrough */
    case 3:
        plant42_joint_move_a2(sweep[PLANT42(g_CurrentEntity)->sweep_dir].a, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        return;
    case 4:
        g_CurrentEntity->action_state = 5U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xC;
        /* fallthrough */
    case 5:
        PLANT42(g_CurrentEntity)->rot_speed = (s16) (((u16) PLANT42(g_CurrentEntity)->rot_speed) + 4);
        if (PLANT42(g_CurrentEntity)->rot_speed >= 0x29) {
            PLANT42(g_CurrentEntity)->rot_speed = (s16) (((u16) PLANT42(g_CurrentEntity)->rot_speed) + 0x1C);
        }
        plant42_joint_move_a2(sweep[PLANT42(g_CurrentEntity)->sweep_dir].b, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        ENTITY_AS(g_CurrentEntity, angle, s16) -= PLANT42(g_CurrentEntity)->rot_speed * sweep[PLANT42(g_CurrentEntity)->sweep_dir].c;
        if (PLANT42(g_CurrentEntity)->rot_speed >= 0xC1) {
            g_CurrentEntity->action_state = 6U;
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 3, 0, g_CurrentEntity->joints + 0x720);
        }
        plant42_sweep_hit();
        return;
    case 6:
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 8);
        plant42_joint_move_a2(sweep[PLANT42(g_CurrentEntity)->sweep_dir].a, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        plant42_sweep_hit();
        if (PLANT42(g_CurrentEntity)->rot_speed > 0) {
            ENTITY_AS(g_CurrentEntity, angle, s16) -= PLANT42(g_CurrentEntity)->rot_speed * sweep[PLANT42(g_CurrentEntity)->sweep_dir].c;
            PLANT42(g_CurrentEntity)->rot_speed = (s16) (PLANT42(g_CurrentEntity)->rot_speed - 0x10);
            g_CurrentEntity->hit_state = 0;
            if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF) && (((s16) ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->position_vz) >= 2)) {
                g_CurrentEntity->hit_state = 1;
            }
            return;
        }
        g_CurrentEntity->action_state = 7U;
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle - (sweep[PLANT42(g_CurrentEntity)->sweep_dir].c * 8));
        return;
    case 7:
        g_CurrentEntity->angle += (sweep[PLANT42(g_CurrentEntity)->sweep_dir].c * 8);
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        PLANT42(g_CurrentEntity)->rot_speed = 0;
        /* fallthrough */
    default:
    }
}

void plant42_sweep_hit(void) {
    u16 dist;

    dist = PLANT42_AS(g_CurrentEntity, player_distance, u16) + 0xF830;
    if (g_CurrentEntity->behavior_flags == 5) {
        dist = PLANT42_AS(g_CurrentEntity, player_distance, u16) + 0xF254;
    }
    if (dist < 0x157CU) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, PLANT42(g_CurrentEntity)->rot_speed) == 0) && (((u8) g_PlayerEntity.hit_state) == 0) && (PLANT42(g_CurrentEntity)->rot_speed >= 0x51)) {
            g_CurrentEntity->angle_z = (u16) (((s16) g_CurrentEntity->angle_z) - 0xC8);
            g_CurrentEntity->action_state = 6;
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
            g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
            g_PlayerEntity.hit_state = 1;
            if (LIT(s32, sp_tmp1C) != 0) {
                if (PLANT42(g_CurrentEntity)->sweep_dir == 0) {
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 0, 0);
                    g_PlayerEntity.angle = g_CurrentEntity->angle + 0x400;
                    (g_ApiCallPtr = api_sound_play_se_3d)(2, 4, 0, g_CurrentEntity->joints + 0x720);
                } else {
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 2, 5);
                    g_Plant42CaptureMatrix_tx = (s16) g_CurrentEntity->angle + 0x400;
                    (g_ApiCallPtr = api_sound_play_se_3d)(2, 2, 0, g_CurrentEntity->joints + 0x720);
                }
            } else if (PLANT42(g_CurrentEntity)->sweep_dir != 0) {
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 0, 0);
                g_PlayerEntity.angle = ((s16) g_CurrentEntity->angle) - 0x400;
                (g_ApiCallPtr = api_sound_play_se_3d)(2, 4, 0, g_CurrentEntity->joints + 0x720);
            } else {
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 2, 5);
                g_Plant42CaptureMatrix_tx = (s16) g_CurrentEntity->angle - 0x400;
                (g_ApiCallPtr = api_sound_play_se_3d)(2, 2, 0, g_CurrentEntity->joints + 0x720);
            }
            g_PlayerEntity.health -= 0x19;
            if ((s16) g_PlayerEntity.health < 0) {
                g_PlayerEntity.health = 1;
            }
        }
    }
}

void plant42_behavior_spit(void) {
    s32 plant42_joint_move_a3() __asm__("plant42_joint_move");
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 7;
        g_CurrentEntity->hit_state = 1;
        PLANT42(g_CurrentEntity)->rot_speed = 4U;
        /* fallthrough */
    case 1:
        if (plant42_joint_move_a3(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40) & 0xFF) {
            g_CurrentEntity->action_state = 2;
            g_CurrentEntity->angle_z = g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed);
            PLANT42(g_CurrentEntity)->rot_speed = (u16) (PLANT42(g_CurrentEntity)->rot_speed - 0x30);
            (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)2, 1, 0, g_CurrentEntity->joints + 0x720);
            return;
        }
        LIT(s32, sp_tmp00) = (((u8) ENT(((void *) g_CurrentEntity->aim_target))->hit_state) * 0x20) + 0x10;
        (g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), LIT(s32, sp_tmp00));
        if ((u8) g_CurrentEntity->animation_frame_id < 8U) {
            g_CurrentEntity->angle_z += 0x40;
            return;
        }
        if ((u8) g_CurrentEntity->animation_frame_id >= 0xDU) {
            g_CurrentEntity->angle_z = g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed);
            PLANT42(g_CurrentEntity)->rot_speed += 0x18;
        }
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        if (((u8) g_CurrentEntity->animation_frame_id >= 0xBU) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 15), ADDR_sp_vec, 0x4B0, ADDR_g_PlayerPosX) != 0) && (JOINT(JOINT_N(joints, 15))->world.t[1] >= -0x9C3)) {
                g_PlayerEntity.hit_state = 1;
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(2, 0, 0x64, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)5, 0xFA, 4);
                PLANT42(g_CurrentEntity)->rot_speed = (u16) (PLANT42(g_CurrentEntity)->rot_speed - 8);
                g_PlayerEntity.health -= 0xF;
                if ((s16) g_PlayerEntity.health < 0) {
                    g_PlayerEntity.health = 1;
                }
                (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)2, 2, 0, g_CurrentEntity->joints + 0x720);
            }
        }
        return;
    case 2:
        g_CurrentEntity->angle_z = g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed);
        PLANT42(g_CurrentEntity)->rot_speed = (u16) (PLANT42(g_CurrentEntity)->rot_speed - 8);
        if ((s16) ((u16) PLANT42(g_CurrentEntity)->rot_speed) < 0) {
            g_CurrentEntity->action_state = 3;
        }
        return;
    case 3:
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->action_state = 4;
        PLANT42(g_CurrentEntity)->rot_speed = (u16) (0x10 - (((g_ApiCallPtr = api_rand)() & 1) * 0x20));
        g_CurrentEntity->blend_counter = 0x3F;
        /* fallthrough */
    case 4:
        g_CurrentEntity->angle_z += 0x10;
        if ((s16) g_CurrentEntity->angle_z >= -0x18F) {
            g_CurrentEntity->action_state = 5;
        }
        g_CurrentEntity->angle += ((u16) PLANT42(g_CurrentEntity)->rot_speed);
        plant42_joint_move_a3(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        return;
    case 5:
        g_CurrentEntity->angle += ((s16) ((u16) PLANT42(g_CurrentEntity)->rot_speed) / 2);
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->hit_state = 0;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF) && (((s16) ENT(((void *) g_CurrentEntity->aim_target))->position_vz) >= 2)) {
            g_CurrentEntity->hit_state = 1;
        }
    }
}

void plant42_behavior_move(void) {
    u8 plant42_joint_move_u8() __asm__("plant42_joint_move");
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        PLANT42(g_CurrentEntity)->rot_speed = 8;
        /* fallthrough */
    case 1:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 5U;
        }
        ENTITY_AS(g_CurrentEntity, angle, s16) -= 0x28;
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x28);
        plant42_joint_move_u8(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        joints = g_CurrentEntity->joints;
        if (JOINT(JOINT_N(joints, 14))->world.t[1] < -0x9C4) {
            g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + 0x10);
            break;
        }
        if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x3E7) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 0x10;
            break;
        }
        if ((s16) g_CurrentEntity->action_ticks_counter < 0x1E) {
            g_CurrentEntity->action_state = 2U;
            g_CurrentEntity->action_ticks_counter = 0x3CU;
        }
        break;
    case 2:
                if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {

        } else {
            g_CurrentEntity->action_state = 5U;
        }
        ENTITY_AS(g_CurrentEntity, angle, s16) -= 0x30;
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x30);
        joints = g_CurrentEntity->joints;
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - JOINT(JOINT_N(joints, 14))->world.t[0]) + __builtin_abs(g_PlayerEntity.pos_z - JOINT(JOINT_N(joints, 14))->world.t[2]);
        LIT(s32, sp_tmp04) = __builtin_abs(g_PlayerEntity.pos_x - JOINT(JOINT_N(joints, 12))->world.t[0]) + __builtin_abs(g_PlayerEntity.pos_z - JOINT(JOINT_N(joints, 12))->world.t[2]);
        if (((LIT(s32, sp_tmp00) < 0x320) || (LIT(s32, sp_tmp04) < 0x1F4)) && !(g_PlayerEntity.collision_flags & 8 & 0xFF) && (((u8) g_PlayerEntity.hit_state) == 0) && (g_PlayerEntity.pos_y == 0) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20) == -0x20)) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->animation_id = 0xE;
            g_CurrentEntity->hit_state = 1;
            PLANT42(g_CurrentEntity)->grab_joint = 0xF;
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 1, 0);
            g_PlayerEntity.hit_state = 1;
            g_PlayerEntity.speed_vx = (JOINT(JOINT_N(joints, 11))->world.t[0] - g_PlayerEntity.pos_x) / 6;
            g_PlayerEntity.speed_vy = (JOINT(JOINT_N(joints, 11))->world.t[1] + 0x7EF - g_PlayerEntity.pos_y) / 6;
            g_PlayerEntity.speed_vz = (JOINT(JOINT_N(joints, 11))->world.t[2] - g_PlayerEntity.pos_z) / 6;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 3;
            goto action_state3;
        }
        plant42_joint_move_u8(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->angle_z = g_CurrentEntity->angle_z + ((u16) PLANT42(g_CurrentEntity)->rot_speed);
        if ((JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x5DB) && !(PLANT42(g_CurrentEntity)->rot_speed & 0x80000000)) {
            PLANT42(g_CurrentEntity)->rot_speed *= -1;
        }
        if ((JOINT(JOINT_N(joints, 14))->world.t[1] < -0x9C4) && (PLANT42(g_CurrentEntity)->rot_speed & 0x80000000)) {
            PLANT42(g_CurrentEntity)->rot_speed *= -1;
        }
        break;
    case 3:
action_state3:
        joints = g_CurrentEntity->joints;
        if ((u8) g_CurrentEntity->animation_frame_id >= 4U) {
            LIT(s32, sp_vec_vx) = -0x60F;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vz) = 0x2BC;
            (g_ApiCallPtr = api_ApplyMatrixLV)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, ADDR_sp_vec, ADDR_sp_vec);
            g_PlayerEntity.speed_vx = ((LIT(s32, sp_vec_vx) + JOINT(((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints))->world.t[0]) - g_PlayerEntity.pos_x) / 2;
            g_PlayerEntity.speed_vy = ((LIT(s32, sp_vec_vy) + JOINT(((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints))->world.t[1]) - g_PlayerEntity.pos_y) / 2;
            g_PlayerEntity.speed_vz = ((LIT(s32, sp_vec_vz) + JOINT(((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints))->world.t[2]) - g_PlayerEntity.pos_z) / 2;
        }
        if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x7CF) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 0x10;
        }
        g_PlayerEntity.pos_x += g_PlayerEntity.speed_vx;
        g_PlayerEntity.pos_y += g_PlayerEntity.speed_vy;
        g_PlayerEntity.pos_z += g_PlayerEntity.speed_vz;
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        break;
    case 4:
        (g_ApiCallPtr = api_sound_play_se_3d)(2, 5, 0, g_CurrentEntity->joints + 0x720);
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
        break;
    case 5:
        g_CurrentEntity->action_ticks_counter = 0x14U;
        g_CurrentEntity->action_state = 6U;
        PLANT42(g_CurrentEntity)->rot_speed = 0;
        joints = g_CurrentEntity->joints;
        if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x7CF) {
            PLANT42(g_CurrentEntity)->rot_speed = 0x10;
        }
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        /* fallthrough */
    case 6:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 7U;
        }
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 8);
        plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        break;
    case 7:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->animation_id = 9;
        g_CurrentEntity->hit_state = 0;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF) && (((s16) ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->position_vz) >= 2)) {
            g_CurrentEntity->hit_state = 1;
        }
    }
    (g_ApiCallPtr = api_billboard_set_size)(ADDR_g_PlayerShadowOffsetX, (g_PlayerEntity.pos_y / 17) + 0x1F4, (g_PlayerEntity.pos_y / 17) + 0x2BC);
}

void plant42_behavior_hold(void) {
    u8 plant42_joint_move_a4() __asm__("plant42_joint_move");
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_ticks_counter = 0xAU;
        g_CurrentEntity->action_state = 1U;
        PLANT42(g_CurrentEntity)->rot_speed = 0x18;
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.status_flags |= 2;
        (g_ApiCallPtr = api_matrix_relative)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, LITP(s32, g_PlayerCoordM00), &g_Plant42CaptureMatrix);
        g_Plant42CaptureMatrix_tx = -0x60F;
        g_Plant42CaptureMatrix_ty = 0;
        g_Plant42CaptureMatrix_tz = 0x2BC;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, (s32 *)6, NULL, g_CurrentEntity->joints + 0x720);
        (g_ApiCallPtr = api_sound_play_se_3d)(2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, NULL, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 1:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 2U;
        }
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed));
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
        break;
    case 2:
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xF;
        if (g_PlayerEntity.health < 0x28) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 8, 0);
            break;
        }
    case 3:
            g_CurrentEntity->action_state += plant42_joint_move_a4(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
            g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 4);
            joints = g_CurrentEntity->joints;
            g_PlayerEntity.has_enter_switch_zone |= 0x80;
            (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
        break;
    case 4:
        g_CurrentEntity->action_ticks_counter = 0xFU;
        g_CurrentEntity->action_state = 5U;
        /* fallthrough */
    case 5:
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 6U;
        }
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 8);
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
        break;
    case 6:
        g_CurrentEntity->action_state = 7U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 0;
        PLANT42(g_CurrentEntity)->rot_speed = 8;
        /* fallthrough */
    case 7:
        g_CurrentEntity->angle_z += ((u16) PLANT42(g_CurrentEntity)->rot_speed);
        PLANT42(g_CurrentEntity)->rot_speed = (s16) (((u16) PLANT42(g_CurrentEntity)->rot_speed) + 1);
        if (g_PlayerEntity.pos_y >= -0x5DB) {
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1A, 0, ADDR_g_PlayerPosX);
            goto block;
        }
        plant42_joint_move_a4(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
        break;
    case 8:
    block:
        g_CurrentEntity->action_state = 9U;
        g_PlayerEntity.pos_y = 0;
        g_PlayerEntity.has_enter_switch_zone &= 0x7F;
        g_PlayerEntity.status_flags &= 0xFD;
        g_CurrentEntity->animation_id = 3;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 2, 0);
        g_PlayerEntity.health = g_PlayerEntity.health - 0x28;
    case 9:
        g_CurrentEntity->action_state += plant42_joint_move_a4(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 2;
        break;
    case 10:
        g_CurrentEntity->hit_state = 0;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF) && (((s16) ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->position_vz) >= 2)) {
            g_CurrentEntity->hit_state = 1;
        }
    }
    (g_ApiCallPtr = api_billboard_set_size)(ADDR_g_PlayerShadowOffsetX, (g_PlayerEntity.pos_y / 17) + 0x1F4, (g_PlayerEntity.pos_y / 17) + 0x2BC);
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void plant42_behavior_grab_alt(void) {
    u8 plant42_joint_move_a5() __asm__("plant42_joint_move");
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->action_ticks_counter = 0x1EU;
        /* fallthrough */
    case 1:
        g_CurrentEntity->angle = (u16) (ENTITY_AS(g_CurrentEntity, angle, s16) - 0x30);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (s32 *)0x10));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x30);
        plant42_joint_move_a5(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        joints = g_CurrentEntity->joints;
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 2U;
            g_CurrentEntity->animation_id = 0xE;
            g_PlayerEntity.speed_vx = (JOINT(JOINT_N(joints, 11))->world.t[0] - g_PlayerEntity.pos_x) / 6;
            g_PlayerEntity.speed_vy = (JOINT(JOINT_N(joints, 11))->world.t[1] + 0x7EF - g_PlayerEntity.pos_y) / 6;
            g_PlayerEntity.speed_vz = (JOINT(JOINT_N(joints, 11))->world.t[2] - g_PlayerEntity.pos_z) / 6;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        if (JOINT(JOINT_N(joints, 14))->world.t[1] < -0x9C4) {
            g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + 0x10);
        }
        if (JOINT(JOINT_N(joints, 14))->world.t[1] >= -0x3E7) {
            g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x10);
        }
        break;
    case 2:
        joints = g_CurrentEntity->joints;
        if ((u8) g_CurrentEntity->animation_frame_id >= 4U) {
            LIT(s32, sp_vec_vx) = -0x60F;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vz) = 0x2BC;
            (g_ApiCallPtr = api_ApplyMatrixLV)(JOINT_N_WORLD(joints, 15), LITP(s32, sp_vec), LITP(s32, sp_vec));
            g_PlayerEntity.speed_vx = (LIT(s32, sp_vec_vx) + JOINT(JOINT_N(joints, 15))->world.t[0] - g_PlayerEntity.pos_x) / 2;
            g_PlayerEntity.speed_vy = (LIT(s32, sp_vec_vy) + JOINT(JOINT_N(joints, 15))->world.t[1] - g_PlayerEntity.pos_y) / 2;
            g_PlayerEntity.speed_vz = (LIT(s32, sp_vec_vz) + JOINT(JOINT_N(joints, 15))->world.t[2] - g_PlayerEntity.pos_z) / 2;
        }
        g_PlayerEntity.pos_x += g_PlayerEntity.speed_vx;
        g_PlayerEntity.pos_y += g_PlayerEntity.speed_vy;
        g_PlayerEntity.pos_z += g_PlayerEntity.speed_vz;
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + plant42_joint_move_a5(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        break;
    case 3:
        g_CurrentEntity->action_ticks_counter = 0xAU;
        g_CurrentEntity->action_state = 4U;
        PLANT42(g_CurrentEntity)->rot_speed = 0x18;
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.status_flags |= 2;
        (g_ApiCallPtr = api_matrix_relative)(JOINT_N_WORLD(joints, 15), LITP(s32, g_PlayerCoordM00), &g_Plant42CaptureMatrix);
        g_Plant42CaptureMatrix_tx = -0x60F;
        g_Plant42CaptureMatrix_ty = 0;
        g_Plant42CaptureMatrix_tz = 0x2BC;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, (s32 *)5, NULL, g_CurrentEntity->joints + 0x720);
        (g_ApiCallPtr = api_pad_vibrate_large)(8, (s32 *)0x8C, NULL);
        (g_ApiCallPtr = api_pad_vibrate_large)(8, (s32 *)0x78, (s32 *)0x14);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xC, (s32 *)0x64, (s32 *)0x23);
        /* fallthrough */
    case 4:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 5U;
            (g_ApiCallPtr = api_sound_play_se_3d)(2, (s32 *)6, NULL, g_CurrentEntity->joints + 0x720);
            (g_ApiCallPtr = api_sound_play_se_3d)(2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, NULL, ADDR_g_PlayerPosX);
        }
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - ((u16) PLANT42(g_CurrentEntity)->rot_speed));
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
        break;
    case 5:
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, LITP(s32, g_PlayerCoordM00));
    }
    (g_ApiCallPtr = api_billboard_set_size)(ADDR_g_PlayerShadowOffsetX, (g_PlayerEntity.pos_y / 17) + 0x1F4, (g_PlayerEntity.pos_y / 17) + 0x2BC);
}

void plant42_behavior_suspend(void) {
    s32 plant42_joint_move_a6(s32, s32, s32, s32) __asm__("plant42_joint_move");
    s32 joints;
    u16 swing;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        joints = g_CurrentEntity->joints;
        g_CurrentEntity->animation_id = 0xE;
        g_CurrentEntity->animation_frame_id = 4;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        plant42_joint_move_a6(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if (!(g_PlayerEntity.id & 1 & 0xFF) && ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 6)) {
            *(MATRIX *)(&g_Plant42CaptureMatrix) = *(MATRIX *)(&plant42_chris_hold_matrix);
        }
        PLANT42_FIELD(g_CurrentEntity, yaw_step) = 1;
        PLANT42(g_CurrentEntity)->tilt_step = 1;
        PLANT42(g_CurrentEntity)->yaw_swing = 3;
        PLANT42(g_CurrentEntity)->tilt_swing = 6;
        PLANT42(g_CurrentEntity)->rot_speed = 1;
        g_CurrentEntity->action_ticks_counter = 0;
        /* fallthrough */
    case 1:
        if ((s16) (((s16) g_CurrentEntity->action_ticks_counter) % 20) == 0) {
            (g_ApiCallPtr = api_pad_vibrate_large)(8, (s32 *)0x50, 0);
        }
        g_CurrentEntity->action_ticks_counter++;
                if ((u32) PLANT42(g_CurrentEntity)->rot_speed-- == 0) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle - PLANT42(g_CurrentEntity)->yaw_swing);
            g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z - PLANT42(g_CurrentEntity)->tilt_swing);
            PLANT42(g_CurrentEntity)->yaw_swing = (s16) (PLANT42(g_CurrentEntity)->yaw_swing - PLANT42_FIELD(g_CurrentEntity, yaw_step));
            PLANT42(g_CurrentEntity)->tilt_swing = (s16) (PLANT42(g_CurrentEntity)->tilt_swing - PLANT42(g_CurrentEntity)->tilt_step);
            swing = PLANT42(g_CurrentEntity)->yaw_swing + 4;
            if (swing >= 9U) {
                PLANT42_FIELD(g_CurrentEntity, yaw_step) *= -1;
            }
            swing = PLANT42(g_CurrentEntity)->tilt_swing + 7;
            if (swing >= 0xFU) {
                PLANT42(g_CurrentEntity)->tilt_step *= -1;
            }
            PLANT42(g_CurrentEntity)->rot_speed = 1;
        }
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
    }
    (g_ApiCallPtr = api_billboard_set_size)(ADDR_g_PlayerShadowOffsetX, (g_PlayerEntity.pos_y / 17) + 0x1F4, (g_PlayerEntity.pos_y / 17) + 0x2BC);
}

void plant42_behavior_release(void) {
    u8 plant42_joint_move_a7() __asm__("plant42_joint_move");
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xF;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 1, 0);
        (g_ApiCallPtr = api_sound_play_se_3d)(2, (s32 *)6, 0, g_CurrentEntity->joints + 0x720);
        (g_ApiCallPtr = api_sound_play_se_3d)(2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, 0, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state += plant42_joint_move_a7(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 4);
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        break;
    case 2:
        g_CurrentEntity->action_ticks_counter = 0xFU;
        g_CurrentEntity->action_state = 3U;
        /* fallthrough */
    case 3:
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 8);
        joints = g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        break;
    case 4:
        g_CurrentEntity->action_state = 5U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0;
        PLANT42(g_CurrentEntity)->rot_speed = 8;
        /* fallthrough */
    case 5:
        g_CurrentEntity->angle_z += ((u16) PLANT42(g_CurrentEntity)->rot_speed);
        PLANT42(g_CurrentEntity)->rot_speed = (s16) (((u16) PLANT42(g_CurrentEntity)->rot_speed) + 1);
        if (g_PlayerEntity.pos_y >= -0x5DB) {
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1A, 0, ADDR_g_PlayerPosX);
            goto block;
        }
            plant42_joint_move_a7(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
            joints = g_CurrentEntity->joints;
            g_PlayerEntity.has_enter_switch_zone |= 0x80;
            (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 15), &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        break;
    case 6:
    block:
        g_CurrentEntity->action_state = 7;
        g_PlayerEntity.pos_y = 0;
        g_PlayerEntity.has_enter_switch_zone &= 0x7F;
        g_PlayerEntity.status_flags &= 0xFD;
        g_CurrentEntity->animation_id = 3;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_PlayerEntity.hit_state = 1;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 2, 0);
        g_PlayerEntity.pos_x = 0x1757;
        g_PlayerEntity.pos_z = 0x3BBD;
        g_PlayerEntity.angle = 0x499;
    case 7:
        g_CurrentEntity->action_state += plant42_joint_move_a7(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 2);
        break;
    case 8:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0, 0);
    }
    (g_ApiCallPtr = api_billboard_set_size)(ADDR_g_PlayerShadowOffsetX, (g_PlayerEntity.pos_y / 17) + 0x1F4, (g_PlayerEntity.pos_y / 17) + 0x2BC);
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void plant42_behavior_kill(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->action_ticks_counter = 0x1E;
        (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)2, (s32 *)6, 0, g_CurrentEntity->joints + 0x720);
        /* fallthrough */
    case 1:
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        FIELD_AT(joints, Joint, [12].flags, u8) = (u8) (FIELD_AT(joints, Joint, [12].flags, u8) & 0xFD);
        JOINT(JOINT_N(joints, 12))->rotation.vx = (s16) (((u16) JOINT(JOINT_N(joints, 12))->rotation.vx) + 2);
        (g_ApiCallPtr = api_RotMatrix)(JOINT_N_ROT(joints, 12), JOINT_N_TRANSFORM(joints, 12));
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 11), JOINT_N_TRANSFORM(joints, 12), JOINT_N_WORLD(joints, 12));
        ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 4;
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 2;
            g_CurrentEntity->action_ticks_counter = 0x5A;
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 8, 1, 4);
            PLANT42(g_CurrentEntity)->rot_speed = 0x18;
        }
        return;
    case 2:
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_rand)() & 3;
        g_CurrentEntity->angle = g_CurrentEntity->angle + ((s16) PLANT42(g_CurrentEntity)->rot_speed / (s32) (LIT(s32, sp_tmp00) + 1));
        ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 1;
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 11), JOINT_N_TRANSFORM(joints, 12), JOINT_N_WORLD(joints, 12));
        PLANT42(g_CurrentEntity)->rot_speed *= -1;
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 3;
            g_CurrentEntity->action_ticks_counter = 0x50;
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), (s32 *)0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), (s32 *)0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), (s32 *)0x30, 0x80820, 0x606060);
            joints = g_PlayerEntity.joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            FIELD_AT(joints, Joint, [2].flags, s8) = (s8) (FIELD_AT(joints, Joint, [2].flags, u8) | 8);
            JOINT(joints)->flags = (s8) (JOINT(joints)->flags | 0x10);
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 8, 0, 0);
        }
        return;
    case 3:
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_GsMulCoord0)((PLANT42(g_CurrentEntity)->grab_joint * 0x7C) + joints + 0x44, &g_Plant42CaptureMatrix, ADDR_g_PlayerCoordM00);
        JOINT(JOINT_N(joints, 12))->rotation.vx = (s16) (((u16) JOINT(JOINT_N(joints, 12))->rotation.vx) + 0x10);
        (g_ApiCallPtr = api_RotMatrix)(JOINT_N_ROT(joints, 12), JOINT_N_TRANSFORM(joints, 12));
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 11), JOINT_N_TRANSFORM(joints, 12), JOINT_N_WORLD(joints, 12));
        g_PlayerEntity.health = -1;
        return;
    case 4:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        joints = g_CurrentEntity->joints;
        FIELD_AT(joints, Joint, [12].flags, u8) = (u8) (FIELD_AT(joints, Joint, [12].flags, u8) | 2);
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 8, 0, 2);
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void plant42_behavior_wither(void) {
    u8 plant42_joint_move_a8() __asm__("plant42_joint_move");
    s32 unused;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xD;
        g_CurrentEntity->scale = 0x1000U;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->prev_angle = 7;
            FIELD_AT(ENTITY_AS(g_CurrentEntity, aim_target, void *), Entity, ->state, s32) = 1;
            FIELD_AT(PLANT42(g_CurrentEntity)->roots, Entity, ->state, s32) = 1;
            (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(0x1E, 0x6E, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x50, 0x6E, 0, 0x1E);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->scale = (u16) (g_CurrentEntity->scale + 0xFFE0);
        g_CurrentEntity->action_state += (plant42_joint_move_a8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40));
        return;
    case 2:
        g_CurrentEntity->action_ticks_counter = 0xD2U;
        g_CurrentEntity->action_state = 3U;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->prev_angle = 4;
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->action_state = 2;
            ENT(PLANT42(g_CurrentEntity)->roots)->action_state = 2;
        }
        /* fallthrough */
    case 3:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        return;
    case 4:
        g_CurrentEntity->action_state = 5U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 2;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->prev_angle = 0xE;
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->action_state = 3;
            ENT(PLANT42(g_CurrentEntity)->roots)->action_state = 3;
            (g_ApiCallPtr = api_pad_vibrate_large)(0x3C, 0x46, 0);
        }
        /* fallthrough */
    case 5:
        g_CurrentEntity->scale = (u16) (g_CurrentEntity->scale + 0x20);
        g_CurrentEntity->action_state += (plant42_joint_move_a8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40));
        return;
    case 6:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->scale = 0U;
        g_CurrentEntity->hit_state = 0;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->prev_angle = 0;
            FIELD_AT(ENTITY_AS(g_CurrentEntity, aim_target, void *), Entity, ->state, s32) = 0;
            FIELD_AT(PLANT42(g_CurrentEntity)->roots, Entity, ->state, s32) = 0;
        }
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            g_CurrentEntity->state = 8;
        }
    }
}

void plant42_behavior_recoil(void) {
    s32 plant42_joint_move_a9() __asm__("plant42_joint_move");
    void *body;
    s32 pad14;
    u8 anims[8] = { 2, 3, 4, 5, 10, 11, 12, 13 };
    s32 pad20;
    u16 swing;

    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, blend_counter, u8) = 0x3FU;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = (u8) anims[((g_ApiCallPtr = api_rand)() & 7)];
        PLANT42(g_CurrentEntity)->rot_speed = (s16) (0x60 - (((g_ApiCallPtr = api_rand)() & 1) * 0xC0));
        PLANT42(g_CurrentEntity)->grab_joint = 8;
        g_CurrentEntity->action_ticks_counter = (((g_ApiCallPtr = api_rand)() & 7) + 1);
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            g_CurrentEntity->action_ticks_counter = 6U;
        }
        if ((g_ApiCallPtr = api_rand)() & 1) {
            ENTITY_AS(g_CurrentEntity, blend_counter, u8) = 7U;
            g_CurrentEntity->action_state = 3U;
            ENTITY_AS(g_CurrentEntity, animation_id, u8) = 0U;
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, (void *)1, 0, g_CurrentEntity->joints + 0x720);
            return;
        }
    case 1:
        swing = ((u16) PLANT42(g_CurrentEntity)->rot_speed) + 0x60;
        if (swing >= 0xC1U) {
            PLANT42(g_CurrentEntity)->grab_joint *= -1;
        }
        PLANT42(g_CurrentEntity)->rot_speed = (((u16) PLANT42(g_CurrentEntity)->rot_speed) - PLANT42(g_CurrentEntity)->grab_joint);
        if (PLANT42(g_CurrentEntity)->rot_speed == 0) {
            ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
            g_CurrentEntity->timing_control = 0;
            ENTITY_AS(g_CurrentEntity, blend_counter, u8) = 0x3FU;
            ENTITY_AS(g_CurrentEntity, animation_id, u8) = (u8) anims[((g_ApiCallPtr = api_rand)() & 7)];
            if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
                g_CurrentEntity->action_state = 2U;
            }
        }
        if (ENTITY_AS(g_CurrentEntity, angle_z, s16) < -0x17C) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) = (s16) ((u16) ENTITY_AS(g_CurrentEntity, angle_z, s16) + 0x10);
        }
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle - ((u16) PLANT42(g_CurrentEntity)->rot_speed));
        plant42_joint_move_a9(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        return;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->hit_state = 0;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = 2U;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            body = ENTITY_AS(g_CurrentEntity, aim_target, void *);
            ENT(body)->hit_state = 0;
            ENT(body)->behavior_flags = 0;
            if (((s16) ENT(body)->position_vz) >= 2) {
                g_CurrentEntity->hit_state = 1;
            }
        }
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0, 0);
        }
        return;
    case 3:
        plant42_joint_move_a9(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(u8, g_PlayerPosX), (void *)0x40);
        if (ENTITY_AS(g_CurrentEntity, angle_z, s16) >= -0x253) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) = (s16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x40);
        }
        if (ENTITY_AS(g_CurrentEntity, blend_counter, u8) == 0) {
            g_CurrentEntity->action_state = 1U;
        }
    }
}

void plant42_behavior_scd_flinch(void) {
    u8 plant42_joint_move_a10() __asm__("plant42_joint_move");
    s32 joints;

    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xD;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 3;
        /* fallthrough */
    case 1:
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 7 & 0xFF) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) -= 8;
        }
        if ((g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && !(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp1C) = API_AS(u32, api_rand)() % 5;
            (g_ApiCallPtr = api_effect_create_billboard)(0xEU, 3, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 0, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(5U, 0x6E, 0);
            PLANT42(g_CurrentEntity)->scd_effect_timer = 0xA;
        }
        g_CurrentEntity->action_state += plant42_joint_move_a10(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        return;
    case 2:
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 7 & 0xFF) {
            ENTITY_AS(g_CurrentEntity, angle_z, s16) += 8;
        }
        if ((g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && !(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp1C) = API_AS(u32, api_rand)() % 5;
            (g_ApiCallPtr = api_effect_create_billboard)(0xEU, 3, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 0, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(5U, 0x6E, 0);
        }
        g_CurrentEntity->action_state += plant42_joint_move_a10(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        return;
    case 3:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->hit_state = 0;
            ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->behavior_flags = 0;
            if (((s16) ENT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->position_vz) >= 2) {
                ENTITY_AS(g_CurrentEntity, hit_state, u8) = 1U;
            }
        }
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            g_CurrentEntity->state = 8;
        }
    }
}

/* Capcom's source file aligned the byte table above to 8 within its own .rodata: the 4 zero
   bytes after it, before plant42_behavior_death's table (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void plant42_behavior_death(void) {
    u8 plant42_joint_move_u8() __asm__("plant42_joint_move");
    u16 delta_a;
    u16 delta_b;
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x3F;
        g_CurrentEntity->animation_id = 0xD;
        g_CurrentEntity->scale = 0x1000;
        g_CurrentEntity->action_ticks_counter = 0;
        PLANT42(g_CurrentEntity)->death_bounce_count = 5;
        PLANT42(g_CurrentEntity)->death_wilt_timer = ((u32) (g_ApiCallPtr = api_rand)() % 14) + 0xF;
        PLANT42(g_CurrentEntity)->rot_speed = ((g_ApiCallPtr = api_rand)() & 7) + 8;
        PLANT42_AS(g_CurrentEntity, rot_speed, u16) -= (PLANT42(g_CurrentEntity)->rot_speed + PLANT42(g_CurrentEntity)->rot_speed) * ((g_ApiCallPtr = api_rand)() & 1);
        PLANT42(g_CurrentEntity)->yaw_swing = ((g_ApiCallPtr = api_rand)() & 1) * 0x800;
        PLANT42(g_CurrentEntity)->tilt_swing = (((g_ApiCallPtr = api_rand)() & 1) * 0x800) + 0x400;
        PLANT42_AS(g_CurrentEntity, yaw_step, u8) = 0x20 - (((g_ApiCallPtr = api_rand)() & 1) * 0x40);
        PLANT42_AS(g_CurrentEntity, tilt_step, u8) = 0x40 - (((g_ApiCallPtr = api_rand)() & 1) * 0x80);
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            (g_ApiCallPtr = api_Flg_on)(LITP(u16, g_EnemiesFlags), g_CurrentEntity->death_event_id);
            (g_ApiCallPtr = api_sound_play_entity_se)((u16 *)7, 0U);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x1BU, 0, g_CurrentEntity->coord_m, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)((u16 *)1, 0U);
            (g_ApiCallPtr = api_pad_vibrate_large)((u16 *)0x1E, 0x46U, 0);
        }
        (g_ApiCallPtr = api_sound_play_se_3d)((u16 *)2, 9U, 0, g_CurrentEntity->joints + 0x4B4);
        /* fallthrough */
    case 1:
        plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->angle_z = g_CurrentEntity->angle_z + 0x10;
        g_CurrentEntity->scale = g_CurrentEntity->scale + 0xFFF0;
        if (!({ (s32) PLANT42_AS(g_CurrentEntity, death_wilt_timer, s8)--; })) {
            g_CurrentEntity->action_state = 2;
        }
        return;
    case 2:
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        /* fallthrough */
    case 3:
        g_CurrentEntity->animation_id = 2;
        if ((u16) g_CurrentEntity->scale >= 0x3E9U) {
            g_CurrentEntity->scale = g_CurrentEntity->scale + 0xFFF8;
            joints = g_CurrentEntity->joints;
            delta_a = (MEMBER_AT((JOINT_N(joints, 7)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16)) + 0x32;
            delta_b = (MEMBER_AT((JOINT_N(joints, 14)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16)) + 0x32;
            if ((delta_a >= 0x65U) && (delta_b >= 0x65U)) {
                delta_a = ((g_CurrentEntity->rot_x - PLANT42(g_CurrentEntity)->tilt_swing) + 0x32) & 0xFFF;
                delta_b = ((g_CurrentEntity->angle - PLANT42(g_CurrentEntity)->yaw_swing) + 0x32) & 0xFFF;
                if (delta_a >= 0x65U) {
                    g_CurrentEntity->rot_x = g_CurrentEntity->rot_x + (s8) PLANT42_AS(g_CurrentEntity, tilt_step, u8);
                }
                if (delta_b >= 0x65U) {
                    g_CurrentEntity->angle = g_CurrentEntity->angle + (s8) PLANT42_AS(g_CurrentEntity, yaw_step, u8);
                }
            }
            g_CurrentEntity->pos_y = g_CurrentEntity->pos_y + (s16) g_CurrentEntity->action_ticks_counter;
            g_CurrentEntity->action_ticks_counter = g_CurrentEntity->action_ticks_counter + 5;
            if (g_CurrentEntity->pos_y >= -0xC7) {
                ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = (s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) / 3) * -1;
                g_CurrentEntity->pos_y = -0xC8;
                if (!({ (s32) PLANT42_AS(g_CurrentEntity, death_bounce_count, s8)--; })) {
                    g_CurrentEntity->action_state = 4;
                    ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0x80;
                }
                if ((s8) PLANT42(g_CurrentEntity)->death_bounce_count == 4) {
                    (g_ApiCallPtr = api_sound_play_se_3d)((u16 *)2, 8U, 0, g_CurrentEntity->joints + 0x530);
                    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                    (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x1BU, 0, g_CurrentEntity->coord_m, ADDR_sp_vec, 0);
                    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
                        LIT(s32, sp_vec_vy) = -0x190;
                        (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
                        (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 7), ADDR_sp_vec, 0);
                        (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 13), ADDR_sp_vec, 0);
                    }
                    LIT(s32, sp_vec_vy) = -0x190;
                    (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 4), ADDR_sp_vec, 0);
                    (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 9), ADDR_sp_vec, 0);
                    (g_ApiCallPtr = api_effect_create_billboard)((u16 *)9, 0x11U, 0, JOINT_N_WORLD(joints, 14), ADDR_sp_vec, 0);
                }
            }
            plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
            plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        }
        return;
    case 4:
        if ((u16) g_CurrentEntity->scale >= 0x12DU) {
            g_CurrentEntity->scale = g_CurrentEntity->scale + 0xFFE0;
            (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x15U, -1);
            joints = g_CurrentEntity->joints;
            delta_a = (MEMBER_AT((JOINT_N(joints, 7)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16)) + 0x32;
            delta_b = (MEMBER_AT((JOINT_N(joints, 14)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16)) + 0x32;
            if ((delta_a >= 0x65U) && (delta_b >= 0x65U)) {
                delta_a = ((g_CurrentEntity->rot_x - PLANT42(g_CurrentEntity)->tilt_swing) + 0x32) & 0xFFF;
                delta_b = ((g_CurrentEntity->angle - PLANT42(g_CurrentEntity)->yaw_swing) + 0x32) & 0xFFF;
                if (delta_a >= 0x65U) {
                    g_CurrentEntity->rot_x = g_CurrentEntity->rot_x + (s8) PLANT42_AS(g_CurrentEntity, tilt_step, u8);
                }
                if (delta_b >= 0x65U) {
                    g_CurrentEntity->angle = g_CurrentEntity->angle + (s8) PLANT42_AS(g_CurrentEntity, yaw_step, u8);
                }
            }
            plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
            plant42_joint_move_u8(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
            return;
        }
        if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
            g_CurrentEntity->has_enter_switch_zone = 0;
        }
        g_CurrentEntity->action_state = 5;
    }
}
/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void player_anim_limb_bounce(void) {
    void *joint;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.blend_counter = 3;
        joint = (*(void * *)&g_PlayerEntity.joints);
        JOINT(joint)->vel_x = 0;
        JOINT(joint)->vel_y = 0;
        JOINT(joint)->vel_z = 0;
        JOINT(JOINT_N(joint, 2))->vel_x = 0;
        JOINT(JOINT_N(joint, 2))->vel_y = 0;
        JOINT(JOINT_N(joint, 2))->vel_z = 0;
        JOINT(JOINT_N(joint, 2))->rotation.vx = 0;
        JOINT(JOINT_N(joint, 2))->rotation.vy = 0;
        JOINT(JOINT_N(joint, 2))->rotation.vz = 0;
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, (void *)0x30, 0x80820, (void *)0x606060);
        (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint, 2), (void *)0x30, 0x80820, (void *)0x606060);
        (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint, 1), (void *)0x30, 0x80820, (void *)0x606060);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, JOINT_N_WORLD(joint, 2), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, JOINT_N_WORLD(joint, 1), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, (void *)3, 0, LITP(void, g_PlayerPosX));
        (g_ApiCallPtr = api_sound_play_se_3d)((void *)4, NULL, 0, LITP(void, g_PlayerPosX));
        g_PlayerEntity.status_flags |= 4;
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) = 0xF;
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x1E, NULL, 0xFA, NULL);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)6, NULL);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)8, (void *)0xF);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)6, (void *)0x19);
        /* fallthrough */
    case 1:
        if ((*(s16 *)&g_PlayerEntity.action_ticks_counter) != 0) {
            (*(s16 *)&g_PlayerEntity.action_ticks_counter)--;
            (g_ApiCallPtr = api_joint_move)(NULL, ((void *) g_PlayerWork.grab_anim_header), g_PlayerWork.grab_anim_base, (void *)0x400);
        }
        joint = (*(void * *)&g_PlayerEntity.joints);
        if (JOINT(joint)->rotation.vz >= -0x3FF) {
            JOINT(joint)->rotation.vz = (u16) (((u16) JOINT(joint)->rotation.vz) - ((u16) JOINT(joint)->vel_x));
            JOINT(joint)->vel_x = (u16) (((u16) JOINT(joint)->vel_x) + 8);
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
        }
        joint += 0xF8;
        if ((JOINT(joint)->rotation.vy < 0x100) && !((s16) ((u16) JOINT(joint)->vel_x) & 0x8000)) {
            JOINT(joint)->rotation.vy = (s16) ((u16) JOINT(joint)->rotation.vy + 0x20);
            JOINT(joint)->rotation.vx = (u16) (((u16) JOINT(joint)->rotation.vx) - ((u16) JOINT(joint)->vel_x));
            JOINT(joint)->vel_x = (u16) (((u16) JOINT(joint)->vel_x) + 2);
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
            (g_ApiCallPtr = api_MulMatrix2)(&JOINT(joint)->transform, &JOINT(joint)->world);
            if (JOINT(joint)->rotation.vy >= 0x100) {
                JOINT(joint)->vel_x = -0x7FFE;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xA, NULL);
                (g_ApiCallPtr = api_pad_vibrate_large)((void *)0x14, (void *)0xFA, 0);
                (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x1E, (void *)0xFA, 0, (void *)0x14);
            }
        }
        return;
    case 2:
        joint = (*(void * *)&g_PlayerEntity.joints);
        JOINT(joint)->vel_x = -0x7FFE;
        g_PlayerEntity.action_state = 3;
        (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.blend_counter = 0xF;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, JOINT_N_WORLD(joint, 2), ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vy) = 0x1F4;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, (void *)3, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(NULL, (*(void * *)&g_PlayerEntity.anim_header), g_PlayerEntity.anim_base, (void *)0x100);
        return;
    case 4:
        g_PlayerEntity.action_state = 5;
        (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 5:
        if ((g_ApiCallPtr = api_joint_move)(NULL, ((void *) g_PlayerWork.grab_anim_header), g_PlayerWork.grab_anim_base, (void *)0x400) != 0) {
            g_PlayerEntity.action_state = 6;
            g_PlayerEntity.timing_control = 0;
            g_PlayerEntity.animation_id = 0;
            g_PlayerEntity.blend_counter = 3;
        }
        return;
    case 6:
        (g_ApiCallPtr = api_joint_move)(NULL, (*(void * *)&g_PlayerEntity.anim_header), g_PlayerEntity.anim_base, (void *)0x400);
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void player_anim_set_attacked_flag(void) {
    extern s32 (*player_plant42_hit_tbl[])();
    player_plant42_hit_tbl[g_PlayerEntity.action_behavior]();
}

void player_anim_knockdown_recover(void) {
    s32 joints;
    s32 pad1C;
    VECTOR saved_pos;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 6;
        g_PlayerEntity.blend_counter = 4;
        g_PlayerEntity.move_speed_current = 0x3E8;
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 2, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0xFA, 0, 0xA);
        /* fallthrough */
    case 1:
        g_PlayerEntity.move_speed_current -= g_PlayerEntity.animation_frame_id * 0xF;
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        saved_pos = *(VECTOR *)LITP(void, g_PlayerPosX);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(ADDR_g_PlayerPosX, (s32) SCA(((void *) g_PlayerEntity.sca_info))->radius);
        *(VECTOR *)LITP(void, g_PlayerPosX) = saved_pos;
        if (LIT(s32, sp_tmp00) != 0) {
            g_PlayerEntity.action_state = 5;
        }
        return;
    case 2:
        g_PlayerEntity.action_state = 3;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 7;
        g_PlayerEntity.blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1D, 0, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 3:
        if (!(g_PlayerEntity.animation_frame_id & 1 & 0xFF) && ((u8) g_PlayerEntity.animation_frame_id < 0xAU)) {
            joints = g_PlayerEntity.joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 5), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        g_PlayerEntity.move_speed_current -= 0xF;
        if ((s16) g_PlayerEntity.move_speed_current < 0) {
            g_PlayerEntity.move_speed_current = 0;
        }
        return;
    case 4:
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
        g_PlayerEntity.hit_state = 0;
        return;
    case 5:
        g_PlayerEntity.action_state = 6;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 4;
        g_PlayerEntity.blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1A, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 2, 0, ADDR_g_PlayerPosX);
        joints = g_PlayerEntity.joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        SCRATCH_VEC->x = (s32) (SCRATCH_VEC->x - 0x190);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 5), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0xFA, 0, 5);
        /* fallthrough */
    case 6:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 7:
        g_PlayerEntity.action_state = 8;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 5;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 8:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 9:
        g_PlayerEntity.action_state = 0xA;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 4;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 10:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        return;
    case 11:
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
        g_PlayerEntity.hit_state = 0;
        g_PlayerEntity.status_flags &= 0xFD;
        /* fallthrough */
    default:
    }
}

void player_anim_grabbed(void) {
    void *joints;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.blend_counter = 3;
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)0xA, 0x78, 0);
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)0x14, 0x78, 0x14);
        /* fallthrough */
    case 1:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, (void *)0x400);
        return;
    case 2:
        g_PlayerEntity.action_state = 3;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 1;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, (void *)0x400);
        if (g_PlayerEntity.animation_frame_id == 0) {
            (g_ApiCallPtr = api_pad_vibrate_large)((void *)0xA, 0x96, 0);
        }
        return;
    case 4:
        g_PlayerEntity.action_state = 5;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, (void *)0x400);
        if (g_PlayerEntity.animation_frame_id == 0) {
            (g_ApiCallPtr = api_pad_vibrate_large)((void *)0xA, 0xC8, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)((void *)0x14, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)((void *)0xA, 0x78, 0xA);
        }
        return;
    case 6:
        joints = ((void *) g_PlayerEntity.joints);
        FIELD_AT(joints, Joint, [2].flags, s8) = (s8) (FIELD_AT(joints, Joint, [2].flags, u8) | 0xC);
        g_PlayerEntity.action_state = 7;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, JOINT_N_WORLD(joints, 2), LITP(void, sp_vec), 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, NULL, JOINT_N_POS(joints, 0), 0);
        (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 2), 0x30, 0x80820, (void *)0x606060);
        (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 2), 0x30, 0x80820, (void *)0x606060);
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void player_anim_thrown(void) {
    s32 joints;
    s32 unused;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 3;
        if (g_PlayerEntity.health < 0) {
            (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)3, 3, 0, ADDR_g_PlayerPosX);
        }
        (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)0xA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xA, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x1E, 0xFA, 0, 0xA);
        /* fallthrough */
    case 1:
        if (g_PlayerEntity.animation_frame_id == 1) {
            joints = g_PlayerEntity.joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 4), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 7), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
        }
        if (g_PlayerEntity.animation_frame_id == 0xC) {
            g_PlayerEntity.action_state = 2;
            (*(s16 *)&g_PlayerEntity.action_ticks_counter) = 0x5A;
            if (g_PlayerEntity.health < 0) {
                g_PlayerEntity.action_state = 8;
            }
        }
        (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 2:
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 4 : 1;
        if ((*(s16 *)&g_PlayerEntity.action_ticks_counter) < 0) {
            g_PlayerEntity.action_state = 3;
        }
        return;
    case 3:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 4:
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0x700;
        return;
    case 5:
        g_PlayerEntity.action_state = 6;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 3;
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) = 0;
        (*(u16 *)&g_PlayerEntity.move_speed_current) = 0x1F4;
        g_PlayerEntity.blend_counter = 7;
        (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)3, 1, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)2, 0x19, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xA, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x1E, 0xFA, 0, 0xA);
        /* fallthrough */
    case 6:
        if ((g_PlayerEntity.animation_frame_id != 5) && (g_PlayerEntity.animation_frame_id != 7)) {

        } else {
            joints = g_PlayerEntity.joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 5), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((s32 *)9, 0x11, 0, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x200);
        if ((u8) g_PlayerEntity.animation_frame_id < 0xFU) {
            LIT(s32, sp_tmp00) = (s32) (*(s16 *)&g_PlayerEntity.angle);
            (*(s16 *)&g_PlayerEntity.angle) = (s16) g_Plant42CaptureMatrix_tx;
            (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
            (*(s16 *)&g_PlayerEntity.angle) = (s16) (u16) LIT(s32, sp_tmp00);
            (*(u16 *)&g_PlayerEntity.move_speed_current) -= (*(s16 *)&g_PlayerEntity.action_ticks_counter) * 5;
            (*(s16 *)&g_PlayerEntity.action_ticks_counter)++;
        }
        return;
    case 7:
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0x700;
        break;
    case 8:;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void plant42_render_copies(void) {
    extern s32 (*plant42_body_state_tbl[])();
    extern s32 (*plant42_roots_clone_state_tbl[])();
    MATRIX light;
    s16 unused;
    s8 unused2;

    unused2 = 0;
    LIT(void *, sp_tmp18) = g_CurrentEntity;
    g_CurrentEntity = ((void *) g_CurrentEntity->aim_target);
    (g_ApiCallPtr = api_update_entity_lighting)(&g_CurrentEntity->pos_x);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    if (g_UpdateFlags & 4 & 0xFFFF) {
        plant42_body_state_tbl[g_CurrentEntity->state]();
    }
    (g_ApiCallPtr = api_ScaleMatrix)((u8 *)g_CurrentEntity + 0x20, (u8 *)g_CurrentEntity + 0x16C);
    (g_ApiCallPtr = api_GsMulCoord0)(g_pWorldScreenMatrix, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_matrix));
    light = *(MATRIX *)g_DeadMoveValue;
    light.t[0] = g_CurrentEntity->pos_x;
    light.t[1] = g_CurrentEntity->pos_y;
    light.t[2] = g_CurrentEntity->pos_z;
    (g_ApiCallPtr = api_MulMatrix0)(g_pLightMatrix, &light, &light);
    LIT(void *, sp_tmp38) = ((void *) g_CurrentEntity->model_load_buffer);
    (g_ApiCallPtr = api_SetLightMatrix)(&light);
    (g_ApiCallPtr = api_GsSetLsMatrix)(LITP(s32, sp_matrix));
    (g_ApiCallPtr = api_GsPrstTG3L)(FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->primitive_top, s32 *), ((s32 *) TMD_OBJ(LIT(s32, sp_tmp38))->vert_top), FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->normal_top, s32 *), g_CurrentEntity->packet + (g_ActiveBuffer * 0x28) + 8, FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->n_primitive, s32), 4, (g_ActiveBuffer * 0x14) + ADDR_g_OtMain);
    (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, NULL, 0);
    g_CurrentEntity = LIT(Entity *, sp_tmp18);
    g_CurrentEntity = PLANT42(g_CurrentEntity)->roots;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = g_CurrentEntity->scale + plant42_pulse_tbl[0x59 - ((s16) g_CurrentEntity->action_ticks_counter)];
    (g_ApiCallPtr = api_ScaleMatrix)((u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec));
    if (g_UpdateFlags & 4 & 0xFFFF) {
        plant42_roots_clone_state_tbl[g_CurrentEntity->state]();
    }
    (g_ApiCallPtr = api_GsMulCoord0)(g_pWorldScreenMatrix, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_matrix));
    LIT(s32, sp_tmp38) = (s32)((void *) g_CurrentEntity->model_load_buffer);
    (g_ApiCallPtr = api_GsSetLsMatrix)(LITP(s32, sp_matrix));
    (g_ApiCallPtr = api_GsPrstTG3L)(FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->primitive_top, s32 *), ((s32 *) TMD_OBJ(LIT(s32, sp_tmp38))->vert_top), FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->normal_top, s32 *), g_CurrentEntity->packet + (g_ActiveBuffer * 0x28) + 8, FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->n_primitive, s32), 4, (g_ActiveBuffer * 0x14) + ADDR_g_OtMain);
    g_CurrentEntity = LIT(Entity *, sp_tmp18);
}

void plant42_body_idle(void) {
    u8 swing;

    PLANT42_BODY_FIELD(g_CurrentEntity, scale_x) = (s32) ((plant42_pulse_tbl[0x59 - ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)] * 3) + 0x1000);
    PLANT42_BODY(g_CurrentEntity)->scale_y = (s32) ((plant42_pulse_tbl[0x59 - ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)] * 3) + 0x1000);
    PLANT42_BODY(g_CurrentEntity)->scale_z = (s32) ((plant42_pulse_tbl[0x59 - ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)] * 3) + 0x1000);
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0x41) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
        if ((u16) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0x59;
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ENTITY_AS(g_CurrentEntity, speed_vy, u16));
    g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + g_CurrentEntity->position_vx);
    ENTITY_AS(g_CurrentEntity, speed_vy, u16) = (u16) (ENTITY_AS(g_CurrentEntity, speed_vy, u16) - ENTITY_AS(g_CurrentEntity, speed_vz, u16));
    g_CurrentEntity->position_vx = (u16) (g_CurrentEntity->position_vx - g_CurrentEntity->position_vy);
    swing = (u8) ENTITY_AS(g_CurrentEntity, speed_vy, u16) + 8;
    if (swing >= 0x11U) {
        g_CurrentEntity->speed_vz *= -1;
    }
    swing = (u8) g_CurrentEntity->position_vx + 7;
    if (swing >= 0xFU) {
        ENTITY_AS(g_CurrentEntity, position_vy, s16) *= -1;
    }
}

void plant42_body_shrivel(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->speed_vx = 0x80;
        (g_ApiCallPtr = api_clut_fade_add_delta)((void *)-1, -1, 0, NULL, 0x200, 8);
        ENTITY_AS(g_CurrentEntity, blend_counter, u8) = 0xFU;
        g_CurrentEntity->action_state = 1U;
        /* fallthrough */
    case 1:
        PLANT42_BODY_FIELD(g_CurrentEntity, scale_x) += -0x18 - g_CurrentEntity->speed_vx;
        PLANT42_BODY(g_CurrentEntity)->scale_y += -0x18 - g_CurrentEntity->speed_vx;
        PLANT42_BODY(g_CurrentEntity)->scale_z += -0x18 - g_CurrentEntity->speed_vx;
        g_CurrentEntity->speed_vx *= -1;
        if (!({ (s32) ENTITY_AS(g_CurrentEntity, blend_counter, u8)--; })) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(NULL, -1, 0, NULL, 0x200, 8);
        }
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x10, -0x10);
        if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && !((ENTITY_AS(g_CurrentEntity, blend_counter, u8) % 30) & 0xFF)) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0xBB8;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x1B, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        }
        break;
    case 2:
        break;
    case 3:
        g_CurrentEntity->speed_vx = -0x80;
        g_CurrentEntity->action_state = 4U;
        /* fallthrough */
    case 4:
        PLANT42_BODY_FIELD(g_CurrentEntity, scale_x) += 0x18 + g_CurrentEntity->speed_vx;
        PLANT42_BODY(g_CurrentEntity)->scale_y += 0x18 + g_CurrentEntity->speed_vx;
        PLANT42_BODY(g_CurrentEntity)->scale_z += 0x18 + g_CurrentEntity->speed_vx;
        g_CurrentEntity->speed_vx *= -1;
        ENTITY_AS(g_CurrentEntity, blend_counter, u8)--;
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 0x10, 0x10);
        if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && !((ENTITY_AS(g_CurrentEntity, blend_counter, u8) % 30) & 0xFF)) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0xBB8;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x1B, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        }
    }
    if (!((g_ApiCallPtr = api_rand)() & 0x1F)) {
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = 0x7D0;
        SCRATCH_VEC->x = (s32) ((g_ApiCallPtr = api_rand)() & 0x1FF);
        LIT(s32, sp_vec_vz) = (g_ApiCallPtr = api_rand)() & 0x1FF;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0x1C, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void plant42_body_fall(void) {

    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->speed_vx = 0;
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, blend_counter, u8) = 0x78U;
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (g_CurrentEntity->speed_vx * 5));
        ++g_CurrentEntity->speed_vx;
        if ((g_CurrentEntity->speed_vx < 6) && ((s16) (g_CurrentEntity->speed_vx & 1) != 0)) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(-1, -1, 0, 0, 0x200, 8);
        }
        if (g_CurrentEntity->pos_y >= 0xC8) {
            (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0x96, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0x96, 0, 0xA);
            --g_CurrentEntity->speed_vx;
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - (g_CurrentEntity->speed_vx * 5));
            g_CurrentEntity->action_state = 2U;
            ENTITY_AS(g_CurrentEntity, blend_counter, u8)--;
            PLANT42_BODY(g_CurrentEntity)->scale_y = (s32) (PLANT42_BODY(g_CurrentEntity)->scale_y - 0x914);
            PLANT42_BODY_FIELD(g_CurrentEntity, scale_x) = (s32) (PLANT42_BODY_FIELD(g_CurrentEntity, scale_x) + 0x9DC);
            PLANT42_BODY(g_CurrentEntity)->scale_z = (s32) (PLANT42_BODY(g_CurrentEntity)->scale_z + 0x9DC);
            if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                SCRATCH_VEC->x = 0x258;
                LIT(s32, sp_vec_vz) = 0x258;
                LIT(s32, sp_vec_vy) = -0xC8;
                (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, (s32) ((u8 *)g_CurrentEntity + 0x20), ADDR_sp_vec, 0x28);
                SCRATCH_VEC->x = -0x258;
                LIT(s32, sp_vec_vz) = 0x258;
                (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0x400, (s32) ((u8 *)g_CurrentEntity + 0x20), ADDR_sp_vec, 0x28);
                SCRATCH_VEC->x = -0x258;
                LIT(s32, sp_vec_vz) = -0x258;
                (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0x200, (s32) ((u8 *)g_CurrentEntity + 0x20), ADDR_sp_vec, 0x28);
                SCRATCH_VEC->x = 0x258;
                LIT(s32, sp_vec_vz) = -0x258;
                (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0x800, (s32) ((u8 *)g_CurrentEntity + 0x20), ADDR_sp_vec, 0x28);
                SCRATCH_VEC->x = 0;
                LIT(s32, sp_vec_vy) = -0x258;
                LIT(s32, sp_vec_vz) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0x1B, 0, (s32) ((u8 *)g_CurrentEntity + 0x20), ADDR_sp_vec, 0x3C);
            }
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 8, 0, (s32) (&g_CurrentEntity->pos_x));
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0, 0, (s32) (&g_CurrentEntity->pos_x));
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0, 0, (s32) (&g_CurrentEntity->pos_x));
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0, 0, (s32) (&g_CurrentEntity->pos_x));
            g_CurrentEntity->speed_vx = 0x3C;
        }
        return;
    case 2:
        if (ENTITY_AS(g_CurrentEntity, blend_counter, u8) != 0) {
            /* ({ x; }) on the read keeps its notes inside the rand() result's life (-O0 stupid.c
             * allocates longest-lived pseudos first): rand() gets a0 before the x78 load.
             * ({ x78 * 2; }) stops fold from re-associating (r & 1) * (x78 * 2). */
            PLANT42_BODY(g_CurrentEntity)->scale_y = ({ PLANT42_BODY(g_CurrentEntity)->scale_y; }) + (g_CurrentEntity->speed_vx - (((g_ApiCallPtr = api_rand)() & 1) * ({ g_CurrentEntity->speed_vx * 2; })));
            PLANT42_BODY(g_CurrentEntity)->scale_x = ({ PLANT42_BODY(g_CurrentEntity)->scale_x; }) + (g_CurrentEntity->speed_vx - (((g_ApiCallPtr = api_rand)() & 1) * ({ g_CurrentEntity->speed_vx * 2; })));
            PLANT42_BODY(g_CurrentEntity)->scale_z = ({ PLANT42_BODY(g_CurrentEntity)->scale_z; }) + (g_CurrentEntity->speed_vx - (((g_ApiCallPtr = api_rand)() & 1) * ({ g_CurrentEntity->speed_vx * 2; })));
            g_CurrentEntity->speed_vx *= -1;
            if (g_CurrentEntity->speed_vx < 0) {
                ++g_CurrentEntity->speed_vx;
            }
            ENTITY_AS(g_CurrentEntity, blend_counter, u8)--;
        }
    }
}

void plant42_roots_clone_idle(void) {

        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        g_CurrentEntity->action_ticks_counter = 0x59U;
    }
}

void plant42_roots_clone_pulse(void) {
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->speed_vx = 0x10;
        g_CurrentEntity->action_state = 1;
    case 1:
        g_CurrentEntity->scale -= (u16) (g_CurrentEntity->speed_vx + 0x30);
        g_CurrentEntity->speed_vx *= -1;
        break;
    case 2:
        break;
    case 3:
        g_CurrentEntity->action_state = 4;
        g_CurrentEntity->speed_vx = -0x10;
    case 4:
        g_CurrentEntity->scale += (u16) (g_CurrentEntity->speed_vx + 0x30);
        g_CurrentEntity->speed_vx *= -1;
    }
}

void plant42_roots_clone_rise(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->speed_vx = 0;
        g_CurrentEntity->action_state = 1U;
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (g_CurrentEntity->speed_vx * 4));
        g_CurrentEntity->speed_vx++;
        if (g_CurrentEntity->pos_y >= -0x512) {
            g_CurrentEntity->speed_vx--;
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - (g_CurrentEntity->speed_vx * 4));
            LIT(s32, sp_vec_vy) = (s32) (LIT(s32, sp_vec_vy) / 2);
        }
        if ((u16) g_CurrentEntity->scale >= 0x3E8U) {
            g_CurrentEntity->scale = (u16) (g_CurrentEntity->scale + 0xFFC8);
        }
    }
}

s32 plant42_joint_move(u8 reverse, void *anim_header, void *anim_base, s16 blend_step) {
    s16 *kf;
    s32 *base;
    u8 *frame;
    s32 *entry;
    void *joint;
    u8 unused;
    u8 blend;
    u16 diff;
    VECTOR rot;

    if (ENTITY_AS(g_CurrentEntity, timing_control, u8) != 0) {
        --ENTITY_AS(g_CurrentEntity, timing_control, u8);
    }
    base = anim_header;
    kf = (s16 *) (base + (s16) (FIELD_AT(base, AnimEntry, ->frame_offset, s16) / 4));
    LIT(s32, sp_tmp00) = (s16) (FIELD_AT(base, AnimEntry, [1].frame_offset, s16) / 2);
    base = anim_base;
    entry = base + ENTITY_AS(g_CurrentEntity, animation_id, u8);
    frame = (u8 *) (base + (u16) (ANIM_ENTRY(entry)->frame_offset >> 2));
    frame += g_CurrentEntity->animation_frame_id * 4;
    if (reverse != 0) {
        frame += ((ANIM_ENTRY(entry)->frame_count - (g_CurrentEntity->animation_frame_id * 2)) * 4) - 4;
    }
    kf += *(u16 *)frame * LIT(s32, sp_tmp00);
    joint = ENTITY_AS(g_CurrentEntity, joints, void *);
    blend = ENTITY_AS(g_CurrentEntity, blend_counter, u8);
    if (blend != 0) {
        --ENTITY_AS(g_CurrentEntity, blend_counter, u8);
    }
    JOINT(joint)->transform.t[0] = *kf;
    if ((blend == 0) && (ENTITY_AS(g_CurrentEntity, timing_control, u8) != 0)) {
        blend = (0x1000 / blend_step) / (ENTITY_AS(g_CurrentEntity, timing_control, u8) + 1);
    }
    LIT(s32, sp_tmp10) = g_CurrentEntity->joint_count;
    if (blend != 0) {
        JOINT(joint)->transform.t[1] = ((JOINT(joint)->transform.t[1] * blend_step * blend) / 4096) + ((*++kf * blend_step * ((0x1000 / blend_step) - blend)) / 4096);
        JOINT(joint)->transform.t[2] = *++kf;
        kf++;
        kf += 3;
        while ((LIT(s32, sp_tmp10) = LIT(s32, sp_tmp10) - 1) != -1) {
            LIT(s32, sp_vec_vx) = *kf;
            LIT(s32, sp_vec_vy) = *++kf;
            LIT(s32, sp_vec_vz) = *++kf;
            LIT(SVECTOR, sp_svec) = JOINT(joint)->rotation;
            if (blend == (0x1000 / blend_step) - 1) {
                LIT(s32, sp_tmp1C) = 2;
                do {
                    diff = (*(u16 *)(LITP(s32, sp_vec) + LIT(s32, sp_tmp1C)) - LITP(u16, sp_svec)[LIT(s32, sp_tmp1C)]) + 0x800;
                    if (diff > 0x1000) {
                        LITP(s16, sp_svec)[LIT(s32, sp_tmp1C)] += !(diff & 0x8000) ? 0x1000 : -0x1000;
                    }
                } while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1);
            }
            LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vx) << 4;
            LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vy) << 4;
            LIT(s32, sp_vec_vz) = LIT(s32, sp_vec_vz) << 4;
            rot.vx = LIT(s16, sp_svec_vx) << 4;
            rot.vy = LIT(s16, sp_svec_vy) << 4;
            rot.vz = LIT(s16, sp_svec_vz) << 4;
            (g_ApiCallPtr = api_LoadAverage12)(&rot, ADDR_sp_vec, blend_step * blend, blend_step * ((0x1000 / blend_step) - blend), &rot);
            LIT(s16, sp_svec_vx) = rot.vx >> 4;
            LIT(s16, sp_svec_vy) = rot.vy >> 4;
            LIT(s16, sp_svec_vz) = rot.vz >> 4;
            JOINT(joint)->rotation = LIT(SVECTOR, sp_svec);
            kf++;
            (g_ApiCallPtr = api_RotMatrix)(ADDR_sp_svec, &JOINT(joint)->transform);
            (*(Joint **)&joint)++;
        }
    } else {
        JOINT(joint)->transform.t[1] = *++kf;
        JOINT(joint)->transform.t[2] = *++kf;
        kf++;
        kf += 3;
        while ((LIT(s32, sp_tmp10) = LIT(s32, sp_tmp10) - 1) != -1) {
            JOINT(joint)->rotation.vx = *kf;
            JOINT(joint)->rotation.vy = *++kf;
            JOINT(joint)->rotation.vz = *++kf;
            kf++;
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
            (*(Joint **)&joint)++;
        }
    }
    if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 0) {
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = ANIM_FRAME(frame)->timing;
        if (++g_CurrentEntity->animation_frame_id > (ANIM_ENTRY(entry)->frame_count - 1)) {
            g_CurrentEntity->animation_frame_id = 0;
            return 1;
        }
    }
    return 0;
}

void plant42_scd_state(void) {
    extern s32 (*plant42_scd_action_tbl[])();

    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    plant42_scd_action_tbl[g_CurrentEntity->action_behavior]();
    if (PLANT42(g_CurrentEntity)->scd_effect_timer != 0) {
        if (({ (s32) PLANT42(g_CurrentEntity)->scd_effect_timer--; }) == 1) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp1C) = API_AS(u32, api_rand)() % 5;
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0xFFF) + 0x200;
            if (ENT(((void *) g_CurrentEntity->aim_target))->pos_y >= -0x7CF) {
                LIT(s32, sp_vec_vy) = 0;
            }
            SCRATCH_VEC->x = (s32) (0x200 - ((g_ApiCallPtr = api_rand)() & 0x3FF));
            LIT(s32, sp_vec_vz) = 0x200 - ((g_ApiCallPtr = api_rand)() & 0x3FF);
            (g_ApiCallPtr = api_effect_create_billboard)(0xEU, 3, 0, ((void *) g_CurrentEntity->aim_target) + 0x20, ADDR_sp_vec, 0x28);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 0, 0, ((void *) g_CurrentEntity->aim_target) + 0x20, ADDR_sp_vec, 0x28);
            PLANT42(g_CurrentEntity)->scd_effect_timer = 0xD;
            (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x1E, 0, ((void *) g_CurrentEntity->aim_target) + 0x34);
            if (ENT(((void *) g_CurrentEntity->aim_target))->pos_y < -0x1388) {
                (g_ApiCallPtr = api_pad_vibrate_large)(API_AS(u32, api_rand)() % 5 + 3, API_AS(u32, api_rand)() % 20 + 0x50, 0);
            }
            if (ENT(((void *) g_CurrentEntity->aim_target))->pos_y >= -0x12B) {
                PLANT42(g_CurrentEntity)->scd_effect_timer = 0;
            }
        }
    }
    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    }
}

void plant42_scd_face_player(void) {
    LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20U);
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp00));
    if (LIT(s32, sp_tmp00) == 0) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
    }
    plant42_behavior_idle();
}

void plant42_scd_recoil(void) {
    if (g_CurrentEntity->action_state == 0) {
        PLANT42(g_CurrentEntity)->scd_effect_timer = 0xA;
    }
    plant42_behavior_recoil();
}

void plant42_scd_death(void) {
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 1, 0xE, 0);
        LIT(s32, sp_tmp04) = --MEMBER_AT(((void *) g_CurrentEntity->aim_target), Entity, ->position_vz, s16);
        if (LIT(s32, sp_tmp04) == 1) {
            --MEMBER_AT(((void *) g_CurrentEntity->aim_target), Entity, ->position_vz, s16);
        }
        if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENT(((void *) g_CurrentEntity->aim_target))->health = -1;
            FIELD_AT(((void *) g_CurrentEntity->aim_target), Entity, ->state, s32) = 2;
            FIELD_AT(PLANT42(g_CurrentEntity)->roots, Entity, ->state, s32) = 2;
        }
    }
    plant42_behavior_death();
}
