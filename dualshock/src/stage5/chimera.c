/* chimera.c (stage5) - Chimera: floor / ceiling idles, walk, swipe, drop and swoop from the ceiling,
   spit, hit reactions and dissolving deaths, and the player's knocked-down animation. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* chimera data */
extern s32 chimera_sca_info_ptr;
extern s8 chimera_seed_tbl;
extern s16 chimera_arc_heights[];
extern s32 chimera_swoop_speed;
extern s8 chimera_death_anim_tbl;

/* ChimeraWork: the chimera's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). CHIMERA(e)->f, CHIMERA_FIELD(e, f) and CHIMERA_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct ChimeraWork {
    /* +0x00 */ s16 turn_step;           /* 0x16C: turn rate given to api_turn_toward_target in turn/turn_back (0x20 walking, 0x40 running), set by the brains */
    /* +0x02 */ s16 path_flags;          /* 0x16E: bit 0 = api_entity_pathfind_update result bit 0 (chimera_behavior_dispatch); gates turning, brains and ceiling idle time */
    /* +0x04 */ s16 player_touch;        /* 0x170: api_check_entity_collision result (chimera_update); claw/brain_b hit checks */
    /* +0x06 */ s16 attack_cooldown;     /* 0x172: 0x3C after swipe, 0x5A after grab/swoop, 0x28 after spit; counted down in chimera_state_run; gates re-attacks */
    /* +0x08 */ s16 shadow_locked;       /* 0x174: 0 = chimera_update resizes the shadow billboard by height; set to 1 when the death dissolve starts */
    /* +0x0A */ s16 room_hit;            /* 0x176: api_check_room_collision result (chimera_update) */
    /* +0x0C */ s16 wall_stuck_frames;   /* 0x178: frames touching walls (chimera_state_run); >= 21 makes the brains pick a turn and reset it */
    /* +0x0E */ s16 run_mode;            /* 0x17A: 1 = run (anim 2, speed 0xC8) instead of walk; set when far away or after turning with a clear line, cleared by most behaviors */
    /* +0x10 */ s16 grab_struggle_timer; /* 0x17C: 0x5A when the grab holds the player; input subtracts 3, copied to the player's action_ticks_counter, released at < 0 */
    /* +0x12 */ s16 aggressive;          /* 0x17E: 1 only for spawn behavior 3 (chimera_state_init, then a random variant); unlocks extra attacks (claw touch damage, follow-ups 5/0xC) */
} ChimeraWork;
#define CHIMERA(e) ENEMY_WORK(e, ChimeraWork)
#define CHIMERA_FIELD(e, f) ENEMY_FIELD(e, ChimeraWork, f)
#define CHIMERA_MEMBER(e, f) ENEMY_MEMBER(e, ChimeraWork, f)

/* state 0: sets up a new chimera (health, sca info, animation, work fields), then state 1 */
void chimera_state_init() {
    ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->hit_state = 0;
    CHIMERA_FIELD(g_CurrentEntity, aggressive) = 0;
    if (g_CurrentEntity->behavior_flags == 3) {
        CHIMERA_FIELD(g_CurrentEntity, aggressive) = 1;
        g_CurrentEntity->behavior_flags = (g_ApiCallPtr = api_rand)() & 1;
    }
    if ((u8) g_CurrentEntity->behavior_flags < 2U) {
        g_CurrentEntity->rot_x = 0;
        g_CurrentEntity->angle_z = 0;
        g_CurrentEntity->pos_y = 0;
    }
    if ((u8) g_CurrentEntity->behavior_flags >= 2U) {
        g_CurrentEntity->rot_x = 0;
        g_CurrentEntity->angle_z = 0x800;
        g_CurrentEntity->pos_y = -0x17E8;
        g_CurrentEntity->behavior_flags = 2U;
    }
    (g_ApiCallPtr = api_reset_joint_transforms)();
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0, 0);
    (g_ApiCallPtr = api_rand)();
    g_CurrentEntity->health = (0x28 + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7) + ((g_ApiCallPtr = api_rand)() & 7)) * 2;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->animation_id = (u8) g_CurrentEntity->behavior_flags;
    (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x40);
    g_CurrentEntity->sca_info = (s32) chimera_sca_info_ptr;
    g_CurrentEntity->scale = 0;
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) = 0;
    CHIMERA_FIELD(g_CurrentEntity, shadow_locked) = 0;
    CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
}

/* every frame for each chimera (enemies_update_functions_tbl): runs chimera_state_tbl[state], then collision checks and
   the shadow / drawing */
void chimera_update(void) {
    extern s32 (*chimera_state_tbl[])();
    s32 joints;
    u16 unused;

    joints = g_CurrentEntity->joints;
    if (g_UpdateFlags & 4 & 0xFFFF) {
        chimera_state_tbl[g_CurrentEntity->state]();
        (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
        CHIMERA_FIELD(g_CurrentEntity, player_touch) = (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
        (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
        g_CurrentEntity->collision_flags = (u8) (g_CurrentEntity->collision_flags & 0xF7);
        CHIMERA_FIELD(g_CurrentEntity, room_hit) = (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius);
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        if (CHIMERA_FIELD(g_CurrentEntity, shadow_locked) == 0) {
            LIT(s32, sp_tmp0C) = ((s32) JOINT(JOINT_N(joints, 3))->world.t[1] >> 4) + 0x320;
            if ((u32) (-JOINT(JOINT_N(joints, 3))->world.t[1] - 0x898) < 0x7D0U) {
                LIT(s32, sp_tmp0C) = 0x258;
            }
            if (g_CurrentEntity->pos_y < -0x17D4) {
                LIT(s32, sp_tmp0C) = (s32) (LIT(s32, sp_tmp0C) - 0xC8);
            }
            (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, LIT(s32, sp_tmp0C) + 0x32, LIT(s32, sp_tmp0C) + 0xC8);
        }
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void chimera_state_run(void) {
    void chimera_behavior_dispatch();
    void chimera_variant_ai();

    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    switch ((((u8) g_CurrentEntity->sub_state))) {
    case 0:
        chimera_variant_ai();
        /* fallthrough */
    case 1:
        chimera_behavior_dispatch();
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    if ((g_CurrentEntity->behavior_flags != 0) && (g_CurrentEntity->action_behavior == 0)) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    }
    if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        (g_ApiCallPtr = api_entity_check_alert_range)(0x1388);
    }
    if (CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
        CHIMERA_FIELD(g_CurrentEntity, attack_cooldown)--;
    }
    if (CHIMERA_FIELD(g_CurrentEntity, room_hit) != 0) {
        CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames)++;
        return;
    }
    CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) = 0U;
}

void chimera_hit_dispatch(void) {
    void chimera_hit_stagger();
    void chimera_hit_knockdown();
    if ((u8) g_CurrentEntity->sub_state == 0) {
        g_CurrentEntity->action_behavior = *(&chimera_seed_tbl + ((u8) g_CurrentEntity->hit_state & 7 & 0xFF));
        g_CurrentEntity->action_behavior += (API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) >> 10) & 1;
        g_CurrentEntity->sub_state = 1;
        g_CurrentEntity->action_state = 0;
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        g_CurrentEntity->animation_id = 0xD;
        chimera_hit_stagger();
        return;
    case 1:
        g_CurrentEntity->animation_id = 0xC;
        chimera_hit_stagger();
        return;
    case 2:
    case 3:
        g_CurrentEntity->animation_id = 0xF;
        if (g_CurrentEntity->behavior_flags != 0) {
            g_CurrentEntity->animation_id = 6;
        }
        chimera_hit_stagger();
        return;
    case 4:
    case 5:
        g_CurrentEntity->animation_id = 0xB;
        chimera_hit_knockdown();
        break;
    }
}

void chimera_death_dispatch(void) {
    void chimera_death_dissolve();
    void chimera_death_drop_dissolve();

    if (((u8) g_CurrentEntity->sub_state) == 0) {
        g_CurrentEntity->action_behavior = (API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) >> 10) & 1;
        g_CurrentEntity->sub_state = 1U;
        g_CurrentEntity->action_state = 0;
        if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
            g_CurrentEntity->action_behavior = 4U;
        }
    }
    switch ((g_CurrentEntity->action_behavior)) {
    case 0:
        g_CurrentEntity->animation_id = 0xB;
        chimera_death_dissolve();
        return;
    case 1:
        g_CurrentEntity->animation_id = 0xC;
        chimera_death_dissolve();
        return;
    case 4:
        g_CurrentEntity->animation_id = 0xB;
        chimera_death_drop_dissolve();
        return;
    }
}

void chimera_behavior_dispatch(void) {
    extern s32 (*chimera_behavior_tbl[])();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
        CHIMERA_FIELD(g_CurrentEntity, path_flags) = (u16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & ~1);
        CHIMERA_FIELD(g_CurrentEntity, path_flags) = (u16) (((u16) CHIMERA_FIELD(g_CurrentEntity, path_flags)) | ((u16) LIT(s32, sp_tmp1C) & 1));
    }
    chimera_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void chimera_variant_ai(void) {
    extern s32 (*chimera_brain_tbl[])();
    LIT(s32, sp_tmp00) = (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + (__builtin_abs(g_PlayerEntity.pos_z - (g_CurrentEntity->pos_z)));
    chimera_brain_tbl[g_CurrentEntity->behavior_flags]();
}

void chimera_brain_a(void) {
    u16 prev_behavior;
    s16 prev_state;

    prev_behavior = (u16) g_CurrentEntity->action_behavior;
    prev_state = (s16) g_CurrentEntity->action_state;
    if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->action_behavior = 1U;
        if (prev_behavior != 1) {
            g_CurrentEntity->animation_id = 9;
            g_CurrentEntity->move_speed_current = 0xB4;
            g_CurrentEntity->action_state = 0U;
        }
    }
    if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x200, 0x5DC, ADDR_g_PlayerPosX) != 0) {
        if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 7;
            if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
                ENTITY_ACTION_U16(g_CurrentEntity) = 0xC;
                if (CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
                    g_CurrentEntity->action_behavior = 5U;
                    g_CurrentEntity->action_state = (u8) prev_state;
                    if (prev_behavior != 5) {
                        g_CurrentEntity->action_state = 0U;
                    }
                }
                return;
            }
            if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) {
                ENTITY_ACTION_U16(g_CurrentEntity) = 0xC;
            }
            return;
        }
    }
    if ((((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) == 0) && (LIT(s32, sp_tmp00) < 0x1770)) || (CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) >= 0x15)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 0xB;
        CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
    }
    if ((g_PlayerEntity.action_behavior == 0x14) && (LIT(s32, sp_tmp00) >= 0x7D1)) {
        if (((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) && (g_PlayerEntity.action_state == 0)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = (s16) ((((g_ApiCallPtr = api_rand)() & 1) * 4) + 7);
            if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
                ENTITY_ACTION_U16(g_CurrentEntity) = 0xB;
            }
        }
    }
    if (((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 0xD;
    }
}

void chimera_brain_b(void) {
    u16 prev_behavior;
    s16 prev_state;

    prev_behavior = (u16) g_CurrentEntity->action_behavior;
    prev_state = (s16) g_CurrentEntity->action_state;
    if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->action_behavior = 1U;
        if (prev_behavior != 1) {
            g_CurrentEntity->action_state = 0U;
            if (LIT(s32, sp_tmp00) >= 0x1389) {
                CHIMERA_FIELD(g_CurrentEntity, run_mode) = 1;
            }
            g_CurrentEntity->animation_id = 3;
            g_CurrentEntity->move_speed_current = 0x64;
            if (CHIMERA_FIELD(g_CurrentEntity, run_mode) != 0) {
                g_CurrentEntity->animation_id = 2;
                g_CurrentEntity->move_speed_current = 0xC8;
            }
        }
    }
    if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x200, 0x5DC, ADDR_g_PlayerPosX) != 0) {
        if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_U16(g_CurrentEntity) = 8;
                if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) {
                    ENTITY_ACTION_U16(g_CurrentEntity) = 0xC;
                }
                if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
                    if (CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) != 0) {
                        g_CurrentEntity->action_behavior = 5U;
                        g_CurrentEntity->action_state = (u8) prev_state;
                        if (prev_behavior != 5) {
                            g_CurrentEntity->action_state = 0U;
                        }
                    } else if (CHIMERA_FIELD(g_CurrentEntity, player_touch) != 0) {
                        ENTITY_ACTION_U16(g_CurrentEntity) = 8;
                    }
                }
                CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
                return;
            }
        }
    }
    if ((((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) == 0) && (LIT(s32, sp_tmp00) < 0x1F40)) || (CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) >= 0x15)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 2;
        g_CurrentEntity->animation_id = 3;
        CHIMERA_FIELD(g_CurrentEntity, turn_step) = 0x20;
        if ((LIT(s32, sp_tmp00) >= 0xFA0) && (CHIMERA_FIELD(g_CurrentEntity, run_mode) == 0)) {

        } else {
            g_CurrentEntity->animation_id = 2;
            CHIMERA_FIELD(g_CurrentEntity, turn_step) = 0x40;
        }
        CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
        CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
    }
    if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
        if (((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) && (LIT(s32, sp_tmp00) >= 0x1771)) {
            if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x40) == 0) && (g_PlayerEntity.action_behavior == 0x13)) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_U16(g_CurrentEntity) = 0xB;
                CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
            }
        }
    }
    if (((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 0xD;
    }
}

void chimera_brain_c(void) {
    u16 prev_behavior;

    prev_behavior = (u16) g_CurrentEntity->action_behavior;
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
    if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) == 0) {
        g_CurrentEntity->sub_state = 0;
        g_CurrentEntity->action_behavior = 9U;
        if (prev_behavior != 9) {
            g_CurrentEntity->action_state = 0;
            if (LIT(s32, sp_tmp00) >= 0xBB9) {
                CHIMERA_FIELD(g_CurrentEntity, run_mode) = 1;
            }
        }
    }
    if (((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) == 0) && (CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) < 0x1F)) {

    } else {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 0xA;
        g_CurrentEntity->animation_id = 3;
        CHIMERA_FIELD(g_CurrentEntity, turn_step) = 0x20;
        if ((LIT(s32, sp_tmp00) >= 0xFA0) && (CHIMERA_FIELD(g_CurrentEntity, run_mode) == 0)) {

        } else {
            g_CurrentEntity->animation_id = 2;
            CHIMERA_FIELD(g_CurrentEntity, turn_step) = 0x40;
        }
        CHIMERA_FIELD(g_CurrentEntity, wall_stuck_frames) = 0;
        CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
    }
    if ((CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) == 0) && (LIT(s32, sp_tmp00) < 0x7D0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 3;
        if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) {
            ENTITY_ACTION_U16(g_CurrentEntity) = 6;
        }
        CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
    }
    if (LIT(s32, sp_tmp00) >= 0x3A99) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x200) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_U16(g_CurrentEntity) = 6;
            CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
        }
    }
    if (((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_U16(g_CurrentEntity) = 0xD;
    }
    g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x800);
}

void chimera_behavior_idle(void) {
    extern s32 (*chimera_idle_variant_tbl[])();
    chimera_idle_variant_tbl[g_CurrentEntity->behavior_flags]();
}

void chimera_idle_floor(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = (u8) g_CurrentEntity->behavior_flags;
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    }
}

void chimera_idle_ceiling(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->action_ticks_counter = (u16) ((((g_ApiCallPtr = api_rand)() & 0x3F) + 0x50) - ((s16) (((u16) CHIMERA_FIELD(g_CurrentEntity, path_flags)) & 1) * 0x50));
        g_CurrentEntity->blend_counter = 3;
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        return;
    }
}

void chimera_behavior_walk(void) {
    void chimera_walk_timeout();
    if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
        (g_ApiCallPtr = api_zone_path_find)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, &g_CurrentEntity->waypoint_x, &g_CurrentEntity->waypoint_z);
        LIT(s32, sp_vec_vx) = (s32) ((s16) g_CurrentEntity->waypoint_x);
        LIT(s32, sp_vec_vz) = (s32) ((s16) g_CurrentEntity->waypoint_z);
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x40);
    } else {
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((s16) (((u16) CHIMERA_FIELD(g_CurrentEntity, path_flags)) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10)));
    }
    chimera_walk_timeout();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
    LIT(s32, sp_tmp0C) = 0;
    if (!((g_CurrentEntity->animation_frame_id % 6) & 0xFF) && (((u8) g_CurrentEntity->animation_id) == 2)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(LIT(s32, sp_tmp0C), 0);
    }
    if (!((g_CurrentEntity->animation_frame_id % 10) & 0xFF) && (((u8) g_CurrentEntity->animation_id) == 3)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(LIT(s32, sp_tmp0C), 0);
    }
    if (!((g_CurrentEntity->animation_frame_id % 15) & 0xFF) && (((u8) g_CurrentEntity->animation_id) == 9)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(LIT(s32, sp_tmp0C), 0);
    }
}

void chimera_walk_timeout(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) * g_CurrentEntity->action_behavior);
        if (!(((u8) g_CurrentEntity->action_behavior >> 1) & 0xFF)) {
            g_CurrentEntity->action_ticks_counter = (u16) (g_CurrentEntity->action_ticks_counter + 0x50);
        }
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(((u8) g_PlayerEntity.hit_state), g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_U16(g_CurrentEntity) = 0;
        }
    }
}

void chimera_behavior_turn(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x1F) + 0x50);
        /* fallthrough */
    case 1:
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, CHIMERA_FIELD(g_CurrentEntity, turn_step));
                if (!((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) || (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C));
        return;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        if ((s16) (CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) != 0) {
            CHIMERA_FIELD(g_CurrentEntity, run_mode) = 1;
        }
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        return;
    }
}

void chimera_behavior_swipe(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 8;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0U);
        /* fallthrough */
    case 1:
        joints = g_CurrentEntity->joints;
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        SCRATCH_VEC->x = 0x64;
        LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX);
        LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joints, 10), ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX);
        if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            if ((LIT(s32, sp_tmp04) == 0) && (LIT(s32, sp_tmp08) == 0)) {

            } else if ((u32) ((g_CurrentEntity->animation_frame_id + 0xF9) & 0xFF) < 6U) {
                SCRATCH_VEC->x = 0;
                LIT(s32, sp_vec_vy) = -0x9D8;
                LIT(s32, sp_vec_vz) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0x200, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
                g_PlayerEntity.health -= 0x14;
                LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp08) + 1;
                g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp08) + 0x66;
                (g_ApiCallPtr = api_sound_play_se_3d)(3, ((u8) g_PlayerEntity.hit_state), 0, ADDR_g_PlayerPosX);
                (g_ApiCallPtr = api_sound_play_entity_se)(5, 0U);
                (g_ApiCallPtr = api_pad_vibrate_small)(8, 0U);
                (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xFAU, 4);
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                if (LIT(s32, sp_tmp04) != 0) {
                    SCRATCH_VEC->x = 0x64;
                    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
                    SCRATCH_VEC->x = 0x12C;
                    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
                    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 6), 0U, 0x40, 0x90);
                }
                if (LIT(s32, sp_tmp08) != 0) {
                    SCRATCH_VEC->x = 0x64;
                    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0, JOINT_N_WORLD(joints, 10), ADDR_sp_vec, 0);
                    SCRATCH_VEC->x = 0x12C;
                    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0, JOINT_N_WORLD(joints, 10), ADDR_sp_vec, 0);
                    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 10), 0x90U, 0x50, 0x10);
                }
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) = 0x3C;
    }
}

void chimera_behavior_grabhold(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_frame_id = 0xAU;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->angle = (u16) g_PlayerEntity.angle;
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        CHIMERA_FIELD(g_CurrentEntity, grab_struggle_timer) = 0x5A;
        g_CurrentEntity->animation_id = 5;
        g_CurrentEntity->anim_base_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->anim_base_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        g_PlayerEntity.anim_base_x = (*(u16 *)&g_PlayerEntity.pos_x);
        g_PlayerEntity.anim_base_z = (*(u16 *)&g_PlayerEntity.pos_z);
        g_CurrentEntity->hit_state = 1;
        g_PlayerEntity.hit_state = 1;
        (*(s16 *)&g_PlayerEntity.state) = 0x906;
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0;
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)0x14, 0xB4, 0);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_entity_apply_anim_vertex)(g_CurrentEntity, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base);
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        g_CurrentEntity->death_timer++;
        if (g_CurrentEntity->animation_frame_id == 0x22) {
            SCRATCH_VEC->x = 0x320;
            LIT(s32, sp_vec_vy) = -0x71C;
            LIT(s32, sp_vec_vz) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0x200, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_sound_play_se_3d)((void *)3, 1, 0, ADDR_g_PlayerPosX);
            (g_ApiCallPtr = api_sound_play_entity_se)((void *)6, 0);
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 6), 0xA0, 0x50, 0x10);
            g_PlayerEntity.health -= 0x1E;
            (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xA, 0);
            (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0xF, 0xFA, 0, 8);
        }
        CHIMERA_FIELD(g_CurrentEntity, grab_struggle_timer) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 3 : 0;
        g_PlayerEntity.action_ticks_counter = (u16) CHIMERA_FIELD(g_CurrentEntity, grab_struggle_timer);
        if (CHIMERA_FIELD(g_CurrentEntity, grab_struggle_timer) < 0) {
            CHIMERA_FIELD(g_CurrentEntity, grab_struggle_timer) = 0;
            g_PlayerEntity.action_ticks_counter = 0;
            return;
        }
        return;
    case 2:
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->behavior_flags = 1;
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 7;
        if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 5;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
            CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) = 0x5A;
        }
    }
}

void chimera_behavior_toggle(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        if (g_CurrentEntity->behavior_flags != 0) {
            g_CurrentEntity->action_state = 3U;
            return;
        }
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 4;
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, 0);
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(g_CurrentEntity->behavior_flags, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, 0);
        g_CurrentEntity->behavior_flags = (g_CurrentEntity->behavior_flags + 1) % 2;
        g_CurrentEntity->action_state = 3U;
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state = 4U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = 7;
        LIT(s32, sp_vec_vx) = 0xC8;
        LIT(s32, sp_vec_vy) = -0x654;
        LIT(s32, sp_vec_vz) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0U, 0x1F, 0, (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
        CHIMERA_FIELD(g_CurrentEntity, run_mode) = 0;
        /* fallthrough */
    case 4:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x400));
        return;
    case 5:
        g_CurrentEntity->action_state = 0U;
        if (CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) == 0) {
            ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        /* fallthrough */
    default:
    }
}

void chimera_behavior_drop(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->animation_id = 0x10;
        g_CurrentEntity->action_ticks_counter = 0;
        g_CurrentEntity->hit_state = 1;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = -chimera_arc_heights[0x20 - g_CurrentEntity->animation_frame_id];
        (g_ApiCallPtr = api_joint_move)(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->action_state = 2U;
        return;
    case 2:
        g_CurrentEntity->angle_z = 0;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->behavior_flags = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        /* fallthrough */
    case 3:
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20));
        if (g_CurrentEntity->animation_frame_id == 0x19) {
            if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x400, 0x7D0, ADDR_g_PlayerPosX) != 0) {
                if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                    if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) {
                        ENTITY_ACTION_S16(g_CurrentEntity) = 4;
                        g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
                        g_PlayerEntity.hit_state = 1;
                        g_CurrentEntity->hit_state = 0;
                        return;
                    }
                }
            }
        }
        g_CurrentEntity->pos_y = -chimera_arc_heights[0x20 - g_CurrentEntity->animation_frame_id];
        g_CurrentEntity->move_speed_current = (u16) *(s32 *)((u8 *)&chimera_swoop_speed + (((0x20 - g_CurrentEntity->animation_frame_id) * 2)));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if (g_CurrentEntity->animation_frame_id == 0x1F) {
            (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        }
        return;
    case 4:
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 3;
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->pos_y = 0;
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void chimera_behavior_swoop(void) {
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->animation_id = 0x10;
        g_CurrentEntity->action_ticks_counter = 0;
        g_CurrentEntity->hit_state = 1;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, NULL);
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) -*(s16 *)((u8 *)&chimera_arc_heights + ((g_CurrentEntity->animation_frame_id * 2)));
        g_CurrentEntity->move_speed_current = (u16) *(s32 *)((u8 *)&chimera_swoop_speed + ((g_CurrentEntity->animation_frame_id * 2)));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        (g_ApiCallPtr = api_joint_move)(0, ((s32 *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400);
        if (g_CurrentEntity->animation_frame_id == 0x20) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        g_CurrentEntity->pos_y = (s32) -*(s16 *)((u8 *)&chimera_arc_heights + ((g_CurrentEntity->animation_frame_id * 2)));
        g_CurrentEntity->angle_z = 0x800;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->behavior_flags = 2;
        (g_ApiCallPtr = api_sound_play_entity_se)(2, NULL);
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, ((s32 *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x400));
        return;
    case 4:
        g_CurrentEntity->hit_state = 0;
        CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) = 0x5A;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFD);
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        (g_ApiCallPtr = api_joint_move)(0, ((s32 *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x40);
        g_CurrentEntity->pos_y = -0x17E8;
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        /* fallthrough */
    default:
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void chimera_behavior_spit(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xE;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
        /* fallthrough */
    case 1:
        joints = g_CurrentEntity->joints;
        if ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x200, 0x7D0, ADDR_g_PlayerPosX) != 0) {
            if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0) && ((u32) ((g_CurrentEntity->animation_frame_id + 0xF6) & 0xFF) < 3U)) {
                SCRATCH_VEC->x = 0;
                LIT(s32, sp_vec_vy) = -0x9D8;
                LIT(s32, sp_vec_vz) = 0;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0x200, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
                g_PlayerEntity.health -= 0x14;
                LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp08) + 1;
                g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp08) + 0x66;
                (g_ApiCallPtr = api_sound_play_se_3d)(3, 1, 0, ADDR_g_PlayerPosX);
                (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 6), ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 6), 0x10, 0x20, 0x60);
                (g_ApiCallPtr = api_pad_vibrate_small)(8, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xFA, 4);
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
            CHIMERA_FIELD(g_CurrentEntity, attack_cooldown) = 0x28;
        }
        g_CurrentEntity->behavior_flags = 0;
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void chimera_behavior_flee(void) {

    g_CurrentEntity->animation_id = 3U;
    g_CurrentEntity->move_speed_current = 0x64;
    if (CHIMERA_FIELD(g_CurrentEntity, run_mode) != 0) {
        g_CurrentEntity->animation_id = 2U;
        g_CurrentEntity->move_speed_current = 0xC8;
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((CHIMERA_FIELD(g_CurrentEntity, path_flags) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x10)));
    g_CurrentEntity->angle = (u16) ((((s16) g_CurrentEntity->angle)) - 0x800);
    chimera_walk_timeout();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
    LIT(s32, sp_tmp0C) = 2;
    if ((g_CurrentEntity->animation_frame_id == 0) && (((u8) g_CurrentEntity->animation_id) == 2)) {
        (g_ApiCallPtr = api_sound_play_entity_se)(LIT(s32, sp_tmp0C), 0);
    }
}

void chimera_behavior_turn_back(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = 0x50U;
        /* fallthrough */
    case 1:
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
        LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, CHIMERA_FIELD(g_CurrentEntity, turn_step));
                if (!((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- != 0) || (LIT(s32, sp_tmp1C) == 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + (u16) LIT(s32, sp_tmp1C));
        g_CurrentEntity->angle = (u16) (ENTITY_AS(g_CurrentEntity, angle, s16) - 0x800);
        return;
    case 2:
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        g_CurrentEntity->waypoint_x = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
        g_CurrentEntity->waypoint_z = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
        return;
    }
}

void chimera_behavior_toggle_short(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 4;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, 0);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(g_CurrentEntity->behavior_flags, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, 0);
        g_CurrentEntity->behavior_flags = (g_CurrentEntity->behavior_flags + 1) % 2;
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void chimera_behavior_claw(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0xA;
        g_CurrentEntity->blend_counter = 3;
        (g_ApiCallPtr = api_sound_play_entity_se)(1, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + ((FIELD_AT((u8 *)g_CurrentEntity + 0x16C, ChimeraWork, ->path_flags, s16) & 1) * (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20)));
        if ((g_CurrentEntity->animation_frame_id == 0xA) && ((g_ApiCallPtr = api_entity_check_view_and_distance)(0x200, 0x7D0, ADDR_g_PlayerPosX) != 0) && ((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (((u8) g_PlayerEntity.hit_state) == 0) && ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) != 0) && !((g_CurrentEntity->collision_flags = g_CurrentEntity->collision_flags & 8) & 0xFF)) {
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
            g_CurrentEntity->angle = (g_ApiCallPtr = api_get_angle_towards_target)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z);
            g_PlayerEntity.hit_state = 1;
            g_CurrentEntity->hit_state = 1;
            return;
        } else if ((CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) && (CHIMERA_FIELD(g_CurrentEntity, player_touch) != 0) && (g_CurrentEntity->animation_frame_id == 0xA) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            g_PlayerEntity.health -= 0xA;
            LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
            g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp08) + 1;
            g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp08) + 0x66;
            (g_ApiCallPtr = api_sound_play_se_3d)(3, 0, 0, ADDR_g_PlayerPosX);
            (g_ApiCallPtr = api_pad_vibrate_small)(3, 0);
            (g_ApiCallPtr = api_pad_vibrate_large)(5, 0xB4, 0);
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 2U;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        if (g_CurrentEntity->animation_frame_id == 0x17) {
            (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        }
        return;
    case 2:
        g_CurrentEntity->behavior_flags = 1;
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void chimera_hit_fall(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->animation_id = (u8) *(&chimera_death_anim_tbl + g_CurrentEntity->behavior_flags);
        (g_ApiCallPtr = api_sound_play_entity_se)(8U, 0);
        /* fallthrough */
    case 1:
        (g_ApiCallPtr = api_joint_move)(((u8) g_PlayerEntity.hit_state), g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    }
}

void chimera_hit_stagger(void) {
    void chimera_acid_burst();
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->move_speed_current = 0xA0;
        (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        if ((g_CurrentEntity->health < 0x14) && !(((u8) g_CurrentEntity->hit_state) & 1 & 0xFF)) {
            chimera_acid_burst();
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if ((u8) ENTITY_AS(g_CurrentEntity, action_behavior, s16) < 2U) {
            if ((u8) g_CurrentEntity->animation_frame_id >= 0xAU) {
                g_CurrentEntity->move_speed_current = 0x3C;
            }
            (g_ApiCallPtr = api_entity_add_speed_xz)((1 - (u8) ENTITY_AS(g_CurrentEntity, action_behavior, s16)) * 0x800);
        }
        if (g_CurrentEntity->animation_frame_id == 0xA) {
            LIT(s32, sp_vec_vz) = 0;
            LIT(s32, sp_vec_vy) = 0;
            LIT(s32, sp_vec_vx) = 0;
            (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        }
        return;
    case 2:
        g_CurrentEntity->hit_state = 0U;
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        if ((g_ApiCallPtr = api_rand)() & 1) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 7;
            if (CHIMERA_FIELD(g_CurrentEntity, aggressive) != 0) {
                ENTITY_ACTION_S16(g_CurrentEntity) = 0xC;
            }
        }
        return;
    }
}

void chimera_hit_knockdown(void) {
    void chimera_acid_burst();
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->animation_id = 0x10;
        g_CurrentEntity->action_ticks_counter = 0;
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) -*(s16 *)((u8 *)&chimera_arc_heights + (((0x20 - g_CurrentEntity->animation_frame_id) * 2)));
        (g_ApiCallPtr = api_joint_move)((s32 *)1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->action_state = 2;
        return;
    case 2:
        g_CurrentEntity->pos_y += 0x3E8;
        g_CurrentEntity->angle_z = 0;
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->behavior_flags = 1;
        g_CurrentEntity->action_ticks_counter = ((s32) (0x82 - g_CurrentEntity->health) >> 2) + 0x14;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        if ((g_CurrentEntity->health < 0x14) && !((u8) g_CurrentEntity->hit_state & 1 & 0xFF)) {
            chimera_acid_burst();
        }
        /* fallthrough */
    case 3:
        g_CurrentEntity->pos_y += 0x64 + ((g_CurrentEntity->animation_frame_id) * 0x32);
        if (g_CurrentEntity->pos_y > 0) {
            g_CurrentEntity->pos_y = 0;
            if ((u8) g_CurrentEntity->hit_state >= 2U) {
                (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
                g_CurrentEntity->hit_state = 1;
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 4;
        }
        return;
    case 4:
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 5;
        }
        return;
    case 5:
        g_CurrentEntity->action_state = 6;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->action_ticks_counter = 0;
        /* fallthrough */
    case 6:
        g_CurrentEntity->animation_id = 0x11;
        if (g_CurrentEntity->animation_frame_id == 0x16) {
            g_CurrentEntity->angle += 0x800;
        }
        g_CurrentEntity->action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    case 7:
        g_CurrentEntity->hit_state = 0;
        ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        if ((g_ApiCallPtr = api_rand)() & 1) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 7;
        }
        /* fallthrough */
    default:
    }
}

void chimera_death_dissolve(void) {
    void chimera_acid_burst();
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        g_CurrentEntity->move_speed_current = 0xA0;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0U, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)8, 0U);
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        chimera_acid_burst();
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if ((u8) g_CurrentEntity->animation_frame_id >= 0xAU) {
            g_CurrentEntity->move_speed_current = 0x3C;
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)((void *) ((1 - g_CurrentEntity->action_behavior) * 0x800));
        if ((g_CurrentEntity->animation_frame_id == 0xF) && (g_CurrentEntity->action_behavior != 0)) {
            g_CurrentEntity->action_state = 2U;
        }
        return;
    case 2:
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
        CHIMERA_FIELD(g_CurrentEntity, shadow_locked) = 1;
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1U, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0U, 0);
        /* fallthrough */
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 0x16U, 0x16);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 4U;
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), g_CurrentEntity->death_event_id);
        }
        return;
    }
}

void chimera_death_drop_dissolve(void) {
    void chimera_acid_burst();
    s32 joints;

    joints = g_CurrentEntity->joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0;
        g_CurrentEntity->animation_id = 0x10;
        g_CurrentEntity->action_ticks_counter = 0x3CU;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)8, 0);
        chimera_acid_burst();
        /* fallthrough */
    case 1:
        g_CurrentEntity->pos_y = (s32) -chimera_arc_heights[0x20 - g_CurrentEntity->animation_frame_id];
        (g_ApiCallPtr = api_joint_move)((void *)1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        g_CurrentEntity->action_state = 2U;
        return;
    case 2:
        g_CurrentEntity->pos_y = (s32) (g_CurrentEntity->pos_y + 0x3E8);
        ENTITY_AS(g_CurrentEntity, angle_z, s16) = 0;
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->blend_counter = 0;
        ENTITY_AS(g_CurrentEntity, behavior_flags, s8) = 1;
        LIT(s32, sp_vec_vz) = 0;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vx) = 0;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
        /* fallthrough */
    case 3:
        g_CurrentEntity->pos_y += 0x64 + (g_CurrentEntity->animation_frame_id * 0x32);
        if (g_CurrentEntity->pos_y > 0) {
            g_CurrentEntity->pos_y = 0;
            if ((u8) ENTITY_AS(g_CurrentEntity, hit_state, u8) >= 2U) {
                (g_ApiCallPtr = api_sound_play_entity_se)((void *)4, 0);
                ENTITY_AS(g_CurrentEntity, hit_state, u8) = 1U;
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
            g_CurrentEntity->action_state = 4U;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 2);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 8);
            CHIMERA_FIELD(g_CurrentEntity, shadow_locked) = 1;
            (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, 0xFFFF50);
            (g_ApiCallPtr = api_billboard_set_size)(&g_CurrentEntity->shadow, 0, 0);
        }
        return;
    case 4:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 0x18, 0x18);
                if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 5U;
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        }
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void player_anim_chimera_knockdown(void) {

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0xC8;
        (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)8, 0xFA, 0x1B);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x28, 0xC8, 0, 0x2A);
        /* fallthrough */
    case 1:
        if ((u8) g_PlayerEntity.animation_frame_id < 0x24U) {
            (g_ApiCallPtr = api_entity_apply_anim_vertex)(LITP(s32, g_PlayerEntity), g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base);
        } else {
            (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            g_PlayerEntity.action_state = 2;
        }
        return;
    case 2:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.action_state = 3;
        g_PlayerEntity.animation_id = 3;
        g_PlayerEntity.blend_counter = 3;
        (*(u8 *)&g_PlayerEntity.hit_state) = 2;
        /* fallthrough */
    case 3:
        if (((u8) (*(u8 *)&g_PlayerEntity.hit_state) >= 2U) && ((u8) g_PlayerEntity.animation_frame_id >= 4U)) {
            (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)3, 2, 0, ADDR_g_PlayerPosX);
            (g_ApiCallPtr = api_sound_play_footstep)((s32 *)2, 0);
            (*(u8 *)&g_PlayerEntity.hit_state) = 1;
        }
        if ((g_PlayerEntity.health < 0) && ((u8) g_PlayerEntity.animation_frame_id >= 0xBU)) {
            (g_ApiCallPtr = api_sound_play_footstep)((s32 *)2, 0);
            g_PlayerEntity.action_state = 8;
            return;
        }
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 3 : 0;
        if ((*(s16 *)&g_PlayerEntity.action_ticks_counter) < 0) {
            if ((g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
                g_PlayerEntity.action_state = 4;
                return;
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            g_PlayerEntity.action_state = 4;
        }
        return;
    case 4:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.action_state = 5;
        g_PlayerEntity.animation_id = 5;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 5:
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) -= ((g_ApiCallPtr = api_get_player_input_masked)() != 0) ? 3 : 0;
        if ((*(s16 *)&g_PlayerEntity.action_ticks_counter) < 0) {
            if ((g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
                g_PlayerEntity.action_state = 6;
                return;
            }
        }
        if ((g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400) != 0) {
            g_PlayerEntity.action_state = 6;
        }
        return;
    case 6:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.action_state = 7;
        g_PlayerEntity.animation_id = 4;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 7:
        if ((g_ApiCallPtr = api_joint_move)((s32 *)1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400) != 0) {
            if (g_PlayerEntity.health < 0) {
                g_PlayerEntity.action_state = 8;
                return;
            }
            (*(s16 *)&g_PlayerEntity.state) = 1;
            ENTITY_ACTION_S16(&g_PlayerEntity) = 0;
            (*(u8 *)&g_PlayerEntity.hit_state) = 0;
        }
        break;
    case 8:
        (g_ApiCallPtr = api_sound_play_se_3d)((s32 *)3, 3, 0, ADDR_g_PlayerPosX);
        g_PlayerEntity.action_state = 9;
        (*(s16 *)&g_PlayerEntity.action_ticks_counter) = 0x5A;
        (g_ApiCallPtr = api_billboard_set_color)(LITP(s32, g_PlayerShadowOffsetX), 1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_set_size)(LITP(s32, g_PlayerShadowOffsetX), 0, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0xA, 0x64, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)0x5A, 0x64, 0, 0xA);
        /* fallthrough */
    case 9:
        (g_ApiCallPtr = api_billboard_adj_size)(LITP(s32, g_PlayerShadowOffsetX), 0x14, 0x14);
                if ((u16) (*(s16 *)&g_PlayerEntity.action_ticks_counter)-- == 0) {
            g_PlayerEntity.action_state = 0xA;
        }
        break;
    }
}

void chimera_acid_burst(void) {
    u8 i;
    s32 joints;

    joints = g_CurrentEntity->joints;
    LIT(s32, sp_vec_vz) = 0;
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vx) = 0;
    (g_ApiCallPtr = api_effect_create_billboard)(0, 0U, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
    (g_ApiCallPtr = api_effect_create_billboard)(3, 0U, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
    LIT(s32, sp_vec_vx) = 0x64;
    LIT(s32, sp_vec_vy) = -0x5DC;
    LIT(s32, sp_vec_vz) = 0;
    i = 5;
    while (i--) {
        (g_ApiCallPtr = api_effect_create_billboard)(0x1D, i, ((s16) g_PlayerEntity.angle) - ((s16) g_CurrentEntity->angle) + 0x600 + ((i - 1) * 0x100), (u8 *)g_CurrentEntity + 0x20, ADDR_sp_vec, 0);
    }
}
