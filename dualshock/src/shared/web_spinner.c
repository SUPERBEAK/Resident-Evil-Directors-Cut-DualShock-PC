/* web_spinner.c - Web spinner (giant spider): floor / ceiling behaviors, lunge and poison spit, leg wounds when hit,
   and the deaths (flips over or bursts, baby spiders come out). Twin of stage3/black_tiger.c.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/web_spinner.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* web spinner data (defined in each overlay that has this enemy) */
extern s32 web_spinner_sca_info_ptr;
extern s32 web_spinner_sca_info_ptr_dead;
extern s32 web_spinner_health_tbl;
extern u8 web_spinner_behavior_pick_tbl[][8];
extern u8 web_spinner_step_se_tbl;
extern u8 web_spinner_wound_joints[];
extern u8 g_WebSpinnerWoundedJoints[];

/* WebSpinnerWork: the web_spinner's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). WEB_SPINNER(e)->f, WEB_SPINNER_FIELD(e, f) and WEB_SPINNER_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct WebSpinnerWork {
    /* +0x00 */ s16 turn_step;         /* 0x16C: turn step for approach (0x80 / 0x20) and strafe (+-0x18, added to angle); web_spinner_behavior_circle reuses it as the add_speed_xz direction (+-0x32C + rand) */
    /* +0x02 */ u16 path_flags;        /* 0x16E: bit 0 = api_entity_pathfind_update result (web_spinner_behavior_dispatch); picks behaviors (web_spinner_behavior_pick_tbl[bit]) and gates turns */
    /* +0x04 */ s16 player_touch;      /* 0x170: api_check_entity_collision result vs the player (web_spinner_update); non-zero + no grab lets the lunge hit */
    /* +0x06 */ s16 attack_cooldown;   /* 0x172: 15 after a lunge/spit, counted down in web_spinner_state_run; handlers only start a new attack at 0 */
    /* +0x08 */ s16 death_started;     /* 0x174: set to 1 the first time web_spinner_state_death runs; when already set the death resumes at behavior 2 / action_state 3 */
    /* +0x0A */ s16 room_hit;          /* 0x176: api_check_room_collision result (web_spinner_update); non-zero counts wall_stuck_frames */
    /* +0x0C */ s16 wall_stuck_frames; /* 0x178: frames in a row against room geometry (web_spinner_state_run); >=0x1F makes the handlers change course; cleared after circling */
    /* +0x0E */ s16 wound_count;       /* 0x17A: wounds made so far; next index into g_WebSpinnerWoundedJoints (web_spinner_hit_react) */
} WebSpinnerWork;
#define WEB_SPINNER(e) ENEMY_WORK(e, WebSpinnerWork)
#define WEB_SPINNER_FIELD(e, f) ENEMY_FIELD(e, WebSpinnerWork, f)
#define WEB_SPINNER_MEMBER(e, f) ENEMY_MEMBER(e, WebSpinnerWork, f)

/* state 0: sets up a new web spinner (health, sca info, animation, work fields), then state 1 */
void web_spinner_state_init(void) {
    extern u8 g_StageId_u8 __asm__("g_StageId");

    g_CurrentEntity->sub_state = 0;
    g_CurrentEntity->action_behavior = 0;
    g_CurrentEntity->action_state = 0;
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->animation_id = 0;
    g_CurrentEntity->state = 1;
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (s16) (*(u8 *)((u8 *)&web_spinner_health_tbl + (((g_ApiCallPtr = api_rand)() & 0xF))));
    if (g_CurrentEntity->behavior_flags != 4) {
        if ((u8) g_CurrentEntity->behavior_flags < 2U) {
            g_CurrentEntity->rot_x = 0;
            g_CurrentEntity->angle_z = 0;
            g_CurrentEntity->pos_y = 0;
        }
        if ((u8) g_CurrentEntity->behavior_flags >= 2U) {
            g_CurrentEntity->rot_x = 0x800;
            g_CurrentEntity->angle_z = 0;
            g_CurrentEntity->pos_y = -0x17E8;
            if (g_StageId_u8 == 3) {
                g_CurrentEntity->pos_y = -0x1C20;
            }
            if ((u8) g_CurrentEntity->behavior_flags >= 4U) {
                g_CurrentEntity->behavior_flags = 2U;
            }
        }
    } else {
        g_CurrentEntity->behavior_flags = 0U;
        g_CurrentEntity->rot_x = 0;
        g_CurrentEntity->angle_z = 0;
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->state = 3;
        g_CurrentEntity->health = -1;
        g_CurrentEntity->hit_state = 0x11;
    }
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400);
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0x4B0, 0x4B0);
    g_CurrentEntity->sca_info = (s32) web_spinner_sca_info_ptr;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->scale = (s16) ((g_CurrentEntity->behavior_flags & 4 & 0xFF) * 0xAAA);
    WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) = 0;
    WEB_SPINNER_FIELD(g_CurrentEntity, death_started) = 0;
    WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    WEB_SPINNER_FIELD(g_CurrentEntity, wound_count) = 0;
    return;
}

/* every frame for each web spinner (enemies_update_functions_tbl): runs web_spinner_state_tbl[state], then collision checks and
   the shadow / drawing */
void web_spinner_update(void) {
    extern s32 (*web_spinner_state_tbl[])();

    if (g_UpdateFlags & 4 & 0xFFFF) {
        web_spinner_state_tbl[g_CurrentEntity->state]();
        if (g_CurrentEntity->state != 5) {
            (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
            WEB_SPINNER_MEMBER(g_CurrentEntity, player_touch) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
            (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            WEB_SPINNER_FIELD(g_CurrentEntity, room_hit) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
        }
    }
    if (!(g_UpdateFlags & 4 & 0xFFFF) && (g_CurrentEntity->state == 3) && (g_CurrentEntity->action_state == 6)) {
        web_spinner_state_tbl[g_CurrentEntity->state]();
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if ((g_CurrentEntity->has_enter_switch_zone != 0) && !(g_CurrentEntity->behavior_flags & 2 & 0xFF)) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void web_spinner_state_run(void) {
    void web_spinner_behavior_dispatch();
    void web_spinner_pick_behavior();

    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        web_spinner_pick_behavior();
        /* fallthrough */
    case 1:
        web_spinner_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
        if ((LIT(s32, sp_tmp00) < 0xBB8) && (g_CurrentEntity->action_behavior != 8)) {
            g_CurrentEntity->sub_state = 1U;
            ENTITY_ACTION_U16(g_CurrentEntity) = 8;
        }
        if ((g_PlayerEntity.action_behavior == 0x14) && (g_PlayerEntity.action_state == 0)) {
            g_CurrentEntity->sub_state = 1U;
            ENTITY_ACTION_U16(g_CurrentEntity) = 8;
        }
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
        WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown)--;
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, room_hit) != 0) {
        WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames)++;
        return;
    }
    WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0U;
}

void web_spinner_state_damaged(void) {
    void web_spinner_hit_react();
    if (g_CurrentEntity->action_behavior != 0) {
        return;
    }
    g_CurrentEntity->animation_id = 1;
    web_spinner_hit_react();
    return;
}

void web_spinner_state_death(void) {
    void web_spinner_die_burst();
    void web_spinner_die_flip();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->action_behavior = ((u8) g_CurrentEntity->hit_state) >> 3;
        g_CurrentEntity->sub_state = 1;
        if (WEB_SPINNER_FIELD(g_CurrentEntity, death_started) != 0) {
            g_CurrentEntity->action_state = 3;
            g_CurrentEntity->action_behavior = 2;
        }
        WEB_SPINNER_FIELD(g_CurrentEntity, death_started) = 1;
    }
    switch (g_CurrentEntity->action_behavior) {
    case 1:
    case 2:
    case 6:
    case 8:
    case 9:
        web_spinner_die_flip();
        break;
    default:
        g_CurrentEntity->animation_id = 1;
        web_spinner_die_burst();
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void web_spinner_state4_nop(void) {

}

void web_spinner_state5_nop(void) {

}

void web_spinner_behavior_dispatch(void) {
    void web_spinner_behavior_idle();
    void web_spinner_behavior_walk();
    void web_spinner_behavior_approach();
    void web_spinner_behavior_strafe();
    void web_spinner_behavior_lunge();
    void web_spinner_behavior_spit();
    void web_spinner_behavior_drop();
    void web_spinner_behavior_nop();
    void web_spinner_behavior_circle();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) = (u16) (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 0xFFFE);
        WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) = (u16) (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        web_spinner_behavior_idle();
        return;
    case 1:
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->angle = (g_CurrentEntity->angle + (((WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1) & 0xFFFF) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10)));
        web_spinner_behavior_walk();
        g_CurrentEntity->move_speed_current = (u16) g_CurrentEntity->move_speed_current + 0x3C;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    case 2:
        web_spinner_behavior_strafe();
        return;
    case 3:
        g_CurrentEntity->animation_id = 2;
        WEB_SPINNER_MEMBER(g_CurrentEntity, turn_step) = 0x80;
        web_spinner_behavior_approach();
        return;
    case 4:
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->angle = ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x30));
        web_spinner_behavior_walk();
        g_CurrentEntity->move_speed_current = (u16) g_CurrentEntity->move_speed_current + 0x64;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    case 5:
        g_CurrentEntity->animation_id = 3;
        WEB_SPINNER_MEMBER(g_CurrentEntity, turn_step) = 0x20;
        web_spinner_behavior_approach();
        return;
    case 6:
        web_spinner_behavior_lunge();
        return;
    case 7:
        web_spinner_behavior_spit();
        return;
    case 8:
        web_spinner_behavior_drop();
        return;
    case 9:
        web_spinner_behavior_nop();
        return;
    case 10:
        web_spinner_behavior_circle();
        return;
    case 11:
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->angle = ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10));
        web_spinner_behavior_walk();
        g_CurrentEntity->move_speed_current = (u16) g_CurrentEntity->move_speed_current + 0x14;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        break;
    }
}

void web_spinner_pick_behavior(void) {
    extern s32 (*web_spinner_pick_tbl[])();

    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    web_spinner_pick_tbl[g_CurrentEntity->behavior_flags]();
}

void web_spinner_pick_floor0(void) {
    if ((LIT(s32, sp_tmp00) < 0x1770) && !(WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 3;
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) == 0) {
        if (!((g_ApiCallPtr = api_rand)() & 0x1FF) && (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
        }
        if (LIT(s32, sp_tmp00) < 0x1B58) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_S16(g_CurrentEntity) = (s16) (((g_ApiCallPtr = api_rand)() & 1) + 6);
            }
        }
    }
    if ((g_PlayerEntity.action_behavior == 0x14) && (g_PlayerEntity.action_state == 0)) {
        if (!((g_ApiCallPtr = api_rand)() & 1)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xA;
        }
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames) >= 0x1F) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xA;
    }
    if (((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xB;
    }
}

void web_spinner_pick_floor1(void) {
    LIT(s32, sp_tmp18) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80);
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x100);
    if ((g_PlayerEntity.action_behavior == 0x14) && (LIT(s32, sp_tmp18) != 0) && (g_PlayerEntity.action_state == 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 3;
    }
    if ((LIT(s32, sp_tmp00) < 0x1F40) && (LIT(s32, sp_tmp18) != 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 3;
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) == 0) {
        if ((LIT(s32, sp_tmp00) < 0x1D4C) && (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = (s16) (((g_ApiCallPtr = api_rand)() & 1) + 6);
        }
        if ((LIT(s32, sp_tmp00) < 0x1770) && (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 7;
        }
    }
    if ((g_PlayerEntity.action_behavior == 0x14) && (g_PlayerEntity.action_state == 0)) {
        if (!((g_ApiCallPtr = api_rand)() & 0xF)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xA;
        }
    }
    if (WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames) >= 0x1F) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xA;
    }
}

void web_spinner_pick_ceiling(void) {
    if (LIT(s32, sp_tmp00) < 0xBB8) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 8;
        return;
    }
    if ((g_PlayerEntity.action_behavior == 0x14) && (g_PlayerEntity.action_state == 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 8;
        return;
    }
    if (LIT(s32, sp_tmp00) >= 0x1389) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
    }
    if (LIT(s32, sp_tmp00) >= 0x1389) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0xB;
        }
    }
}

void web_spinner_behavior_idle(void) {
    extern s32 (*web_spinner_idle_tbl[])();
    web_spinner_idle_tbl[g_CurrentEntity->behavior_flags]();
}

void web_spinner_idle_then_pick(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0;
        g_CurrentEntity->action_ticks_counter = (u16) ((((g_ApiCallPtr = api_rand)() & 0x3F) + 0x50) - ((WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) * 0x50));
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            g_CurrentEntity->action_behavior = web_spinner_behavior_pick_tbl[WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1][(g_ApiCallPtr = api_rand)() & 7];
            g_CurrentEntity->action_state = 0U;
        }
        return;
    }
}

void web_spinner_idle_hold(void) {
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

void web_spinner_behavior_walk(void) {
    s32 web_spinner_leg_step_speed_a1() __asm__("web_spinner_leg_step_speed");
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
        /* fallthrough */
    case 1:
        if (*(&web_spinner_step_se_tbl + g_CurrentEntity->animation_frame_id) != 0) {
            (g_ApiCallPtr = api_sound_play_entity_se)(*(&web_spinner_step_se_tbl + g_CurrentEntity->animation_frame_id), 0);
        }
        (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if ((g_CurrentEntity->animation_frame_id != 0) && ((u8) g_CurrentEntity->animation_frame_id < 9U)) {
            web_spinner_leg_step_speed_a1(1, g_CurrentEntity->scale);
        } else {
            web_spinner_leg_step_speed_a1(0, g_CurrentEntity->scale);
        }
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_U16(g_CurrentEntity) = 0;
        }
    }
}

void web_spinner_behavior_approach(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x50);
        /* fallthrough */
    case 1:
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, ((u16) WEB_SPINNER_MEMBER(g_CurrentEntity, turn_step)));
                if (!((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) || (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C));
        return;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_AS(g_CurrentEntity, action_behavior, s8) = (s8) ((WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) * 0xB);
        g_CurrentEntity->action_state = 0U;
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        return;
    }
}

void web_spinner_behavior_strafe(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 3;
        g_CurrentEntity->action_ticks_counter = (u16) ((g_ApiCallPtr = api_rand)() & 0x3F);
        g_CurrentEntity->blend_counter = 3;
        WEB_SPINNER_FIELD(g_CurrentEntity, turn_step) = 0x18U;
        if ((s16) (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) & 1) != 0) {
            WEB_SPINNER_FIELD(g_CurrentEntity, turn_step) = -0x18;
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((u16) WEB_SPINNER_FIELD(g_CurrentEntity, turn_step)));
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        if (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) {
            if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, (s32) ((u16) WEB_SPINNER_MEMBER(g_CurrentEntity, turn_step))) == 0) {
                ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 4;
                g_CurrentEntity->action_state = 0U;
                return;
            }
        }
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_AS(g_CurrentEntity, action_behavior, s8) = (s8) ((WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) * 4);
            g_CurrentEntity->action_state = 0U;
        }
        return;
    }
}

void web_spinner_behavior_lunge(void) {
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
        if ((((u16) WEB_SPINNER_MEMBER(g_CurrentEntity, player_touch)) != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_is_facing_toward_entity)(LITP(s32, g_PlayerEntity));
            g_CurrentEntity->action_state = 6;
            (g_ApiCallPtr = api_sound_play_entity_se)((s32 *)4, 0);
            g_PlayerEntity.health -= 0x12;
            if ((s16) g_PlayerEntity.health < 0) {
                g_PlayerEntity.health = 1;
            }
            if ((s16) g_PlayerEntity.health >= 0) {
                g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp1C) + 1;
                g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp1C) + 0x66;
            }
            (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)3, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)8, 0xFA, 0);
        }
        return;
    case 6:
        g_CurrentEntity->action_state = 7;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 9;
        g_CurrentEntity->action_ticks_counter = 7;
        /* fallthrough */
    case 7:
        g_CurrentEntity->action_state = (g_CurrentEntity->action_state) + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->move_speed_current = 0x64;
        (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        return;
    case 8:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) = 0xF;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void web_spinner_behavior_spit(void) {
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
        (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0xB;
        WEB_SPINNER_FIELD(g_CurrentEntity, attack_cooldown) = 0xF;
    }
}

void web_spinner_behavior_drop(void) {
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
        g_CurrentEntity->rot_x = (u16) (ENTITY_AS(g_CurrentEntity, rot_x, s16) - 0x100);
        if ((s16) g_CurrentEntity->rot_x & 0x80000000) {
            g_CurrentEntity->rot_x = 0U;
        }
        if (!(g_CurrentEntity->pos_y & 0x80000000)) {
            g_CurrentEntity->pos_y = 0;
            g_CurrentEntity->rot_x = 0U;
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 2:
        g_CurrentEntity->action_state = 3U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 7;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xFD);
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


void web_spinner_behavior_nop(void) {

}

void web_spinner_behavior_circle(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_id = 6;
        WEB_SPINNER_FIELD(g_CurrentEntity, turn_step) = (s16) (0x32C - (((g_ApiCallPtr = api_rand)() & 1) * 0x658));
        WEB_SPINNER_FIELD(g_CurrentEntity, turn_step) += ((g_ApiCallPtr = api_rand)() & 0xFFF) * !(WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1);
        g_CurrentEntity->move_speed_current = (s16) (((g_ApiCallPtr = api_rand)() & 0x3F) + 0xC8);
        (g_ApiCallPtr = api_entity_add_speed_xz)(WEB_SPINNER_FIELD(g_CurrentEntity, turn_step));
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40));
        g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x + g_CurrentEntity->speed_vx);
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + g_CurrentEntity->speed_vy);
        g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z + g_CurrentEntity->speed_vz);
        return;
    case 2:
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->action_behavior = (s8) ((WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) * 4);
        g_CurrentEntity->action_state = 0U;
        WEB_SPINNER_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    }
}

void web_spinner_hit_react(void) {
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
                if (web_spinner_wound_joints[LIT(s32, sp_tmp1C)] == g_WebSpinnerWoundedJoints[LIT(s32, sp_tmp18)]) {
                    LIT(s32, sp_tmp14) = 1;
                }
            }
            if (LIT(s32, sp_tmp14) == 0) {
                joints = g_CurrentEntity->joints;
                g_WebSpinnerWoundedJoints[WEB_SPINNER_FIELD(g_CurrentEntity, wound_count)] = web_spinner_wound_joints[LIT(s32, sp_tmp1C)];
                (g_ApiCallPtr = api_joint_setup_attack_effect)((web_spinner_wound_joints[LIT(s32, sp_tmp1C)] * 0x7C) + joints, 0x1E, 0x14, 3);
                (g_ApiCallPtr = api_joint_setup_attack_effect)((web_spinner_wound_joints[LIT(s32, sp_tmp1C)] * 0x7C) + joints + 0x7C, 0x1E, 0x14, 3);
                LIT(s32, sp_vec_vz) = 0;
                LIT(s32, sp_vec_vy) = 0;
                LIT(s32, sp_vec_vx) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 8, 0, (web_spinner_wound_joints[LIT(s32, sp_tmp1C)] * 0x7C) + joints + 0xC0, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_effect_create_billboard)(0, 8, 0, (web_spinner_wound_joints[LIT(s32, sp_tmp1C)] * 0x7C) + joints + 0x44, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
                WEB_SPINNER_FIELD(g_CurrentEntity, wound_count)++;
            }
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        return;
    case 2:
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        g_CurrentEntity->action_behavior = (s8) ((g_CurrentEntity->behavior_flags & 1) + 3);
        g_CurrentEntity->action_state = 0U;
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_CurrentEntity->hit_state = 0U;
        if (WEB_SPINNER_MEMBER(g_CurrentEntity, path_flags) & 1 & 0xFFFF) {
            g_CurrentEntity->sub_state = 1;
            g_CurrentEntity->action_behavior = 0xA;
        }
        return;
    }
}

void web_spinner_die_burst(void) {
    void *joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_ticks_counter = 0x1EU;
        g_CurrentEntity->blend_counter = 3;
        joints = ENTITY_AS(g_CurrentEntity, joints, void *);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(joints, 0x1EU, 0x1E, (void *)3);
        (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1EU, 0x1E, (void *)3);
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8U, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8U, 0, JOINT_N_WORLD(joints, 19), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0U);
        LIT(s32, sp_tmp1C) = 9;
        while ((LIT(s32, sp_tmp1C) = LIT(s32, sp_tmp1C) - 1) != 0) {
            if ((FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) == 0) || (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) & 0x20 & 0xFF)) {
                continue;
            }
            FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [1].flags, u8) | 0xC);
            FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, s8) = (s8) (FIELD_AT(((LIT(s32, sp_tmp1C) * 0xF8) + joints), Joint, [2].flags, u8) | 0x10);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, (void *)0xDF809F);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x64U, -0x64);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_ticks_counter = 0x5AU;
        g_CurrentEntity->sca_info = (s32) web_spinner_sca_info_ptr_dead;
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 3U, 3);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state = 6U;
            (g_ApiCallPtr = api_entity_clone)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 0x2) + 8, 0xF2U, (s32) g_CurrentEntity->joint_count, &g_CurrentEntity->aim_target);
        }
        return;
    case 6:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 2) + 8);
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void web_spinner_die_flip(void) {
    void *joints;

    if ((g_CurrentEntity->action_state == 1) || (g_CurrentEntity->action_state == 3)) {
        if (ENTITY_AS(g_CurrentEntity, hit_state, u8) & 2 & 0xFF) {
            joints = ENTITY_AS(g_CurrentEntity, joints, void *);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(joints, 0x1EU, 0x1E, (void *)3);
            (g_ApiCallPtr = api_joint_setup_attack_effect)(JOINT_N(joints, 19), 0x1EU, 0x1E, (void *)3);
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8U, 0, &JOINT(joints)->world, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 8U, 0, JOINT_N_WORLD(joints, 19), ADDR_sp_vec, 0);
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
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        g_CurrentEntity->animation_id = 4;
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        /* fallthrough */
    case 1:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        if ((u8) g_CurrentEntity->animation_frame_id >= 0xEU) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        }
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 2:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        ENTITY_AS(g_CurrentEntity, hit_state, u8) = 0U;
        ENTITY_FIELD_AS(g_CurrentEntity, shadow.poly[0].r0, u16) = 0x809F;
        ENTITY_FIELD_AS(g_CurrentEntity, shadow.poly[1].r0, u16) = 0x809F;
        g_CurrentEntity->shadow.poly[0].b0 = 0xDF;
        g_CurrentEntity->shadow.poly[1].b0 = 0xDF;
        /* fallthrough */
    case 3:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        return;
    case 4:
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0U);
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, (void *)0xDF809F);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x64U, -0x64);
        g_CurrentEntity->action_state = 5U;
        g_CurrentEntity->action_ticks_counter = 0x78U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        g_CurrentEntity->sca_info = (s32) web_spinner_sca_info_ptr_dead;
        /* fallthrough */
    case 5:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 3U, 3);
        (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state = 6U;
            (g_ApiCallPtr = api_entity_clone)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 0x2) + 4, 0xF2U, (s32) g_CurrentEntity->joint_count, &g_CurrentEntity->aim_target);
        }
        return;
    case 6:
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        (g_ApiCallPtr = api_web_spinner_update_webs)(((((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >> 3) & 0xFF) * 2) + 4);
        break;
    }
}

void web_spinner_leg_step_speed(u8 leg, s32 scale) {
    s32 joint;
    u8 i;
    joint = g_CurrentEntity->joints;
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, g_CurrentEntity->coord_m);
    (g_ApiCallPtr = api_GsMulCoord0)((u16 *) g_CurrentEntity->coord_m, &JOINT(joint)->transform, ADDR_sp_matrix);
    if (scale != 0) {
        LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vy) = LIT(s32, sp_vec_vz) = scale;
        (g_ApiCallPtr = api_ScaleMatrix)(LITP(u16, sp_matrix), LITP(u8, sp_vec));
    }
    joint = (s32)((u8 *) joint + ((leg * 0x3E0) + 0x1F0));
    if (!(*(u8 *)(joint - 0x7C) & 1 & 0xFF)) {
        g_CurrentEntity->move_speed_current = 0;
        return;
    }
    i = 2;
    while (i != 0) {
        (g_ApiCallPtr = api_GsMulCoord3)(LITP(u16, sp_matrix), (joint - (i * 0x7C)) + 0xA0);
        i--;
    }
    LIT(s32, sp_matrix_t0) = (s32) (LIT(s32, sp_matrix_t0) - JOINT(joint)->world.t[0]);
    LIT(s32, sp_matrix_t1) = 0;
    LIT(s32, sp_matrix_t2) = (s32) (LIT(s32, sp_matrix_t2) - JOINT(joint)->world.t[2]);
    (g_ApiCallPtr = api_Square0)(LITP(u16, sp_matrix_t0), LITP(u8, sp_vec));
    g_CurrentEntity->move_speed_current = (g_ApiCallPtr = api_SquareRoot0)(LIT(s32, sp_vec_vx) + LIT(s32, sp_vec_vz));
}
