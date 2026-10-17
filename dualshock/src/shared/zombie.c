/* zombie.c - Zombie: wander / chase / eat behaviors, grab and bite, push-backs and falls, deaths (head shot or
   kicked off), script (SCD) actions, and the player's grabbed animation.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/zombie.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* zombie data (defined in each overlay that has this enemy) */
extern s32 zombie_sca_info_ptr[3];
extern u8 zombie_health_tbl[];
extern u8 zombie_anim_id_tbl[];
extern s32 zombie_stagger_tbl;
extern u8 zombie_damage_action_tbl[][6];
extern u16 attack_dir_offset_tbl[];
extern u8 zombie_damage_tbl[];
extern s32 zombie_recovery_timer_tbl;

/* zombie sound effects: ids for api_sound_play_entity_se (the zombie's sound bank) */
#define ZOMBIE_SE_STEP_A      0  /* footsteps (walk frames 8 / 0x1A) */
#define ZOMBIE_SE_STEP_B      1
#define ZOMBIE_SE_STEP_C      2  /* shuffle / stagger step */
#define ZOMBIE_SE_EAT         3  /* eating a body / biting the player */
#define ZOMBIE_SE_GROAN       4
#define ZOMBIE_SE_FALL        5  /* body hits the floor */
#define ZOMBIE_SE_GORE        6  /* head shot off / leg blown off */
#define ZOMBIE_SE_VOMIT       7
#define ZOMBIE_SE_HEAD_BOUNCE 8  /* the detached head bouncing (zombie_head_bounce_physics) */
#define ZOMBIE_SE_HURT        9

/* ZombieWork: the zombie's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). ZOMBIE(e)->f, ZOMBIE_FIELD(e, f) and ZOMBIE_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct ZombieWork {
    /* +0x00 */ u8 attack_direction;    /* 0x16C: which attack animation (zombie_attack_anim_tbl index; facing test) */
    /* +0x01 */ u8 dir_control_flags;   /* 0x16D: room-collision bits of this frame (also given to the wander turn) */
    /* +0x02 */ u8 pad02[2];
    /* +0x04 */ s16 angle_turn_delta;   /* 0x170: turn step towards the target (added to angle) */
    /* +0x06 */ s16 walk_flags;         /* 0x172: bit 0 = wall/turn result copied while walking; non-zero = blocked */
    /* +0x08 */ s16 head_detached;      /* 0x174: 1 once the head (joint 2) has flag 0x40 = knocked off (seen in zombie_attack_main); zombie_update then moves it with zombie_head_bounce_physics */
    /* +0x0A */ s8 unused_176;          /* 0x176: only cleared in zombie_init */
    /* +0x0B */ s8 grab_struggle_timer; /* 0x177: grab hold time; the player's struggle input counts it down */
    /* +0x0C */ s8 unused_178;          /* 0x178: only cleared in zombie_init */
    /* +0x0D */ s8 wander_turn_state;   /* 0x179: given by address to api_entity_update_wander_turn */
    /* +0x0E */ u16 wander_angle;       /* 0x17A: wander direction (from the path search) */
    /* +0x10 */ s8 hit_count;           /* 0x17C: hits taken; falls down when it reaches hit_threshold; -128 (bit 7) = knocked down */
    /* +0x11 */ u8 hit_threshold;       /* 0x17D: random per zombie (zombie_init) */
    /* +0x12 */ u8 down_flags;          /* 0x17E: bit 2 = lying on the floor (falldown), bit 0 checked when hit */
    /* +0x13 */ u8 push_back_count;     /* 0x17F: short push-backs in a row */
    /* +0x14 */ u8 move_speed;          /* 0x180: walk speed (45) */
    /* +0x15 */ u8 turn_speed;          /* 0x181: wander turn speed (24; 14 after a push-back) */
    /* +0x16 */ u8 push_cooldown;       /* 0x182: frames until the next push-back may start (150) */
    /* +0x17 */ u8 pad17[1];
    /* +0x18 */ s32 saved_state;        /* 0x184: copy of the state word (state, ignore flag, behavior, action) */
    /* +0x1C */ u8 stagger_timer;       /* 0x188: poise; counts down per hit (zombie_stagger_tbl), falls when 0 */
    /* +0x1D */ s8 walk_decision_delay; /* 0x189: frames before the walk picks a new direction (60 after a grab) */
} ZombieWork;
#define ZOMBIE(e) ENEMY_WORK(e, ZombieWork)
#define ZOMBIE_FIELD(e, f) ENEMY_FIELD(e, ZombieWork, f)
#define ZOMBIE_MEMBER(e, f) ENEMY_MEMBER(e, ZombieWork, f)

/* a u8[32] table copied whole to the stack: an array in a struct (BYTES) so it can be assigned */
typedef BYTES(32) ZombieHitThresholds;
extern const ZombieHitThresholds zombie_hit_threshold_hard_tbl;

/* state 0: sets up a new zombie (health, sca info, animation, work fields), then state 1 */
void zombie_init(void) {
    extern u8 zombie_stagger_tbl_u8[] __asm__("zombie_stagger_tbl");
    void *joints;
    ZombieHitThresholds thresholds;
    { register ZombieHitThresholds *p = &thresholds; }
    thresholds = zombie_hit_threshold_hard_tbl;

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->sca_info = (s32) zombie_sca_info_ptr[0];
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    g_CurrentEntity->pool_buffer = (s32)g_LoadPointer;
    (g_ApiCallPtr = api_billboard_reserve)((void *)2);
    LIT(s32, sp_tmp1C) = 0xFFFF50;
    (g_ApiCallPtr = api_billboard_init)(LITP(void, sp_svec), ((void *) g_CurrentEntity->pool_buffer), 0x190, 0x190);
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x808080;
    (g_ApiCallPtr = api_billboard_init)(LITP(void, sp_svec), &g_CurrentEntity->shadow, 0x2BC, 0x384);
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        g_CurrentEntity->health = zombie_health_tbl[g_RandSeed & 0xF & 0xFFFF] - g_RandSeed % 22;
    } else {
        g_CurrentEntity->health = zombie_health_tbl[(g_ApiCallPtr = api_rand)() & 0xF] - API_AS(u32, api_rand)() % 22;
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 5) {
        g_CurrentEntity->action_state = 2;
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 0xB) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 4);
        g_CurrentEntity->sca_info = (s32) zombie_sca_info_ptr[2];
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
    }
    if (g_CurrentEntity->id == 1) {
        g_CurrentEntity->sca_info = (s32) zombie_sca_info_ptr[1];
    }
    ZOMBIE(g_CurrentEntity)->walk_flags = 0;
    ZOMBIE(g_CurrentEntity)->unused_176 = 0;
    ZOMBIE(g_CurrentEntity)->head_detached = 0;
    ZOMBIE(g_CurrentEntity)->dir_control_flags = 0;
    ZOMBIE(g_CurrentEntity)->wander_turn_state = 0;
    ZOMBIE(g_CurrentEntity)->hit_count = 0;
    ZOMBIE(g_CurrentEntity)->hit_threshold = thresholds.b[g_RandSeed & 0x1F & 0xFFFF];
    ZOMBIE(g_CurrentEntity)->stagger_timer = zombie_stagger_tbl_u8[g_RandSeed & 0x1F & 0xFFFF];
    ZOMBIE(g_CurrentEntity)->unused_178 = 0;
    ZOMBIE(g_CurrentEntity)->down_flags = 0;
    ZOMBIE(g_CurrentEntity)->move_speed = 0x2D;
    g_CurrentEntity->dc_double_step = 0;
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 0xC) {
        g_CurrentEntity->dc_double_step = 1;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xF0);
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 0xD) {
        g_CurrentEntity->dc_double_step = 1;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xF1);
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 0xE) {
        g_CurrentEntity->dc_double_step = 1;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xF0);
        g_CurrentEntity->health = (u16) (g_CurrentEntity->health + 0x64);
    }
    ZOMBIE(g_CurrentEntity)->turn_speed = 0x18;
    ZOMBIE(g_CurrentEntity)->push_back_count = 0;
    ZOMBIE(g_CurrentEntity)->push_cooldown = 0;
    g_CurrentEntity->blend_counter = 0;
    g_CurrentEntity->animation_id = zombie_anim_id_tbl[g_CurrentEntity->behavior_flags & 0xF & 0xFF];
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    (g_ApiCallPtr = api_joint_move)(NULL, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400);
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        g_CurrentEntity->pos_y = 1;
    }
    if (g_CurrentEntity->behavior_flags == 6) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->health = -1;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 4, 0);
    }
    if (g_CurrentEntity->behavior_flags == 0xA) {
        joints = ((void *) g_CurrentEntity->joints);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 4);
        FIELD_AT(joints, Joint, [9].flags, u8) = (u8) (FIELD_AT(joints, Joint, [9].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [10].flags, u8) = (u8) (FIELD_AT(joints, Joint, [10].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [11].flags, u8) = (u8) (FIELD_AT(joints, Joint, [11].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [12].flags, u8) = (u8) (FIELD_AT(joints, Joint, [12].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [13].flags, u8) = (u8) (FIELD_AT(joints, Joint, [13].flags, u8) & 0xFE);
        FIELD_AT(joints, Joint, [14].flags, u8) = (u8) (FIELD_AT(joints, Joint, [14].flags, u8) & 0xFE);
    }
    if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
        g_CurrentEntity->state = 8;
    }
}

/* copied to the stack by zombie_init and zombie_damaged (byte-identical to the PC version's table) */
const ZombieHitThresholds zombie_hit_threshold_hard_tbl = {
    { 3, 2, 3, 3, 2, 3, 4, 3, 2, 3, 3, 4, 3, 2, 3, 3,
      3, 2, 3, 3, 3, 3, 2, 3, 3, 2, 3, 4, 3, 4, 3, 2 }
};

/* every frame for each zombie (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void zombie_update(void) {
    void zombie_head_bounce_physics_a1() __asm__("zombie_head_bounce_physics");
    extern s32 (*zombie_state_tbl[])();
    void *joints;
    SVECTOR probe_neg = { -600, 0, 0, 0 };
    SVECTOR probe_pos = { 600, 0, 0, 0 };

    joints = ((void *) g_CurrentEntity->joints);
    if (g_UpdateFlags & 4) {
        zombie_state_tbl[g_CurrentEntity->state]();
        ZOMBIE(g_CurrentEntity)->saved_state = ENTITY_STATE_WORD(g_CurrentEntity);
        if (ZOMBIE(g_CurrentEntity)->push_cooldown != 0) {
            ZOMBIE(g_CurrentEntity)->push_cooldown--;
        }
        if (g_CurrentEntity->state != 5) {
            if ((g_CurrentEntity->behavior_flags & 0xF) != 0xB) {
                (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
                (g_ApiCallPtr = api_check_entity_collision)(ADDR_g_PlayerEntity, g_CurrentEntity);
                (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            }
            g_CurrentEntity->collision_flags &= 0xF7;
            if (g_CurrentEntity->behavior_flags & 2) {
                ZOMBIE(g_CurrentEntity)->dir_control_flags |= (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
                ZOMBIE(g_CurrentEntity)->wander_angle = LIT(u16, sp_tmp18);
                ZOMBIE(g_CurrentEntity)->dir_control_flags |= (g_ApiCallPtr = api_check_room_collision_two_point)(&probe_neg, &probe_pos);
            } else {
                ZOMBIE(g_CurrentEntity)->dir_control_flags |= (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
                ZOMBIE(g_CurrentEntity)->wander_angle = LIT(u16, sp_tmp18);
            }
        }
        if (ZOMBIE(g_CurrentEntity)->head_detached != 0) {
            zombie_head_bounce_physics_a1(JOINT_N(joints, 2), 6);
        }
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0, ((s16) g_CurrentEntity->angle));
    }
    joints = ((void *) g_CurrentEntity->joints);
    if ((JOINT(JOINT_N(joints, 4))->flags & 4) && (JOINT(JOINT_N(joints, 4))->world.t[1] == -100)) {
        if ((g_ApiCallPtr = api_is_point_in_quad)(JOINT_N_POS(joints, 4), g_CameraSwitchZones) != 0) {
            (g_ApiCallPtr = api_billboard_draw)(JOINT_N_POS(joints, 4), ((void *) g_CurrentEntity->pool_buffer), 0, JOINT(JOINT_N(joints, 4))->rotation.vy);
        }
    }
}

void zombie_state_check(void) {
    extern s32 (*zombie_behavior_tbl[])();
    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    zombie_behavior_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if (!(g_CurrentEntity->behavior_flags & 2 & 0xFF) && !(ZOMBIE(g_CurrentEntity)->hit_count & 0x80)) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
        (g_ApiCallPtr = api_entity_check_alert_range)(0xBB8);
    }
    (g_ApiCallPtr = api_entity_check_visual_range)(0x1194);
    LIT(s32, sp_tmp00) = g_PlayerEntity.pos_y - g_CurrentEntity->pos_y;
    if ((LIT(s32, sp_tmp00) >= -0x64) && (LIT(s32, sp_tmp00) < 0x65)) {

    } else {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        if (LIT(s32, sp_tmp00) < 0) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        } else {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x80);
        }
    }
    if ((g_CurrentEntity->behavior_flags != 7) && (g_CurrentEntity->behavior_flags != 5)) {

    } else {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    }
    if (ZOMBIE(g_CurrentEntity)->down_flags & 4 & 0xFF) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    }
}

void zombie_chase_player(void) {
    void zombie_action_dispatch();
    void zombie_move_update();
    void zombie_attack_main();
    void *unused_init = ((void *) g_CurrentEntity->joints);
    s32 unused;
    if (!(ZOMBIE(g_CurrentEntity)->down_flags & 4 & 0xFF) && !(ZOMBIE(g_CurrentEntity)->hit_count & 0x80)) {
        if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x2BC, 0x5DC, ADDR_g_PlayerPosX) != 0) {
            if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
                ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 5;
                g_CurrentEntity->action_state = 0;
                zombie_attack_main();
                return;
            }
        }
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        zombie_move_update();
    }
    zombie_action_dispatch();
}

void zombie_pushed_back(void) {
    void zombie_pushback_action();
    void zombie_move_update();
    void zombie_attack_main();
    void *unused_init = ((void *) g_CurrentEntity->joints);
    s32 unused;
    if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x200, 0x898, ADDR_g_PlayerPosX) != 0) {
        if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 5;
            g_CurrentEntity->action_state = 0;
            zombie_attack_main();
            return;
        }
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        zombie_move_update();
    }
    zombie_pushback_action();
}

void zombie_random_chase(void) {
    void zombie_action_dispatch();
    void zombie_move_update();
    void zombie_attack_main();
    void *joints;
    if (!(ZOMBIE(g_CurrentEntity)->down_flags & 4 & 0xFF) && !(ZOMBIE(g_CurrentEntity)->hit_count & 0x80) && !(g_CurrentEntity->collision_flags & 8 & 0xFF)) {
        joints = ((void *) g_CurrentEntity->joints);
        if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x2BC, 0x5DC, ADDR_g_PlayerPosX) != 0) {
            if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
                ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 5;
                g_CurrentEntity->action_state = 0;
                zombie_attack_main();
                return;
            }
        }
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        zombie_move_update();
    }
    zombie_action_dispatch();
}

void zombie_damaged(void) {
    extern s32 (*zombie_damage_behavior_tbl[])();
    u8 beh = 0;
    ZombieHitThresholds thresholds;
    { register ZombieHitThresholds *p = &thresholds; }
    thresholds = zombie_hit_threshold_hard_tbl;

    if (((u8) g_CurrentEntity->sub_state) == 0) {
        beh = g_CurrentEntity->behavior_flags & 0xF;
        if (ZOMBIE(g_CurrentEntity)->down_flags & 4 & 0xFF) {
            ENTITY_STATE_WORD(g_CurrentEntity) = (s32) ZOMBIE(g_CurrentEntity)->saved_state;
            g_CurrentEntity->hit_state = 0U;
            return;
        }
        if (ZOMBIE(g_CurrentEntity)->hit_count & 0x80) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 3);
            g_CurrentEntity->blend_counter = 0;
            g_CurrentEntity->hit_state = 1U;
            if (!(ZOMBIE(g_CurrentEntity)->down_flags & 1 & 0xFF)) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HURT, 0);
            }
            return;
        }
        if ((((u8) g_CurrentEntity->hit_state) & 0x78 & 0xFF) == 8) {
            if ((++ZOMBIE(g_CurrentEntity)->hit_count >= ((s8) ZOMBIE(g_CurrentEntity)->hit_threshold)) && !(g_CurrentEntity->behavior_flags & 2 & 0xFF)) {
                ZOMBIE(g_CurrentEntity)->hit_count = -0x80;
                ZOMBIE(g_CurrentEntity)->hit_threshold = thresholds.b[g_RandSeed & 0x1F & 0xFFFF];
            }
        }
        if (((((u8) g_CurrentEntity->hit_state) & 0x78 & 0xFF) == 0x10) && (g_CurrentEntity->action_state == 0)) {
            if ((--ZOMBIE(g_CurrentEntity)->stagger_timer == 0) && !(g_CurrentEntity->behavior_flags & 2 & 0xFF)) {
                ZOMBIE(g_CurrentEntity)->hit_count = -0x80;
            }
        }
        g_CurrentEntity->action_behavior = zombie_damage_action_tbl[((g_CurrentEntity->behavior_flags & 2 & 0xFF) >> 1) & 0xFF][(((u8) g_CurrentEntity->hit_state) & 7 & 0xFF) - 1];
        if (((((u8) g_CurrentEntity->hit_state) & 7 & 0xFF) - 1) & 2) {
            g_CurrentEntity->action_behavior += (API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) >> 10) & 1;
        }
        if ((beh == 7) || (beh == 5)) {
            g_CurrentEntity->action_behavior = 7U;
            if (((u8) g_CurrentEntity->animation_id) == 0xD) {
                g_CurrentEntity->action_state = 1U;
            }
        }
        g_CurrentEntity->sub_state = 1U;
    }
    zombie_damage_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void zombie_die(void) {
    void zombie_headless_walk();
    void zombie_dead_animation();
    void *joints;

    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->sub_state = 1U;
        g_CurrentEntity->action_behavior = 0U;
        if (!(g_CurrentEntity->behavior_flags & 2 & 0xFF) && !(ZOMBIE(g_CurrentEntity)->hit_count & 0x80) && (g_CurrentEntity->behavior_flags != 5)) {

        } else {
            g_CurrentEntity->action_behavior = 2U;
        }
        if ((((u8) g_CurrentEntity->hit_state) & 7 & 0xFF) == 4) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) == 0) {
                g_CurrentEntity->action_behavior = 1U;
            }
        }
        if (g_CurrentEntity->action_behavior == 1) {
            joints = ((void *) g_CurrentEntity->joints);
            if ((FIELD_AT(joints, Joint, [2].flags, u8) & 0x40 & 0xFF) && (g_RandSeed & 1 & 0xFFFF)) {
                g_CurrentEntity->action_behavior = 3U;
            }
        }
        ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags & 0xFB);
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->move_speed_current = 0x14U;
        if (((u8) g_CurrentEntity->timing_control) == 1) {
            if (g_CurrentEntity->animation_frame_id == 8) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0U);
            }
            if (g_CurrentEntity->animation_frame_id == 0x1A) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_A, 0U);
            }
        }
        zombie_dead_animation();
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    case 1:
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->move_speed_current = 0x14U;
        if ((g_CurrentEntity->animation_frame_id == 0x12) && (((u8) g_CurrentEntity->timing_control) == 1)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_A, 0U);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 5U) {
            g_CurrentEntity->move_speed_current = (u16) (((u16) g_CurrentEntity->move_speed_current) + 0x5A);
        }
        zombie_dead_animation();
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        return;
    case 2:
        g_CurrentEntity->animation_id = 0x10;
        zombie_dead_animation();
        return;
    case 3:
        zombie_headless_walk();
        return;
    }
}

void zombie_state4_nop(void) {

}

void zombie_attack(void) {
    void zombie_attack_main();
    zombie_attack_main();
}

void zombie_action_dispatch(void) {
    extern s32 (*zombie_action_tbl[])();
    zombie_action_tbl[g_CurrentEntity->action_behavior]();
}

void zombie_pushback_action(void) {
    void zombie_pushback_idle();
    void zombie_pushback_stagger();
    if (g_CurrentEntity->action_behavior == 0) {
        zombie_pushback_idle();
        return;
    }
    zombie_pushback_stagger();
}

void zombie_move_update(void) {
    extern s32 (*zombie_move_behavior_tbl[])();

    if (ZOMBIE(g_CurrentEntity)->walk_flags != 0) {
        (g_ApiCallPtr = api_zone_path_find)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, &g_CurrentEntity->waypoint_x, &g_CurrentEntity->waypoint_z);
    }
    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    zombie_move_behavior_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
}

void zombie_walk1(void) {

    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_tmp1C) = 6;
    LIT(s32, sp_tmp10) = (s32) g_CurrentEntity->action_behavior;
    if (ZOMBIE(g_CurrentEntity)->walk_flags == 0) {
        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_entity_pathfind_update)();
        if (((s32) LIT(s32, sp_tmp14) >> 1) == 0) {
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (ZOMBIE(g_CurrentEntity)->walk_flags & ~1);
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (((u16) ZOMBIE(g_CurrentEntity)->walk_flags) | ((u16) LIT(s32, sp_tmp14) & 1));
        }
    }
    if (ZOMBIE(g_CurrentEntity)->hit_count & 0x80) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        return;
    }
    if ((g_CurrentEntity->action_behavior != 0) && (g_CurrentEntity->action_behavior != 1)) {
        LIT(s32, sp_tmp1C) = 7;
        (g_ApiCallPtr = api_entity_update_wander_turn)(ZOMBIE(g_CurrentEntity)->wander_angle, &ZOMBIE(g_CurrentEntity)->dir_control_flags, &ZOMBIE(g_CurrentEntity)->wander_turn_state, ZOMBIE(g_CurrentEntity)->turn_speed, 0x3C);
    }
    if (LIT(s32, sp_tmp00) < 0xBB8) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x400) != 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 3;
            return;
        }
    }
    if ((ZOMBIE(g_CurrentEntity)->walk_flags != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        if (LIT(s32, sp_tmp10) != 2) {
            g_CurrentEntity->action_state = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if (!(((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF)) {
        return;
    }
    if (--ZOMBIE(g_CurrentEntity)->walk_decision_delay != 0) {
        return;
    }
    if (LIT(s32, sp_tmp14) & 1) {
        if (LIT(s32, sp_tmp18) != 2) {
            g_CurrentEntity->action_state = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if (LIT(s32, sp_tmp00) < 0x4B0) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x2C8) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 4;
        }
    }
}

void zombie_check_player_distance(void) {

    g_CurrentEntity->action_ticks_counter++;
    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    LIT(s32, sp_vec_vy) = 0;
    g_CurrentEntity->angle = ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 4U));
    if (((g_ApiCallPtr = api_entity_pathfind_update)() & 1) && (LIT(s32, sp_tmp00) < 0x2328)) {
        g_CurrentEntity->behavior_flags--;
        if (g_CurrentEntity->behavior_flags == 7) {
            g_CurrentEntity->behavior_flags = 4U;
        }
        return;
    }
    if ((g_CurrentEntity->action_behavior == 0) && (LIT(s32, sp_tmp00) >= 0xBB8)) {
        return;
    }
    g_CurrentEntity->behavior_flags--;
    if (g_CurrentEntity->behavior_flags == 7) {
        g_CurrentEntity->behavior_flags = 4U;
    }
}

void zombie_slow_walk_alt(void) {

    LIT(s32, sp_tmp08) = __builtin_abs(((s16) g_CurrentEntity->waypoint_x) - g_CurrentEntity->pos_x) + __builtin_abs(((s16) g_CurrentEntity->waypoint_z) - (g_CurrentEntity->pos_z));
    LIT(s32, sp_tmp04) = (s32) g_CurrentEntity->action_behavior;
    if (ZOMBIE(g_CurrentEntity)->walk_flags == 0) {
        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_entity_pathfind_update)();
        if (((s32) LIT(s32, sp_tmp14) >> 1) == 0) {
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (ZOMBIE(g_CurrentEntity)->walk_flags & ~1);
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (((u16) ZOMBIE(g_CurrentEntity)->walk_flags) | ((u16) LIT(s32, sp_tmp14) & 1));
        }
        return;
    }
    if (LIT(s32, sp_tmp04) != 1) {
        g_CurrentEntity->action_state = 0;
        g_CurrentEntity->blend_counter = 3;
    }
    g_CurrentEntity->action_behavior = 1U;
    g_CurrentEntity->sub_state = 0;
}

void zombie_idling(void) {
    if (g_CurrentEntity->action_behavior != 0) {
        ENTITY_ACTION_U16(g_CurrentEntity) = 0;
    }
    (g_ApiCallPtr = api_entity_pathfind_update)();
}

void zombie_walk2(void) {

    LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
    LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_tmp1C) = 6;
    LIT(s32, sp_tmp18) = (s32) g_CurrentEntity->action_behavior;
    if (ZOMBIE(g_CurrentEntity)->walk_flags == 0) {
        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_entity_pathfind_update)();
        if (((s32) LIT(s32, sp_tmp14) >> 1) == 0) {
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (ZOMBIE(g_CurrentEntity)->walk_flags & ~1);
            ZOMBIE(g_CurrentEntity)->walk_flags = (s16) (((u16) ZOMBIE(g_CurrentEntity)->walk_flags) | ((u16) LIT(s32, sp_tmp14) & 1));
        }
    }
    if ((g_CurrentEntity->action_behavior != 0) && (g_CurrentEntity->action_behavior != 1)) {
        LIT(s32, sp_tmp1C) = 7;
        (g_ApiCallPtr = api_entity_update_wander_turn)(ZOMBIE(g_CurrentEntity)->wander_angle, &ZOMBIE(g_CurrentEntity)->dir_control_flags, &ZOMBIE(g_CurrentEntity)->wander_turn_state, ZOMBIE(g_CurrentEntity)->turn_speed, 0x3C);
    }
    if (ZOMBIE(g_CurrentEntity)->hit_count & 0x80) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        return;
    }
    if (LIT(s32, sp_tmp00) < 0xBB8) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x400) != 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 3;
            return;
        }
    }
    if ((g_CurrentEntity->collision_flags & 8 & 0xFF) && (LIT(s32, sp_tmp00) < 0x7D0)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x200) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 6;
            if (((u8) g_PlayerEntity.hit_state) != 0) {
                g_CurrentEntity->action_state = 2;
            }
            return;
        }
    }
    if ((ZOMBIE(g_CurrentEntity)->walk_flags != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        if (LIT(s32, sp_tmp18) != 2) {
            g_CurrentEntity->action_state = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if (!(((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF)) {
        return;
    }
    if (--ZOMBIE(g_CurrentEntity)->walk_decision_delay != 0) {
        return;
    }
    if (LIT(s32, sp_tmp14) & 1) {
        if (LIT(s32, sp_tmp18) != 2) {
            g_CurrentEntity->action_state = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if (!(g_CurrentEntity->collision_flags & 8 & 0xFF) && (LIT(s32, sp_tmp00) < 0x4B0)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (void *)0x2C8) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 4;
        }
    }
}

void zombie_idle(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0x7F) + 0x32);
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        }
        return;
    case 2:
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0xF) + 8);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
            g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0xF) + 8);
        }
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0xC);
        return;
    case 4:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 5U;
            g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0xF) + 4);
        }
        g_CurrentEntity->angle = (u16) (ENTITY_AS(g_CurrentEntity, angle, s16) - 0x18);
        return;
    case 5:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 6U;
            g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0xF) + 8);
        }
        g_CurrentEntity->angle = (u16) (ENTITY_AS(g_CurrentEntity, angle, s16) - 0x20);
        return;
    case 6:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 7U;
            g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0xF) + 4);
        }
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x18);
        return;
    case 7:
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void zombie_slow_walk(void) {

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->move_speed_current = 0x14;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0x7F) + 0x12C);
        LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
        LIT(u16, sp_svec_vx) = 0x1388;
        LIT(u16, sp_svec_vz) = 0;
        LIT(s16, sp_svec_vy) = 0;
        (g_ApiCallPtr = api_RotMatrixY)(ENTITY_AS(g_CurrentEntity, angle, s16) + 8, LITP(void, sp_matrix));
        (g_ApiCallPtr = api_ApplyMatrixSV)(ADDR_sp_matrix, LITP(void, sp_svec), LITP(void, sp_svec));
        ENTITY_AS(g_CurrentEntity, waypoint_x, s16) = (s16) (LIT(s32, sp_svec_vx) + ENTITY_AS(g_CurrentEntity, pos_x, u16));
        ENTITY_AS(g_CurrentEntity, waypoint_z, s16) = (s16) (LIT(s32, sp_svec_vz) + ENTITY_AS(g_CurrentEntity, pos_z, u16));
    }
    if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
        if (g_CurrentEntity->animation_frame_id == 0xD) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, NULL);
        }
        if (g_CurrentEntity->animation_frame_id == 0x15) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_C, NULL);
        }
    }
    (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400U);
    if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 0x200;
    }
    LIT(s32, sp_tmp1C) = 7;
    (g_ApiCallPtr = api_entity_update_wander_turn)(ZOMBIE(g_CurrentEntity)->wander_angle, &ZOMBIE(g_CurrentEntity)->dir_control_flags, &ZOMBIE(g_CurrentEntity)->wander_turn_state, ZOMBIE(g_CurrentEntity)->turn_speed, 0x3C);
    if ((u8) g_CurrentEntity->animation_frame_id >= 0x10U) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
    }
}

void zombie_chase_walk(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400));
        break;
    case 2:
        g_CurrentEntity->move_speed_current = (s16) ZOMBIE(g_CurrentEntity)->move_speed;
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0x7F) + 0xA);
        g_CurrentEntity->next_turn_timer = 0U;
        /* fallthrough */
    case 3:
        if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
            if (g_CurrentEntity->animation_frame_id == 8) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, NULL);
            }
            if (g_CurrentEntity->animation_frame_id == 0x1D) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, NULL);
            }
        }
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->next_turn_timer = (u16) ((g_RandSeed & 0xF) + 0x14);
            g_CurrentEntity->action_ticks_counter = (u16) (g_CurrentEntity->next_turn_timer + (g_RandSeed & 0x7F));
        }
        (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400);
        if (ENTITY_AS(g_CurrentEntity, dc_double_step, u8) != 0) {
            (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400);
        }
        g_CurrentEntity->move_speed_current = 0x2D;
        if ((u8) ZOMBIE(g_CurrentEntity)->move_speed >= 0x2EU) {
            g_CurrentEntity->move_speed_current = (s16) ZOMBIE(g_CurrentEntity)->move_speed;
        }
        if ((s16) g_CurrentEntity->next_turn_timer != 0) {
            LIT(s32, sp_tmp1C) = 7;
            (g_ApiCallPtr = api_entity_update_wander_turn)(ZOMBIE(g_CurrentEntity)->wander_angle, &ZOMBIE(g_CurrentEntity)->dir_control_flags, &ZOMBIE(g_CurrentEntity)->wander_turn_state, -(ZOMBIE(g_CurrentEntity)->turn_speed * 2), 0x3C);
            g_CurrentEntity->move_speed_current = 0xA;
            ENTITY_AS(g_CurrentEntity, next_turn_timer, s16)--;
        }
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
        if ((((u8) g_PlayerEntity.hit_state) != 0) && (LIT(s32, sp_tmp00) < 0x320)) {
            g_CurrentEntity->sub_state = 1;
            g_CurrentEntity->action_state++;
            g_CurrentEntity->action_ticks_counter = 0x3CU;
            g_CurrentEntity->animation_frame_id = 0U;
            ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
            g_CurrentEntity->animation_id = 0;
            g_CurrentEntity->blend_counter = 3;
            g_CurrentEntity->move_speed_current = 0;
        }
        break;
    case 4:
        (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400);
        if (ENTITY_AS(g_CurrentEntity, dc_double_step, u8) != 0) {
            (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x400);
        }
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->action_state = 0U;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
    if (ENTITY_AS(g_CurrentEntity, dc_double_step, u8) != 0) {
        (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void zombie_attack_main(void) {
    u8 zombie_grab_struggle_input(void);
    extern struct { u8 a; u8 b; u8 c; } zombie_attack_anim_tbl[];
    extern s32 (*player_bite_blood_tint_tbl[])();
    void *joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GROAN, 0U);
        ZOMBIE_FIELD(g_CurrentEntity, attack_direction) = (u8) ((g_ApiCallPtr = api_is_facing_toward_entity)(LITP(void, g_PlayerEntity)) + (g_CurrentEntity->behavior_flags & 2));
        g_CurrentEntity->animation_id = zombie_attack_anim_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].a;
        g_PlayerEntity.animation_id = zombie_attack_anim_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].b;
        g_PlayerEntity.action_ticks_counter = attack_dir_offset_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)];
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        (g_ApiCallPtr = api_snap_player_to_grab_position)(LITP(void, g_PlayerEntity));
        g_PlayerEntity.hit_state = 1;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(5, 0, 0, 0);
        g_PlayerEntity.angle = g_CurrentEntity->angle;
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xF, 0U);
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)6, 0xFAU, 0);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        break;
    case 2:
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0U;
        ZOMBIE(g_CurrentEntity)->grab_struggle_timer = 0x69;
        if (((s16) g_PlayerEntity.next_turn_timer) != 0) {
            ZOMBIE(g_CurrentEntity)->grab_struggle_timer = 0x1E;
        }
        /* fallthrough */
    case 3:
        if (((s16) g_CurrentEntity->action_ticks_counter++ % 19) == 0) {
            (g_ApiCallPtr = api_pad_vibrate_large)((void *)6, 0xFAU, 0);
            (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xC, 0U);
            g_PlayerEntity.health -= zombie_damage_tbl[g_CurrentEntity->behavior_flags & 0xF];
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_EAT, 0U);
            joints = ((void *) g_CurrentEntity->joints);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0U, (s16) g_PlayerEntity.angle + 0x800, g_DeadMoveValue, JOINT_N_POS(joints, 2), 0);
            if (((s16) ((u16) g_PlayerEntity.health) < 0) && (ZOMBIE_FIELD(g_CurrentEntity, attack_direction) & 2 & 0xFF)) {
                g_PlayerEntity.health = 1;
            }
        }
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        ZOMBIE(g_CurrentEntity)->grab_struggle_timer = ZOMBIE(g_CurrentEntity)->grab_struggle_timer - (zombie_grab_struggle_input() + 1);
        if (ZOMBIE(g_CurrentEntity)->grab_struggle_timer < 0) {
            g_CurrentEntity->action_state = 4U;
            g_PlayerEntity.action_state = 3;
        }
        if ((s16) ((u16) g_PlayerEntity.health) < 0) {
            ZOMBIE(g_CurrentEntity)->walk_decision_delay = 0x3C;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            joints = ((void *) g_CurrentEntity->joints);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0U, 0x200, JOINT_N_WORLD(joints, 2), LITP(void, sp_vec), 0);
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
            g_CurrentEntity->action_behavior = g_CurrentEntity->action_behavior - ((g_CurrentEntity->behavior_flags & 2) >> 1) * 5;
            *(u8 *)(s32 *)&g_PlayerEntity.state = 1;
            g_PlayerEntity.action_behavior = 0xC8;
            player_bite_blood_tint_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)](g_PlayerEntity.joints);
        }
        break;
    case 4:
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
            /* a crawling zombie (behavior_flags & 2, bit 1) that grabbed Jill from this side: Jill kicks its head off
               (state 8: joint 2 = the head detaches, spins and bounces; the zombie dies) - confirmed in game */
            if ((g_PlayerEntity.id & 1 & 0xFF) && (ZOMBIE_FIELD(g_CurrentEntity, attack_direction) == 2)) {
                g_CurrentEntity->action_state = 8U;
                goto head_kicked_off;
            }
            g_CurrentEntity->action_state = 6U;
            goto bite_hold;
        }
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)3, 0xFAU, 0);
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 1, 6, 0);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xF5);
            g_CurrentEntity->hit_state = 1;
        }
        break;
    case 6:
bite_hold:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        if (g_CurrentEntity->animation_frame_id == zombie_attack_anim_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].c) {
            joints = ((void *) g_CurrentEntity->joints);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 2), 0x1EU, 0, (void *)3);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)3, 0U, 0, JOINT_N_WORLD(joints, 2), LITP(void, sp_vec), 0);
            SCRATCH_VEC->x = (s32) (SCRATCH_VEC->x + 0x1F4);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)4, 0U, 0x800, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)4, 1U, 0x5E8, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)4, 2U, 0x9F4, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)4, 4U, 0xB84, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)4, 2U, 0xDB8, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 3), 0x30U, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 6), 0x30U, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GORE, 0U);
        }
        break;
    case 7:
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, (void *)0xFFFF50);
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        g_CurrentEntity->death_timer = 0x46;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 2, 3);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xE);
        g_CurrentEntity->health = -1;
        g_CurrentEntity->hit_state = 1;
        break;
    case 8:
head_kicked_off:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        joints = ((void *) g_CurrentEntity->joints);
        if (g_CurrentEntity->animation_frame_id == zombie_attack_anim_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].c) {
            FIELD_AT(joints, Joint, [2].flags, u8) = (u8) (FIELD_AT(joints, Joint, [2].flags, u8) | 0x88);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 2), 0x30U, 0x80820, (void *)0x606060);
            JOINT(JOINT_N(joints, 2))->rotation.vx = -0x12C;
            JOINT(JOINT_N(joints, 2))->rotation.vy = 0xFA;
            JOINT(JOINT_N(joints, 2))->fx_frame = 0;
            JOINT((JOINT_N(joints, 2)))->fx_state = 0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0U, 0, JOINT_N_WORLD(joints, 2), LITP(void, sp_vec), 0);
        }
        if (FIELD_AT(joints, Joint, [2].flags, u8) & 0x40 & 0xFF) {
            ZOMBIE(g_CurrentEntity)->head_detached = 1;
        }
        if (g_PlayerEntity.animation_frame_id == 6) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_VOMIT, 0U);
        }
        break;
    case 9:
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, (void *)0xFFFF50);
        g_CurrentEntity->death_timer = 0x46;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 2, 3);
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xE);
        g_CurrentEntity->health = -1;
        g_CurrentEntity->hit_state = 1;
    }
}

void player_bite_blood_tint_0(s32 joints) {
    LIT(s32, sp_tmp14) = (s32)g_CurrentEntity;
    g_CurrentEntity = (Entity *)ADDR_g_PlayerEntity;
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 12), 0x30, 0x80820, 0x606060);
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 1), 0x30, 0x80820, 0x606060);
    g_CurrentEntity = LIT(Entity *, sp_tmp14);
}

void player_bite_blood_tint_1(s32 joints) {
    LIT(s32, sp_tmp14) = (s32)g_CurrentEntity;
    g_CurrentEntity = (Entity *)ADDR_g_PlayerEntity;
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 12), 0x30, 0x80820, 0x606060);
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 1), 0x30, 0x80820, 0x606060);
    g_CurrentEntity = LIT(Entity *, sp_tmp14);
}

void player_bite_blood_tint_2(s32 joints) {
    LIT(s32, sp_tmp14) = (s32)g_CurrentEntity;
    g_CurrentEntity = (Entity *)ADDR_g_PlayerEntity;
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 4), 0x30, 0x80820, 0x606060);
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 5), 0x30, 0x80820, 0x606060);
    g_CurrentEntity = LIT(Entity *, sp_tmp14);
}

void player_bite_blood_tint_3(s32 joints) {
    LIT(s32, sp_tmp14) = (s32)g_CurrentEntity;
    g_CurrentEntity = (Entity *)ADDR_g_PlayerEntity;
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 8), 0x30, 0x80820, 0x606060);
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), 0x30, 0x80820, 0x606060);
    g_CurrentEntity = LIT(Entity *, sp_tmp14);
}

void fast_player_facing(void) {
    u8 unused_init = 169;

    if (g_CurrentEntity->action_state == 0) {
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->hit_state = 0;
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) != 0) {

        }
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0x3F) + 0x1E);
    }
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x54);
    if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) || (LIT(s32, sp_tmp1C) == 0)) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 2);
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
    } else {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C);
    }
}

void zombie_pushback_idle(void) {
    g_CurrentEntity->animation_id = 9;
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
}

void benddown_and_eat(void) {
    s32 pl_joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = 0U;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        g_CurrentEntity->blend_counter = 3;
        return;
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state++;
        }
        return;
    case 2:
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_state++;
        g_CurrentEntity->blend_counter = 3;
        ENTITY_AS(g_CurrentEntity, animation_id, u8) = 0xBU;
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20));
        return;
    case 4:
        g_CurrentEntity->action_state++;
        ENTITY_AS(g_CurrentEntity, animation_id, u8)++;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0x78U;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (!(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
            LIT(s32, sp_vec_vx) = 0x320;
            LIT(s32, sp_vec_vy) = -0x12C;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_EAT, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 0xE) {
            pl_joints = g_PlayerEntity.joints;
            (g_ApiCallPtr = api_joint_apply_color_tint)(pl_joints, 0x30, 0x80820, (void *)0x606060);
            LIT(s32, sp_vec_vx) = 0x64;
            LIT(s32, sp_vec_vy) = -0x2BC;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(pl_joints, 0), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(pl_joints, 2), 0x30, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(pl_joints, 2), ADDR_sp_vec, 0);
        }
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state++;
            ENTITY_AS(g_CurrentEntity, animation_id, u8)++;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 3;
        }
        return;
    case 6:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
        }
    }
}

void turn_towards_player(void) {
    extern struct { s16 a; s16 b; s16 c; } zombie_attack_data_tbl[];

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->move_speed_current = 0x2D;
        (g_ApiCallPtr = api_entity_add_speed_xz)(zombie_attack_data_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].a);
        g_CurrentEntity->action_ticks_counter = zombie_attack_data_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].b;
    }
    if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
        if (g_CurrentEntity->animation_frame_id == 8) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 0x1D) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
        }
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle - zombie_attack_data_tbl[ZOMBIE_FIELD(g_CurrentEntity, attack_direction)].c);
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 4;
    }
    g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x + g_CurrentEntity->speed_vx);
    g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + g_CurrentEntity->speed_vy);
    g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z + g_CurrentEntity->speed_vz);
}

void zombie_pushback_stagger(void) {
    void zombie_leg_step_speed(u8 part);
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0xE;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if (g_CurrentEntity->animation_frame_id == 5) {
            g_CurrentEntity->action_state = 2U;
            g_CurrentEntity->blend_counter = 3;
            g_CurrentEntity->move_speed_current = 0x14;
            g_CurrentEntity->action_ticks_counter = 1U;
        }
        break;
    case 2:
        if ((g_CurrentEntity->animation_frame_id == 7 || g_CurrentEntity->animation_frame_id == 0x18) && ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_C, 0);
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        zombie_leg_step_speed(1);
        if ((FIELD_AT(ENTITY_AS(g_CurrentEntity, joints, void *), Joint, [4].flags, u8) & 4) && (g_CurrentEntity->animation_frame_id >= 0x12)) {
            return;
        }
        LIT(s32, sp_vec_vx) = ENTITY_AS(g_CurrentEntity, waypoint_x, s16);
        LIT(s32, sp_vec_vz) = ENTITY_AS(g_CurrentEntity, waypoint_z, s16);
        LIT(s32, sp_vec_vy) = 0;
        ZOMBIE(g_CurrentEntity)->angle_turn_delta = (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 8);
        if (ZOMBIE(g_CurrentEntity)->dir_control_flags & 0x80) {
            ZOMBIE(g_CurrentEntity)->angle_turn_delta *= -1;
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->action_ticks_counter = (g_RandSeed & 0x1F) + 0x1E;
        }
        g_CurrentEntity->angle += ((u16) ZOMBIE(g_CurrentEntity)->angle_turn_delta);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        break;
    case 3:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        zombie_leg_step_speed(1);
        g_CurrentEntity->angle += ((u16) ZOMBIE(g_CurrentEntity)->angle_turn_delta);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 3U;
        }
    }
    if (g_CurrentEntity->collision_flags & 8) {
        g_CurrentEntity->behavior_flags++;
    }
}

void zombie_vomiting(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 5;
        LIT(s32, sp_vec_vx) = 0x1F4;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = -0x9C4;
        (g_ApiCallPtr = api_effect_create_billboard)(0x20, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_VOMIT, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        g_CurrentEntity->action_state++;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed & 0x1F) + 0x14);
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

void zombie_eating(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = (u8) (g_RandSeed & 0x1F);
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0x1E;
        g_CurrentEntity->action_ticks_counter = (s16) (((s16) g_RandSeed & 0xF) + 0x2D);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            if (g_RandSeed & 0x01000001) {
                g_CurrentEntity->animation_id = 0x1D;
            } else {
                g_CurrentEntity->animation_id = 0x1E;
            }
        }
        if (g_CurrentEntity->animation_frame_id == 0x19) {
            LIT(s32, sp_vec_vx) = 0x320;
            LIT(s32, sp_vec_vy) = -0x12C;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_EAT, 0);
        }
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
        if (LIT(s32, sp_tmp00) < 0xBB8) {
            g_CurrentEntity->action_state++;
        }
        break;
    case 2:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xD;
        /* fallthrough */
    case 3:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            g_CurrentEntity->behavior_flags = 0;
            if (g_StageRoom == STAGE_ROOM(4, 5)) {
                g_CurrentEntity->behavior_flags = 4;
            }
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->pos_y = 0;
        }
    }
}

void zombie_falldown(void) {
    u8 unused_init = 222;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x14;
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_FALL, 0);
        g_CurrentEntity->pos_y = 1;
        /* fallthrough */
    case 1:
        ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags & 0xFB);
        g_CurrentEntity->hit_state = 1;
        ZOMBIE(g_CurrentEntity)->hit_count = (s8) (ZOMBIE(g_CurrentEntity)->hit_count | ~0x7F);
        if ((u8) g_CurrentEntity->animation_frame_id < 4U) {
            ZOMBIE(g_CurrentEntity)->hit_count = (s8) (((u8) ZOMBIE(g_CurrentEntity)->hit_count) & 0x7F);
            ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags | 4);
            g_CurrentEntity->hit_state = 0;
        }
        if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
            if (g_CurrentEntity->animation_frame_id == 8) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
            }
            if (g_CurrentEntity->animation_frame_id == 0x1A) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_A, 0);
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->hit_state = 0;
            g_CurrentEntity->action_ticks_counter = (u16) (*(u8 *)((u8 *)&zombie_recovery_timer_tbl + ((g_RandSeed & 0xF & 0xFFFF))) * 0x1E);
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    case 2:
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->blend_counter = 0;
            g_CurrentEntity->hit_state = 1;
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_FALL, 0);
        }
        return;
    case 3:
        ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags & 0xFB);
        g_CurrentEntity->hit_state = 1;
        ZOMBIE(g_CurrentEntity)->hit_count = (s8) (ZOMBIE(g_CurrentEntity)->hit_count | ~0x7F);
        if ((u8) g_CurrentEntity->animation_frame_id >= 0x1BU) {
            ZOMBIE(g_CurrentEntity)->hit_count = (s8) (((u8) ZOMBIE(g_CurrentEntity)->hit_count) & 0x7F);
            ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags | 4);
            g_CurrentEntity->hit_state = 0;
        }
        if ((g_ApiCallPtr = api_joint_move)(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            if (ZOMBIE(g_CurrentEntity)->stagger_timer != 0) {
                ZOMBIE(g_CurrentEntity)->hit_count = 0;
            } else {
                ZOMBIE(g_CurrentEntity)->stagger_timer = (u8) *(u8 *)((u8 *)&zombie_stagger_tbl + ((g_RandSeed & 0x1F & 0xFFFF)));
                ZOMBIE(g_CurrentEntity)->hit_count = (s8) (((u8) ZOMBIE(g_CurrentEntity)->hit_count) & 0x7F);
            }
            g_CurrentEntity->hit_state = 0;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags & 0xFB);
            g_CurrentEntity->pos_y = 0;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
    }
}

void zombie_headless_walk(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags | 1);
        g_CurrentEntity->move_speed_current = 0x2D;
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0U;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x20);
        if (((u8) g_CurrentEntity->timing_control) == 1) {
            if (g_CurrentEntity->animation_frame_id == 8) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
            }
            if (g_CurrentEntity->animation_frame_id == 0x1D) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void dc_standup_lunge(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0x1D;
        g_CurrentEntity->action_ticks_counter = (s16) ((g_RandSeed & 0xF) + 0x2D);
        /* fallthrough */
    case 1:
        if (g_CurrentEntity->animation_frame_id == 0x14) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GROAN, 0);
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->move_speed_current = 0x1E7;
            (g_ApiCallPtr = api_entity_add_speed_xz)(0);
            g_CurrentEntity->move_speed_current = 0x3DF;
            (g_ApiCallPtr = api_entity_add_speed_xz)(0x400);
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
            g_CurrentEntity->behavior_flags = 0;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->sca_info = (s32) zombie_sca_info_ptr[0];
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFB);
        }
    }
}

void short_push_back(void) {
    void *joints;

    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        g_CurrentEntity->animation_id = (g_CurrentEntity->action_behavior == 0) ? 4 : 5;
    }
    g_CurrentEntity->move_speed_current = 0xF;
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            LIT(s32, sp_vec_vx) = 0x64;
            LIT(s32, sp_vec_vy) = -0xA3C;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
        }
        joints = ((void *) g_CurrentEntity->joints);
        if (((((u8) g_CurrentEntity->hit_state) & 7 & 0xFF) == 3) && !(FIELD_AT(joints, Joint, [4].flags, u8) & 4 & 0xFF) && ((0x40 >> (g_RandSeed & 7 & 0xFFFF)) & 1)) {
            FIELD_AT(joints, Joint, [4].flags, u8) = (u8) (FIELD_AT(joints, Joint, [4].flags, u8) | 0xC);
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 4), LITP(void, sp_vec), 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, NULL, JOINT_N_POS(joints, 4), 0);
            FIELD_AT(joints, Joint, [5].flags, u8) = (u8) (FIELD_AT(joints, Joint, [5].flags, u8) | 0x10);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 4), 0x30, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 3), 0x30, 0x80820, (void *)0x606060);
        }
        if (ZOMBIE(g_CurrentEntity)->push_cooldown == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HURT, 0);
            ZOMBIE(g_CurrentEntity)->push_cooldown = 0x96;
        }
    }
    if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
        g_CurrentEntity->action_state++;
        if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
            if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 5) {
                g_CurrentEntity->behavior_flags = 0U;
            }
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        }
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_CurrentEntity->hit_state = 0U;
    }
    if (((u8) g_CurrentEntity->hit_state) & 1 & 0xFF) {
        if ((++ZOMBIE(g_CurrentEntity)->push_back_count & 0xFF) == 2) {
            ZOMBIE(g_CurrentEntity)->move_speed = 0x14;
            ZOMBIE(g_CurrentEntity)->turn_speed = 0xE;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
}

void push_and_stagger(void) {
    u16 push_dir;

    if (g_CurrentEntity->action_behavior == 2) {
        g_CurrentEntity->animation_id = 7;
        push_dir = 0x800;
    } else {
        g_CurrentEntity->animation_id = 6;
        push_dir = 0;
    }
    g_CurrentEntity->move_speed_current = 0xFU;
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 6;
        if (ZOMBIE(g_CurrentEntity)->push_cooldown == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HURT, 0);
            ZOMBIE(g_CurrentEntity)->push_cooldown = 0x96;
        }
    }
    if ((g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 5) {
            g_CurrentEntity->behavior_flags = 0U;
        }
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_CurrentEntity->hit_state = 0;
    } else {
        if ((u8) g_CurrentEntity->animation_frame_id < 5U) {
            g_CurrentEntity->move_speed_current = (u16) (g_CurrentEntity->move_speed_current + 0x5A);
        }
        if (g_CurrentEntity->animation_frame_id == 0x14) {
            g_CurrentEntity->hit_state = 0;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(push_dir);
}

void explode_leg_and_drop(void) {
    s32 joints;
    SVECTOR probe_neg = { -800, 0, 0 }; /* start and end point of the two-point room collision probe */
    SVECTOR probe_pos = { 800, 0, 0 };
    VECTOR saved_pos;

    /* zero word: Capcom's jump table below was 8-aligned (Makefile alignfix_for) */
    __asm__(".section .rodata\n\t.word 0\n\t.text");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, death_timer, u8) = 0x46U;
        g_CurrentEntity->animation_frame_id = 0U;
        ENTITY_AS(g_CurrentEntity, timing_control, u8) = 0U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->move_speed_current = 0x14;
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 10), (s32 *)0x14, 5, 3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 11), (s32 *)0x14, 5, 3);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, 0, JOINT_N_WORLD(joints, 9), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, 0, JOINT_N_WORLD(joints, 10), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, 0, JOINT_N_WORLD(joints, 13), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GORE, NULL);
        if (ZOMBIE(g_CurrentEntity)->push_cooldown == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HURT, NULL);
            ZOMBIE(g_CurrentEntity)->push_cooldown = 0x96;
        }
        if (g_CurrentEntity->health < 0) {
            g_CurrentEntity->health = 1;
        }
        g_CurrentEntity->pos_y = 1;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, ENTITY_AS(g_CurrentEntity, anim_header, s32 *), g_CurrentEntity->anim_base, 0x400));
        if (ENTITY_AS(g_CurrentEntity, timing_control, u8) == 1) {
            if (g_CurrentEntity->animation_frame_id == 8) {
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, NULL);
            }
            if (g_CurrentEntity->animation_frame_id == 0x1A) {
                (g_ApiCallPtr = api_sound_play_entity_se)(NULL, NULL);
            }
        }
        break;
    case 2:
        saved_pos = *(VECTOR *)(&g_CurrentEntity->pos_x);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xE);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision_two_point)(&probe_neg, &probe_pos);
        *(VECTOR *)(&g_CurrentEntity->pos_x) = saved_pos;
        if ((g_CurrentEntity->behavior_flags != 4) && (g_StageRoom != STAGE_ROOM(1, 2)) && (LIT(s32, sp_tmp00) == 0) && (g_CurrentEntity->health > 0)) {
            g_CurrentEntity->behavior_flags = (!(g_RandSeed & 3 & 0xFFFF)) ? 3 : 2;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
            g_CurrentEntity->hit_state = 0;
            g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
            g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xF1);
            return;
        }
        g_CurrentEntity->health = -1;
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, (s32 *)1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, (s32 *)-0x64, -0x64);
        (g_ApiCallPtr = api_Flg_on)(LITP(s32, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xE);
        g_CurrentEntity->move_speed_current = 0;
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, (s32 *)6, 6);
        if (--ENTITY_AS(g_CurrentEntity, death_timer, u8) == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        break;
    case 4:;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}


/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void push_and_drop(void) {
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xF;
        if (ZOMBIE(g_CurrentEntity)->push_cooldown == 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HURT, 0);
            ZOMBIE(g_CurrentEntity)->push_cooldown = 0x96;
        }
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_CurrentEntity->hit_state = 0;
        if (!(g_CurrentEntity->collision_flags & 8 & 0xFF) && !(g_CurrentEntity->behavior_flags & 0x20 & 0xFF) && ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 3)) {
            g_CurrentEntity->behavior_flags = 2U;
        }
    }
}

void long_push_back(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0xCU;
        g_CurrentEntity->timing_control = 1;
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->animation_id = 7;
        g_CurrentEntity->move_speed_current = 0x1E;
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
            g_CurrentEntity->action_ticks_counter = (u16) ((g_RandSeed + 0x10) & 7);
            g_CurrentEntity->move_speed_current = 0xF;
        } else if (g_CurrentEntity->animation_frame_id == 0x14) {
            g_CurrentEntity->hit_state = 0;
        }
        break;
    case 2:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 2);
            g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
            g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
            g_CurrentEntity->hit_state = 0;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
}

void benddown_and_standup(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xD;
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            g_CurrentEntity->behavior_flags = 0;
            if (g_StageRoom == STAGE_ROOM(4, 5)) {
                g_CurrentEntity->behavior_flags = 4;
            }
            g_CurrentEntity->hit_state = 0;
        }
    }
}

void zombie_dead_animation(void) {
    SVECTOR probe_neg = { -800, 0, 0 };
    SVECTOR probe_pos = { 800, 0, 0 };
    VECTOR saved_pos;
    u16 saved_angle;
    void *parts;

    saved_angle = 0;
    parts = ENTITY_AS(g_CurrentEntity, joints, void *);
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, death_timer, u8) = 0x46U;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->hit_state = 1;
        if (!(ZOMBIE(g_CurrentEntity)->down_flags & 1 & 0xFF) && !(FIELD_AT(parts, Joint, [2].flags, u8) & 0xCC & 0xFF)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_FALL, NULL);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, ENTITY_AS(g_CurrentEntity, anim_header, s32 *), g_CurrentEntity->anim_base, 0x400));
        return;
    case 2:
        saved_angle = g_CurrentEntity->angle;
        saved_pos = *(VECTOR *)(&g_CurrentEntity->pos_x);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision_two_point)(&probe_neg, &probe_pos);
        *(VECTOR *)(&g_CurrentEntity->pos_x) = saved_pos;
        g_CurrentEntity->angle = saved_angle;
        parts = ENTITY_AS(g_CurrentEntity, joints, void *);
        if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && (g_CurrentEntity->behavior_flags != 4) && !(FIELD_AT(parts, Joint, [2].flags, u8) & 0xCC & 0xFF) && (g_StageRoom != STAGE_ROOM(1, 2)) && (ENTITY_AS(g_CurrentEntity, animation_id, u8) == 8) && (LIT(s32, sp_tmp00) == 0) && !(g_RandSeed & 3 & 0xFFFF) && !(g_GameStateFlags & 0x10000)) {
            g_CurrentEntity->behavior_flags = (!(g_RandSeed & 3 & 0xFFFF)) ? 3 : 2;
            g_CurrentEntity->health = 1;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
            g_CurrentEntity->hit_state = 0;
            g_CurrentEntity->pos_y = 1;
            ZOMBIE(g_CurrentEntity)->down_flags = (s8) (ZOMBIE(g_CurrentEntity)->down_flags & 0xFB);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            return;
        }
        g_CurrentEntity->health = -1;
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, (s32 *)1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, (s32 *)-0x64, -0x64);
        (g_ApiCallPtr = api_Flg_on)(LITP(s32, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xE);
    case 3:
        g_CurrentEntity->move_speed_current = 0;
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, (s32 *)6, 6);
        if (--ENTITY_AS(g_CurrentEntity, death_timer, u8) == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        return;
    case 4:
        g_CurrentEntity->move_speed_current = 0;
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(LITP(s32, g_SysFlags), g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");

void player_anim_zombie_grabbed(void) {
    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.blend_counter = 0;
        g_PlayerEntity.pos_x = (s32) g_CurrentEntity->anim_base_x;
        g_PlayerEntity.pos_z = (s32) g_CurrentEntity->anim_base_z;
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
        g_PlayerEntity.status_flags |= 2;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        if ((g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            g_PlayerEntity.animation_id++;
            g_PlayerEntity.action_state = 2;
            g_PlayerEntity.timing_control = 0;
        }
        return;
    case 2:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 3:
        g_PlayerEntity.animation_id++;
        g_PlayerEntity.action_state = 4;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        /* fallthrough */
    case 4:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        if ((g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
            g_PlayerEntity.status_flags &= 0xFD;
            if ((((u8) g_PlayerEntity.animation_id) != 2) && (((u8) g_PlayerEntity.animation_id) != 8)) {

            } else {
                g_PlayerEntity.angle += 0x800;
            }
            g_PlayerEntity.hit_state = 0;
            if (((s16) g_PlayerEntity.next_turn_timer) == 0) {
                g_PlayerEntity.next_turn_timer = 0x96;
            }
        }
        /* fallthrough */
    default:
    }
}

void zombie_leg_step_speed(u8 part) {
    void *joint;
    u8 n;
    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), JOINT_N_TRANSFORM(joint, 1));
    joint = joint + ((part * 0x174) + 0x26C);
    n = 3;
    while (n != 0) {
        (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), (joint - (n * 0x7C)) + 0xA0);
        n--;
    }
    LIT(s32, sp_matrix_t0) = (s32) (LIT(s32, sp_matrix_t0) - JOINT(joint)->world.t[0]);
    LIT(s32, sp_matrix_t1) = 0;
    LIT(s32, sp_matrix_t2) = (s32) (LIT(s32, sp_matrix_t2) - JOINT(joint)->world.t[2]);
    (g_ApiCallPtr = api_Square0)(LITP(void, sp_matrix_t0), LITP(void, sp_vec));
    g_CurrentEntity->move_speed_current = (g_ApiCallPtr = api_SquareRoot0)(LIT(s32, sp_vec_vx) + LIT(s32, sp_vec_vz));
}

void zombie_head_bounce_physics(void *part, s16 gravity) {
    SVECTOR vel;
    VECTOR old_pos;
    void *mirror;

    if (g_GameStateFlags & GAME_MIRROR_ON) {
        mirror = (u8 *) ((void *) g_CurrentEntity->mirror_joints) + 0xF8;
    }
    if ((JOINT(part)->world.t[1] >= -0x64) && ((u32) (JOINT(part)->fx_state & 0x1F & 0xFF) >= 6U)) {
        JOINT(part)->flags = (u8) (JOINT(part)->flags & 0xFC);
        return;
    }
    vel.vy = (6 - (JOINT(part)->fx_state & 0x1F & 0xFF)) * 0x10;
    vel.vx = (6 - (JOINT(part)->fx_state & 0x1F & 0xFF)) * 8;
    vel.vz = 0x40;
    (g_ApiCallPtr = api_RotMatrix)(&vel, LITP(void, sp_matrix));
    (g_ApiCallPtr = api_MulMatrix)(&JOINT(part)->world, LITP(void, sp_matrix));
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    (g_ApiCallPtr = api_RotMatrixY)(((s16) g_CurrentEntity->angle), LITP(void, sp_matrix));
    (g_ApiCallPtr = api_ApplyMatrixSV)(LITP(s16, sp_matrix), &JOINT(part)->rotation, &vel);
    old_pos = FIELD_AT(part, Joint, ->world.t[0], VECTOR);
    JOINT(part)->world.t[0] = (s32) (JOINT(part)->world.t[0] + (vel.vx - (vel.vx * ({ (u8) ((u8) (JOINT(part)->fx_state & 0x40) >> 6) * 2; }))));
    JOINT(part)->world.t[2] = (s32) (JOINT(part)->world.t[2] + (vel.vz - (vel.vz * ({ (u8) ((u8) (JOINT(part)->fx_state & 0x80) >> 7) * 2; }))));
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    if ((g_ApiCallPtr = api_room_collision_check_point)(part + 0x58, ADDR_sp_svec) != 0) {
        JOINT(part)->world.t[0] = old_pos.vx;
        JOINT(part)->world.t[2] = old_pos.vz;
        JOINT(part)->fx_state = (u8) (JOINT(part)->fx_state ^ 0x40);
        JOINT(part)->world.t[0] = (s32) (JOINT(part)->world.t[0] + (vel.vx - (vel.vx * ({ (u8) ((u8) (JOINT(part)->fx_state & 0x40) >> 6) * 2; }))));
        JOINT(part)->world.t[2] = (s32) (JOINT(part)->world.t[2] + (vel.vz - (vel.vz * ({ (u8) ((u8) (JOINT(part)->fx_state & 0x80) >> 7) * 2; }))));
        if ((g_ApiCallPtr = api_room_collision_check_point)(part + 0x58, ADDR_sp_svec) != 0) {
            JOINT(part)->fx_state = (u8) (JOINT(part)->fx_state ^ 0xC0);
        }
        JOINT(part)->world.t[0] = old_pos.vx;
        JOINT(part)->world.t[2] = old_pos.vz;
        JOINT(part)->rotation.vx = (s16) ((s16) JOINT(part)->rotation.vx >> 1);
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, (s32) (&JOINT(part)->world), ADDR_sp_vec, 0);
    }
    JOINT(part)->rotation.vy = (u16) (((u16) JOINT(part)->rotation.vy) - (gravity * JOINT(part)->fx_frame));
    JOINT(part)->world.t[1] = (s32) (JOINT(part)->world.t[1] - (s16) ((u16) JOINT(part)->rotation.vy));
    if (JOINT(part)->world.t[1] >= -0x64) {
        JOINT(part)->world.t[1] = -0x64;
        JOINT(part)->fx_state++;
        JOINT(part)->rotation.vy *= -1;
        JOINT(part)->rotation.vy = (u16) ((s16) ((u16) JOINT(part)->rotation.vy) >> 2);
        JOINT(part)->fx_frame = 0U;
        JOINT(part)->rotation.vx = (s16) ((u16) JOINT(part)->rotation.vx + 0x28);
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, (s32) (&JOINT(part)->world), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_HEAD_BOUNCE, NULL);
    }
    if (JOINT(part)->rotation.vx > 0) {
        JOINT(part)->rotation.vx = 0;
    }
    JOINT(part)->fx_frame++;
}

u8 zombie_grab_struggle_input(void) {
    u8 power;

    power = 0;
    if (g_PadTrigger & 0xF000 & 0xFFFF) {
        power = 3;
    }
    if (g_PadTrigger & 0xF0 & 0xFFFF) {
        power += 2;
    }
    return power;
}

void zombie_scd_action_dispatch(void) {
    extern s32 (*zombie_action_tbl_scd[])();
    zombie_action_tbl_scd[g_CurrentEntity->action_behavior]();
}

void zombie_scd_action_nop(void) {

}

void zombie_scd_walk_to_point(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GROAN, 0U);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400U) != 0) {
            g_CurrentEntity->action_state++;
            g_CurrentEntity->move_speed_current = 0x23;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->animation_id = 3;
            g_CurrentEntity->blend_counter = 3;
        }
        break;
    case 2:
        LIT(s32, sp_vec_vx) = (s32) g_CurrentEntity->anim_base_x;
        LIT(s32, sp_vec_vz) = (s32) g_CurrentEntity->anim_base_z;
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x40U);
        if (g_CurrentEntity->animation_frame_id == 8) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0U);
        }
        if (g_CurrentEntity->animation_frame_id == 0x1D) {
            (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0U);
        }
        (g_ApiCallPtr = api_joint_move)(g_CurrentEntity->scd_entity_flags & 1 & 0xFFFF, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400U);
        if ((g_ApiCallPtr = api_SquareRoot0)(((g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x) * (g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x)) + ((g_CurrentEntity->pos_z - g_CurrentEntity->anim_base_z) * ((g_CurrentEntity->pos_z) - (g_CurrentEntity->anim_base_z)))) < 0x96U) {
            g_CurrentEntity->action_state++;
        }
        break;
    case 3:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        if (g_CurrentEntity->collision_flags & 0x80 & 0xFF) {
            g_CurrentEntity->action_state = 2U;
        } else {
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
            break;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void zombie_headshot(void) {
    s32 joints;

    g_CurrentEntity->animation_id = 8;
    g_CurrentEntity->move_speed_current = 0x14;
    if (g_CurrentEntity->action_state == 0) {
        LIT(s32, sp_vec_vx) = 0x64;
        LIT(s32, sp_vec_vy) = -0xA3C;
        LIT(s32, sp_vec_vz) = 0;
        joints = g_CurrentEntity->joints;
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 2), 0x1E, 2, (void *)3);
        (g_ApiCallPtr = api_effect_create_billboard)(3, 0, (((s16) g_PlayerEntity.angle) - ((s16) g_CurrentEntity->angle)) + 0x800, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vx) = 0;
        LIT(s32, sp_vec_vy) = -0x258;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0, 3, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(4, 0, (((s16) g_PlayerEntity.angle) - ((s16) g_CurrentEntity->angle)) + 0x600, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(4, 2, (((s16) g_PlayerEntity.angle) - ((s16) g_CurrentEntity->angle)) + 0x700, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(4, 3, (((s16) g_PlayerEntity.angle) - ((s16) g_CurrentEntity->angle)) + 0x900, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_GORE, 0);
    }
    if (g_CurrentEntity->animation_frame_id == 8) {
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_B, 0);
    }
    if (g_CurrentEntity->animation_frame_id == 0x1A) {
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_A, 0);
    }
    zombie_dead_animation();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void zombie_scd_dying(void) {
    g_CurrentEntity->animation_id = 0xA;
    g_CurrentEntity->move_speed_current = 0x14U;
    if (g_CurrentEntity->animation_frame_id == 0x12) {
        (g_ApiCallPtr = api_sound_play_entity_se)(ZOMBIE_SE_STEP_C, 0);
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 5U) {
        g_CurrentEntity->move_speed_current = (u16) (g_CurrentEntity->move_speed_current + 0x5A);
    }
    zombie_dead_animation();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
}

void zombie_scd_vomiting(void) {
    g_CurrentEntity->animation_id = 4;
    short_push_back();
    if (g_CurrentEntity->action_state & 2 & 0xFF) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void zombie_scd_vomiting2(void) {
    g_CurrentEntity->animation_id = 5;
    short_push_back();
    if (g_CurrentEntity->action_state & 2 & 0xFF) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}
