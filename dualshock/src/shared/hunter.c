/* hunter.c - Hunter: AI variants, attacks (swipe, leap, decapitation), dodge, death, script (SCD) states and
   the player's decapitation animation.
   Shared by the stage overlays that have this enemy: each compiles this file through its
   src/stageN/hunter.c wrapper, after that stage's globals.h (tools/sharetwins.py). */

/* hunter data (defined in each overlay that has this enemy) */
extern s32 hunter_sca_info_ptr;
extern s32 hunter_sca_info_ptr_late;
extern u8 hunter_health_tbl;
extern s32 hunter_decap_chance_mid;
extern s32 hunter_decap_chance_low;
extern u8 g_HunterPlayerHealthGate[2];   /* [player id & 1] (Chris, Jill): see decap_chance */
extern s8 hunter_behavior_seed_tbl;
extern s8 hunter_intro_anim_tbl;
extern u16 hunter_intro_angle_tbl[];
extern u8 g_HunterIntroJumpKind;
extern void *g_HunterScdVictim;

/* HunterWork: the hunter's own use of entity bytes 0x16C.. (tools/enemywork.py,
   config/enemy_work). HUNTER(e)->f, HUNTER_FIELD(e, f) and HUNTER_MEMBER(e, f) reach the same
   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */
typedef struct HunterWork {
    /* +0x00 */ u8 pad00[2];
    /* +0x02 */ s16 path_flags;        /* 0x16E: bit 0 = api_entity_pathfind_update result (hunter_behavior_update); non-zero sets decap_cooldown and lets the variant AIs close in */
    /* +0x04 */ s16 airborne;          /* 0x170: 1 while jumping (dodge_start, ledgejump); the hit dispatch lands it when pos_y >= -0x1F3, cleared on landing */
    /* +0x06 */ u8 pad06[2];
    /* +0x08 */ void *npc_target;      /* 0x174: the NPC a variant 0/1 hunter goes for: enemy slot 0 (0x800C52A4), slot 1 if the hunter is slot 0; no active NPC there -> normal hunter; her health < 0 -> hunts the player. The "save Rebecca in time" scene: STAGE7/ROOM7060 (Chris) spawns Rebecca (model 0x2C) in slot 0 + variant-0 hunters */
    /* +0x0C */ s16 leap_target_x;     /* 0x178: X the decapitation leap aims at (pos_x + rotated offset to the player); also used by the scripted (SCD) leap */
    /* +0x0E */ s16 leap_target_z;     /* 0x17A: Z the decapitation leap aims at */
    /* +0x10 */ s16 decap_claw_joint;  /* 0x17C: which claw strikes: 6 or 9 (side picked by rand): anim id = 0x12 - n, joint index n*0x7C, yaw tweak (n-6)*0x14; cleared when idle */
    /* +0x12 */ u16 dir_control_flags; /* 0x17E: copy of the room-collision direction word (0x1F800018) each frame; first arg of api_entity_update_wander_turn */
    /* +0x14 */ u8 room_hit;           /* 0x180: OR of api_check_room_collision results (hunter_update); variant 0 checks it; address given to the wander turn */
    /* +0x15 */ u8 pad15[1];
    /* +0x16 */ s8 decap_chance;       /* 0x182: rolled from hunter_decap_chance_low/mid by health; non-zero allows the decapitation leap; only rolled while the player's HP < g_HunterPlayerHealthGate (Chris 98/140, Jill 67/96; Advanced 75/52), else 0. Decapitation = instant death (confirmed in game) */
    /* +0x17 */ s8 hit_window_timer;   /* 0x183: 50 when a hit reaction starts (hunter_behavior_dispatch), counted down in hunter_update; at <0 clears hit_window_count */
    /* +0x18 */ u8 hit_window_count;   /* 0x184: hit reactions inside the hit window; ==2 at the end of the swipe triggers the counter move (hunter_act_swipe) */
    /* +0x19 */ s8 backstep_dir;       /* 0x185: 1..3 = turn_toward_target(.,1)+2; angle += (n-2)<<6, speed n*0x400 (hunter_behavior_backstep) */
    /* +0x1A */ s8 wander_turn_state;  /* 0x186: cleared in init; passed by address to api_entity_update_wander_turn */
    /* +0x1B */ s8 approach_frames;    /* 0x187: frames in hunter_behavior_approach; 90 ends the approach */
    /* +0x1C */ s8 unused_188;         /* 0x188: only cleared (hunter_state_init, hunter_variant_2_ai) */
    /* +0x1D */ s8 decap_cooldown;     /* 0x189: 60 when path_flags is set, counted down in hunter_state_run; non-zero blocks the decapitation leap */
    /* +0x1E */ u8 leap_followup;      /* 0x18A: bit 0 set at random on the leap attack; at its end bit 0 chains into state 0x60101 and is cleared */
} HunterWork;
#define HUNTER(e) ENEMY_WORK(e, HunterWork)
#define HUNTER_AS(e, f, T) ENEMY_WORK_AS(e, HunterWork, f, T)
#define HUNTER_FIELD(e, f) ENEMY_FIELD(e, HunterWork, f)
#define HUNTER_MEMBER(e, f) ENEMY_MEMBER(e, HunterWork, f)

/* state 0: sets up a new hunter (health, sca info, animation, work fields), then state 1 */
void hunter_state_init(void) {
    if (g_StatusFlags & STATUS_ADVANCED) {
        g_HunterPlayerHealthGate[0] = 0x4B;
        g_HunterPlayerHealthGate[1] = 0x34;
    }
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 0, 0);
    g_CurrentEntity->sca_matrix_head = 0;
    g_CurrentEntity->unused_0C1 = 0;
    g_CurrentEntity->action_ticks_counter = 0;
    g_CurrentEntity->animation_id = 0x15;
    g_CurrentEntity->hit_state = 0;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 0;
    (g_ApiCallPtr = api_reset_joint_transforms)();
    (g_ApiCallPtr = api_joint_move)(0, ((void *) g_CurrentEntity->anim_header), g_CurrentEntity->anim_base, 0x200);
    LIT(s32, sp_tmp1C) = 0x808080;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    (g_ApiCallPtr = api_billboard_init)(ADDR_sp_svec, &g_CurrentEntity->shadow, 0x3E8, 0x3E8);
    g_CurrentEntity->health = (s16) *(&hunter_health_tbl + ((g_ApiCallPtr = api_rand)() & 0xF));
    g_CurrentEntity->sca_info = (s32) hunter_sca_info_ptr;
    if (g_CurrentEntity->behavior_flags & 0x20 & 0xFF) {
        g_CurrentEntity->sca_info = (s32) hunter_sca_info_ptr_late;
    }
    if ((u8) g_StageId >= 5U) {
        g_CurrentEntity->sca_info = (s32) hunter_sca_info_ptr_late;
    }
    if ((u32) (g_CurrentEntity->behavior_flags & 0xF & 0xFF) < 2U) {
        HUNTER(g_CurrentEntity)->npc_target = (void *)ADDR_g_EnemiesList;
        if (g_CurrentEntity == HUNTER(g_CurrentEntity)->npc_target) {
            HUNTER(g_CurrentEntity)->npc_target = (void *)ADDR_g_EnemySlot1;
        }
        if (!(*((u8 *) HUNTER(g_CurrentEntity)->npc_target) & 1 & 0xFF)) {
            g_CurrentEntity->behavior_flags = 2U;
        }
    }
    if (!(g_CurrentEntity->behavior_flags & 0xF & 0xFF)) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags | 2);
    }
    HUNTER(g_CurrentEntity)->path_flags = 0;
    HUNTER(g_CurrentEntity)->airborne = 0;
    HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
    HUNTER(g_CurrentEntity)->decap_chance = 0;
    HUNTER(g_CurrentEntity)->unused_188 = 0;
    HUNTER(g_CurrentEntity)->decap_cooldown = 0;
    HUNTER(g_CurrentEntity)->wander_turn_state = 0;
    g_CurrentEntity->look_at_joint_idx = 2;
    HUNTER(g_CurrentEntity)->hit_window_timer = 0;
    HUNTER(g_CurrentEntity)->hit_window_count = 0;
    if (g_CurrentEntity->behavior_flags & 8 & 0xFF) {
        g_CurrentEntity->sub_state = 1;
        g_CurrentEntity->action_behavior = 0xB;
    }
}

/* every frame for each hunter (enemies_update_functions_tbl): then collision checks and
   the shadow / drawing */
void hunter_update(void) {
    extern s32 (*hunter_state_tbl[])();
    u16 unused;

    if ((g_CurrentEntity->state != 0) && (g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        g_CurrentEntity->state = 8U;
    }
    if (--HUNTER(g_CurrentEntity)->hit_window_timer < 0) {
        HUNTER(g_CurrentEntity)->hit_window_timer = 0;
        HUNTER(g_CurrentEntity)->hit_window_count = 0;
    }
    if (g_UpdateFlags & 4 & 0xFFFF) {
        hunter_state_tbl[g_CurrentEntity->state]();
        if (g_CurrentEntity->state != 5) {
            (g_ApiCallPtr = api_entity_rotate_collision_points)(g_CurrentEntity);
            (g_ApiCallPtr = api_check_entity_collision)(LITP(void, g_PlayerEntity), g_CurrentEntity);
            (g_ApiCallPtr = api_entity_resolve_enemy_player_collisions)();
            HUNTER(g_CurrentEntity)->room_hit = (s8) (HUNTER(g_CurrentEntity)->room_hit | (g_ApiCallPtr = api_check_room_collision)(&g_CurrentEntity->pos_x, SCA(((void *) g_CurrentEntity->sca_info))->radius));
            HUNTER(g_CurrentEntity)->dir_control_flags = (u16) LIT(u16, sp_tmp18);
        }
    }
    g_CurrentEntity->has_enter_switch_zone = (g_ApiCallPtr = api_is_point_in_quad)(&g_CurrentEntity->pos_x, g_CameraSwitchZones);
    if (g_CurrentEntity->has_enter_switch_zone != 0) {
        (g_ApiCallPtr = api_billboard_draw)(&g_CurrentEntity->pos_x, &g_CurrentEntity->shadow, 0U, ((s16) g_CurrentEntity->angle));
    }
}

void hunter_state_run(void) {
    void hunter_behavior_update();
    void hunter_variant_dispatch();
    if (g_CurrentEntity->behavior_flags & 0x80 & 0xFF) {
        return;
    }
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
    (g_ApiCallPtr = api_entity_check_visual_range)(0xFA0);
    LIT(s32, sp_tmp00) = g_PlayerEntity.pos_y - g_CurrentEntity->pos_y;
    if (LIT(s32, sp_tmp00) != 0) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        if (LIT(s32, sp_tmp00) < 0) {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        } else {
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x80);
        }
    }
    if (HUNTER(g_CurrentEntity)->decap_cooldown != 0) {
        HUNTER(g_CurrentEntity)->decap_cooldown--;
    }
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        hunter_variant_dispatch();
    }
    hunter_behavior_update();
}

void hunter_behavior_dispatch(void) {
    extern u8 hunter_behavior_seed_tbl_u8[] __asm__("hunter_behavior_seed_tbl");
    extern s32 (*hunter_act_tbl[])();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        HUNTER(g_CurrentEntity)->hit_window_count++;
        if (HUNTER(g_CurrentEntity)->hit_window_timer == 0) {
            HUNTER(g_CurrentEntity)->hit_window_timer = 0x32;
        }
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(2, 1, 0, 0);
        g_CurrentEntity->action_behavior = hunter_behavior_seed_tbl_u8[((u8) g_CurrentEntity->hit_state) & 7];
        g_CurrentEntity->action_behavior += (API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) >> 10) & 1;
        if (HUNTER(g_CurrentEntity)->airborne != 0) {
            LIT(s32, sp_tmp1C) = (s32) g_CurrentEntity->action_behavior;
            g_CurrentEntity->action_behavior = 4U;
            if (g_CurrentEntity->pos_y >= -0x1F3) {
                g_CurrentEntity->pos_y = 0;
                HUNTER(g_CurrentEntity)->airborne = 0;
                g_CurrentEntity->action_behavior = (u8) LIT(s32, sp_tmp1C);
            }
        }
        if ((g_CurrentEntity->status_flags & 0xE0 & 0xFF) == 0x20) {
            g_CurrentEntity->action_behavior = 5U;
        }
        if (g_CurrentEntity->behavior_flags & 8 & 0xFF) {
            g_CurrentEntity->action_behavior = 8U;
        }
    }
    hunter_act_tbl[g_CurrentEntity->action_behavior]();
}

void hunter_death_dispatch(void) {
    extern s32 (*hunter_death_tbl[])();
    if (((u8) g_CurrentEntity->sub_state) == 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(3, 1, 0, 0);
        g_CurrentEntity->action_behavior = (u8) *(&hunter_behavior_seed_tbl + (((u8) g_CurrentEntity->hit_state) & 7 & 0xFF));
        if ((((u8) g_CurrentEntity->hit_state) & 3 & 0xFF) != 2) {
            g_CurrentEntity->action_behavior = (u8) (g_CurrentEntity->action_behavior + ((API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400U) >> 0xA) & 1));
        }
        if (HUNTER(g_CurrentEntity)->airborne != 0) {
            LIT(s32, sp_tmp1C) = (s32) g_CurrentEntity->action_behavior;
            g_CurrentEntity->action_behavior = 4U;
            if (g_CurrentEntity->pos_y >= -0x1F3) {
                g_CurrentEntity->pos_y = 0;
                HUNTER(g_CurrentEntity)->airborne = 0;
                g_CurrentEntity->action_behavior = (u8) LIT(s32, sp_tmp1C);
            }
        }
        if ((g_CurrentEntity->status_flags & 0xE0 & 0xFF) == 0x20) {
            g_CurrentEntity->action_behavior = 6U;
        }
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_EnemiesFlags, g_CurrentEntity->death_event_id);
    }
    hunter_death_tbl[g_CurrentEntity->action_behavior]();
}

void hunter_death_fall(void) {
    void hunter_death_fall_driver();
    void hunter_recenter_on_joint(u8 side);
    g_CurrentEntity->animation_id = 0x11;
    g_CurrentEntity->move_speed_current = 0;
    hunter_death_fall_driver();
    if ((g_CurrentEntity->animation_frame_id != 0x32) && (g_CurrentEntity->animation_frame_id != 0x24)) {

    } else {
        (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
    }
    if (g_CurrentEntity->animation_frame_id == 0x4B) {
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
    }
    if (((u8) g_CurrentEntity->blend_counter) == 0) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 0x18U) {
            hunter_recenter_on_joint(1);
            return;
        }
        hunter_recenter_on_joint(0);
    }
}

void hunter_death_thrash(void) {
    void hunter_death_fall_driver();
    g_CurrentEntity->animation_id = 7;
    g_CurrentEntity->move_speed_current = 0x46;
    if ((u8) g_CurrentEntity->animation_frame_id < 3U) {
        g_CurrentEntity->move_speed_current = 0xFA;
    }
    hunter_death_fall_driver();
    if ((g_CurrentEntity->animation_frame_id != 0xD) && (g_CurrentEntity->animation_frame_id != 0x17)) {
        return;
    }
    (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
}

void hunter_death_settle(void) {
    void hunter_death_fall_driver();
    void hunter_recenter_on_joint(u8 side);
    g_CurrentEntity->animation_id = 0xF;
    g_CurrentEntity->move_speed_current = 0;
    hunter_death_fall_driver();
    hunter_recenter_on_joint(0);
}

void hunter_death_collapse(void) {
    void hunter_death_fall_driver();
    g_CurrentEntity->animation_id = 6;
    g_CurrentEntity->move_speed_current = 0;
    hunter_death_fall_driver();
}

void hunter_state4_nop(void) {

}

void hunter_behavior_update(void) {
    extern s32 (*hunter_behavior_tbl[])();
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_entity_pathfind_update)();
    if (HUNTER(g_CurrentEntity)->path_flags == 0) {
        if (((s32) LIT(s32, sp_tmp1C) >> 1) == 0) {
            HUNTER(g_CurrentEntity)->path_flags = (s16) (HUNTER(g_CurrentEntity)->path_flags & ~1);
            HUNTER(g_CurrentEntity)->path_flags = (s16) (((u16) HUNTER(g_CurrentEntity)->path_flags) | ((u16) LIT(s32, sp_tmp1C) & 1));
        }
        if (HUNTER(g_CurrentEntity)->path_flags != 0) {
            HUNTER(g_CurrentEntity)->decap_cooldown = 0x3C;
        }
    }
    (g_ApiCallPtr = api_zone_path_find)(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, &g_CurrentEntity->waypoint_x, &g_CurrentEntity->waypoint_z);
    hunter_behavior_tbl[g_CurrentEntity->action_behavior]();
}

void hunter_variant_dispatch(void) {
    void hunter_coward_ai();
    extern s32 (*hunter_variant_tbl[])();

    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    if (g_CurrentEntity->behavior_flags & 0x10 & 0xFF) {
        hunter_coward_ai();
        return;
    }
    hunter_variant_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
}

void hunter_variant_0_ai(void) {
    u16 dist_off;
    u16 player_hp;
    void *npc;

    if (g_CurrentEntity->behavior_flags == 0) {
        g_CurrentEntity->behavior_flags = 2U;
        return;
    }
    npc = HUNTER(g_CurrentEntity)->npc_target;
    if (ENT(npc)->health < 0) {
        g_CurrentEntity->behavior_flags = 2U;
    }
    LIT(s32, sp_tmp04) = __builtin_abs((ENT(npc)->pos_x - g_CurrentEntity->pos_x)) + __builtin_abs((ENT(npc)->pos_z - g_CurrentEntity->pos_z));
    LIT(s32, sp_tmp08) = (s32) g_CurrentEntity->action_behavior;
    if (((u8) ENT(npc)->hit_state) != 0) {
        if (LIT(s32, sp_tmp08) != 2) {
            ENTITY_AS(g_CurrentEntity, action_state, s8) = 0;
            g_CurrentEntity->blend_counter = 7;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if ((HUNTER(g_CurrentEntity)->path_flags != 0) && (LIT(s32, sp_tmp00) < 0xFA0)) {
        if (LIT(s32, sp_tmp08) != 2) {
            ENTITY_AS(g_CurrentEntity, action_state, s8) = 0;
            g_CurrentEntity->blend_counter = 7;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if ((((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) && (LIT(s32, sp_tmp00) < 0xBB8)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 9;
    }
    if (((u8) g_PlayerEntity.hit_state) != 0) {
        return;
    }
    if (((g_ApiCallPtr = api_check_line_of_sight)(LITP(void, g_PlayerPosX)) == 0) && (LIT(s32, sp_tmp00) < 0xBB8)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x80) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
        }
    }
    if ((HUNTER(g_CurrentEntity)->path_flags != 0) && ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) != 7)) {
        dist_off = LIT(s16, sp_tmp00) - 5000;
        if (LIT(s32, sp_tmp00) > LIT(s32, sp_tmp04)) {
            if (((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x20) == 0) && (LIT(s32, sp_tmp04) < 0xFA0)) {
                if (((g_ApiCallPtr = api_turn_toward_target)(&ENT(npc)->pos_x, 0x200) == 0) && (ENT(npc)->move_speed_current < 0x32)) {
                    g_CurrentEntity->sub_state = 1;
                    ENTITY_ACTION_S16(g_CurrentEntity) = 6;
                    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
                }
            }
        }
        if (((g_ApiCallPtr = api_turn_toward_target)(&ENT(npc)->pos_x, 0x200) == 0) && (HUNTER(g_CurrentEntity)->room_hit != 0) && (LIT(s32, sp_tmp04) < 0xBB8)) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 6;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        }
    }
    LIT(s32, sp_tmp00) = (s32) (__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + __builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
    dist_off = LIT(s16, sp_tmp00) - 3000;
    player_hp = ((u16) g_PlayerEntity.health);
    if (LIT(s32, sp_tmp00) < 0x7D0) {
        if (((g_ApiCallPtr = api_turn_toward_target)(LITP(void, g_PlayerPosX), 0x100) == 0) && (((u8) HUNTER(g_CurrentEntity)->decap_chance) != 0)) {
            LIT(u16, sp_svec_vx) = (u16) g_PlayerEntity.pos_x - (u16) g_CurrentEntity->pos_x;
            LIT(s16, sp_svec_vy) = 0;
            LIT(u16, sp_svec_vz) = (u16) g_PlayerEntity.pos_z - (u16) g_CurrentEntity->pos_z;
            LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
            LIT(s32, sp_tmp00) = -0x96;
            LIT(s32, sp_tmp00) = (s32) (LIT(s32, sp_tmp00) + ((g_RandSeed & 1 & 0xFFFF) * 0x12C));
            (g_ApiCallPtr = api_RotMatrixY)((void *) LIT(s32, sp_tmp00), ADDR_sp_matrix);
            (g_ApiCallPtr = api_ApplyMatrixSV)(LITP(void, sp_matrix), ADDR_sp_svec, ADDR_sp_svec);
            HUNTER(g_CurrentEntity)->decap_claw_joint = (s16) (((LIT(s32, sp_tmp00) + 0x96) / 100) + 6);
            HUNTER(g_CurrentEntity)->leap_target_x = (s16) ((u16) g_CurrentEntity->pos_x + LIT(s32, sp_svec_vx));
            HUNTER(g_CurrentEntity)->leap_target_z = (s16) ((u16) g_CurrentEntity->pos_z + LIT(s32, sp_svec_vz));
            (g_ApiCallPtr = api_entity_add_speed_xz)(NULL);
            ENTITY_AS(g_CurrentEntity, angle, s16) += 0x20 - (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) - 6) * 0x14;
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 5;
            return;
        }
    }
}

void hunter_variant_1_ai(void) {
    void *npc;

    npc = HUNTER(g_CurrentEntity)->npc_target;
    LIT(s32, sp_tmp04) = __builtin_abs(ENT(npc)->pos_x - g_CurrentEntity->pos_x) + __builtin_abs(ENT(npc)->pos_z - g_CurrentEntity->pos_z);
    if (g_CurrentEntity->action_behavior != 8) {
        g_CurrentEntity->action_state = 0;
        g_CurrentEntity->blend_counter = 7;
    }
    g_CurrentEntity->action_behavior = 8U;
    g_CurrentEntity->sub_state = 0;
}

void hunter_variant_2_ai(void) {
    u16 player_hp;

    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    LIT(s32, sp_tmp08) = (s32) g_CurrentEntity->action_behavior;
    player_hp = ((u16) g_PlayerEntity.health);
    HUNTER(g_CurrentEntity)->decap_chance = 0;
    if ((player_hp < (u32) (g_HunterPlayerHealthGate[g_PlayerEntity.id & 1 & 0xFF] & 0xFFFF)) && ((s16) ((u16) g_PlayerEntity.health) > 0)) {
        HUNTER(g_CurrentEntity)->unused_188 = 0;
        if (g_CurrentEntity->health < 0x28) {
            HUNTER(g_CurrentEntity)->decap_chance = (u8) *(u8 *)((u8 *)&hunter_decap_chance_low + (((g_ApiCallPtr = api_rand)() & 0xF)));
        }
        if (g_CurrentEntity->health < 0x4F) {
            HUNTER(g_CurrentEntity)->decap_chance = (u8) *(u8 *)((u8 *)&hunter_decap_chance_mid + (((g_ApiCallPtr = api_rand)() & 0xF)));
        }
        if ((s16) ((u16) g_PlayerEntity.health) >= 0x10) {
            if ((LIT(s32, sp_tmp00) >= 0x157C) && (((u8) g_PlayerEntity.hit_state) == 0)) {

            } else {
                HUNTER(g_CurrentEntity)->decap_chance = 0;
            }
        }
    }
    if (HUNTER(g_CurrentEntity)->decap_cooldown != 0) {
        HUNTER(g_CurrentEntity)->decap_chance = 0;
    }
    if (((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x2C8) == 0) && (LIT(s32, sp_tmp00) < 0x1900) && (((u8) HUNTER(g_CurrentEntity)->decap_chance) != 0)) {
        LIT(u16, sp_svec_vx) = (u16) g_PlayerEntity.pos_x - (u16) g_CurrentEntity->pos_x;
        LIT(s16, sp_svec_vy) = 0;
        LIT(u16, sp_svec_vz) = (u16) g_PlayerEntity.pos_z - (u16) g_CurrentEntity->pos_z;
        LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
        LIT(s32, sp_tmp00) = -0x96;
        LIT(s32, sp_tmp00) = (s32) (LIT(s32, sp_tmp00) + (((g_ApiCallPtr = api_rand)() & 1) * 0x12C));
        (g_ApiCallPtr = api_RotMatrixY)(LIT(s32, sp_tmp00), ADDR_sp_matrix);
        (g_ApiCallPtr = api_ApplyMatrixSV)(ADDR_sp_matrix, ADDR_sp_svec, ADDR_sp_svec);
        HUNTER(g_CurrentEntity)->decap_claw_joint = (s16) (((LIT(s32, sp_tmp00) + 0x96) / 100) + 6);
        HUNTER(g_CurrentEntity)->leap_target_x = (s16) ((u16) g_CurrentEntity->pos_x + LIT(s32, sp_svec_vx));
        HUNTER(g_CurrentEntity)->leap_target_z = (s16) ((u16) g_CurrentEntity->pos_z + LIT(s32, sp_svec_vz));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        g_CurrentEntity->angle = (s16) ((g_CurrentEntity->angle + 0x20) - ((((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) - 6) * 0x14));
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 5;
        return;
    }
    if ((HUNTER(g_CurrentEntity)->path_flags != 0) && (LIT(s32, sp_tmp00) < 0x1770)) {
        if (LIT(s32, sp_tmp08) != 2) {
            g_CurrentEntity->action_state = 0;
            g_CurrentEntity->blend_counter = 7;
        }
        g_CurrentEntity->action_behavior = 2U;
        g_CurrentEntity->sub_state = 0;
    }
    if ((((u8) g_PlayerEntity.hit_state) & 0x80 & 0xFF) && (LIT(s32, sp_tmp00) < 0xBB8)) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 9;
    }
    if (((u8) g_PlayerEntity.hit_state) != 0) {
        return;
    }
    if (((g_ApiCallPtr = api_check_line_of_sight)(ADDR_g_PlayerPosX) == 0) && (LIT(s32, sp_tmp00) < 0xBB8)) {
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80) == 0) {
            g_CurrentEntity->sub_state = 1;
            ENTITY_ACTION_S16(g_CurrentEntity) = 4;
            return;
        }
    }
    if ((HUNTER(g_CurrentEntity)->path_flags != 0) && ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) != 7) && (LIT(s32, sp_tmp00) >= 0x1901) && ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20) == 0)) {
        if ((g_PlayerEntity.action_behavior == 0x12) || ((g_PlayerEntity.action_behavior == 0x13) && (g_RandSeed & 1 & 0xFFFF))) {
            if ((g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity) == 0) {
                g_CurrentEntity->sub_state = 1;
                ENTITY_ACTION_S16(g_CurrentEntity) = 6;
                g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            }
        }
    }
}

void hunter_variant_4_ai(void) {
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if (LIT(s32, sp_tmp00) < 0xFA0) {
        g_CurrentEntity->sub_state = 1;
        ENTITY_ACTION_S16(g_CurrentEntity) = 8;
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags + 0xFC);
    }
}

void hunter_coward_ai(void) {
    if ((HUNTER(g_CurrentEntity)->path_flags != 0) && (LIT(s32, sp_tmp00) < 0x2328)) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xEF);
    }
    if (LIT(s32, sp_tmp00) < 0xFA0) {
        g_CurrentEntity->behavior_flags = (u8) (g_CurrentEntity->behavior_flags & 0xEF);
    }
}

void hunter_behavior_idle_dispatch(void) {
    extern s32 (*hunter_idle_variant_tbl[])();
    hunter_idle_variant_tbl[(g_CurrentEntity->behavior_flags & 0xF & 0xFF)]();
}

void hunter_idle_stand(void) {

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 0x15;
        g_CurrentEntity->action_ticks_counter = 0x1EU;
        g_CurrentEntity->blend_counter = 7;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
        g_CurrentEntity->move_speed_current = 0;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        if ((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
        if (g_CurrentEntity->behavior_flags & 0x10 & 0xFF) {
            g_CurrentEntity->action_ticks_counter = 0xC8U;
            return;
        }
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
}

void hunter_idle_walk(void) {
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id = 1;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->move_speed_current = 0;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
}

void hunter_behavior_chase(void) {

    g_CurrentEntity->animation_id = 0x10;
    g_CurrentEntity->move_speed_current = 0x28;
    LIT(VECTOR, sp_vec) = LIT(VECTOR, g_PlayerPosX);
    LIT(s32, sp_tmp1C) = 6;
    (g_ApiCallPtr = api_entity_update_wander_turn)(HUNTER(g_CurrentEntity)->dir_control_flags, &HUNTER(g_CurrentEntity)->room_hit, &HUNTER(g_CurrentEntity)->wander_turn_state, 0x18, 0x3C);
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 3;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
    }
    if (g_CurrentEntity->animation_frame_id == 1) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, NULL);
    }
    if (g_CurrentEntity->animation_frame_id == 0x20) {
        (g_ApiCallPtr = api_sound_play_entity_se)(0U, NULL);
    }
    LIT(s32, sp_tmp00) = (__builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x)) + (__builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z));
    if (LIT(s32, sp_tmp00) < 0x7D0) {
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, (void *)0x18);
    }
    (g_ApiCallPtr = api_joint_move)(0U, ((void *) g_CurrentEntity->anim_header), ((void *) g_CurrentEntity->anim_base), 0x400);
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
}

void hunter_behavior_nop(void) {

}

void hunter_behavior_attack(void) {
    void hunter_recenter_on_joint(u8 side);
    extern s32 (*hunter_attack_sub_tbl[])();
    hunter_attack_sub_tbl[g_CurrentEntity->action_state]();
    hunter_recenter_on_joint(1);
}

void hunter_atk_start(void) {
    g_CurrentEntity->action_state = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->move_speed_current = 0;
    g_CurrentEntity->animation_id = 5;
}

void hunter_atk_swing(void) {
    u8 frame;
    s32 joint;

    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        g_CurrentEntity->action_state++;
        if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        }
        return;
    }
    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        frame = g_CurrentEntity->animation_frame_id + 0xFC;
        if (frame < 8U) {
            joint = g_CurrentEntity->joints;
            joint += 0x45C;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joint, 0), ADDR_sp_vec, 0x320, ADDR_g_PlayerPosX);
            if ((LIT(s32, sp_tmp04) != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                (g_ApiCallPtr = api_sound_play_entity_se)(5, 0);
                g_PlayerEntity.hit_state = 1;
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(2, 0, 0x64, 0);
                (g_ApiCallPtr = api_pad_vibrate_small)(7, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)(3, 0xFA, 4);
                g_CurrentEntity->action_state = 2U;
                g_PlayerEntity.health -= 0xD;
            }
        }
    }
}

void hunter_atk_lunge(void) {
    s32 joint;

    g_CurrentEntity->move_speed_current = 0xC8;
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x52C);
    g_CurrentEntity->action_state = 3;
    g_CurrentEntity->action_ticks_counter = 4;
    joint = g_CurrentEntity->joints;
    joint += 0x45C;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    SCRATCH_VEC->x = 0xC8;
    (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joint, 0), ADDR_sp_vec, 0);
    (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
}

void hunter_atk_recover(void) {
    g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
    if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) != 0) {
        ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
        g_PlayerEntity.pos_x += g_CurrentEntity->speed_vx;
        g_PlayerEntity.pos_z += g_CurrentEntity->speed_vz;
    }
}

void hunter_atk_finish(void) {
    void *npc;

    ENTITY_FIELD_AS(g_CurrentEntity, state, s16) = 1;
    LIT(s32, sp_tmp1C) = ((g_ApiCallPtr = api_rand)() & 3) == 0;
    if (!(g_CurrentEntity->behavior_flags & 0xF & 0xFF)) {
        npc = HUNTER(g_CurrentEntity)->npc_target;
        if ((s16) (ENT(npc)->health & 0x80) == 0) {
            LIT(s32, sp_tmp1C) = (s32) ((g_ApiCallPtr = api_rand)() & 1);
        }
    }
    if ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) == 7) {
        LIT(s32, sp_tmp1C) = 0;
    }
    g_CurrentEntity->sub_state = (u8) LIT(s32, sp_tmp1C);
    g_CurrentEntity->action_behavior = (s8) ((LIT(s32, sp_tmp1C) * 5) + 1);
    g_CurrentEntity->action_state = 0;
}

void hunter_behavior_decap_leap(void) {
    extern s32 (*hunter_decap_leap_sub_tbl[])();

    HUNTER(g_CurrentEntity)->decap_chance = 0;
    hunter_decap_leap_sub_tbl[g_CurrentEntity->action_state]();
    if ((u8) g_CurrentEntity->animation_frame_id < 0x17U) {
        LIT(s32, sp_vec_vx) = (s32) HUNTER(g_CurrentEntity)->leap_target_x;
        LIT(s32, sp_vec_vz) = (s32) HUNTER(g_CurrentEntity)->leap_target_z;
        LIT(s32, sp_vec_vy) = 0;
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 0x18U));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0x1FU) {
        g_CurrentEntity->move_speed_current = 0x32;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        return;
    }
}

void hunter_decap_leap_start(void) {
    g_CurrentEntity->action_state = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->move_speed_current = 0x12C;
    g_CurrentEntity->animation_id = (s8) (0x12 - HUNTER_AS(g_CurrentEntity, decap_claw_joint, u8));
    if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
        g_CurrentEntity->animation_id = 0xC;
    }
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
}

void hunter_decap_strike(void) {
    u8 frame;
    void *joint;

    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF) && (g_PlayerEntity.health > 0)) {
        frame = g_CurrentEntity->animation_frame_id + 0xF9;
        joint = ((void *) g_PlayerEntity.joints);
        if ((frame < 8U) && !(FIELD_AT(joint, Joint, [1].flags, u8) & 0x40 & 0xFF)) {
            joint = ((void *) g_CurrentEntity->joints);
            joint = joint + (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) * 0x7C);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(&JOINT(joint)->world, ADDR_sp_vec, 0x2BC, LITP(void, g_PlayerPosX));
            if (LIT(s32, sp_tmp04) != 0) {
                g_PlayerEntity.hit_state = 1;
                ENTITY_STATE_WORD(&g_PlayerEntity) = STATE4(7, 6, 0, 0);
                g_CurrentEntity->action_state = 3U;
                g_CurrentEntity->action_ticks_counter = 4;
                joint = ((void *) g_CurrentEntity->joints);
                joint = joint + (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) * 0x7C);
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                SCRATCH_VEC->x = 0xC8;
                (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
                (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, (void *)0x606060);
                (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
                (g_ApiCallPtr = api_pad_vibrate_small)((void *)8, 0);
                return;
            }
        }
    }
    g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200));
}

void hunter_decap_leap_end(void) {
    ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
    HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
}

void hunter_decap_leap_track(void) {
    s32 hunter_track_player_joint_a1() __asm__("hunter_track_player_joint");
    void *pjoints;

    g_CurrentEntity->action_state += (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    pjoints = ((void *) g_PlayerEntity.joints);
    FIELD_AT(pjoints, Joint, [1].flags, s8) = (s8) (FIELD_AT(pjoints, Joint, [1].flags, u8) & 0xFE);
    hunter_track_player_joint_a1(HUNTER_AS(g_CurrentEntity, decap_claw_joint, u8));
}

void hunter_decap_leap_to_hold(void) {
    ENTITY_ACTION_S16(g_CurrentEntity) = 7;
}

void hunter_behavior_dodge(void) {
    extern s32 (*hunter_dodge_sub_tbl[])();
    hunter_dodge_sub_tbl[g_CurrentEntity->action_state]();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void hunter_dodge_start(void) {
    g_CurrentEntity->action_state = 1;
    g_CurrentEntity->sub_state = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->death_timer = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->move_speed_current = 0x5A;
    g_CurrentEntity->animation_id = 0x13;
    HUNTER(g_CurrentEntity)->airborne = 1;
    HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
    if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
        g_CurrentEntity->move_speed_current = 0x3C;
    }
}

void hunter_dodge_hop(void) {
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
    if (g_CurrentEntity->animation_frame_id & 4 & 0xFF) {
        g_CurrentEntity->action_state = 2;
        g_CurrentEntity->move_speed_current = 0x82;
        (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
    }
}

void hunter_dodge_swipe(void) {
    extern struct { u8 f0; u8 pad1[1]; s16 f2; s16 f4; s16 f6; s16 f8; } hunter_swipe_reach_tbl[];
    u8 frame;
    s32 joint;

    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if ((u8) g_CurrentEntity->animation_frame_id >= 9U) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x80);
    }
    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        LIT(s32, sp_tmp1C) = 2;
        do {
        frame = g_CurrentEntity->animation_frame_id - hunter_swipe_reach_tbl[LIT(s32, sp_tmp1C)].f0;
        if ((frame < hunter_swipe_reach_tbl[LIT(s32, sp_tmp1C)].f2) && (((u8) g_PlayerEntity.hit_state) == 0)) {
            joint = g_CurrentEntity->joints;
            joint = joint + (hunter_swipe_reach_tbl[LIT(s32, sp_tmp1C)].f4 * 0x7C);
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            SCRATCH_VEC->x = (s32) hunter_swipe_reach_tbl[LIT(s32, sp_tmp1C)].f6;
            LIT(s32, sp_tmp04) = (g_ApiCallPtr = api_joint_check_player_in_range)(JOINT_N_WORLD(joint, 0), ADDR_sp_vec, hunter_swipe_reach_tbl[LIT(s32, sp_tmp1C)].f8, ADDR_g_PlayerPosX);
            if ((LIT(s32, sp_tmp04) != 0) && (((u8) g_PlayerEntity.hit_state) == 0)) {
                LIT(s32, sp_tmp08) = (g_ApiCallPtr = api_is_facing_toward_entity)(ADDR_g_PlayerEntity);
                g_PlayerEntity.hit_state = (u8) LIT(s32, sp_tmp08) + 2;
                g_PlayerEntity.action_behavior = (u8) LIT(s32, sp_tmp08) + 0x66;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
                SCRATCH_VEC->x = 0x64;
                (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joint, 0), ADDR_sp_vec, 0);
                g_CurrentEntity->action_state = 4;
                (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, 0x606060);
                (g_ApiCallPtr = api_sound_play_entity_se)(3, 0);
                g_PlayerEntity.health -= 0x14;
                (g_ApiCallPtr = api_pad_vibrate_small)(7, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)(3, 0xFA, 4);
                return;
            }
        }
        } while (--LIT(s32, sp_tmp1C) != -1);
    }
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    if ((g_ApiCallPtr = api_entity_ballistic_step)((s32) g_CurrentEntity->move_speed_current, 0x244, -0x32, 0) != 0) {
        g_CurrentEntity->action_state = 3;
        g_CurrentEntity->move_speed_current = 0x5A;
        HUNTER(g_CurrentEntity)->airborne = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        return;
    }
}

void hunter_dodge_end(void) {
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
        return;
    }
}

void hunter_dodge_land(void) {
    (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    if ((u8) g_CurrentEntity->animation_frame_id >= 9U) {
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xC0);
    }
    if ((g_ApiCallPtr = api_entity_ballistic_step)(g_CurrentEntity->move_speed_current, 0x244, -0x32, 0) != 0) {
        g_CurrentEntity->sub_state = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 2;
        HUNTER(g_CurrentEntity)->airborne = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        return;
    }
}

void hunter_behavior_grabhold(void) {
    s32 hunter_track_player_joint_a2() __asm__("hunter_track_player_joint");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->move_speed_current = 0;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) ((g_CurrentEntity->action_state) + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        hunter_track_player_joint_a2(HUNTER_AS(g_CurrentEntity, decap_claw_joint, u8));
        return;
    case 2:
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->action_ticks_counter = 5U;
        /* fallthrough */
    case 3:
        g_CurrentEntity->action_ticks_counter = (u16) ((g_CurrentEntity->action_ticks_counter) - (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400));
        if ((s16) g_CurrentEntity->action_ticks_counter == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        hunter_track_player_joint_a2(HUNTER_AS(g_CurrentEntity, decap_claw_joint, u8));
        return;
    case 4:
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void hunter_behavior_approach(void) {

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->move_speed_current = 0xF0;
        g_CurrentEntity->action_ticks_counter = 0xAU;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
        HUNTER(g_CurrentEntity)->approach_frames = 0;
    }
    if (++HUNTER(g_CurrentEntity)->approach_frames == 0x5A) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 1;
    }
    (g_ApiCallPtr = api_joint_move)(0U, ENTITY_AS(g_CurrentEntity, anim_header, void *), ENTITY_AS(g_CurrentEntity, anim_base, void *), 0x200);
    if (g_CurrentEntity->animation_frame_id == 1) {
        (g_ApiCallPtr = api_sound_play_entity_se)(1U, NULL);
    }
    if (g_CurrentEntity->animation_frame_id == 0xF) {
        (g_ApiCallPtr = api_sound_play_entity_se)(1U, NULL);
    }
    LIT(VECTOR, sp_vec) = LIT(VECTOR, g_PlayerPosX);
    LIT(s32, sp_tmp1C) = 7;
    (g_ApiCallPtr = api_entity_update_wander_turn)(HUNTER(g_CurrentEntity)->dir_control_flags, &HUNTER(g_CurrentEntity)->room_hit, &HUNTER(g_CurrentEntity)->wander_turn_state, 0x60, 0x3C);
    LIT(s32, sp_tmp00) = (__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x))) + (__builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
    ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)--;
    LIT(s32, sp_tmp00) = (s32) ((__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x))) + (__builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z))));
    if (LIT(s32, sp_tmp00) < 0xFA0) {
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_g_PlayerPosX, (void *)0x60);
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0U);
}

void hunter_behavior_ledgejump(void) {

    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0U;
        g_CurrentEntity->move_speed_current = 0x5A;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
        HUNTER(g_CurrentEntity)->airborne = 1;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        if ((u8) g_CurrentEntity->animation_frame_id < 0xAU) {

            break;
        }
    case 2:
        if ((g_ApiCallPtr = api_entity_ballistic_step)(0, 0, -0x3C, 0) != 0) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 7;
            g_CurrentEntity->animation_id++;
            g_CurrentEntity->move_speed_current = 0;
            (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        }
        break;
    case 3:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->sub_state = 0;
            ENTITY_ACTION_S16(g_CurrentEntity) = 1;
            HUNTER(g_CurrentEntity)->airborne = 0;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0xFB);
            if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
                (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
                ENTITY_ACTION_S16(g_CurrentEntity) = 0;
            }
        }
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void hunter_behavior_scream(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 1;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0x17;
        /* fallthrough */
    case 1:
        if (g_CurrentEntity->animation_frame_id == 8) {
            (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        }
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
                (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            }
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
    }
}

void hunter_behavior_backstep(void) {
    LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerEntity.pos_x - g_CurrentEntity->pos_x) + __builtin_abs(g_PlayerEntity.pos_z - g_CurrentEntity->pos_z);
    g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 1;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0x14;
        g_CurrentEntity->move_speed_current = 0x96;
        if (LIT(s32, sp_tmp00) >= 0xBB9) {
            g_CurrentEntity->move_speed_current = 0xFA;
        }
        HUNTER(g_CurrentEntity)->backstep_dir = (s8) ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 1) + 2);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 0);
        }
        g_CurrentEntity->angle += (HUNTER(g_CurrentEntity)->backstep_dir - 2) << 6;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(HUNTER(g_CurrentEntity)->backstep_dir * 0x400);
}

void hunter_behavior_intro(void) {
    extern struct { s16 x, y; } hunter_intro_move_tbl[];
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        switch (g_StageRoom) {
        case STAGE_ROOM(2, 0xA):
            g_HunterIntroJumpKind = 2;
            break;
        case STAGE_ROOM(5, 3):
            g_HunterIntroJumpKind = 3;
            break;
        case STAGE_ROOM(5, 9):
            g_HunterIntroJumpKind = 4;
            if ((g_PrevRoomId != 0xA) && (g_PrevRoomId != 0x15)) {

            } else {
                g_HunterIntroJumpKind = 5;
            }
            break;
        }
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = (u8) *((g_CurrentEntity->behavior_flags * 2) + &hunter_intro_anim_tbl);
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 0);
            g_CurrentEntity->angle = (g_CurrentEntity->angle + hunter_intro_angle_tbl[g_CurrentEntity->behavior_flags]) & 0xFFF;
            g_CurrentEntity->behavior_flags = 2U;
            return;
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 6U) {
            g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x + (hunter_intro_move_tbl[g_HunterIntroJumpKind].x * 2));
            g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z + (hunter_intro_move_tbl[g_HunterIntroJumpKind].y * 2));
        }
        if (g_CurrentEntity->animation_frame_id == 0x14) {
            (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        }
        return;
    }
}

void hunter_act_swipe(void) {
    s32 joints;
    g_CurrentEntity->animation_id = ((g_CurrentEntity->action_behavior == 0) ? 3 : 4);
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        (g_ApiCallPtr = api_sound_play_entity_se)(6, 0U);
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(3, 8U, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 6U, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        return;
    case 2:
        LIT(u32, sp_tmp00) = API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x20U) >> 5;
        if (!((g_ApiCallPtr = api_rand)() & 1)) {
            LIT(s32, sp_tmp00) = (u32) (API_AS(u32, api_turn_toward_target)(ADDR_g_PlayerPosX, 0x80U) >> 7);
            g_CurrentEntity->action_behavior += (LIT(s32, sp_tmp00) == 0) ? 5 : 0;
            g_CurrentEntity->sub_state = (u8) (LIT(s32, sp_tmp00) == 0);
        }
        if (g_CurrentEntity->behavior_flags & 2 & 0xFF) {
            LIT(s32, sp_tmp00) = (u32) ((g_ApiCallPtr = api_rand)() & 1);
            g_CurrentEntity->sub_state = (u8) LIT(s32, sp_tmp00);
            g_CurrentEntity->action_behavior = (u8) ((LIT(s32, sp_tmp00) * 4) + 2);
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        }
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
        g_CurrentEntity->hit_state = 0;
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        } else {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 1, 0);
        }
        if ((HUNTER(g_CurrentEntity)->hit_window_timer != 0) && (HUNTER(g_CurrentEntity)->hit_window_count == 2)) {
            HUNTER(g_CurrentEntity)->hit_window_timer = 0;
            HUNTER(g_CurrentEntity)->hit_window_count = 0;
            g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 0xA, 0);
        }
        return;
    }
}

void hunter_act_leapattack(void) {
    s32 joints;

    switch (g_CurrentEntity->action_state) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 7;
        (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)(9, 6, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(3, 8, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        /* fallthrough */
    case 2:
        if ((g_ApiCallPtr = api_entity_ballistic_step)((g_CurrentEntity->move_speed_current / 2), 0, -0x32, 0) != 0) {
            g_CurrentEntity->action_state = 3U;
        }
        return;
    case 3:
        g_CurrentEntity->action_state = 4U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0x12;
        /* fallthrough */
    case 4:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->action_state = 5U;
            g_CurrentEntity->animation_frame_id = 0U;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->blend_counter = 7;
            g_CurrentEntity->animation_id = 8;
            ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) = (s16) (((g_ApiCallPtr = api_rand)() & 0x20) + 0x2D);
            g_CurrentEntity->move_speed_current = 0;
            g_CurrentEntity->hit_state = 0;
        }
        return;
    case 5:
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags & 0x1F);
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x20);
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        LIT(s32, sp_tmp00) = (__builtin_abs((g_PlayerEntity.pos_x - g_CurrentEntity->pos_x))) + (__builtin_abs((g_PlayerEntity.pos_z - g_CurrentEntity->pos_z)));
        if (ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 3) {
            if ((g_ApiCallPtr = api_rand)() & 1) {
                g_CurrentEntity->action_state = 0U;
                ENTITY_AS(g_CurrentEntity, action_behavior, s8) = 6;
            }
        }
                if ((u16) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) {
            g_CurrentEntity->action_state = 6U;
        }
        return;
    case 6:
        g_CurrentEntity->action_state = 7U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0xF;
        g_CurrentEntity->move_speed_current = 0x50;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0xC00);
        g_CurrentEntity->hit_state = 1;
        if ((g_RandSeed & 1 & 0xFFFF) && ((g_CurrentEntity->behavior_flags & 0xF & 0xFF) != 7)) {
            HUNTER(g_CurrentEntity)->leap_followup = (s8) (HUNTER(g_CurrentEntity)->leap_followup | 1);
        }
        /* fallthrough */
    case 7:
        LIT(s32, sp_tmp00) = (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (u8) LIT(s32, sp_tmp00));
        if ((u8) g_CurrentEntity->animation_frame_id < 0x1FU) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x40);
        }
        if ((u8) g_CurrentEntity->animation_frame_id < 0x14U) {
            g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x + g_CurrentEntity->speed_vx);
            g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z + g_CurrentEntity->speed_vz);
        }
        if (LIT(s32, sp_tmp00) != 0) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x800);
        }
        return;
    case 8:
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        } else {
            ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 0);
            if (HUNTER(g_CurrentEntity)->leap_followup & 1 & 0xFF) {
                ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 1, 6, 0);
                HUNTER(g_CurrentEntity)->leap_followup = (s8) (HUNTER(g_CurrentEntity)->leap_followup & 0xFE);
            }
        }
        g_CurrentEntity->move_speed_current = 0;
        HUNTER(g_CurrentEntity)->airborne = 0;
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
        g_CurrentEntity->hit_state = 0;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0x40);
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void hunter_act_pounce(void) {
    if (g_CurrentEntity->action_state == 0) {
        if (g_CurrentEntity->behavior_flags & 8 & 0xFF) {
            if ((u8) g_CurrentEntity->animation_frame_id >= 6U) {
                g_CurrentEntity->angle += hunter_intro_angle_tbl[g_CurrentEntity->behavior_flags];
            }
            g_CurrentEntity->behavior_flags = 2U;
        }
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        if (g_CurrentEntity->behavior_flags & 8 & 0xFF) {
            g_CurrentEntity->blend_counter = 0;
        }
        g_CurrentEntity->animation_id = 7U;
        (g_ApiCallPtr = api_sound_play_entity_se)(6U, 0);
    }
    if ((g_ApiCallPtr = api_joint_move)(0U, g_CurrentEntity->anim_header, ((s32 *) g_CurrentEntity->anim_base), 0x200) != 0) {
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->action_ticks_counter = (s16) (((g_ApiCallPtr = api_rand)() & 0x20) + 0x1E);
        g_CurrentEntity->move_speed_current = 0;
        ENTITY_ACTION_S16(g_CurrentEntity) = 0x604;
        if (g_CurrentEntity->behavior_flags & 8 & 0xFF) {
            g_CurrentEntity->behavior_flags = 2U;
        }
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        return;
    }
    g_CurrentEntity->move_speed_current = 0x46;
    if ((u8) g_CurrentEntity->animation_frame_id < 3U) {
        g_CurrentEntity->move_speed_current = 0xFA;
    }
    if (g_CurrentEntity->animation_frame_id == 0x10) {
        (g_ApiCallPtr = api_sound_play_entity_se)(4U, 0);
    }
    if (g_CurrentEntity->behavior_flags == 8) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 6U) {
            g_CurrentEntity->angle = (u16) (((s16) g_CurrentEntity->angle) - 0x400);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x400U);
        return;
    }
    if (g_CurrentEntity->behavior_flags == 9) {
        if ((u8) g_CurrentEntity->animation_frame_id >= 6U) {
            g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + 0x400);
        }
        (g_ApiCallPtr = api_entity_add_speed_xz)(0xC00U);
        return;
    }
    (g_ApiCallPtr = api_entity_add_speed_xz)(0x800U);
}

void hunter_act_flurry(void) {
    s32 joints;

    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 6;
        (g_ApiCallPtr = api_sound_play_entity_se)(6, 0);
    }
    joints = g_CurrentEntity->joints;
    LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
    (g_ApiCallPtr = api_effect_create_billboard)(0, 0, 0, JOINT_N_WORLD(joints, 3), ADDR_sp_vec, 0);
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        ENTITY_ACTION_S16(g_CurrentEntity) = 0x604;
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
            ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        }
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 6;
        g_CurrentEntity->action_ticks_counter = 9;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->hit_state = 0;
    }
}

void hunter_act_holdplayer(void) {
    s32 unused;
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0x16;
        g_CurrentEntity->hit_state = 1;
    }
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        ENTITY_STATE_WORD(g_CurrentEntity) = STATE4(1, 0, 2, 0);
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->hit_state = 0;
    }
}

void hunter_death_fall_driver(void) {
    s32 joints;
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->health = -1;
        g_CurrentEntity->action_state = 1U;
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->action_ticks_counter = 0x46U;
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)7, 0);
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            joints = g_CurrentEntity->joints;
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)9, 6, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)((void *)3, 8, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, JOINT_N_WORLD(joints, 1), ADDR_sp_vec, 0);
        }
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) ((g_CurrentEntity->action_state) + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        (g_ApiCallPtr = api_entity_add_speed_xz)((void *)0x800);
        return;
    case 2:
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, 0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0x64, -0x64);
        (g_ApiCallPtr = api_Flg_on)(LITP(void, g_EnemiesFlags), (s32) g_CurrentEntity->death_event_id);
        g_CurrentEntity->action_state = 3U;
        g_CurrentEntity->status_flags = (u8) (g_CurrentEntity->status_flags | 0xA);
        g_CurrentEntity->move_speed_current = 0;
        return;
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 0xC, 0xC);
        if (--ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16) == 0) {
            g_CurrentEntity->action_state = 4U;
        }
        return;
    case 4:
        if (g_CurrentEntity->behavior_flags & 0x40 & 0xFF) {
            (g_ApiCallPtr = api_Flg_on)(LITP(void, g_SysFlags), (s32) g_CurrentEntity->scd_anim_param);
        }
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void hunter_death_pounce(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->death_timer = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->action_ticks_counter = 0x46;
        g_CurrentEntity->animation_id = 0x11;
        (g_ApiCallPtr = api_sound_play_entity_se)(7, 0);
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200));
        /* fallthrough */
    case 2:
        if ((g_ApiCallPtr = api_entity_ballistic_step)((g_CurrentEntity->move_speed_current / 2), 0, -0x32, 0) != 0) {
            g_CurrentEntity->action_state = 3U;
            g_CurrentEntity->move_speed_current = 0;
            HUNTER(g_CurrentEntity)->airborne = 0;
        }
        return;
    case 3:
        g_CurrentEntity->action_state = 4U;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 7;
        g_CurrentEntity->animation_id = 0x12;
        (g_ApiCallPtr = api_sound_play_entity_se)(4, 0);
        /* fallthrough */
    case 4:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
            g_CurrentEntity->action_behavior = 0;
            g_CurrentEntity->action_state = 2U;
        }
        break;
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


void player_anim_hunter_decapitation(void) {
    void *pjoints;

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = (*(u8 *)&g_PlayerEntity.hit_state) + 0xFF;
        (*(u8 *)&g_PlayerEntity.death_timer) = 0xB4;
        g_UpdateFlags &= 0xFFBF;
        g_PlayerEntity.blend_counter = 3;
        pjoints = (*(void * *)&g_PlayerEntity.joints);
        FIELD_AT(pjoints, Joint, [1].flags, s8) = (s8) (FIELD_AT(pjoints, Joint, [1].flags, u8) | 8);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, JOINT_N_WORLD(pjoints, 1), LITP(void, sp_vec), 0);
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 3, 0, g_DeadMoveValue, JOINT_N_POS(pjoints, 1), 0);
        g_PlayerEntity.health = -1;
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)0xA, 0xA);
        (g_ApiCallPtr = api_pad_vibrate_small)((void *)8, 0x19);
        (g_ApiCallPtr = api_pad_vibrate_large)((void *)0x14, 0xC8, 0);
        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((void *)0x5A, 0xC8, 0, (void *)0x14);
        /* fallthrough */
    case 1:
        if ((u8) g_CurrentEntity->animation_frame_id < 0xAU) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = -0x898;
            (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, (u8 *)g_CurrentEntity + 0x20, LITP(void, sp_vec), 0);
        }
        if (g_CurrentEntity->animation_frame_id == 3) {
            pjoints = (*(void * *)&g_PlayerEntity.joints);
            (g_ApiCallPtr = api_joint_apply_color_tint)(pjoints, 0x30, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(pjoints, 9), 0x30, 0x80820, (void *)0x606060);
            (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(pjoints, 12), 0x30, 0x80820, (void *)0x606060);
        }
        if (g_CurrentEntity->animation_frame_id == 0x2A) {
            (g_ApiCallPtr = api_sound_play_footstep)((void *)2, 0);
        }
        g_PlayerEntity.action_state += (g_ApiCallPtr = api_joint_move)(NULL, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, (void *)0x400);
        (g_ApiCallPtr = api_entity_update_weapon_joint)(NULL);
        return;
    case 2:
        LIT(s16, sp_svec_vx) = -0x384;
        LIT(u16, sp_svec_vy) = 0;
        LIT(u16, sp_svec_vz) = 0;
        LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
        (g_ApiCallPtr = api_RotMatrixY)(*(s16 *)&g_PlayerEntity.angle, ADDR_sp_matrix);
        (g_ApiCallPtr = api_ApplyMatrixSV)(LITP(void, sp_matrix), ADDR_sp_svec, ADDR_sp_svec);
        g_CurrentEntity->shadow.offset_x = (u16) (g_CurrentEntity->shadow.offset_x + LIT(s32, sp_svec_vx));
        g_CurrentEntity->shadow.offset_y = (u16) (g_CurrentEntity->shadow.offset_y + LIT(s32, sp_svec_vy));
        g_CurrentEntity->shadow.offset_z = (u16) (g_CurrentEntity->shadow.offset_z + LIT(s32, sp_svec_vz));
        (g_ApiCallPtr = api_billboard_set_color)(&g_CurrentEntity->shadow, 1, 2, (void *)0xFFFF50);
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, -0xC8, -0xC8);
        g_PlayerEntity.action_state = 3;
        (*(u8 *)&g_PlayerEntity.hit_state) = 0x80;
        return;
    case 3:
        (g_ApiCallPtr = api_billboard_adj_size)(&g_CurrentEntity->shadow, 0x10, 0x10);
        if ((*(u8 *)&g_PlayerEntity.death_timer) == 0xA0) {
            g_FadeTypeId = 1;
            g_FadeStep = 0x100;
            (g_ApiCallPtr = api_fade_update)();
        }
        if ((((*(u8 *)&g_PlayerEntity.death_timer) = (*(u8 *)&g_PlayerEntity.death_timer) + 0xFF) & 0xFF) == 0x20) {
            (*(s8 *)&g_PlayerEntity.state) = 4;
        }
        return;
    case 4:
        (*(s8 *)&g_PlayerEntity.state) = 4;
        break;
    }
}

void hunter_track_player_joint(s8 claw_joint) {
    void *joint;
    void *pjoint;

    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), JOINT_N_TRANSFORM(joint, 1));
    joint = joint + (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) * 0x7C);
    LIT(s32, sp_tmp00) = 3;
    for (;;) {
        (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), (joint - (LIT(s32, sp_tmp00) * 0x7C)) + 0xA0);
        if ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) == 0) {
            break;
        }
    }
    JOINT(joint)->world = LIT(MATRIX, sp_matrix);
    pjoint = ((void *) g_PlayerEntity.joints);
    (*(Joint **)&pjoint)++;
    FIELD_AT(pjoint, Joint, ->world.t[0], VECTOR) = FIELD_AT(joint, Joint, ->world.t[0], VECTOR);
    LIT(s16, sp_svec_vx) = 0xC8;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    (g_ApiCallPtr = api_RotMatrixY)(((s16) g_CurrentEntity->angle) + 0xBB8 - ((((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) - 6) * 0x258), LITP(void, sp_matrix));
    (g_ApiCallPtr = api_ApplyMatrixSV)(LITP(void, sp_matrix), LITP(void, sp_svec), ADDR_sp_svec);
    JOINT(pjoint)->world.t[0] = (s32) (JOINT(pjoint)->world.t[0] + LIT(s16, sp_svec_vx));
    JOINT(pjoint)->world.t[2] = (s32) (JOINT(pjoint)->world.t[2] + LIT(s16, sp_svec_vz));
}

void hunter_recenter_on_joint(u8 side) {
    void *joint;
    u8 i;
    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_GsMulCoord0)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    joint = joint + ((side * 0x174) + 0x5D0);
    i = 3;
    while (i != 0) {
        (g_ApiCallPtr = api_GsMulCoord3)(LITP(void, sp_matrix), (joint - (i * 0x7C)) + 0xA0);
        i--;
    }
    LIT(s32, sp_matrix_t0) = (s32) (LIT(s32, sp_matrix_t0) - JOINT(joint)->world.t[0]);
    LIT(s32, sp_matrix_t1) = 0;
    LIT(s32, sp_matrix_t2) = (s32) (LIT(s32, sp_matrix_t2) - JOINT(joint)->world.t[2]);
    g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - LIT(s32, sp_matrix_t0));
    g_CurrentEntity->pos_z = (s32) (g_CurrentEntity->pos_z - LIT(s32, sp_matrix_t2));
}

void hunter_scd_state_dispatch(void) {
    extern s32 g_HunterScdVictim_s32 __asm__("g_HunterScdVictim");
    extern s32 (*hunter_scd_state_tbl[])();
    if (!(g_CurrentEntity->behavior_flags & 0x40 & 0xFF)) {
        g_CurrentEntity->state = 1;
        return;
    }
    g_HunterScdVictim_s32 = ADDR_g_EnemiesList;
    hunter_scd_state_tbl[g_CurrentEntity->action_behavior]();
}

void hunter_scd_idle(void) {
    if (g_CurrentEntity->action_state == 0) {
        g_CurrentEntity->animation_id = 0x15;
        g_CurrentEntity->move_speed_current = 0;
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
    }
    (g_ApiCallPtr = api_joint_move)(g_CurrentEntity->scd_entity_flags & 1 & 0xFFFF, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
}

void hunter_scd_walk(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->animation_id = 0x10;
        g_CurrentEntity->move_speed_current = 0x28;
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        HUNTER(g_CurrentEntity)->decap_claw_joint = 0;
        /* fallthrough */
    case 1:
        LIT(s32, sp_vec_vx) = (s32) g_CurrentEntity->anim_base_x;
        LIT(s32, sp_vec_vz) = (s32) g_CurrentEntity->anim_base_z;
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x40U);
        (g_ApiCallPtr = api_joint_move)(g_CurrentEntity->scd_entity_flags & 1 & 0xFFFF, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80U);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        if (g_CurrentEntity->animation_frame_id == 1) {
            (g_ApiCallPtr = api_sound_play_entity_se)(0, 0U);
        }
        if (g_CurrentEntity->animation_frame_id == 0x20) {
            (g_ApiCallPtr = api_sound_play_entity_se)(0, 0U);
        }
        if ((g_ApiCallPtr = api_SquareRoot0)(((g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x) * (g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x)) + ((g_CurrentEntity->pos_z - g_CurrentEntity->anim_base_z) * ((g_CurrentEntity->pos_z) - (g_CurrentEntity->anim_base_z)))) < 0x96U) {
            g_CurrentEntity->action_state++;
        }
        return;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        if (g_CurrentEntity->collision_flags & 0x80 & 0xFF) {
            g_CurrentEntity->action_state = 1U;
            return;
        }
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        return;
    }
}

void hunter_scd_run(void) {
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->animation_frame_id = 0U;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id = 2;
        g_CurrentEntity->move_speed_current = 0xC8;
        /* fallthrough */
    case 1:
        LIT(s32, sp_vec_vx) = (s32) g_CurrentEntity->anim_base_x;
        LIT(s32, sp_vec_vz) = (s32) g_CurrentEntity->anim_base_z;
        LIT(s32, sp_vec_vy) = 0;
        (g_ApiCallPtr = api_joint_move)(g_CurrentEntity->scd_entity_flags & 1 & 0xFFFF, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80U);
        if (g_CurrentEntity->animation_frame_id == 1) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0U);
        }
        if (g_CurrentEntity->animation_frame_id == 0xF) {
            (g_ApiCallPtr = api_sound_play_entity_se)(1, 0U);
        }
        (g_ApiCallPtr = api_entity_rotate_toward_target)(ADDR_sp_vec, 0x60U);
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        if ((g_ApiCallPtr = api_SquareRoot0)(((g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x) * (g_CurrentEntity->pos_x - g_CurrentEntity->anim_base_x)) + ((g_CurrentEntity->pos_z - g_CurrentEntity->anim_base_z) * ((g_CurrentEntity->pos_z) - (g_CurrentEntity->anim_base_z)))) < 0x96U) {
            g_CurrentEntity->action_state++;
        }
        return;
    case 2:
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
        if (g_CurrentEntity->collision_flags & 0x80 & 0xFF) {
            g_CurrentEntity->action_state = 1U;
            return;
        }
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        return;
    }
}

void hunter_scd_dodge_run(void) {
    extern s32 (*hunter_scd_dodge_tbl[])();
    hunter_scd_dodge_tbl[g_CurrentEntity->action_state]();
    (g_ApiCallPtr = api_entity_add_speed_xz)(0);
}

void hunter_scd_anim_release(void) {
    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400) != 0) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
    }
}

void hunter_scd_decap_leap_run(void) {
    extern s32 (*hunter_scd_decap_leap_tbl[])();

    hunter_scd_decap_leap_tbl[g_CurrentEntity->action_state]();
    if ((u8) g_CurrentEntity->animation_frame_id < 0x17U) {
        LIT(s32, sp_vec_vx) = (s32) HUNTER(g_CurrentEntity)->leap_target_x;
        LIT(s32, sp_vec_vz) = (s32) HUNTER(g_CurrentEntity)->leap_target_z;
        LIT(s32, sp_vec_vy) = 0;
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 0x18U));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0x1FU) {
        g_CurrentEntity->move_speed_current = 0x32;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        return;
    }
}

void hunter_scd_flag_release(void) {
    (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
}

void hunter_scd_swipe_run(void) {
    extern s32 (*hunter_scd_swipe_tbl[])();
    hunter_scd_swipe_tbl[g_CurrentEntity->action_state]();
    hunter_recenter_on_joint(1);
}

void hunter_scd_tint_release(void) {
    s32 joints;

    joints = g_CurrentEntity->joints;
    (g_ApiCallPtr = api_joint_apply_color_tint)(JOINT_N(joints, 9), 0x30U, 0x80820, 0x606060);
    (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, g_CurrentEntity->scd_anim_param);
    ENTITY_ACTION_S16(g_CurrentEntity) = 0;
}

void hunter_scd_attack_run(void) {
    extern s32 (*hunter_scd_attack_tbl[])();

    hunter_scd_attack_tbl[g_CurrentEntity->action_state]();
    if ((u8) g_CurrentEntity->animation_frame_id < 0x17U) {
        LIT(s32, sp_vec_vx) = (s32) HUNTER(g_CurrentEntity)->leap_target_x;
        LIT(s32, sp_vec_vz) = (s32) HUNTER(g_CurrentEntity)->leap_target_z;
        LIT(s32, sp_vec_vy) = 0;
        g_CurrentEntity->angle = (u16) ((g_CurrentEntity->angle) + (g_ApiCallPtr = api_turn_toward_target)(ADDR_sp_vec, 0x18U));
        (g_ApiCallPtr = api_entity_add_speed_xz)(0);
        return;
    }
    if ((u8) g_CurrentEntity->animation_frame_id < 0x1FU) {
        g_CurrentEntity->move_speed_current = 0x32;
        (g_ApiCallPtr = api_entity_add_speed_xz)(0x800);
        return;
    }
}

void hunter_scd_lunge_start(void) {
    g_CurrentEntity->action_state = 1;
    g_CurrentEntity->animation_frame_id = 0;
    g_CurrentEntity->timing_control = 0;
    g_CurrentEntity->blend_counter = 7;
    g_CurrentEntity->move_speed_current = 0x12C;
    g_CurrentEntity->animation_id = (s8) (0x12 - HUNTER_AS(g_CurrentEntity, decap_claw_joint, u8));
    g_CurrentEntity->animation_id = 0xC;
    (g_ApiCallPtr = api_sound_play_entity_se)(2, 0);
}

void hunter_scd_bite_driver(void) {
    u8 frame;
    void *joint;

    frame = g_CurrentEntity->animation_frame_id + 0xF9;
    joint = ((void *) ENT(g_HunterScdVictim)->joints);
    if ((frame < 8U) && !(FIELD_AT(joint, Joint, [1].flags, u8) & 0x40 & 0xFF)) {
        joint = ((void *) g_CurrentEntity->joints);
        joint = joint + (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) * 0x7C);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        ENT(g_HunterScdVictim)->hit_state = 1;
        FIELD_AT(g_HunterScdVictim, Entity, ->state, s32) = 0x20001;
        g_CurrentEntity->action_state = 2U;
        g_CurrentEntity->action_ticks_counter = 4;
        joint = ((void *) g_CurrentEntity->joints);
        joint = joint + (((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) * 0x7C);
        LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
        SCRATCH_VEC->x = 0xC8;
        (g_ApiCallPtr = api_effect_create_billboard)(NULL, 0, 0, &JOINT(joint)->world, ADDR_sp_vec, 0);
        (g_ApiCallPtr = api_joint_apply_color_tint)(joint, 0x30, 0x80820, (void *)0x606060);
        (g_ApiCallPtr = api_sound_play_entity_se)((void *)5, 0);
        return;
    }
    g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + (g_ApiCallPtr = api_joint_move)(NULL, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, (void *)0x200));
}

void hunter_scd_bite_end(void) {
    s32 hunter_scd_track_joint_a1() __asm__("hunter_scd_track_joint");
    void *vjoints;

    if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200) != 0) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 7;
    }
    vjoints = ((void *) ENT(g_HunterScdVictim)->joints);
    FIELD_AT(vjoints, Joint, [1].flags, u8) = (u8) (FIELD_AT(vjoints, Joint, [1].flags, u8) & 0xFE);
    hunter_scd_track_joint_a1(6);
}

void hunter_scd_death(void) {
    s32 hunter_scd_track_joint_a2(s32) __asm__("hunter_scd_track_joint");
    switch ((g_CurrentEntity->action_state)) {
    case 0:
        g_CurrentEntity->action_state++;
        g_CurrentEntity->animation_frame_id = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->animation_id++;
        g_CurrentEntity->move_speed_current = 0;
        /* fallthrough */
    case 1:
        if ((g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80) != 0) {
            g_CurrentEntity->action_state++;
            g_CurrentEntity->animation_frame_id = 0;
            g_CurrentEntity->timing_control = 0;
            g_CurrentEntity->animation_id++;
        }
        hunter_scd_track_joint_a2(6);
        return;
    case 2:
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        hunter_scd_track_joint_a2(6);
        return;
    }
}

void hunter_scd_track_joint(s8 claw_joint) {
    void *joint;
    void *vjoint;

    joint = ((void *) g_CurrentEntity->joints);
    (g_ApiCallPtr = api_RotMatrix)(&g_CurrentEntity->rot_x, (u8 *)g_CurrentEntity + 0x20);
    (g_ApiCallPtr = api_CompMatrix)((u8 *)g_CurrentEntity + 0x20, &JOINT(joint)->transform, ADDR_sp_matrix);
    (g_ApiCallPtr = api_CompMatrix)(LITP(void, sp_matrix), JOINT_N_TRANSFORM(joint, 1), ADDR_sp_matrix);
    joint += 0x2E8;
    LIT(s32, sp_tmp00) = 3;
    for (;;) {
        (g_ApiCallPtr = api_CompMatrix)(LITP(void, sp_matrix), (joint - (LIT(s32, sp_tmp00) * 0x7C)) + 0xA0, ADDR_sp_matrix);
        if ((LIT(s32, sp_tmp00) = LIT(s32, sp_tmp00) - 1) == 0) {
            break;
        }
    }
    JOINT(joint)->world = LIT(MATRIX, sp_matrix);
    vjoint = ((void *) ENT(g_HunterScdVictim)->joints);
    (*(Joint **)&vjoint)++;
    FIELD_AT(vjoint, Joint, ->world.t[0], VECTOR) = FIELD_AT(joint, Joint, ->world.t[0], VECTOR);
    LIT(s16, sp_svec_vx) = 0xC8;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(MATRIX, sp_matrix) = *(MATRIX *)g_DeadMoveValue;
    (g_ApiCallPtr = api_RotMatrixY)(((s16) g_CurrentEntity->angle) + 0xBB8 - ((((u16) HUNTER(g_CurrentEntity)->decap_claw_joint) - 6) * 0x258), LITP(void, sp_matrix));
    (g_ApiCallPtr = api_ApplyMatrixSV)(LITP(void, sp_matrix), LITP(void, sp_svec), ADDR_sp_svec);
    JOINT(vjoint)->world.t[0] = (s32) (JOINT(vjoint)->world.t[0] + LIT(s16, sp_svec_vx));
    JOINT(vjoint)->world.t[2] = (s32) (JOINT(vjoint)->world.t[2] + LIT(s16, sp_svec_vz));
}

void hunter_scd_stalk_player(void) {

    if (g_CurrentEntity->action_state == 0) {
        ENTITY_AS(g_CurrentEntity, animation_frame_id, s8) = 0;
        g_CurrentEntity->timing_control = 0;
        g_CurrentEntity->blend_counter = 0x1F;
        g_CurrentEntity->hit_state = 0;
        if ((g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x400) != 0) {
            g_CurrentEntity->animation_id = 0x10;
        }
        g_CurrentEntity->action_state = 1U;
        g_CurrentEntity->action_ticks_counter = (u16) (((g_ApiCallPtr = api_rand)() & 0x3F) + 0x1E);
    }
    LIT(s32, sp_tmp1C) = (g_ApiCallPtr = api_turn_toward_target)(ADDR_g_PlayerPosX, 0x64);
    if (((u32) ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)-- == 0) || (LIT(s32, sp_tmp1C) == 0)) {
        (g_ApiCallPtr = api_Flg_on)(ADDR_g_SysFlags, (s32) g_CurrentEntity->scd_anim_param);
        ENTITY_ACTION_S16(g_CurrentEntity) = 0;
        g_CurrentEntity->waypoint_x = (u16) g_PlayerEntity.pos_x;
        g_CurrentEntity->waypoint_z = (u16) g_PlayerEntity.pos_z;
    } else {
        (g_ApiCallPtr = api_joint_move)(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x80);
        g_CurrentEntity->angle += LIT(u16, sp_tmp1C);
    }
}
