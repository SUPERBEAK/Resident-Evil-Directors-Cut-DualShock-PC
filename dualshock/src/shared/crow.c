/* crow.c - Crow: perching, flying (wingbeat, dive, bank, ascend / descend), pecking and grabbing the player,
   script-driven flight, and the player animation for a crow attack.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/crow.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* crow data (defined in each overlay that has this enemy) */
extern s32 crow_sca_info_ptr;

/* CrowWork: the crow's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). CROW(e)->f, CROW_FIELD(e, f) and CROW_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct CrowWork {
    /* +0x00 */ s16 bank_turn_rate;  /* 0x16C: turn step for the banking turn (0x80 +-0x20/0x40 random, crow_beh_bank_turn); used by crow_anim_turn */
    /* +0x02 */ s16 path_flags;      /* 0x16E: bit 0 = api_entity_pathfind_update result (crow_behavior_dispatch); gates the turn in descend/ascend/peck */
    /* +0x04 */ s16 player_touch;    /* 0x170: api_check_entity_collision result vs the player (crow_update); non-zero starts the attack / recoil check */
    /* +0x06 */ s16 climb_limit_y;   /* 0x172: altitude the ascend climbs to (-(rand*1000+4500) + altitude_bias); passing it ends crow_beh_ascend */
    /* +0x08 */ s16 descend_limit_y; /* 0x174: altitude the descend sinks to (altitude_bias*2 - (rand*1000+1500)); crow_beh_dive reuses it for its drop step */
    /* +0x0A */ s16 vert_speed;      /* 0x176: vertical speed added to/subtracted from pos_y (200..500 ramp, reset to 200 at the floor/ceiling clamp) */
    /* +0x0C */ u16 player_dist;     /* 0x178: |dx|+|dz| to the player (crow_state_run); sets flight speed (dist/100+200) and range checks */
    /* +0x0E */ u16 room_hit;        /* 0x17A: api_check_room_collision result (crow_update); non-zero counts wall_hit_frames and drives the swerve */
    /* +0x10 */ u16 wall_hit_frames; /* 0x17C: frames spent against room geometry; >=6 turns towards player, >=0x1F swerves and resets */
    /* +0x12 */ s16 unused_17E;      /* 0x17E: set to 100 in crow_state_init, never read */
    /* +0x14 */ u8 pad14[2];
    /* +0x16 */ s16 swerve_angle;    /* 0x182: only passed by address (with 0x184) to api_entity_swerve_around_obstacle (crow_state_run), which keeps its swerve angle here */
    /* +0x18 */ s8 swerve_latch;     /* 0x184: cleared in init; passed by address (with 0x182) to api_entity_swerve_around_obstacle */
    /* +0x19 */ u8 pad19[1];
    /* +0x1A */ s8 grab_struggle;    /* 0x186: grab struggle counter (100, minus 9 per button press / 1 per frame); <0 frees the player */
    /* +0x1B */ u8 pad1B[3];
    /* +0x1E */ s16 altitude_bias;   /* 0x18A: altitude offset for the flight limits: 0, or -400 when behavior_flags bit 0 */
} CrowWork;
#define CROW(e) ENEMY_WORK(e, CrowWork)
#define CROW_FIELD(e, f) ENEMY_FIELD(e, CrowWork, f)
#define CROW_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, CrowWork, f, T)
#define CROW_MEMBER(e, f) ENEMY_MEMBER(e, CrowWork, f)

/* state 0: sets up a new crow (health, sca info, animation, work fields), then state 1 */
void crow_state_init(void) {

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->animation_id = 2;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->collision_flags |= 4;
    if (g_CurrentEntity->behavior_flags & 0x10 & 0xFF) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8;
        g_CurrentEntity->move_speed_current = ((((g_ApiCallPtr = api_rand)() & 1) << 4) + 0x32);
        g_CurrentEntity->angle_z = 0x100;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
        g_CurrentEntity->animation_id = 0;
    }
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0, 0);
        g_CurrentEntity->animation_id = 0;
    }
    CROW_FIELD(g_CurrentEntity, wall_hit_frames) = 0;
    CROW_FIELD(g_CurrentEntity, unused_17E) = 0x64;
    CROW_FIELD(g_CurrentEntity, swerve_latch) = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400);
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0xC8, 0xC8);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = ((((g_ApiCallPtr = api_rand)() & 7) + 5) + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7)) * 2;
    CROW_FIELD(g_CurrentEntity, altitude_bias) = 0;
    if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
        g_CurrentEntity->health = 0;
        CROW_FIELD(g_CurrentEntity, altitude_bias) = -0x190;
    }
    g_CurrentEntity->sca_info = (s32) crow_sca_info_ptr;
    g_CurrentEntity->scale = 0x14CC;
}

/* every frame for each crow (enemies_update_functions_tbl): runs crow_state_tbl[state], then collision checks and
   the shadow / drawing */
void crow_update(void) {
    extern s32 (*crow_state_tbl[])();

    if (g_UpdateFlags & 4 & 0xFFFF) {
        crow_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        CROW_FIELD(g_CurrentEntity, player_touch) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
        CROW_FIELD(g_CurrentEntity, room_hit) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
        g_CurrentEntity->floor_y = (u16) LIT(u16, sp_tmp1C);
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->pos_y > 0) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8;
        g_CurrentEntity->pos_y = -0x64;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
    }
    if (g_CurrentEntity->pos_y < -0x7530) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8;
        g_CurrentEntity->pos_y = -0x7148;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
    }
    LIT(s32, sp_tmp0C) = ((s32) g_CurrentEntity->pos_y >> 4) + 0x190;
    if (LIT(s32, sp_tmp0C) < 0) {
        LIT(s32, sp_tmp0C) = 0x32;
    }
    (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, (void *) LIT(s32, sp_tmp0C), LIT(s32, sp_tmp0C));
    if ((g_CurrentEntity->has_enter_switch_zone != 0) && ((s16) ((u16) g_CurrentEntity->floor_y) >= -0x63) && ((u16) ENTITY_FIELD_AS(g_CurrentEntity, state, u16) != 1)) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void crow_state_run(void) {
    void crow_behavior_dispatch();
    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    if (g_CurrentEntity->behavior_flags & 0x10 & 0xFF) {
        if ((CROW_FIELD(g_CurrentEntity, player_touch) != 0) && (((u8) g_PlayerEntity.hit_state) == 0) && (g_CurrentEntity->action_behavior != 9)) {
            if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
                if ((u32) (-g_CurrentEntity->pos_y - 0x9C4) >= 0x12DU) {
                    g_CurrentEntity->pos_y = -0x9C4;
                }
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 9, 0);
                return;
            }
            if (g_CurrentEntity->move_speed_current >= 0x12D) {
                if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) {
                    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 0, 0, 0);
                    g_PlayerEntity.hit_state = 1;
                    g_PlayerEntity.action_behavior = 0x64;
                    (g_ApiCallPtr = api_pad_vibrate_small)(5, 0);
                    (g_ApiCallPtr = api_pad_vibrate_large)(2, 0xC8, 0);
                    return;
                }
            }
        }
        if (CROW_FIELD(g_CurrentEntity, room_hit) != 0) {
            ++CROW_FIELD(g_CurrentEntity, wall_hit_frames);
            if ((CROW_FIELD(g_CurrentEntity, wall_hit_frames) >= 6) && (g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
                g_CurrentEntity->angle = g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40);
            }
            if ((CROW_FIELD(g_CurrentEntity, wall_hit_frames) >= 0x1F) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                CROW_FIELD(g_CurrentEntity, wall_hit_frames) = 0;
                if ((g_CurrentEntity->move_speed_current >= 0x12D) && (((u8) g_PlayerEntity.hit_state) == 0) && !(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
                    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 0, 0, 0);
                    return;
                }
                LIT(s32, sp_vec_vx) = g_PlayerEntity.pos_x;
                LIT(s32, sp_vec_vy) = g_PlayerEntity.pos_y;
                LIT(s32, sp_vec_vz) = g_PlayerEntity.pos_z;
                LIT(s32, sp_tmp1C) = 4;
                g_CurrentEntity->angle = g_CurrentEntity->angle + (g_ApiCallPtr = api_entity_swerve_around_obstacle)(0x40, CROW_FIELD_AS(g_CurrentEntity, room_hit, u8), &CROW(g_CurrentEntity)->swerve_angle, &CROW(g_CurrentEntity)->swerve_latch);
            }
        }
        if (((u8) g_PlayerEntity.hit_state) != 0) {
            g_CurrentEntity->angle = g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40);
            if (g_PlayerEntity.action_state >= 4) {
                ENTITY_AS(g_CurrentEntity, angle, s16) += ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) ? 1 : 0;
            }
            if ((g_PlayerEntity.health < 0) && (g_PlayerEntity.action_behavior == 0xC8) && (g_CurrentEntity->action_behavior == 9)) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
            }
        }
    }
    CROW_FIELD(g_CurrentEntity, player_dist) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    switch (ENTITY_AS(g_CurrentEntity, sub_state, u8)) {
    case 0:
        if ((ENTITY_STATE_WORD(&g_PlayerEntity) == STATE4(1, 3, 0x14, 0)) && !(g_CurrentEntity->behavior_flags & 0x10 & 0xFF)) {
            g_CurrentEntity->angle = g_CurrentEntity->angle + ((g_ApiCallPtr = api_rand)() & 1) * 0x40;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xD, 0);
        }
        /* fallthrough */
    case 1:
        crow_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = g_CurrentEntity->status_flags & 0x1F;
    (g_ApiCallPtr = api_entity_check_alert_range)(0x1388);
    if (g_CurrentEntity->pos_y >= -0x1193) {
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags & 0x1F;
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags | 0x40;
        if (g_CurrentEntity->pos_y >= -0x7CF) {
            (g_ApiCallPtr = api_entity_check_visual_range)(0x7D0);
        }
    }
    if (g_CurrentEntity->pos_y >= -0x5DB) {
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags & 0x1F;
        (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
    }
}

void crow_state_recoil(void) {
    void crow_anim_hold();
    void crow_anim_step();
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        g_CurrentEntity->animation_id = 6;
        crow_anim_hold();
        g_CurrentEntity->move_speed_current = 0xC8;
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0xC8);
        if (CROW_FIELD(g_CurrentEntity, player_touch) != 0) {
            (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        }
        if (g_CurrentEntity->animation_frame_id == 1) {
            (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, ((s16) g_CurrentEntity->angle), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, ((s16) g_CurrentEntity->angle) + 0x400, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        }
        if (g_CurrentEntity->pos_y >= -0x1C1) {
            ENTITY_ACTION_U16(g_CurrentEntity) = 1;
            g_CurrentEntity->pos_y = -0x1C2;
        }
        return;
    case 1:
        g_CurrentEntity->animation_id = 9;
        crow_anim_step();
        if (g_CurrentEntity->action_state == 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->hit_state = 0;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
        }
        return;
    }
}

void crow_state_death(void) {
    void crow_anim_hold();
    void crow_anim_step();
    s32 joints;

    if (g_CurrentEntity->floor_y < -0x7D0) {
        g_CurrentEntity->floor_y = 0x64;
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        g_CurrentEntity->animation_id = 6;
        if (!(((u8) g_CurrentEntity->hit_state) & 1 & 0xFF)) {
            g_CurrentEntity->move_speed_current = 0xC8;
            (g_ApiCallPtr = api_entity_add_speed_xz)((((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x400U) == 0) ? (void *)0x800 : NULL));
        }
        crow_anim_hold();
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0xC8);
        g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + 0x80);
        if (g_CurrentEntity->animation_frame_id == 1) {
            (g_ApiCallPtr = api_sound_play_entity_se)(NULL, 0U);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)0x1C, 0U, ((s16) g_CurrentEntity->angle), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)0x1C, 0U, ((s16) g_CurrentEntity->angle) + 0x400, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        }
        if (g_CurrentEntity->pos_y >= -0x1C1) {
            if (g_CurrentEntity->floor_y == 0x64) {
                ENTITY_ACTION_U16(g_CurrentEntity) = 2;
                break;
            }
            ENTITY_ACTION_U16(g_CurrentEntity) = 1;
            g_CurrentEntity->pos_y = -0x1C2;
            g_CurrentEntity->angle_z = 0U;
            (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, (void *)0xFFFF50);
            (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x64U, -0x64);
        }
        break;
    case 1:
        g_CurrentEntity->animation_id = 0xA;
        crow_anim_step();
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 6U, 6);
        if (g_CurrentEntity->action_state == 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
        }
        return;
    case 2:
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0U, 0);
        LIT(s32, sp_tmp1C) = (s32) g_CurrentEntity->joint_count;
        while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != -1) {
            (g_ApiCallPtr = api_joint_setup_attack_effect)((LIT(s32, sp_tmp1C) * 0x7C) + joints, 0x1EU, 0x1E, (void *)3);
        }
        (g_ApiCallPtr = api_sound_play_entity_se)(NULL, 0U);
        ENTITY_ACTION_S16(g_CurrentEntity) = 4;
        /* fallthrough */
    case 4:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        return;
    }
}

void crow_behavior_dispatch(void) {
    extern s32 (*crow_behavior_tbl[])();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        CROW_FIELD(g_CurrentEntity, path_flags) = (u16) (CROW_FIELD(g_CurrentEntity, path_flags) & ~1);
        CROW_FIELD(g_CurrentEntity, path_flags) = (u16) (((u16) CROW_FIELD(g_CurrentEntity, path_flags)) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    LIT(s32, sp_tmp18) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100);
    LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200);
    crow_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void crow_beh_perch_thunk(void) {
    void crow_beh_perch();
    crow_beh_perch();
}

void crow_beh_wingbeat(void) {
    void crow_anim_step();

    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->move_speed_current = (((g_ApiCallPtr = api_rand)() & 1) << 4) + 0x32;
    crow_anim_step();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x1E);
    if ((u8) g_CurrentEntity->animation_frame_id >= 9U) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = (s16) (g_CurrentEntity->animation_frame_id + 0x34);
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + CROW_FIELD(g_CurrentEntity, vert_speed));
    }
    g_CurrentEntity->angle_z = (u16) (g_CurrentEntity->angle_z + 0x15);
    if ((((u8) g_CurrentEntity->sub_state) != 0) && (g_CurrentEntity->pos_y < -0x5DB)) {
        return;
    }
    g_CurrentEntity->angle_z = 0x100U;
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
    if (g_CurrentEntity->pos_y >= -0x31F) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
    }
}

void crow_beh_dive(void) {
    void crow_anim_step();
    g_CurrentEntity->animation_id = 4;
    crow_anim_step();
    g_CurrentEntity->move_speed_current = (s16) (0xC8 - (g_CurrentEntity->animation_frame_id * 6));
    if (g_CurrentEntity->move_speed_current > 0) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    }
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (-CROW_FIELD(g_CurrentEntity, descend_limit_y) / 40));
    g_CurrentEntity->angle_z = (u16) (((s16) g_CurrentEntity->angle_z) - 6);
    if ((((u8) g_CurrentEntity->sub_state) != 0) && (g_CurrentEntity->pos_y < -0x1C1)) {
        return;
    }
    g_CurrentEntity->move_speed_current = 0;
    g_CurrentEntity->angle_z = 0U;
    g_CurrentEntity->pos_y = -0x1C2;
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
    if (LIT(s32, sp_tmp14) == 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 4;
    }
    if (g_PlayerEntity.health < 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void crow_beh_bank_turn(void) {
    void crow_anim_turn();

    g_CurrentEntity->animation_id = 3;
    CROW_FIELD(g_CurrentEntity, bank_turn_rate) = (s16) ((((((g_ApiCallPtr = api_rand)()) & 1) << 5) + 0x80) - ((API_AS(u32, api_rand)() & 1) << 6));
    crow_anim_turn();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        if (LIT(s32, sp_tmp14) == 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
        }
        if (g_PlayerEntity.health < 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

void crow_beh_strike_run(void) {
    void crow_anim_step();
    g_CurrentEntity->animation_id = 7;
    crow_anim_step();
    g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x19);
    if ((u8) g_CurrentEntity->animation_frame_id >= 9U) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = (s16) ((s32) ((((CROW_FIELD(g_CurrentEntity, player_dist) / 100) & 0xFFFF) + 0xC8) * 0xA28) / (s32) CROW_FIELD(g_CurrentEntity, player_dist));
        if (CROW_FIELD(g_CurrentEntity, vert_speed) >= 0x1F5) {
            CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8;
        }
        g_CurrentEntity->move_speed_current = (s16) ((CROW_FIELD(g_CurrentEntity, player_dist) / 100) + 0xC8);
        ENTITY_AS(g_CurrentEntity, angle_z, s16) = -0x100;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
    }
}

void crow_beh_descend(void) {
    void crow_anim_hold();

    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((CROW_FIELD(g_CurrentEntity, path_flags) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x10) * 2));
    if (((u16) CROW_FIELD(g_CurrentEntity, player_dist) < 0x7D0U) && !((g_CurrentEntity->animation_frame_id % 9) & 0xFF)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
    }
    crow_anim_hold();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    CROW_FIELD(g_CurrentEntity, vert_speed) = (u16) (((u16) CROW_FIELD(g_CurrentEntity, vert_speed)) + 1);
    if ((s16) ((u16) CROW_FIELD(g_CurrentEntity, vert_speed)) >= 0x1F5) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8U;
    }
    g_CurrentEntity->pos_y = (g_CurrentEntity->pos_y + (s16) ((u16) CROW_FIELD(g_CurrentEntity, vert_speed)));
    CROW_FIELD(g_CurrentEntity, descend_limit_y) = (s16) ((CROW_FIELD(g_CurrentEntity, altitude_bias) * 2) - (((API_AS(u32, api_rand)() & 1) * 0x3E8) + 0x5DC));
    if ((g_CurrentEntity->pos_y <= CROW_FIELD(g_CurrentEntity, descend_limit_y)) && (g_CurrentEntity->pos_y < -0x5DB)) {

    } else {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        if ((LIT(s32, sp_tmp14) != 0) && !(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
        }
    }
    if ((u16) CROW_FIELD(g_CurrentEntity, player_dist) < 0x4B0U) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && ((u32) (-g_CurrentEntity->pos_y - 0x9C4) < 0x12CU)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 9, 0);
        }
    }
}

void crow_beh_ascend(void) {
    void crow_anim_hold();

    g_CurrentEntity->animation_id = 5;
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((CROW_FIELD(g_CurrentEntity, path_flags) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x10) * 2));
    if (((u16) CROW_FIELD(g_CurrentEntity, player_dist) < 0x7D0U) && !((g_CurrentEntity->animation_frame_id % 9) & 0xFF)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
    }
    crow_anim_hold();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    CROW_FIELD(g_CurrentEntity, vert_speed) = (u16) (((u16) CROW_FIELD(g_CurrentEntity, vert_speed)) + 1);
    if ((s16) ((u16) CROW_FIELD(g_CurrentEntity, vert_speed)) >= 0x1F5) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8U;
    }
    g_CurrentEntity->pos_y = (g_CurrentEntity->pos_y - (s16) ((u16) CROW_FIELD(g_CurrentEntity, vert_speed)));
    CROW_FIELD(g_CurrentEntity, climb_limit_y) = (s16) (-((((g_ApiCallPtr = api_rand)() & 1) * 0x3E8) + 0x1194) + ((u16) CROW_FIELD(g_CurrentEntity, altitude_bias)));
    if (g_CurrentEntity->pos_y < CROW_FIELD(g_CurrentEntity, climb_limit_y)) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
    }
    if ((u16) CROW_FIELD(g_CurrentEntity, player_dist) < 0x4B0U) {
        if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x100) == 0) && ((u32) (-g_CurrentEntity->pos_y - 0x9C4) < 0x12CU)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 9, 0);
        }
    }
}

void crow_beh_dive_attack(void) {
    void crow_anim_step();

    g_CurrentEntity->animation_id = 7;
    crow_anim_step();
    g_CurrentEntity->move_speed_current = (s16) (((CROW_FIELD(g_CurrentEntity, player_dist) / 100) + 0xC8) - (((g_ApiCallPtr = api_rand)() & 1) * 0x32));
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    g_CurrentEntity->angle_z = (u16) (ENTITY_AS(g_CurrentEntity, angle_z, s16) - 0x19);
    if (((u8) g_CurrentEntity->animation_frame_id < 0x13U) && !(((u8) g_PlayerEntity.hit_state) & 1 & 0xFF)) {
        return;
    }
    if ((((u8) g_PlayerEntity.hit_state) & 1 & 0xFF) && (g_PlayerEntity.action_state == 4)) {
        g_CurrentEntity->pos_y = (s32) ((g_CurrentEntity->pos_y) - 0x32);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (((g_ApiCallPtr = api_rand)() & 1) * 0x800));
    }
    ENTITY_AS(g_CurrentEntity, angle_z, s16) = -0x100;
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
    if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
        CROW_FIELD(g_CurrentEntity, vert_speed) = (s16) ((s32) ((((CROW_FIELD(g_CurrentEntity, player_dist) / 100) & 0xFFFF) + 0xC8) * 0xA28) / (s32) CROW_FIELD(g_CurrentEntity, player_dist));
        if (CROW_FIELD(g_CurrentEntity, vert_speed) >= 0x1F5) {
            CROW_FIELD(g_CurrentEntity, vert_speed) = 0xC8;
        }
    }
}

void crow_beh_level_out(void) {
    void crow_anim_step();
    g_CurrentEntity->animation_id = 5;
    crow_anim_step();
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x19);
    if (((s16) g_CurrentEntity->angle_z) > 0) {
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0x32);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    g_CurrentEntity->angle_z = (s16) ((u16) ((s16) g_CurrentEntity->angle_z) + 0x19);
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->angle_z = 0x100;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
    }
}

void crow_beh_approach(void) {
    void crow_anim_hold();
    g_CurrentEntity->animation_id = 5;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x40);
    crow_anim_hold();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    if ((u8) g_CurrentEntity->animation_frame_id >= 9U) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
    }
}

void crow_beh_peck(void) {
    void crow_anim_step();

    g_CurrentEntity->animation_id = 8;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
    crow_anim_step();
    if ((((u8) g_PlayerEntity.hit_state) & 1 & 0xFF) && ((u8) g_PlayerEntity.action_state >= 4U)) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
        g_CurrentEntity->move_speed_current = (((CROW_FIELD(g_CurrentEntity, player_dist) / 100) + 0xC8) - (((g_ApiCallPtr = api_rand)() & 1) * 0x32));
        ENTITY_AS(g_CurrentEntity, angle_z, s16) = -0x100;
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (((g_ApiCallPtr = api_rand)() & 1) * 0x800));
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x64);
    }
    if (g_CurrentEntity->action_state == 0) {
        ENTITY_AS(g_CurrentEntity, angle_z, s16) = 0x100;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, (s16) g_CurrentEntity->angle, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        if ((u16) CROW_FIELD(g_CurrentEntity, player_dist) < 0x5DCU) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) == 0) && ((s16) (CROW_FIELD(g_CurrentEntity, path_flags) & 1) != 0)) {
                ENTITY_ACTION_S16(g_CurrentEntity) = 9;
                if (((u8) g_PlayerEntity.hit_state) == 0) {
                    g_PlayerEntity.health -= 0x10;
                    g_PlayerEntity.hit_state = 1;
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 5, 0, 0);
                    (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, LITP(void, g_PlayerPosX));
                    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                    LIT(s32, sp_vec_vy) = -0x9D8;
                    (g_ApiCallPtr = api_effect_create_billboard)(0, 1, ((s16) g_PlayerEntity.angle), LITP(void, g_PlayerCoordM00), ADDR_sp_vec, 0);
                    g_CurrentEntity->hit_state = 1;
                    ENTITY_AS(g_CurrentEntity, angle_z, s16) = 0;
                    ENTITY_ACTION_S16(g_CurrentEntity) = 0xB;
                }
            }
        }
    }
}

void crow_beh_grab(void) {
    void crow_anim_grab();

    g_CurrentEntity->animation_id = 8;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x80);
    g_CurrentEntity->move_speed_current = 0x32;
    crow_anim_grab();
    (g_ApiCallPtr = api_entity_add_speed_xz)(((((g_ApiCallPtr = api_rand)() & 1) * 0x800) + 0x400));
}

void crow_beh_victory_descend(void) {
    void crow_anim_hold();
    g_CurrentEntity->animation_id = 0;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x80);
    crow_anim_hold();
    g_CurrentEntity->move_speed_current = 0xC8;
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    CROW_FIELD(g_CurrentEntity, vert_speed) = (u16) (((u16) CROW_FIELD(g_CurrentEntity, vert_speed)) + 1);
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + (s16) ((u16) CROW_FIELD(g_CurrentEntity, vert_speed)));
    if (g_CurrentEntity->pos_y >= -0xBB7) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
    }
}

void crow_beh_takeoff_thunk(void) {
    void crow_beh_takeoff();
    crow_beh_takeoff();
}

void crow_anim_hold(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    }
}

void crow_anim_step(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        return;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void crow_anim_turn(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) ((((g_ApiCallPtr = api_rand)() & 0x1F) * 2) + 0x50);
        /* fallthrough */
    case 1:
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, CROW_FIELD(g_CurrentEntity, bank_turn_rate));
        if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) || (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C));
        return;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        return;
    }
}

void crow_anim_grab(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        CROW_FIELD(g_CurrentEntity, grab_struggle) = 0x64;
        (g_ApiCallPtr = api_pad_vibrate_small)(8, 0);
        (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0x96, 4);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (g_PlayerEntity.health < 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xC, 0);
            (*(s16 *)&g_PlayerEntity.state) = 3;
            ENTITY_ACTION_S16(&g_PlayerEntity) = 0xC8;
            return;
        }
        CROW_FIELD(g_CurrentEntity, grab_struggle) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 9 : 1;
        if (((CROW_FIELD(g_CurrentEntity, grab_struggle) < 0) && (g_PlayerEntity.action_state == 3)) || (((u16) CROW_FIELD(g_CurrentEntity, player_dist) >= 0x5DDU) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100) != 0))) {
            g_CurrentEntity->action_state = 2U;
            g_PlayerEntity.action_state = 4;
            return;
        }
        if (!((g_PlayerEntity.animation_frame_id % 20) & 0xFF)) {
            SCRATCH_VEC->x = 0x64;
            LIT(s32, sp_vec_vy) = -0x974;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 1, 0x200, LITP(void, g_PlayerCoordM00), ADDR_sp_vec, 0);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, (s32) (s16) g_CurrentEntity->angle, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            g_PlayerEntity.health = g_PlayerEntity.health - 4;
            (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(0xF, 0x96, 4);
            (g_ApiCallPtr = api_pad_vibrate_small)(3, 0xF);
        }
        return;
    case 2:
        g_CurrentEntity->move_speed_current = ((CROW_FIELD(g_CurrentEntity, player_dist) / 100) + 0xC8) - (((g_ApiCallPtr = api_rand)() & 1) * 0x32);
        ENTITY_AS(g_CurrentEntity, angle_z, s16) = -0x100;
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (((g_ApiCallPtr = api_rand)() & 1) * 0x800));
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x64);
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
        g_CurrentEntity->hit_state = 0;
    }
}

void crow_beh_perch(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->action_ticks_counter = (((g_ApiCallPtr = api_rand)() & 0x1F) * 2) + 0x28;
        /* fallthrough */
    case 1:
        if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) && (g_PlayerEntity.health >= 0)) {

        } else {
            g_CurrentEntity->action_state = 2;
        }
        return;
    case 2:
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->animation_id = 0xB;
        if ((g_ApiCallPtr = api_rand)() & 1) {
            g_CurrentEntity->animation_id = 0xC;
            if ((g_ApiCallPtr = api_rand)() & 1) {
                g_CurrentEntity->animation_id = 2;
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)1, 0);
            }
        }
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 4:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void crow_beh_takeoff(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((((g_ApiCallPtr = api_rand)()) & 1) * 3) + (((g_ApiCallPtr = api_rand)() & 1) * 8));
        g_CurrentEntity->sub_state = 1;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 0x10);
        /* fallthrough */
    case 1:
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 2U;
        }
        break;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 1, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, ENTITY_AS(g_CurrentEntity, angle, s16), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0x1C, 0, ENTITY_AS(g_CurrentEntity, angle, s16) + 0x400, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
}

void player_anim_multi_attack() {
    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0;
        /* fallthrough */
    case 1:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        break;
    case 2:
        g_PlayerEntity.animation_id = 1;
        g_PlayerEntity.action_state = 3;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        break;
    case 4:
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.action_state = 5;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        /* fallthrough */
    case 5:
        if ((g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
            g_PlayerEntity.hit_state = 0;
        }
        break;
    }
    if (g_PlayerEntity.animation_frame_id > (0xA - ((g_PlayerEntity.id & 1) * 4))) {
        (g_ApiCallPtr = api_entity_update_weapon_joint)(1);
        return;
    }
    (g_ApiCallPtr = api_entity_update_weapon_joint)(0);
}

void crow_state_scd(void) {
    void crow_scd_fly_to_target();

    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 2:
    case 0:
        crow_scd_fly_to_target();
        break;
    case 1:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(0, 0, 0, 0);
        g_CurrentEntity->behavior_flags = 0x11U;
        break;
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
}

void crow_scd_fly_to_target(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->move_speed_current = 0xC8U;
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        /* fallthrough */
    case 1:
        if (!((g_CurrentEntity->animation_frame_id % 9) & 0xFF)) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                if ((g_ApiCallPtr = api_rand)() & 1) {
                    (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
                }
            }
        }
        LIT(s32, sp_vec_vx) = (s32) g_CurrentEntity->anim_base_x;
        LIT(s32, sp_vec_vz) = (s32) g_CurrentEntity->anim_base_z;
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x40);
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200U);
        g_CurrentEntity->move_speed_current = (u16) (g_CurrentEntity->move_speed_current + 1);
        if ((s16) g_CurrentEntity->move_speed_current >= 0x191) {
            g_CurrentEntity->move_speed_current = 0xC8U;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        if (g_CurrentEntity->pos_y >= -0x5DB) {
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 0x1F4);
        }
        if ((g_ApiCallPtr = api_SquareRoot0)(((g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x) * (g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x)) + ((g_CurrentEntity->pos_z - g_CurrentEntity->anim_base_z) * ((g_CurrentEntity->pos_z) - (g_CurrentEntity->anim_base_z)))) < 0x96U) {
            g_CurrentEntity->action_state++;
        }
        break;
    case 2:
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        (g_CurrentEntity->animation_frame_id / 9);
    }
}
