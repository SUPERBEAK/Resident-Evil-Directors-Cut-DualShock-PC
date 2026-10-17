/* player_scd.c - Player behaviours driven by room scripts (SCD state 8). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void load_equipped_weapon_animation(s32 weapon, s32 joint_index, s32 buf, u32 *anim_object);
void entity_apply_walk_speed(u16 speed);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
void entity_update_weapon_joint(u8 limb);


void player_state_08_scd(void) {
    extern s32 (*g_PlayerScdBehaviors[])();
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    void entity_update_weapon_joint_i(s32) __asm__("entity_update_weapon_joint");
    u16 f;
    s32 t;

    g_PlayerScdBehaviors[g_PlayerEntity.action_behavior]();
    if (g_PlayerEntity.scd_entity_flags & 2) {
        g_PlayerScdBehaviors[g_PlayerActionBehavior_u8]();
    }
    f = g_PlayerEntity.scd_entity_flags;
    if (f & 4) {
        t = f & 8;
        entity_update_weapon_joint_i(t != 0);
    }
}

void player_scd_behavior_00(void) {
    s32 load_equipped_weapon_animation_a2() __asm__("load_equipped_weapon_animation");
    s32 joint_move_a28() __asm__("joint_move");
    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.blend_counter = 3;
        /* fallthrough */
    case 1:
        joint_move_a28(g_PlayerEntity.scd_entity_flags & 1, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
        break;
    case 2:
        load_equipped_weapon_animation_a2(g_PlayerEntity.behavior_flags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
        g_PlayerEntity.action_state = 6;
        joint_move_a28(0, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
        break;
    case 3:
        load_equipped_weapon_animation_a2(g_PlayerEntity.behavior_flags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
        g_PlayerEntity.action_state = 6;
        joint_move_a28(0, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        break;
    case 4:
        load_equipped_weapon_animation_a2(g_PlayerEntity.behavior_flags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
        g_PlayerEntity.action_state = 6;
        joint_move_a28(0, g_PlayerWork.action_anim_header, g_PlayerWork.action_anim_base, 0x400);
        break;
    case 5:
        load_equipped_weapon_animation_a2(g_PlayerEntity.behavior_flags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
        g_PlayerEntity.action_state = 6;
        joint_move_a28(0, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x400);
        break;
    case 6:
        break;
    }
}

void player_scd_behavior_01(void) {
    s32 Flg_on(s32, u8);
    s32 joint_move_l() __asm__("joint_move");
    extern u8 g_PlayerAnimationId_l __asm__("g_PlayerAnimationId");

    switch (g_PlayerEntity.action_state) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 7;
        g_PlayerEntity.move_speed_current = 0;
        if (g_PlayerEntity.scd_entity_flags & 0x20) {
            g_PlayerEntity.blend_counter = 0;
        }
        if (g_PlayerAnimationId_l >= 0x3E) {
            g_PlayerActionState = 4;
            g_PlayerAnimationId_l -= 0x3E;
        } else if (g_PlayerAnimationId_l >= 0x10) {
            g_PlayerActionState = 3;
        } else {
            g_PlayerActionState = g_ScdAnimChainTbl[g_PlayerAnimationId_l][0] + 1;
            g_PlayerAnimationId_l = g_ScdAnimChainTbl[g_PlayerAnimationId_l][1];
        }
        g_PlayerActionTicksCounter = 1;
        break;
    case 1:
        if ((g_PlayerEntity.scd_entity_flags & 0x80) && g_PlayerEntity.action_ticks_counter-- == 0) {
            g_PlayerActionTicksCounter = 1;
            break;
        }
        if (joint_move_l(g_PlayerEntity.scd_entity_flags & 1, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x200)) {
            g_PlayerActionState = 5;
            g_PlayerAngle += g_PlayerScdTimer;
        }
        break;
    case 2:
        if ((g_PlayerEntity.scd_entity_flags & 0x80) && g_PlayerEntity.action_ticks_counter-- == 0) {
            g_PlayerActionTicksCounter = 1;
            break;
        }
        if (joint_move_l(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x200)) {
            g_PlayerEntity.action_state = 5;
            g_PlayerAngle += g_PlayerEntity.scd_timer;
            g_PlayerAnimationId_l += 5;
        }
        break;
    case 3:
        if ((g_PlayerEntity.scd_entity_flags & 0x80) && g_PlayerEntity.action_ticks_counter-- == 0) {
            g_PlayerActionTicksCounter = 1;
            break;
        }
        if (joint_move_l(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.action_anim_header, g_PlayerWork.action_anim_base, 0x200)) {
            g_PlayerActionState = 5;
            g_PlayerAngle += g_PlayerScdTimer;
        }
        break;
    case 4:
        if ((g_PlayerEntity.scd_entity_flags & 0x80) && g_PlayerEntity.action_ticks_counter-- == 0) {
            g_PlayerActionTicksCounter = 1;
            break;
        }
        if (joint_move_l(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.grab_anim_header, g_PlayerWork.grab_anim_base, 0x200)) {
            g_PlayerEntity.action_state = 5;
            g_PlayerAngle += g_PlayerEntity.scd_timer;
            g_PlayerAnimationId_l += 0x3E;
        }
        break;
    case 5:
        Flg_on(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
        g_PlayerEntity.scd_timer = 0;
        if (g_PlayerEntity.scd_entity_flags & 0x10) {
            g_PlayerEntity.action_state = 0;
        }
        break;
    }
}

void player_scd_behavior_02(void) {
    void entity_add_speed_xz(s32);
    s32 joint_move_a29() __asm__("joint_move");
    s32 Flg_on(s32, u8);
    s32 turn_toward_target();
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u8 act;
    void *unused;
    void *e;
    void *e2;
    void *e3;
    void *e4;

    e = LIT(void *, g_CurrentEntity);
    act = FIELD_AT(e, Entity, ->action_state, u8);
    switch (act) {
    case 0:
        FIELD_AT(e, Entity, ->animation_frame_id, u8) = 0U;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 2;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        e2 = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e2, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e2, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        CURRENT_ENTITY_LIT->angle += turn_toward_target(ADDR_sp_vec, ((s16) g_PlayerEntity.scd_timer));
        joint_move_a29(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        if (turn_toward_target(ADDR_sp_vec, 0x16A) == 0) {
            CURRENT_ENTITY_LIT->action_state = 2U;
            return;
        }
        return;
    case 2:
        FIELD_AT(e, Entity, ->animation_frame_id, u8) = 0U;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 2;
        CURRENT_ENTITY_LIT->action_state = 3U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 3:
        if (CURRENT_ENTITY_LIT->animation_frame_id == 8) {
            sound_play_footstep(0, 0);
        }
        if (CURRENT_ENTITY_LIT->animation_frame_id == 0x16) {
            sound_play_footstep(0, -4);
        }
        entity_apply_walk_speed(0x5D);
        e3 = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e3, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e3, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        entity_rotate_toward_target(ADDR_sp_vec, ((s16) g_PlayerEntity.scd_timer));
        joint_move_a29(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        entity_add_speed_xz(0);
        e4 = LIT(void *, g_CurrentEntity);
        dx = FIELD_AT(e4, Entity, ->pos_x, s32) - FIELD_AT(e4, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e4, Entity, ->pos_z, s32) - FIELD_AT(e4, Entity, ->anim_base_z, u16);
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0x96) {
            Flg_on(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
            if (!(g_PlayerEntity.collision_flags & 0x80)) {
                CURRENT_ENTITY_LIT_FIELD_AS(state, s32) = 1;
            }
        }
        break;
    }
}

void player_scd_behavior_03(void) {
    void joint_move_a30(s32, s32, s32, s32) __asm__("joint_move");
    void Flg_on_v() __asm__("Flg_on");
    s32 turn_toward_target(void *, s32);
    void entity_add_speed_xz(s32);
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u16 cnt;
    u8 *e1;
    u8 *e2;
    u8 *e3;
    u8 *e4;
    u8 *e5;
    u8 *e6;
    u8 *e7;

    switch (ENT(g_CurrentEntity)->action_state) {
    case 0:
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 2;
        LIT(u8 *, g_CurrentEntity)[0x87] = 1;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        /* fallthrough */
    case 1:
        e1 = LIT(u8 *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = FIELD_AT(e1, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = FIELD_AT(e1, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        *(u16 *)(LIT(u8 *, g_CurrentEntity) + 0x74) += turn_toward_target(LITP(void, sp_vec), ((s16) g_PlayerEntity.scd_timer));
        joint_move_a30(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        if (turn_toward_target(LITP(void, sp_vec), 0x16A) == 0) {
            LIT(u8 *, g_CurrentEntity)[0x87] = 2;
        }
        break;
    case 2:
        e2 = LIT(u8 *, g_CurrentEntity);
        FIELD_AT(e2, Entity, ->move_speed_current, u16) = 0xD2;
        ENT(e2)->animation_frame_id = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 3;
        LIT(u8 *, g_CurrentEntity)[0x87] = 3;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        /* fallthrough */
    case 3:
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 0) {
            sound_play_footstep(1, 0);
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 0xA) {
            sound_play_footstep(1, -4);
        }
        e3 = LIT(u8 *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = FIELD_AT(e3, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = FIELD_AT(e3, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        entity_rotate_toward_target(LITP(void, sp_vec), ((s16) g_PlayerEntity.scd_timer));
        joint_move_a30(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        entity_add_speed_xz(0);
        e4 = LIT(u8 *, g_CurrentEntity);
        dx = FIELD_AT(e4, Entity, ->pos_x, s32) - FIELD_AT(e4, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e4, Entity, ->pos_z, s32) - FIELD_AT(e4, Entity, ->anim_base_z, u16);
        if (SquareRoot0(dx * dx + dz * dz) < 0xFA) {
            if (g_PlayerEntity.collision_flags & 0x80) {
                Flg_on_v(LITP(void, g_SysFlags), g_PlayerEntity.scd_anim_param);
                break;
            }
            LIT(u8 *, g_CurrentEntity)[0x87] = 4;
        }
        break;
    case 4:
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 0;
        LIT(u8 *, g_CurrentEntity)[0x87] = 5;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        *(u16 *)(LIT(u8 *, g_CurrentEntity) + 0xC4) = 0;
        /* fallthrough */
    case 5:
        e5 = LIT(u8 *, g_CurrentEntity);
        joint_move_a30(g_PlayerEntity.scd_entity_flags & 1, FIELD_AT(e5, Entity, ->anim_header, s32), FIELD_AT(e5, Entity, ->anim_base, s32), 0x400);
        e6 = LIT(u8 *, g_CurrentEntity);
        cnt = FIELD_AT(e6, Entity, ->action_ticks_counter, u16) + 1;
        FIELD_AT(e6, Entity, ->action_ticks_counter, u16) = cnt;
        if ((s16)cnt >= 4) {
            ENT(e6)->action_state = 6;
        }
        e7 = LIT(u8 *, g_CurrentEntity);
        FIELD_AT(e7, Entity, ->move_speed_current, u16) -= 0x1E;
        entity_add_speed_xz(0);
        break;
    case 6:
        FIELD_AT(g_CurrentEntity, Entity, ->state, s32) = 1;
        Flg_on_v(LITP(void, g_SysFlags), g_PlayerEntity.scd_anim_param);
        break;
    }
}

void player_scd_behavior_04(void) {
    void entity_add_speed_xz(s32);
    s32 joint_move_a31(s32, s32, s32, s32) __asm__("joint_move");
    void Flg_on_v() __asm__("Flg_on");
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u8 act;
    u8 frame;
    u8 frame2;
    void *e;
    void *e2;
    void *e3;

    act = g_CurrentEntity->action_state;
    switch (act) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter = 3;
        g_CurrentEntity->animation_id = 3;
        /* fallthrough */
    case 1:
        frame = g_CurrentEntity->animation_frame_id;
        if ((frame == 8) || (frame == 0x16)) {
            sound_play_footstep(0, 0);
        }
        g_PlayerMoveSpeedCurrent = 0x3C;
        frame2 = g_CurrentEntity->animation_frame_id;
        if ((frame2 >= 5U) || (frame2 < 8U)) {
            g_PlayerMoveSpeedCurrent = 0x40;
        }
        e = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        FIELD_AT(e, Entity, ->angle, u16) = (u16) ((FIELD_AT(e, Entity, ->angle, u16) + 0x800) & 0xFFF);
        entity_rotate_toward_target(ADDR_sp_vec, g_PlayerScdTimer);
        e2 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e2, Entity, ->angle, u16) = (u16) (FIELD_AT(e2, Entity, ->angle, u16) - 0x800);
        joint_move_a31(g_PlayerScdEntityFlags & 1, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
        entity_add_speed_xz(0x800);
        e3 = LIT(void *, g_CurrentEntity);
        dx = FIELD_AT(e3, Entity, ->pos_x, s32) - FIELD_AT(e3, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e3, Entity, ->pos_z, s32) - FIELD_AT(e3, Entity, ->anim_base_z, u16);
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0x64) {
            Flg_on_v(ADDR_g_SysFlags, g_PlayerScdAnimParam);
            if (!(g_PlayerCollisionFlags & 0x80)) {
                CURRENT_ENTITY_LIT_FIELD_AS(state, s32) = 1;
            }
        }
        return;
    }
}

void player_scd_behavior_05(void) {
    s32 joint_move_a32(s32, s32, s32, s32) __asm__("joint_move");
    s32 Flg_on(s32, u8);
    s32 sound_play_footstep_a14(s32, s32) __asm__("sound_play_footstep");
    s32 entity_rotate_toward_target_a8(s32, s16) __asm__("entity_rotate_toward_target");
    s32 entity_add_speed_xz_a7(s32) __asm__("entity_add_speed_xz");
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u8 act;
    u8 frame;
    void *e;
    void *e2;
    void *e3;

    act = g_CurrentEntity->action_state;
    switch (act) {
    case 0:
        g_PlayerEntity.action_state = 1;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_CurrentEntity->animation_id = 2;
        g_PlayerEntity.move_speed_current = 0x1D;
        /* fallthrough */
    case 1:
        frame = g_CurrentEntity->animation_frame_id;
        if (((frame == 7) || (frame == 0x1B)) && (((u8) g_CurrentEntity->timing_control) == 2)) {
            sound_play_footstep_a14(0, -4);
        }
        e = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        FIELD_AT(e, Entity, ->angle, u16) = (u16) ((FIELD_AT(e, Entity, ->angle, u16) + 0x800) & 0xFFF);
        entity_rotate_toward_target_a8(ADDR_sp_vec, g_PlayerEntity.scd_timer);
        e2 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e2, Entity, ->angle, u16) = (u16) (FIELD_AT(e2, Entity, ->angle, u16) - 0x800);
        joint_move_a32(g_PlayerEntity.scd_entity_flags & 1, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
        entity_add_speed_xz_a7(0x800);
        e3 = LIT(void *, g_CurrentEntity);
        dx = FIELD_AT(e3, Entity, ->pos_x, s32) - FIELD_AT(e3, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e3, Entity, ->pos_z, s32) - FIELD_AT(e3, Entity, ->anim_base_z, u16);
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0x64) {
            Flg_on(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
            if (!(g_PlayerEntity.collision_flags & 0x80)) {
                CURRENT_ENTITY_LIT_FIELD_AS(state, s32) = 1;
            }
        }
        return;
    }
}

void player_scd_behavior_06(void) {
    s32 joint_move_a33() __asm__("joint_move");
    s32 Flg_on(s32, u8);
    s32 turn_toward_target();
    u8 act;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    act = FIELD_AT(e, Entity, ->action_state, u8);
    switch (act) {
    case 0:
        FIELD_AT(e, Entity, ->animation_frame_id, s8) = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 2;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        e2 = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e2, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e2, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        entity_rotate_toward_target(ADDR_sp_vec, 0x38);
        joint_move_a33(g_PlayerEntity.scd_entity_flags & 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
        if (turn_toward_target(ADDR_sp_vec, ((s16) g_PlayerEntity.scd_timer)) == 0) {
            CURRENT_ENTITY_LIT->action_state = 2U;
            return;
        }
        return;
    case 2:
        FIELD_AT(e, Entity, ->state, s32) = 1;
        Flg_on(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
        break;
    }
}

void player_scd_behavior_07(void) {
    s32 joint_move_a34(s32, s32, s32, s32) __asm__("joint_move");
    s32 Flg_on(s32, u8);
    u8 act;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    act = ENT(e)->action_state;
    switch (act) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        e2 = LIT(void *, g_CurrentEntity);
        ENT(e2)->animation_id = (u8) (((u8) ENT(e2)->animation_id) + 0xFB);
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + joint_move_a34(g_PlayerScdEntityFlags & 1, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400));
        break;
    case 2:
        Flg_on(ADDR_g_SysFlags, g_PlayerScdAnimParam);
        break;
    }
    g_PlayerAngle += g_PlayerScdTimer;
}

void player_scd_behavior_08(void) {
    s32 joint_move_a35(s32, s32, s32, s32) __asm__("joint_move");
    s32 Flg_on(s32, u8);
    u8 act;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    act = ENT(e)->action_state;
    switch (act) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        e2 = LIT(void *, g_CurrentEntity);
        ENT(e2)->animation_id = (u8) (((u8) ENT(e2)->animation_id) + 0xFB);
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + joint_move_a35(0, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400));
        return;
    case 2:
        Flg_on(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
        return;
    }
}

void player_scd_behavior_09(void) {
    s32 joint_move_a36() __asm__("joint_move");
    s32 Flg_on_k() __asm__("Flg_on");
    u8 act;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    act = ENT(e)->action_state;
    switch (act) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        e2 = LIT(void *, g_CurrentEntity);
        ENT(e2)->animation_id = (u8) (((u8) ENT(e2)->animation_id) + 0xFB);
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        g_CurrentEntity->action_state = (u8) (g_CurrentEntity->action_state + joint_move_a36(1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400));
        return;
    case 2:
        FIELD_AT(e, Entity, ->state, s32) = 1;
        Flg_on_k(ADDR_g_SysFlags, g_PlayerEntity.scd_anim_param);
        return;
    }
}
