/* yawn.c - Yawn (giant snake): moves along fixed paths, bite / swallow, flee and reposition, damage and death,
   its jointed body, and the player's swallowed-by-Yawn death animation.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/yawn.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* yawn data (defined in each overlay that has this enemy) */
extern void *yawn_sca_info_ptr;
extern s32 g_YawnCaptureMatrix;
extern s32 g_YawnCaptureMatrix_tx;
extern s32 g_YawnCaptureMatrix_ty;
extern s32 g_YawnCaptureMatrix_tz;
extern s16 g_YawnFleeTimer;

/* YawnWork: the yawn's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). YAWN(e)->f, YAWN_FIELD(e, f) and YAWN_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct YawnWork {
    /* +0x00 */ s8 speed_accel;       /* 0x16C: accelerator for move_speed (yawn_turn_accel): |n| doubles up to 0x40, flips sign at the speed limit; 0x40 at init, 1 on flee/reposition */
    /* +0x01 */ u8 pad01[1];
    /* +0x02 */ u8 form;              /* 0x16E: 0 = first-fight form, 1 = grown/rematch; picks bite anim (n*10+3), bite window, range (n*1000+5000), rear/emerge logic */
    /* +0x03 */ s8 bite_count;        /* 0x16F: bites in a row (++ per bite/swallow); >=5 within range forces the rear-up; reset in yawn_action_rear */
    /* +0x04 */ u16 swallow_scale;    /* 0x170: model scale ramp: 5000 at swallow start, +100 per step; non-zero applies scale (n-300) in yawn_update / swallow */
    /* +0x06 */ s16 move_speed;       /* 0x172: current XZ speed handed to api_entity_add_speed_xz, integrated by yawn_turn_accel; cleared when stopping */
    /* +0x08 */ s16 ground_y;         /* 0x174: ground height taken from joint 7 in yawn_init; yawn_anim_advance pins the body Y to it (<<9) */
    /* +0x0A */ s16 death_scale;      /* 0x176: death shrink ramp (0x1000, -0x14 per frame down to 0x1F4) in yawn_die_run; yawn_post_move tests it */
    /* +0x0C */ s32 saved_state;      /* 0x178: copy of the state word (yawn_state_check); restored by yawn_damaged */
    /* +0x10 */ s8 bite_turn_dir;     /* 0x17C: -1/0/1 turn during a bite (rotate_toward_target n*0x10); 1 on flee/reposition; yawn_damaged checks it */
    /* +0x11 */ u8 bite_recovery;     /* 0x17D: recovery countdown after a bite/hit (0xDA in yawn_damaged_run, low 7 bits counted down in yawn_action_move); bit 7 = interrupted */
    /* +0x12 */ u8 waypoint_index;    /* 0x17E: index into yawn_entry_path / yawn_flee_path / yawn_reposition_path */
    /* +0x13 */ s8 player_near_death; /* 0x17F: 1 when the player's health < 10 (pick_action), forces the rear-up; cleared otherwise */
    /* +0x14 */ u16 death_fx_timer;   /* 0x180: death effect frame counter (up to 800) in yawn_die_run; spawns effects every 16 / 8 frames */
    /* +0x16 */ u8 wall_frames;       /* 0x182: frames against room geometry (yawn_post_move); >=0x97 forces a reposition */
    /* +0x17 */ u8 wall_reset_delay;  /* 0x183: 20 after each wall contact, counted down; at 0 clears wall_frames; grown form bite checks >=11 */
    /* +0x18 */ u8 dust_timer;        /* 0x184: 0xF0 after a bite, counted down in yawn_state_check; spawns dust every 8 frames while non-zero */
    /* +0x19 */ u8 swallow_cooldown;  /* 0x185: 30 after a swallow, counted down in yawn_update; must be 0 before the next grab (state 0x40101) */
} YawnWork;
#define YAWN(e) ENEMY_WORK(e, YawnWork)
#define YAWN_FIELD(e, f) ENEMY_FIELD(e, YawnWork, f)
#define YAWN_MEMBER(e, f) ENEMY_MEMBER(e, YawnWork, f)

/* state 0: sets up a new yawn (health, sca info, animation, work fields), then state 1 */
void yawn_init(void) {
    void yawn_pose_init();
    void *joints;
    Entity *slot;

    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->hit_state = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(u16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(LITP(void, sp_svec), &g_CurrentEntity->shadow, 0x3E8, 0x3E8);
    g_CurrentEntity->health = 0xBEA;
    if (g_CurrentEntity->id == 0x12) {
        g_CurrentEntity->health = 0x190;
    }
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->animation_id = (s8) (((g_CurrentEntity->behavior_flags & 2 & 0xFF) * 5) + 1);
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 2) {
        g_CurrentEntity->animation_id = 2;
    }
    g_CurrentEntity->blend_counter = 0;
    joints = ((void *) g_CurrentEntity->joints);
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vx) = MEMBER_AT(joints, Joint, ->transform.t[0], u16);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_ApplyMatrixSV)((u8 *)g_CurrentEntity + 0x20, LITP(void, sp_svec), ADDR_sp_svec);
    g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - LIT(s16, sp_svec_vx));
    g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z - LIT(s16, sp_svec_vz));
    (g_ApiCallPtr = api_joint_move)(NULL, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x40);
    (g_ApiCallPtr = api_entity_compute_joint_world_matrices)(NULL);
    joints = ((void *) g_CurrentEntity->joints);
    g_CurrentEntity->pos_x = (s32) JOINT(joints)->world.t[0];
    g_CurrentEntity->pos_y = 0;
    g_CurrentEntity->pos_z = (s32) JOINT(joints)->world.t[2];
    g_CurrentEntity->position_vx = (u16) g_CurrentEntity->pos_x;
    g_CurrentEntity->position_vy = 0;
    g_CurrentEntity->position_vz = (u16) g_CurrentEntity->pos_z;
    YAWN(g_CurrentEntity)->ground_y = (u16) MEMBER_AT((JOINT_N(joints, 7)), Joint, ->world.t[1], u16);
    g_CurrentEntity->sca_info = (s32)yawn_sca_info_ptr;
    *((s16 *) g_CurrentEntity->sca_hit_data) = 0;
    HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->z = 0;
    HIT_POINT(((s16 *) g_CurrentEntity->sca_hit_data))->y = (u16) ((u16) SCA(((void *) g_CurrentEntity->sca_info))->ofs_y);
    JOINT(joints)->transform.t[0] = 0;
    JOINT(joints)->transform.t[2] = 0;
    FIELD_AT(joints, Joint, [3].flags, u8) = (u8) (FIELD_AT(joints, Joint, [3].flags, u8) | 8);
    FIELD_AT(joints, Joint, [4].flags, u8) = (u8) (FIELD_AT(joints, Joint, [4].flags, u8) | 8);
    FIELD_AT(joints, Joint, [5].flags, u8) = (u8) (FIELD_AT(joints, Joint, [5].flags, u8) | 8);
    FIELD_AT(joints, Joint, [6].flags, u8) = (u8) (FIELD_AT(joints, Joint, [6].flags, u8) | 8);
    FIELD_AT(joints, Joint, [7].flags, u8) = (u8) (FIELD_AT(joints, Joint, [7].flags, u8) | 8);
    FIELD_AT(joints, Joint, [8].flags, u8) = (u8) (FIELD_AT(joints, Joint, [8].flags, u8) | 8);
    FIELD_AT(joints, Joint, [9].flags, u8) = (u8) (FIELD_AT(joints, Joint, [9].flags, u8) | 8);
    FIELD_AT(joints, Joint, [10].flags, u8) = (u8) (FIELD_AT(joints, Joint, [10].flags, u8) | 8);
    FIELD_AT(joints, Joint, [11].flags, u8) = (u8) (FIELD_AT(joints, Joint, [11].flags, u8) | 8);
    FIELD_AT(joints, Joint, [12].flags, u8) = (u8) (FIELD_AT(joints, Joint, [12].flags, u8) | 8);
    FIELD_AT(joints, Joint, [13].flags, u8) = (u8) (FIELD_AT(joints, Joint, [13].flags, u8) | 8);
    FIELD_AT(joints, Joint, [14].flags, u8) = (u8) (FIELD_AT(joints, Joint, [14].flags, u8) | 8);
    (g_ApiCallPtr = api_entity_compute_joint_world_matrices)(NULL);
    YAWN(g_CurrentEntity)->move_speed = 0;
    YAWN(g_CurrentEntity)->speed_accel = 0x40;
    YAWN(g_CurrentEntity)->form = 0;
    YAWN(g_CurrentEntity)->bite_count = 0;
    YAWN(g_CurrentEntity)->swallow_scale = 0;
    YAWN(g_CurrentEntity)->bite_turn_dir = 0;
    YAWN(g_CurrentEntity)->bite_recovery = 0;
    YAWN(g_CurrentEntity)->waypoint_index = 0;
    YAWN(g_CurrentEntity)->death_scale = 0;
    YAWN(g_CurrentEntity)->death_fx_timer = 0;
    YAWN(g_CurrentEntity)->player_near_death = 0;
    YAWN(g_CurrentEntity)->wall_frames = 0;
    YAWN(g_CurrentEntity)->dust_timer = 0;
    YAWN(g_CurrentEntity)->swallow_cooldown = 0;
    slot = LITP(void, g_EnemiesList);
    g_EnemyCount += 0xC;
    joints = ((void *) g_CurrentEntity->joints);
    LIT(s32, sp_tmp00) = 0xD;
    slot += 12;
    while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != 0) {
        (g_ApiCallPtr = api_mem_copy)(slot, g_CurrentEntity, ENTITY_SIZE);
        slot->aim_target = (s32)((LIT(s32, sp_tmp00) * 0x7C) + joints + 0xF8);
        slot->pos_x = (s32) JOINT(((LIT(s32, sp_tmp00) * 0x7C) + joints + 0xF8))->world.t[0];
        slot->pos_y = (s32) JOINT(((LIT(s32, sp_tmp00) * 0x7C) + joints + 0xF8))->world.t[1];
        slot->pos_z = (s32) JOINT(((LIT(s32, sp_tmp00) * 0x7C) + joints + 0xF8))->world.t[2];
        slot->position_vx = (u16) g_CurrentEntity->pos_x;
        slot->position_vy = (u16) g_CurrentEntity->pos_y;
        slot->position_vz = (u16) g_CurrentEntity->pos_z;
        slot->behavior_flags = 1;
        slot->health = -1;
        slot->status_flags = (u8) (slot->status_flags | 0x64);
        if (((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 2) || ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 6)) {
            slot->status_flags = (u8) (slot->status_flags & 0xFB);
        }
        slot--;
    }
    yawn_pose_init();
    if (!(g_CurrentEntity->behavior_flags & 0xF & 0xFF) || ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 4)) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 4);
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 5;
        if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
            g_CurrentEntity->state = 4;
        }
        return;
    }
    YAWN(g_CurrentEntity)->form = 1;
    if (g_CurrentEntity->id == 0xD) {
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_ScenarioFlags), (void *)0x10);
    }
    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        g_CurrentEntity->state = 4;
    }
}

/* every frame for each yawn (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void yawn_update(void) {
    void yawn_chain_follow(void *parent, void *joint, s16 max_turn);
    void yawn_chain_align();
    extern u16 g_EnemySlot0_Angle_u16 __asm__("g_EnemySlot0_Angle");
    extern s16 g_EnemySlot0_Health_s16[] __asm__("g_EnemySlot0_Health");
    extern s32 (*yawn_state_tbl[])();
    void *joint;
    void *slot;

    if ((g_UpdateFlags & 4 & 0xFFFF) && !(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        yawn_state_tbl[g_CurrentEntity->state]();
        if (YAWN(g_CurrentEntity)->swallow_cooldown != 0) {
            YAWN(g_CurrentEntity)->swallow_cooldown--;
        }
    }
    if (g_CurrentEntity->behavior_flags == 1) {
        joint = ENTITY_AS(g_CurrentEntity, aim_target, void *);
        g_CurrentEntity->pos_x = (s32) MEMBER_AT(joint, Joint, ->world.t[0], s32);
        g_CurrentEntity->pos_y = (s32) MEMBER_AT(joint, Joint, ->world.t[1], s32);
        g_CurrentEntity->pos_z = (s32) MEMBER_AT(joint, Joint, ->world.t[2], s32);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_visual_range)((void *)0xFA0);
        if (g_CurrentEntity->pos_y < -0x5DC) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xC0);
        }
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 7 & 0xFF) {
            ENTITY_AS(g_CurrentEntity, hit_state, u8)--;
            LIT(s32, sp_vec_vx) = 0;
            LIT(s32, sp_vec_vy) = -0x12C;
            if (MEMBER_AT(joint, Joint, ->world.t[1], s32) < -0x5DC) {
                LIT(s32, sp_vec_vy) = 0x12C;
            }
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8U, 0U, &JOINT(joint)->world, ADDR_sp_vec, 0);
            if (((ENTITY_AS(g_CurrentEntity, hit_state, u8) & 0x78 & 0xFF) != 0x40) && ((ENTITY_AS(g_CurrentEntity, hit_state, u8) & 0x78 & 0xFF) != 0x50)) {

            } else {
                g_EnemySlot0_Health_s16[0] -= 0x41;
                g_EnemySlot0_Angle_u16 += (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x10U);
                if ((s16) g_EnemySlot0_Health_s16[0] < 0) {
                    g_EnemySlot0_State = 3;
                }
            }
            if (!(ENTITY_AS(g_CurrentEntity, hit_state, u8) & 7 & 0xFF)) {
                g_EnemySlot0_Health_s16[0] -= 0xF;
                if ((u8) JOINT(ENTITY_AS(g_CurrentEntity, aim_target, void *))->index < 0xAU) {
                    g_EnemySlot0_Health_s16[0] = g_EnemySlot0_Health_s16[0];
                }
                g_EnemySlot0_Angle_u16 += (g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x10U);
                if ((s16) g_EnemySlot0_Health_s16[0] < 0) {
                    g_EnemySlot0_State = 3;
                }
            }
            ENTITY_AS(g_CurrentEntity, hit_state, u8) = (u8) (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 0x87);
        }
    }
    LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(ENTITY_AS(g_CurrentEntity, sca_info, void *))->radius);
    if ((g_CurrentEntity->behavior_flags == 1) && (LIT(s32, sp_tmp00) != 0)) {
        LIT(s32, sp_tmp18) = (s32) MEMBER_AT(joint, Joint, ->index, u8);
        {
            if (MEMBER_AT(joint, Joint, ->index, u8) == 3) {
                LIT(s32, sp_tmp04) = MEMBER_AT(joint, Joint, ->world.t[0], s32) - g_CurrentEntity->pos_x;
                LIT(s32, sp_tmp08) = MEMBER_AT(joint, Joint, ->world.t[2], s32) - g_CurrentEntity->pos_z;
                LIT(s32, sp_tmp1C) = 4;
                while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != 0) {
                    MEMBER_AT((joint - (LIT(s32, sp_tmp1C) * 0x7C)), Joint, ->world.t[0], s32) = (s32) (MEMBER_AT((joint - (LIT(s32, sp_tmp1C) * 0x7C)), Joint, ->world.t[0], s32) - LIT(s32, sp_tmp04));
                    MEMBER_AT((joint - (LIT(s32, sp_tmp1C) * 0x7C)), Joint, ->world.t[2], s32) = (s32) (MEMBER_AT((joint - (LIT(s32, sp_tmp1C) * 0x7C)), Joint, ->world.t[2], s32) - LIT(s32, sp_tmp08));
                }
                MEMBER_AT((JOINT_N(joint, 1)), Joint, ->world.t[0], s32) = (s32) (MEMBER_AT((JOINT_N(joint, 1)), Joint, ->world.t[0], s32) - LIT(s32, sp_tmp04));
                MEMBER_AT((JOINT_N(joint, 1)), Joint, ->world.t[2], s32) = (s32) (MEMBER_AT((JOINT_N(joint, 1)), Joint, ->world.t[2], s32) - LIT(s32, sp_tmp08));
                MEMBER_AT(joint, Joint, ->world.t[0], s32) = (s32) g_CurrentEntity->pos_x;
                MEMBER_AT(joint, Joint, ->world.t[2], s32) = (s32) g_CurrentEntity->pos_z;
                LIT(s32, sp_tmp18) = 4;
            } else {
                MEMBER_AT(joint, Joint, ->world.t[0], s32) = (s32) g_CurrentEntity->pos_x;
                MEMBER_AT(joint, Joint, ->world.t[2], s32) = (s32) g_CurrentEntity->pos_z;
                if (((u8) MEMBER_AT(joint, Joint, ->index, u8) >= 6U) && (YAWN(g_CurrentEntity)->form == 0)) {
                    yawn_chain_align(joint - 0x7C, joint);
                }
            }
        }
        for (; LIT(s32, sp_tmp18) < 0xF; LIT(s32, sp_tmp18)++, (*(Joint **)&joint)++) {
            yawn_chain_follow(joint - 0x7C, joint, 0x30);
        }
        joint = ENTITY_AS(g_CurrentEntity, aim_target, void *);
        LIT(void *, sp_tmp20) = g_CurrentEntity;
        g_CurrentEntity = LITP(void, g_EnemiesList);
        if ((JOINT(joint)->index == 4) && (((s16) YAWN(g_CurrentEntity)->swallow_scale) != 0) && (YAWN(g_CurrentEntity)->form == 0)) {
            LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = ((s16) YAWN(g_CurrentEntity)->swallow_scale) - 0x12C;
            (g_ApiCallPtr = api_ScaleMatrix)(joint - 0x38, ADDR_sp_vec);
        }
        if ((JOINT(joint)->index == 3) && (((s16) YAWN(g_CurrentEntity)->swallow_scale) != 0)) {
            LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = ((s16) YAWN(g_CurrentEntity)->swallow_scale) - 0x12C;
            (g_ApiCallPtr = api_ScaleMatrix)(&JOINT(joint)->world, ADDR_sp_vec);
        }
        g_CurrentEntity = LIT(Entity *, sp_tmp20);
    }
    g_CurrentEntity->has_enter_switch_zone = (u8) (g_CurrentEntity->has_enter_switch_zone & 0x80);
    g_CurrentEntity->has_enter_switch_zone = (u8) (g_CurrentEntity->has_enter_switch_zone | (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones));
    if (g_CurrentEntity->behavior_flags == 1) {
        joint = ENTITY_AS(g_CurrentEntity, aim_target, void *);
        slot = LITP(void, g_EnemiesList);
        if (ENT(slot)->health < 0) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
            ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0x80U;
        }
        if (JOINT(joint)->index == 0xE) {
            LIT(s32, sp_tmp00) = 0xC;
            joint = ((void *) ENT(slot)->joints);
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                if (ENT(slot)->has_enter_switch_zone & 0x7F & 0xFF) {
                    (g_ApiCallPtr = api_billboard_draw)(JOINT_N_POS(joint, 0), (&ENT(slot)->shadow), 0U, (s16) ENT(slot)->angle);
                }
                (*(Entity **)&slot)++;
                joint = ((void *) ENT(slot)->aim_target);
            }
        }
    }
}

void yawn_state_check(void) {
    void yawn_check_actions();
    void yawn_select_behavior();
    void yawn_spawn_dust();

    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    ENTITY_FIELD(g_CurrentEntity, status_flags) = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    if ((g_ApiCallPtr = api_entity_check_alert_range)(0xFA0U) < 0xFA0U) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xBF);
    }
    if (YAWN(g_CurrentEntity)->form == 0) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x7F);
        (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0U);
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        yawn_select_behavior();
    }
    yawn_check_actions();
    YAWN(g_CurrentEntity)->saved_state = (s32) ENTITY_STATE_WORD(g_CurrentEntity);
    if (YAWN(g_CurrentEntity)->dust_timer != 0) {
        if (!(YAWN(g_CurrentEntity)->dust_timer-- & 7)) {
            yawn_spawn_dust((g_ApiCallPtr = api_rand)() & 1);
        }
    }
}

void yawn_damaged(void) {
    void yawn_damaged_run();
    if (YAWN(g_CurrentEntity)->bite_turn_dir != 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = (s32) YAWN(g_CurrentEntity)->saved_state;
        return;
    }
    if (YAWN(g_CurrentEntity)->bite_recovery != 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = (s32) YAWN(g_CurrentEntity)->saved_state;
        return;
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 1, 0, 0);
    }
    yawn_damaged_run();
}

void yawn_die(void) {
    void yawn_die_run();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
    }
    yawn_die_run();
}

void yawn_state_wait(void) {
    if (!(g_CurrentEntity->behavior_flags & 0x80 & 0xFF)) {
        g_CurrentEntity->state = 1;
    }
}

void yawn_check_actions(void) {
    extern s32 (*yawn_action_tbl[])();
    yawn_action_tbl[g_CurrentEntity->action_behavior]();
}

void yawn_select_behavior(void) {
    extern s32 (*yawn_selector_tbl[])();

    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    yawn_selector_tbl[(g_CurrentEntity->behavior_flags & 7 & 0xFF)]();
}

void yawn_pick_action_normal(void) {
    void yawn_turn_accel();
    s32 unused;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
    if (((u8) g_PlayerEntity.hit_state) == 0) {
        LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX);
        if ((LIT(s32, sp_tmp00) < 0x7D0) && (LIT(s32, sp_tmp04) == 0) && (g_PlayerEntity.health >= 0xA)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
        }
        if ((LIT(s32, sp_tmp00) < ((YAWN(g_CurrentEntity)->form * 0x3E8) + 0x1388)) && (LIT(s32, sp_tmp04) == 0) && (g_PlayerEntity.health >= 0xA)) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
            }
        }
        if ((LIT(s32, sp_tmp00) < 0x1B58) && (YAWN(g_CurrentEntity)->bite_count >= 5)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        }
        if ((LIT(s32, sp_tmp00) < 0xFA0) && (YAWN(g_CurrentEntity)->form != 0) && (LIT(s32, sp_tmp04) == 0)) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40) == 0) && ((g_PlayerEntity.health - 0xA) < 0) && (YAWN(g_CurrentEntity)->swallow_cooldown == 0)) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
            }
        }
        if (((g_PlayerEntity.health - 0xA) < 0) && (YAWN(g_CurrentEntity)->form == 0)) {
            YAWN(g_CurrentEntity)->player_near_death = 1;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        }
        if ((g_CurrentEntity->pos_z >= 0x4269) && (YAWN(g_CurrentEntity)->form != 0) && (g_CurrentEntity->action_behavior == 2)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
            YAWN(g_CurrentEntity)->player_near_death = 0;
        }
        if ((u8) YAWN(g_CurrentEntity)->wall_frames >= 0x97U) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 8, 0);
        }
        if (g_PlayerEntity.collision_flags & 8 & 0xFF) {
            if ((YAWN(g_CurrentEntity)->form != 0) && ((g_PlayerEntity.health - 0xA) >= 0)) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
                YAWN(g_CurrentEntity)->player_near_death = 0;
            }
        } else if (g_PlayerEntity.health >= 0xA) {

        }
        if (g_CurrentEntity->health < 0xAF0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        }
    }
    yawn_turn_accel();
}

void yawn_pick_action_scripted(void) {
    void yawn_turn_accel();
    s32 unused;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
    if (((u8) g_PlayerEntity.hit_state) == 0) {
        LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX);
        if ((LIT(s32, sp_tmp00) < 0x7D0) && (LIT(s32, sp_tmp04) == 0) && (g_PlayerEntity.health >= 0xA)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
        }
        if ((LIT(s32, sp_tmp00) < ((YAWN(g_CurrentEntity)->form * 0x3E8) + 0x1388)) && (LIT(s32, sp_tmp04) == 0) && (g_PlayerEntity.health >= 0xA)) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 2, 0);
            }
        }
        if ((LIT(s32, sp_tmp00) < 0x1B58) && (YAWN(g_CurrentEntity)->bite_count >= 5)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        }
        if ((LIT(s32, sp_tmp00) < 0xFA0) && (YAWN(g_CurrentEntity)->form != 0) && (LIT(s32, sp_tmp04) == 0)) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40) == 0) && ((g_PlayerEntity.health - 0xA) < 0) && (YAWN(g_CurrentEntity)->swallow_cooldown == 0)) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
            }
        }
        if (((g_PlayerEntity.health - 0xA) < 0) && (YAWN(g_CurrentEntity)->form == 0)) {
            YAWN(g_CurrentEntity)->player_near_death = 1;
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
        }
    }
    yawn_turn_accel();
}

void yawn_action_idle(void) {
    s32 yawn_anim_advance_a1(s32, s32) __asm__("yawn_anim_advance");
    s32 yawn_post_move_a1(s32) __asm__("yawn_post_move");

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 7;
        YAWN(g_CurrentEntity)->move_speed = 0;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0xF) + 0xF);
    }
    yawn_anim_advance_a1(0, 0x200);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
    yawn_post_move_a1(0x30);
}

void yawn_action_move(void) {
    s32 yawn_anim_advance_a2() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a2() __asm__("yawn_post_move");
    void yawn_spawn_dust();
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = (u8) (YAWN(g_CurrentEntity)->form + 1);
        g_CurrentEntity->blend_counter = 7;
        YAWN(g_CurrentEntity)->move_speed = 0;
        g_CurrentEntity->move_speed_current = 0xA0;
        if (((g_PlayerEntity.health - 0xA) >= 0) && (((u8) YAWN(g_CurrentEntity)->player_near_death) == 0) && !(YAWN(g_CurrentEntity)->bite_recovery & 0x80 & 0xFF)) {

        } else if (YAWN(g_CurrentEntity)->form != 0) {
            g_CurrentEntity->animation_id = 5U;
        }
    }
    if (YAWN(g_CurrentEntity)->bite_recovery & 0x7F & 0xFF) {
        YAWN(g_CurrentEntity)->bite_recovery--;
        if (YAWN(g_CurrentEntity)->bite_recovery == 0) {
            YAWN(g_CurrentEntity)->bite_recovery = 0;
            g_CurrentEntity->action_state = 0U;
        }
    }
    if (((u8) g_CurrentEntity->animation_id) == 5) {
        if ((g_CurrentEntity->animation_frame_id != 0xF) && (g_CurrentEntity->animation_frame_id != 0x28)) {

        } else {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
    }
    yawn_anim_advance_a2(0, 0x200);
    if ((YAWN(g_CurrentEntity)->dust_timer != 0) && !(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
        yawn_spawn_dust((g_ApiCallPtr = api_rand)() & 1);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move_a2(0x30);
}

void yawn_action_bite(void) {
    u8 yawn_anim_advance_a3() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a3() __asm__("yawn_post_move");
    extern struct { u8 a; u8 b; } yawn_bite_window[];
    s32 joints;
    u8 bite_frame;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = (s8) ((YAWN(g_CurrentEntity)->form * 0xA) + 3);
        g_CurrentEntity->move_speed_current = 0x64;
        YAWN(g_CurrentEntity)->bite_count++;
        YAWN(g_CurrentEntity)->bite_turn_dir = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
        /* fallthrough */
    case 1:
        bite_frame = g_CurrentEntity->animation_frame_id - yawn_bite_window[YAWN(g_CurrentEntity)->form].a;
        if (bite_frame < yawn_bite_window[YAWN(g_CurrentEntity)->form].b) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            SCRATCH_VEC->x = 0x3E8;
            joints = g_CurrentEntity->joints;
            LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX);
            if (((u8) g_PlayerEntity.action_behavior >= 0x66U) && (g_CurrentEntity->animation_frame_id & 1 & 0xFF)) {
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
                SCRATCH_VEC->x = 0;
                LIT(s32, sp_vec_vy) = -0x12C - (YAWN(g_CurrentEntity)->form * 0x5DC);
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
            }
            if ((LIT(s32, sp_tmp04) != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_PlayerEntity.health -= 0x1C;
                if ((s16) g_PlayerEntity.health < 0) {
                    g_PlayerEntity.health = 1;
                }
                if ((s16) g_PlayerEntity.health >= 0) {
                    g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp1C) + 1;
                    g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp1C) + 0x66;
                }
                (g_ApiCallPtr = api_pad_vibrate_small)(5, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xC8, 0);
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                SCRATCH_VEC->x = 0x3E8;
                joints = g_CurrentEntity->joints;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 1), 0x30, 0x80820, 0x606060);
                g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x100);
                if (g_CurrentEntity->id == 0xD) {
                    if ((g_ApiCallPtr = api_Flg_ck)(ADDR_g_ScenarioFlags, 0x47) == 0) {
                        (g_ApiCallPtr = api_Flg_on)(ADDR_g_ScenarioFlags2, 0x43);
                        g_PlayerEntity.collision_flags |= 0x20;
                    }
                }
                YAWN(g_CurrentEntity)->bite_turn_dir = -1;
                YAWN(g_CurrentEntity)->bite_recovery = 0;
                YAWN(g_CurrentEntity)->wall_frames = 0;
                YAWN(g_CurrentEntity)->dust_timer = 0xF0;
                (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
            }
        }
        g_CurrentEntity->action_state += yawn_anim_advance_a3(0, 0x200);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x38);
        if (g_CurrentEntity->animation_frame_id == ((YAWN(g_CurrentEntity)->form * 5) + 8)) {
            if ((YAWN(g_CurrentEntity)->form != 0) && ((u8) YAWN(g_CurrentEntity)->wall_reset_delay >= 0xBU)) {
                g_CurrentEntity->sub_state = 0;
                ENTITY_ACTION_S16(g_CurrentEntity) = 1;
                YAWN(g_CurrentEntity)->bite_turn_dir = 0;
                YAWN(g_CurrentEntity)->bite_count = 5;
                g_CurrentEntity->hit_state = 0;
                (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, -0x10);
                break;
            }
            g_CurrentEntity->move_speed_current = 0xC8;
            (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        }
        break;
    case 2:
        g_CurrentEntity->action_ticks_counter = 0x1EU;
        g_CurrentEntity->move_speed_current = 0x78;
        g_CurrentEntity->action_state = (u8) (YAWN(g_CurrentEntity)->form + 3);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, YAWN(g_CurrentEntity)->bite_turn_dir * 0x10);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
            YAWN(g_CurrentEntity)->bite_turn_dir = 0;
            g_CurrentEntity->hit_state = 0;
        }
        break;
    case 4:
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x78;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, YAWN(g_CurrentEntity)->bite_turn_dir * 0x10);
        if (yawn_anim_advance_a3(0, 0x200) & 0xFF) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
            YAWN(g_CurrentEntity)->bite_turn_dir = 0;
            g_CurrentEntity->hit_state = 0;
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    yawn_post_move_a3(0x30);
}

void yawn_action_rear(void) {
    void yawn_turn_accel();
    void yawn_post_move(s16 max_turn);
    u8 yawn_anim_advance_u8() __asm__("yawn_anim_advance");
    s32 unused;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x78;
        YAWN(g_CurrentEntity)->bite_count = 0;
        if ((((u8) YAWN(g_CurrentEntity)->player_near_death) != 0) && (YAWN(g_CurrentEntity)->form != 0)) {
            g_CurrentEntity->action_state = 3;
            break;
        }
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + yawn_anim_advance_u8(YAWN(g_CurrentEntity)->form, 0x200);
        break;
    case 2:
        if (YAWN(g_CurrentEntity)->form == 0) {
            YAWN(g_CurrentEntity)->form = 1;
            g_CurrentEntity->action_state = 3;
        } else {
            YAWN(g_CurrentEntity)->form = 0;
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        }
        break;
    case 3:
        g_CurrentEntity->action_state = 4;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x46;
        g_CurrentEntity->action_ticks_counter = 0x3C;
        /* fallthrough */
    case 4:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
        yawn_anim_advance_u8(YAWN(g_CurrentEntity)->form, 0x200);
        if ((g_CurrentEntity->animation_frame_id == 0xF) || (g_CurrentEntity->animation_frame_id == 0x28)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        if (!({ (s32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--; })) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        }
    }
    yawn_turn_accel();
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move(0x30);
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void yawn_action_swallow(void) {
    u8 yawn_anim_advance_a4() __asm__("yawn_anim_advance");
    void yawn_post_move(s16 max_turn);
    void yawn_spawn_dust();
    void *joints;                                     /* compiler-managed */

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0xE;
        ++YAWN(g_CurrentEntity)->bite_count;
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags | 2;
        g_PlayerEntity.status_flags |= 2;
        g_PlayerEntity.status_flags |= 4;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 0xD, 0, 0);
        YAWN(g_CurrentEntity)->swallow_cooldown = 0x1E;
        /* fallthrough */
    case 1:
        yawn_anim_advance_a4(0, 0x200);
        LIT(s32, sp_tmp00) = __builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + __builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z));
        g_CurrentEntity->move_speed_current = 0xA0;
        if (LIT(s32, sp_tmp00) < 0x9C4) {
            g_CurrentEntity->move_speed_current = 0;
        }
        (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(s32, g_PlayerPosX), (s32 *)0x30);
        if (g_CurrentEntity->animation_frame_id == 0xA) {
            (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)3, NULL);
        }
        if (g_CurrentEntity->animation_frame_id == 0x14) {
            g_CurrentEntity->hit_state = 0;
            if (LIT(s32, sp_tmp00) >= 0x7D1) {
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
                g_CurrentEntity->sub_state = 0;
                ENTITY_ACTION_S16(g_CurrentEntity) = 1;
                g_PlayerEntity.status_flags &= 0xFD;
                g_PlayerEntity.status_flags &= 0xFB;
                g_PlayerEntity.hit_state = 0;
                break;
            }
            {
                g_CurrentEntity->action_state = 2;
                g_CurrentEntity->hit_state = 1;
                LIT(s32, sp_tmp14) = 0xD;
                while ((LIT(s32, sp_tmp14) = LIT(s32, sp_tmp14) - 1) != -1) {
                    *(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp14) * ENTITY_SIZE)) |= 4;
                }
                joints = ((void *) g_PlayerEntity.joints);
                FIELD_AT(joints, Joint, ->flags, s8) = (s8) (FIELD_AT(joints, Joint, ->flags, u8) & 0xFE);
                FIELD_AT(joints, Joint, [1].flags, s8) = (s8) (FIELD_AT(joints, Joint, [1].flags, u8) & 0xFE);
                FIELD_AT(joints, Joint, [9].flags, s8) = (s8) (FIELD_AT(joints, Joint, [9].flags, u8) & 0xFE);
                FIELD_AT(joints, Joint, [12].flags, s8) = (s8) (FIELD_AT(joints, Joint, [12].flags, u8) & 0xFE);
                g_CurrentEntity->move_speed_current = 0;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vy) = 0x3E8;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, NULL, &JOINT(joints)->world, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, &JOINT(joints)->world, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, NULL, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 3), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 4), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 5), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 6), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 7), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 8), (s32 *)0x70, (s32 *)0x484860, (s32 *)0xA0A0A0);
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)5, NULL);
                (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)2, (g_PlayerEntity.id & 1 & 0xFF) + 0x17, NULL, &g_CurrentEntity->pos_x);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)6, NULL);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)7, (s32 *)0xA);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)6, (s32 *)0x16);
                (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xF, (s32 *)0xFA, NULL);
                (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x1E, (s32 *)0xFA, NULL, (s32 *)0xF);
            }
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        break;
    case 2:
        joints = (void *)g_CurrentEntity->joints;
        g_CurrentEntity->action_state = 3;
        (g_ApiCallPtr = api_matrix_relative)(&JOINT(joints)->world, LITP(s32, g_PlayerCoordM00), &g_YawnCaptureMatrix);
        LIT(s16, sp_svec_vx) = 0;
        LIT(s16, sp_svec_vy) = 0;
        LIT(s16, sp_svec_vz) = -0xC8;
        (g_ApiCallPtr = api_RotMatrix)(LITP(s32, sp_svec), LITP(s32, sp_matrix));
        (g_ApiCallPtr = api_MulMatrix2)(LITP(s32, sp_matrix), &g_YawnCaptureMatrix);
        g_YawnCaptureMatrix_tx = 0x937;
        g_YawnCaptureMatrix_ty = 0x2BC;
        g_YawnCaptureMatrix_tz = 0;
        YAWN(g_CurrentEntity)->swallow_scale = 0x1388U;
        (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xF, (s32 *)0xFA, NULL);
        /* fallthrough */
    case 3:
        joints = (void *)g_CurrentEntity->joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(&JOINT(joints)->world, &g_YawnCaptureMatrix, LITP(s32, g_PlayerCoordM00));
        g_CurrentEntity->action_state = g_CurrentEntity->action_state + yawn_anim_advance_a4(0, 0x200);
        if (((u8) g_CurrentEntity->animation_frame_id >= 0x43U) && ((u8) g_CurrentEntity->animation_frame_id < 0x4BU)) {
            if (g_CurrentEntity->animation_frame_id == 0x43) {
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)6, NULL);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)4, NULL);
                (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)4, (s32 *)0xFA, NULL);
            }
            if (g_CurrentEntity->animation_frame_id == 0x4A) {
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)7, NULL);
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)4, NULL);
                (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)0xF, (s32 *)0xC8, NULL);
                (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x2D, (s32 *)0xC8, NULL, (s32 *)0xF);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)8, NULL);
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)6, (s32 *)0x10);
            }
            g_YawnCaptureMatrix_tx -= 0x78;
            g_YawnCaptureMatrix_ty -= 0x28;
            joints = ((void *) g_PlayerEntity.joints);
            FIELD_AT(joints, Joint, [2].flags, s8) = (s8) (FIELD_AT(joints, Joint, [2].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [10].flags, s8) = (s8) (FIELD_AT(joints, Joint, [10].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [13].flags, s8) = (s8) (FIELD_AT(joints, Joint, [13].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [11].flags, s8) = (s8) (FIELD_AT(joints, Joint, [11].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [14].flags, s8) = (s8) (FIELD_AT(joints, Joint, [14].flags, u8) & 0xFE);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x1F4;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, NULL, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, (s32 *)3, NULL, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 2), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, NULL, NULL, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
            LIT(s16, sp_svec_vx) = 0;
            LIT(s16, sp_svec_vy) = (s16) ((s32) ((((s16) g_CurrentEntity->angle - ((s16) g_PlayerEntity.angle)) + 0x800) & 0xFFF) / 8);
            LIT(s16, sp_svec_vz) = 0;
            (g_ApiCallPtr = api_RotMatrix)(LITP(s32, sp_svec), LITP(s32, sp_matrix));
            (g_ApiCallPtr = api_MulMatrix)(&g_YawnCaptureMatrix, LITP(s32, sp_matrix));
            YAWN(g_CurrentEntity)->swallow_scale = (u16) (YAWN(g_CurrentEntity)->swallow_scale + 0x64);
        }
        break;
    case 4:
        g_CurrentEntity->action_state = 5;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->move_speed_current = 0;
        g_PlayerEntity.health = -1;
        /* fallthrough */
    case 5:
        g_CurrentEntity->action_state += yawn_anim_advance_a4(1, 0x200);
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        if (g_CurrentEntity->animation_frame_id == 0xF) {
            LIT(s32, sp_tmp00) = 0xD;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                *(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) |= 4;
            }
        }
        break;
    case 6:
        g_CurrentEntity->action_state = 7;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_ticks_counter = 0x96;
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags | 4;
        /* fallthrough */
    case 7:
        g_YawnCaptureMatrix_tx -= 0xA;
        g_YawnCaptureMatrix_ty -= 6;
        yawn_anim_advance_a4(0, 0x200);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 8;
        }
        if ((s16) g_CurrentEntity->action_ticks_counter == 0x6E) {
            joints = ((void *) g_PlayerEntity.joints);
            FIELD_AT(joints, Joint, [3].flags, u8) = (u8) (FIELD_AT(joints, Joint, [3].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [4].flags, u8) = (u8) (FIELD_AT(joints, Joint, [4].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [5].flags, u8) = (u8) (FIELD_AT(joints, Joint, [5].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [6].flags, u8) = (u8) (FIELD_AT(joints, Joint, [6].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [7].flags, u8) = (u8) (FIELD_AT(joints, Joint, [7].flags, u8) & 0xFE);
            FIELD_AT(joints, Joint, [8].flags, u8) = (u8) (FIELD_AT(joints, Joint, [8].flags, u8) & 0xFE);
        }
        break;
    case 8:
    }
    yawn_post_move(0x30);
    if ((u8) g_CurrentEntity->action_state >= 2U) {
        if (!((g_ApiCallPtr = api_rand)() & 1)) {
            yawn_spawn_dust(0);
        }
        if (!((g_ApiCallPtr = api_rand)() & 1)) {
            yawn_spawn_dust(1);
        }
        if (!((g_ApiCallPtr = api_rand)() & 1)) {
            yawn_spawn_dust(2);
        }
    }
    if ((s16) YAWN(g_CurrentEntity)->swallow_scale != 0) {
        joints = (void *)g_CurrentEntity->joints;
        SCRATCH_VEC->x = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = (s16) YAWN(g_CurrentEntity)->swallow_scale - 0x12C;
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 3), LITP(s32, sp_vec));
        SCRATCH_VEC->x = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = (s16) YAWN(g_CurrentEntity)->swallow_scale;
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_TRANSFORM(joints, 2), LITP(s32, sp_vec));
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void yawn_action_entry(void) {
    s32 yawn_anim_advance_a5() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a4() __asm__("yawn_post_move");
    extern struct { u16 a; u16 b; } yawn_entry_path[];

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 7;
        YAWN(g_CurrentEntity)->move_speed = 0;
        g_CurrentEntity->move_speed_current = 0xBE;
        if (g_CurrentEntity->behavior_flags == 4) {
            g_CurrentEntity->move_speed_current = 0x82;
            YAWN(g_CurrentEntity)->waypoint_index = 3;
        }
        g_CurrentEntity->action_ticks_counter = 2U;
        g_CurrentEntity->hit_state = 1;
        if (g_CurrentEntity->id == 0xD) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_ScenarioFlags, 0x10);
        }
    }
    LIT(s32, sp_vec_vx) = yawn_entry_path[YAWN(g_CurrentEntity)->waypoint_index].a;
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = yawn_entry_path[YAWN(g_CurrentEntity)->waypoint_index].b;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x30);
    if ((g_ApiCallPtr = api_SquareRoot0)(((LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x) * (LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x)) + ((LIT(s32, sp_vec_vz) - g_CurrentEntity->pos_z) * (LIT(s32, sp_vec_vz) - (g_CurrentEntity->pos_z)))) < 0x1F4U) {
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
        }
        YAWN(g_CurrentEntity)->waypoint_index++;
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
    if (g_CurrentEntity->behavior_flags & 0x3F & 0xFF) {
        LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2904);
        LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x3FF) * -1;
        LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) + 0x2904);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
    } else {
        LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x10CC);
        LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x3FF) * -1;
        LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) + 0x5C94);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
    }
    yawn_anim_advance_a5(0, 0x200);
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move_a4(0x30);
}

void yawn_action_emerge(void) {
    u8 yawn_anim_advance_a6() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a5() __asm__("yawn_post_move");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0;
        YAWN(g_CurrentEntity)->form = 1;
        YAWN(g_CurrentEntity)->waypoint_index = 0;
        /* fallthrough */
    case 1:
        if (yawn_anim_advance_a6(0U, 0x200) & 0xFF) {
            g_CurrentEntity->action_state = 2;
            if (g_CurrentEntity->behavior_flags == 0) {
                g_CurrentEntity->action_state = 6;
            }
        }
        return;
    case 2:
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = (YAWN(g_CurrentEntity)->form * 0xA) + 3;
        (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xB4, 0x12);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0xB4, 0, 0x17);
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state += yawn_anim_advance_a6(0U, 0x200);
        if (g_CurrentEntity->animation_frame_id == ((YAWN(g_CurrentEntity)->form * 5) + 8)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
            g_CurrentEntity->move_speed_current = 0xBE;
        }
        if ((u8) g_CurrentEntity->animation_frame_id >= 0x18U) {
            g_CurrentEntity->move_speed_current = 0x32;
            LIT(s32, sp_vec_vx) = ((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74;
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x13, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x15, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x13, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x15, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x13, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x13, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0xE, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x7D0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0xE74);
            (g_ApiCallPtr = api_effect_create_billboard)(0x11, 0x11, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        yawn_post_move_a5(0x30);
        return;
    case 4:
        g_CurrentEntity->action_state = 5;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 7;
        /* fallthrough */
    case 5:
        g_CurrentEntity->action_state += yawn_anim_advance_a6(0U, 0x200);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
        return;
    case 6:
        g_CurrentEntity->action_state = 7;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x46;
        g_CurrentEntity->action_ticks_counter = 0x5A;
        /* fallthrough */
    case 7:
        yawn_anim_advance_a6(YAWN(g_CurrentEntity)->form, 0x200);
        if ((g_CurrentEntity->animation_frame_id != 0xF) && (g_CurrentEntity->animation_frame_id != 0x28)) {

        } else {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
            g_CurrentEntity->hit_state = 0;
            LIT(s32, sp_tmp00) = 0xD;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                *(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = (*(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) & 0xFB);
            }
        }
    }
}

void yawn_action_flee(void) {
    void yawn_turn_accel();
    s32 yawn_anim_advance_a7() __asm__("yawn_anim_advance");
    void yawn_post_move(s16 max_turn);
    extern struct { u16 a; u16 b; } yawn_flee_path[];
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 0xF;
        g_CurrentEntity->move_speed_current = 0xB4;
        YAWN(g_CurrentEntity)->wall_frames = 0;
        YAWN(g_CurrentEntity)->waypoint_index = 4;
        if ((g_CurrentEntity->pos_x < 0x2328) && (g_CurrentEntity->pos_z < 0x59D8)) {
            YAWN(g_CurrentEntity)->waypoint_index = 3;
        }
        if (g_CurrentEntity->pos_z < 0x3C8C) {
            YAWN(g_CurrentEntity)->waypoint_index = 2;
        }
        if (g_CurrentEntity->pos_z < 0x2328) {
            YAWN(g_CurrentEntity)->waypoint_index = 0;
        }
        if (g_CurrentEntity->pos_x >= 0x2581) {
            YAWN(g_CurrentEntity)->waypoint_index = 1;
        }
        YAWN(g_CurrentEntity)->form = 0;
        YAWN(g_CurrentEntity)->bite_turn_dir = 1;
        YAWN(g_CurrentEntity)->speed_accel = 1;
        YAWN(g_CurrentEntity)->move_speed = 0;
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
        g_YawnFleeTimer = 0;
    }
    if ((u8) YAWN(g_CurrentEntity)->waypoint_index >= 7U) {
        LIT(s32, sp_vec_vx) = ((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x10CC;
        LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x3FF) * -1;
        LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x58AC;
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, g_DeadMoveValue, ADDR_sp_vec, 0);
    }
    LIT(s32, sp_vec_vx) = yawn_flee_path[YAWN(g_CurrentEntity)->waypoint_index].a;
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = yawn_flee_path[YAWN(g_CurrentEntity)->waypoint_index].b;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x30U);
    if ((g_ApiCallPtr = api_SquareRoot0)(((LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x) * (LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x)) + ((LIT(s32, sp_vec_vz) - g_CurrentEntity->pos_z) * (LIT(s32, sp_vec_vz) - g_CurrentEntity->pos_z))) < 0x320U) {
        ++YAWN(g_CurrentEntity)->waypoint_index;
        if (YAWN(g_CurrentEntity)->waypoint_index == 1) {
            YAWN(g_CurrentEntity)->waypoint_index = 2;
        }
        YAWN(g_CurrentEntity)->move_speed = 0;
        if (YAWN(g_CurrentEntity)->waypoint_index == 6) {
            LIT(s32, sp_tmp00) = 4;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                *(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = (*(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) | 4);
            }
        }
        if (YAWN(g_CurrentEntity)->waypoint_index == 7) {
            LIT(s32, sp_tmp00) = 4;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                *(u8 *)(ADDR_g_EnemySlot4 + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = (*(u8 *)(ADDR_g_EnemySlot4 + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) | 4);
            }
        }
        if (YAWN(g_CurrentEntity)->waypoint_index == 8) {
            LIT(s32, sp_tmp00) = 5;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                *(u8 *)(ADDR_g_EnemySlot8 + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) = (*(u8 *)(ADDR_g_EnemySlot8 + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) | 4);
            }
        }
        if (YAWN(g_CurrentEntity)->waypoint_index == 9) {
            g_CurrentEntity->behavior_flags = g_CurrentEntity->behavior_flags | 0x80;
        }
    }
    if ((u8) YAWN(g_CurrentEntity)->wall_frames >= 0x97U) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 8, 0);
    }
    if (g_YawnFleeTimer >= 0x2329) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 8, 0);
    }
    if ((u8) YAWN(g_CurrentEntity)->waypoint_index < 6U) {
        g_YawnFleeTimer++;
    }
    yawn_anim_advance_a7(0, 0x100);
    if ((u8) YAWN(g_CurrentEntity)->waypoint_index < 4U) {
        yawn_turn_accel();
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move(0x38);
}

void yawn_action_reposition(void) {
    void yawn_turn_accel();
    s32 yawn_anim_advance_a8() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a6() __asm__("yawn_post_move");
    extern struct { u16 a; u16 b; } yawn_reposition_path[];

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 0xF;
        g_CurrentEntity->move_speed_current = 0xB4;
        YAWN(g_CurrentEntity)->wall_frames = 0;
        YAWN(g_CurrentEntity)->waypoint_index = 3;
        if (g_CurrentEntity->pos_x >= 0x2329) {
            YAWN(g_CurrentEntity)->waypoint_index = 2;
        }
        if (g_CurrentEntity->pos_z < 0x4A38) {
            YAWN(g_CurrentEntity)->waypoint_index = 1;
        }
        if (g_CurrentEntity->pos_z < 0x3A98) {
            YAWN(g_CurrentEntity)->waypoint_index = 0;
        }
        YAWN(g_CurrentEntity)->form = 0;
        YAWN(g_CurrentEntity)->bite_turn_dir = 1;
        YAWN_FIELD(g_CurrentEntity, speed_accel) = 1;
        YAWN(g_CurrentEntity)->move_speed = 0;
    }
    if ((u8) YAWN(g_CurrentEntity)->wall_frames >= 0x97U) {
        g_CurrentEntity->action_state = 0U;
    }
    LIT(s32, sp_vec_vx) = yawn_reposition_path[YAWN(g_CurrentEntity)->waypoint_index].a;
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = yawn_reposition_path[YAWN(g_CurrentEntity)->waypoint_index].b;
    (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x30);
    if ((g_ApiCallPtr = api_SquareRoot0)(((LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x) * (LIT(s32, sp_vec_vx) - g_CurrentEntity->pos_x)) + ((LIT(s32, sp_vec_vz) - g_CurrentEntity->pos_z) * (LIT(s32, sp_vec_vz) - (g_CurrentEntity->pos_z)))) < 0x320U) {
        YAWN(g_CurrentEntity)->move_speed = 0;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
    }
    yawn_anim_advance_a8(0, 0x100);
    yawn_turn_accel();
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move_a6(0x38);
}

void yawn_damaged_run(void) {
    void yawn_turn_accel();
    u8 yawn_anim_advance_a9() __asm__("yawn_anim_advance");
    s32 yawn_post_move_a7() __asm__("yawn_post_move");
    void *joint;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = (s8) (YAWN(g_CurrentEntity)->form + 9);
        g_CurrentEntity->blend_counter = 7;
        YAWN(g_CurrentEntity)->move_speed = 0;
        g_CurrentEntity->move_speed_current = 0x46;
        joint = ENTITY_AS(g_CurrentEntity, joints, void *);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x12C;
        if (JOINT(joint)->world.t[1] < -0x5DC) {
            LIT(s32, sp_vec_vy) = 0x12C;
        }
        (g_ApiCallPtr = api_effect_create_billboard)(0, 8, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state += yawn_anim_advance_a9(0, 0x200);
        break;
    case 2:
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x78;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0x3C;
        /* fallthrough */
    case 3:
        yawn_anim_advance_a9(0, 0x200);
        if ((g_CurrentEntity->animation_frame_id != 0xF) && (g_CurrentEntity->animation_frame_id != 0x28)) {

        } else {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        LIT(s32, sp_tmp00) = 1;
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) >= 0x1F) {
            LIT(s32, sp_tmp00) = -1;
        }
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, LIT(s32, sp_tmp00) * 0x30);
                if ((u16) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
            YAWN(g_CurrentEntity)->form = 1;
            g_CurrentEntity->hit_state = 0;
            YAWN(g_CurrentEntity)->bite_recovery = 0xDA;
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        yawn_turn_accel();
        joint;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
    yawn_post_move_a7(0x30);
}

void yawn_turn_accel(void) {
    u16 biased_speed;

    biased_speed = ((u16) YAWN(g_CurrentEntity)->move_speed) + 0x384;
    if (biased_speed >= 0x709U) {
        YAWN(g_CurrentEntity)->move_speed = (s16) (((u16) YAWN(g_CurrentEntity)->move_speed) - YAWN_FIELD(g_CurrentEntity, speed_accel));
        YAWN_FIELD(g_CurrentEntity, speed_accel) = (s8) ((s8) YAWN_FIELD(g_CurrentEntity, speed_accel) / 64);
        YAWN_FIELD(g_CurrentEntity, speed_accel) *= -1;
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
    if ((u8) YAWN_FIELD(g_CurrentEntity, speed_accel) & 0x3F & 0xFF) {
        YAWN_FIELD(g_CurrentEntity, speed_accel) = (s8) (YAWN_FIELD(g_CurrentEntity, speed_accel) * 2);
    }
    YAWN(g_CurrentEntity)->move_speed = (s16) (((u16) YAWN(g_CurrentEntity)->move_speed) + YAWN_FIELD(g_CurrentEntity, speed_accel));
}

void yawn_die_run(void) {
    u8 yawn_anim_advance_a10() __asm__("yawn_anim_advance");
    void yawn_post_move(s16 max_turn);
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 6;
        g_CurrentEntity->blend_counter = 7;
        YAWN(g_CurrentEntity)->move_speed = 0;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->hit_state = 1;
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x12C;
        if (JOINT(joints)->world.t[1] < -0x5DC) {
            LIT(s32, sp_vec_vy) = 0x1F4;
        }
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0xBU, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(0, 8U, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0U);
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state += yawn_anim_advance_a10(0, 0x200);
        (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
        yawn_post_move(0x30);
        break;
    case 2:
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 7;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x46;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0U);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30U);
        g_CurrentEntity->action_state += yawn_anim_advance_a10(0, 0x200);
        (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
        yawn_post_move(0x30);
        break;
    case 4:
        g_CurrentEntity->action_state = 5;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xF;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0x64;
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0U);
        /* fallthrough */
    case 5:
        g_CurrentEntity->action_state += yawn_anim_advance_a10(0, 0x200);
        if (g_CurrentEntity->animation_frame_id == 0x32) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x1F4;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0xBU, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(0, 8U, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(5, 0x96U, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x2D, 0x96U, 0, 5);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
        yawn_post_move(0x30);
        break;
    case 6:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->status_flags = g_CurrentEntity->status_flags | 6;
        LIT(s32, sp_tmp00) = 0xD;
        while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
            *(u8 *)(ADDR_g_EnemiesList + (LIT(s32, sp_tmp00) * ENTITY_SIZE)) |= 4;
        }
        g_CurrentEntity->action_state = 7;
        YAWN(g_CurrentEntity)->death_scale = 0x1000;
        g_CurrentEntity->action_ticks_counter = 0;
        /* fallthrough */
    case 7:
        (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
        yawn_post_move(0x30);
        joints = g_CurrentEntity->joints;
        SCRATCH_VEC->x = ((0x1000 - YAWN(g_CurrentEntity)->death_scale) / 2) + 0x1000;
        LIT(s32, sp_vec_vy) = (s32) YAWN(g_CurrentEntity)->death_scale;
        LIT(s32, sp_vec_vz) = ((0x1000 - YAWN(g_CurrentEntity)->death_scale) / 2) + 0x1000;
        g_CurrentEntity->has_enter_switch_zone = g_CurrentEntity->has_enter_switch_zone | 0x80;
        (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, g_CurrentEntity->coord_m);
        (g_ApiCallPtr = api_ScaleMatrix)(g_CurrentEntity->coord_m, ADDR_sp_vec);
        g_CurrentEntity->pos_y = g_CurrentEntity->pos_y + 2;
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 3), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 4), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 5), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 6), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 7), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 8), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 9), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 10), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 11), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 12), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 13), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 14), ADDR_sp_vec);
        if ((s16) g_CurrentEntity->action_ticks_counter < 0x50) {
            ++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16);
            if ((s16) (++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 7) == 0) {
                (g_ApiCallPtr = api_clut_fade_add_delta)(-1, 0U, 0, 0, 0x200, (s32) g_CurrentEntity->id);
            }
            if ((s16) (++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 7) == 0) {
                (g_ApiCallPtr = api_clut_fade_add_delta)(0, -1U, 0, 0, 0x200, (s32) g_CurrentEntity->id);
            }
            if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 0x1F) == 0) {
                (g_ApiCallPtr = api_clut_fade_add_delta)(0, 0U, -1, 0, 0x200, (s32) g_CurrentEntity->id);
            }
        }
        YAWN(g_CurrentEntity)->death_scale -= 0x14;
        if (YAWN(g_CurrentEntity)->death_scale < 0x1F4) {
            g_CurrentEntity->action_state = 8;
            g_CurrentEntity->action_ticks_counter = 0;
            LIT(s32, sp_tmp00) = 0xD;
            while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
                (g_ApiCallPtr = api_billboard_set_color)((LIT(s32, sp_tmp00) * ENTITY_SIZE) + ADDR_g_EnemySlot0_Shadow, 1U, 2, 0xAFDF9F);
            }
        }
        break;
    case 8:
        (g_ApiCallPtr = api_entity_add_speed_xz)(YAWN(g_CurrentEntity)->move_speed);
        yawn_post_move(0x30);
        joints = g_CurrentEntity->joints;
        SCRATCH_VEC->x = (s32) (0x1706 - ((s16) g_CurrentEntity->action_ticks_counter * 0x10));
        LIT(s32, sp_vec_vy) = (s32) (0x1F4 - (s16) g_CurrentEntity->action_ticks_counter);
        LIT(s32, sp_vec_vz) = (s32) (0x1706 - ((s16) g_CurrentEntity->action_ticks_counter * 0x10));
        g_CurrentEntity->has_enter_switch_zone = g_CurrentEntity->has_enter_switch_zone | 0x80;
        (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, g_CurrentEntity->coord_m);
        (g_ApiCallPtr = api_ScaleMatrix)(g_CurrentEntity->coord_m, ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 3), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 4), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 5), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 6), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 7), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 8), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 9), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 10), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 11), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 12), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 13), ADDR_sp_vec);
        (g_ApiCallPtr = api_ScaleMatrix)(JOINT_N_WORLD(joints, 14), ADDR_sp_vec);
        if (++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) >= 0x15F) {
            g_CurrentEntity->action_state = 9;
        }
    }
    if ((u16) YAWN(g_CurrentEntity)->death_fx_timer < 0x320U) {
        ++YAWN(g_CurrentEntity)->death_fx_timer;
        if (!(YAWN(g_CurrentEntity)->death_fx_timer & 0xF & 0xFFFF)) {
            LIT(s32, sp_vec_vy) = 0x1F4;
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            joints = g_CurrentEntity->joints;
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_rand)() & 0xF;
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
        }
        if (!((YAWN(g_CurrentEntity)->death_fx_timer + 3) & 7) && ((u16) YAWN(g_CurrentEntity)->death_fx_timer < 0x258U)) {
            LIT(s32, sp_vec_vy) = 0x1F4;
            SCRATCH_VEC->x = 0;
            LIT(s32, sp_vec_vz) = 0;
            joints = g_CurrentEntity->joints;
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            SCRATCH_VEC->x = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x200);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11U, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
        }
    }
}

void player_anim_death_alt(void) {
    extern s32 (*player_death_alt_tbl[])();
    player_death_alt_tbl[g_PlayerEntity.action_behavior]();
}

void player_anim_death_alt_body(void) {
    s32 joints;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.hit_state = 1;
        /* fallthrough */
    case 1:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, ((s32 *) g_PlayerWork.grab_anim_header), g_PlayerWork.grab_anim_base, 0x400);
        if (g_EnemySlot0_Health < 0) {
            ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
            g_PlayerEntity.status_flags &= 0xFD;
            g_PlayerEntity.status_flags &= 0xFB;
            g_PlayerEntity.hit_state = 0;
        }
        return;
    case 2:
        joints = g_EnemySlot0_Joints;
        g_PlayerEntity.has_enter_switch_zone |= 0x80;
        (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 0), &g_YawnCaptureMatrix, ADDR_g_PlayerCoordM00);
    }
}

void yawn_chain_follow(void *parent, void *joint, s16 max_turn) {
    s32 dx;
    s32 dz;
    u16 dist;
    void *head;
    s32 unused2;
    VECTOR new_pos;
    LIT(s32, sp_tmp00) = (s32) JOINT(joint)->rotation.vy;
    head = LITP(void, g_EnemiesList);
    SCRATCH_VEC->x = (s32) (JOINT(joint)->transform.t[0] << 0xC);
    LIT(s32, sp_vec_vy) = JOINT(joint)->transform.t[1] << 0xC;
    LIT(s32, sp_vec_vz) = JOINT(joint)->transform.t[2] << 0xC;
    (g_ApiCallPtr = api_ApplyMatrixLV)(&JOINT(parent)->world, ADDR_sp_vec, ADDR_sp_vec);
    new_pos = LIT(VECTOR, sp_vec);
    SCRATCH_VEC->x = (s32) ((SCRATCH_VEC->x / 35) * 0x22);
    LIT(s32, sp_vec_vy) = (s32) ((LIT(s32, sp_vec_vy) / 35) * 0x22);
    LIT(s32, sp_vec_vz) = (s32) ((LIT(s32, sp_vec_vz) / 35) * 0x22);
    SCRATCH_VEC->x = (s32) (SCRATCH_VEC->x + (JOINT(parent)->world.t[0] << 0xC));
    LIT(s32, sp_vec_vy) = (s32) (LIT(s32, sp_vec_vy) + (JOINT(parent)->world.t[1] << 0xC));
    LIT(s32, sp_vec_vz) = (s32) (LIT(s32, sp_vec_vz) + (JOINT(parent)->world.t[2] << 0xC));
    new_pos.vx += JOINT(parent)->world.t[0] << 0xC;
    new_pos.vy += JOINT(parent)->world.t[1] << 0xC;
    new_pos.vz += JOINT(parent)->world.t[2] << 0xC;
    dx = SCRATCH_VEC->x - (JOINT(joint)->world.t[0] << 0xC);
    dz = LIT(s32, sp_vec_vz) - (JOINT(joint)->world.t[2] << 0xC);
    if ((dist = (g_ApiCallPtr = api_SquareRoot0)(((dx >> 0xC) * (dx >> 0xC)) + ((dz >> 0xC) * (dz >> 0xC)))) < 0x32U) {
        goto block;
    }
        dx = dx >> 4;
        dz = dz >> 4;
        if (dx != 0) {
            JOINT(joint)->rotation.vy = ((dx < 0) ? 0x800 : 0x1000) - (g_ApiCallPtr = api_catan)((dz * 0x1000) / dx);
        } else {
            JOINT(joint)->rotation.vy = (dz > 0) ? 0xC00 : 0x400;
        }
        LIT(s32, sp_tmp04) = ((JOINT(joint)->rotation.vy - LIT(s32, sp_tmp00)) + max_turn) & 0xFFF;
        if ((max_turn + max_turn) < LIT(s32, sp_tmp04)) {
            JOINT(joint)->rotation.vy = (s16) ((u16) LIT(s32, sp_tmp00) - (u16) max_turn);
            if ((max_turn + 0x800) >= LIT(s32, sp_tmp04)) {
                JOINT(joint)->rotation.vy = (s16) ((u16) JOINT(joint)->rotation.vy + ((u16) max_turn + (u16) max_turn));
            }
        } else if (ENT(head)->move_speed_current < 0x64) {
            JOINT(joint)->rotation.vy = (s16) (u16) LIT(s32, sp_tmp00);
        }
        dx = (s32) JOINT(parent)->rotation.vy;
        if ((u8) JOINT(joint)->index < 5U) {
            if (JOINT(joint)->index == 3) {
                dx = (s32) g_EnemySlot0_Angle;
            }
            if ((dz = ((JOINT(joint)->rotation.vy - dx) + 0x400) & 0xFFF) >= 0x801) {
                JOINT(joint)->rotation.vy = (s16) (dx - 0x400);
                if (dz < 0xC00) {
                    JOINT(joint)->rotation.vy = (s16) ((u16) dx + 0x400);
                }
            }
        }
block:
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    (g_ApiCallPtr = api_RotMatrix)((s32) (&JOINT(joint)->rotation), (s32) (&JOINT(joint)->world));
    JOINT(joint)->world.t[0] = (s32) (new_pos.vx >> 0xC);
    JOINT(joint)->world.t[1] = (s32) (new_pos.vy >> 0xC);
    JOINT(joint)->world.t[2] = (s32) (new_pos.vz >> 0xC);
}

void yawn_chain_align(void *parent, void *joint) {
    s32 dx;
    s32 dz;

    LIT(s32, sp_vec_vx) = JOINT(joint)->transform.t[0] << 0xC;
    LIT(s32, sp_vec_vy) = JOINT(joint)->transform.t[1] << 0xC;
    LIT(s32, sp_vec_vz) = JOINT(joint)->transform.t[2] << 0xC;
    (g_ApiCallPtr = api_ApplyMatrixLV)(&JOINT(parent)->world, LITP(void, sp_vec), ADDR_sp_vec);
    LIT(s32, sp_vec_vx) = (s32) ((LIT(s32, sp_vec_vx) / 35) * 0x22);
    LIT(s32, sp_vec_vy) = (s32) ((LIT(s32, sp_vec_vy) / 35) * 0x22);
    LIT(s32, sp_vec_vz) = (s32) ((LIT(s32, sp_vec_vz) / 35) * 0x22);
    LIT(s32, sp_vec_vx) = (s32) (LIT(s32, sp_vec_vx) + (JOINT(parent)->world.t[0] << 0xC));
    LIT(s32, sp_vec_vy) = (s32) (LIT(s32, sp_vec_vy) + (JOINT(parent)->world.t[1] << 0xC));
    LIT(s32, sp_vec_vz) = (s32) (LIT(s32, sp_vec_vz) + (JOINT(parent)->world.t[2] << 0xC));
    dx = LIT(s32, sp_vec_vx) - (JOINT(joint)->world.t[0] << 0xC);
    dz = LIT(s32, sp_vec_vz) - (JOINT(joint)->world.t[2] << 0xC);
    dx = dx >> 4;
    dz = dz >> 4;
    if (dx != 0) {
        LIT(s32, sp_tmp04) = ((dx < 0) ? 0x800 : 0x1000) - (g_ApiCallPtr = api_catan)((dz * 0x1000) / dx);
    } else {
        LIT(s32, sp_tmp04) = (dz > 0) ? 0xC00 : 0x400;
    }
    if ((((LIT(s32, sp_tmp04) - JOINT(joint)->rotation.vy) + 0x800) & 0xFFF) < 0x800) {
        LIT(s32, sp_tmp04) = (s32) ((JOINT(joint)->rotation.vy - LIT(s32, sp_tmp04)) & 0xFFF);
        if (LIT(s32, sp_tmp04) >= 0x31) {
            LIT(s32, sp_tmp04) = 0x30;
        }
        JOINT(parent)->rotation.vy = (u16) (((u16) JOINT(parent)->rotation.vy) - (u16) LIT(s32, sp_tmp04));
    } else {
        LIT(s32, sp_tmp04) = (s32) ((LIT(s32, sp_tmp04) - JOINT(joint)->rotation.vy) & 0xFFF);
        if (LIT(s32, sp_tmp04) >= 0x31) {
            LIT(s32, sp_tmp04) = 0x30;
        }
        JOINT(parent)->rotation.vy = (u16) (((u16) JOINT(parent)->rotation.vy) + (u16) LIT(s32, sp_tmp04));
    }
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    (g_ApiCallPtr = api_RotMatrix)((s32) (&JOINT(parent)->rotation), &JOINT(parent)->world);
}

s32 yawn_anim_advance(u8 reverse, s16 blend_step) {
    s16 *frame_data;
    s32 *anim;
    u8 *frame;
    s32 *entry;
    void *joint;
    s32 unused;
    VECTOR root_delta;

    if (ENTITY_AS(g_CurrentEntity, timing_control, u8) >= 2) {
        --ENTITY_AS(g_CurrentEntity, timing_control, u8);
        return 0;
    }
    anim = ENTITY_AS(g_CurrentEntity, anim_header, void *);
    frame_data = (s16 *) (anim + (s16) (FIELD_AT(anim, AnimEntry, ->frame_offset, s16) / 4));
    LIT(s32, sp_tmp00) = (s16) (FIELD_AT(anim, AnimEntry, [1].frame_offset, s16) / 2);
    anim = ENTITY_AS(g_CurrentEntity, anim_base, void *);
    entry = anim + ENTITY_AS(g_CurrentEntity, animation_id, u8);
    frame = (u8 *) (anim + (u16) (ANIM_ENTRY(entry)->frame_offset >> 2));
    if (reverse != 0) {
        frame += ((ANIM_ENTRY(entry)->frame_count - g_CurrentEntity->animation_frame_id) * 4) - 4;
    } else {
        frame += g_CurrentEntity->animation_frame_id * 4;
    }
    frame_data += *(u16 *)frame * LIT(s32, sp_tmp00);
    joint = ENTITY_AS(g_CurrentEntity, joints, void *);
    JOINT(joint)->transform.t[1] = *++frame_data;
    frame_data += 5;
    LIT(s32, sp_tmp18) = 3;
    LIT(s32, sp_tmp14) = ENTITY_AS(g_CurrentEntity, blend_counter, u8);
    if (LIT(s32, sp_tmp14) == 0) {
        while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
            JOINT(joint)->rotation.vx = *frame_data;
            JOINT(joint)->rotation.vy = *++frame_data;
            JOINT(joint)->rotation.vz = *++frame_data;
            frame_data++;
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
            (*(Joint **)&joint)++;
        }
    } else {
        while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
            LIT(u16, sp_svec_vx) = *frame_data;
            LIT(u16, sp_svec_vy) = *++frame_data;
            LIT(u16, sp_svec_vz) = *++frame_data;
            (g_ApiCallPtr = api_LoadAverageShort12)(&JOINT(joint)->rotation, ADDR_sp_svec, blend_step * LIT(s32, sp_tmp14), blend_step * ((0x1000 / blend_step) - LIT(s32, sp_tmp14)), &JOINT(joint)->rotation);
            frame_data++;
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
            (*(Joint **)&joint)++;
        }
    }
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, joint - 0x150, joint - 0x130);
    (g_ApiCallPtr = api_GsMulCoord0)(joint - 0x130, joint - 0x58, joint - 0x38);
    JOINT(joint - 0x7C)->world.t[0] = JOINT(joint - 0x7C)->world.t[0] << 9;
    JOINT(joint - 0x7C)->world.t[1] = JOINT(joint - 0x7C)->world.t[1] << 9;
    JOINT(joint - 0x7C)->world.t[2] = JOINT(joint - 0x7C)->world.t[2] << 9;
    root_delta = FIELD_AT(joint, Joint, [10].world.t[0], VECTOR);
    root_delta.vy = YAWN(g_CurrentEntity)->ground_y << 9;
    root_delta.vx <<= 9;
    root_delta.vz <<= 9;
    if (LIT(s32, sp_tmp14) == 0) {
        LIT(s32, sp_tmp1C) = *++frame_data;
        if (g_CurrentEntity->animation_frame_id == 0) {
            JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
        }
        JOINT(joint)->rotation.vy = ((u16) JOINT(joint)->rotation.vy) + (LIT(u16, sp_tmp1C) - ((u16) JOINT(joint)->vel_y));
        JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
        JOINT(joint)->rotation.vz = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz) + ((u16) JOINT(joint - 0x174)->rotation.vz);
        frame_data++;
    } else {
        LIT(u16, sp_svec_vx) = 0;
        LIT(s32, sp_tmp1C) = *++frame_data;
        if (g_CurrentEntity->animation_frame_id == 0) {
            JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
        }
        LIT(u16, sp_svec_vy) = (((u16) JOINT(joint)->rotation.vy) + LIT(u16, sp_tmp1C)) - ((u16) JOINT(joint)->vel_y);
        JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
        LIT(u16, sp_svec_vz) = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz) + ((u16) JOINT(joint - 0x174)->rotation.vz);
        (g_ApiCallPtr = api_LoadAverageShort12)(&JOINT(joint)->rotation, ADDR_sp_svec, blend_step * LIT(s32, sp_tmp14), blend_step * ((0x1000 / blend_step) - LIT(s32, sp_tmp14)), &JOINT(joint)->rotation);
        frame_data++;
    }
    LIT(s32, sp_vec_vx) = JOINT(joint)->transform.t[0] << 9;
    LIT(s32, sp_vec_vy) = JOINT(joint)->transform.t[1] << 9;
    LIT(s32, sp_vec_vz) = JOINT(joint)->transform.t[2] << 9;
    (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->world);
    (g_ApiCallPtr = api_ApplyMatrixLV)(joint - 0x38, ADDR_sp_vec, ADDR_sp_vec);
    JOINT(joint)->world.t[0] = JOINT(joint - 0x7C)->world.t[0] + LIT(s32, sp_vec_vx);
    JOINT(joint)->world.t[1] = JOINT(joint - 0x7C)->world.t[1] + LIT(s32, sp_vec_vy);
    JOINT(joint)->world.t[2] = JOINT(joint - 0x7C)->world.t[2] + LIT(s32, sp_vec_vz);
    (*(Joint **)&joint)++;
    LIT(s32, sp_tmp18) = 0xB;
    if (LIT(s32, sp_tmp14) == 0) {
        while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
            LIT(s32, sp_tmp1C) = *++frame_data;
            if (g_CurrentEntity->animation_frame_id == 0) {
                JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
            }
            JOINT(joint)->rotation.vy = ((u16) JOINT(joint)->rotation.vy) + (LIT(u16, sp_tmp1C) - ((u16) JOINT(joint)->vel_y));
            JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
            JOINT(joint)->rotation.vz = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz);
            frame_data++;
            LIT(s32, sp_vec_vx) = JOINT(joint)->transform.t[0] << 9;
            LIT(s32, sp_vec_vy) = JOINT(joint)->transform.t[1] << 9;
            LIT(s32, sp_vec_vz) = JOINT(joint)->transform.t[2] << 9;
            if (LIT(s32, sp_tmp18) == 7) {
                JOINT(joint)->rotation.vz = 0;
            }
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->world);
            (g_ApiCallPtr = api_ApplyMatrixLV)(joint - 0x38, ADDR_sp_vec, ADDR_sp_vec);
            JOINT(joint)->world.t[0] = JOINT(joint - 0x7C)->world.t[0] + LIT(s32, sp_vec_vx);
            JOINT(joint)->world.t[1] = JOINT(joint - 0x7C)->world.t[1] + LIT(s32, sp_vec_vy);
            JOINT(joint)->world.t[2] = JOINT(joint - 0x7C)->world.t[2] + LIT(s32, sp_vec_vz);
            (*(Joint **)&joint)++;
        }
    } else {
        while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
            LIT(u16, sp_svec_vx) = 0;
            LIT(s32, sp_tmp1C) = *++frame_data;
            if (g_CurrentEntity->animation_frame_id == 0) {
                JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
            }
            LIT(u16, sp_svec_vy) = (((u16) JOINT(joint)->rotation.vy) + LIT(u16, sp_tmp1C)) - ((u16) JOINT(joint)->vel_y);
            JOINT(joint)->vel_y = LIT(u16, sp_tmp1C);
            LIT(u16, sp_svec_vz) = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz);
            (g_ApiCallPtr = api_LoadAverageShort12)(&JOINT(joint)->rotation, ADDR_sp_svec, blend_step * LIT(s32, sp_tmp14), blend_step * ((0x1000 / blend_step) - LIT(s32, sp_tmp14)), &JOINT(joint)->rotation);
            frame_data++;
            LIT(s32, sp_vec_vx) = JOINT(joint)->transform.t[0] << 9;
            LIT(s32, sp_vec_vy) = JOINT(joint)->transform.t[1] << 9;
            LIT(s32, sp_vec_vz) = JOINT(joint)->transform.t[2] << 9;
            if (LIT(s32, sp_tmp18) == 7) {
                JOINT(joint)->rotation.vz = 0;
            }
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->world);
            (g_ApiCallPtr = api_ApplyMatrixLV)(joint - 0x38, ADDR_sp_vec, ADDR_sp_vec);
            JOINT(joint)->world.t[0] = JOINT(joint - 0x7C)->world.t[0] + LIT(s32, sp_vec_vx);
            JOINT(joint)->world.t[1] = JOINT(joint - 0x7C)->world.t[1] + LIT(s32, sp_vec_vy);
            JOINT(joint)->world.t[2] = JOINT(joint - 0x7C)->world.t[2] + LIT(s32, sp_vec_vz);
            (*(Joint **)&joint)++;
        }
        --ENTITY_AS(g_CurrentEntity, blend_counter, u8);
    }
    root_delta.vx -= JOINT(joint - 0xF8)->world.t[0];
    root_delta.vy -= JOINT(joint - 0xF8)->world.t[1];
    root_delta.vz -= JOINT(joint - 0xF8)->world.t[2];
    (*(Joint **)&joint)--;
    LIT(s32, sp_tmp18) = 0xC;
    while ((LIT(s32, sp_tmp18) = LIT(s32, sp_tmp18) - 1) != -1) {
        JOINT(joint)->world.t[0] = (JOINT(joint)->world.t[0] + root_delta.vx) >> 9;
        JOINT(joint)->world.t[1] = (JOINT(joint)->world.t[1] + root_delta.vy) >> 9;
        JOINT(joint)->world.t[2] = (JOINT(joint)->world.t[2] + root_delta.vz) >> 9;
        (*(Joint **)&joint)--;
    }
    (*(Joint **)&joint)++;
    g_CurrentEntity->pos_x += root_delta.vx >> 9;
    g_CurrentEntity->pos_z += root_delta.vz >> 9;
    joint = ENTITY_AS(g_CurrentEntity, joints, void *);
    JOINT(joint)->transform.t[1] += root_delta.vy >> 9;
    ENTITY_AS(g_CurrentEntity, timing_control, u8) = ANIM_FRAME(frame)->timing;
    if (++g_CurrentEntity->animation_frame_id > (ANIM_ENTRY(entry)->frame_count - 1)) {
        g_CurrentEntity->animation_frame_id = 0;
        return 1;
    }
    return 0;
}

void yawn_pose_init(void) {
    s16 *frame_data;
    s32 *anim;
    u16 *frame;
    s32 *entry;
    void *joint;
    u8 i;
    u8 unused_u8;
    u8 count;
    s32 unused[6];

    anim = ((void *) g_CurrentEntity->anim_header);
    frame_data = (s16 *) (anim + (s16) (FIELD_AT(anim, AnimEntry, ->frame_offset, s16) / 4));
    LIT(s32, sp_tmp00) = (s16) (FIELD_AT(anim, AnimEntry, [1].frame_offset, s16) / 2);
    anim = ((void *) g_CurrentEntity->anim_base);
    entry = anim + ((u8) g_CurrentEntity->animation_id);
    frame = (u16 *) (anim + (u16) (ANIM_ENTRY(entry)->frame_offset >> 2));
    frame += g_CurrentEntity->animation_frame_id * 2;
    frame_data += *frame * LIT(s32, sp_tmp00);
    joint = ((void *) g_CurrentEntity->joints);
    JOINT(joint)->transform.t[1] = *++frame_data;
    frame_data += 5;
    i = 0;
    count = g_CurrentEntity->joint_count;
    while (i < count) {
        if (i < 3) {
            JOINT(joint)->rotation.vx = *frame_data;
            JOINT(joint)->rotation.vy = *++frame_data;
            JOINT(joint)->rotation.vz = *++frame_data;
            frame_data++;
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
            if (i == 0) {
                (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
                (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, &JOINT(joint)->world);
            }
            if (i & 2) {
                (g_ApiCallPtr = api_GsMulCoord0)(joint - 0xB4, &JOINT(joint)->transform, &JOINT(joint)->world);
            }
        } else {
            LIT(s32, sp_tmp1C) = *++frame_data;
            JOINT(joint)->vel_y = LIT(s32, sp_tmp1C);
            if (i == 3) {
                JOINT(joint)->rotation.vy = LIT(s32, sp_tmp1C) + g_CurrentEntity->angle;
                JOINT(joint)->rotation.vz = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz) + ((u16) JOINT(joint - 0x174)->rotation.vz);
            } else {
                JOINT(joint)->rotation.vy = LIT(s32, sp_tmp1C) + ((u16) JOINT(joint - 0x7C)->rotation.vy);
                JOINT(joint)->rotation.vz = *++frame_data + ((u16) JOINT(joint - 0x7C)->rotation.vz);
            }
            frame_data++;
            if (i == 7) {
                JOINT(joint)->rotation.vz = 0;
            }
            (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->world);
            (g_ApiCallPtr = api_ApplyMatrixLV)(joint - 0x38, joint + 0x38, LITP(void, sp_vec));
            JOINT(joint)->world.t[0] = JOINT(joint - 0x7C)->world.t[0] + LIT(s32, sp_vec_vx);
            JOINT(joint)->world.t[1] = JOINT(joint - 0x7C)->world.t[1] + LIT(s32, sp_vec_vy);
            JOINT(joint)->world.t[2] = JOINT(joint - 0x7C)->world.t[2] + LIT(s32, sp_vec_vz);
        }
        (*(Joint **)&joint)++;
        i++;
    }
}

void yawn_post_move(s16 max_turn) {
    u8 *joint;
    VECTOR old_pos;
    joint = (u8 *)g_CurrentEntity->joints;
    joint += 0x174;
    old_pos = *(VECTOR *)(&g_CurrentEntity->pos_x);
    if ((g_CurrentEntity->action_behavior != 5) && (g_CurrentEntity->action_behavior != 6) && (g_CurrentEntity->action_behavior != 7)) {
        (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
    }
    LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
    if (!(g_CurrentEntity->behavior_flags & 1 & 0xFF)) {
        if (LIT(s32, sp_tmp00) != 0) {
            YAWN(g_CurrentEntity)->wall_frames++;
            YAWN(g_CurrentEntity)->wall_reset_delay = 0x14;
        } else if (YAWN(g_CurrentEntity)->wall_reset_delay != 0) {
            if (!({ (s32) YAWN(g_CurrentEntity)->wall_reset_delay--; })) {
                YAWN(g_CurrentEntity)->wall_frames = 0;
            }
        }
    }
    old_pos.vx -= g_CurrentEntity->pos_x;
    old_pos.vz -= g_CurrentEntity->pos_z;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, joint - 0x150, joint - 0x130);
    (g_ApiCallPtr = api_GsMulCoord0)(joint - 0x130, joint - 0x58, joint - 0x38);
    yawn_chain_follow(joint - 0x7C, joint, max_turn);
    yawn_chain_follow(joint, JOINT_N(joint, 1), max_turn);
    yawn_chain_follow(JOINT_N(joint, 1), JOINT_N(joint, 2), max_turn);
    yawn_chain_follow(JOINT_N(joint, 2), JOINT_N(joint, 3), max_turn);
    yawn_chain_follow(JOINT_N(joint, 3), JOINT_N(joint, 4), max_turn);
    yawn_chain_follow(JOINT_N(joint, 4), JOINT_N(joint, 5), max_turn);
    yawn_chain_follow(JOINT_N(joint, 5), JOINT_N(joint, 6), max_turn);
    yawn_chain_follow(JOINT_N(joint, 6), JOINT_N(joint, 7), max_turn);
    if ((g_CurrentEntity->move_speed_current == 0) && (YAWN(g_CurrentEntity)->death_scale == 0)) {
        return;
    }
    yawn_chain_follow(JOINT_N(joint, 7), JOINT_N(joint, 8), max_turn);
    yawn_chain_follow(JOINT_N(joint, 8), JOINT_N(joint, 9), max_turn);
    yawn_chain_follow(JOINT_N(joint, 9), JOINT_N(joint, 10), (s16) (max_turn / 2));
    yawn_chain_follow(JOINT_N(joint, 10), JOINT_N(joint, 11), (s16) (max_turn / 2));
}

void yawn_spawn_dust(s32 offset_index) {
    extern struct { s16 a; s16 b; } yawn_dust_offset[];
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    LIT(s32, sp_matrix_t2) = yawn_dust_offset[offset_index].b - (yawn_dust_offset[offset_index].b * ((g_ApiCallPtr = api_rand)() & 2));
    LIT(s32, sp_matrix_t1) = 0x32;
    LIT(s32, sp_matrix_t0) = (s32) yawn_dust_offset[offset_index].a;
    (g_ApiCallPtr = api_GsMulCoord2)(g_CurrentEntity->joints + 0xC0, ADDR_sp_matrix);
    LIT(s32, sp_vec_vx) = LIT(s32, sp_matrix_t0);
    LIT(s32, sp_vec_vy) = LIT(s32, sp_matrix_t1);
    LIT(s32, sp_vec_vz) = LIT(s32, sp_matrix_t2);
    (g_ApiCallPtr = api_effect_create_billboard)(0, 4, 0, g_DeadMoveValue, ADDR_sp_vec, 0x14);
}
