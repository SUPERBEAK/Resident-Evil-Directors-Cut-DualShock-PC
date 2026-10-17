/* cerberus.c - Cerberus (zombie dog): init / update, behavior selection (patrol, chase, leap, strafe, stalk,
   bite / maul), damage and death, and the player's getting-up animation after a dog knocks them down.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/cerberus.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* cerberus data (defined in each overlay that has this enemy) */
extern s32 cerberus_sca_info;
extern s16 cerberus_probe_dist;
extern s32 cerberus_health_tbl;
extern s32 g_CerberusBarked;
extern s32 cerberus_recoil_angle_tbl;
extern u8 cerberus_turn_anim_tbl[];

/* CerberusWork: the cerberus's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). CERBERUS(e)->f, CERBERUS_FIELD(e, f) and CERBERUS_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct CerberusWork {
    /* +0x00 */ s32 player_dist;       /* 0x16C: |dx|+|dz| to the player, recomputed every frame in cerberus_behavior_run; range checks (0x1388, 0x1194, 0x9C4..) */
    /* +0x04 */ u16 turn_step;         /* 0x170: turn step per frame (rand&7+0x3D chase, rand&7+0x10 stalk, sign-flipped); also knockback direction for add_speed_xz in damaged/die */
    /* +0x06 */ s16 turn_delta;        /* 0x172: signed turn being applied (added to angle in patrol/strafe/skid; +-0x80 strafe, -1/probe result in wall slide) */
    /* +0x08 */ s16 ballistic_vel_y;   /* 0x174: initial vertical velocity for api_entity_ballistic_step (300/480/400 on entrance leaps) */
    /* +0x0A */ s16 ballistic_gravity; /* 0x176: gravity for api_entity_ballistic_step (-0x28); chase_run also caches its swerve turn here (cleared in consider_attack) */
    /* +0x0C */ u16 probe_history;     /* 0x178: wall-probe bits: bit 0 this frame (cerberus_probe_ahead), bits 1-3 previous frames (<<1 &0xE), bit 4 = did not move */
    /* +0x0E */ u8 path_result;        /* 0x17A: api_entity_pathfind_update result (bit 0/bit 1 tested in chase_run) */
    /* +0x0F */ u8 pad0F[1];
    /* +0x10 */ u16 aggro_counter;     /* 0x17C: chase +2 per blocked frame (>=0x10 -> skid), stalk counter (0x41), strafe frame count; leap/bite_player store the bite result here */
    /* +0x12 */ s16 phase_counter;     /* 0x17E: per-phase frame/step counter (lost-path frames ==6 -> give up, wall-follow steps clamped to 8, tumble land-sound latch) */
    /* +0x14 */ s16 swerve;            /* 0x180: reused: chase swerve countdown, consider_attack counter (<0xA), head_track yaw (+-0x100), tumble spin (+0x80 to 0x800), stumble joint_move result */
    /* +0x16 */ s16 blood_count;       /* 0x182: blood billboards still to spawn (3 on bite, 6 on maul); cerberus_spawn_blood decrements */
    /* +0x18 */ s8 alert_latch;        /* 0x184: 0 = still looking (api_entity_check_visual_range runs), 1 after leap/getup; bit 7 = already stumbled from a hit */
    /* +0x19 */ u8 pad19[1];
    /* +0x1A */ u16 beh_flags;         /* 0x186: behavior flags: init 1, patrol 4 or behavior_flags&1, entrance 5; bit 0 run, bit 1 hunting (stalk), bit 2 gave up (lost path) */
    /* +0x1C */ u16 ai_flags;          /* 0x188: AI flags: 0x80 active/woken, 0x20 hit reaction, 0x10 wall-follow dead end, 0x0C skid, 0x02 locked on, 0x01 re-probe */
    /* +0x1E */ s16 retarget_delay;    /* 0x18A: frames before chase_run turns straight at the target again (rand&3*2+8 in chase_turn, counted down) */
} CerberusWork;
#define CERBERUS(e) ENEMY_WORK(e, CerberusWork)
#define CERBERUS_FIELD(e, f) ENEMY_FIELD(e, CerberusWork, f)
#define CERBERUS_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, CerberusWork, f, T)
#define CERBERUS_MEMBER(e, f) ENEMY_MEMBER(e, CerberusWork, f)

/* every frame for each cerberus (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void cerberus_update(void) {
    s8 cerberus_probe_ahead_s8() __asm__("cerberus_probe_ahead");
    extern s32 (*cerberus_state_tbl[])();
    s32 old_x;
    s32 old_z;
    s32 unused[4];

    old_x = g_CurrentEntity->pos_x;
    old_z = g_CurrentEntity->pos_z;
    if (g_UpdateFlags & 4 & 0xFFFF) {
        cerberus_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
        CERBERUS_FIELD(g_CurrentEntity, probe_history) = ((CERBERUS_FIELD(g_CurrentEntity, probe_history) << 1) & 0xE);
        if (!(g_CurrentEntity->status_flags & 4 & 0xFF)) {
            CERBERUS_FIELD(g_CurrentEntity, probe_history) = (u16) (CERBERUS_FIELD(g_CurrentEntity, probe_history) | cerberus_probe_ahead_s8());
        } else {
            g_CurrentEntity->position_vx = (u16) g_CurrentEntity->pos_x;
            g_CurrentEntity->position_vy = (u16) ENTITY_AS(g_CurrentEntity, pos_y, u16);
            g_CurrentEntity->position_vz = (u16) g_CurrentEntity->pos_z;
        }
    }
    if (!(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 1 & 0xFFFF) && (g_CurrentEntity->pos_x == old_x) && (g_CurrentEntity->pos_z == old_z)) {
        CERBERUS_FIELD(g_CurrentEntity, probe_history) = (u16) (CERBERUS_FIELD(g_CurrentEntity, probe_history) | 0x10);
    }
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0, ((s16) g_CurrentEntity->angle));
    }
}

/* state 0: sets up a new cerberus (health, sca info, animation, work fields), then state 1 */
void cerberus_init(void) {

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (s16) (*(u8 *)((u8 *)&cerberus_health_tbl + ((((g_ApiCallPtr = api_rand)()) & 0xF))) + ((g_ApiCallPtr = api_rand)() & 3));
    g_CurrentEntity->hit_state = 0;
    CERBERUS_FIELD(g_CurrentEntity, probe_history) = 0;
    CERBERUS_FIELD(g_CurrentEntity, beh_flags) = 1;
    CERBERUS_FIELD(g_CurrentEntity, ai_flags) = 0x80;
    g_CurrentEntity->sca_info = (s32)&cerberus_sca_info;
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) & 0x1F);
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    g_CurrentEntity->action_ticks_counter = 0;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
    CERBERUS_FIELD(g_CurrentEntity, retarget_delay) = 0;
    g_CurrentEntity->move_speed_current = 0;
    CERBERUS_FIELD(g_CurrentEntity, path_result) = 0;
    CERBERUS_FIELD(g_CurrentEntity, alert_latch) = 0;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = (s32) cerberus_probe_dist;
    (g_ApiCallPtr = api_RotMatrix)((&g_CurrentEntity->rot_x), ADDR_sp_matrix);
    (g_ApiCallPtr = api_ApplyMatrixLV)(ADDR_sp_matrix, ADDR_sp_vec, ADDR_sp_vec);
    g_CurrentEntity->position_vx = (u16) (g_CurrentEntity->position_vx + (u16) SCRATCH_VEC->x);
    g_CurrentEntity->position_vz = (u16) (g_CurrentEntity->position_vz + LIT(u16, sp_vec_vz));
    g_CerberusBarked = 0;
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, (s32) (&g_CurrentEntity->shadow), 0x550, 0x200);
    switch (g_CurrentEntity->behavior_flags) {
    case 6:
    case 7:
    case 0x80:
        g_CurrentEntity->status_flags |= 4;
        break;
    default:
        g_CurrentEntity->pos_y = 0;
        break;
    }
}

void cerberus_behavior_run(void) {
    void cerberus_behavior_select();
    extern s32 (*cerberus_behavior_tbl[])();
    s32 unused;

    if (g_CurrentEntity->behavior_flags == 0x80) {
        g_CurrentEntity->sub_state = 0U;
    }
    CERBERUS_FIELD(g_CurrentEntity, player_dist) = (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z));
    CERBERUS_FIELD(g_CurrentEntity, path_result) = (g_ApiCallPtr = api_entity_pathfind_update)();
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    if (CERBERUS_FIELD(g_CurrentEntity, alert_latch) == 0) {
        (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        cerberus_behavior_select();
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
    cerberus_behavior_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void cerberus_behavior_select(void) {
    void cerberus_beh_select_scd();
    extern s32 (*cerberus_behavior_select_tbl[])();
    CERBERUS_FIELD(g_CurrentEntity, alert_latch) = 0;
    g_CurrentEntity->timing_control = 0;
    if (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0x80 & 0xFFFF) {
        g_CurrentEntity->animation_frame_id = 0;
    }
    if (g_CurrentEntity->behavior_flags == 0x80) {
        cerberus_beh_select_scd();
        return;
    }
    cerberus_behavior_select_tbl[g_CurrentEntity->behavior_flags]();
}

void cerberus_beh_select_scd(void) {
    g_CurrentEntity->sub_state = 8;
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->blend_counter = 0xF;
}

void cerberus_beh_select_patrol(void) {
    void cerberus_beh_select_stalk();
    extern struct { u8 b0; u8 b1; u16 h2; } cerberus_walk_params[];

    CERBERUS_FIELD(g_CurrentEntity, beh_flags) = 4;
    if (g_CurrentEntity->behavior_flags == 3) {
        cerberus_beh_select_stalk();
        return;
    }
    g_CurrentEntity->sub_state = 1;
    g_CurrentEntity->animation_id = cerberus_walk_params[(g_CurrentEntity->behavior_flags >> 1) & 0xFF].b0;
    g_CurrentEntity->animation_frame_id = (s8) ((g_ApiCallPtr = api_rand)() & 0x1F);
    g_CurrentEntity->move_speed_current = cerberus_walk_params[(g_CurrentEntity->behavior_flags >> 1) & 0xFF].h2;
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = (s16) (((((u8) g_CurrentEntity->behavior_flags >> 1) & 0xFF) + 1) * 0x10);
    CERBERUS_FIELD(g_CurrentEntity, beh_flags) = (g_CurrentEntity->behavior_flags & 1 & 0xFF);
    CERBERUS_FIELD(g_CurrentEntity, swerve) = (s16) (((g_ApiCallPtr = api_rand)() + 0x1E) & 0xF);
    g_CurrentEntity->blend_counter = 0xF;
}

void cerberus_beh_select_chase(void) {

    if ((u8) g_CurrentEntity->blend_counter >= 8U) {
        g_CurrentEntity->blend_counter = 7U;
    }
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = (((g_ApiCallPtr = api_rand)() & 7) + 0x3D);
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
    CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = 0;
    if (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 1 & 0xFFFF) {
        CERBERUS_FIELD(g_CurrentEntity, swerve) = (s16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0x14);
    }
    g_CurrentEntity->sub_state = 2;
    g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
    g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
    CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) ((CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFF9) | 0x80);
}

void cerberus_beh_select_entrance(void) {

    g_CurrentEntity->death_timer = 0;
    switch ((g_CurrentEntity->behavior_flags)) {
    case 5:
        if (((u8) g_CurrentEntity->animation_id) == 0x10) {
            g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x20);
        } else if (((u8) g_CurrentEntity->animation_id) == 0xF) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x20);
        }
        g_CurrentEntity->animation_id = 6U;
        g_CurrentEntity->move_speed_current = 0x41;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y) = 0x12C;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = -0x28;
        break;
    case 6:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 6);
        g_CurrentEntity->move_speed_current = 0x41;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y) = 0x1E0;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = -0x28;
        g_CurrentEntity->animation_id = 6U;
        CERBERUS_FIELD(g_CurrentEntity, beh_flags) = 5;
        g_CurrentEntity->hit_state = 1;
        break;
    case 7:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 4);
        g_CurrentEntity->animation_id = 7U;
        g_CurrentEntity->death_timer = 0xA;
        g_CurrentEntity->move_speed_current = 0x118;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y) = 0x190;
        CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = -0x28;
        CERBERUS_FIELD(g_CurrentEntity, beh_flags) = 5;
        g_CurrentEntity->hit_state = 1;
        break;
    }
    g_CurrentEntity->sub_state = 3;
    g_CurrentEntity->blend_counter = 3;
    g_CurrentEntity->action_ticks_counter = 8;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, blood_count) = 0;
}

void cerberus_beh_select_bite(void) {
    g_CurrentEntity->sub_state = 7;
    g_CurrentEntity->animation_id = 4;
    g_CurrentEntity->blend_counter = 0;
    g_CurrentEntity->hit_state = 1;
}

void cerberus_beh_select_strafe(void) {

    g_CurrentEntity->sub_state = 4;
    if (CERBERUS_FIELD(g_CurrentEntity, turn_delta) == 0) {
        CERBERUS_FIELD(g_CurrentEntity, turn_delta) = ((g_ApiCallPtr = api_rand)() & 1) ? 0x80 : -0x80;
    }
    g_CurrentEntity->action_ticks_counter = 8;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 0x80);
}

void cerberus_beh_select_alert(void) {
    g_CurrentEntity->sub_state = 5;
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->blend_counter = 0xF;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->action_ticks_counter = 0x14;
    CERBERUS_FIELD(g_CurrentEntity, turn_delta) = 0;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    g_CurrentEntity->hit_state = 1;
}

void cerberus_beh_select_stalk(void) {
    g_CurrentEntity->sub_state = 9;
    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->animation_frame_id = (s8) ((g_ApiCallPtr = api_rand)() & 0x1F);
    g_CurrentEntity->blend_counter = 0;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    g_CurrentEntity->move_speed_current = 0;
    if (!(CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 4 & 0xFFFF)) {
        CERBERUS_FIELD(g_CurrentEntity, beh_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, beh_flags) | 2);
    }
    g_CurrentEntity->hit_state = 0;
}

void cerberus_beh_chase(void) {
    void cerberus_consider_attack();
    extern s32 (*cerberus_chase_tbl[])();
    s32 unused;
    if ((u8) g_CurrentEntity->action_behavior < 2U) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    }
    cerberus_chase_tbl[g_CurrentEntity->action_behavior]();
    cerberus_consider_attack();
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
}

void cerberus_chase_run(void) {
    void cerberus_speed_up(u16 step, s16 limit);
    s32 cerberus_set_turn_anim_a1() __asm__("cerberus_set_turn_anim");
    s8 cerberus_probe_toward_player(s16 speed_mul);
    s16 turn;
    s16 unused;

    cerberus_speed_up(0x14, 0xF0);
    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    if ((CERBERUS_FIELD(g_CurrentEntity, retarget_delay) == 0) && (CERBERUS_FIELD(g_CurrentEntity, swerve) < 0xA) && (g_CurrentEntity->move_speed_current >= 0xF0)) {
        turn = (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, ((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)));
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, ((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)));
    } else {
        if (CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0x11 & 0xFFFF) {
            CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, ((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)) + ((CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 1) ? 0 : 0x20));
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFFE);
        }
        turn = ((u16) CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + turn);
        if (--CERBERUS_FIELD(g_CurrentEntity, swerve) < 0xB) {
            CERBERUS_FIELD(g_CurrentEntity, swerve) = 5;
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFFE);
        }
    }
    if (!(CERBERUS_FIELD(g_CurrentEntity, path_result) & 2)) {
        if (!(CERBERUS_FIELD(g_CurrentEntity, path_result) & 1)) {
            if (++CERBERUS_FIELD(g_CurrentEntity, phase_counter) == 6) {
                g_CurrentEntity->sub_state = 0;
                g_CurrentEntity->behavior_flags = 0xC;
                CERBERUS_FIELD(g_CurrentEntity, beh_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, beh_flags) | 4);
                return;
            }
        } else {
            CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0U;
            g_CurrentEntity->move_speed_current = 0xF0;
            g_CurrentEntity->waypoint_x = (s16) g_PlayerEntity.pos_x;
            g_CurrentEntity->waypoint_z = (s16) g_PlayerEntity.pos_z;
        }
    }
    cerberus_set_turn_anim_a1(turn);
    if (!(CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 2 & 0xFFFF)) {
        if (CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0x11 & 0xFFFF) {
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = (u16) (CERBERUS_FIELD(g_CurrentEntity, aggro_counter) + 2);
            if ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) >= 0x10) {
                g_CurrentEntity->sub_state = 0;
                g_CurrentEntity->behavior_flags = 9;
                CERBERUS_FIELD(g_CurrentEntity, turn_delta) = (s16) (turn << 1);
                CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFF7F);
            }
        } else {
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = (u16) ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) - ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) >= 1));
        }
    } else {
        if ((cerberus_probe_toward_player(4) != 0) && (CERBERUS_FIELD(g_CurrentEntity, retarget_delay) == 0)) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        }
        CERBERUS_FIELD(g_CurrentEntity, retarget_delay) = (s16) (CERBERUS_FIELD(g_CurrentEntity, retarget_delay) - (CERBERUS_FIELD(g_CurrentEntity, retarget_delay) >= 1));
    }
    if (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xC & 0xFFFF) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
    }
}

void cerberus_chase_turn(void) {
    s32 cerberus_wall_follow_step(s16 max_steps, s16 turn_rate);
    void cerberus_speed_up(u16 step, s16 limit);
    s32 cerberus_set_turn_anim_a2() __asm__("cerberus_set_turn_anim");
    s16 dir;

    cerberus_speed_up(0x14, 0xF0);
    dir = cerberus_wall_follow_step(0x5A, 0x40);
    if (dir != 3) {
        cerberus_set_turn_anim_a2((s16) (dir - 1));
    }
    if (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0x10 & 0xFFFF) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->action_behavior = 4;
        CERBERUS_FIELD(g_CurrentEntity, turn_step) = (s16) (((g_ApiCallPtr = api_rand)() & 7) + 0x3D);
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFF7F;
        CERBERUS_FIELD(g_CurrentEntity, retarget_delay) = (s16) ((((g_ApiCallPtr = api_rand)() & 3) * 2) + 8);
    }
}

void cerberus_chase_skid(void) {
    void cerberus_speed_up(u16 step, s16 limit);
    void cerberus_slow_down(u16 step, s16 limit);
    s32 cerberus_set_turn_anim_a3() __asm__("cerberus_set_turn_anim");

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->action_ticks_counter = 0;
    }
    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    CERBERUS_FIELD(g_CurrentEntity, turn_delta) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 0x60);
    (g_ApiCallPtr = api_entity_add_speed_xz)(-((s16) g_CurrentEntity->action_ticks_counter));
    if (g_CurrentEntity->action_state == 1) {
        cerberus_slow_down(0xE, 0xA);
        if (g_CurrentEntity->move_speed_current < 0xB) {
            g_CurrentEntity->action_state++;
        }
    } else {
        cerberus_speed_up(0xE, 0xF0);
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta)));
    g_CurrentEntity->action_ticks_counter = (s16) ((u16) ((s16) g_CurrentEntity->action_ticks_counter) + ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta)));
    cerberus_set_turn_anim_a3((s16) ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta)));
    if (!(__builtin_abs(((s16) g_CurrentEntity->action_ticks_counter)) < 0x800) || ((s16) CERBERUS_FIELD(g_CurrentEntity, turn_delta) == 0)) {
        g_CurrentEntity->action_behavior = 0;
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFF3);
    }
}

void cerberus_beh_leap(void) {
    s32 cerberus_spawn_blood_a1() __asm__("cerberus_spawn_blood");
    extern s32 (*cerberus_leap_tbl[])();
    cerberus_leap_tbl[g_CurrentEntity->action_behavior]();
    cerberus_spawn_blood_a1(g_CurrentEntity, 4, 0);
}

void cerberus_leap_crouch(void) {

    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if ((--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) && (ENTITY_AS(g_CurrentEntity, death_timer, u8) == 0)) {

    } else {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->move_speed_current = 0x118;
        CERBERUS_FIELD(g_CurrentEntity, alert_latch) = 1;
        if (g_CurrentEntity->behavior_flags != 7) {
            (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
        }
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
}

void cerberus_leap_airborne(void) {
    u8 cerberus_bite_player();
    s32 head;
    s32 vel_y;
    s16 reach;

    ENTITY_FIELD(g_CurrentEntity, status_flags) = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_ballistic_step)(g_CurrentEntity->move_speed_current, CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y), CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity), 0);
    vel_y = CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y) + (CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) * (((u8) g_CurrentEntity->death_timer) + 1));
    head = g_CurrentEntity->joints;
    head += 0x1F0;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    reach = (g_PlayerEntity.health >= 0x11) ? 0x3E8 : 0x320;
    if (g_PlayerEntity.collision_flags & 8 & 0xFF) {
        reach = 0x5DC;
    }
    if (CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16) == 0) {
        CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16) = (g_ApiCallPtr = api_joint_check_player_in_range)(head + 0x44, ADDR_sp_vec, reach, ADDR_g_PlayerPosX);
        if ((CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16) != 0) && (((u8) g_PlayerEntity.hit_state) == 0) && (g_CurrentEntity->pos_y >= -0x707) && ((cerberus_bite_player() & 0xFF) == 1)) {
            if (g_CurrentEntity->animation_frame_id != 0) {
                (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
            }
            return;
        }
    }
    if (vel_y <= 0) {
        if (CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16) == 2) {
            g_CurrentEntity->sub_state = 6;
            g_CurrentEntity->action_behavior = 0U;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
            g_PlayerEntity.status_flags |= 2;
        } else {
            g_CurrentEntity->action_behavior++;
            if (((u8) g_CurrentEntity->animation_id) == 6) {
                g_CurrentEntity->animation_id++;
                g_CurrentEntity->timing_control = 0;
                g_CurrentEntity->blend_counter = 0xF;
            }
        }
    }
    if (g_CurrentEntity->animation_frame_id != 0) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
    }
}

void cerberus_leap_land(void) {
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_ballistic_step)(g_CurrentEntity->move_speed_current, CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y), CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity), 0);
    if (g_CurrentEntity->pos_y >= 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags & 0xF9) | 0x20);
        (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
    }
    if (g_CurrentEntity->animation_frame_id != 9) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
    }
}

void cerberus_leap_recover(void) {
    s8 cerberus_probe_both_turns(s16 speed_mul, s8 if_both);
    void cerberus_slow_down(u16 step, s16 limit);

    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    cerberus_slow_down(0x40, 0xF0);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100) != 0) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFB);
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->hit_state = 0;
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFFE);
        if (!(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0x11) || (g_CurrentEntity->behavior_flags == 6) || (g_CurrentEntity->behavior_flags == 7) || (CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 2)) {
            g_CurrentEntity->behavior_flags = 4U;
        } else {
            g_CurrentEntity->behavior_flags = 9U;
            if ((CERBERUS_FIELD(g_CurrentEntity, turn_delta) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, ((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step))) * 2) == 0) {
                CERBERUS_FIELD(g_CurrentEntity, turn_delta) = (s16) (cerberus_probe_both_turns(1, -1) * (((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)) << 1));
            }
        }
    }
}

void cerberus_leap_tumble(void) {
    if ((u8) g_CurrentEntity->animation_frame_id < 0x10U) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
    }
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) < 0x800) {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x80);
        CERBERUS_FIELD(g_CurrentEntity, swerve) = (s16) ((u16) CERBERUS_FIELD(g_CurrentEntity, swerve) + 0x80);
    }
    (g_ApiCallPtr = api_entity_ballistic_step)(0, CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y), CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity), 0);
    if ((g_CurrentEntity->pos_y >= 0) && (CERBERUS_FIELD(g_CurrentEntity, phase_counter) == 0)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
        CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 1;
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100) != 0) {
        g_CurrentEntity->behavior_flags = 4;
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->move_speed_current = 0;
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 1);
    }
}

void cerberus_beh_strafe(void) {
    extern s32 (*cerberus_strafe_tbl[])();
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta)));
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    cerberus_strafe_tbl[g_CurrentEntity->action_behavior]();
    if ((g_ApiCallPtr = api_joint_move)(0, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x200) != 0) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, NULL);
    }
}

void cerberus_strafe_hold(void) {
    void cerberus_consider_attack();
    void cerberus_set_turn_anim_a4() __asm__("cerberus_set_turn_anim");
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) > 0) {
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    } else {
        if (!(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0x11)) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 4;
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFF7F;
            if (!(CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 1)) {
                CERBERUS_FIELD(g_CurrentEntity, swerve) = 0x28;
            }
        } else if ((CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16)++ >= 8) && (CERBERUS_FIELD(g_CurrentEntity, phase_counter) == 0)) {
            g_CurrentEntity->action_behavior++;
            CERBERUS_FIELD_AS(g_CurrentEntity, turn_delta, u16) = -CERBERUS_FIELD_AS(g_CurrentEntity, turn_delta, u16);
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0U;
            return;
        }
        cerberus_consider_attack();
    }
    cerberus_set_turn_anim_a4((s16) CERBERUS_FIELD_AS(g_CurrentEntity, turn_delta, u16));
}

void cerberus_strafe_accel(void) {
    void cerberus_speed_up(u16 step, s16 limit);
    s32 cerberus_set_turn_anim_a5(s16) __asm__("cerberus_set_turn_anim");

    if (CERBERUS_FIELD(g_CurrentEntity, phase_counter) != 0) {
        cerberus_speed_up(0x32, 0x82);
    }
    if (!(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0x11 & 0xFFFF)) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFF7F);
        if (!(CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 1 & 0xFFFF)) {
            CERBERUS_FIELD(g_CurrentEntity, swerve) = 0x28;
        }
    } else if (CERBERUS_FIELD_AS(g_CurrentEntity, aggro_counter, s16)++ >= 8) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->move_speed_current = -0x82;
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0U;
        return;
    }
    cerberus_set_turn_anim_a5(CERBERUS_FIELD(g_CurrentEntity, turn_delta));
}

void cerberus_strafe_spring(void) {
    void cerberus_slow_down(u16 step, s16 limit);
    if ((u8) g_CurrentEntity->animation_frame_id < 0xFU) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 8U) {
            cerberus_slow_down(0x20, 0x28);
        }
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x40);
    } else {
        g_CurrentEntity->move_speed_current = 0;
        CERBERUS_FIELD(g_CurrentEntity, turn_delta) = 0;
    }
    if ((u8) g_CurrentEntity->animation_frame_id >= 0x13U) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
    }
}

void cerberus_beh_alert(void) {
    void cerberus_alert_wait();
    void cerberus_alert_track();
    u8 frame;

    frame = g_CurrentEntity->animation_frame_id;
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    g_CurrentEntity->animation_frame_id = frame;
    if (g_CurrentEntity->action_behavior == 0) {
        cerberus_alert_wait();
        return;
    }
    cerberus_alert_track();
}

void cerberus_alert_wait(void) {
    s32 unused;

    if ((CERBERUS_FIELD(g_CurrentEntity, player_dist) >= 0x1964) && (ENTITY_STATE_WORD(&g_PlayerEntity) != STATE4(1, 3, 0x14, 0)) && (g_CurrentEntity->behavior_flags != 0xB)) {
        return;
    }
    g_CurrentEntity->action_behavior++;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x1F;
    g_CurrentEntity->action_ticks_counter = (s16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0x20);
}

void cerberus_alert_track(void) {
    s8 cerberus_head_track();

    cerberus_head_track();
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->sub_state = 0;
        ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 0xC;
    }
}

void cerberus_beh_maul(void) {
    void cerberus_maul_begin();
    void cerberus_maul_hold();
    if ((u8) g_CurrentEntity->action_behavior >= 2U) {
        return;
    }
    if (g_CurrentEntity->action_behavior == 0) {
        cerberus_maul_begin();
        return;
    }
    cerberus_maul_hold();
}

void cerberus_maul_begin(void) {
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->animation_id = 8;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    CERBERUS_FIELD(g_CurrentEntity, blood_count) = 0;
    g_PlayerEntity.status_flags |= 6;
    g_PlayerEntity.angle = g_CurrentEntity->angle;
    (g_ApiCallPtr = api_snap_player_to_grab_position)(ADDR_g_PlayerEntity);
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
    g_PlayerEntity.hit_state = 1;
    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 2, 0, 0);
    g_CurrentEntity->move_speed_current = 0;
    (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x14, 0x78, 0x78, 0);
    (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x2D, 0xFA, 0x64, 0x28);
}

void cerberus_maul_hold(void) {
    s32 cerberus_spawn_blood_a2() __asm__("cerberus_spawn_blood");
    void *pl_joints;
    void *joint;
    u8 frame;

    g_CurrentEntity->pos_y = 0;
    if ((u8) g_CurrentEntity->animation_frame_id < 0x14U) {
        (g_ApiCallPtr = api_snap_player_to_grab_position)(LITP(void, g_PlayerEntity));
    }
    (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
    if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100) != 0) {
        g_CurrentEntity->action_behavior++;
    }
    frame = g_CurrentEntity->animation_frame_id;
    if (frame == 0x2A) {
        g_PlayerEntity.health = -1;
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xF, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x14, 0xFA, 0x64, 0);
    }
    if (frame == 0x28) {
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)3, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xF, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x1E, 0xFA, 0x64, 0);
    }
    if (frame == 0x5E) {
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)7, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)8, 0);
    }
    if (frame == 0x66) {
        CERBERUS_FIELD(g_CurrentEntity, blood_count) = 6;
        pl_joints = ((void *) g_PlayerEntity.joints);
        joint = pl_joints;
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
        joint = JOINT_N(pl_joints, 1);
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
        joint = JOINT_N(pl_joints, 9);
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
        joint = JOINT_N(pl_joints, 12);
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
        joint = ((void *) g_CurrentEntity->joints);
        joint += 0x1F0;
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
        (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, 3, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)8, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)8, 0x64, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x3C, 0xFA, 0, 8);
    }
    cerberus_spawn_blood_a2(LITP(void, g_PlayerEntity), 1, 3);
    cerberus_spawn_blood_a2(g_CurrentEntity, 3, 0);
}

void cerberus_beh_bite(void) {
    void cerberus_bite_wait();
    void cerberus_bite_track();
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
    if (g_CurrentEntity->action_behavior == 0) {
        cerberus_bite_wait();
        return;
    }
    cerberus_bite_track();
}

void cerberus_bite_wait(void) {
    if ((CERBERUS_FIELD(g_CurrentEntity, player_dist) >= 0x1195) && (ENTITY_STATE_WORD(&g_PlayerEntity) != STATE4(1, 3, 0x14, 0))) {
        return;
    }
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->animation_id++;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0xF;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
}

void cerberus_bite_track(void) {
    s8 cerberus_head_track();
    cerberus_head_track();
    if (g_CurrentEntity->animation_frame_id == 0x21) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        g_CurrentEntity->hit_state = 0;
    }
}

void cerberus_beh_scd(void) {
    g_CurrentEntity->sub_state = 0;
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
}

void cerberus_beh_patrol(void) {
    extern s32 (*cerberus_patrol_tbl[])();
    if ((CERBERUS_FIELD(g_CurrentEntity, player_dist) >= 0x1194) && (ENTITY_STATE_WORD(&g_PlayerEntity) != STATE4(1, 3, 0x14, 0))) {

    } else {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        g_CurrentEntity->blend_counter = 7;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(8);
    cerberus_patrol_tbl[g_CurrentEntity->action_behavior]();
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
    if ((g_CurrentEntity->animation_frame_id != 0) && (g_CurrentEntity->animation_frame_id != 0x1B)) {
        return;
    }
    (g_ApiCallPtr = api_sound_play_entity_se)(9, 0);
}

void cerberus_patrol_begin(void) {
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->pos_y = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0xF;
    CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
}

void cerberus_patrol_pick_turn(void) {
    s8 cerberus_probe_both_turns(s16 speed_mul, s8 if_both);
    s16 dir;

    if ((CERBERUS_FIELD(g_CurrentEntity, probe_history) & 1) || ((CERBERUS_FIELD(g_CurrentEntity, beh_flags) == 0) && (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0))) {
        g_CurrentEntity->action_behavior++;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0x400;
        if ((dir = cerberus_probe_both_turns(3, 0)) == 0) {
            if (CERBERUS_FIELD(g_CurrentEntity, beh_flags) == 0) {
                CERBERUS_FIELD(g_CurrentEntity, turn_delta) = ((g_ApiCallPtr = api_rand)() & 1) ? CERBERUS_FIELD(g_CurrentEntity, turn_step) : -CERBERUS_FIELD(g_CurrentEntity, turn_step);
            } else {
                CERBERUS_FIELD(g_CurrentEntity, turn_delta) = CERBERUS_FIELD(g_CurrentEntity, turn_step);
            }
        } else {
            CERBERUS_FIELD(g_CurrentEntity, turn_delta) = (u16) dir * CERBERUS_FIELD(g_CurrentEntity, turn_step);
            CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
        }
        if (CERBERUS_FIELD(g_CurrentEntity, beh_flags) == 0) {
            if ((u32) (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) < 0U) {
                ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) >> 2;
            }
        }
    } else {
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    }
}

void cerberus_patrol_turn(void) {

    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta)));
    g_CurrentEntity->action_ticks_counter = (u16) (g_CurrentEntity->action_ticks_counter - __builtin_abs((s16) FIELD_AT((u8 *)g_CurrentEntity + 0x16C, CerberusWork, ->turn_delta, u16)));
    if ((s16) g_CurrentEntity->action_ticks_counter < 0) {
        g_CurrentEntity->action_behavior = 0;
        if (CERBERUS_FIELD(g_CurrentEntity, beh_flags) == 0) {
            g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x4B);
        }
    }
}

void cerberus_beh_stalk(void) {
    void cerberus_speed_up(u16 step, s16 limit);
    s8 cerberus_head_track();
    s8 cerberus_probe_toward_player(s16 speed_mul);
    extern s32 (*cerberus_stalk_tbl[])();
    cerberus_speed_up(2, 0x28);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    cerberus_stalk_tbl[g_CurrentEntity->action_behavior]();
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
    if ((g_CurrentEntity->animation_frame_id != 0) && (g_CurrentEntity->animation_frame_id != 0x1B)) {

    } else {
        (g_ApiCallPtr = api_sound_play_entity_se)(9, 0);
    }
    cerberus_head_track();
    if ((cerberus_probe_toward_player(8) != 0) && (g_CurrentEntity->action_behavior != 3)) {
        g_CurrentEntity->action_behavior = 3U;
        g_CurrentEntity->action_state = 0;
    }
    if (CERBERUS_FIELD(g_CurrentEntity, player_dist) < 0x9C4) {
        CERBERUS_FIELD(g_CurrentEntity, aggro_counter)++;
        if (CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 4 & 0xFFFF) {
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 0x41U;
        }
        if (g_PlayerEntity.move_speed_current >= 0xC9) {
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = (u16) (CERBERUS_FIELD(g_CurrentEntity, aggro_counter) + 0x14);
        }
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) && (CERBERUS_FIELD(g_CurrentEntity, player_dist) >= 0x321)) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->behavior_flags = 5;
            g_CerberusBarked = 1;
        }
    } else {
        CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = (u16) ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) - ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) >= 1));
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) && (g_PlayerEntity.move_speed_current >= 0xC9)) {
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = (u16) (CERBERUS_FIELD(g_CurrentEntity, aggro_counter) + 6);
        }
    }
    if (!(((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter) < 0x41) && (ENTITY_STATE_WORD(&g_PlayerEntity) != STATE4(1, 3, 0x14, 0))) || ((g_CerberusBarked != 0) && !(CERBERUS_FIELD(g_CurrentEntity, beh_flags) & 4 & 0xFFFF))) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
        if (g_CerberusBarked == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
            g_CerberusBarked = 1;
        }
    }
}

void cerberus_stalk_begin(void) {
    g_CurrentEntity->action_behavior++;
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = ((g_ApiCallPtr = api_rand)() & 7) + 0x10;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) < 0) {
        CERBERUS_FIELD(g_CurrentEntity, turn_step) = -CERBERUS_FIELD(g_CurrentEntity, turn_step);
    }
    g_CurrentEntity->action_ticks_counter = (s16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x2D);
}

void cerberus_stalk_arc(void) {

    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + CERBERUS_FIELD(g_CurrentEntity, turn_step));
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->action_ticks_counter = ((g_ApiCallPtr = api_rand)() & 7) - (u16)(((g_ApiCallPtr = api_rand)() & 0xF) - 30);
    }
}

void cerberus_stalk_reface(void) {

    if ((--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) && !(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 1 & 0xFFFF)) {
        return;
    }
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (((g_ApiCallPtr = api_rand)() & 7) + 0x10));
    g_CurrentEntity->action_ticks_counter = 2U;
    ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 1;
    g_CurrentEntity->action_ticks_counter = (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x2D);
    if ((u32) ((g_ApiCallPtr = api_rand)() & 0x3F) >= 0x31U) {
        (g_ApiCallPtr = api_sound_play_entity_se)(2U, 0);
    }
}

void cerberus_stalk_wallfollow(void) {
    s32 cerberus_wall_follow_step(s16 max_steps, s16 turn_rate);
    cerberus_wall_follow_step(0x258, 0x10);
    if (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0x10 & 0xFFFF) {
        g_CurrentEntity->action_behavior = 0;
    }
}

s32 cerberus_wall_follow_step(s16 max_steps, s16 turn_rate) {
    s8 cerberus_probe_toward_player(s16 speed_mul);
    extern s32 (*cerberus_wall_follow_tbl[])();
    s16 turn;
    turn = cerberus_wall_follow_tbl[g_CurrentEntity->action_state](turn_rate);
    if ((cerberus_probe_toward_player(4) == 0) && (CERBERUS_FIELD(g_CurrentEntity, phase_counter) >= 0xD)) {
        CERBERUS_FIELD(g_CurrentEntity, turn_delta) = 0;
        CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 0x10);
    }
    if (++CERBERUS_FIELD(g_CurrentEntity, phase_counter) > max_steps) {
        g_CurrentEntity->action_state = 0U;
    }
    return ((turn != 0) ? (turn > 0) ? 2 : 0 : 1);
}

s16 cerberus_wall_step_pick(s16 turn_rate) {
    s16 cerberus_wall_step_wait(s16);
    g_CurrentEntity->action_state++;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    if ((CERBERUS_FIELD_AS(g_CurrentEntity, turn_step, s16) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, turn_rate)) == 0) {
        CERBERUS_FIELD_AS(g_CurrentEntity, turn_step, s16) = (s16) (u16) turn_rate;
    }
    CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFEF);
    return cerberus_wall_step_wait(turn_rate);
}

s16 cerberus_wall_step_wait(s16 turn_rate) {
    u16 turn;
    turn = 0;
    if (CERBERUS_FIELD(g_CurrentEntity, probe_history) & 1 & 0xFFFF) {
        g_CurrentEntity->action_state++;
        turn = CERBERUS_FIELD(g_CurrentEntity, turn_step);
    }
    if (CERBERUS_FIELD(g_CurrentEntity, phase_counter) >= 9) {
        CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 8;
    }
    return (s16) turn;
}

s16 cerberus_wall_step_slide(s16 turn_rate) {
    s8 cerberus_probe_both_turns(s16 speed_mul, s8 if_both);
    u16 turn;
    turn = 0;
    if (!(CERBERUS_FIELD(g_CurrentEntity, probe_history) & 0xF & 0xFFFF)) {
        g_CurrentEntity->action_state++;
        g_CurrentEntity->action_ticks_counter = 0;
        if ((CERBERUS_FIELD(g_CurrentEntity, turn_delta) = cerberus_probe_both_turns(2, 1)) == 0) {
            CERBERUS_FIELD(g_CurrentEntity, turn_delta) = -1;
        }
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + CERBERUS_FIELD(g_CurrentEntity, turn_step));
    turn = CERBERUS_FIELD(g_CurrentEntity, turn_step);
    if (CERBERUS_FIELD(g_CurrentEntity, phase_counter) >= 9) {
        CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 8;
    }
    return (s16) turn;
}

s32 cerberus_wall_step_commit(s16 turn_rate) {
    s8 cerberus_probe_turn_a1() __asm__("cerberus_probe_turn");
    if (((cerberus_probe_turn_a1((s16) (CERBERUS_FIELD(g_CurrentEntity, turn_delta) * 0x400), 1) == 0) && (((s16) g_CurrentEntity->action_ticks_counter) == 0)) || (CERBERUS_FIELD(g_CurrentEntity, probe_history) & 1)) {
        g_CurrentEntity->action_state = 4;
        CERBERUS_FIELD(g_CurrentEntity, turn_step) = ((u16) CERBERUS_FIELD(g_CurrentEntity, turn_delta));
        g_CurrentEntity->action_ticks_counter = 0x400;
        return 0;
    }
    g_CurrentEntity->action_ticks_counter = ((s16) g_CurrentEntity->action_ticks_counter) - (((s16) g_CurrentEntity->action_ticks_counter) >= 1);
    return 0;
}

s16 cerberus_wall_step_run(u16 turn_rate) {
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (CERBERUS_FIELD(g_CurrentEntity, turn_step) * turn_rate));
    g_CurrentEntity->action_ticks_counter = (u16) (g_CurrentEntity->action_ticks_counter - turn_rate);
    if ((s16) g_CurrentEntity->action_ticks_counter <= 0) {
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->action_ticks_counter = 4U;
    }
    return (s16) CERBERUS_FIELD(g_CurrentEntity, turn_step);
}

void cerberus_damaged(void) {
    extern s32 (*cerberus_damaged_tbl[])();
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if (!(CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0x20 & 0xFFFF)) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    }
    (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
    cerberus_damaged_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void cerberus_damaged_begin(void) {
    u8 beh;

    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 3;
    CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 0x20);
    beh = g_CurrentEntity->behavior_flags;
    if (!((beh >= 4U) && (beh < 8U)) && (beh != 9) && (((u8) g_CurrentEntity->hit_state) & 1)) {
        g_CurrentEntity->sub_state = 1;
        g_CurrentEntity->animation_id = 9;
    } else {
        CERBERUS_FIELD(g_CurrentEntity, turn_step) = (u16) ((g_PlayerEntity.angle & 0xFFF) - ((g_CurrentEntity->angle) & 0xFFF));
        CERBERUS_FIELD(g_CurrentEntity, turn_step) = (u16) (CERBERUS_FIELD(g_CurrentEntity, turn_step) + *(s32 *)((u8 *)&cerberus_recoil_angle_tbl + ((((g_ApiCallPtr = api_rand)() & 7) * 2))));
        if ((beh == 4) || ((s8) (CERBERUS_FIELD(g_CurrentEntity, alert_latch) & 0x7F) == 0)) {
            g_CurrentEntity->sub_state = 3;
            g_CurrentEntity->animation_id = 0xC;
            g_CurrentEntity->blend_counter = 0;
            g_CurrentEntity->move_speed_current = 0xF0;
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        } else {
            g_CurrentEntity->sub_state = 2;
            g_CurrentEntity->animation_id = 0xB;
            g_CurrentEntity->move_speed_current = (s16) ((s16) g_CurrentEntity->move_speed_current >> 1);
            CERBERUS_FIELD(g_CurrentEntity, alert_latch) = (u8) (CERBERUS_FIELD(g_CurrentEntity, alert_latch) | ~0x7F);
        }
    }
    g_CurrentEntity->action_behavior = 0;
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    (g_ApiCallPtr = api_sound_play_entity_se)(8, 0U);
}

void cerberus_damaged_stumble(void) {
    void cerberus_stumble_end();
    void cerberus_stumble_getup();
    CERBERUS_FIELD(g_CurrentEntity, swerve) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    if (g_CurrentEntity->action_behavior == 0) {
        cerberus_stumble_end();
        return;
    }
    cerberus_stumble_getup();
}

void cerberus_stumble_end(void) {
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->move_speed_current = 0x82;
    }
}

void cerberus_stumble_getup(void) {
    void cerberus_slow_down(u16 step, s16 limit);
    if ((u8) g_CurrentEntity->animation_frame_id < 0xFU) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 8U) {
            cerberus_slow_down(0x20, 0x28);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x40);
    }
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) != 0) {
        g_CurrentEntity->state = 1;
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        g_CurrentEntity->hit_state = 0;
    }
}

void cerberus_knock_airborne(void) {
    (g_ApiCallPtr = api_entity_add_speed_xz)(((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)));
    (g_ApiCallPtr = api_entity_ballistic_step)(0, CERBERUS_FIELD(g_CurrentEntity, ballistic_vel_y), CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity), 0);
    if (g_CurrentEntity->pos_y >= 0) {
        g_CurrentEntity->sub_state++;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->pos_y = 0;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
    }
    if (g_CurrentEntity->animation_frame_id != 0) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    }
}

void cerberus_damaged_skid(void) {
    void cerberus_slow_down(u16 step, s16 limit);
    if (((u8) g_CurrentEntity->animation_id) == 0xC) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)));
        cerberus_slow_down(0xA, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->hit_state = 0;
        if (((u8) g_CurrentEntity->animation_id) == 0xE) {
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFDF);
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                g_CurrentEntity->sub_state++;
                g_CurrentEntity->animation_id = 3U;
            } else {
                g_CurrentEntity->sub_state = 1U;
                g_CurrentEntity->action_behavior = 1;
                g_CurrentEntity->animation_id = 0xAU;
                g_CurrentEntity->move_speed_current = 0x82;
            }
        }
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
    }
}

void cerberus_damaged_getup(void) {
    if ((g_CurrentEntity->animation_frame_id == 0xA) && (((u8) g_CurrentEntity->timing_control) == 1)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        g_CurrentEntity->state = 1;
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 4;
        g_CurrentEntity->hit_state = 0;
        CERBERUS_FIELD(g_CurrentEntity, alert_latch) = (u8) (((u8) CERBERUS_FIELD(g_CurrentEntity, alert_latch)) & 0x7F);
        (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
    }
}

void cerberus_die(void) {
    extern s32 (*cerberus_die_tbl[])();
    if ((u8) ((u8) g_CurrentEntity->sub_state) >= 4U) {
        return;
    }
    cerberus_die_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void cerberus_die_begin(void) {
    (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->action_behavior = 0;
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = (s16) ((g_PlayerEntity.angle & 0xFFF) - (g_CurrentEntity->angle & 0xFFF));
    if ((g_CurrentEntity->behavior_flags == 4) || ((s8) (CERBERUS_FIELD(g_CurrentEntity, alert_latch) & 0x7F) == 0)) {
        g_CurrentEntity->sub_state = 2;
        g_CurrentEntity->animation_id = 0xC;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->move_speed_current = 0xF0;
    } else {
        g_CurrentEntity->sub_state = 1;
        g_CurrentEntity->animation_id = 0xB;
        g_CurrentEntity->move_speed_current = (s16) ((s16) g_CurrentEntity->move_speed_current >> 1);
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    (g_ApiCallPtr = api_sound_play_entity_se)(4, 0U);
}

void cerberus_die_slide(void) {
    void cerberus_slow_down(u16 step, s16 limit);
    if (((u8) g_CurrentEntity->animation_id) == 0xC) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(((s16) CERBERUS_FIELD(g_CurrentEntity, turn_step)));
        cerberus_slow_down(0xA, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->timing_control = 0;
    }
    if (((u8) g_CurrentEntity->animation_id) == 0xD) {
        g_CurrentEntity->sub_state++;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_ticks_counter = 0x46;
        (g_ApiCallPtr = api_billboard_set_color)((&g_CurrentEntity->shadow), 1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)((&g_CurrentEntity->shadow), -0x5A, -0x5A);
    }
}

void cerberus_die_shadow(void) {
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 6, 6);
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
        return;
    }
    ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
}

void cerberus_state4_nop(void) {

    return;
}

void cerberus_behavior0_nop(void) {

}

void player_anim_simple_recovery(void) {
    void player_anim_simple_recovery_begin();
    void player_anim_simple_recovery_run();
    if ((u8) g_PlayerEntity.action_state >= 2U) {
        return;
    }
    if (g_PlayerEntity.action_state == 0) {
        player_anim_simple_recovery_begin();
        return;
    }
    player_anim_simple_recovery_run();
}

void player_anim_simple_recovery_begin(void) {
    s32 unused;
    g_PlayerEntity.action_state++;
    g_PlayerEntity.animation_id = 2;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.blend_counter = 0;
    g_PlayerEntity.move_speed_current = 0;
    (g_ApiCallPtr = api_sound_play_se_3d)(3, 2, 0, ADDR_g_PlayerPosX);
}

void player_anim_simple_recovery_run(void) {
    if (g_PlayerEntity.animation_frame_id == 0xF) {
        (g_ApiCallPtr = api_sound_play_footstep)(2, 0);
    }
    if (g_PlayerEntity.health >= 0) {
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x200) != 0) {
        g_PlayerEntity.angle += 0x800;
        g_PlayerEntity.action_state++;
    }
}

u8 cerberus_bite_player(void) {
    u8 facing;

    g_CurrentEntity->hit_state = 0;
    if (g_PlayerEntity.health < 0x11) {
        if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) {
            g_PlayerEntity.status_flags |= 2;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
            g_CurrentEntity->move_speed_current = 0;
            CERBERUS_FIELD(g_CurrentEntity, aggro_counter) = 2;
            g_PlayerEntity.angle = (g_CurrentEntity->angle + 0x800) & 0xFFF;
            g_CurrentEntity->hit_state = 1;
            g_PlayerEntity.hit_state = 1;
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 2, 0, 2);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFB);
            return (u8) ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter));
        }
    }
    g_PlayerEntity.action_state = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
    g_CurrentEntity->anim_base_x = (u16) g_PlayerEntity.pos_x;
    g_CurrentEntity->anim_base_z = (u16) g_PlayerEntity.pos_z;
    if (g_CurrentEntity->behavior_flags != 6) {
        g_CurrentEntity->action_behavior = 4;
        g_CurrentEntity->animation_id = 0x11;
        g_CurrentEntity->animation_frame_id = 1;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0xF;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFB);
        g_CurrentEntity->move_speed_current = (s16) ((s16) g_CurrentEntity->move_speed_current >> 2);
    }
    g_PlayerEntity.anim_base_x = g_PlayerEntity.pos_x;
    g_PlayerEntity.anim_base_z = g_PlayerEntity.pos_z;
    facing = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
    g_PlayerEntity.hit_state = facing + 1;
    g_PlayerEntity.action_behavior = facing + 0x66;
    (g_ApiCallPtr = api_pad_vibrate_small)(5, 0);
    (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xC8, 0);
    g_PlayerEntity.health = g_PlayerEntity.health - 0x10;
    CERBERUS_FIELD(g_CurrentEntity, turn_step) = 0x200;
    CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
    CERBERUS_FIELD(g_CurrentEntity, blood_count) = 3;
    CERBERUS_FIELD(g_CurrentEntity, phase_counter) = 0;
    (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
    return (u8) ((s16) CERBERUS_FIELD(g_CurrentEntity, aggro_counter));
}

void cerberus_consider_attack(void) {
    s32 unused;
    u16 hp;
    s16 chance;

    hp = ((u16) g_PlayerEntity.health);
    if ((CERBERUS_FIELD(g_CurrentEntity, player_dist) < 0x1388) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) {
            chance = (g_PlayerEntity.collision_flags & 8 & 0xFF) ? 0x64 : 0x7E;
            if ((s16) hp < 0x11) {
                if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) {
                    if ((u32) ((g_ApiCallPtr = api_rand)() & 0x7F) < (u32) chance) {
                        g_CurrentEntity->sub_state = 0;
                        g_CurrentEntity->behavior_flags = 9;
                        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
                        return;
                    }
                }
            }
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->behavior_flags = 5;
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 0x80);
            CERBERUS_FIELD(g_CurrentEntity, swerve) = 0;
            return;
        }
        if (CERBERUS_FIELD(g_CurrentEntity, swerve) < 0xA) {
            if (++CERBERUS_FIELD(g_CurrentEntity, swerve) >= 0xA) {
                CERBERUS_FIELD(g_CurrentEntity, swerve) += (s16) (((g_ApiCallPtr = api_rand)() & 7) + 0x1E);
                CERBERUS_FIELD(g_CurrentEntity, ballistic_gravity) = 0;
            }
        }
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) == 0) {
            CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) (CERBERUS_FIELD(g_CurrentEntity, ai_flags) | 2);
            return;
        }
        if ((CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 3 & 0xFFFF) == 2) {
            if ((u32) ((g_ApiCallPtr = api_rand)() & 0x40) >= 0x39U) {
                CERBERUS_FIELD(g_CurrentEntity, ai_flags) = (u16) ((CERBERUS_FIELD(g_CurrentEntity, ai_flags) & 0xFFFD) | 5);
            }
        }
    }
}

void cerberus_spawn_blood(void *ent, u8 joint_idx, u8 fx_type) {
    u8 *joint;

    if (CERBERUS_FIELD(g_CurrentEntity, blood_count) <= 0) {
        return;
    }
    joint = ((u8 *) ENT(ent)->joints);
    joint = joint + (joint_idx * 0x7C);
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    (g_ApiCallPtr = api_effect_create_billboard)(0, fx_type, 0, g_DeadMoveValue, JOINT_N_POS(joint, 0), 0);
    CERBERUS_FIELD(g_CurrentEntity, blood_count)--;
}

s32 cerberus_probe_ahead(void) {
    VECTOR ofs;
    s8 hit;

    ofs = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    ofs.vx = cerberus_probe_dist;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, LITP(s32, sp_matrix));
    (g_ApiCallPtr = api_ApplyMatrixLV)(LITP(u16, sp_matrix), &ofs, &ofs);
    g_CurrentEntity->pos_x = g_CurrentEntity->pos_x + ofs.vx;
    g_CurrentEntity->pos_z = g_CurrentEntity->pos_z + ofs.vz;
    hit = (g_ApiCallPtr = api_check_room_collision)((u16 *) &g_CurrentEntity->pos_x, (s32 *)0x190);
    g_CurrentEntity->pos_x = g_CurrentEntity->pos_x - ofs.vx;
    g_CurrentEntity->pos_z = g_CurrentEntity->pos_z - ofs.vz;
    return hit >= 1;
}

s8 cerberus_probe_turn(s16 angle_ofs, u16 speed_mul) {
    s32 old_x;
    s32 old_z;
    s32 old_vx;
    s32 old_vz;
    u16 old_speed;
    s8 blocked;

    old_x = g_CurrentEntity->pos_x;
    old_z = g_CurrentEntity->pos_z;
    old_vx = ((s16) g_CurrentEntity->position_vx);
    old_vz = ((s16) g_CurrentEntity->position_vz);
    old_speed = ((u16) g_CurrentEntity->move_speed_current);
    g_CurrentEntity->move_speed_current = old_speed * speed_mul;
    (g_ApiCallPtr = api_entity_add_speed_xz)(angle_ofs);
    blocked = cerberus_probe_ahead();
    g_CurrentEntity->pos_x = old_x;
    g_CurrentEntity->pos_z = old_z;
    g_CurrentEntity->position_vx = old_vx;
    g_CurrentEntity->position_vz = old_vz;
    g_CurrentEntity->move_speed_current = old_speed;
    return blocked;
}

s8 cerberus_probe_both_turns(s16 speed_mul, s8 if_both) {
    s8 cerberus_probe_turn_a2() __asm__("cerberus_probe_turn");
    s8 blocked_pos;
    s8 blocked_neg;
    blocked_pos = cerberus_probe_turn_a2(0x400, speed_mul);
    blocked_neg = -cerberus_probe_turn_a2(-1024, speed_mul);
    if ((blocked_pos != 0) && (blocked_neg != 0)) {
        return if_both;
    }
    return (s8) ((u8) blocked_pos + (u8) blocked_neg);
}

void cerberus_speed_up(u16 step, s16 limit) {
    if (g_CurrentEntity->move_speed_current < limit) {
        g_CurrentEntity->move_speed_current = (s16) ((u16) g_CurrentEntity->move_speed_current + step);
        if (g_CurrentEntity->move_speed_current > limit) {
            g_CurrentEntity->move_speed_current = (s16) (u16) limit;
        }
    }
}

void cerberus_slow_down(u16 step, s16 limit) {
    if (g_CurrentEntity->move_speed_current > limit) {
        g_CurrentEntity->move_speed_current = (s16) ((u16) g_CurrentEntity->move_speed_current - step);
        if (g_CurrentEntity->move_speed_current < limit) {
            g_CurrentEntity->move_speed_current = (s16) (u16) limit;
        }
    }
}

void cerberus_set_turn_anim(s16 turn) {
    u8 anim;

    anim = cerberus_turn_anim_tbl[(turn != 0) ? ((turn > 0) ? 2 : 0) : 1];
    if (((u8) g_CurrentEntity->animation_id) != anim) {
        g_CurrentEntity->animation_id = anim;
        g_CurrentEntity->timing_control = 0;
        if (((u8) g_CurrentEntity->blend_counter) < 4U) {
            g_CurrentEntity->blend_counter = 7U;
        }
    }
}

void *cerberus_rotate_spine(u16 rx, u16 ry, u16 rz) {
    Joint *joint;
    joint = ((void *) g_CurrentEntity->joints);
    joint += 1;
    joint->rotation.vx = (s16) (((u16) joint->rotation.vx) + rx);
    joint->rotation.vy = (s16) (((u16) joint->rotation.vy) + ry);
    joint->rotation.vz = (s16) (((u16) joint->rotation.vz) + rz);
    (g_ApiCallPtr = api_RotMatrix)(&joint->rotation, &joint->transform);
    joint++;
    joint->rotation.vx = (s16) (((u16) joint->rotation.vx) + (rx + ((s16) rx >> 2)));
    joint->rotation.vy = (s16) (((u16) joint->rotation.vy) + (ry + ((s16) ry >> 2)));
    joint->rotation.vz = (s16) (((u16) joint->rotation.vz) + (rz + ((s16) rz >> 2)));
    (g_ApiCallPtr = api_RotMatrix)(&joint->rotation, &joint->transform);
    joint++;
    joint->rotation.vx = (s16) (((u16) joint->rotation.vx) + (rx + ((s16) rx >> 1)));
    joint->rotation.vy = (s16) (((u16) joint->rotation.vy) + (ry + ((s16) ry >> 1)));
    joint->rotation.vz = (s16) (((u16) joint->rotation.vz) + (rz + ((s16) rz >> 1)));
    (g_ApiCallPtr = api_RotMatrix)(&joint->rotation, &joint->transform);
    return joint;
}

s8 cerberus_head_track(void) {
    void *cerberus_rotate_spine_p(s32, s32, s32) __asm__("cerberus_rotate_spine");
    void *head;
    s32 unused;
    VECTOR saved_pos;
    u16 saved_angle;
    s16 clamped;

    clamped = 0;
    head = cerberus_rotate_spine_p(0, CERBERUS_FIELD(g_CurrentEntity, swerve), 0);
    saved_pos.vx = g_CurrentEntity->pos_x;
    saved_pos.vz = g_CurrentEntity->pos_z;
    saved_angle = g_CurrentEntity->angle;
    g_CurrentEntity->pos_x = (s32) (JOINT(head)->transform.t[0] + JOINT(head)->world.t[0]);
    g_CurrentEntity->pos_z = (s32) (JOINT(head)->transform.t[2] + JOINT(head)->world.t[2]);
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) CERBERUS_FIELD(g_CurrentEntity, swerve) + (CERBERUS_FIELD(g_CurrentEntity, swerve) >> 1)));
    CERBERUS_FIELD(g_CurrentEntity, swerve) = (s16) ((u16) CERBERUS_FIELD(g_CurrentEntity, swerve) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 8));
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) < -0x100) {
        CERBERUS_FIELD(g_CurrentEntity, swerve) = -0x100;
        clamped = 1;
    }
    if (CERBERUS_FIELD(g_CurrentEntity, swerve) >= 0x101) {
        CERBERUS_FIELD(g_CurrentEntity, swerve) = 0x100;
        clamped = 1;
    }
    g_CurrentEntity->pos_x = saved_pos.vx;
    g_CurrentEntity->pos_z = saved_pos.vz;
    g_CurrentEntity->angle = saved_angle;
    return (s8) clamped;
}

s8 cerberus_probe_toward_player(s16 speed_mul) {
    s8 cerberus_probe_turn_a3(s16, s16) __asm__("cerberus_probe_turn");
    u16 to_player;
    to_player = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
    return cerberus_probe_turn_a3((s16) (to_player - g_CurrentEntity->angle), speed_mul);
}
