/* tyrant.c - Tyrant: attacks (claw swipe / slash / thrust, impale, charge), the lab and roof fights, script (SCD)
   behaviors, heart / claw trail effects, and the player's hit and limb animations.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/tyrant.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* tyrant data (defined in each overlay that has this enemy) */
extern s32 tyrant_sca_info_ptr;
extern void *g_TyrantClawGhostBlock;
extern u16 g_TyrantClawScaleA;
extern s16 g_TyrantClawScaleB;
extern s8 g_TyrantClawScaleStep;
extern void *g_TyrantTrailBlock;
extern u16 g_TyrantTrailTimer;
extern s8 g_TyrantTrailSegments;
extern s32 g_TyrantTrailNear;
extern s32 g_TyrantTrailFar;
extern s8 tyrant_uv_scroll_u_tbl[];
extern s16 tyrant_uv_scroll_v_tbl[];
extern s32 tyrant_heartbeat_tbl;
extern u16 tyrant_uv_scroll_poly_tbl[];
extern u16 g_TyrantClutRectX;
extern u16 g_TyrantClutRectY;
extern s32 g_TyrantScdVictim;

/* TyrantWork: the tyrant's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). TYRANT(e)->f, TYRANT_FIELD(e, f) and TYRANT_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct TyrantWork {
    /* +0x00 */ u8 room_hit;           /* 0x16C: OR of api_check_room_collision results (tyrant_update); address given to the wander turn */
    /* +0x01 */ u8 uv_scroll_step;     /* 0x16D: index into tyrant_uv_scroll_u/v_tbl for tyrant_scroll_body_uvs, wraps at 30 (tyrant_update) */
    /* +0x02 */ u8 hit_mask;           /* 0x16E: attack that already connected this swing: 1 swipe, 2 slash/rush, 4 thrust, 8 backhand/charge; 0x80 = impale/kill trigger (low health, close too long) */
    /* +0x03 */ u8 look_at_timer;      /* 0x16F: look-at countdown while flags bit 1 (3 at init, 8 on a hit); 0 = look at the claw joint, 1..3 = look off, >=4 = fixed look point */
    /* +0x04 */ void *heart;           /* 0x170: cloned heart entity (api_entity_clone in tyrant_init); tyrant_draw_heart switches g_CurrentEntity to it, rocket death moves it */
    /* +0x08 */ s32 saved_state;       /* 0x174: copy of the state word each frame (tyrant_update); restored by tyrant_state_hit */
    /* +0x0C */ u8 unused_178;         /* 0x178: only set to 3 in tyrant_init */
    /* +0x0D */ u8 flags;              /* 0x179: 1 draw ground shadow, 2 look-at live, 4 swing hit-sound played, 8 hidden/dead (skip heart, claw ghosts, health floor) */
    /* +0x0E */ u8 pad0E[2];
    /* +0x10 */ u16 dir_control_flags; /* 0x17C: copy of the room-collision direction word (0x1F800018) each frame; first arg of api_entity_update_wander_turn */
    /* +0x12 */ u8 wander_turn_state;  /* 0x17E: cleared in init; passed by address to api_entity_update_wander_turn */
    /* +0x13 */ u8 attack_cooldown;    /* 0x17F: frames before the think may pick an attack (90 at init; 10/13/15 after each attack), counted down in think_lab/think_roof */
    /* +0x14 */ u8 rocket_flag;        /* 0x180: bit 7 set in tyrant_think_roof when g_PlayerEntity.behavior_flags == 0xA (the PC decomp reads that byte as the equipped weapon, 10 = rocket launcher); never read here */
    /* +0x15 */ u8 special_timer;      /* 0x181: 210, counted down in think_roof, reset on hits/attacks; at 0 the next move becomes behavior 0xA/0xB (special attack) */
    /* +0x16 */ u8 close_frames;       /* 0x182: frames the player stays within 0xFA0 (tyrant_think); >=121 starts frenzy_timer and sets hit_mask 0x80; +20 on swipe */
    /* +0x17 */ u8 frenzy_timer;       /* 0x183: 60 when close_frames runs out; while non-zero the walk turns twice per frame and the close-range attack check is allowed */
} TyrantWork;
#define TYRANT(e) ENEMY_WORK(e, TyrantWork)
#define TYRANT_FIELD(e, f) ENEMY_FIELD(e, TyrantWork, f)
#define TYRANT_MEMBER(e, f) ENEMY_MEMBER(e, TyrantWork, f)

/* state 0: sets up a new tyrant (health, sca info, animation, work fields), then state 1 */
void tyrant_init(void) {
    void *heart;
    s32 claw;
    s32 unused[4];

    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->hit_state = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(LITP(u16, sp_svec), &g_CurrentEntity->shadow, 0x3E8U, (u8 *)0x3E8);
    g_CurrentEntity->health = 0xDC;
    if (g_CurrentEntity->id == 0x10) {
        g_CurrentEntity->health = 0x258;
    }
    g_CurrentEntity->sca_info = tyrant_sca_info_ptr;
    (g_ApiCallPtr = api_entity_clone)((u16 *)1, (u16 *)0x1388, g_CurrentEntity->joint_count, &g_CurrentEntity->sub16C[4]);
    heart = TYRANT(g_CurrentEntity)->heart;
    ENT(heart)->rot_x = 0;
    ENT(heart)->angle = 0;
    ENT(heart)->angle_z = 0;
    ENT(heart)->pos_x = 0x138;
    ENT(heart)->pos_y = -0x365;
    ENT(heart)->pos_z = -0x11F;
    ENT(heart)->position_vx = 0x1000;
    ENT(heart)->action_ticks_counter = 0x15;
    TYRANT(g_CurrentEntity)->uv_scroll_step = 0;
    TYRANT(g_CurrentEntity)->hit_mask = 0;
    TYRANT(g_CurrentEntity)->look_at_timer = 3;
    TYRANT(g_CurrentEntity)->unused_178 = 3;
    TYRANT(g_CurrentEntity)->flags = 0;
    TYRANT(g_CurrentEntity)->attack_cooldown = 0;
    TYRANT(g_CurrentEntity)->rocket_flag = 0;
    TYRANT(g_CurrentEntity)->special_timer = 0xD2;
    TYRANT(g_CurrentEntity)->room_hit = 0;
    TYRANT(g_CurrentEntity)->wander_turn_state = 0;
    TYRANT(g_CurrentEntity)->close_frames = 0;
    TYRANT(g_CurrentEntity)->frenzy_timer = 0;
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0xB, 0);
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    g_CurrentEntity->animation_id = 6;
    if (g_CurrentEntity->id == 0x10) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 9, 0);
    }
    (g_ApiCallPtr = api_joint_move)(NULL, (u16 *) g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (u8 *)0x40);
    g_CurrentEntity->look_at_joint_idx = 2;
    g_CurrentEntity->look_at_flags = 2;
    ENTITY_AS(g_CurrentEntity, look_at_yaw_step, u8) = 0xA0;
    g_CurrentEntity->look_at_pitch_step = 0x60;
    if (g_CurrentEntity->id == 0xC) {
        return;
    }
    g_TyrantClawGhostBlock = g_LoadPointer;
    claw = g_CurrentEntity->joints;
    claw += 0x3E0;
    g_LoadPointer += 0xF8;
    LIT(s32, sp_tmp00) = 2;
    while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->flags = JOINT(claw)->flags;
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->world = JOINT(claw)->world;
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->anim_slot = JOINT(claw)->anim_slot;
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->anim_field = JOINT(claw)->anim_field;
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->dobj_id = JOINT(claw)->dobj_id;
        JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->scale_flag = JOINT(claw)->scale_flag;
    }
    LIT(s32, sp_tmp08) = 2;
    while ((LIT(s32, sp_tmp08) = LIT(s32, sp_tmp08) - 1) != -1) {
        JOINT(((LIT(s32, sp_tmp08) * 0x7C) + g_TyrantClawGhostBlock))->anim_object = (u16 *) g_LoadPointer;
        g_LoadPointer = (void *)(g_ApiCallPtr = api_GsPresetObject)((LIT(s32, sp_tmp08) * 0x7C) + g_TyrantClawGhostBlock + 0xC, g_LoadPointer);
    }
    (g_ApiCallPtr = api_joint_apply_color_tint)(g_TyrantClawGhostBlock, (u16 *)0xFF, 0xFFU, (u8 *)0xFF);
    (g_ApiCallPtr = api_joint_apply_color_tint)(g_TyrantClawGhostBlock + 0x7C, (u16 *)0x30000, 0x300000U, (u8 *)0x300000);
    claw = g_CurrentEntity->joints;
    claw += 0x3E0;
    g_TyrantTrailBlock = g_LoadPointer;
    (g_ApiCallPtr = api_tyrant_trail_alloc)((u16 *)9, g_TyrantTrailBlock, 0x70U);
    TYRANT(g_CurrentEntity)->attack_cooldown = 0x5A;
}

void tyrant_trail_update(void) {
    s32 claw;

    claw = g_CurrentEntity->joints;
    claw += 0x3E0;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, claw - 0x3BC, ADDR_sp_matrix);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), claw - 0x340);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), claw - 0xD4);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), claw - 0x58);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), claw + 0x24);
    (g_ApiCallPtr = api_tyrant_trail_push)(g_TyrantTrailBlock, claw + 0x44, ADDR_sp_matrix, &g_TyrantTrailNear, &g_TyrantTrailFar, 0, (s32) g_TyrantTrailSegments);
    if (g_UpdateFlags & 4 & 0xFFFF) {
        if ((s32) g_TyrantTrailTimer < g_TyrantTrailSegments) {
            g_TyrantTrailSegments--;
        }
        g_TyrantTrailTimer--;
    }
}

void tyrant_draw_claw_ghosts(void) {
    extern u16 g_TyrantClawScaleB_u16 __asm__("g_TyrantClawScaleB");
    void *claw;
    MATRIX mtx;

    if (g_CurrentEntity->id == 0xC) {
        return;
    }
    claw = ((void *) g_CurrentEntity->joints);
    claw += 0x3E0;
    LIT(void *, sp_tmp38) = ((void *) JOINT(claw)->anim_slot);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, claw - 0x3BC, LITP(s32, sp_matrix));
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(s32, sp_matrix), claw - 0x340);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(s32, sp_matrix), claw - 0xD4);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(s32, sp_matrix), claw - 0x58);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(s32, sp_matrix), &JOINT(claw)->transform);
    if (g_UpdateFlags & 4 & 0xFFFF) {
        JOINT((g_TyrantClawGhostBlock + 0x7C))->world = LIT(MATRIX, sp_matrix);
        g_TyrantClawScaleA += g_TyrantClawScaleStep;
        g_TyrantClawScaleB_u16 += g_TyrantClawScaleStep;
        LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vz) = (s16) g_TyrantClawScaleA;
        LIT(s32, sp_vec_vy) = (s32) (s16) g_TyrantClawScaleB_u16;
        (g_ApiCallPtr = api_ScaleMatrix)(LITP(s32, sp_matrix), LITP(void, sp_vec));
        if (((s16) g_TyrantClawScaleA >= 0x1771) || ((s16) g_TyrantClawScaleA < 0xBB8)) {
            g_TyrantClawScaleStep *= -1;
        }
        JOINT(g_TyrantClawGhostBlock)->world = LIT(MATRIX, sp_matrix);
    }
    if (g_CurrentEntity->has_enter_switch_zone == 0) {
        return;
    }
    LIT(s32, sp_tmp00) = 2;
    while ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) != -1) {
        (g_ApiCallPtr = api_GsMulCoord0)(g_pWorldScreenMatrix, (LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock + 0x44, &mtx);
        (g_ApiCallPtr = api_GsSetLsMatrix)(&mtx);
        (g_ApiCallPtr = api_GsPrstTG3NL)(FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->primitive_top, s32 *), ((void *) TMD_OBJ(LIT(s32, sp_tmp38))->vert_top), FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->normal_top, s32 *), ((s32) JOINT(((LIT(s32, sp_tmp00) * 0x7C) + g_TyrantClawGhostBlock))->anim_object) + (g_ActiveBuffer * 0x28) + 8, FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->n_primitive, s32), 4, (g_ActiveBuffer * 0x14) + ADDR_g_OtMain);
    }
}

/* every frame for each tyrant (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void tyrant_update(void) {
    void tyrant_draw_heart();
    void tyrant_scroll_body_uvs(s16 du, s16 dv);
    extern s32 (*tyrant_state_tbl[])();
    void *joint;

    if ((g_CurrentEntity->state != 0) && (g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        g_CurrentEntity->state = 8U;
    }
    if ((g_CurrentEntity->state != 0) && (g_CurrentEntity->behavior_flags & 0x80 & 0xFF)) {
        return;
    }
    if (g_UpdateFlags & 4 & 0xFFFF) {
        tyrant_state_tbl[g_CurrentEntity->state]();
        if (g_CurrentEntity->action_behavior != 9) {
            (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
            (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            TYRANT_FIELD(g_CurrentEntity, room_hit) = (u8) (TYRANT_FIELD(g_CurrentEntity, room_hit) | (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius));
            TYRANT(g_CurrentEntity)->dir_control_flags = (u16) LIT(u16, sp_tmp18);
        }
        tyrant_scroll_body_uvs(tyrant_uv_scroll_u_tbl[TYRANT(g_CurrentEntity)->uv_scroll_step], tyrant_uv_scroll_v_tbl[TYRANT(g_CurrentEntity)->uv_scroll_step]);
        if (++TYRANT(g_CurrentEntity)->uv_scroll_step >= 0x1E) {
            TYRANT(g_CurrentEntity)->uv_scroll_step = 0;
        }
        if (TYRANT(g_CurrentEntity)->flags & 2 & 0xFF) {
            if (TYRANT(g_CurrentEntity)->look_at_timer == 0) {
                g_CurrentEntity->look_at_flags = 0x13;
                joint = ((void *) g_PlayerEntity.joints);
                (*(Joint **)&joint)++;
                g_CurrentEntity->scd_pos_x = (s32) JOINT(joint)->world.t[0];
                g_CurrentEntity->scd_pos_y = (s32) JOINT(joint)->world.t[1];
                g_CurrentEntity->scd_pos_z = (s32) JOINT(joint)->world.t[2];
            } else if ((u8) TYRANT(g_CurrentEntity)->look_at_timer < 4U) {
                TYRANT(g_CurrentEntity)->look_at_timer--;
                g_CurrentEntity->look_at_flags = 0;
                g_CurrentEntity->hit_state = 0;
            } else {
                TYRANT(g_CurrentEntity)->look_at_timer--;
                g_CurrentEntity->look_at_flags = 0x33;
                g_CurrentEntity->scd_pos_x = 0x190;
                g_CurrentEntity->scd_pos_y = 0xE0C;
            }
        }
        if (!(g_CurrentEntity->health & 0x8000)) {
            (g_ApiCallPtr = api_entity_update_look_at_angles)();
        }
        TYRANT(g_CurrentEntity)->saved_state = ENTITY_STATE_WORD(g_CurrentEntity);
    }
    if (!(TYRANT(g_CurrentEntity)->flags & 8 & 0xFF)) {
        tyrant_draw_heart();
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if ((g_CurrentEntity->id != 0xC) && !(TYRANT(g_CurrentEntity)->flags & 8 & 0xFF)) {
        tyrant_draw_claw_ghosts();
        if (g_TyrantTrailTimer & 0x8000 & 0xFFFF) {
            joint = ((void *) g_CurrentEntity->joints);
            joint += 0x3E0;
            g_TyrantTrailSegments = 8;
            LIT(s32, sp_tmp14) = 7;
            if (g_TyrantClawScaleB >= 0x1F41) {
                SVEC(&g_TyrantTrailFar)->vy = (u16) (((u16) SVEC(&g_TyrantTrailFar)->vy) + 0x3E8);
            }
            SVEC(&g_TyrantTrailFar)->vy = (u16) (SVEC(&g_TyrantTrailFar)->vy - 0x320);
            for (;;) {
                SVEC(&g_TyrantTrailFar)->vy = (u16) (((u16) SVEC(&g_TyrantTrailFar)->vy) + 0x64);
                (g_ApiCallPtr = api_tyrant_trail_push)(g_TyrantTrailBlock, &JOINT(joint)->world, (&JOINT(joint)->world), &g_TyrantTrailNear, &g_TyrantTrailFar, LIT(s32, sp_tmp14), 0);
                if ((LIT(s32, sp_tmp14) = LIT(s32, sp_tmp14) - 1) == -1) {
                    break;
                }
            }
            (g_ApiCallPtr = api_tyrant_trail_push)(g_TyrantTrailBlock, &JOINT(joint)->world, (&JOINT(joint)->world), &g_TyrantTrailNear, &g_TyrantTrailNear, 8, 0);
            g_TyrantTrailTimer &= 0x7FFF;
            if (g_TyrantClawScaleB >= 0x1F41) {
                SVEC(&g_TyrantTrailFar)->vy = (u16) (SVEC(&g_TyrantTrailFar)->vy - 0x3E8);
            }
        }
        if (g_TyrantTrailTimer != 0) {
            tyrant_trail_update();
        }
    }
    if (TYRANT(g_CurrentEntity)->flags & 1 & 0xFF) {
        joint = ((void *) g_CurrentEntity->joints);
        if (g_CurrentEntity->has_enter_switch_zone != 0) {
            (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, (s16) g_CurrentEntity->angle);
        }
    }
    if (!(TYRANT(g_CurrentEntity)->flags & 8 & 0xFF) && (g_CurrentEntity->id == 0x10) && (g_CurrentEntity->health < 0xC9)) {
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_SysFlags), (void *)0x1F);
        g_CurrentEntity->health = 0xC8;
    }
    (g_ApiCallPtr = api_check_room_actions)(g_CurrentEntity, (void *)2);
}

void tyrant_state1(void) {
    void tyrant_act();
    void tyrant_think();

    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    (g_ApiCallPtr = api_entity_check_alert_range)(0xFA0);
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        tyrant_think();
        /* fallthrough */
    case 1:
        tyrant_act();
    }
}

void tyrant_state_hit(void) {
    s32 unused;

    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = (s32) TYRANT(g_CurrentEntity)->saved_state;
        if (TYRANT(g_CurrentEntity)->look_at_timer == 0) {
            TYRANT(g_CurrentEntity)->look_at_timer = 8;
        }
        if (TYRANT(g_CurrentEntity)->special_timer != 0) {
            TYRANT(g_CurrentEntity)->special_timer = 0xD2;
        }
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        LIT(s32, sp_vec_vy) = -0x834;
        LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0, LIT(s32, sp_tmp04), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
}

void tyrant_state_forced(void) {
    extern s32 (*tyrant_behavior_tbl[])();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        if (g_CurrentEntity->id == 0x10) {
            g_CurrentEntity->action_behavior = 1U;
        }
    }
    tyrant_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void tyrant_state4_nop(void) {

}

void tyrant_act(void) {
    extern s32 (*tyrant_behavior_tbl_state1[])();
    s32 unused;
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    (g_ApiCallPtr = api_zone_path_find)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, &g_CurrentEntity->waypoint_x, &g_CurrentEntity->waypoint_z);
    tyrant_behavior_tbl_state1[g_CurrentEntity->action_behavior]();
}

void tyrant_think(void) {
    extern s32 (*tyrant_behavior_tbl_think[])();

    if (g_PlayerEntity.health & 0x8000) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0, 0);
        return;
    }
    if (g_CurrentEntity->health < 0x3C) {
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
    }
    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    if (LIT(s32, sp_tmp00) < 0xFA0) {
        if ((u32) (++TYRANT(g_CurrentEntity)->close_frames & 0xFF) >= 0x79U) {
            TYRANT(g_CurrentEntity)->frenzy_timer = 0x3C;
            TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
        }
    } else {
        TYRANT(g_CurrentEntity)->close_frames = 0;
    }
    tyrant_behavior_tbl_think[(((u8) g_CurrentEntity->id >> 2) & 0xFF)]();
}

void tyrant_think_lab(void) {

    if (TYRANT(g_CurrentEntity)->attack_cooldown != 0) {
        TYRANT(g_CurrentEntity)->attack_cooldown--;
    }
    TYRANT(g_CurrentEntity)->special_timer = 0;
    if (TYRANT(g_CurrentEntity)->attack_cooldown == 0) {
        if (LIT(s32, sp_tmp00) < 0xA8C) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x340) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
            }
        }
        if ((TYRANT(g_CurrentEntity)->frenzy_timer != 0) && (LIT(s32, sp_tmp00) < 0xA8C)) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x340) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
                return;
            }
        }
        if (LIT(s32, sp_tmp00) < 0x7D0) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
                if (g_PlayerEntity.health < 0x10) {
                    g_CurrentEntity->action_behavior += ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) ? 4 : 0;
                }
            }
        }
    }
}

void tyrant_think_roof(void) {

    if (TYRANT(g_CurrentEntity)->attack_cooldown != 0) {
        TYRANT(g_CurrentEntity)->attack_cooldown--;
    }
    if (g_PlayerEntity.behavior_flags == 0xA) {
        TYRANT(g_CurrentEntity)->rocket_flag = (s8) (TYRANT(g_CurrentEntity)->rocket_flag | 0x80);
    }
    if (TYRANT(g_CurrentEntity)->special_timer != 0) {
        TYRANT(g_CurrentEntity)->special_timer--;
    }
    if (TYRANT(g_CurrentEntity)->attack_cooldown == 0) {
        if (LIT(s32, sp_tmp00) >= 0x2EE1) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 8, 0);
            }
        }
        if ((TYRANT(g_CurrentEntity)->frenzy_timer != 0) && (LIT(s32, sp_tmp00) < 0xA8C)) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x340) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
                return;
            }
        }
        if (LIT(s32, sp_tmp00) < 0xA8C) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x340) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 4, 0);
            }
        }
        if (LIT(s32, sp_tmp00) < 0x7D0) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 3, 0);
                if (g_PlayerEntity.health < 0x10) {
                    g_CurrentEntity->action_behavior += ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) ? 4 : 0;
                }
            }
        }
        if (LIT(s32, sp_tmp00) < 0x1B58) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x4C8) != 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
            }
        }
        if (TYRANT(g_CurrentEntity)->special_timer == 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xB, 0);
        }
    }
    if (((g_ApiCallPtr = api_Flg_ck)(ADDR_g_SysFlags, 0x1E) != 0) && (LIT(s32, sp_tmp00) < 0xA8C)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 7, 0);
        }
    }
}

void tyrant_behavior_pause(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        g_CurrentEntity->blend_counter = 0x1F;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        }
        return;
    }
}

void tyrant_behavior_walk(void) {
    s32 tyrant_root_motion_a1() __asm__("tyrant_root_motion");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 1;
        /* fallthrough */
    case 1:
        LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
        (g_ApiCallPtr = api_entity_update_wander_turn)(TYRANT(g_CurrentEntity)->dir_control_flags, (u8 *)g_CurrentEntity + 0x16C, &TYRANT(g_CurrentEntity)->wander_turn_state, 0x28, 0x28);
        if (TYRANT(g_CurrentEntity)->frenzy_timer != 0) {
            TYRANT(g_CurrentEntity)->frenzy_timer--;
            (g_ApiCallPtr = api_entity_update_wander_turn)(TYRANT(g_CurrentEntity)->dir_control_flags, (u8 *)g_CurrentEntity + 0x16C, &TYRANT(g_CurrentEntity)->wander_turn_state, 0x28, 0x28);
        }
        (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), ((void *) g_CurrentEntity->anim_base), 0x200);
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 6U) && ((u8) g_CurrentEntity->animation_frame_id < 0x1FU)) {
        if (g_CurrentEntity->animation_frame_id == 6) {
            (g_ApiCallPtr = api_sound_play_entity_se)(0U, NULL);
        }
        tyrant_root_motion_a1(0, 0);
        g_CurrentEntity->move_speed_current = (u16) (((u16) g_CurrentEntity->move_speed_current) + 0x19);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
        return;
    }
    if (g_CurrentEntity->animation_frame_id == 0x1F) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, NULL);
    }
    tyrant_root_motion_a1(1, 0);
    g_CurrentEntity->move_speed_current = (u16) (((u16) g_CurrentEntity->move_speed_current) + 0x19);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
}

void tyrant_behavior_nop(void) {

}

void tyrant_behavior_claw_swipe(void) {
    s32 claw;
    u16 hit_frame;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFB);
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->move_speed_current = 0xC8;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xA;
        TYRANT(g_CurrentEntity)->close_frames = (s8) (TYRANT(g_CurrentEntity)->close_frames + 0x14);
        g_TyrantTrailTimer = 0x801F;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        if (g_CurrentEntity->move_speed_current != 0) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x10);
            if (TYRANT(g_CurrentEntity)->frenzy_timer != 0) {
                (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x10);
            }
        }
        hit_frame = g_CurrentEntity->animation_frame_id - 5;
        if (hit_frame < 4U) {
            claw = g_CurrentEntity->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x1F4;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(claw + 0x44, ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(claw, 0xFF, 0x80880, 0x808080);
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vy) = 0x320;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 3, 0, claw + 0x44, ADDR_sp_vec, 0);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 1, 0);
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 1);
                if (!(TYRANT(g_CurrentEntity)->flags & 4 & 0xFF)) {
                    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
                    TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 4);
                }
            }
        }
        if ((hit_frame == 3) && (TYRANT(g_CurrentEntity)->hit_mask & 1 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x12;
        }
        if (g_CurrentEntity->animation_frame_id == 0x32) {
            g_CurrentEntity->action_state = 2U;
        }
        break;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0x74);
    }
    g_CurrentEntity->move_speed_current -= g_CurrentEntity->animation_frame_id * 5;
    if (g_CurrentEntity->move_speed_current < 0) {
        g_CurrentEntity->move_speed_current = 0;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void tyrant_behavior_claw_slash(void) {
    u16 hit_frame;
    s32 claw;

    switch (g_CurrentEntity->action_state) {
    case 0:
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFB);
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 4;
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xF;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 8);
        if (TYRANT(g_CurrentEntity)->frenzy_timer != 0) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x28);
        }
        if (g_CurrentEntity->animation_frame_id == 7) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
            g_TyrantTrailTimer = 0x8008;
        }
        hit_frame = g_CurrentEntity->animation_frame_id + 0xFFF9;
        if (hit_frame < 5U) {
            claw = g_CurrentEntity->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(claw + 0x44, ADDR_sp_vec, 0x5DC, ADDR_g_PlayerPosX) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(claw, 0xFF, 0x80880, 0x808080);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 1, 0);
                if ((g_CurrentEntity->id == 0x10) && ((g_PlayerEntity.health - 0x12) > 0)) {
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 2, 0);
                    LIT(s32, sp_tmp0C) = g_PlayerEntity.pos_x - g_CurrentEntity->pos_x;
                    LIT(s32, sp_tmp10) = g_PlayerEntity.pos_z - g_CurrentEntity->pos_z;
                    if (LIT(s32, sp_tmp0C) != 0) {
                        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_catan)((s32) (LIT(s32, sp_tmp10) * 0x1000) / (s32) LIT(s32, sp_tmp0C));
                        g_PlayerEntity.angle = (((LIT(s32, sp_tmp0C) < 0) ? 0x400 : 0xC00) - LIT(s32, sp_tmp14)) & 0xFFF;
                    } else {
                        g_PlayerEntity.angle = ((LIT(s32, sp_tmp10) > 0) ? 0x800 : 0);
                    }
                    TYRANT(g_CurrentEntity)->special_timer = 0xD2;
                }
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                ENTITY_AS(g_CurrentEntity, angle, s16) -= 0x400;
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 2);
                if (!(TYRANT(g_CurrentEntity)->flags & 4 & 0xFF)) {
                    TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 4);
                }
            }
        }
        if ((hit_frame == 5) && (TYRANT(g_CurrentEntity)->hit_mask & 2 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x12;
            if (g_PlayerEntity.health & 0x8000) {
                g_PlayerEntity.health = 1;
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
                (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
            } else {
                (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
            }
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        if (((TYRANT(g_CurrentEntity)->hit_mask & 0x82 & 0xFF) == 0x82) && (g_CurrentEntity->animation_frame_id == 0x11)) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
            (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x20);
        }
        return;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        LIT(s32, sp_tmp00) = (__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x))) + (__builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
        if ((g_CurrentEntity->id == 0x10) && (LIT(s32, sp_tmp00) >= 0x1389) && (TYRANT(g_CurrentEntity)->hit_mask & 2 & 0xFF)) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
            }
        }
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0x75);
    }
}

void tyrant_behavior_claw_thrust(void) {
    void tyrant_root_motion(u8 foot, u8 apply);
    u16 hit_frame;
    s32 claw;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 7;
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xF;
        /* fallthrough */
    case 1:
        if (g_CurrentEntity->animation_frame_id == 0xA) {
            g_TyrantTrailTimer = 0x8008;
        }
        if (g_CurrentEntity->animation_frame_id == 0xC) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        hit_frame = g_CurrentEntity->animation_frame_id + 0xFFF8;
        if (hit_frame < 8U) {
            claw = g_CurrentEntity->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(claw + 0x44, ADDR_sp_vec, 0x5DC, ADDR_g_PlayerPosX) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(claw, 0xFF, 0x80880, 0x808080);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 1, 0);
                if ((g_PlayerEntity.health - 0x14) > 0) {
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 2, 0);
                    LIT(s32, sp_tmp0C) = g_PlayerEntity.pos_x - g_CurrentEntity->pos_x;
                    LIT(s32, sp_tmp10) = g_PlayerEntity.pos_z - g_CurrentEntity->pos_z;
                    if (LIT(s32, sp_tmp0C) != 0) {
                        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_catan)((s32) (LIT(s32, sp_tmp10) * 0x1000) / (s32) LIT(s32, sp_tmp0C));
                        g_PlayerEntity.angle = (((LIT(s32, sp_tmp0C) < 0) ? 0xC00 : 0x1400) - LIT(s32, sp_tmp14)) & 0xFFF;
                    } else {
                        g_PlayerEntity.angle = ((LIT(s32, sp_tmp10) > 0) ? 0x1000 : 0x800);
                    }
                }
                TYRANT(g_CurrentEntity)->special_timer = 0xD2;
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 4);
            }
        }
        if ((hit_frame == 7) && (TYRANT(g_CurrentEntity)->hit_mask & 4 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x14;
            if (g_PlayerEntity.health < 0) {
                (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
            } else {
                (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->action_state = 2U;
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
        }
        break;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        LIT(s32, sp_tmp00) = (__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x))) + (__builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
        if ((g_CurrentEntity->id == 0x10) && (LIT(s32, sp_tmp00) >= 0x1389) && (TYRANT(g_CurrentEntity)->hit_mask & 4 & 0xFF)) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
            }
        }
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0xFB);
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0xEU) {
        tyrant_root_motion(0, 1);
        return;
    }
    tyrant_root_motion(1, 1);
}

void tyrant_behavior_backhand(void) {
    extern s16 g_TyrantTrailTimer_s16 __asm__("g_TyrantTrailTimer");
    u16 hit_frame;
    s32 claw;

    switch (g_CurrentEntity->action_state) {
    case 0:
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFB);
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->move_speed_current = 0x12CU;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x30);
        g_TyrantTrailTimer_s16 = 0xF;
        g_TyrantTrailSegments = 8;
        /* fallthrough */
    case 1:
        if (g_CurrentEntity->animation_frame_id == 4) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        }
        hit_frame = g_CurrentEntity->animation_frame_id + 0xFFFC;
        if (hit_frame < 4U) {
            claw = g_CurrentEntity->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x1F4;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(claw + 0x44, ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(claw, 0xFF, 0x80880, 0x808080);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 0, 0);
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 8);
                if (!(TYRANT(g_CurrentEntity)->flags & 4 & 0xFF)) {
                    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
                    TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 4);
                }
            }
        }
        if ((hit_frame == 3) && (TYRANT(g_CurrentEntity)->hit_mask & 8 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x12;
            if ((g_PlayerEntity.health & 0x8000) && ((TYRANT(g_CurrentEntity)->hit_mask & 0x82 & 0xFF) == 0x82)) {
                g_PlayerEntity.health = 1;
            }
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        g_CurrentEntity->move_speed_current = (u16) (((u16) g_CurrentEntity->move_speed_current) - ((g_CurrentEntity->animation_frame_id) * 3));
        if ((s16) ((u16) g_CurrentEntity->move_speed_current) < 0) {
            g_CurrentEntity->move_speed_current = 0U;
        }
        if (((TYRANT(g_CurrentEntity)->hit_mask & 0x8A & 0xFF) == 0x8A) && (g_CurrentEntity->animation_frame_id == 0xC) && (((u8) g_PlayerEntity.hit_state) != 0)) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, 0x20);
            ENTITY_ACTION_S16(g_CurrentEntity) = 3;
            if (g_PlayerEntity.health < 0x10) {
                g_CurrentEntity->action_behavior += ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) ? 4 : 0;
            }
        }
        break;
    case 2:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0x75);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void tyrant_behavior_impale(void) {
    s32 claw;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->move_speed_current = 0xC8;
        g_CurrentEntity->hit_state = 1;
        (g_ApiCallPtr = api_snap_player_to_grab_position)(LITP(void, g_PlayerEntity));
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(void, g_PlayerPosX), 0x400);
        g_PlayerEntity.angle = g_CurrentEntity->angle;
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 0xC, 0, 0);
        g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)1, 0);
        g_TyrantTrailTimer = 0x802F;
        /* fallthrough */
    case 1:
        if ((g_CurrentEntity->animation_frame_id != 5) && (g_CurrentEntity->animation_frame_id != 0x5C)) {

        } else {
            claw = g_CurrentEntity->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x320;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, claw + 0x44, ADDR_sp_vec, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 5) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)4, 0);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 0x61U) {
            if (!(g_CurrentEntity->animation_frame_id % 7 & 0xFF)) {
                claw = g_CurrentEntity->joints;
                claw += 0x3E0;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vy) = 0x320;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, claw + 0x44, ADDR_sp_vec, 0);
            }
        }
        if (g_CurrentEntity->animation_frame_id == 0x5F) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)7, 0);
        }
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        if (g_CurrentEntity->move_speed_current != 0) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(void, g_PlayerPosX), 0x10);
        }
        break;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0xF8);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
    }
    g_CurrentEntity->move_speed_current = (s16) ((u16) g_CurrentEntity->move_speed_current - ((g_CurrentEntity->animation_frame_id) * 5));
    if (g_CurrentEntity->move_speed_current < 0) {
        g_CurrentEntity->move_speed_current = 0;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
    g_CurrentEntity->anim_base_x = (u16) (g_CurrentEntity->anim_base_x + g_CurrentEntity->speed_vx);
    g_CurrentEntity->anim_base_z = (u16) (g_CurrentEntity->anim_base_z + g_CurrentEntity->speed_vz);
    g_PlayerEntity.anim_base_x += g_CurrentEntity->speed_vx;
    g_PlayerEntity.anim_base_z += g_CurrentEntity->speed_vz;
}

void tyrant_behavior_charge(void) {
    u16 hit_frame;
    void *joint_base;
    VECTOR saved_pos;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = (u8) ((u8) g_CurrentEntity->animation_frame_id >> 1);
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 2;
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x190U;
        if ((g_ApiCallPtr = api_Flg_ck)(LITP(void, g_SysFlags), 0x1E) != 0) {
            ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x12CU;
        }
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xF;
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_Flg_ck)(LITP(void, g_SysFlags), 0x1E) != 0) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(void, g_PlayerPosX), 0x30);
        } else {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(void, g_PlayerPosX), 0x20);
        }
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200);
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
        if (((LIT(s32, sp_tmp00) < 0x1388) && ((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x80) == 0)) || ((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x384) != 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        saved_pos = *(VECTOR *)(&g_CurrentEntity->pos_x);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(ENTITY_AS(g_CurrentEntity, sca_info, void *))->radius);
        g_CurrentEntity->pos_x = saved_pos.vx;
        g_CurrentEntity->pos_y = saved_pos.vy;
        g_CurrentEntity->pos_z = saved_pos.vz;
        if (LIT(s32, sp_tmp00) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        if ((g_CurrentEntity->animation_frame_id == 3) || (g_CurrentEntity->animation_frame_id == 0xF)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(NULL, 0);
        }
        break;
    case 2:
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 9;
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x12CU;
        /* fallthrough */
    case 3:
        if (g_CurrentEntity->animation_frame_id == 8) {
            g_TyrantTrailTimer = 0x800F;
        }
        hit_frame = g_CurrentEntity->animation_frame_id - 0xF;
        if (hit_frame < 5U) {
            joint_base = ENTITY_AS(g_CurrentEntity, joints, void *);
            joint_base += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x1F4;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joint_base, 0), ADDR_sp_vec, 0x708, LITP(void, g_PlayerPosX)) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(joint_base, 0xFF, 0x80880, (void *)0x808080);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 0, 0);
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                ENTITY_AS(g_CurrentEntity, angle, s16) -= 0x400;
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(LITP(void, g_PlayerEntity));
                g_CurrentEntity->angle = g_CurrentEntity->angle + 0x400;
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 8);
                TYRANT(g_CurrentEntity)->special_timer = 0xD2;
            }
        }
        if ((hit_frame == 4) && (TYRANT(g_CurrentEntity)->hit_mask & 8 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x1E;
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)2, 0);
        }
        if (((u8) g_CurrentEntity->animation_frame_id >= 8U) && ((u8) g_CurrentEntity->animation_frame_id < 0xFU)) {
            joint_base = ENTITY_AS(g_CurrentEntity, joints, void *);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x3E8;
            (g_ApiCallPtr = api_effect_create_billboard)((void *)0x11, 4, 0, JOINT_N_WORLD(joint_base, 8), ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vy) = 0x384;
            (g_ApiCallPtr = api_effect_create_billboard)((void *)0x11, 4, 0, JOINT_N_WORLD(joint_base, 8), ADDR_sp_vec, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 7) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 0xD) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)1, 0);
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200));
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = (u16) (ENTITY_AS(g_CurrentEntity, move_speed_current, u16) - g_CurrentEntity->animation_frame_id);
        if ((s16) ENTITY_AS(g_CurrentEntity, move_speed_current, u16) < 0) {
            ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0U;
        }
        break;
    case 4:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0xF7);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void tyrant_behavior_erupt(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->pos_y = 0x12C;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 6;
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200);
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        (g_ApiCallPtr = api_voice_command)(1U, 0x2D);
        g_GameStateFlags |= GAME_VOICE_PLAYING;
        (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x1C, 0, &g_CurrentEntity->pos_x);
        (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x1D, 0, &g_CurrentEntity->pos_x);
        (g_ApiCallPtr = api_sound_play_se_3d)(2U, 0x1E, 0, &g_CurrentEntity->pos_x);
        (g_ApiCallPtr = api_srand)(0xB23U);
        /* fallthrough */
    case 1:
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state++;
            return;
        }
        if (((s16) g_CurrentEntity->action_ticks_counter >= 0x1A) && ((s16) g_CurrentEntity->action_ticks_counter < 0x21)) {
            LIT(s32, sp_vec_vx) = ((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194;
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_rand)() & 0xFFF;
            (g_ApiCallPtr = api_effect_create_billboard)(0x18U, 3, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xF);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x18U, 5, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x14);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x18U, 5, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xF);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x19);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xA);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xF);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xA);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x18U, 3, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x14);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0xA);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x19U, 6, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 5);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x1194);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xFFF);
            (g_ApiCallPtr = api_effect_create_billboard)(0x18U, 3, LIT(s32, sp_tmp1C), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 5);
        }
        if (((s16) g_CurrentEntity->action_ticks_counter >= 0x26) && ((s16) g_CurrentEntity->action_ticks_counter < 0x37)) {
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0xFFF) - 0xBB8);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) ((g_ApiCallPtr = api_rand)() & 0x7FF);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 5, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0xFFF) - 0xBB8);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 5, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0xFFF) - 0xBB8);
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = (s32) -((g_ApiCallPtr = api_rand)() & 0x7FF);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 5, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0x1E);
        }
        return;
    case 2:
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200);
        if (g_CurrentEntity->animation_frame_id == 0x21) {
            g_CurrentEntity->action_state++;
            g_CurrentEntity->action_ticks_counter = 1U;
        }
        g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z - 8);
        g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - 8);
        return;
    case 3:
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) > 0) {
            return;
        }
        g_CurrentEntity->action_state += (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200);
        if ((u8) g_CurrentEntity->animation_frame_id < 0x23U) {
            g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z - 0x14);
            g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - 0x14);
        }
        g_CurrentEntity->pos_y -= 0xA;
        if (g_CurrentEntity->pos_y < 0) {
            g_CurrentEntity->pos_y = 0;
        }
        if (((u8) g_CurrentEntity->animation_frame_id >= 0x24U) && ((u8) g_CurrentEntity->animation_frame_id < 0x5AU) && !(g_CurrentEntity->animation_frame_id & 3 & 0xFF)) {
            joints = g_CurrentEntity->joints;
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            LIT(s32, sp_vec_vx) = 0;
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 1, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
            LIT(s32, sp_vec_vx) = 0;
            LIT(s32, sp_vec_vy) = ((g_ApiCallPtr = api_rand)() & 0x1FF) * -1;
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 0xF);
            (g_ApiCallPtr = api_effect_create_billboard)(9U, 1, 0, (LIT(s32, sp_tmp1C) * 0x7C) + joints + 0x44, ADDR_sp_vec, 0x14);
        }
        if (g_CurrentEntity->animation_frame_id == 0x2B) {
            TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 1);
        }
        return;
    case 4:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 2);
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void tyrant_behavior_rush(void) {
    u16 hit_frame;
    u8 *claw;
    VECTOR saved_pos;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = (u8) ((u8) g_CurrentEntity->animation_frame_id >> 1);
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 2;
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x190U;
        if ((g_ApiCallPtr = api_Flg_ck)(LITP(s32, g_SysFlags), 0x1E) != 0) {
            ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0x12CU;
        }
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0x3C;
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xD;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(s32, g_PlayerPosX), 0x80);
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
        if (LIT(s32, sp_tmp00) < 0x1770) {
            if ((g_ApiCallPtr = api_turn_toward_target)(LITP(s32, g_PlayerPosX), 0x200) == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
                if (TYRANT(g_CurrentEntity)->special_timer == 0) {
                    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 2);
                }
                TYRANT(g_CurrentEntity)->special_timer = 0xD2;
            }
        }
        saved_pos = *(VECTOR *)(&g_CurrentEntity->pos_x);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(ENTITY_AS(g_CurrentEntity, sca_info, void *))->radius);
        g_CurrentEntity->pos_x = saved_pos.vx;
        g_CurrentEntity->pos_y = saved_pos.vy;
        g_CurrentEntity->pos_z = saved_pos.vz;
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
            --ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16);
        }
        if (!((LIT(s32, sp_tmp00) == 0) && (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0))) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
            if (TYRANT(g_CurrentEntity)->special_timer == 0) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 2);
            }
            TYRANT(g_CurrentEntity)->special_timer = 0xD2;
        }
        if ((g_CurrentEntity->animation_frame_id == 3) || (g_CurrentEntity->animation_frame_id == 0xF)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(NULL, 0);
        }
        break;
    case 2:
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFB);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 0xB;
        TYRANT(g_CurrentEntity)->attack_cooldown = 0xF;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(s32, g_PlayerPosX), 8);
        if (g_CurrentEntity->animation_frame_id == 6) {
            g_TyrantTrailTimer = 0x800A;
        }
        if (g_CurrentEntity->animation_frame_id == 0xA) {
            (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)1, 0);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 3U) {
            ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = (u16) (ENTITY_AS(g_CurrentEntity, move_speed_current, u16) + 0x1E);
        }
        hit_frame = g_CurrentEntity->animation_frame_id - 9;
        if (hit_frame < 8U) {
            claw = ENTITY_AS(g_CurrentEntity, joints, u8 *);
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0;
            if (((g_ApiCallPtr = api_joint_check_player_in_range)(claw + 0x44, ADDR_sp_vec, 0x4B0, ADDR_g_PlayerPosX) != 0) && !(g_PlayerEntity.health & 0x8000)) {
                (g_ApiCallPtr = api_joint_apply_color_tint)(claw, 0xFF, 0x80880, 0x808080);
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 1, 0);
                if ((g_PlayerEntity.health - 0x12) > 0) {
                    ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(6, 0xC, 2, 0);
                    LIT(s32, sp_tmp0C) = g_PlayerEntity.pos_x - g_CurrentEntity->pos_x;
                    LIT(s32, sp_tmp10) = g_PlayerEntity.pos_z - g_CurrentEntity->pos_z;
                    if (LIT(s32, sp_tmp0C) != 0) {
                        LIT(s32, sp_tmp14) = (g_ApiCallPtr = api_catan)((s32 *) ((LIT(s32, sp_tmp10) * 0x1000) / (s32) LIT(s32, sp_tmp0C)));
                        g_PlayerEntity.angle = (((LIT(s32, sp_tmp0C) < 0) ? 0xC00 : 0x1400) - LIT(s32, sp_tmp14)) & 0xFFF;
                    } else {
                        g_PlayerEntity.angle = (LIT(s32, sp_tmp10) > 0) ? 0x1000 : 0x800;
                    }
                }
                g_PlayerEntity.aim_target = (s32)g_CurrentEntity;
                ENTITY_AS(g_CurrentEntity, angle, s16) += 0x400;
                g_PlayerEntity.animation_id = (g_ApiCallPtr = api_is_facing_toward_entity)(LITP(s32, g_PlayerEntity));
                ENTITY_AS(g_CurrentEntity, angle, s16) -= 0x400;
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 2);
                if (!(TYRANT(g_CurrentEntity)->flags & 4 & 0xFF)) {
                    TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 4);
                }
            }
        }
        if ((g_TyrantClawScaleB >= 0x1389) && (hit_frame >= 0xAU)) {
            g_TyrantClawScaleB = g_TyrantClawScaleB - 0x320;
        }
        if ((hit_frame == 8) && (TYRANT(g_CurrentEntity)->hit_mask & 2 & 0xFF)) {
            g_PlayerEntity.health = g_PlayerEntity.health - 0x1E;
            if (g_PlayerEntity.health & 0x8000) {
                g_PlayerEntity.health = 1;
                TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask | 0x80);
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)2, 0);
            } else {
                (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)3, 0);
            }
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if (((TYRANT(g_CurrentEntity)->hit_mask & 0x82 & 0xFF) == 0x82) && (g_CurrentEntity->animation_frame_id == 0x11)) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
            (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(s32, g_PlayerPosX), 0x20);
        }
        if (!(TYRANT(g_CurrentEntity)->hit_mask & 2 & 0xFF) && ((u8) g_CurrentEntity->animation_frame_id < 0x11U)) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(LITP(s32, g_PlayerPosX), 0x40);
        }
        if (((s16) ENTITY_AS(g_CurrentEntity, move_speed_current, u16) != 0) && ((u8) g_CurrentEntity->animation_frame_id >= 9U)) {
            ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = (u16) (ENTITY_AS(g_CurrentEntity, move_speed_current, u16) - g_CurrentEntity->animation_frame_id);
            if ((s16) ENTITY_AS(g_CurrentEntity, move_speed_current, u16) < 0) {
                ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = 0U;
            }
        }
        break;
    case 4:
        g_TyrantClawScaleB = (s16) g_TyrantClawScaleA;
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        if ((g_CurrentEntity->id == 0x10) && (TYRANT(g_CurrentEntity)->hit_mask & 2 & 0xFF)) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 5, 0);
            }
        }
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0x75);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void tyrant_behavior_rise(void) {
    s32 tyrant_root_motion_a2() __asm__("tyrant_root_motion");
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 0xC;
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 0x29U) && (g_TyrantClawScaleB < 0x2328)) {
        g_TyrantClawScaleB = (u16) g_TyrantClawScaleB + 0x3E8;
    }
    if (g_CurrentEntity->animation_frame_id == 0x28) {
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
    }
    tyrant_root_motion_a2(1, 1);
}

void tyrant_behavior_restrained(void) {
    s32 tyrant_root_motion_a3() __asm__("tyrant_root_motion");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0U;
        g_CurrentEntity->blend_counter = 0xF;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->health = -1;
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x100));
        if ((g_CurrentEntity->animation_frame_id == 0x19) && (((u8) g_CurrentEntity->timing_control) & 1 & 0xFF)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(6, 0U);
        }
        if ((g_CurrentEntity->animation_frame_id == 0x32) && (((u8) g_CurrentEntity->timing_control) & 1 & 0xFF)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(6, 0U);
        }
        break;
    case 2:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    }
    tyrant_root_motion_a3(0, 0);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void tyrant_behavior_yield_to_scd(void) {

    (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    g_CurrentEntity->behavior_flags = (u8) ((g_CurrentEntity->behavior_flags) | 0x40);
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->blend_counter = 0;
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(8, 0, 0, 1);
    (g_ApiCallPtr = api_sound_load_bank1_from_memory)();
}

void player_anim_enemy_interact(void) {
    s32 joints;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.blend_counter = 0;
        (*(u8 *)&g_PlayerEntity.hit_state) = 0x80;
        g_PlayerEntity.status_flags |= 6;
        (g_ApiCallPtr = api_pad_vibrate_large)(8, 0x50, 0);
        (g_ApiCallPtr = api_pad_vibrate_small)(0xF, 6);
        /* fallthrough */
    case 1:
        if (g_PlayerEntity.animation_frame_id == 8) {
            (g_ApiCallPtr = api_sound_play_se_3d)(3, 3, 0, ADDR_g_PlayerPosX);
            joints = g_PlayerEntity.joints;
            (g_ApiCallPtr = api_joint_apply_color_tint)(joints, 0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 1), 0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 2), 0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), 0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 12), 0x30, 0x80820, 0x606060);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0x50, 0xC8, 5);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x5A, 0xC8, 0, 0x23);
        }
        if ((u8) g_PlayerEntity.animation_frame_id < 9U) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = -0x5DC;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        }
        if (g_PlayerEntity.animation_frame_id == 0x5C) {
            (g_ApiCallPtr = api_pad_vibrate_large)(5, 0x96, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0x96, 0, 5);
        }
        if ((u8) g_PlayerEntity.animation_frame_id >= 0x60U) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        }
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(ADDR_g_PlayerEntity, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 2:
        g_PlayerEntity.health = -1;
    }
}

void player_anim_limb_physics(void) {
    extern s32 (*player_tyrant_hit_tbl[])();
    player_tyrant_hit_tbl[g_PlayerEntity.action_behavior]();
}

void player_tyrant_hit_00(void) {
    s32 claw;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0x258;
        g_PlayerEntity.hit_state = 1;
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)6, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)8, 0x50, 0xC8, 3);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)5, 0xC8, 0, 0xB);
        /* fallthrough */
    case 1:
        if (g_PlayerEntity.health < 0x1F) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(((void *) g_PlayerEntity.aim_target) + 0x34, 0x40);
        }
        if (g_PlayerEntity.animation_frame_id == 3) {
            (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, ((u8) g_PlayerEntity.animation_id) + 1, 0, ADDR_g_PlayerPosX);
        }
        if ((u8) g_PlayerEntity.animation_frame_id < 4U) {
            claw = ENT(((void *) g_PlayerEntity.aim_target))->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x320;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, claw + 0x44, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vy) = -0x514;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        break;
    case 2:
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
        g_PlayerEntity.hit_state = 0;
        claw;
    }
    g_PlayerEntity.move_speed_current -= g_PlayerEntity.animation_frame_id * 0x28;
    if ((s16) ((u16) g_PlayerEntity.move_speed_current) < 0) {
        g_PlayerEntity.move_speed_current = 0;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(((s16) ENT(((void *) g_PlayerEntity.aim_target))->angle) + 0xDF4 - ((s16) g_PlayerEntity.angle));
}

void player_tyrant_hit_01(void) {
    s32 claw;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0x1F4;
        g_PlayerEntity.hit_state = 1;
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)6, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)8, 0x50, 0xC8, 3);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)5, 0xC8, 0, 0xB);
        /* fallthrough */
    case 1:
        if (g_PlayerEntity.health < 0x1F) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(((void *) g_PlayerEntity.aim_target) + 0x34, 0x40);
        }
        if (g_PlayerEntity.animation_frame_id == 3) {
            (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, ((u8) g_PlayerEntity.animation_id) + 1, 0, ADDR_g_PlayerPosX);
        }
        if ((u8) g_PlayerEntity.animation_frame_id < 4U) {
            claw = ENT(((void *) g_PlayerEntity.aim_target))->joints;
            claw += 0x3E0;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x320;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, claw + 0x44, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vy) = -0x5DC;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        break;
    case 2:
        ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(1, 0, 0, 0);
        g_PlayerEntity.hit_state = 0;
        claw;
    }
    g_PlayerEntity.move_speed_current -= g_PlayerEntity.animation_frame_id * 0x1E;
    if ((s16) ((u16) g_PlayerEntity.move_speed_current) < 0) {
        g_PlayerEntity.move_speed_current = 0;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(((s16) ENT(((void *) g_PlayerEntity.aim_target))->angle) + 0x1F4 - ((s16) g_PlayerEntity.angle));
}

void player_tyrant_hit_02(void) {
    s32 joints;
    s32 pad1C;
    VECTOR saved_pos;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 5;
        g_PlayerEntity.blend_counter = 4;
        g_PlayerEntity.move_speed_current = 0x384;
        g_PlayerEntity.hit_state = 1;
        (g_ApiCallPtr = api_pad_vibrate_small)(6, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(8, 0x50, 0xC8, 3);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x14, 0xC8, 0, 0xB);
        /* fallthrough */
    case 1:
        g_PlayerEntity.move_speed_current -= g_PlayerEntity.animation_frame_id * 0xC;
        if (g_PlayerEntity.animation_frame_id == 3) {
            (g_ApiCallPtr = api_sound_play_se_3d)(3, 2, 0, ADDR_g_PlayerPosX);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        saved_pos = LIT(VECTOR, g_PlayerPosX);
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_check_room_collision)(ADDR_g_PlayerPosX, (s32) SCA(((void *) g_PlayerEntity.sca_info))->radius);
        *(VECTOR *)LITP(void, g_PlayerPosX) = saved_pos;
        if (LIT(s32, sp_tmp00) != 0) {
            g_PlayerEntity.action_state = 5;
            g_PlayerEntity.hit_state = 0;
        }
        return;
    case 2:
        g_PlayerEntity.action_state = 3;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 6;
        g_PlayerEntity.blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1F, 0, ADDR_g_PlayerPosX);
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
        g_PlayerEntity.move_speed_current -= 0xC;
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
        g_PlayerEntity.animation_id = 3;
        g_PlayerEntity.blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x20, 0, ADDR_g_PlayerPosX);
        (g_ApiCallPtr = api_sound_play_se_3d)(3, 2, 0, ADDR_g_PlayerPosX);
        joints = g_PlayerEntity.joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        SCRATCH_VEC->x = (s32) (SCRATCH_VEC->x - 0x190);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 5), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x16, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 0), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(9, 0x11, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(5, 0x64, 0xFA, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0x1E, 0xFA, 0, 5);
        /* fallthrough */
    case 6:
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        return;
    case 7:
        g_PlayerEntity.action_state = 8;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 4;
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
    }
}

void tyrant_root_motion(u8 foot, u8 apply) {
    void *joint;
    u8 i;
    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    if (g_CurrentEntity->scale != 0) {
        LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = g_CurrentEntity->scale;
        (g_ApiCallPtr = api_ScaleMatrix)(LITP(void, sp_matrix), LITP(void, sp_vec));
    }
    joint += (foot * 0x174) + 0x554;
    i = 3;
    while (i != 0) {
        (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), (joint - (i * 0x7C)) + 0xA0);
        i--;
    }
    LIT(s32, sp_matrix_t0) = (s32) (LIT(s32, sp_matrix_t0) - JOINT(joint)->world.t[0]);
    LIT(s32, sp_matrix_t1) = 0;
    LIT(s32, sp_matrix_t2) = (s32) (LIT(s32, sp_matrix_t2) - JOINT(joint)->world.t[2]);
    (g_ApiCallPtr = api_Square0)(LITP(void, sp_matrix_t0), LITP(void, sp_vec));
    g_CurrentEntity->move_speed_current = (g_ApiCallPtr = api_SquareRoot0)(LIT(s32, sp_vec_vx) + LIT(s32, sp_vec_vz));
    if (apply != 0) {
        g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - LIT(s32, sp_matrix_t0));
        g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z - LIT(s32, sp_matrix_t2));
    }
}

void tyrant_draw_heart(void) {
    MATRIX mtx;
    s32 joints;
    u16 health;

    if (!(g_CurrentEntity->has_enter_switch_zone & 0x7F & 0xFF)) {
        return;
    }
    health = (u16) g_CurrentEntity->health;
    (g_ApiCallPtr = api_update_entity_lighting)(&g_CurrentEntity->pos_x);
    joints = g_CurrentEntity->joints;
    LIT(Entity *, sp_tmp18) = g_CurrentEntity;
    g_CurrentEntity = ((Entity *) TYRANT(g_CurrentEntity)->heart);
    if (g_UpdateFlags & 4 & 0xFFFF) {
        (g_ApiCallPtr = api_RotMatrix)((s32 *) &g_CurrentEntity->rot_x, g_CurrentEntity->coord_m);
        if (!((s16) health & 0x8000)) {
            LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = (s16) g_CurrentEntity->position_vx + 0x1F4;
            (g_ApiCallPtr = api_ScaleMatrix)((s32 *) g_CurrentEntity->coord_m, LITP(u8, sp_vec));
            g_CurrentEntity->position_vx = g_CurrentEntity->position_vx + (*(s8 *)((u8 *)&tyrant_heartbeat_tbl + ((s16) g_CurrentEntity->action_ticks_counter)) * 2);
            if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
                g_CurrentEntity->action_ticks_counter = 0x15;
            }
        }
    }
    (g_ApiCallPtr = api_GsMulCoord0)(JOINT_N_WORLD(joints, 1), g_CurrentEntity->coord_m, &mtx);
    g_CurrentEntity->position_vy = mtx.t[1];
    (g_ApiCallPtr = api_GsMulCoord0)(g_pWorldScreenMatrix, &mtx, LITP(u8, sp_matrix));
    (g_ApiCallPtr = api_MulMatrix0)(g_pLightMatrix, JOINT_N_WORLD(joints, 1), &mtx);
    LIT(s32, sp_tmp38) = g_CurrentEntity->model_load_buffer;
    (g_ApiCallPtr = api_SetLightMatrix)(&mtx);
    (g_ApiCallPtr = api_GsSetLsMatrix)(LITP(s32, sp_matrix));
    (g_ApiCallPtr = api_GsPrstTG3L)(FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->primitive_top, s32 *), TMD_OBJ(LIT(s32, sp_tmp38))->vert_top, FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->normal_top, u8 *), g_CurrentEntity->packet + (g_ActiveBuffer * 0x28) + 8, FIELD_AT(LIT(s32, sp_tmp38), TmdObject, ->n_primitive, s32), 4, (g_ActiveBuffer * 0x14) + ADDR_g_OtMain);
    g_CurrentEntity = LIT(Entity *, sp_tmp18);
}

void tyrant_scroll_body_uvs(s16 du, s16 dv) {
    void *tmd;
    void *prim;
    s32 n;
    s32 prim_idx;
    s32 tbl_idx;
    void *joint;
    joint = ((void *) g_CurrentEntity->joints);
    joint += 0xF8;
    tmd = ((void *) JOINT(joint)->anim_slot);
    prim = (void *)(((s32) JOINT(joint)->anim_object) + 8);
    n = TMD_OBJ(tmd)->n_primitive;
    prim_idx = 0;
    tbl_idx = 0;
    while (n-- != 0) {
        if (prim_idx == tyrant_uv_scroll_poly_tbl[tbl_idx]) {
            GT3(prim)->u0 = (u8) (GT3(prim)->u0 + (u8) du);
            GT3(prim)->u1 = (u8) (GT3(prim)->u1 + (u8) du);
            GT3(prim)->u2 = (u8) (GT3(prim)->u2 + (u8) du);
            GT3(prim)->v0 = (u8) (GT3(prim)->v0 + (u8) dv);
            GT3(prim)->v1 = (u8) (GT3(prim)->v1 + (u8) dv);
            GT3(prim)->v2 = (u8) (GT3(prim)->v2 + (u8) dv);
            GT3(prim + 0x28)->u0 = (s8) (GT3(prim + 0x28)->u0 + (u8) du);
            GT3(prim + 0x28)->u1 = (s8) (GT3(prim + 0x28)->u1 + (u8) du);
            GT3(prim + 0x28)->u2 = (s8) (GT3(prim + 0x28)->u2 + (u8) du);
            GT3(prim + 0x28)->v0 = (s8) (GT3(prim + 0x28)->v0 + (u8) dv);
            GT3(prim + 0x28)->v1 = (s8) (GT3(prim + 0x28)->v1 + (u8) dv);
            GT3(prim + 0x28)->v2 = (s8) (GT3(prim + 0x28)->v2 + (u8) dv);
            tbl_idx++;
        }
        prim_idx++;
        prim += 0x50;
    }
}

void tyrant_clut_set_stp_unused(void) {
    struct { s16 x, y, w, h; } rect;
    void *slot;
    u32 clut[0x200];
    u16 *clut_pos;

    slot = ((void *) JOINT(((void *) g_CurrentEntity->joints))->anim_slot);
    clut_pos = ((u16 *) JOINT(slot)->data_ptr);
    clut_pos += 3;
    rect.x = *clut_pos & 0x3F;
    rect.y = (s16) ((u32) (*clut_pos & 0x7FC0 & 0xFFFF) >> 6);
    rect.w = 0x100;
    rect.h = 2;
    (g_ApiCallPtr = api_StoreImage)(&rect, clut);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    LIT(s32, sp_tmp00) = 0x200;
    while (--LIT(s32, sp_tmp00) != -1) {
        clut[LIT(s32, sp_tmp00)] |= 0x80008000;
    }
    g_TyrantClutRectY += 0x10;
    g_TyrantClutRectX += 0x10;
    (g_ApiCallPtr = api_LoadImage)(&rect, clut);
    (g_ApiCallPtr = api_DrawSync)(NULL);
}

void tyrant_state_scd(void) {
    extern s32 (*tyrant_scd_behavior_tbl[])();
    g_TyrantScdVictim = ADDR_g_EnemiesList;
    if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
        tyrant_scd_behavior_tbl[g_CurrentEntity->action_behavior]();
        return;
    }
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
    TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 2);
}

void tyrant_scd_behavior_idle(void) {
    extern s32 g_TyrantTrailTimer_s32 __asm__("g_TyrantTrailTimer");
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_TyrantTrailTimer_s32 = 0;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
}

void tyrant_scd_behavior_walk_to(void) {
    s32 tyrant_root_motion_a4() __asm__("tyrant_root_motion");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->action_ticks_counter = 2U;
        if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
            g_CurrentEntity->action_ticks_counter = 1U;
        }
        /* fallthrough */
    case 1:
        LIT(s32, sp_vec_vx) = (s32) g_CurrentEntity->anim_base_x;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = (s32) g_CurrentEntity->anim_base_z;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x28U);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80U);
            g_CurrentEntity->action_ticks_counter = 2U;
            if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
                g_CurrentEntity->action_ticks_counter = 1U;
            }
        }
        if ((g_ApiCallPtr = api_SquareRoot0)(((g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x) * (g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x)) + ((g_CurrentEntity->pos_z - g_CurrentEntity->anim_base_z) * ((g_CurrentEntity->pos_z) - (g_CurrentEntity->anim_base_z)))) < 0x5DCU) {
            g_CurrentEntity->action_state++;
        }
        break;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        if (g_CurrentEntity->collision_flags & 0x80 & 0xFF) {
            g_CurrentEntity->action_state = 1U;
        } else {
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        break;
    }
    if ((g_CurrentEntity->animation_frame_id != 3) && (g_CurrentEntity->animation_frame_id != 0x24)) {

    } else if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 1) != 0) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0U);
    }
    if (((u8) g_CurrentEntity->animation_frame_id >= 4U) && ((u8) g_CurrentEntity->animation_frame_id < 0x23U)) {
        tyrant_root_motion_a4(0, 0);
        ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = (u16) (ENTITY_AS(g_CurrentEntity, move_speed_current, u16) + 5);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    }
    tyrant_root_motion_a4(1, 0);
    ENTITY_AS(g_CurrentEntity, move_speed_current, u16) = (u16) (ENTITY_AS(g_CurrentEntity, move_speed_current, u16) + 5);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void tyrant_scd_behavior_break_glass(void) {
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 6;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80));
        if (g_CurrentEntity->animation_frame_id == 0x1E) {
            LIT(s32, sp_vec_vx) = 0x2A30;
            LIT(s32, sp_vec_vy) = -0x1004;
            LIT(s32, sp_vec_vz) = 0x1388;
            (g_ApiCallPtr = api_effect_create_billboard)(1, 0U, 0, 0, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x19U, 0, ADDR_sp_vec);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xA, 0x96U, 0x50, 0);
        }
        if (g_CurrentEntity->animation_frame_id == 0x55) {
            LIT(s32, sp_vec_vx) = 0x2A30;
            LIT(s32, sp_vec_vy) = -0x1004;
            LIT(s32, sp_vec_vz) = 0x1388;
            (g_ApiCallPtr = api_voice_command)(1, 0xB3U);
            g_GameStateFlags |= GAME_VOICE_PLAYING;
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1AU, 0, ADDR_sp_vec);
            (g_ApiCallPtr = api_sound_play_se_3d)(2, 0x1BU, 0, ADDR_sp_vec);
            (g_ApiCallPtr = api_pad_vibrate_small)(8, 0U);
            (g_ApiCallPtr = api_pad_vibrate_large)(0xA, 0xFAU, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)(0xF, 0xFAU, 0x50, 0xA);
            (g_ApiCallPtr = api_pad_vibrate_small)(4, 0x19U);
            (g_ApiCallPtr = api_pad_vibrate_small)(4, 0x20U);
            (g_ApiCallPtr = api_pad_vibrate_small)(8, 0x28U);
        }
        if (g_CurrentEntity->animation_frame_id == 0x59) {
            LIT(s32, sp_tmp1C) = ((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254;
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x14B4);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 2U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 3U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 4U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x1E);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x1E);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x1E);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x254);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x2E18);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x12C0);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 2U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 3U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 4U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x19);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xB00);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x28A0);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0x1388);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x19);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x14);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 2U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x19);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 3U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 4U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 5);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 3U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 4U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2AF8);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 5U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xA);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x19);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 5);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 6U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 7U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0x19);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) - 0x31C);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 5);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 1U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 5);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 2U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 5);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 3U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
            LIT(s32, sp_tmp1C) = (s32) (((g_ApiCallPtr = api_rand)() & 0x1FF) + 0xCF4);
            LIT(s32, sp_vec_vy) = (s32) (-0xA28 - ((g_ApiCallPtr = api_rand)() & 0xFFF));
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x2A30);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) + 0x1324);
            (g_ApiCallPtr = api_effect_create_billboard)(0x15, 4U, LIT(s32, sp_tmp1C), g_DeadMoveValue, ADDR_sp_vec, 0xF);
        }
        if (g_CurrentEntity->animation_frame_id == 0x91) {
            TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 1);
        }
        if ((g_CurrentEntity->animation_frame_id == 0x97) || (g_CurrentEntity->animation_frame_id == 0x8F)) {
            (g_ApiCallPtr = api_sound_play_entity_se)(0, 0U);
        }
        return;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void tyrant_scd_behavior_pod(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->pos_y = -0xC8;
        g_CurrentEntity->action_state++;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->animation_id = 6;
        g_CurrentEntity->action_ticks_counter = 5U;
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        /* fallthrough */
    case 1:
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_ticks_counter = 5U;
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y - 5);
            if (g_CurrentEntity->pos_y == -0x15E) {
                g_CurrentEntity->action_state++;
            }
        }
        break;
    case 2:
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_ticks_counter = 5U;
            g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 5);
            if (g_CurrentEntity->pos_y == -0xC8) {
                g_CurrentEntity->action_state--;
            }
        }
        break;
    case 3:
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 4);
        if (g_CurrentEntity->pos_y > 0) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            g_CurrentEntity->action_state++;
            g_CurrentEntity->pos_y = 0;
        }
        break;
    }
    if ((g_ApiCallPtr = api_Flg_ck)(ADDR_g_SysFlags, 0x1F) != 0) {
        g_CurrentEntity->action_state = 3U;
    }
}

void tyrant_scd_behavior_impale(void) {
    extern void * g_TyrantScdVictim_voidp __asm__("g_TyrantScdVictim");
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->move_speed_current = 0x64;
        (g_ApiCallPtr = api_snap_player_to_grab_position)(g_TyrantScdVictim_voidp);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        (g_ApiCallPtr = api_entity_rotate_toward_target)(g_TyrantScdVictim_voidp + 0x34, 0x400);
        ENT(g_TyrantScdVictim_voidp)->status_flags = (u8) (ENT(g_TyrantScdVictim_voidp)->status_flags | 6);
        FIELD_AT(g_TyrantScdVictim_voidp, Entity, ->state, s32) = 0x30001;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)1, 0);
        return;
    case 1:
        if (g_CurrentEntity->animation_frame_id == 6) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)4, 0);
        }
        if ((g_CurrentEntity->animation_frame_id != 6) && (g_CurrentEntity->animation_frame_id != 0x5C)) {

        } else {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x320;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 8), 0xFF, 0x80880, 0x808080);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 0x61U) {
            if (!((g_CurrentEntity->animation_frame_id % 7) & 0xFF)) {
                joints = g_CurrentEntity->joints;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vy) = 0x320;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
            }
        }
        if (g_CurrentEntity->animation_frame_id == 0x5F) {
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)7, 0);
        }
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80));
        if (g_CurrentEntity->move_speed_current != 0) {
            (g_ApiCallPtr = api_entity_rotate_toward_target)(g_TyrantScdVictim_voidp + 0x34, 0x10);
        }
        g_CurrentEntity->move_speed_current = (s16) ((u16) g_CurrentEntity->move_speed_current - (g_CurrentEntity->animation_frame_id * 5));
        if (g_CurrentEntity->move_speed_current < 0) {
            g_CurrentEntity->move_speed_current = 0;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        g_CurrentEntity->anim_base_x = (u16) (g_CurrentEntity->anim_base_x + g_CurrentEntity->speed_vx);
        g_CurrentEntity->anim_base_z = (u16) (g_CurrentEntity->anim_base_z + g_CurrentEntity->speed_vz);
        ENT(g_TyrantScdVictim_voidp)->anim_base_x = (u16) (ENT(g_TyrantScdVictim_voidp)->anim_base_x + g_CurrentEntity->speed_vx);
        ENT(g_TyrantScdVictim_voidp)->anim_base_z = (u16) (ENT(g_TyrantScdVictim_voidp)->anim_base_z + g_CurrentEntity->speed_vz);
        return;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_SysFlags), 0x1E);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        TYRANT(g_CurrentEntity)->hit_mask = (s8) (TYRANT(g_CurrentEntity)->hit_mask & 0xF8);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
    }
}

void tyrant_scd_behavior_rocket_death(void) {
    void tyrant_limb_launch_k() __asm__("tyrant_limb_launch");
    void tyrant_limb_update();
    extern struct { u8 pad[0x9C]; struct { s32 a; u16 b; u8 p6[2]; s32 c; u8 rest[0x20]; } arr[1]; } *g_RoomData;
    u8 *joint_base;
    SVECTOR rot;
    VECTOR pos;

    switch (g_CurrentEntity->action_state) {
    case 0:
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags | 8);
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0;
        TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFD);
        g_CurrentEntity->look_at_flags = 0;
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0;
        (g_ApiCallPtr = api_srand)((u8 *)0x5FE);
        MEMBER_AT(TYRANT(g_CurrentEntity)->heart, Entity, ->position_vz, s16) = -0x12C;
        (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x19, 0, &g_CurrentEntity->pos_x);
        (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1A, 0, &g_CurrentEntity->pos_x);
        (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1B, 0, &g_CurrentEntity->pos_x);
        joint_base = ENTITY_AS(g_CurrentEntity, joints, u8 *);
        *joint_base |= 0x28;
        (g_ApiCallPtr = api_joint_setup_attack_effect)(joint_base, 0x13, 0xC, (u8 *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joint_base, 1), 0x13, 0, (u8 *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joint_base, 3), 0x13, 0, (u8 *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joint_base, 6), 0x13, 0, (u8 *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joint_base, 9), 0x13, 0, (u8 *)3);
        LIT(s16, sp_svec_vx) = 0;
        LIT(s16, sp_svec_vy) = -0x1F4;
        LIT(s16, sp_svec_vz) = 0;
        rot.vx = 0x40;
        rot.vy = 0;
        rot.vz = 0x60;
        tyrant_limb_launch_k(JOINT_N(joint_base, 2), ADDR_sp_svec, &rot, 0xF, 3);
        LIT(s16, sp_svec_vx) = 0xA;
        LIT(s16, sp_svec_vy) = -0x190;
        LIT(s16, sp_svec_vz) = 0x19;
        rot.vx = 0x40;
        rot.vy = 0x60;
        rot.vz = 0;
        tyrant_limb_launch_k(JOINT_N(joint_base, 4), ADDR_sp_svec, &rot, 0xF, 3);
        LIT(s16, sp_svec_vx) = -0x1E;
        LIT(s16, sp_svec_vy) = -0x1C2;
        LIT(s16, sp_svec_vz) = 0xF;
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0x40;
        tyrant_limb_launch_k(JOINT_N(joint_base, 7), ADDR_sp_svec, &rot, 0xF, 3);
        LIT(s16, sp_svec_vx) = -0x14;
        LIT(s16, sp_svec_vy) = -0x15E;
        LIT(s16, sp_svec_vz) = -0xA;
        rot.vx = 0;
        rot.vy = 0x60;
        rot.vz = 0;
        tyrant_limb_launch_k(JOINT_N(joint_base, 10), ADDR_sp_svec, &rot, 0xF, 3);
        LIT(s16, sp_svec_vx) = -0x14;
        LIT(s16, sp_svec_vy) = -0x12C;
        LIT(s16, sp_svec_vz) = 0xF;
        rot.vx = 0;
        rot.vy = 0x40;
        rot.vz = 0x40;
        tyrant_limb_launch_k(JOINT_N(joint_base, 12), ADDR_sp_svec, &rot, 0xF, 3);
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0;
        g_CurrentEntity->speed_vx = (s16) ((g_RoomData->arr[g_CutId].a - g_CurrentEntity->pos_x) / 50);
        g_CurrentEntity->speed_vy = (s16) (g_RoomData->arr[g_CutId].b - ((g_CurrentEntity->pos_y - 0x3E8) / 50));
        g_CurrentEntity->speed_vz = (s16) ((g_RoomData->arr[g_CutId].c - g_CurrentEntity->pos_z) / 50);
        (g_ApiCallPtr = api_pad_vibrate_small)((u8 *)5, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((u8 *)5, 0x64, 0xFA, NULL);
        (g_ApiCallPtr = api_pad_vibrate_large)((u8 *)0xF, 0xFA, 5);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((u8 *)0x1E, 0xFA, 0x3C, (u8 *)0x14);
        (g_ApiCallPtr = api_pad_vibrate_large)((u8 *)0x14, 0x3C, 0x32);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((u8 *)0x4B, 0x3C, 0, (u8 *)0x46);
        (g_ApiCallPtr = api_pad_vibrate_small)((u8 *)2, 7);
        (g_ApiCallPtr = api_pad_vibrate_small)((u8 *)0xF, 0xB);
        /* fallthrough */
    case 1:
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 2) {
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x19, 0, &g_CurrentEntity->pos_x);
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1A, 0, &g_CurrentEntity->pos_x);
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1B, 0, &g_CurrentEntity->pos_x);
        }
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 6) {
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x19, 0, &g_CurrentEntity->pos_x);
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1A, 0, &g_CurrentEntity->pos_x);
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x1B, 0, &g_CurrentEntity->pos_x);
        }
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 9) {
            (g_ApiCallPtr = api_sound_play_se_3d)((u8 *)2, 0x19, 0, &g_CurrentEntity->pos_x);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 0xEU) {
            pos.vx = g_CurrentEntity->pos_x + ((ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) * 0xE7A) / 12);
            pos.vy = g_CurrentEntity->pos_y + (-(ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) * 0xFB0) / 12) - 0x9C4;
            pos.vz = g_CurrentEntity->pos_z + (-(ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) * 0x9AE) / 12);
            LIT(s32, sp_vec_vx) = pos.vx;
            LIT(s32, sp_vec_vy) = pos.vy;
            LIT(s32, sp_vec_vz) = pos.vz;
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, NULL, &pos, 0xA);
            LIT(s32, sp_vec_vx) = (s32) (pos.vx + 0x40 - ((g_ApiCallPtr = api_rand)() & 0x7F));
            LIT(s32, sp_vec_vy) = (s32) (pos.vy + 0x40 - ((g_ApiCallPtr = api_rand)() & 0x7F));
            LIT(s32, sp_vec_vz) = (s32) (pos.vz + 0x40 - ((g_ApiCallPtr = api_rand)() & 0x7F));
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, NULL, LITP(s32, sp_vec), 0x14);
            ++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16);
            if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) >= 0xB) {
                ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0xA;
                LIT(s32, sp_vec_vx) = (s32) (pos.vx + 0x100 - ((g_ApiCallPtr = api_rand)() & 0x1FF));
                LIT(s32, sp_vec_vy) = (s32) (pos.vy + 0x40 - ((g_ApiCallPtr = api_rand)() & 0x7F));
                LIT(s32, sp_vec_vz) = (s32) (pos.vz + 0x100 - ((g_ApiCallPtr = api_rand)() & 0x1FF));
                (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, NULL, LITP(s32, sp_vec), 0xF);
                LIT(s32, sp_vec_vx) = (s32) (pos.vx + 0x100 - ((g_ApiCallPtr = api_rand)() & 0x1FF));
                LIT(s32, sp_vec_vy) = (s32) (pos.vy + 0x40 - ((g_ApiCallPtr = api_rand)() & 0x7F));
                LIT(s32, sp_vec_vz) = (s32) (pos.vz + 0x100 - ((g_ApiCallPtr = api_rand)() & 0x1FF));
                (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 0xB, 0, NULL, LITP(s32, sp_vec), 0xA);
                TYRANT(g_CurrentEntity)->flags = (s8) (TYRANT(g_CurrentEntity)->flags & 0xFE);
            }
        }
        joint_base = ENTITY_AS(g_CurrentEntity, joints, u8 *);
        if (g_CurrentEntity->animation_frame_id == 8) {
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 2), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 4), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 5), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 7), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 8), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 10), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 11), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 12), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 13), 0, 0x102810, (u8 *)0x202030);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joint_base, 14), 0, 0x102810, (u8 *)0x202030);
        }
        if (((u8) g_CurrentEntity->animation_frame_id < 0xAU) && !(g_CurrentEntity->animation_frame_id & 7 & 0xFF)) {
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x1F4);
            LIT(s32, sp_vec_vy) = (s32) ((g_ApiCallPtr = api_rand)() & 0x3FF);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x1F4);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, JOINT_N_WORLD(joint_base, 2), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, JOINT_N_WORLD(joint_base, 4), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, JOINT_N_WORLD(joint_base, 7), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, JOINT_N_WORLD(joint_base, 10), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 3, 0, JOINT_N_WORLD(joint_base, 12), LITP(s32, sp_vec), 0x1E);
        }
        if (((u8) g_CurrentEntity->animation_frame_id >= 0x11U) && ((u8) g_CurrentEntity->animation_frame_id < 0x13U)) {
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 5, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0x15, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0x15, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 1, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 0x11, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            LIT(s32, sp_vec_vy) = (s32) (-0x3E8 - ((g_ApiCallPtr = api_rand)() & 0x7FF));
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)0xE, 1, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
        }
        if (((u8) g_CurrentEntity->animation_frame_id >= 0x13U) && !((g_CurrentEntity->animation_frame_id + 1) & 3)) {
            LIT(s32, sp_vec_vx) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x1F4);
            LIT(s32, sp_vec_vy) = (s32) ((g_ApiCallPtr = api_rand)() & 0x3FF);
            LIT(s32, sp_vec_vz) = (s32) (((g_ApiCallPtr = api_rand)() & 0x3FF) - 0x1F4);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 2), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 4), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 7), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 10), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 12), LITP(s32, sp_vec), 0x1E);
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (u8 *)0x200) != 0) {
            g_CurrentEntity->action_state = 2U;
            ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = 0;
        }
        if (MEMBER_AT(TYRANT(g_CurrentEntity)->heart, Entity, ->position_vy, s16) < -0x190) {
            ENT(TYRANT(g_CurrentEntity)->heart)->pos_y = (s32) (ENT(TYRANT(g_CurrentEntity)->heart)->pos_y + MEMBER_AT(TYRANT(g_CurrentEntity)->heart, Entity, ->position_vz, s16));
            MEMBER_AT(TYRANT(g_CurrentEntity)->heart, Entity, ->angle_z, s16) = (s16) (ENT(TYRANT(g_CurrentEntity)->heart)->angle_z + 0x18);
        }
        MEMBER_AT(TYRANT(g_CurrentEntity)->heart, Entity, ->position_vz, s16) = (s16) (ENT(TYRANT(g_CurrentEntity)->heart)->position_vz + 0xF);
        tyrant_limb_update(JOINT_N(joint_base, 2));
        tyrant_limb_update(JOINT_N(joint_base, 4));
        tyrant_limb_update(JOINT_N(joint_base, 7));
        tyrant_limb_update(JOINT_N(joint_base, 10));
        tyrant_limb_update(JOINT_N(joint_base, 12));
        return;
    case 2:
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) >= 0xC9) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->health = -1;
        }
        if ((s16) (++ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 3) == 0) {
            LIT(s32, sp_vec_vx) = ((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vz) = ((g_ApiCallPtr = api_rand)() & 0x7FF) - 0x3E8;
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, (u8 *)g_CurrentEntity + 0x20, LITP(s32, sp_vec), 0x1E);
        }
        if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 7) == 0) {
            LIT(s32, sp_vec_vx) = 0;
            LIT(s32, sp_vec_vy) = 0x3E8;
            LIT(s32, sp_vec_vz) = 0;
            joint_base = ENTITY_AS(g_CurrentEntity, joints, u8 *);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 4), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 7), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 10), LITP(s32, sp_vec), 0x1E);
            (g_ApiCallPtr = api_effect_create_billboard)((u8 *)9, 0xD, 0, JOINT_N_WORLD(joint_base, 12), LITP(s32, sp_vec), 0x1E);
            return;
        }
    }
}

void tyrant_limb_launch(void *joint, void *vel, void *rot, u16 gravity, u8 bounces) {
    JOINT(joint)->flags = (u8) (JOINT(joint)->flags | 8);
    JOINT(joint)->vel_x = (u16) ((u16) SVEC(vel)->vy);
    JOINT(joint)->vel_y = (u16) ((u16) SVEC(vel)->vx);
    JOINT(joint)->vel_z = (u16) ((u16) SVEC(vel)->vz);
    JOINT(joint)->rot_delta_x = gravity;
    JOINT(joint)->rot_delta_y = bounces;
    JOINT(joint)->rotation = *(SVECTOR *)rot;
}

void tyrant_limb_update(void *joint) {

    if (JOINT(joint)->rot_delta_y == 0) {
        return;
    }
    JOINT(joint)->world.t[0] = (s32) (JOINT(joint)->world.t[0] + JOINT(joint)->vel_y);
    JOINT(joint)->world.t[2] = (s32) (JOINT(joint)->world.t[2] + JOINT(joint)->vel_z);
    JOINT(joint)->vel_x = (u16) (((u16) JOINT(joint)->vel_x) + ((u16) JOINT(joint)->rot_delta_x));
    JOINT(joint)->world.t[1] = (s32) (JOINT(joint)->world.t[1] + (s16) ((u16) JOINT(joint)->vel_x));
    if (JOINT(joint)->world.t[1] >= -0xC7) {
        JOINT(joint)->world.t[1] = (s32) (JOINT(joint)->world.t[1] - (s16) ((u16) JOINT(joint)->vel_x));
        JOINT(joint)->vel_x /= -2;
        JOINT(joint)->rot_delta_y--;
    }
    (g_ApiCallPtr = api_RotMatrix)(&JOINT(joint)->rotation, &JOINT(joint)->transform);
    (g_ApiCallPtr = api_MulMatrix2)(&JOINT(joint)->transform, &JOINT(joint)->world);
}

void tyrant_scd_behavior_anim(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->move_speed_current = 0;
        (g_ApiCallPtr = api_pad_vibrate_large)(0x1E, 0x50, 0);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

void tyrant_scd_behavior_face_player(void) {

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->hit_state = 0;
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) != 0) {
            g_CurrentEntity->animation_id = 1;
        }
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x3F) + 0x28);
    }
    if (g_CurrentEntity->behavior_flags & 1 & 0xFF) {
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x14);
    } else {
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x28);
    }
    if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) || (LIT(s32, sp_tmp1C) == 0)) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
    } else {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        g_CurrentEntity->angle += LIT(u16, sp_tmp1C);
    }
    if ((g_CurrentEntity->animation_frame_id == 3) || (g_CurrentEntity->animation_frame_id == 0x24)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0, 0);
    }
}

void tyrant_scd_behavior_strike(void) {
    extern void * g_TyrantScdVictim_voidp __asm__("g_TyrantScdVictim");
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 7;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        if (g_CurrentEntity->animation_frame_id == 0xB) {
            FIELD_AT(g_TyrantScdVictim_voidp, Entity, ->state, s32) = 0x10001;
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = 0x320;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 3, 0, JOINT_N_WORLD(joints, 8), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 8), 0xFF, 0x80880, 0x808080);
            (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
        }
        break;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, 0x1E);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        joints;
    }
}
