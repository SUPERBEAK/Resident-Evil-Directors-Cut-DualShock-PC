/* monster_plant.c - Monster plant (the small plants): watch, grab, strike and ambush modes, swaying, death, and the
   player animation while a plant holds them (player_anim_plant_hold).
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/monster_plant.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* monster plant data (defined in each overlay that has this enemy) */
extern s32 monster_plant_sca_info;
extern s32 g_MonsterPlantLastSides;
extern s16 monster_plant_extend_stops;
extern s32 monster_plant_retract_stops;

/* MonsterPlantWork: the monster_plant's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). MONSTER_PLANT(e)->f, MONSTER_PLANT_FIELD(e, f) and MONSTER_PLANT_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct MonsterPlantWork {
    /* +0x00 */ s32 player_dist;     /* 0x16C: |dx|+|dz| to the player (monster_plant_state_check); attack/retreat range checks (0xA8D, 0xBB8/0x9C4, 0x2135..) */
    /* +0x04 */ s16 attack_cooldown; /* 0x170: 30 after a strike (monster_plant_grab_end / monster_plant_grab_lash_repeat), counted down in monster_plant_watch_strike_range/monster_plant_grab_wait; non-zero blocks the next attack */
    /* +0x06 */ s16 anim_end;        /* 0x172: last api_joint_move result (non-zero = animation finished), tested by the grab / strike / ambush step handlers */
    /* +0x08 */ s16 vine_segment;    /* 0x174: vine segment cursor (14 in monster_plant_start_ambush); monster_plant_ambush_extend / monster_plant_ambush_retract walk it against monster_plant_extend_stops setting joint bit 0 */
    /* +0x0A */ s16 timer;           /* 0x176: general countdown: strike/extend delays (2, 0xC), and the tint fade period for flag 0x29 (0x28) and death (4) */
    /* +0x0C */ s16 tint_timer;      /* 0x178: second tint fade period (0x28 for flag 0x29, 8 on death); each expiry steps api_clut_fade_add_delta */
    /* +0x0E */ s16 fidget_timer;    /* 0x17A: idle sway countdown (rand & 0x1F), monster_plant_mode_watch / monster_plant_idle_fidget */
    /* +0x10 */ s16 unengaged;       /* 0x17C: 1 until the first strike (init / monster_plant_start_*), cleared in monster_plant_strike_swing; widens the reach check (0xBB8 vs 0x9C4) */
    /* +0x12 */ u16 saved_angle;     /* 0x17E: yaw saved before a test turn and restored (monster_plant_watch_ambush_range, monster_plant_grab_hold / _grab_end / _ambush_retract); flag 0x29 uses it as tint steps left */
    /* +0x14 */ u16 lunge_side;      /* 0x180: 0 none, 1/2 = side to lunge (angle += (n-1)<<11, monster_plant_watch_ambush_range); flag 0x29 uses it as tint steps left */
    /* +0x16 */ s16 shadow_flags;    /* 0x182: bit 0 = draw the ground shadow billboard (monster_plant_update); 1 or 0 in monster_plant_init/monster_plant_start_hidden */
    /* +0x18 */ s32 saved_state;     /* 0x184: copy of the state word (monster_plant_state_check); restored by monster_plant_damaged */
    /* +0x1C */ s16 hit_count;       /* 0x188: hits taken (monster_plant_damaged); at 4 sets game flag 0x5B */
} MonsterPlantWork;
#define MONSTER_PLANT(e) ENEMY_WORK(e, MonsterPlantWork)
#define MONSTER_PLANT_FIELD(e, f) ENEMY_FIELD(e, MonsterPlantWork, f)
#define MONSTER_PLANT_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, MonsterPlantWork, f, T)
#define MONSTER_PLANT_MEMBER(e, f) ENEMY_MEMBER(e, MonsterPlantWork, f)

/* every frame for each monster plant (enemies_update_functions_tbl): runs monster_plant_state_tbl[state], then collision checks and
   the shadow / drawing */
void monster_plant_update(void) {
    extern s32 (*monster_plant_state_tbl[])();
    s32 joints;

    joints = g_CurrentEntity->joints;
    if (g_UpdateFlags & 4 & 0xFFFF) {
        monster_plant_state_tbl[g_CurrentEntity->state]();
        *((s16 *) g_CurrentEntity->sca_hit_data) = MEMBER_AT((JOINT_N(joints, 11)), Joint, ->world.t[0], u16) - ENTITY_AS(g_CurrentEntity, pos_x, u16);
        HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->y = (s16) (MEMBER_AT((JOINT_N(joints, 11)), Joint, ->world.t[1], u16) - ENTITY_AS(g_CurrentEntity, pos_y, u16));
        HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->z = (s16) (MEMBER_AT((JOINT_N(joints, 11)), Joint, ->world.t[2], u16) - ENTITY_AS(g_CurrentEntity, pos_z, u16));
    }
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if ((g_CurrentEntity->has_enter_switch_zone != 0) && (((u16) MONSTER_PLANT_FIELD(g_CurrentEntity, shadow_flags)) & 1 & 0xFFFF)) {
        (g_ApiCallPtr = api_billboard_draw)(JOINT_N_POS(joints, 11), &g_CurrentEntity->shadow, 0, ((s16) g_CurrentEntity->angle));
    }
}

/* state 0: sets up a new monster plant (health, sca info, animation, work fields), then state 1 */
void monster_plant_init(void) {
    u32 i;
    Joint *joint;

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->health = 1;
    g_CurrentEntity->sca_info = (s32)&monster_plant_sca_info;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) ((g_CurrentEntity->status_flags) | 0xA);
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_tmp1C) = 0x404040;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0x1F4, 0x64);
    MONSTER_PLANT_FIELD(g_CurrentEntity, shadow_flags) = 1;
    MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, unengaged) = 1;
    MONSTER_PLANT_FIELD(g_CurrentEntity, hit_count) = 0;
    (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x80);
    g_MonsterPlantLastSides = 0;
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        joint = ((void *) g_CurrentEntity->joints);
        i = 0;
        while (i < 0xFU) {
            joint->flags = joint->flags & 0xFE;
            joint++;
            i++;
        }
        g_CurrentEntity->health = -1;
        MONSTER_PLANT_FIELD(g_CurrentEntity, shadow_flags) = 0;
    }
}

void monster_plant_state_check(void) {
    void monster_plant_select_variant();
    extern s32 (*monster_plant_mode_tbl[])();
    u8 bflags;

    bflags = g_CurrentEntity->behavior_flags;
    if (bflags & 0x40 & 0xFF) {
        g_CurrentEntity->state = 3;
        g_CurrentEntity->sub_state = 0U;
        return;
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if ((bflags != 0) && (bflags != 1)) {

    } else {
        (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
    }
    if ((bflags != 3) && (bflags != 4)) {

    } else {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xC0);
        (g_ApiCallPtr = api_entity_check_visual_range)(0x1388);
    }
    MONSTER_PLANT(g_CurrentEntity)->player_dist = (s32) (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z));
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        monster_plant_select_variant();
    }
    monster_plant_mode_tbl[((u8) g_CurrentEntity->sub_state)]();
    MONSTER_PLANT_FIELD(g_CurrentEntity, saved_state) = (s32) ENTITY_STATE_WORD(g_CurrentEntity);
}

void monster_plant_select_variant(void) {
    void monster_plant_start_watch();
    extern s32 (*monster_plant_start_tbl[])();
    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        monster_plant_start_watch();
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
    } else {
        monster_plant_start_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
    }
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
}

void monster_plant_start_watch(void) {
    g_CurrentEntity->sub_state = 1;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xF7);
    MONSTER_PLANT_FIELD(g_CurrentEntity, unengaged) = 1;
}

void monster_plant_start_hidden(void) {
    u32 i;
    Joint *joint;

    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
    g_CurrentEntity->sub_state = 1;
    g_CurrentEntity->animation_id = (g_PlayerEntity.id & 1 & 0xFF) ? 0x10 : 0xF;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, shadow_flags) = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 0;
    joint = ((void *) g_CurrentEntity->joints);
    i = 0;
    while (i < 0xFU) {
        joint->flags = joint->flags & 0xFE;
        joint++;
        i++;
    }
}

void monster_plant_start_grab(void) {
    g_CurrentEntity->sub_state = 2;
    g_CurrentEntity->animation_id = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x1F;
}

void monster_plant_start_strike(void) {
    g_CurrentEntity->sub_state = 3;
    if ((((s16) g_CurrentEntity->angle) >= 0x800) && (((s16) g_CurrentEntity->angle) < 0x1000)) {
        g_CurrentEntity->animation_id = 2;
    } else {
        g_CurrentEntity->animation_id = 9;
    }
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
}

void monster_plant_start_ambush(void) {
    void monster_plant_grab_seize();
    u32 i;
    Joint *joint;
    s32 unused;

    g_CurrentEntity->sub_state = 4;
    g_CurrentEntity->animation_id = (g_PlayerEntity.id & 1 & 0xFF) ? 0x10 : 0xF;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    joint = ((void *) g_CurrentEntity->joints);
    i = 0;
    while (i < 0xFU) {
        joint->flags = joint->flags & 0xFE;
        joint++;
        i++;
    }
    MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment) = 0xE;
    g_CurrentEntity->action_ticks_counter = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 2;
    MONSTER_PLANT_FIELD(g_CurrentEntity, unengaged) = 1;
    MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle) = (u16) g_CurrentEntity->angle;
    g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
    monster_plant_grab_seize();
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
}

void monster_plant_start_dead(void) {

    g_CurrentEntity->sub_state = 5;
    g_CurrentEntity->health = -1;
    g_CurrentEntity->animation_id = (!((g_ApiCallPtr = api_rand)() & 1)) ? 7 : 0;
    g_CurrentEntity->animation_frame_id = (s8) ((g_ApiCallPtr = api_rand)() & 0xF);
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    g_CurrentEntity->action_ticks_counter = 1;
    if (g_CurrentEntity->behavior_flags == 7) {
        (g_ApiCallPtr = api_clut_fade_set)(2, -3, 0, 0, 0x100, (s32) g_CurrentEntity->id);
    }
}

void monster_plant_start_sway(void) {
    u8 side;

    g_CurrentEntity->sub_state = 6;
    side = (g_ApiCallPtr = api_rand)() & 1;
    if ((g_MonsterPlantLastSides & 3) == (side * 3)) {
        side ^= 1;
    }
    g_MonsterPlantLastSides = ((g_MonsterPlantLastSides << 1) & ~1) | side;
    g_CurrentEntity->animation_id = (side != 0) ? 0x11 : 0x13;
    g_CurrentEntity->animation_frame_id = (((g_ApiCallPtr = api_rand)() & 7) * 3);
    if (((u8) g_CurrentEntity->animation_id) == 0x11) {
        g_CurrentEntity->animation_frame_id = (u8) (g_CurrentEntity->animation_frame_id + (((g_ApiCallPtr = api_rand)() & 7) * 3));
    }
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0x1F;
    MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = 0;
    g_CurrentEntity->action_ticks_counter = 1;
    if (g_CurrentEntity->behavior_flags == 0x29) {
        MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 0x28;
        MONSTER_PLANT_FIELD(g_CurrentEntity, tint_timer) = 0x28;
        MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle) = __builtin_abs(2);
        MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(4U, 0U);
    }
}

void monster_plant_mode_watch(void) {
    extern s32 (*monster_plant_watch_tbl[])();

    if (g_CurrentEntity->behavior_flags & 0x20 & 0xFF) {
        g_CurrentEntity->sub_state = 0;
        return;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) == 0) {
        g_CurrentEntity->animation_id = (!((g_ApiCallPtr = api_rand)() & 1)) ? 7 : 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = (s16) ((g_ApiCallPtr = api_rand)() & 0x1F);
        return;
    }
    MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer)--;
    if (g_CurrentEntity->behavior_flags != 0x80) {
        monster_plant_watch_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
    }
    if (g_CurrentEntity->behavior_flags != 2) {
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
            g_CurrentEntity->sub_state = 0;
        }
    }
    if (ENTITY_STATE_WORD(&g_PlayerEntity) == STATE4(1, 3, 0x14, 0)) {
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
            g_CurrentEntity->sub_state = 0;
        }
    }
}

void monster_plant_watch_grab_range(void) {
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) < 0xA8D) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 3;
    }
}

void monster_plant_watch_strike_range(void) {
    s32 range;

    if (MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
        MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown)--;
        return;
    }
    range = (MONSTER_PLANT_FIELD_AS(g_CurrentEntity, unengaged, u16) != 0) ? 0xBB8 : 0x9C4;
    if ((MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) <= range) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) == 0) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->behavior_flags = 4;
        }
    }
}

void monster_plant_watch_ambush_range(void) {
    if (g_CurrentEntity->behavior_flags != 2) {
        g_CurrentEntity->sub_state = 0;
        return;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) >= 0xFA1) {
        MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 0U;
        MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 0;
        return;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) < 0xC45) {
        if (MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) == 0) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 2U;
            }
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x600) != 0) {
                MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 1U;
            }
            return;
        }
        if (((u8) g_PlayerEntity.hit_state) == 0) {
            MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle) = (u16) g_CurrentEntity->angle;
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((((s16) MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side)) - 1) << 0xB));
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                g_CurrentEntity->sub_state = 0;
                g_CurrentEntity->behavior_flags = 5U;
            }
            g_CurrentEntity->angle = (u16) MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle);
        }
    }
}

void monster_plant_mode_grab(void) {
    extern s32 (*monster_plant_grab_step_tbl[])();
    if (g_CurrentEntity->behavior_flags & 0x20 & 0xFF) {
        g_CurrentEntity->sub_state = 0;
        return;
    }
    MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    monster_plant_grab_step_tbl[g_CurrentEntity->action_behavior]();
}

void monster_plant_grab_wait(void) {
    void monster_plant_idle_fidget();

    if ((MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) && (((u8) g_CurrentEntity->animation_id) == 1)) {
        g_CurrentEntity->animation_id = 8U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0xF;
    }
    if (((u8) g_CurrentEntity->animation_id) != 1) {
        monster_plant_idle_fidget();
        if (g_PlayerEntity.action_behavior == 0x14) {
            (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        }
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) >= 0x2135) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 0;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
        MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown)--;
        return;
    }
    if ((MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) < 0x708) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = (g_PlayerEntity.id & 1 & 0xFF) ? 0xA : 4;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) != 0) {
            g_CurrentEntity->action_behavior = 4U;
            MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 2;
            return;
        }
    }
}

void monster_plant_grab_seize(void) {
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
    g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) + 2;
    g_PlayerEntity.angle = g_CurrentEntity->angle;
    (g_ApiCallPtr = api_snap_player_to_grab_position)(ADDR_g_PlayerEntity);
    g_PlayerEntity.hit_state = 1;
    g_PlayerEntity.state = 6;
    g_PlayerEntity.sub_state = 0xF;
    g_PlayerEntity.action_behavior = 0;
    g_PlayerEntity.action_state = 0;
    g_PlayerEntity.status_flags |= 2;
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
    (g_ApiCallPtr = api_pad_vibrate_small)(8, 0);
    (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x14, 0x50, 0x82, 0);
}

void monster_plant_grab_hold(void) {
    MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle) = (u16) g_CurrentEntity->angle;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x80);
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->action_ticks_counter = 0x3C;
        MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 0xC;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        if ((g_PlayerEntity.id & 3 & 0xFF) != 3) {
            (g_ApiCallPtr = api_sound_play_se_3d)(2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, 0, ADDR_g_PlayerPosX);
            return;
        }
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
    }
}

void monster_plant_grab_squeeze(void) {
    if (--MONSTER_PLANT_FIELD(g_CurrentEntity, timer) == 0) {
        g_PlayerEntity.health -= 2;
        if ((g_CurrentEntity->behavior_flags == 5) && ((s16) g_PlayerEntity.health < 0)) {
            g_PlayerEntity.health = 0;
        }
        MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 0xCU;
    }
    if ((ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) > 0) && ((s16) g_PlayerEntity.health >= 0)) {
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 4 : 1;
    } else if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->action_behavior++;
        ENTITY_AS(g_CurrentEntity, animation_id, u8)++;
        g_CurrentEntity->timing_control = 0;
        g_PlayerEntity.action_state = 2;
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x19U, 0x96, 0, 0);
    }
}

void monster_plant_grab_end(void) {
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 0x1E;
        g_CurrentEntity->angle = (u16) MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle);
    }
}

void monster_plant_grab_lash_repeat(void) {

    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        if (--MONSTER_PLANT_FIELD(g_CurrentEntity, timer) == 0) {
            if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) < 0xA8D) {
                g_CurrentEntity->action_behavior = 0;
                g_CurrentEntity->animation_id = 8U;
                g_CurrentEntity->animation_frame_id = 0;
                g_CurrentEntity->timing_control = 0;
                g_CurrentEntity->blend_counter = 0xF;
            } else {
                g_CurrentEntity->sub_state = 0;
                g_CurrentEntity->behavior_flags = 3;
            }
            MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 0x1E;
            return;
        }
        g_CurrentEntity->animation_id = (u8) (g_CurrentEntity->animation_id + 2);
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0xF;
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
}

void monster_plant_mode_strike(void) {
    extern s32 (*monster_plant_strike_step_tbl[])();
    if (g_CurrentEntity->behavior_flags & 0x20 & 0xFF) {
        g_CurrentEntity->sub_state = 0;
        return;
    }
    monster_plant_strike_step_tbl[g_CurrentEntity->action_behavior]();
}

void monster_plant_strike_swing(void) {
    u8 hit_dir;

    MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->action_behavior++;
        MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = 0;
        MONSTER_PLANT_FIELD(g_CurrentEntity, unengaged) = 0;
    }
    if ((g_CurrentEntity->animation_frame_id == 6) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) == 0) {
            g_PlayerEntity.action_state = 0;
            hit_dir = (((s16) g_PlayerEntity.angle) < 0x401) || (((s16) g_PlayerEntity.angle) >= 0xC00);
            g_PlayerEntity.hit_state = hit_dir + 1;
            g_PlayerEntity.action_behavior = hit_dir + 0x66;
            (g_ApiCallPtr = api_pad_vibrate_large)(5, 0x64, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
            g_PlayerEntity.health -= 2;
            if ((s16) g_PlayerEntity.health < 0) {
                g_PlayerEntity.health = 0;
            }
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
            MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 1;
        }
    }
}

void monster_plant_strike_recover(void) {
    void monster_plant_idle_fidget();
    monster_plant_idle_fidget();
    monster_plant_watch_strike_range();
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, player_dist) >= 0x2134) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->behavior_flags = 1;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    if (g_PlayerEntity.action_behavior == 0x14) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    }
}

void monster_plant_mode_ambush(void) {
    extern s32 (*monster_plant_ambush_step_tbl[])();
    MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    monster_plant_ambush_step_tbl[g_CurrentEntity->action_behavior]();
}

void monster_plant_ambush_extend(void) {
    s16 seg;
    Joint *joint;

    if ((MONSTER_PLANT_FIELD(g_CurrentEntity, timer) == 0) && (((s16) g_CurrentEntity->action_ticks_counter) < 4)) {
        joint = ((void *) g_CurrentEntity->joints);
        joint = joint + (MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment));
        seg = (u16) MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment);
        while ((s16) seg >= *(s16 *)((u8 *)&monster_plant_extend_stops + ((((s16) g_CurrentEntity->action_ticks_counter) * 2)))) {
            joint->flags = joint->flags | 1;
            joint--;
            seg--;
        }
        MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment) = (s16) seg;
        g_CurrentEntity->action_ticks_counter++;
    } else {
        MONSTER_PLANT_FIELD(g_CurrentEntity, timer)--;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->action_behavior++;
        g_CurrentEntity->animation_id = (g_PlayerEntity.id & 1 & 0xFF) ? 0xB : 5;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_PlayerEntity.status_flags |= 8;
        g_CurrentEntity->action_ticks_counter = 0x3C;
        MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 0xC;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        if ((g_PlayerEntity.id & 3 & 0xFF) != 3) {
            (g_ApiCallPtr = api_sound_play_se_3d)(2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, 0, ADDR_g_PlayerPosX);
            return;
        }
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
    }
}

void monster_plant_ambush_retract_start(void) {
    g_CurrentEntity->action_behavior++;
    g_CurrentEntity->animation_id = 0x12;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment) = 0;
    g_CurrentEntity->action_ticks_counter = 0;
}

void monster_plant_ambush_retract(void) {
    u16 seg;
    Joint *joint;

    if (((u8) g_CurrentEntity->animation_frame_id >= 3U) && (MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment) < 0xF)) {
        joint = ((void *) g_CurrentEntity->joints);
        joint = joint + (MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment));
        seg = (u16) MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment);
        while ((s16) seg <= *(s16 *)((u8 *)&monster_plant_retract_stops + ((((s16) g_CurrentEntity->action_ticks_counter) * 2)))) {
            joint->flags = joint->flags & 0xFE;
            joint++;
            seg++;
        }
        MONSTER_PLANT_FIELD(g_CurrentEntity, vine_segment) = (s16) seg;
        g_CurrentEntity->action_ticks_counter++;
    }
    if (MONSTER_PLANT_FIELD(g_CurrentEntity, anim_end) != 0) {
        g_CurrentEntity->sub_state = 0;
        if (g_CurrentEntity->behavior_flags != 0x80) {
            g_CurrentEntity->behavior_flags = 2U;
        }
        g_CurrentEntity->angle = (u16) MONSTER_PLANT_FIELD(g_CurrentEntity, saved_angle);
        MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) = 0;
        MONSTER_PLANT_FIELD(g_CurrentEntity, attack_cooldown) = 0;
    }
}

void monster_plant_mode_dead(void) {
    g_CurrentEntity->health = -1;
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100);
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    }
}

void monster_plant_mode_sway(void) {
    s32 unused;

    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
        if ((u32) ((g_ApiCallPtr = api_rand)() & 0x80) >= 0x7EU) {
            g_CurrentEntity->animation_id = ((g_ApiCallPtr = api_rand)() & 1) ? 0x11 : 0x13;
            g_CurrentEntity->blend_counter = 0x1F;
        }
    }
    if (g_CurrentEntity->behavior_flags != 0x29) {
        return;
    }
    if (--MONSTER_PLANT_FIELD(g_CurrentEntity, timer) == 0) {
        if (MONSTER_PLANT_FIELD_AS(g_CurrentEntity, saved_angle, s16) != 0) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(1, 0, 0, 0, 0x100, (s32) g_CurrentEntity->id);
            MONSTER_PLANT_FIELD_AS(g_CurrentEntity, saved_angle, s16)--;
        }
        MONSTER_PLANT_FIELD_AS(g_CurrentEntity, timer, u16) = 0x28U;
    }
    if (--MONSTER_PLANT_FIELD(g_CurrentEntity, tint_timer) == 0) {
        if (MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side) != 0) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(0, -1, 0, 0, 0x100, (s32) g_CurrentEntity->id);
            MONSTER_PLANT_FIELD(g_CurrentEntity, lunge_side)--;
        }
        MONSTER_PLANT_FIELD_AS(g_CurrentEntity, tint_timer, u16) = 0x28U;
    }
}

void monster_plant_damaged(void) {
    if ((u32) (++MONSTER_PLANT_FIELD_AS(g_CurrentEntity, hit_count, u16) & 0xFFFF) >= 4U) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_ScenarioFlags, 0x5B);
    }
    ENTITY_STATE_WORD(g_CurrentEntity) = (s32) MONSTER_PLANT_FIELD(g_CurrentEntity, saved_state);
    g_CurrentEntity->hit_state = 0;
    monster_plant_state_check();
}

void monster_plant_die(void) {
    extern s32 (*monster_plant_die_tbl[])();
    if ((u8) ((u8) g_CurrentEntity->sub_state) >= 3U) {
        return;
    }
    monster_plant_die_tbl[((u8) g_CurrentEntity->sub_state)]();
}

void monster_plant_die_begin(void) {
    void monster_plant_die_writhe();

    g_CurrentEntity->blend_counter = 0x1F;
    g_CurrentEntity->sub_state = 1;
    MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = 0;
    g_CurrentEntity->action_ticks_counter = (s16) (((((g_ApiCallPtr = api_rand)()) & 0x1F) + ((g_ApiCallPtr = api_rand)() & 0x1F)) + 1);
    g_CurrentEntity->health = -1;
    monster_plant_die_writhe();
}

void monster_plant_die_writhe(void) {
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        if ((--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)) == 0) {
            g_CurrentEntity->animation_id = 0xE;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 0x1F;
            goto play_fall_anim;
        }
        monster_plant_mode_sway();
        return;
    }
    if (1) {
play_fall_anim:
        if ((g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
            ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
            (g_ApiCallPtr = api_rand)();
            g_CurrentEntity->animation_id = (!((g_ApiCallPtr = api_rand)() & 1)) ? (7) : (0);
            g_CurrentEntity->animation_frame_id = (g_ApiCallPtr = api_rand)() & 0xF;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 0x1F;
            ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = (s16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0x32);
            MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 4;
            MONSTER_PLANT_FIELD(g_CurrentEntity, tint_timer) = 8;
        }
    }
}

void monster_plant_die_fade(void) {

    if ((u32) ((g_ApiCallPtr = api_rand)() & 0x3F) < (u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)) {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
    }
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_AS(g_CurrentEntity, sub_state, u8)++;
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, (s32) g_CurrentEntity->death_event_id);
    }
    if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
        if (--MONSTER_PLANT_FIELD(g_CurrentEntity, timer) == 0) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(1, 0, 0, 0, 0x100, (s32) g_CurrentEntity->id);
            MONSTER_PLANT_FIELD(g_CurrentEntity, timer) = 4U;
        }
        if (--MONSTER_PLANT_FIELD(g_CurrentEntity, tint_timer) == 0) {
            (g_ApiCallPtr = api_clut_fade_add_delta)(0, 1, 0, 0, 0x100, (s32) g_CurrentEntity->id);
            MONSTER_PLANT_FIELD(g_CurrentEntity, tint_timer) = 8U;
        }
    }
}

void monster_plant_state4_nop(void) {

}

void monster_plant_mode0_nop(void) {

}

void player_anim_plant_hold(void) {
    extern s32 (*player_plant_hold_tbl[])();
    player_plant_hold_tbl[g_PlayerEntity.action_state]();
}

void player_plant_hold_start(void) {
    g_PlayerEntity.action_state++;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.blend_counter = 0;
    (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
}

void player_plant_hold_squeezed(void) {
    (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
    (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x80);
    if ((u8) g_PlayerEntity.animation_frame_id >= 0x19U) {
        g_PlayerEntity.animation_frame_id = 0xA;
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0x50, 0x82, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0x82, 0x50, 0xA);
    }
}

void player_plant_hold_release(void) {
    (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
    if ((g_PlayerEntity.animation_frame_id != 0x27) && (g_PlayerEntity.health >= 0)) {

    } else {
        (*(s16 *)&g_PlayerEntity.state) = 1;
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0;
        g_PlayerEntity.hit_state = 0;
        if (((u8) g_PlayerEntity.animation_id) == 2) {
            g_PlayerEntity.angle += 0x800;
        }
        g_PlayerEntity.status_flags &= 0xF5;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x80);
}

void monster_plant_idle_fidget(void) {

    if (MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) == 0) {
        g_CurrentEntity->animation_id = ((g_ApiCallPtr = api_rand)() & 1) ? 8 : 3;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer) = (s16) ((g_ApiCallPtr = api_rand)() & 0x1F);
        if ((((u8) g_CurrentEntity->animation_id) == 3) && ((u8) g_CurrentEntity->animation_frame_id >= 0x14U)) {
            g_CurrentEntity->animation_frame_id = (u8) (g_CurrentEntity->animation_frame_id + 0xF6);
        }
        return;
    }
    MONSTER_PLANT_FIELD(g_CurrentEntity, fidget_timer)--;
}

void monster_plant_nop_unused(void) {

}
