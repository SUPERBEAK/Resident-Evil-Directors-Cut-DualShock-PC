/* player.c - The player character: state machine, control (walk, run, turn, actions, stairs, ladders, pushing), animation
   and hit reactions, death. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 is_point_in_quad(s32 *pos, u16 *quad);
void load_equipped_weapon_animation(s32 weapon, s32 joint_index, s32 buf, u32 *anim_object);
void load_entity_model(void);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
s32 mirror_point_visible(s32 view, s32 axis, s32 pt);
void entity_render_mirror(void);
void clear_anim_timing();
void player_update_detached_joint(void);
void entity_rotate_collision_points(u8 *obj);
s32 check_action_object(void);
s32 check_climb_object(void);
void pad_vibrate_small(s32 duration, s32 delay);
void pad_vibrate_large(s32 duration, s32 vol, s32 delay);
s32 pad_vibrate_large_ramp(s32 duration, s32 vol_start, s32 vol_end, s32 delay);
void pad_vibration_reset(void);


void update_player_anim(void) {
    s32 is_point_in_quad_a3() __asm__("is_point_in_quad");
    s32 mirror_point_visible();
    s32 entity_render_mirror_a1() __asm__("entity_render_mirror");
    void player_update_detached_joint();
    s32 entity_rotate_collision_points_a2() __asm__("entity_rotate_collision_points");
    s32 billboard_draw_a2() __asm__("billboard_draw");
    extern s32 (*g_PlayerStateFuncs[])();
    s32 unused[8];
    u8 zone_flags;

    g_CurrentEntity = LITP(Entity, g_PlayerEntity);
    if (g_UpdateFlags & 1) {
        g_PlayerStateFuncs[g_PlayerEntity.state]();
        entity_update_look_at_angles();
    }
    entity_rotate_collision_points_a2(ADDR_g_PlayerEntity);
    if ((g_PlayerEntity.state != 5) && (g_PlayerEntity.action_behavior != 0x11)) {
        entity_resolve_enemy_player_collisions();
        check_room_collision(ADDR_g_PlayerPosX, SCA(((void *) g_PlayerEntity.sca_info))->radius);
    }
    g_PlayerEntity.sca_matrix_head = 0;
    g_PlayerEntity.has_enter_switch_zone &= 0xFE;
    zone_flags = g_PlayerEntity.has_enter_switch_zone | is_point_in_quad_a3(ADDR_g_PlayerPosX, g_CameraSwitchZones);
    g_PlayerEntity.has_enter_switch_zone = zone_flags;
    if ((zone_flags & 0x7F) && !(g_PlayerEntity.scd_entity_flags & 0x40)) {
        billboard_draw_a2(ADDR_g_PlayerPosX, ADDR_g_PlayerShadowOffsetX, g_PlayerEntity.floor_y, ((s16) g_PlayerEntity.angle));
    }
    if ((g_GameStateFlags & GAME_MIRROR_ON) && (mirror_point_visible((s32)ROOM_CAMERA_VIEW(g_CutId), (u32) (g_GameStateFlags & GAME_MIRROR_AXIS) >> 1, ADDR_g_PlayerPosX) != 0)) {
        entity_render_mirror_a1();
    }
    player_update_detached_joint();
}

void player_state_init(void) {
    s32 joint_move_a11() __asm__("joint_move");
    s32 clear_anim_timing_p() __asm__("clear_anim_timing");
    s32 billboard_init_a2() __asm__("billboard_init");
    void **pp;

    g_PadHeld = 0;
    ENTITY_STATE_WORD(&g_PlayerEntity) = 1;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    pp = (void **)&g_PlayerScaHitData; /* reading the pointer through pp forces the reload the original has */
    LIT(s16, sp_svec_vx) = 0;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a2(ADDR_sp_svec, ADDR_g_PlayerShadowOffsetX, 0x1F4, 0x2BC);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    CURRENT_ENTITY_LIT->animation_id = 0;
    CURRENT_ENTITY_LIT->animation_frame_id = 0;
    CURRENT_ENTITY_LIT->timing_control = 0;
    joint_move_a11(0, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
    FIELD_AT(g_PlayerScaHitData, HitPoint, ->x, s16) = 0;
    FIELD_AT(g_PlayerScaHitData, HitPoint, ->z, u16) = 0;
    FIELD_AT(*pp, HitPoint, ->y, u16) = ((u16) SCA(((void *) g_PlayerEntity.sca_info))->ofs_y);
    g_PlayerEntity.has_enter_switch_zone = 0;
    g_PlayerEntity.hit_state = 0;
    g_PlayerEntity.next_turn_timer = 0;
    g_PlayerEntity.position_vx = (*(u16 *)&g_PlayerEntity.pos_x);
    g_PlayerEntity.position_vy = (*(u16 *)&g_PlayerEntity.pos_y);
    g_PlayerEntity.position_vz = (*(u16 *)&g_PlayerEntity.pos_z);
    g_PlayerEntity.collision_flags |= 0x10;
    clear_anim_timing_p();
}

void setup_character_data(void) {
    s32 load_equipped_weapon_animation_a1(u8, s32, s32, s32) __asm__("load_equipped_weapon_animation");
    s32 init_player_entity_a1() __asm__("init_player_entity");
    s32 entity_set_joint_anim_data(s32);
    extern s16 g_PlayerPositionVx_s16 __asm__("g_PlayerPositionVx");
    extern s16 g_PlayerPositionVz_s16 __asm__("g_PlayerPositionVz");
    extern void * g_PlayerJoints_voidp __asm__("g_PlayerJoints");
    extern s8 g_VramTpageCursor_s8 __asm__("g_VramTpageCursor");
    extern s32 g_CurrentEntity_s32 __asm__("g_CurrentEntity");
    void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
    g_CurrentEntity_s32 = ADDR_g_PlayerEntity;
    init_player_entity_a1();
    g_VramClutRowCursor = 7;
    g_VramTpageCursor_s8 = 0x16;
    load_entity_model();
    GsInitCoordinate2(NULL, LITP(GsCOORDINATE2, g_PlayerCoord2));
    entity_set_joint_anim_data(0xE);
    g_PlayerEntity.behavior_flags = 0;
    if (g_EquippedSlot != 0) {
        g_PlayerBehaviorFlags = *((g_EquippedSlot * 2) + &LIT(s8, g_InventorySlotsBase1));
    }
    load_equipped_weapon_animation_a1(g_PlayerBehaviorFlags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
    g_PlayerEntity.pos_y = 0;
    g_PlayerEntity.floor_y = 0;
    g_PlayerEntity.pos_x = (s32) g_PlayerPositionVx_s16;
    g_PlayerEntity.pos_z = (s32) g_PlayerPositionVz_s16;
    FIELD_AT(g_PlayerJoints_voidp, Joint, [1].rot_delta_x, s16) = 0;
    FIELD_AT(g_PlayerJoints_voidp, Joint, [1].rot_delta_y, s16) = 0;
    FIELD_AT(g_PlayerJoints_voidp, Joint, [1].rot_delta_z, s16) = 0x10;
    g_PlayerEntity.sca_info = *(s32 *)((u8 *)&g_DefaultScaInfo + ((g_PlayerEntity.id & 1) * 8));
    clear_anim_timing(g_PlayerPositionVz_s16);
}

void init_player_entity(void) {
    g_PlayerEntity.death_timer = 0;
    g_PlayerEntity.animation_id = 0;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.pad0C0 = 0;
    g_PlayerEntity.status_flags = 1;
    g_PlayerEntity.has_enter_switch_zone = 1;
    g_PlayerCoord2Ptr = ADDR_g_PlayerCoord2;
    ENTITY_STATE_WORD(&g_PlayerEntity) = 0;
    g_PlayerEntity.unused_0C1 = 0;
    g_PlayerEntity.move_speed_current = 0;
    g_PlayerEntity.hit_state = 0;
    g_PlayerEntity.speed_vx = 0;
    g_PlayerEntity.speed_vy = 0;
    g_PlayerEntity.speed_vz = 0;
    g_PlayerEntity.rot_x = 0;
    g_PlayerEntity.angle_z = 0;
    g_PlayerCoordM00 = 0x1000;
    g_PlayerCoordM01 = 0;
    g_PlayerCoordM02 = 0;
    g_PlayerCoordM10 = 0;
    g_PlayerCoordM11 = 0x1000;
    g_PlayerCoordM12 = 0;
    g_PlayerCoordM20 = 0;
    g_PlayerCoordM21 = 0;
    g_PlayerCoordM22 = 0x1000;
    g_PlayerEntity.scale = 0;
    g_PlayerEntity.look_at_flags = 0;
}

void player_state_01_control(void) {
    extern s32 (*g_PlayerCtrlFrameFuncs[])();
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    extern s8 g_PlayerActionState_s8 __asm__("g_PlayerActionState");
    extern s16 g_PlayerHealth_s16 __asm__("g_PlayerHealth");
    s16 health;
    s32 lethal;
    u8 timer;

    if (g_PlayerHealth_s16 < 0) {
        if (g_PlayerActionTicksCounter == 0x7FFF) {
            g_PlayerAngle += 0x800;
        }
        g_PlayerState = 3;
        g_PlayerActionBehavior_u8 = 0xC8;
        g_PlayerHitState = 1;
        if (g_StatusFlags & STATUS_IN_WATER) {
            g_PlayerHealth_s16 = 1;
        }
    } else {
        if (g_PlayerHitState & 0x3F) {
            *(s16 *)&g_PlayerState = 2;
            g_PlayerActionState_s8 = 0;
            return;
        }
        if (g_PlayerCollisionFlags & 0x62) {
            timer = g_PlayerPoisonTimer;
            g_PlayerPoisonTimer = timer + 0xFF;
            if (timer == 0) {
                g_PlayerPoisonTimer = 0x78;
                lethal = g_PlayerCollisionFlags & 0x40;
                if (lethal != 0) {
                    g_PlayerPoisonTimer = 7;
                }
                health = g_PlayerHealth_s16 - 2;
                g_PlayerHealth_s16 = health;
                if (((s16) health < 0) && (lethal == 0)) {
                    g_PlayerHealth_s16 = 1;
                }
            }
        }
        if (g_PlayerNextTurnTimer != 0) {
            g_PlayerNextTurnTimer -= 1;
        }
        g_PlayerCtrlFrameFuncs[g_PlayerSubState]();
    }
}

void player_state_02(void) {
    void player_hit_react_common();
    if (((u8) g_PlayerEntity.sub_state) == 0) {
        player_hit_react_common();
    }
}

void player_state_03(void) {
    s32 player_state_03_death();
    player_state_03_death();
}

void player_state_block_input(void) {
    g_UpdateFlags = 0;
}

void player_state_anim_window0(void) {
    if (g_PlayerActionState == 0) {
        if ((g_GameStateFlags & GAME_PUSHING) || (g_PlayerCollisionFlags & 0x80)) {
            LIT(u16, g_UpdateFlags) |= 0x40;
        }
        g_PlayerCollisionFlags &= 0x7F;
    }
    ((s32 (**)())ADDR_g_PlayerAnimFunctions)[g_PlayerSubState]();
}

void player_state_anim_window1(void) {
    if (g_PlayerActionState == 0) {
        if ((g_GameStateFlags & GAME_PUSHING) || (g_PlayerCollisionFlags & 0x80)) {
            LIT(u16, g_UpdateFlags) |= 0x40;
        }
        g_PlayerCollisionFlags &= 0x7F;
    }
    ((s32 (**)())ADDR_g_PlayerAnimFunctions + 19)[g_PlayerSubState]();
}

void player_state_anim_window2(void) {
    ((s32 (**)())ADDR_g_PlayerAnimFunctions + 38)[((u8) g_PlayerEntity.sub_state)]();
}

void player_ctrl_frame0(void) {
    s32 player_ctrl_behavior_dispatch_a1() __asm__("player_ctrl_behavior_dispatch");
    s32 player_input_to_behavior_a1() __asm__("player_input_to_behavior");
    player_input_to_behavior_a1();
    player_ctrl_behavior_dispatch_a1();
}

void player_ctrl_behavior_dispatch(void) {
    extern u16 g_PlayerTurnSpeedRun_u16 __asm__("g_PlayerTurnSpeedRun");
    extern u16 g_PlayerMoveSpeedCurrent_u16 __asm__("g_PlayerMoveSpeedCurrent");
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void player_behavior_00_idle_l() __asm__("player_behavior_00_idle");
    void player_ctrl_behavior_walk_l() __asm__("player_ctrl_behavior_walk");
    void player_ctrl_behavior_turn_l() __asm__("player_ctrl_behavior_turn");
    void player_behavior_09_nop_l() __asm__("player_behavior_09_nop");
    void player_behavior_0c_interact_l() __asm__("player_behavior_0c_interact");
    void player_ctrl_behavior_walk_back_l() __asm__("player_ctrl_behavior_walk_back");
    void player_behavior_0d_run_l() __asm__("player_behavior_0d_run");
    void player_behavior_0b_ladder_l() __asm__("player_behavior_0b_ladder");
    void player_behavior_10_push_l() __asm__("player_behavior_10_push");
    void player_climb_sequence_l() __asm__("player_climb_sequence");

    switch (g_PlayerEntity.action_behavior) {
    case 0:
        player_behavior_00_idle_l();
        break;
    case 2:
        g_PlayerAngle = (g_PlayerEntity.angle + g_PlayerTurnSpeedWalk) & 0xFFF;
        player_ctrl_behavior_walk_l();
        break;
    case 3:
        g_PlayerAngle = (g_PlayerEntity.angle - g_PlayerTurnSpeedWalk) & 0xFFF;
    case 1:
        player_ctrl_behavior_walk_l();
        break;
    case 4:
        g_PlayerAngle = (g_PlayerEntity.angle + g_PlayerTurnSpeedStand) & 0xFFF;
        player_ctrl_behavior_turn_l();
        break;
    case 5:
        g_PlayerAngle = (g_PlayerEntity.angle - g_PlayerTurnSpeedStand) & 0xFFF;
        player_ctrl_behavior_turn_l();
        break;
    case 6:
        g_PlayerAngle = (g_PlayerEntity.angle + g_PlayerTurnSpeedWalk) & 0xFFF;
        player_ctrl_behavior_walk_back_l();
        break;
    case 7:
        g_PlayerAngle = (g_PlayerEntity.angle - g_PlayerTurnSpeedWalk) & 0xFFF;
    case 8:
        player_ctrl_behavior_walk_back_l();
        break;
    case 9:
        player_behavior_09_nop_l();
        break;
    case 11:
        player_behavior_0b_ladder_l();
        break;
    case 12:
        player_behavior_0c_interact_l();
        break;
    case 14:
        SCRATCH->sv_z = 0;
        SCRATCH->sv_y = 0;
        g_PlayerAngle = (g_PlayerEntity.angle + g_PlayerTurnSpeedRun_u16) & 0xFFF;
        SCRATCH->sv_x = g_PlayerMoveSpeedCurrent_u16;
        RotMatrix(LITP(SVECTOR, g_PlayerRotX), LITP(MATRIX, sp_matrix));
        ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, g_PlayerSpeed));
        player_behavior_0d_run_l();
        break;
    case 15:
        SCRATCH->sv_z = 0;
        SCRATCH->sv_y = 0;
        g_PlayerAngle = (g_PlayerEntity.angle - g_PlayerTurnSpeedRun_u16) & 0xFFF;
        SCRATCH->sv_x = g_PlayerMoveSpeedCurrent_u16;
        RotMatrix(LITP(SVECTOR, g_PlayerRotX), LITP(MATRIX, sp_matrix));
        ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, g_PlayerSpeed));
    case 13:
        player_behavior_0d_run_l();
        break;
    case 16:
        player_behavior_10_push_l();
        break;
    case 10:
    case 17:
        player_climb_sequence_l();
        break;
    }
}

void player_ctrl_frame1(void) {
    extern s32 (*g_PlayerBehaviorFuncs[])();
    g_PlayerBehaviorFuncs[g_PlayerEntity.action_behavior]();
}

void player_ctrl_weapon_dispatch(void) {
    s32 player_behavior_14_autoaim();
    s32 player_behavior_15_gun_aim_change();
    s32 player_behavior_17_holster();
    s32 player_behavior_19_fire_click();
    switch (g_PlayerEntity.action_behavior) {
    case 18:
        player_behavior_12_gun_aim();
        break;
    case 19:
        player_behavior_13_gun_raise();
        player_behavior_13_gun_hold_input();
        break;
    case 20:
        player_behavior_14_autoaim();
        break;
    case 21:
        LIT(s32, sp_tmp1C) = 0;
        player_behavior_15_gun_aim_change();
        break;
    case 22:
        LIT(s32, sp_tmp1C) = 1;
        player_behavior_15_gun_aim_change();
        break;
    case 23:
        player_behavior_17_holster();
        break;
    case 24:
        player_behavior_18_reload();
        break;
    case 25:
        player_behavior_19_fire_click();
        break;
    case 26:
        player_behavior_1a_lockon_turn();
        break;
    }
    entity_update_weapon_joint(0);
    if (g_PlayerEntity.behavior_flags == 6) {
        auto_aim_pitch_update();
    }
}

void player_ctrl_frame4(void) {
    extern s32 (*g_PlayerBehaviorFuncs[])();
    g_PlayerBehaviorFuncs[g_PlayerEntity.action_behavior]();
}

void player_hit_react_common(void) {
    void player_hit_react_light();
    void player_hit_react_damage();
    void player_hit_react_heavy();
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    if (g_PlayerActionState == 0) {
        if ((g_GameStateFlags & GAME_PUSHING) || (g_PlayerCollisionFlags & 0x80)) {
            LIT(u16, g_UpdateFlags) |= 0x40;
        }
        g_PlayerCollisionFlags &= 0x7F;
    }
    switch (g_PlayerActionBehavior_u8) {
    case 0x64:
    case 0x65:
        player_hit_react_light();
        return;
    case 0x66:
        player_hit_react_damage();
        if ((u8) g_PlayerAnimationFrameId >= 4U) {
            g_PlayerMoveSpeedCurrent = 0x1E;
        }
        entity_add_speed_xz(0x800);
        return;
    case 0x67:
        player_hit_react_damage();
        if ((u8) g_PlayerAnimationFrameId >= 4U) {
            g_PlayerMoveSpeedCurrent = 0x1E;
        }
        entity_add_speed_xz(0);
        return;
    case 0x68:
        player_hit_react_heavy();
        /* fallthrough */
    default:
        return;
    }
}

void player_input_to_behavior(void) {
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    s32 check_action_object_l() __asm__("check_action_object");
    s32 check_climb_object_l() __asm__("check_climb_object");
    void player_input_turn_only_l() __asm__("player_input_turn_only");
    s32 t;

    t = g_PadHeld & (PAD_FIRE | PAD_ACTION);
    if ((t == PAD_ACTION || t == (PAD_FIRE | PAD_ACTION)) && (g_PadPressed & PAD_ACTION)) {
        if (check_climb_object_l() != 0) {
            g_PlayerHitState = 0x80;
            g_PlayerSubState = 1;
            g_PlayerActionBehavior = 10;
            LIT(u16, g_UpdateFlags) &= ~0x40;
            LIT(s32, g_GameStateFlags) |= GAME_ON_BOX;
            return;
        }
        if (check_action_object_l() != 0) {
            g_PlayerSubState = 1;
            g_PlayerActionBehavior = 12;
            LIT(u16, g_UpdateFlags) &= ~0x40;
            g_PlayerCollisionFlags |= 0x80;
            return;
        }
        if (g_PlayerHasEnterSwitchZone & 0x20) {
            LIT(u16, g_UpdateFlags) &= ~0x40;
            g_PlayerHitState = 0x80;
            g_PlayerSubState = 1;
            LIT(s16, g_PlayerActionBehavior) = 0x11;
            if (g_GameStateFlags & GAME_LADDER) {
                LIT(s16, g_PlayerActionBehavior) = 0xB;
            }
            return;
        }
    }
    if (g_GameStateFlags & GAME_ON_BOX) {
        player_input_turn_only_l();
        return;
    }
    if (g_GameStateFlags & GAME_PUSHING) {
        g_PlayerSubState = 1;
        g_PlayerActionBehavior = 0x10;
        LIT(u16, g_UpdateFlags) &= ~0x40;
        return;
    }
    if (g_PlayerHasEnterSwitchZone & 0x20) {
        LIT(u16, g_UpdateFlags) &= ~0x40;
        g_PlayerHitState = 0x80;
        g_PlayerSubState = 1;
        LIT(s16, g_PlayerActionBehavior) = 0x11;
        if (g_GameStateFlags & GAME_LADDER) {
            LIT(s16, g_PlayerActionBehavior) = 0xB;
        }
        return;
    }
    if ((g_PadHeld & PAD_AIM) && g_PlayerBehaviorFlags != 0 && g_PlayerBehaviorFlags < 11) {
        g_PlayerSubState = 3;
        if (g_PlayerBehaviorFlags == 1) {
            g_PlayerSubState = 4;
        }
        g_PlayerActionBehavior = 0x12;
        return;
    }
    switch (g_PadHeld & PAD_DIRECTIONS) {
    case 1:
        g_PlayerActionBehavior_u8 = 1;
        if (!(g_PadHeldPrev & PAD_FORWARD)) {
            g_PlayerActionState = 0;
        }
        break;
    case 3:
        g_PlayerActionBehavior_u8 = 2;
        if (!(g_PadHeldPrev & PAD_FORWARD)) {
            g_PlayerActionState = 0;
        }
        break;
    case 9:
        g_PlayerActionBehavior_u8 = 3;
        if (!(g_PadHeldPrev & PAD_FORWARD)) {
            g_PlayerActionState = 0;
        }
        break;
    case 2:
        if (g_PlayerActionBehavior_u8 != 4) {
            g_PlayerActionState = 0;
        }
        g_PlayerActionBehavior_u8 = 4;
        break;
    case 8:
        if (g_PlayerActionBehavior_u8 != 5) {
            g_PlayerActionState = 0;
        }
        g_PlayerActionBehavior_u8 = 5;
        break;
    case 6:
        g_PlayerActionBehavior_u8 = 6;
        if (!(g_PadHeldPrev & PAD_BACK)) {
            g_PlayerActionState = 0;
        }
        break;
    case 12:
        g_PlayerActionBehavior_u8 = 7;
        if (!(g_PadHeldPrev & PAD_BACK)) {
            g_PlayerActionState = 0;
        }
        break;
    case 4:
        g_PlayerActionBehavior_u8 = 8;
        if (!(g_PadHeldPrev & PAD_BACK)) {
            g_PlayerActionState = 0;
        }
        break;
    case 5:
    case 7:
    case 10:
    case 11:
        break;
    }
}

void player_behavior_00_idle(void) {
    s32 joint_move_a12(s32, s32, s32, s32) __asm__("joint_move");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerAnimationId = 0;
        g_PlayerActionTicksCounter = 0x64;
        g_PlayerHitState = 0;
        g_PlayerMoveSpeedCurrent = 0;
        /* fallthrough */
    case 1:
        joint_move_a12(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
        if ((--g_PlayerActionTicksCounter << 16) == 0 && (g_UpdateFlags & 0x100)) {
            g_PlayerAnimationFrameId = 0;
            g_PlayerTimingControl = 0;
            g_PlayerAnimationId = 0;
            g_PlayerBlendCounter = 3;
            g_PlayerActionState += 1;
        }
        break;
    case 2:
        if (joint_move_a12(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
            g_PlayerAnimationId = 1;
            g_PlayerAnimationFrameId = 0;
            g_PlayerTimingControl = 0;
            g_PlayerBlendCounter = 3;
            g_PlayerActionState += 1;
        }
        break;
    case 3:
        joint_move_a12(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        break;
    }
}

void player_ctrl_behavior_walk(void) {
    s32 joint_move_l() __asm__("joint_move");
    void pad_vibrate_large_ramp_l() __asm__("pad_vibrate_large_ramp");
    /* per character (Chris, Jill): {frame A, frame B, dip A, dip B} - the walk speed is lowered during
       those animation frames */
    u8 tbl[2][4] = { { 21, 23, 13, 14 }, { 20, 22, 15, 15 } };
    u8 *t;
    u8 *q;
    u8 *p;

    if (!(g_PadHeld & PAD_FORWARD)) {
        g_PlayerActionState = 2;
    }
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerAnimationId = 2;
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        g_PlayerActionTicksCounter = 0;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep(0, 0);
        }
    case 1:
        if (g_PadHeld & PAD_RUN) {
            g_PlayerSubState = 1;
            g_PlayerActionBehavior = 0xD;
        }
        if (g_PlayerActionTicksCounter == 0) {
            if (ENT(g_CurrentEntity)->animation_frame_id == 8) {
                sound_play_footstep(0, 0);
                if (g_StatusFlags & STATUS_IN_WATER) {
                    pad_vibrate_large_ramp_l(0x14, 0, 0x46, 0);
                    pad_vibrate_large_ramp_l(0xA, 0x46, 0, 0x14);
                }
            }
            if (ENT(g_CurrentEntity)->animation_frame_id == 0x16) {
                sound_play_footstep(0, -4);
                if (g_StatusFlags & STATUS_IN_WATER) {
                    pad_vibrate_large_ramp_l(0x14, 0, 0x46, 0);
                    pad_vibrate_large_ramp_l(0xA, 0x46, 0, 0x14);
                }
            }
        }
        g_PlayerMoveSpeedCurrent = 0x5D;
        p = (u8 *)g_CurrentEntity;
        t = tbl[g_PlayerId & 1];
        if ((u8)(ENT(p)->animation_frame_id - t[0]) < 7) {
            g_PlayerMoveSpeedCurrent -= t[2];
        }
        if (ENT(p)->animation_frame_id >= 7 && ENT(p)->animation_frame_id < 14) {
            g_PlayerMoveSpeedCurrent -= t[2];
            q = t;
        } else {
            q = t;
        }
        if ((u8)(ENT(p)->animation_frame_id - q[1]) < 3) {
            g_PlayerMoveSpeedCurrent -= q[3];
        }
        if (ENT(p)->animation_frame_id >= 9 && ENT(p)->animation_frame_id < 12) {
            g_PlayerMoveSpeedCurrent -= t[3];
        }
        if (g_StatusFlags & STATUS_IN_WATER) {
            g_PlayerMoveSpeedCurrent /= 2;
            if (g_PlayerActionTicksCounter-- == 0) {
                joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
                g_PlayerActionTicksCounter = 1;
            }
        } else {
            joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
            g_PlayerActionTicksCounter = 0;
        }
        break;
    case 2:
        g_PlayerActionBehavior = 0;
        sound_play_footstep(0, 0);
        break;
    }
    entity_add_speed_xz(0);
}

void player_ctrl_behavior_turn(void) {
    s32 joint_move_a13(s32, s32, s32, s32) __asm__("joint_move");
    if (!(g_PadHeld & (PAD_RIGHT | PAD_LEFT))) {
        g_PlayerActionState = 2;
    }
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerAnimationId = 2;
        g_PlayerActionState = 1;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerHitState = 0;
        g_PlayerMoveSpeedCurrent = 1;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep(0, 0);
        }
        /* fallthrough */
    case 1:
        if (CURRENT_ENTITY_LIT->animation_frame_id == 8) {
            sound_play_footstep(0, 0);
        }
        if (CURRENT_ENTITY_LIT->animation_frame_id == 0x16) {
            sound_play_footstep(0, -4);
        }
        joint_move_a13(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        return;
    case 2:
        g_PlayerActionBehavior = 0;
        sound_play_footstep(0, 0);
        return;
    }
}

void player_input_turn_only(void) {
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    switch (g_PadHeld & PAD_DIRECTIONS) {
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12:
        break;
    case 2:
        if (g_PlayerEntity.action_behavior != 4) {
            g_PlayerEntity.action_state = 0;
        }
        g_PlayerActionBehavior_u8 = 4;
        break;
    case 8:
        if (g_PlayerEntity.action_behavior != 5) {
            g_PlayerEntity.action_state = 0;
        }
        g_PlayerActionBehavior_u8 = 5;
        break;
    }
}

void player_behavior_09_nop(void) {
}

void player_behavior_0c_interact(void) {
    s32 joint_move_a14() __asm__("joint_move");
    extern u8 * g_pRoomActionEntry_u8p __asm__("g_pRoomActionEntry");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 4;
        g_PlayerAnimationFrameId = 0;
        g_PlayerBlendCounter = 3;
        /* fallthrough */
    case 1:
        if (joint_move_a14(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
            if (*g_pRoomActionEntry_u8p == 0xD) {
                LIT(u32, g_GameStateFlags) |= GAME_FILE_PICKUP;
            } else {
                LIT(u32, g_GameStateFlags) |= MENU_REQUEST(MENU_MODE_PICKUP);
            }
            g_PlayerAnimationFrameId = 0;
            g_PlayerAnimationId = 0;
            g_PlayerBlendCounter = 0;
            g_PlayerActionState += 1;
            LIT(u16, g_UpdateFlags) |= 0x40;
            g_PlayerCollisionFlags &= 0x7F;
        }
        break;
    case 2:
        joint_move_a14(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
        g_PlayerSubState = 0;
        g_PlayerActionBehavior = 0;
        break;
    }
}

void player_ctrl_behavior_walk_back(void) {
    extern s8 g_PlayerHitState_l __asm__("g_PlayerHitState");
    extern u8 g_PlayerAnimationId_l __asm__("g_PlayerAnimationId");
    extern u8 *g_CurrentEntity_l __asm__("g_CurrentEntity");
    s32 joint_move_l() __asm__("joint_move");
    void pad_vibrate_large_ramp_l(s32, s32, s32, s32) __asm__("pad_vibrate_large_ramp");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    s32 entity_check_view_and_distance_l(s32, s32, void *) __asm__("entity_check_view_and_distance");
    void entity_add_speed_xz_l(s32) __asm__("entity_add_speed_xz");
    u8 *p;
    u8 n;
    s32 found;
    u8 v;

    if (!(g_PadHeld & PAD_BACK)) {
        g_PlayerActionState = 2;
    }
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        g_PlayerHitState_l = 0;
        g_PlayerActionTicksCounter = 0;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep_l(0, 0);
        }
        /* fallthrough */
    case 1:
        p = LITP(u8, g_EnemiesList);
        n = g_EnemyCount;
        found = 0;
        if (n != 0) {
        loop:
            if (!(FIELD_AT(p, Entity, ->status_flags, u8) & 1)) {
                p += ENTITY_SIZE;
                goto next;
            }
            n--;
            if (ENT(p)->health >= 0 && !(FIELD_AT(p, Entity, ->behavior_flags, u8) & 0x80)) {
                found |= entity_check_view_and_distance_l(0x200, 0x1F40, &ENT(p)->pos_x);
            }
            p += ENTITY_SIZE;
            /* FAKEMATCH: empty asm = an insn that emits no code. It keeps jump2's cross-jumping from merging
             * the two `p += ENTITY_SIZE` (the original keeps both; this shape is unique in the binary).
             * No natural C construct found that has the same effect. */
            __asm__("");
        next:
            if (n & 0xFF) {
                goto loop;
            }
        }
        if (!(found & 0xFF)) {
            if (g_PlayerAnimationId_l != 3) {
                g_PlayerAnimationId_l = 3;
                g_PlayerAnimationFrameId = 0;
                g_PlayerTimingControl = 0;
                g_PlayerBlendCounter = 3;
            }
            if (g_PlayerActionTicksCounter == 0 && (FIELD_AT(g_CurrentEntity_l, Entity, ->animation_frame_id, u8) == 8 || FIELD_AT(g_CurrentEntity_l, Entity, ->animation_frame_id, u8) == 0x16)) {
                sound_play_footstep_l(0, 0);
                if (g_StatusFlags & STATUS_IN_WATER) {
                    pad_vibrate_large_ramp_l(0xF, 0, 0x46, 0);
                    pad_vibrate_large_ramp_l(0xA, 0x46, 0, 0xF);
                }
            }
            g_PlayerMoveSpeedCurrent = 0x3C;
            v = FIELD_AT(g_CurrentEntity_l, Entity, ->animation_frame_id, u8);
            if (v >= 5 || v < 8) {     /* always true; reproduces the original code */
                g_PlayerMoveSpeedCurrent = 0x40;
            }
        } else {
            if (g_PlayerAnimationId_l != 2) {
                g_PlayerAnimationId_l = 2;
                g_PlayerAnimationFrameId = 0;
                g_PlayerTimingControl = 0;
                g_PlayerBlendCounter = 3;
            }
            if ((FIELD_AT(g_CurrentEntity_l, Entity, ->animation_frame_id, u8) == 7 || FIELD_AT(g_CurrentEntity_l, Entity, ->animation_frame_id, u8) == 0x1B) && FIELD_AT(g_CurrentEntity_l, Entity, ->timing_control, u8) == 1
                && g_PlayerActionTicksCounter == 0) {
                sound_play_footstep_l(0, -4);
                if (g_StatusFlags & STATUS_IN_WATER) {
                    pad_vibrate_large_ramp_l(0xF, 0, 0x46, 0);
                    pad_vibrate_large_ramp_l(0xA, 0x46, 0, 0xF);
                }
            }
            g_PlayerMoveSpeedCurrent = 0x3C;
        }
        if (g_StatusFlags & STATUS_IN_WATER) {
            g_PlayerMoveSpeedCurrent = g_PlayerMoveSpeedCurrent / 2;
            if (g_PlayerActionTicksCounter-- == 0) {
                joint_move_l(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
                g_PlayerActionTicksCounter = 1;
            }
        } else {
            joint_move_l(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
        }
        break;
    case 2:
        g_PlayerActionBehavior = 0;
        sound_play_footstep_l(0, 0);
        break;
    }
    entity_add_speed_xz_l(0x800);
}

void player_behavior_0d_run(void) {
    s32 joint_move_a15() __asm__("joint_move");
    s32 check_action_object_a1() __asm__("check_action_object");
    s32 check_climb_object_a1() __asm__("check_climb_object");
    s32 sound_play_footstep_a4() __asm__("sound_play_footstep");
    s32 entity_add_speed_xz();
    extern s8 g_PlayerSubState_s8 __asm__("g_PlayerSubState");
    extern u8 g_PlayerDeathTimer_u8 __asm__("g_PlayerDeathTimer");
    extern u8 g_PlayerAnimationFrameId_u8 __asm__("g_PlayerAnimationFrameId");
    extern u16 g_PlayerMoveSpeedCurrent_u16 __asm__("g_PlayerMoveSpeedCurrent");
    s32 unused;
    u16 pad;
    u16 ticks;
    s32 start_frame;
    u32 stop_frame;
    u8 stop_timer;

    if (LIT(s32, g_GameStateFlags) & GAME_PUSHING) {
        g_PlayerSubState_s8 = 1;
        g_PlayerActionBehavior = 0x10;
        return;
    }
    if (g_PadPressed & PAD_ACTION) {
        if (check_climb_object_a1() != 0) {
            g_PlayerHitState = 0x80;
            g_PlayerSubState_s8 = 1;
            g_PlayerActionBehavior = 0xA;
            LIT(u16, g_UpdateFlags) &= 0xFFBF;
            LIT(s32, g_GameStateFlags) |= GAME_ON_BOX;
            return;
        }
        if (check_action_object_a1() != 0) {
            g_PlayerSubState_s8 = 1;
            g_PlayerActionBehavior = 0xC;
            LIT(s16, g_UpdateFlags) = (u16) (LIT(s32, g_UpdateFlags) & 0xFFBF);
            g_PlayerCollisionFlags |= 0x80;
            return;
        }
        if (g_PlayerHasEnterSwitchZone & 0x20) {
            g_PlayerHitState = 0x80;
            g_PlayerSubState_s8 = 1;
            LIT(s16, g_UpdateFlags) = (u16) (LIT(s32, g_UpdateFlags) & 0xFFBF);
            LIT(s16, g_PlayerActionBehavior) = 0x11;
            if (LIT(s32, g_GameStateFlags) & GAME_LADDER) {
                LIT(s16, g_PlayerActionBehavior) = 0xB;
            }
        } else {
            goto block_10;
        }
    } else {
block_10:
        if ((u8) g_PlayerActionState < 2U) {
            pad = g_PadHeld;
            if (!(pad & 1)) {
                g_PlayerActionState = 2;
            }
            if ((pad & 0x100) && (g_PlayerBehaviorFlags != 0)) {
                g_PlayerActionState = 2;
            }
        }
        g_PlayerAngle = (((g_PlayerAngle + (g_PlayerTurnSpeedRun * ((u32) (g_PadHeld & (PAD_FORWARD | PAD_RIGHT)) >> 1))) & 0xFFF) - (g_PlayerTurnSpeedRun * ((u32) (g_PadHeld & (PAD_FORWARD | PAD_LEFT)) >> 3))) & 0xFFF;
        switch (g_PlayerActionState) {
        case 0:
            g_PlayerMoveSpeedCurrent_u16 = 0xD2;
            start_frame = g_PlayerAnimationFrameId_u8;
            g_PlayerTimingControl = 1;
            g_PlayerAnimationId = 3;
            g_PlayerActionState = 1;
            g_PlayerBlendCounter = 3;
            g_PlayerHitState = 0;
            g_PlayerAnimationFrameId_u8 = 1;
            LIT(s32, sp_tmp00) = (s32) start_frame;
            if ((u32) (start_frame - 0xA) < 0xFU) {
                g_PlayerAnimationFrameId_u8 = 0xC;
            }
            g_PlayerActionTicksCounter = 0;
            if (g_StatusFlags & STATUS_IN_WATER) {
                sound_play_footstep_a4(1, 0);
            }
            /* fallthrough */
        case 1:
            if (g_PlayerActionTicksCounter == 0) {
                if (g_CurrentEntity->animation_frame_id == 0) {
                    sound_play_footstep_a4(1, 0);
                    if (g_StatusFlags & STATUS_IN_WATER) {
                        pad_vibrate_large_ramp(8, 0, 0x50, 0);
                        pad_vibrate_large_ramp(0xF, 0x50, 0, 8);
                    }
                }
                if (g_CurrentEntity->animation_frame_id == 0xA) {
                    sound_play_footstep_a4(1, -4);
                    if (g_StatusFlags & STATUS_IN_WATER) {
                        pad_vibrate_large_ramp(8, 0, 0x50, 0);
                        pad_vibrate_large_ramp(0xF, 0x50, 0, 8);
                    }
                }
            }
            g_PlayerMoveSpeedCurrent_u16 = 0xD2;
            if (g_StatusFlags & STATUS_IN_WATER) {
                ticks = (u16) g_PlayerActionTicksCounter;
                g_PlayerMoveSpeedCurrent_u16 = 0x69;
                g_PlayerActionTicksCounter = ticks - 1;
                if (ticks == 0) {
                    joint_move_a15(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
                    g_PlayerActionTicksCounter = 1;
                }
            } else {
                joint_move_a15(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
                g_PlayerActionTicksCounter = 0;
            }
            if (!(g_PadHeld & PAD_RUN)) {
                g_PlayerActionBehavior = 0x101;
                stop_frame = g_PlayerAnimationFrameId_u8;
                g_PlayerSubState_s8 = 0;
                g_PlayerAnimationFrameId_u8 = 0xA;
                LIT(s32, sp_tmp00) = (s32) stop_frame;
                if ((u32) (stop_frame - 1) < 0xBU) {
                    g_PlayerAnimationFrameId_u8 = 0x19;
                }
                g_PlayerAnimationId = 2;
                g_PlayerBlendCounter = 3;
            }
            break;
        case 2:
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerTimingControl = 0;
            g_PlayerAnimationId = 0;
            g_PlayerActionState = 3;
            g_PlayerBlendCounter = 3;
            g_PlayerDeathTimer_u8 = 0;
            /* fallthrough */
        case 3:
            joint_move_a15(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
            stop_timer = g_PlayerDeathTimer_u8 + 1;
            g_PlayerDeathTimer_u8 = stop_timer;
            if ((u32) (stop_timer & 0xFF) >= 4U) {
                g_PlayerSubState_s8 = 0;
                g_PlayerActionBehavior = 0;
                g_PlayerMoveSpeedCurrent_u16 = 0;
                sound_play_footstep_a4(1, 0);
            }
            g_PlayerMoveSpeedCurrent_u16 -= 0x1E;
            break;
        }
        entity_add_speed_xz(0);
    }
}

void player_behavior_nop(void) {
}

void player_behavior_0b_ladder(void) {
    long SquareRoot0(long a);
    s32 joint_move_l() __asm__("joint_move");
    s32 pad_vibrate_small_l() __asm__("pad_vibrate_small");
    s32 pad_vibrate_large_l() __asm__("pad_vibrate_large");
    s32 billboard_set_rect_l() __asm__("billboard_set_rect");
    extern u8 g_PlayerAnimationFrameId_g __asm__("g_PlayerAnimationFrameId");
    extern u16 g_PlayerPosY_h __asm__("g_PlayerPosY");
    u8 *p;
    s32 dx;
    s32 dz;
    s32 dir_neg;
    u16 prev;
    u16 unused2;
    u32 unused;

    switch (g_PlayerActionState) {
    case 0:
        g_PlayerAnimationId = 2;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId_g = 0;
        g_PlayerTimingControl = 0;
        g_PlayerMoveSpeedCurrent = 0x5D;
        g_PlayerActionState += 1;
        /* fallthrough */
    case 1:
        LIT(s32, sp_vec_vx) = (s32) ENTITY_FIELD(g_CurrentEntity, anim_base_x);
        LIT(s32, sp_vec_vz) = (s32) ENTITY_FIELD(g_CurrentEntity, anim_base_z);
        LIT(s32, sp_vec_vy) = 0;
        entity_rotate_toward_target(ADDR_sp_vec, 0x40);
        joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x200);
        entity_add_speed_xz(0);
        dx = g_PlayerPosX - g_PlayerAnimBaseX;
        dz = g_PlayerPosZ - g_PlayerAnimBaseZ;
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0x384) {
            goto inc;
        }
        return;
    case 2:
        joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x200);
        prev = g_PlayerAngle;
        if (prev & 0x400) {
            g_PlayerAngle = prev - (u8)(prev >> 2);
        } else {
            g_PlayerAngle = prev + (u8)(prev >> 2);
        }
        if (g_PlayerAngle & 0x3E0) {
            break;
        }
    inc:
        g_PlayerActionState += 1;
        return;
    case 3:
        g_PlayerAnimationFrameId_g = 0;
        g_PlayerTimingControl = 0;
        g_PlayerAnimationId = 0x33;
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            g_PlayerAnimationId = 0x35;
        }
        g_PlayerBlendCounter = 3;
        g_PlayerMoveSpeedCurrent = 0;
        g_PlayerActionState += 1;
        billboard_set_rect_l(ADDR_g_PlayerShadowOffsetX, 0x320, 0x2BC, 0x2BC, 0x2BC);
        /* fallthrough */
    case 4:
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            if (g_PlayerAnimationFrameId_g == 0xF) {
                g_PlayerFloorY = 0xA8C;
                LIT(s32, sp_tmp00) = -0x708;
                if ((s16) g_PlayerAngle >= 0x801) {
                    LIT(s32, sp_tmp00) = 0x708;
                }
                g_PlayerShadowOffsetZ += (u16) LIT(s32, sp_tmp00);
            }
            if (g_PlayerAnimationFrameId_g == 0x1A) {
                sound_play_se_3d(2, 0x17, 0, ADDR_g_PlayerPosX);
            }
            if (g_PlayerAnimationFrameId_g == 0x18) {
                pad_vibrate_small_l(2, 0);
                pad_vibrate_large_l(9, 0x78, 0);
                pad_vibrate_small_l(1, 3);
            }
        } else {
            if (g_PlayerAnimationFrameId_g == g_LadderStepFrames[g_PlayerMoveSpeedCurrent]) {
                sound_play_se_3d(2, 0x23, 0, ADDR_g_PlayerPosX);
                g_PlayerMoveSpeedCurrent = (u16) g_PlayerMoveSpeedCurrent + 1;
            }
            if (g_PlayerAnimationFrameId_g == 0x32) {
                sound_play_se_3d(2, 0x2D, 0, ADDR_g_PlayerPosX);
            }
        }
        g_PlayerActionState += joint_move_l(0, g_PlayerActionAnimHeader, g_PlayerActionAnimBase, 0x400);
        return;
    case 5:
        do {
            g_PlayerAnimationFrameId_g = 0;
            g_PlayerTimingControl = 0;
            g_PlayerBlendCounter = 0;
            g_PlayerMoveSpeedCurrent = 0;
            g_PlayerAnimationId += 1;
            joint_move_l(0, g_PlayerActionAnimHeader, g_PlayerActionAnimBase, 0x400);
            dir_neg = (s16) g_PlayerAngle < 0x801;
            LIT(s32, sp_tmp08) = -0x3E8;
        } while (0);
        if (dir_neg == 0) {
            LIT(s32, sp_tmp08) = 0x3E8;
        }
        g_PlayerPosY = 0;
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            LIT(s32, sp_tmp08) = -0x7D0;
            if (dir_neg == 0) {
                LIT(s32, sp_tmp08) = 0x7D0;
            }
            g_PlayerPosY = 0xA8C;
        }
        g_PlayerPosZ += LIT(s32, sp_tmp08);
        g_PlayerPositionVx = (u16) g_PlayerPosX;
        g_PlayerPositionVy = g_PlayerPosY_h;
        g_PlayerPositionVz = (u16) g_PlayerPosZ;
        billboard_set_rect_l(ADDR_g_PlayerShadowOffsetX, 0x1F4, 0x1F4, 0x2BC, 0x2BC);
        g_PlayerShadowOffsetX = 0;
        g_PlayerShadowOffsetZ = 0;
        g_PlayerFloorY = g_PlayerPosY_h;
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            g_PlayerActionState = 8;
            return;
        }
    case 6:
        g_PlayerActionState = 7;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationId = 2;
        g_PlayerAnimationFrameId_g = 0;
        g_PlayerTimingControl = 0;
        g_PlayerMoveSpeedCurrent = 0x3C;
        g_PlayerActionTicksCounter = 0xF;
        LIT(u8, g_PlayerEntity) |= 4;
        return;
    case 7:
        if (ENTITY_FIELD(g_CurrentEntity, animation_frame_id) == 8) {
            sound_play_footstep(0, 0);
        }
        p = LITP(u8, g_PlayerEntity);
        joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        entity_add_speed_xz(0);
        prev = g_PlayerActionTicksCounter;
        g_PlayerActionTicksCounter = prev - 1;
        if (prev == 0) {
            g_PlayerActionState = 8;
            *p &= 0xFB;
            sound_play_footstep(0, -4);
            return;
        }
        break;
    case 8:
        g_PlayerHitState = 0;
        g_PlayerState = 1;
        g_PlayerHasEnterSwitchZone &= 0xEF;
        LIT(s32, g_GameStateFlags) &= ~GAME_LADDER;
        LIT(u16, g_UpdateFlags) |= 0x40;
        break;
    }
}

typedef struct {
    u8 flags;
    u8 type;
    u8 status;
    u8 alive;
    u8 pad04[0x30];
    s32 x;
    s32 y;
    s32 z;
    u8 pad40[0x48];
    s16 hp;
    u8 pad8A[0x102];
} AimEntity;

s32 player_find_aim_target(void) {
    long SquareRoot0(long a);
    u8 aim_ray_blocked_by_wall_l(VECTOR *) __asm__("aim_ray_blocked_by_wall");
    VECTOR v;
    u8 best_pos;
    u8 best_neg;
    u32 dmin;
    u32 dpos;
    u32 dneg;
    u32 d;
    u8 idx;
    u8 i;
    u8 count;
    AimEntity *e;
    s32 *dist;
    s32 dx;
    s32 dz;

    i = 0;
    best_pos = 0;
    best_neg = 0;
    idx = 0;
    dmin = 0x7FFFFFFF;
    dpos = 0x7FFFFFFF;
    dneg = 0x7FFFFFFF;
    count = g_EnemyCount;
    e = LITP(AimEntity, g_EnemiesList);
    if (count != 0) {
        dist = LITP(s32, sp_tmp08);
        do {
            if (!(e->flags & 1)) {
                i++;
                e++;
                continue;
            }
            if (e->alive != 0 && e->hp >= 0 && !(e->status & 0xC0) && e->type < 0x13) {
                v.vx = dx = e->x - g_PlayerEntity.pos_x;
                dz = e->z - g_PlayerEntity.pos_z;
                v.vy = 0;
                v.vz = dz;
                *dist = SquareRoot0(dx * dx + dz * dz);
                if (turn_toward_target(&e->x, 0x800) == 0 && !aim_ray_blocked_by_wall_l(&v)) {
                    d = *dist;
                    if (d < dmin) {
                        dmin = d;
                        idx = i + 1;
                    }
                    if (e->y > 0) {
                        if (d < dpos) {
                            dpos = d;
                            best_pos = i + 1;
                        }
                    } else if (d < dneg) {
                        dneg = d;
                        best_neg = i + 1;
                    }
                }
            }
            i++;
            e++;
            count--;
        } while (count != 0);
    }
    if (idx == 0) {
        g_PlayerEntity.aim_target = ADDR_g_EnemiesList;
        return 0;
    }
    if (g_PlayerEntity.behavior_flags == 1) {
        g_PlayerEntity.aim_target = idx * ENTITY_SIZE + ADDR_g_EnemySlotBase1;
    } else {
        if (best_pos != 0) {
            g_PlayerEntity.aim_target = best_pos * ENTITY_SIZE + ADDR_g_EnemySlotBase1;
        }
        if (best_neg != 0) {
            g_PlayerEntity.aim_target = best_neg * ENTITY_SIZE + ADDR_g_EnemySlotBase1;
        }
    }
    return 1;
}

s32 player_next_aim_target(void) {
    u8 aim_ray_blocked_by_wall_l() __asm__("aim_ray_blocked_by_wall");
    s32 v[3];
    u8 i;
    u8 *p;
    s8 n;

    i = 0;
    p = (u8 *)g_PlayerEntity.aim_target;
    n = g_EnemyCount;
    while (p != LITP(u8, g_EnemiesList) + i * ENTITY_SIZE) {
        i++;
    }
    i++;
    p += ENTITY_SIZE;
    if (i == 30) {
        i = 0;
        p = LITP(u8, g_EnemiesList);
    }
    while (n != 0) {
        if (!(ENT(p)->status_flags & 1)) {
            i++;
            p += ENTITY_SIZE;
            if (i == 30) {
                i = 0;
                p = LITP(u8, g_EnemiesList);
            }
            continue;
        }
        if (ENT(p)->has_enter_switch_zone != 0 && ENT(p)->health >= 0 && !(ENT(p)->behavior_flags & 0xC0) && ENT(p)->id < 0x13) {
            v[0] = ENT(p)->pos_x - g_PlayerEntity.pos_x;
            v[2] = ENT(p)->pos_z - g_PlayerPosZ;
            v[1] = 0;
            if (aim_ray_blocked_by_wall_l(v) == 0) {
                g_PlayerAimTarget = (s32)p;
                return 1;
            }
        }
        i++;
        p += ENTITY_SIZE;
        n--;
    }
    return 0;
}

s32 aim_ray_blocked_by_wall(VECTOR *dir) {
    void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
    void VectorNormal(VECTOR *v0, VECTOR *v1);
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    VECTOR v1;
    VECTOR v2;
    u8 *obj;
    u16 *p;
    u16 a, b, c, d;
    s32 ax, az;
    s32 px, pz;

    obj = ((u8 *) ROOM_L->collision);
    p = BOUND_HDR(obj)->group[0];
    px = g_PlayerEntity.pos_x / 18;
    pz = g_PlayerEntity.pos_z / 18;
    ax = dir->vx / 18;
    az = dir->vz / 18;
    for (LIT(u16 *, sp_tmp2C) = BOUND_HDR(obj)->group[4]; p < LIT(u16 *, sp_tmp2C); p += 6) {
        a = p[0] / 18;
        b = p[1] / 18;
        c = p[2] / 18;
        d = p[3] / 18;
        if ((p[5] &= 0x300) != 0x300) {
            continue;
        }
        if (p[4] == 4 || p[4] == 5) {
            continue;
        }
        {
        s32 z0, z1, x0, x1;
        x0 = c;
        x1 = a;
        z0 = b;
        z1 = d;
        LIT(s32, sp_vec_vy) = 0;
        v1.vy = 0;
        v2.vy = 0;
        LIT(s32, sp_vec_vx) = x0 - x1;
        LIT(s32, sp_vec_vz) = z0 - z1;
        v1.vx = px + ax - x1;
        v1.vz = pz + az - z1;
        v2.vx = px - x1;
        v2.vz = pz - z1;
        OuterProduct0(LITP(VECTOR, sp_vec), &v1, &v1);
        OuterProduct0(LITP(VECTOR, sp_vec), &v2, &v2);
        if ((v1.vy ^ v2.vy) & 0x80000000) {
            v1.vx = x0 - px;
            v1.vz = z0 - pz;
            v1.vy = 0;
            OuterProduct0(dir, &v1, &v1);
            v2.vx = x1 - px;
            v2.vz = z1 - pz;
            v2.vy = 0;
            VectorNormal(&v2, &v2);
            OuterProduct0(dir, &v2, &v2);
            if ((v1.vy ^ v2.vy) & 0x80000000) {
                return 1;
            }
        }
        }
        {
        s32 z0, z1, x0, x1;
        x0 = a;
        x1 = c;
        z0 = b;
        z1 = d;
        LIT(s32, sp_vec_vy) = 0;
        v1.vy = 0;
        v2.vy = 0;
        LIT(s32, sp_vec_vx) = x0 - x1;
        LIT(s32, sp_vec_vz) = z0 - z1;
        v1.vx = px + ax - x1;
        v1.vz = pz + az - z1;
        v2.vx = px - x1;
        v2.vz = pz - z1;
        OuterProduct0(LITP(VECTOR, sp_vec), &v1, &v1);
        OuterProduct0(LITP(VECTOR, sp_vec), &v2, &v2);
        if ((v1.vy ^ v2.vy) & 0x80000000) {
            v1.vx = x0 - px;
            v1.vz = z0 - pz;
            v1.vy = 0;
            OuterProduct0(dir, &v1, &v1);
            v2.vx = x1 - px;
            v2.vz = z1 - pz;
            v2.vy = 0;
            OuterProduct0(dir, &v2, &v2);
            if ((v1.vy ^ v2.vy) & 0x80000000) {
                return 1;
            }
        }
        }
    }
    return 0;
}

void billboard_init(SVECTOR *pos, u8 *bb, s16 half_w, s16 half_d) {
    u_short GetClut(int x, int y);
    u_short GetTPage(int tp, int abr, int x, int y);
    void SetPolyFT4(POLY_FT4 *p);
    void SetSemiTrans(void *p, int abe);
    SVECTOR v;
    POLY_FT4 *poly;

    poly = (POLY_FT4 *)(bb + 8);
    FIELD_AT(bb, Billboard, ->poly[0].r0, s32) = LIT(s32, sp_tmp1C);
    SetPolyFT4(poly);
    SetSemiTrans(poly, 1);
    poly->tpage = GetTPage(1, 2, 0x180, 0x100);
    poly->clut = GetClut(0, 0x1E7);
    poly->u0 = 0x51;
    poly->v0 = 0xC8;
    poly->u1 = 0x6B;
    poly->v1 = 0xC8;
    poly->u2 = 0x51;
    poly->v2 = 0xE5;
    poly->u3 = 0x6B;
    poly->v3 = 0xE5;
    v.vx = half_w * -1;
    v.vy = 0;
    v.vz = half_d;
    BILLBOARD(bb)->corners[0] = v;
    v.vx = v.vx * -1;
    BILLBOARD(bb)->corners[1] = v;
    v.vx = v.vx * -1;
    v.vz = v.vz * -1;
    BILLBOARD(bb)->corners[2] = v;
    v.vx = v.vx * -1;
    BILLBOARD(bb)->corners[3] = v;
    *(SVECTOR *)bb = *pos;
    mem_copy(bb + 0x30, poly, 0x28);
}

void billboard_set_color(u8 *bb, s32 semi, s32 unused, s32 color) {
    u_short GetTPage(int tp, int abr, int x, int y);
    void SetSemiTrans(void *p, int abe);
    s32 rgb;
    u16 tpage;
    s32 mask;

    tpage = GetTPage(1, 2, 0x180, 0x100);
    mask = 0xFF000000;
    FIELD_AT(bb, Billboard, ->poly[0].tpage, u16) = tpage;
    FIELD_AT(bb, Billboard, ->poly[1].tpage, u16) = tpage;
    SetSemiTrans(bb + 8, semi);
    SetSemiTrans(bb + 0x30, semi);
    rgb = color & 0xFFFFFF;
    FIELD_AT(bb, Billboard, ->poly[0].r0, s32) = rgb | (FIELD_AT(bb, Billboard, ->poly[0].r0, s32) & mask);
    FIELD_AT(bb, Billboard, ->poly[1].r0, s32) = rgb | (FIELD_AT(bb, Billboard, ->poly[1].r0, s32) & mask);
}

void billboard_adj_size(void *bb, s32 dx, s32 dz) {
    BILLBOARD(bb)->corners[0].vx = (u16) (((u16) BILLBOARD(bb)->corners[0].vx) - dx);
    BILLBOARD(bb)->corners[0].vz = (u16) (((u16) BILLBOARD(bb)->corners[0].vz) + dz);
    BILLBOARD(bb)->corners[1].vx = (u16) (((u16) BILLBOARD(bb)->corners[1].vx) + dx);
    BILLBOARD(bb)->corners[1].vz = (u16) (((u16) BILLBOARD(bb)->corners[1].vz) + dz);
    BILLBOARD(bb)->corners[2].vx = (u16) (((u16) BILLBOARD(bb)->corners[2].vx) - dx);
    BILLBOARD(bb)->corners[2].vz = (u16) (((u16) BILLBOARD(bb)->corners[2].vz) - dz);
    BILLBOARD(bb)->corners[3].vx = (u16) (((u16) BILLBOARD(bb)->corners[3].vx) + dx);
    BILLBOARD(bb)->corners[3].vz = (u16) (((u16) BILLBOARD(bb)->corners[3].vz) - dz);
}

void billboard_set_size(void *bb, s16 half_w, int half_d) {
  s16 unused;
  s16 neg_w;
  neg_w = -half_w;
  *((s16 *) (((s8 *) bb) + 0x5C)) = half_d;
  *((s16 *) (((s8 *) bb) + 0x64)) = half_d;
  ;
  *((s16 *) (((s8 *) bb) + 0x58)) = neg_w;
  *((s16 *) (((s8 *) bb) + 0x60)) = half_w;
  *((s16 *) (((s8 *) bb) + 0x68)) = neg_w;
  *((s16 *) (((s8 *) bb) + 0x6C)) = -half_d;
  *((s16 *) (((s8 *) bb) + 0x70)) = half_w;
  *((s16 *) (((s8 *) bb) + 0x74)) = -half_d;
}

void billboard_set_rect(void *bb, s16 x_pos, s16 x_neg, s16 z_pos, s32 z_neg) {
    s16 x_min;
    s32 z_min;

    x_min = -x_neg;
    BILLBOARD(bb)->corners[0].vx = x_min;
    FIELD_AT(bb, Billboard, ->corners[0].vz, s16) = z_pos;
    FIELD_AT(bb, Billboard, ->corners[1].vx, s16) = x_pos;
    FIELD_AT(bb, Billboard, ->corners[1].vz, s16) = z_pos;
    FIELD_AT(bb, Billboard, ->corners[2].vx, s16) = x_min;
    FIELD_AT(bb, Billboard, ->corners[3].vx, s16) = x_pos;
    z_min = -z_neg;
    FIELD_AT(bb, Billboard, ->corners[2].vz, s16) = (s16) z_min;
    FIELD_AT(bb, Billboard, ->corners[3].vz, s16) = (s16) z_min;
}

void billboard_draw(VECTOR *pos, u8 *obj, s16 y, s16 ang) {
    MATRIX *RotMatrixY(long r, MATRIX *m);
    void SetRotMatrix(MATRIX *m);
    void SetTransMatrix(MATRIX *m);
    long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *flag);
    void AddPrim(void *ot, void *p);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    MATRIX m;
    MATRIX ls;
    s32 unused[2];
    long p;
    long flag;
    u32 z;

    m = g_IdentityMatrix_l;
    m.t[0] = *(s16 *)obj + pos->vx;
    m.t[1] = y;
    m.t[2] = ((s16) BILLBOARD(obj)->offset_z) + pos->vz;
    RotMatrixY(ang, &m);
    GsMulCoord0(&g_WorldScreenMatrix, &m, &ls);
    SetRotMatrix(&ls);
    SetTransMatrix(&ls);
    z = RotAverage4((SVECTOR *)(obj + 0x58), (SVECTOR *)(obj + 0x60), (SVECTOR *)(obj + 0x68),
                    (SVECTOR *)(obj + 0x70), (long *)(LIT(u8, g_ActiveBuffer) * 0x28 + (s32)obj + 0x10),
                    (long *)(LIT(u8, g_ActiveBuffer) * 0x28 + (s32)obj + 0x18),
                    (long *)(LIT(u8, g_ActiveBuffer) * 0x28 + (s32)obj + 0x20),
                    (long *)(LIT(u8, g_ActiveBuffer) * 0x28 + (s32)obj + 0x28), &p, &flag);
    if (g_StatusFlags & STATUS_SHADOW_DEPTH_SORT) {
        if (z >= 0xFF0) {
            z = 0xFF0;
        }
        AddPrim((u32 *)LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer) + 2].org + (z >> 4),
                obj + (LIT(u8, g_ActiveBuffer) * 0x28 + 8));
    } else {
        AddPrim((u32 *)LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer) + 4].org + 1,
                obj + (LIT(u8, g_ActiveBuffer) * 0x28 + 8));
    }
}

void billboard_reserve(s32 count) {
    LIT(s32, g_LoadPointer) += (count & 0xFF) * 0x78;
}

void player_hit_react_light(void) {
    s32 joint_move_a16(s32, s32, s32, s32) __asm__("joint_move");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerAnimationId = 1;
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        g_PlayerMoveSpeedCurrent = 0;
        sound_play_se_3d(3, 0, 0, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 1:
        if (joint_move_a16(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400) != 0) {
            g_PlayerState = 1;
            g_PlayerHitState = 0;
        }
        return;
    }
}

void player_hit_react_damage(void) {
    s32 joint_move_a17(s32, s32, s32, s32) __asm__("joint_move");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerMoveSpeedCurrent = 0xFA;
        g_PlayerAnimationId = g_PlayerHitState + 0xFF;
        sound_play_se_3d(3, 0, 0, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 1:
        if (joint_move_a17(0, g_PlayerGrabAnimHeader, g_PlayerGrabAnimBase, 0x400) != 0) {
            g_PlayerState = 1;
            g_PlayerHitState = 0;
        }
        return;
    }
}

void player_hit_react_heavy(void) {
    s32 joint_move_a18(s32, s32, s32, s32) __asm__("joint_move");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        g_PlayerMoveSpeedCurrent = 0;
        g_PlayerAnimationId = g_PlayerHitState + 0xFF;
        sound_play_se_3d(3, 3, 0, ADDR_g_PlayerPosX);
        /* fallthrough */
    case 1:
        if (joint_move_a18(0, g_PlayerGrabAnimHeader, g_PlayerGrabAnimBase, 0x400) != 0) {
            g_PlayerState = 1;
            g_PlayerHitState = 0;
        }
        return;
    }
}

void player_behavior_10_push(void) {
    s32 joint_move_a19(s32, s32, s32, s32) __asm__("joint_move");
    s32 pad_vibrate_small_a1(s32, s32) __asm__("pad_vibrate_small");
    void pad_vibration_reset();
    s32 sound_play_se_3d_a7(s32, s32, s32, s32) __asm__("sound_play_se_3d");
    s32 entity_add_speed_xz_a4(s32) __asm__("entity_add_speed_xz");
    extern u8 g_PlayerAnimationId_u8 __asm__("g_PlayerAnimationId");
    extern u8 g_PlayerAnimationFrameId_u8 __asm__("g_PlayerAnimationFrameId");
    s32 se_id;

    switch (g_PlayerActionState) {
    case 0:
        g_PlayerAnimationId_u8 = 0x30;
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId_u8 = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        /* fallthrough */
    case 1:
        if (joint_move_a19(0, g_PlayerActionAnimHeader, g_PlayerActionAnimBase, 0x400) != 0) {
            g_PlayerActionState = 2;
            g_PlayerTimingControl = 0;
            g_PlayerBlendCounter = 3;
            g_PlayerMoveSpeedCurrent = 0;
            g_PlayerAnimationId_u8 += 1;
            return;
        }
        return;
    case 2:
        joint_move_a19(0, g_PlayerActionAnimHeader, g_PlayerActionAnimBase, 0x400);
        if ((u8) g_PlayerAnimationFrameId_u8 < 0x10U) {
            g_PlayerMoveSpeedCurrent = 0x32;
            entity_add_speed_xz_a4(0);
        }
        if (!(g_GameStateFlags & GAME_PUSHING)) {
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerTimingControl = 0;
            g_PlayerActionState = 3;
            g_PlayerBlendCounter = 3;
            g_PlayerMoveSpeedCurrent = 0;
            g_PlayerAnimationId_u8 += 1;
            pad_vibration_reset();
        }
        if (g_PlayerAnimationFrameId_u8 == 1) {
            se_id = 0x16;
            if (ENT(g_PushObject)->id & 0x40) {
                se_id = 0x17;
            }
            sound_play_se_3d_a7(2, se_id, 0, ADDR_g_PlayerPosX);
            pad_vibrate_small_a1(1, 0);
            pad_vibrate_large(3, 0x78, 0);
            pad_vibrate_large_ramp(0xF, 0x78, 0, 3);
            return;
        }
        break;
    case 3:
        if (joint_move_a19(0, g_PlayerActionAnimHeader, g_PlayerActionAnimBase, 0x400) != 0) {
            g_PlayerState = 1;
            LIT(u16, g_UpdateFlags) |= 0x40;
        }
        break;
    }
}

void player_climb_sequence(void) {
    s32 joint_move_l() __asm__("joint_move");
    s32 billboard_set_rect_l() __asm__("billboard_set_rect");
    s32 sound_play_se_3d_l() __asm__("sound_play_se_3d");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    extern u8 g_PlayerAnimationFrameId_l __asm__("g_PlayerAnimationFrameId");
    extern u8 g_RoomId_l __asm__("g_RoomId");
    extern u8 *g_CurrentEntity_l __asm__("g_CurrentEntity");
    u8 *obj;
    u8 *p;
    u16 v;
    s32 f;

    obj = g_PlayerJoints;
    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.animation_id = 2;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0;
        joint_move_l(0, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        if (g_GameStateFlags & GAME_ON_BOX) {
            v = g_PlayerEntity.angle;
            if (v & 0x200) {
                g_PlayerAngle = v + (u8)(v >> 2);
            } else {
                g_PlayerAngle = v - (u8)(v >> 2);
            }
            LIT(s32, g_StatusFlags) |= STATUS_BOX_CLIMB;
        }
        if (g_StatusFlags & STATUS_STAIRS_ZONE) {
            if (g_PlayerEntity.has_enter_switch_zone & 0x40) {
                p = g_CurrentEntity_l;
                v = ENT(p)->angle;
                if (!(v & 0x400)) {
                    ENT(p)->angle = v + (u8)(v >> 2);
                } else {
                    ENT(p)->angle = v - (u8)(v >> 2);
                }
            } else {
                p = g_CurrentEntity_l;
                v = ENT(p)->angle;
                if (v & 0x400) {
                    ENT(p)->angle = v + (u8)(v >> 2);
                } else {
                    ENT(p)->angle = v - (u8)(v >> 2);
                }
            }
        }
        v = g_PlayerEntity.angle;
        if (!(v & 0x3E0)) {
            g_PlayerEntity.action_state = 1;
            g_PlayerAngle = v & 0xFF00;
        }
        break;
    case 1:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0x33;
        if (g_PlayerEntity.has_enter_switch_zone & 0x10) {
            g_PlayerEntity.animation_id = 0x35;
        }
        g_PlayerActionState = 2;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.move_speed_current = 0;
        billboard_set_rect_l(ADDR_g_PlayerShadowOffsetX, 0x320, 0x2BC, 0x2BC, 0x2BC);
        /* fallthrough */
    case 2:
        if (g_GameStateFlags & GAME_ON_BOX) {
            if (g_PlayerEntity.animation_frame_id == g_PlayerEntity.move_speed_current * 15 + 12) {
                sound_play_se_3d_l(2, 0x23, 0, ADDR_g_PlayerPosX);
                g_PlayerEntity.move_speed_current++;
            }
            if (g_PlayerMoveSpeedCurrent == 3) {
                g_PlayerMoveSpeedCurrent = 7;
            }
            if (g_PlayerMoveSpeedCurrent == 7 && g_PlayerAnimationFrameId_l == 0x35) {
                sound_play_se_3d_l(2, 0x23, 0, ADDR_g_PlayerPosX);
            }
            if ((g_PlayerEntity.has_enter_switch_zone & 0x10) && g_PlayerMoveSpeedCurrent == 2) {
                g_PlayerEntity.move_speed_current = 5;
            }
            if (g_PlayerMoveSpeedCurrent >= 5 && g_PlayerEntity.animation_frame_id == (g_PlayerMoveSpeedCurrent - 5) * 9 + 0x2C) {
                g_PlayerEntity.move_speed_current = 6;
                sound_play_footstep_l(0, 0);
            }
        } else {
            if (g_PlayerEntity.animation_frame_id == g_PlayerEntity.move_speed_current * 15 + 12) {
                sound_play_se_3d_l(2, 0x2D, 0, ADDR_g_PlayerPosX);
                g_PlayerEntity.move_speed_current++;
            }
        }
        LIT(s16, sp_svec_vx) = 0;
        LIT(s16, sp_svec_vz) = 0;
        SCRATCH->sv_y = -0x29;
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            SCRATCH->sv_y = 0x29;
        }
        if (g_RoomId_l == 0xE || g_RoomId_l == 5) {
            SCRATCH->sv_y = -0x24;
            if (g_PlayerHasEnterSwitchZone & 0x10) {
                SCRATCH->sv_y = 0x24;
            }
        }
        if (g_GameStateFlags & GAME_ON_BOX) {
            g_PlayerScdEntityFlags |= 0x40;
        }
        g_PlayerEntity.shadow.offset_x = FIELD_AT(obj, Joint, ->world.t[0], u16) - g_PlayerEntity.pos_x;
        g_PlayerEntity.floor_y += ((u16) SCRATCH->sv_y);
        g_PlayerEntity.shadow.offset_z = FIELD_AT(obj, Joint, ->world.t[2], u16) - g_PlayerEntity.pos_z;
        g_PlayerEntity.action_state += joint_move_l(0, g_PlayerWork.action_anim_header, g_PlayerWork.action_anim_base, 0x400);
        break;
    case 3:
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 0;
        g_PlayerEntity.move_speed_current = 0;
        g_PlayerEntity.animation_id++;
        joint_move_l(0, g_PlayerWork.action_anim_header, g_PlayerWork.action_anim_base, 0x400);
        LIT(s32, sp_tmp08) = 0;
        LIT(s32, sp_tmp00) = g_PlayerActionTicksCounter * 4350;
        if (g_PlayerAngle & 0x400) {
            LIT(s32, sp_tmp00) = 0;
            LIT(s32, sp_tmp08) = g_PlayerActionTicksCounter * 4350;
        }
        LIT(s32, sp_tmp04) = -0xB45;
        if (g_PlayerHasEnterSwitchZone & 0x10) {
            LIT(s32, sp_tmp04) = 0xB45;
        }
        if (g_RoomId_l == 0xE || g_RoomId_l == 5) {
            LIT(s32, sp_tmp00) = g_PlayerActionTicksCounter * 2114;
            LIT(s32, sp_tmp08) = 0;
            if (g_PlayerAngle & 0x400) {
                LIT(s32, sp_tmp00) = 0;
                LIT(s32, sp_tmp08) = g_PlayerActionTicksCounter * 2114;
            }
            LIT(s32, sp_tmp04) = -0x57A;
            if (g_PlayerHasEnterSwitchZone & 0x10) {
                LIT(s32, sp_tmp04) = 0x57A;
            }
        }
        f = LIT(s32, g_GameStateFlags);
        if (f & 0x80) {
            LIT(s32, sp_tmp08) = 0;
            LIT(s32, sp_tmp00) = -g_PlayerActionTicksCounter * 1850;
            if (g_PlayerEntity.angle & 0x400) {
                LIT(s32, sp_tmp00) = 0;
                LIT(s32, sp_tmp08) = g_PlayerActionTicksCounter * 1850;
            }
            LIT(s32, sp_tmp04) = -0x708;
            if (g_PlayerEntity.has_enter_switch_zone & 0x10) {
                LIT(s32, sp_tmp04) = 0x708;
                LIT(s32, g_GameStateFlags) = f & ~GAME_ON_BOX;
                g_PlayerEntity.scd_entity_flags &= 0xFFBF;
            }
            LIT(s32, g_StatusFlags) &= ~STATUS_BOX_CLIMB;
        }
        g_PlayerPosX += SCRATCH->tmp00;
        g_PlayerPosY += SCRATCH->tmp04;
        g_PlayerPosZ += SCRATCH->tmp08;
        g_PlayerEntity.position_vx = (u16)g_PlayerPosX;
        g_PlayerEntity.position_vy = (u16)g_PlayerPosY;
        g_PlayerEntity.position_vz = (u16)g_PlayerPosZ;
        billboard_set_rect_l(ADDR_g_PlayerShadowOffsetX, 0x1F4, 0x1F4, 0x2BC, 0x2BC);
        g_PlayerEntity.shadow.offset_x = 0;
        g_PlayerEntity.shadow.offset_z = 0;
        g_PlayerEntity.hit_state = 0;
        g_PlayerState = 1;
        g_PlayerEntity.floor_y = g_PlayerEntity.pos_y;
        LIT(u16, g_UpdateFlags) |= 0x40;
        LIT(s32, g_StatusFlags) &= ~STATUS_STAIRS_ZONE;
        break;
    }
}
