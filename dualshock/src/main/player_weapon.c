/* player_weapon.c - Player weapons: aiming, firing, auto-aim, reloading, the knife, and their animation steps. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
u8 apply_weapon_damage(s32 weapon);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
s16 get_angle_towards_target(s16 x, s16 z);
void rearrange_item_slots(void);
u32 get_item_slot(s32 item_id);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
void player_clip_eject_start(void);
void pad_vibrate_small(s32 duration, s32 delay);
void pad_vibrate_large(s32 duration, s32 vol, s32 delay);
s32 pad_vibrate_large_ramp(s32 duration, s32 vol_start, s32 vol_end, s32 delay);
s32 player_find_aim_target(void);
s32 player_next_aim_target(void);
void billboard_set_color(u8 *bb, s32 semi, s32 unused, s32 color);
void billboard_adj_size(void *bb, s32 dx, s32 dz);


void player_behavior_12_gun_aim(void) {
    extern s8 g_PlayerActionBehavior_g __asm__("g_PlayerActionBehavior");
    extern s8 g_PlayerHitState_g __asm__("g_PlayerHitState");
    extern u8 g_PlayerAnimationFrameId_g __asm__("g_PlayerAnimationFrameId");
    s32 joint_move_l() __asm__("joint_move");
    u16 player_find_aim_target_l() __asm__("player_find_aim_target");
    void sound_play_footstep_l() __asm__("sound_play_footstep");
    s32 turn_toward_target_l() __asm__("turn_toward_target");
    void entity_rotate_toward_target_l() __asm__("entity_rotate_toward_target");
    u8 *p;
    u16 x;
    s32 y;

    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 5;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId_g = 0;
        g_PlayerHitState_g = 0;
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerAimFlags = 0;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep_l(0, 0);
        }
        if (g_AutoAimEnabled != 0) {
            g_PlayerAimState = player_find_aim_target_l();
        }
        g_PlayerAimState |= 2;
        /* fallthrough */
    case 1:
        if (g_AutoAimEnabled != 0) {
            if (g_PadHeldRaw & PSX_L1) {
                g_PlayerAimState = 0;
            }
            p = LITP(u8, g_PlayerEntity);
            if (g_PlayerAimState == 3) {
                LIT(s32, sp_tmp1C) = ((g_PlayerId & 1) << 5) + 0xF0;
                if (turn_toward_target_l(g_PlayerAimTarget + 0x34, 0x200) == 0) {
                    LIT(s32, sp_tmp1C) = ((g_PlayerId & 1) << 5) + 0xC0;
                }
                if ((*(s32 *)p & 0xFF0100) == 0x20000) {
                    LIT(s32, sp_tmp1C) += 0x20;
                }
                entity_rotate_toward_target_l(g_PlayerAimTarget + 0x34, LIT(s32, sp_tmp1C));
            }
        }
        g_PlayerActionState += joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        x = g_PadHeld;
        if (x & 2) {
            *(s16 *)&g_PlayerAngle = (g_PlayerAngle + 0x48) - ((g_PlayerId & 1) * 0x10);
        }
        if (x & 8) {
            y = g_PlayerAngle - 0x48;
            *(s16 *)&g_PlayerAngle = y + ((g_PlayerId & 1) * 0x10);
        }
        if ((g_PlayerBehaviorFlags != 0xA) && ((!(g_PlayerId & 1) && (g_PlayerAnimationFrameId_g >= 5) && ((g_PlayerBehaviorFlags == 2) || ((u32)(g_PlayerBehaviorFlags - 4) < 2U))) || (g_PlayerAnimationFrameId_g >= 0xA))) {
            if (g_PadHeld & PAD_BACK) {
                g_PlayerActionBehavior_g = 0x15;
                g_PlayerAnimationId = 0xB;
                g_PlayerAimFlags &= 0x1F;
                g_PlayerActionState = 0;
                LIT(u8, g_PlayerEntity) &= 0x1F;
                g_PlayerAimFlags |= 0x20;
                return;
            }
            if (g_PadHeld & PAD_FORWARD) {
                g_PlayerActionBehavior_g = 0x15;
                g_PlayerAnimationId = 8;
                g_PlayerAimFlags &= 0x1F;
                g_PlayerActionState = 0;
                LIT(u8, g_PlayerEntity) &= 0x1F;
                g_PlayerAimFlags |= 0x80;
                return;
            }
        }
        break;
    case 2:
        *(s16 *)&g_PlayerActionBehavior_g = 0x13;
        break;
    }
}

void player_behavior_13_gun_raise(void) {
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
    void joint_move_h6(s32, s32, s32, s32) __asm__("joint_move");
    void sound_play_footstep_h6(s32, s32) __asm__("sound_play_footstep");
    s32 x;
    s32 y;
    s32 t;

    switch (g_PlayerActionState) {
    case 0:
        x = g_PlayerAimFlags;
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerBlendCounter_u8 = 3;
        g_PlayerMoveSpeedCurrent = 0;
        t = (u32) x >> 7;
        y = t * 2;
        y += t;
        y += 7;
        x &= 0x20;
        x = x != 0;
        y += x * 6;
        g_PlayerAnimationId = y;
    case 1:
        joint_move_h6(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        break;
    case 2:
        if (g_PlayerBlendCounter_u8 == 0) {
            y = g_PlayerBehaviorFlags;
            g_PlayerActionState = 3;
            g_PlayerBlendCounter_u8 = 7;
            g_PlayerTimingControl = 0;
            g_PlayerAnimationFrameId = 0;
            g_PlayerMoveSpeedCurrent = 1;
            g_PlayerAnimationId = (y - 1) * 3 + 5 + (g_PlayerAimFlags >> 7) + ((g_PlayerAimFlags & 0x20) >> 4);
            if (g_StatusFlags & STATUS_IN_WATER) {
                sound_play_footstep_h6(0, 0);
            }
        } else {
            joint_move_h6(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x200);
        }
        break;
    case 3:
        if (g_PlayerBlendCounter_u8 == 0) {
            x = g_PlayerAimFlags;
            g_PlayerActionState = 2;
            g_PlayerBlendCounter_u8 = 7;
            g_PlayerAnimationFrameId = 0;
            g_PlayerTimingControl = 0;
            g_PlayerMoveSpeedCurrent = 1;
            t = (u32) x >> 7;
            y = t * 2;
            y += t;
            y += 7;
            x &= 0x20;
            x = x != 0;
            y += x * 6;
            g_PlayerAnimationId = y;
        } else {
            joint_move_h6(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x200);
        }
        break;
    }
}

void player_behavior_13_gun_hold_input(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    extern u8 g_PlayerActionBehavior_u8 __asm__("g_PlayerActionBehavior");
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
    extern u8 g_PlayerAnimationId_u8 __asm__("g_PlayerAnimationId");
    u8 weapon_get_ammo(void);
    u8 weapon_has_ammo_item_l(void) __asm__("weapon_has_ammo_item");
    s32 Flg_ck_l(s32, s32) __asm__("Flg_ck");
    void pad_vibrate_large_ramp_l(s32, s32, s32, s32) __asm__("pad_vibrate_large_ramp");
    void sound_play_se_3d_l(s32, s32, s32, s32) __asm__("sound_play_se_3d");
    u8 old;
    u8 cur;
    u16 pad;
    s32 sel;
    s32 prev;
    s32 off;
    s32 t;
    s32 v;
    s32 h;

    old = g_PlayerAimFlags;
    pad = g_PadHeld;
    LIT(s32, sp_tmp1C) = old;
    g_PlayerAimFlags = (old & 0x1F) | 0x40;
    if (pad & 0x10) {
        g_PlayerAimFlags = (g_PlayerAimFlags & 0x1F) | 0x80;
    }
    if (pad & 0x20) {
        g_PlayerAimFlags = (g_PlayerAimFlags & 0x1F) | 0x20;
    }
    cur = g_PlayerAimFlags;
    sel = cur & 0xA0;
    if ((old & 0xA0) != sel) {
        do {
            g_PlayerActionBehavior_u8 = 0x15;
        } while (0);
        h = cur >> 7;
        v = h * 2;
        v += h;
        v += 5;
        t = cur & 0x20;
        t = t != 0;
        v += t * 6;
        g_PlayerActionState = 0;
        g_PlayerAnimationId_u8 = v;
    }
    if ((old & 0x40) && sel != 0) {
        do {
            g_PlayerActionBehavior_u8 = 0x15;
        } while (0);
        h = cur >> 7;
        v = h * 2;
        v += h;
        v += 5;
        t = cur & 0x20;
        t = t != 0;
        v += t * 6;
        g_PlayerActionState = 0;
        g_PlayerAnimationId_u8 = v;
    }
    prev = LIT(s32, sp_tmp1C);
    if ((prev & 0xA0) && (g_PlayerAimFlags & 0x40)) {
        g_PlayerActionBehavior_u8 = 0x16;
        g_PlayerActionState = 0;
        g_PlayerAnimationId_u8 = ((prev & 0x80) >> 7) * 3 + 5 + ((prev & 0x20) >> 5) * 6;
    }
    if (!(g_PadHeld & PAD_AIM)) {
        g_PlayerActionBehavior_u8 = 0x17;
        g_PlayerActionState = 0;
        return;
    }
    if (g_PadHeld & PAD_FIRE) {
        if (weapon_get_ammo()) {
            g_PlayerActionBehavior_u8 = 0x14;
            g_PlayerActionState = -(g_PlayerBehaviorFlags == 6) & 3;
            off = (g_PlayerBehaviorFlags - 2) * 10;
            pad_vibrate_small(*(s16 *)(g_FireVibrationTbl + off), 2);
            off = (g_PlayerBehaviorFlags - 2) * 10;
            pad_vibrate_large(*(s16 *)(g_FireVibrationTbl + off), 0xFA, 2);
            off = (g_PlayerBehaviorFlags - 2) * 10;
            pad_vibrate_large_ramp_l(*(s16 *)(g_FireVibrationTbl_RampFrames + off), *(s16 *)(g_FireVibrationTbl_RampFrom + off),
                            *(s16 *)(g_FireVibrationTbl_RampTo + off), *(s16 *)(g_FireVibrationTbl + off) + 2);
            return;
        }
        if (g_PadPressed & PAD_FIRE) {
            if (Flg_ck_l(ADDR_g_ScenarioFlags, 0x7A) == 0 || g_ItemSlotsPointer_u8p[g_EquippedSlot * 2 - 2] != 5) {
                sound_play_se_3d_l(1, 9, 0, ADDR_g_PlayerPosX);
            }
            if (weapon_has_ammo_item_l() && g_PlayerBehaviorFlags < 6) {
                *(u16 *)&g_PlayerActionBehavior_u8 = 0x18;
                return;
            }
        }
    }
    if ((g_PadTrigger & PSX_L1) && player_next_aim_target() != 0) {
        *(u16 *)&g_PlayerActionBehavior_u8 = 0x1A;
        return;
    }
    if (g_PadHeld & PAD_RIGHT) {
        g_PlayerAngle = ({ g_PlayerAngle + 0x48; }) - (g_PlayerId & 1) * 16;
        if (g_PlayerActionState < 2) {
            g_PlayerActionState = 2;
            g_PlayerBlendCounter_u8 = 0;
        }
    } else if (g_PadHeld & PAD_LEFT) {
        g_PlayerAngle = ({ g_PlayerAngle - 0x48; }) + (g_PlayerId & 1) * 16;
        if (g_PlayerActionState < 2) {
            g_PlayerActionState = 2;
            g_PlayerBlendCounter_u8 = 0;
        }
    } else if (g_PlayerActionState >= 2) {
        g_PlayerActionState = 0;
    }
}

void player_behavior_14_autoaim(void) {
    extern s32 (*player_fire_state_tbl[])();
    player_fire_state_tbl[g_PlayerEntity.action_state]();
}

void player_fire_start(void) {
    s32 player_shoot_weapon_a1() __asm__("player_shoot_weapon");
    s32 weapon_fire_camera_spark_a1(s32) __asm__("weapon_fire_camera_spark");
    s32 aim_down;
    s32 y;

    g_PlayerEntity.action_state = 1;
    g_PlayerEntity.blend_counter = 3;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.move_speed_current = 1;
    y = (g_PlayerAimFlags >> 7) * 3 + 6;
    do {
        aim_down = g_PlayerAimFlags & 0x20;
    } while (0);
    aim_down = aim_down != 0;
    g_PlayerAnimationId = y + aim_down * 6;
    weapon_fire_camera_spark_a1(aim_down);
    if (g_StatusFlags & STATUS_IN_WATER) {
        sound_play_footstep(1, 0);
    }
    player_shoot_weapon_a1();
}

typedef struct {
    u8 frame;
    u8 fx_type;
    u8 fx_variant;
    u8 pad3;
    s16 off_x;
    s16 off_y;
    s16 off_z;
} ShotEffect;
typedef struct {
    s32 hit_type;
    u8 frame;
    u8 se1;
    u8 se2;
    u8 pad7;
} FireHit;

void player_shoot_weapon(void) {
    extern FireHit g_WeaponFireHitTbl_l[] __asm__("g_WeaponFireHitTbl");
    extern ShotEffect g_WeaponMuzzleFlashFx_l[] __asm__("g_WeaponMuzzleFlashFx");
    extern ShotEffect g_WeaponShellFx_l[] __asm__("g_WeaponShellFx");
    extern ShotEffect g_WeaponMuzzleFlash2Fx_l[] __asm__("g_WeaponMuzzleFlash2Fx");
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    s32 joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void auto_aim_pitch_update(void);
    s32 weapon_get_ammo_l(void) __asm__("weapon_get_ammo");
    s32 effect_create_billboard_l(u8, u8, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    s32 apply_weapon_damage_l(s32) __asm__("apply_weapon_damage");
    s16 weapon;
    s32 k;
    s32 b;
    s32 r;
    u8 t;

    weapon = g_PlayerEntity.behavior_flags - 2;     /* weapon table index: the equipped item id - 2 */
    g_PlayerEntity.action_state += joint_move_l(0, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
    if (g_PlayerEntity.animation_frame_id == 2) {
        auto_aim_pitch_update();
    }
    if (g_PlayerEntity.animation_frame_id == g_WeaponMuzzleFlashFx_l[weapon].frame) {
        g_ItemSlotsPointer_l[g_EquippedSlot * 2 - 1]--;
        if (weapon == 8) {
            LIT(s32, sp_tmp0C) = (weapon_get_ammo_l() & 3) + 8;
            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlashFx_l[LIT(s32, sp_tmp0C)].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlashFx_l[LIT(s32, sp_tmp0C)].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlashFx_l[LIT(s32, sp_tmp0C)].off_z;
            effect_create_billboard_l(g_WeaponMuzzleFlashFx_l[LIT(s32, sp_tmp0C)].fx_type, g_WeaponMuzzleFlashFx_l[LIT(s32, sp_tmp0C)].fx_variant, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
        } else {
            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlashFx_l[weapon].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlashFx_l[weapon].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlashFx_l[weapon].off_z;
            effect_create_billboard_l(g_WeaponMuzzleFlashFx_l[weapon].fx_type, g_WeaponMuzzleFlashFx_l[weapon].fx_variant, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
            if (weapon == 2) {
                LIT(s32, sp_vec_vx) = 0x96;
                SCRATCH->vec_y = 0x17C;
                SCRATCH->vec_z = 0;
                effect_create_billboard_l(0x11, 3, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
            }
            if (weapon == 3) {
                LIT(s32, sp_vec_vx) = 0x96;
                SCRATCH->vec_y = 0x17C;
                SCRATCH->vec_z = 0;
                effect_create_billboard_l(0x11, 0xB, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
            }
        }
    }
    if (g_PlayerEntity.animation_frame_id == g_WeaponFireHitTbl_l[weapon].frame) {
        if (weapon < 6) {
            apply_weapon_damage_l(g_WeaponFireHitTbl_l[weapon].hit_type);
        }
        sound_play_se_3d(1, g_WeaponFireHitTbl_l[weapon].se1, 0, ADDR_g_PlayerPosX);
        sound_play_se_3d(1, g_WeaponFireHitTbl_l[weapon].se2, 0, ADDR_g_PlayerPosX);
    }
    if (g_PlayerEntity.animation_frame_id == g_WeaponShellFx_l[weapon].frame) {
        b = g_PlayerEntity.id & 1;
        LIT(s32, sp_vec_vx) = g_WeaponShellFx_l[weapon].off_x;
        SCRATCH->vec_y = g_WeaponShellFx_l[weapon].off_y + b * (1 - weapon) * 300;
        SCRATCH->vec_z = g_WeaponShellFx_l[weapon].off_z;
        if (weapon == 8) {
            LIT(s32, sp_vec_vx) = g_WeaponShellFx_l[8].off_x;
            SCRATCH->vec_y = g_WeaponShellFx_l[8].off_y + b * 500;
            SCRATCH->vec_z = g_WeaponShellFx_l[8].off_z;
        }
        r = effect_create_billboard_l(g_WeaponShellFx_l[weapon].fx_type, g_WeaponShellFx_l[weapon].fx_variant, -(weapon != 8) & 0x555, ADDR_g_PlayerCoordM00, LITP(s32, sp_vec), 0);
        LIT(s32, sp_tmp00) = r;
        if (weapon == 8) {
            LITP(Effect, g_EffectPool)[r].hdr04 = weapon;
        } else {
            LITP(Effect, g_EffectPool)[r].hdr07 = weapon;
        }
    }
    if (g_PlayerEntity.animation_frame_id == g_WeaponMuzzleFlash2Fx_l[weapon].frame) {
        if (weapon == 8) {
            LIT(s32, sp_tmp0C) = (weapon_get_ammo_l() & 3) + 8;
            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlash2Fx_l[LIT(s32, sp_tmp0C)].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlash2Fx_l[LIT(s32, sp_tmp0C)].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlash2Fx_l[LIT(s32, sp_tmp0C)].off_z;
            LIT(s32, sp_tmp10) = effect_create_billboard_l(g_WeaponMuzzleFlash2Fx_l[LIT(s32, sp_tmp0C)].fx_type, g_WeaponMuzzleFlash2Fx_l[LIT(s32, sp_tmp0C)].fx_variant, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
            LITP(Effect, g_EffectPool)[LIT(s32, sp_tmp10)].hdr04 = weapon;
        } else {
            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlash2Fx_l[weapon].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlash2Fx_l[weapon].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlash2Fx_l[weapon].off_z;
            LIT(s32, sp_tmp0C) = effect_create_billboard_l(g_WeaponMuzzleFlash2Fx_l[weapon].fx_type, g_WeaponMuzzleFlash2Fx_l[weapon].fx_variant, 0, g_PlayerEntity.joints + 0x70C, LITP(s32, sp_vec), 0);
            LITP(Effect, g_EffectPool)[LIT(s32, sp_tmp0C)].hdr04 = weapon;
        }
    }
    if (weapon == 1 && (g_PlayerEntity.animation_frame_id == 7 || g_PlayerEntity.animation_frame_id == 9)) {
        apply_weapon_damage_l(g_WeaponFireHitTbl_l[1].hit_type);
    }
    if (g_PlayerEntity.animation_frame_id > g_WeaponFireEndFrame[weapon] && !(g_PadHeld & PAD_AIM)) {
        ENTITY_ACTION_S16(&g_PlayerEntity) = 0x13;
    }
    if (g_PlayerEntity.animation_frame_id == g_WeaponFireEndFrame[weapon] - 2 && weapon == 1) {
        sound_play_se_3d(1, 0xB, 0, ADDR_g_PlayerPosX);
    }
    if (g_PlayerEntity.animation_frame_id > g_WeaponFireEndFrame[weapon] && weapon == 1) {
        t = g_PlayerWork.aim_flags;
        if (((t & 0xA0) && !(g_PadHeld & (PAD_FORWARD | PAD_BACK))) || ((t & 0x80) && !(g_PadHeld & PAD_FORWARD))
            || ((t & 0x20) && !(g_PadHeld & PAD_BACK))) {
            ENTITY_ACTION_S16(&g_PlayerEntity) = 0x13;
            g_PlayerWork.aim_flags = (g_PlayerWork.aim_flags & 0x1F) | 0x40;
        }
    }
}

void player_fire_end_click(void) {
    ENTITY_ACTION_S16(&g_PlayerEntity) = 0x13;
    if ((((InventorySlot *)((LIT(u8, g_EquippedSlot) * 2) + g_ItemSlotsPointer))[-1].count == 0) && ((Flg_ck(ADDR_g_ScenarioFlags, 0x7A) == 0) || (((InventorySlot *)((LIT(u8, g_EquippedSlot) * 2) + g_ItemSlotsPointer))[-1].item_id != 5))) {
        sound_play_se_3d(1, 9, 0, ADDR_g_PlayerPosX);
    }
}

void player_fire_reraise_start(void) {
    void player_fire_reraise();
    s32 aim_down;

    (*(s8 *)&g_PlayerEntity.action_state) = 4;
    g_PlayerEntity.blend_counter = 3;
    (*(s8 *)&g_PlayerEntity.animation_frame_id) = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.move_speed_current = 1;
    (*(u8 *)&g_PlayerEntity.animation_id) = ((g_PlayerWork.aim_flags >> 7) * 3) + 6 + ((aim_down = (g_PlayerWork.aim_flags & ({ 0x20; })) != 0) * 6);
    player_fire_reraise(aim_down);
}

void player_fire_reraise(void) {
    s32 joint_move_a20(s32, s32, s32, s32) __asm__("joint_move");
    g_PlayerEntity.action_state += joint_move_a20(0, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
}

void player_fire_hold_start(void) {
    s32 player_fire_hold_a1() __asm__("player_fire_hold");
    g_PlayerEntity.action_state = 6;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.blend_counter = 3;
    g_PlayerEntity.animation_id = ((u8) g_PlayerWork.aim_flags >> 7) + ((((u8) g_PlayerWork.aim_flags >> 4) & 2) + 0xE);
    player_fire_hold_a1();
}

void player_fire_hold(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    extern u8 g_PlayerAnimationFrameId_u8 __asm__("g_PlayerAnimationFrameId");
    s32 effect_create_billboard_l(u8, u8, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    s32 joint_move_l() __asm__("joint_move");
    u8 weapon_get_ammo(void);
    u16 pad;
    s32 old;
    u32 cur;
    s32 t;
    s32 a;
    s32 b;

    pad = g_PadHeld;
    SCRATCH->tmp1C = g_PlayerAimFlags;
    g_PlayerAimFlags = (g_PlayerAimFlags & 0x1F) | 0x40;
    if (pad & 0x10) {
        g_PlayerAimFlags = (g_PlayerAimFlags & 0x1F) | 0x80;
    }
    if (pad & 0x20) {
        g_PlayerAimFlags = (g_PlayerAimFlags & 0x1F) | 0x20;
    }
    if (weapon_get_ammo() != 0) {
        if (g_PlayerAnimationFrameId_u8 % 15 == 0) {
            sound_play_se_3d(1, 3, 0, LITP(void, g_PlayerPosX));
            sound_play_se_3d(1, 4, 0, LITP(void, g_PlayerPosX));
            pad_vibrate_large_ramp(0xC, 0xC8, 0x32, 0);
        }
        if (!(g_PlayerAnimationFrameId_u8 & 3)) {
            LIT(s32, sp_vec_vx) = 0x21C;
            SCRATCH->vec_y = 0x4EC;
            SCRATCH->vec_z = 0;
            effect_create_billboard_l(0xC, 0, 0, (s32)(g_PlayerJoints + 0x70C), LITP(s32, sp_vec), 0);
        }
        g_ItemSlotsPointer_u8p[g_EquippedSlot * 2 - 1]--;
    } else {
        sound_play_se_3d(1, 3, 0, LITP(void, g_PlayerPosX));
        g_PlayerActionBehavior = 0x13;
        return;
    }
    if ((SCRATCH->tmp1C & 0x40) && ((cur = g_PlayerAimFlags) & 0xA0)) {
        goto set;
    } else if (/* volatile: keeps the reload GCC's CSE (skip-blocks) would merge */
               ((old = ((volatile struct { char pad[0x1C]; s32 f; } *)ADDR_sp_tmp00)->f) & 0xA0) && ((cur = g_PlayerAimFlags) & 0x40)) {
        g_PlayerActionState = 8;
        a = ({ ((old & 0x80) >> 7) * 3 + 5; }) + ((old & 0x20) >> 5) * 6;
        b = cur | 1;
        goto store;
    } else if ((SCRATCH->tmp1C & 0xA0) != ((cur = g_PlayerAimFlags) & 0xA0)) {
set:
        g_PlayerActionState = 8;
        a = ({ (cur >> 7) * 3 + 5; }) + ({ t = cur & 0x20; t = t != 0; t; }) * 6;
        b = cur & 0xFE;
store:
        g_PlayerAnimationId = a;
        g_PlayerAimFlags = b;
    } else {
        joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        if (!(g_PadHeld & PAD_FIRE)) {
            g_PlayerActionBehavior = 0x13;
            return;
        }
        if (g_PadHeld & PAD_RIGHT) {
            g_PlayerAngle += 0x20;
        } else if (g_PadHeld & PAD_LEFT) {
            g_PlayerAngle -= 0x20;
        }
    }
}

void player_fire_nop(void) {
}

void player_fire_reverse_start(void) {
    void player_fire_reverse(void);
    g_PlayerEntity.action_state = 9;
    g_PlayerEntity.animation_frame_id = 0;
    g_PlayerEntity.timing_control = 0;
    g_PlayerEntity.blend_counter = 3;
    sound_play_se_3d(1, 6, 0, ADDR_g_PlayerPosX);
    player_fire_reverse();
}

void player_fire_reverse(void) {
    s32 joint_move_a21(s32, s32, s32, s32) __asm__("joint_move");
    g_PlayerEntity.action_state += joint_move_a21(g_PlayerWork.aim_flags | 1, g_PlayerWork.weapon_anim_header, g_PlayerWork.weapon_anim_base, 0x400);
    if (g_PadHeld & (PAD_FORWARD | PAD_BACK)) {
        g_PlayerEntity.action_state = 5;
    }
}

void player_fire_loop(void) {
    g_PlayerEntity.action_state = 5;
}

void player_behavior_15_gun_aim_change(void) {
    s32 joint_move_a22(s32, s32, s32, s32) __asm__("joint_move");
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerHitState = 0;
        g_PlayerMoveSpeedCurrent = 1;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep(0, 0);
        }
        /* fallthrough */
    case 1:
        g_PlayerActionState += joint_move_a22(LIT(s32, sp_tmp1C), g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400);
        if (g_PadHeld & (PAD_FORWARD | PAD_BACK)) {
        case 2:
            g_PlayerActionBehavior = 0x13;
        }
        return;
    }
}

void player_behavior_17_holster(void) {
    s32 joint_move_a23(s32, s32, s32, s32) __asm__("joint_move");
    if (g_PadHeld & PAD_RIGHT) {
        g_PlayerAngle += 0x50;
    }
    if (g_PadHeld & PAD_LEFT) {
        g_PlayerAngle -= 0x50;
    }
    if (g_PlayerActionState == 0) {
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 5;
        g_PlayerBlendCounter = 3;
        g_PlayerMoveSpeedCurrent = 1;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep(0, 0);
        }
    }
    if (joint_move_a23(1, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
        g_PlayerSubState = 0;
        g_PlayerActionBehavior = 0;
        g_PlayerMoveSpeedCurrent = 0;
    }
}

void player_behavior_18_reload(void) {
    s32 joint_move_a24() __asm__("joint_move");
    s32 sound_play_footstep_a8() __asm__("sound_play_footstep");
    extern s32 (*reload_fx_tbl[])();
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
    u16 msg_flags;
    u8 coll_flags;

    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 0xE;
        g_PlayerBlendCounter_u8 = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerCollisionFlags |= 0x80;
        LIT(u16, g_UpdateFlags) &= 0xFFBF;
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep_a8(0, 0);
        }
        /* fallthrough */
    case 1:
        reload_fx_tbl[g_PlayerBehaviorFlags]();
        if (joint_move_a24(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
            LIT(s16, g_PlayerActionBehavior) = 0x13;
            coll_flags = g_PlayerCollisionFlags & 0x7F;
            g_PlayerCollisionFlags = coll_flags;
            msg_flags = LIT(s32, g_UpdateFlags) | 0x40;
            LIT(s16, g_UpdateFlags) = msg_flags;
            if ((u8) g_PlayerBehaviorFlags >= 7U) {
                LIT(s16, g_PlayerActionBehavior) = 0x218;
                g_PlayerBlendCounter_u8 = 7;
                g_PlayerAnimationId = 7;
                g_PlayerCollisionFlags = coll_flags | 0x80;
                g_PlayerAnimationFrameId = 0;
                g_PlayerTimingControl = 0;
                LIT(s16, g_UpdateFlags) = (u16) (msg_flags & 0xFFBF);
                return;
            }
        }
        return;
    case 2:
        joint_move_a24(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x200);
        if (g_PlayerBlendCounter_u8 == 0) {
            g_PlayerActionBehavior = 0x13;
            g_PlayerCollisionFlags &= 0x7F;
            LIT(s16, g_UpdateFlags) = (u16) (LIT(s32, g_UpdateFlags) | 0x40);
        }
        break;
    }
}

void reload_fx_beretta(void) {
    s32 fire_ammo_volley_gate();
    s32 weapon_reload_from_stack_a1() __asm__("weapon_reload_from_stack");
    if ((((*(s32 *)&g_PlayerEntity.death_timer) & 0xFFFF0000) == 0x010A0000) && (fire_ammo_volley_gate() & 0xFF)) {
        player_clip_eject_start();
    }
    if (((*(s32 *)&g_PlayerEntity.death_timer) & 0xFFFF0000) == 0x01110000) {
        weapon_reload_from_stack_a1();
        sound_play_se_3d(1, 5, 0, ADDR_g_PlayerPosX);
    }
}

void reload_fx_shotgun(void) {
    s32 weapon_reload_from_stack_a2() __asm__("weapon_reload_from_stack");
    if (((g_PlayerEntity.animation_frame_id == 0xF) || (g_PlayerEntity.animation_frame_id == 0x19) || (g_PlayerEntity.animation_frame_id == 0x23)) && (((u8) g_PlayerEntity.timing_control) == 1)) {
        sound_play_se_3d(1, 9, 5, ADDR_g_PlayerPosX);
        if (g_PlayerEntity.animation_frame_id == 0xF) {
            weapon_reload_from_stack_a2();
        }
    }
}

void reload_fx_colt(void) {
    s32 effect_create_billboard_a14(s32, s32, s32, s32, s32, s32) __asm__("effect_create_billboard");
    s32 fire_ammo_volley_gate();
    s32 weapon_reload_from_stack_a3() __asm__("weapon_reload_from_stack");
    s32 unused[6];
    if ((((*(s32 *)&g_PlayerEntity.death_timer) & 0xFFFF0000) == 0x010C0000) && (fire_ammo_volley_gate() & 0xFF)) {
        LIT(s32, sp_vec_vx) = FIELD_AT(((void *) g_PlayerEntity.joints), Joint, [14].world.t[0], s32);
        LIT(s32, sp_vec_vy) = FIELD_AT(((void *) g_PlayerEntity.joints), Joint, [14].world.t[1], s32);
        LIT(s32, sp_vec_vz) = FIELD_AT(((void *) g_PlayerEntity.joints), Joint, [14].world.t[2], s32);
        effect_create_billboard_a14(5, 4, 0, 0, ADDR_sp_vec, 5);
    }
    if (((*(s32 *)&g_PlayerEntity.death_timer) & 0xFFFF0000) == 0x011C0000) {
        weapon_reload_from_stack_a3();
        sound_play_se_3d(1, 9, 0xA, ADDR_g_PlayerPosX);
    }
}

void reload_fx_default(void) {
    s32 weapon_reload_from_stack_a4() __asm__("weapon_reload_from_stack");
    if (((*(s32 *)&g_PlayerEntity.death_timer) & 0xFFFF0000) == 0x01120000) {
        weapon_reload_from_stack_a4();
        sound_play_se_3d(1, 5, 0, ADDR_g_PlayerPosX);
    }
}

void player_behavior_1a_lockon_turn(void) {
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
    extern u8 g_PlayerAnimationId_u8 __asm__("g_PlayerAnimationId");
    extern u8 g_PlayerAnimationFrameId_u8 __asm__("g_PlayerAnimationFrameId");
    extern u8 g_PlayerTimingControl_u8 __asm__("g_PlayerTimingControl");
    void player_next_aim_target_l(void) __asm__("player_next_aim_target");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    void joint_move_v(s32, s32, s32, s32) __asm__("joint_move");
    s32 turn_toward_target_l(s32, s32) __asm__("turn_toward_target");
    void entity_rotate_toward_target_l(s32, s32) __asm__("entity_rotate_toward_target");
    s32 *w;
    u8 c;
    s32 v;
    s32 h;
    s32 t;

    if (g_PlayerActionState != 0 && (g_PadTrigger & PSX_L1)) {
        player_next_aim_target_l();
    }
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerBlendCounter_u8 = 0;
        g_PlayerActionState = 1;
        /* fallthrough */
    case 1:
        if (g_PlayerBlendCounter_u8 == 0) {
            g_PlayerActionState = 2;
            g_PlayerTimingControl_u8 = 0;
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerBlendCounter_u8 = 7;
            g_PlayerAnimationId_u8 = (g_PlayerBehaviorFlags - 1) * 3 + 5 + (g_PlayerAimFlags >> 7) + ((g_PlayerAimFlags & 0x20) >> 4);
            if (g_StatusFlags & STATUS_IN_WATER) {
                sound_play_footstep_l(0, 0);
            }
            return;
        }
        joint_move_v(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x200);
        break;
    case 2:
        if (g_PlayerBlendCounter_u8 == 0) {
            c = g_PlayerAimFlags;
            g_PlayerActionState = 1;
            g_PlayerBlendCounter_u8 = 7;
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerTimingControl_u8 = 0;
            h = c >> 7;
            v = h * 2;
            v += h;
            v += 7;
            t = c & 0x20;
            t = t != 0;
            v += t * 6;
            g_PlayerAnimationId_u8 = v;
            return;
        }
        joint_move_v(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x200);
        break;
    }
    w = LITP(s32, sp_tmp1C);
    *w = ((g_PlayerId & 1) << 5) + 0xF0;
    if (turn_toward_target_l(g_PlayerAimTarget + 0x34, 0x200) == 0) {
        *w = ((g_PlayerId & 1) << 5) + 0xC0;
    }
    entity_rotate_toward_target_l(g_PlayerAimTarget + 0x34, *w);
    if (turn_toward_target_l(g_PlayerAimTarget + 0x34, 0x20) == 0) {
        g_PlayerActionBehavior = 0x13;
    }
}

void player_behavior_19_fire_click(void) {
    u16 ticks;

    if (g_PlayerActionState == 0) {
        g_PlayerActionState = 1;
        g_PlayerActionTicksCounter = 0xF;
        sound_play_se_3d(1, 9, 0, ADDR_g_PlayerPosX);
        if (g_StatusFlags & STATUS_IN_WATER) {
            sound_play_footstep(0, 0);
        }
    }
    ticks = g_PlayerActionTicksCounter;
    g_PlayerActionTicksCounter = ticks - 1;
    if (ticks == 0) {
        g_PlayerActionBehavior = 0x13;
    }
}

void player_behavior_12_knife_aim(void) {
    s32 joint_move_a25() __asm__("joint_move");
    u16 player_find_aim_target_a1() __asm__("player_find_aim_target");
    s32 entity_update_weapon_joint_a3() __asm__("entity_update_weapon_joint");
    s32 turn_toward_target();
    u16 angle;

    if (g_PlayerActionState == 0) {
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 5;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerHitState = 0;
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerActionTicksCounter = 0;
        if (g_AutoAimEnabled != 0) {
            g_PlayerAimState = player_find_aim_target_a1();
            goto block_3;
        }
    } else {
block_3:
        if (g_AutoAimEnabled != 0) {
            g_PlayerAimState |= 2;
            if (g_PadHeldRaw & PSX_L1) {
                g_PlayerAimState = 0;
            }
            if (g_PlayerAimState == 3) {
                LIT(s32, sp_tmp1C) = ((g_PlayerId & 1) << 5) + 0xF0;
                if (turn_toward_target(g_PlayerAimTarget + 0x34, 0x200) == 0) {
                    LIT(s32, sp_tmp1C) = ((g_PlayerId & 1) << 5) + 0xC0;
                }
                entity_rotate_toward_target(g_PlayerAimTarget + 0x34, LIT(s32, sp_tmp1C));
            }
        }
    }
    if (joint_move_a25(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
        g_PlayerActionBehavior = 0x13;
        g_PlayerBlendCounter = 0;
    }
    entity_update_weapon_joint_a3(0);
    if (g_PadHeld & PAD_RIGHT) {
        angle = g_PlayerAngle + 0x20;
        goto block_16;
    }
    if (g_PadHeld & PAD_LEFT) {
        angle = g_PlayerAngle - 0x20;
block_16:
        g_PlayerAngle = angle;
    }
}

void player_behavior_13_knife_hold(void) {
    s32 joint_move_a26() __asm__("joint_move");
    s32 entity_update_weapon_joint_a4() __asm__("entity_update_weapon_joint");
    s32 sound_play_footstep_a10() __asm__("sound_play_footstep");
    extern s16 g_PlayerAngle_s16 __asm__("g_PlayerAngle");
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
  s32 *pl_flags_ptr;
  u16 angle;
  u8 aim2;
  u8 aim3;
  u8 aim;
  aim = ((LIT(u8, g_PlayerEntity)) & 0x1F) | 0x40;
  LIT(s32, sp_tmp1C) = (s32) (LIT(u8, g_PlayerEntity));
  LIT(u8, g_PlayerEntity) = aim;
  if (g_PadHeld & PAD_AIM_UP)
  {
    LIT(u8, g_PlayerEntity) = (aim & 0x1F) | 0x80;
  }
  if (g_PadHeld & PAD_AIM_DOWN)
  {
    LIT(u8, g_PlayerEntity) = ((LIT(u8, g_PlayerEntity)) & 0x1F) | 0x20;
  }
  g_PlayerAimFlags &= 0xFD;
  if (g_PlayerActionState == 0)
  {
    if (g_PlayerBlendCounter_u8 == 0)
    {
      g_PlayerActionState = 1;
      g_PlayerAnimationFrameId = 0;
      g_PlayerTimingControl = 0;
      g_PlayerBlendCounter_u8 = 0xF;
      aim2 = LIT(u8, g_PlayerEntity);
      g_PlayerMoveSpeedCurrent = 1;
      g_PlayerAnimationId = (((u32) (aim2 & 0x80)) >> 6) + ((((u32) (aim2 & 0x20)) >> 3) + 0xA);
      if (g_StatusFlags & STATUS_IN_WATER)
      {
        sound_play_footstep_a10(0, 0);
      }
    }
    joint_move_a26(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x100);
  }
  pl_flags_ptr = LITP(s32, g_PlayerEntity);
  if (g_PlayerBlendCounter_u8 == 0)
  {
    g_PlayerActionState = 0;
    g_PlayerAnimationFrameId = 0;
    g_PlayerTimingControl = 0;
    g_PlayerBlendCounter_u8 = 0xF;
    aim3 = *pl_flags_ptr;
    g_PlayerMoveSpeedCurrent = 1;
    g_PlayerAnimationId = (((u32) (aim3 & 0x80)) >> 6) + ((((u32) (aim3 & 0x20)) >> 3) + 9);
  }
  joint_move_a26(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x100);
  entity_update_weapon_joint_a4(1);
  if (g_PlayerActionTicksCounter != 0)
  {
    g_PlayerActionTicksCounter -= 1;
  }
  if ((g_PadHeld & PAD_FIRE) && (g_PlayerActionTicksCounter == 0))
  {
    g_PlayerActionBehavior = 0x14;
    g_PlayerActionTicksCounter = 0xA;
    return;
  }
  if (!(g_PadHeld & PAD_AIM))
  {
    g_PlayerActionBehavior = 0x15;
    return;
  }
  if (g_PadHeld & PAD_RIGHT)
  {
    angle = (g_PlayerAngle_s16 + 0x48) - ((g_PlayerId & 1) * 0x10);
    goto block_22;
  }
  if (g_PadHeld & PAD_LEFT)
  {
    angle = (g_PlayerAngle_s16 - 0x48) + ((g_PlayerId & 1) * 0x10);
    block_22:
    g_PlayerAngle_s16 = angle;

  }
}

void player_behavior_14_knife_swing(void) {
    extern u8 *g_OmodelTable_l __asm__("g_OmodelTable");
    extern u8 g_PlayerBlendCounter_l __asm__("g_PlayerBlendCounter");
    extern s32 g_PlayerJoints_l __asm__("g_PlayerJoints");
    extern u8 g_PlayerAnimationId_l __asm__("g_PlayerAnimationId");
    extern u8 g_PlayerAnimationFrameId_l __asm__("g_PlayerAnimationFrameId");
    extern u8 g_PlayerTimingControl_l __asm__("g_PlayerTimingControl");
    extern u8 *g_DeadMoveValue_l __asm__("g_DeadMoveValue");
    s32 apply_weapon_damage_l(s32) __asm__("apply_weapon_damage");
    s32 effect_create_billboard_l(u8, u8, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    s32 joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void entity_update_weapon_joint_l(s32) __asm__("entity_update_weapon_joint");
    void sound_play_se_3d_l(s32, s32, s32, s32) __asm__("sound_play_se_3d");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    /* per character (rows 0-2 Chris, 3-5 Jill) and knife anim 6/7/8: {first frame, frame count} in which
       the swing deals damage */
    u8 tbl[6][2] = { { 6, 7 }, { 4, 2 }, { 4, 4 }, { 3, 8 }, { 4, 2 }, { 4, 4 } };
    u8 *p;
    u8 *q;
    s32 *s;
    u8 x;
    s32 v;
    s32 e;

    p = LITP(u8, g_PlayerEntity);
    if (g_PlayerActionState == 0) {
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId_l = 0;
        g_PlayerTimingControl_l = 0;
        g_PlayerBlendCounter_l = 3;
        x = *p;
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerAnimationId_l = (x >> 7) + (u8)(((x & 0x20) >> 4) + 6);
    }
    if (g_PlayerAnimationFrameId_l == 7 && (g_CurrentEntity->timing_control & 1)) {
        sound_play_se_3d_l(1, 0, 0, ADDR_g_PlayerPosX);
    }
    if ((g_StatusFlags & STATUS_IN_WATER) && g_PlayerPosY - MODEL_OBJ(g_OmodelTable_l)->coord.t[1] > 1000) {
        if (g_PlayerAnimationId_l == 7) {
            if (g_PlayerAnimationFrameId_l < 3) {
                v = g_PlayerJoints_l;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue_l, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vx) = 0x96;
                effect_create_billboard_l(0x17, 8, 0, v + 0x70C, LITP(s32, sp_vec), 0);
            }
            if (g_PlayerAnimationFrameId_l == 0 && (u8)g_CurrentEntity->timing_control == 0) {
                sound_play_footstep_l(1, 0);
            }
        } else {
            if (!(g_PlayerAnimationFrameId_l & 1)) {
                v = g_PlayerJoints_l;
                LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue_l, MATRIX, ->t, VECTOR);
                LIT(s32, sp_vec_vx) = 0x96;
                effect_create_billboard_l(0x17, 8, 0, v + 0x70C, LITP(s32, sp_vec), 0);
            }
            if (g_PlayerAnimationFrameId_l == 0 && (u8)g_CurrentEntity->timing_control == 0) {
                sound_play_se_3d_l(1, 2, 0, ADDR_g_PlayerPosX);
            }
        }
    }
    e = g_PlayerAnimationId_l - 6;
    g_PlayerHitState = 0;
    q = tbl[(g_PlayerId & 1) * 3 + e];
    if ((u8)(g_PlayerAnimationFrameId_l - q[0]) < q[1]) {
        if (!(g_PlayerAimFlags & 2)) {
            if (apply_weapon_damage_l(1) != 0) {
                pad_vibrate_large(5, 0x96, 0);
                if (g_PlayerAnimationId_l != 6 || g_EnemySlot0_Id == 8 || g_EnemySlot0_Id == 0xD || g_EnemySlot1_Id == 0x13) {
                    g_PlayerAimFlags |= 2;
                }
            }
        }
    }
    if (joint_move_l(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
        g_PlayerActionBehavior = 0x13;
    }
    entity_update_weapon_joint_l(0);
}

void player_behavior_15_knife_holster(void) {
    s32 joint_move_a27(s32, s32, s32, s32) __asm__("joint_move");
    s32 entity_update_weapon_joint_a5(s32) __asm__("entity_update_weapon_joint");
    if (g_PadHeld & PAD_RIGHT) {
        g_PlayerAngle += 0x50;
    }
    if (g_PadHeld & PAD_LEFT) {
        g_PlayerAngle -= 0x50;
    }
    if (g_PlayerActionState == 0) {
        g_PlayerActionState = 1;
        g_PlayerAnimationId = 5;
        g_PlayerBlendCounter = 3;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerMoveSpeedCurrent = 1;
    }
    if (joint_move_a27(1, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x400) != 0) {
        g_PlayerSubState = 0;
        g_PlayerActionBehavior = 0;
    }
    entity_update_weapon_joint_a5(0);
}

void player_behavior_16_knife_turn(void) {
    extern u8 g_PlayerBlendCounter_u8 __asm__("g_PlayerBlendCounter");
    extern u8 * g_PlayerAimTarget_u8p __asm__("g_PlayerAimTarget");
    extern u8 g_PlayerAnimationId_u8 __asm__("g_PlayerAnimationId");
    extern u8 g_PlayerAnimationFrameId_u8 __asm__("g_PlayerAnimationFrameId");
    extern u8 g_PlayerTimingControl_u8 __asm__("g_PlayerTimingControl");
    void player_next_aim_target_l(void) __asm__("player_next_aim_target");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    void joint_move_v(s32, s32, s32, s32) __asm__("joint_move");
    s32 turn_toward_target_v(void *, s32) __asm__("turn_toward_target");
    void entity_rotate_toward_target_v(void *, s32) __asm__("entity_rotate_toward_target");
    s32 *sc;
    u8 *p;
    u8 *q;

    if (g_PlayerActionState != 0 && (g_PadTrigger & PSX_L1)) {
        player_next_aim_target_l();
    }
    p = LITP(u8, g_PlayerEntity);
    switch (g_PlayerActionState) {
    case 0:
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId_u8 = 0;
        g_PlayerTimingControl_u8 = 0;
        g_PlayerBlendCounter_u8 = 0xF;
        g_PlayerMoveSpeedCurrent = 1;
        g_PlayerAnimationId_u8 = ((*p & 0x80) >> 6) + ({ (({ *p & 0x20U; }) >> 3) + 9; });
        /* fallthrough */
    case 1:
        q = LITP(u8, g_PlayerEntity);
        if (g_PlayerBlendCounter_u8 == 0) {
            g_PlayerActionState = 2;
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerTimingControl_u8 = 0;
            g_PlayerBlendCounter_u8 = 0xF;
            g_PlayerAnimationId_u8 = ((*q & 0x80) >> 6) + ({ (({ *q & 0x20U; }) >> 3) + 0xA; });
            if (g_StatusFlags & STATUS_IN_WATER) {
                sound_play_footstep_l(0, 0);
            }
        }
        joint_move_v(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x100);
        joint_move_v(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x100);
        break;
    case 2:
        if (g_PlayerBlendCounter_u8 == 0) {
            g_PlayerActionState = 1;
            g_PlayerAnimationFrameId_u8 = 0;
            g_PlayerTimingControl_u8 = 0;
            g_PlayerBlendCounter_u8 = 0xF;
            g_PlayerAnimationId_u8 = ((*p & 0x80) >> 6) + ({ (({ *p & 0x20U; }) >> 3) + 9; });
        }
        joint_move_v(0, g_PlayerWeaponAnimHeader, g_PlayerWeaponAnimBase, 0x100);
        break;
    }
    sc = LITP(s32, sp_tmp1C);
    *sc = ((g_PlayerId & 1) << 5) + 0xF0;
    if (turn_toward_target_v(g_PlayerAimTarget_u8p + 0x34, 0x200) == 0) {
        *sc = ((g_PlayerId & 1) << 5) + 0xC0;
    }
    entity_rotate_toward_target_v(g_PlayerAimTarget_u8p + 0x34, *sc);
    if (turn_toward_target_v(g_PlayerAimTarget_u8p + 0x34, 0x20) == 0) {
        g_PlayerActionBehavior = 0x13;
    }
}


void player_state_03_death(void) {
    extern s8 g_PlayerState_l __asm__("g_PlayerState");
    extern u16 g_PlayerMoveSpeedCurrent_l __asm__("g_PlayerMoveSpeedCurrent");
    extern u16 g_StageId_l __asm__("g_StageId");
    s32 joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void pad_vibrate_large_ramp_l(s32, s32, s32, s32) __asm__("pad_vibrate_large_ramp");
    void billboard_set_color_l(void *, s32, s32, s32) __asm__("billboard_set_color");
    void sound_play_se_3d_l() __asm__("sound_play_se_3d");
    void sound_play_footstep_l(s32, s32) __asm__("sound_play_footstep");
    void entity_add_speed_xz_l(s32) __asm__("entity_add_speed_xz");
    u16 t[4] = { 25, 0, 15, 0x400 }; /* per character (Chris, Jill): {anim speed, xz push} of the death fall */
    u8 *tp;
    u8 *q;
    u16 v;

    tp = (u8 *)&t;
    switch (g_PlayerActionState) {
    case 0:
        v = *(u16 *)(tp + ({ (g_PlayerId & 1) * 4; }));
        g_PlayerActionState = 1;
        g_PlayerAnimationFrameId = 0;
        g_PlayerTimingControl = 0;
        g_PlayerAnimationId = 4;
        g_PlayerMoveSpeedCurrent_l = v;
        sound_play_se_3d_l(3, 3, 0, LITP(void, g_PlayerPosX));
        g_PlayerActionTicksCounter = 0xB4;
        g_PlayerHitState = 1;
        g_PlayerBlendCounter = 0;
        LIT(u16, g_UpdateFlags) &= 0xFFBF;
        pad_vibrate_small(0xF, 0);
        pad_vibrate_large_ramp_l(0x14, 0, 0xFA, 0);
        pad_vibrate_large_ramp_l(0x50, 0xFA, 0, 0x14);
        /* fallthrough */
    case 1:
        if ((g_PlayerDeathTimer & 0xFFFF0000) == 0x01190000) {
            sound_play_footstep_l(2, 0);
        }
        g_PlayerActionState += joint_move_l(0, g_PlayerAnimHeader, g_PlayerAnimBase, 0x400);
        q = (u8 *)&t + ({ (g_PlayerId & 1) * 4; });
        entity_add_speed_xz_l(FIELD_AT(q, DeathFallSpeed, ->push, u16));
        break;
    case 2:
        if (g_StageId_l != 0x501 && g_StageId_l != 0x903 && g_StageId_l != 0x704) {
            billboard_set_color_l(LIT(u8 *, g_CurrentEntity) + 0xE4, 1, 2, 0xFFFF50);
            billboard_adj_size(LIT(u8 *, g_CurrentEntity) + 0xE4, -0x64, -0x64);
            g_PlayerActionState = 3;
            g_PlayerHitState = 0x80;
            break;
        }
        g_PlayerState_l = 4;
        break;
    case 3:
        billboard_adj_size(LIT(u8 *, g_CurrentEntity) + 0xE4, 0x10, 0x10);
        if (--g_PlayerActionTicksCounter == 0x20) {
            g_PlayerState_l = 4;
        }
        break;
    }
}

void entity_set_joint_anim_data(s32 joint_idx) {
    void *joint;

    joint = ((void *) g_CurrentEntity->joints);

    joint = (u8 *) joint + ((joint_idx & 0xFF) * 0x7C);
    g_CurrentEntity->weapon_part_anim_slot = (s32) JOINT(joint)->anim_slot;
    g_CurrentEntity->weapon_part_anim_object = (s32) ((s32) JOINT(joint)->anim_object);
}

void entity_update_weapon_joint(u8 limb) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    void GsMulCoord3(MATRIX *m1, MATRIX *m2);
    s32 dx;
    s32 joints;
    s32 n;
    void *tip;
    void *e;

    e = LIT(void *, g_CurrentEntity);
    n = 3;
    joints = ENT(e)->joints;
    tip = (void *)joints;
    RotMatrix((SVECTOR *)&ENT(e)->rot_x, e + 0x20);
    GsMulCoord0(LIT(void *, g_CurrentEntity) + 0x20, (MATRIX *)(joints + 0x24), LITP(MATRIX, sp_matrix));
    GsMulCoord3(LITP(MATRIX, sp_matrix), (MATRIX *)(joints + 0x11C));
    tip = (u8 *) tip + (((limb) * 0x174) + 0x26C);
    do {
        GsMulCoord3(LITP(MATRIX, sp_matrix), (tip - ((n & 0xFF) * 0x7C)) + 0xA0);
        n -= 1;
    } while (n & 0xFF);
    dx = SCRATCH->mat_t0 - JOINT(tip)->world.t[0];
    SCRATCH->mat_t1 = 0;
    SCRATCH->mat_t0 = dx;
    SCRATCH->mat_t2 = (s32) (SCRATCH->mat_t2 - JOINT(tip)->world.t[2]);
    g_CurrentEntity->pos_x = (s32) (g_CurrentEntity->pos_x - dx);
    g_CurrentEntity->pos_z = (g_CurrentEntity->pos_z - SCRATCH->mat_t2);
}

void entity_update_look_at_angles(void) {
    long SquareRoot0(long a);
    long catan(long a);
    s32 get_angle_towards_target_l(s32, s32) __asm__("get_angle_towards_target");
    u8 *o;
    u8 f;
    u8 f3;
    u8 f2;
    s32 v;
    s16 ang;
    s16 cur;
    s16 cur2;
    u8 spd;
    u8 spd2;
    s16 dy;
    s32 dx;
    s32 dz;
    u16 r;
    s16 b;
    f = LIT(u8 *, g_CurrentEntity)[0xD8];
    if (!(f & 0x10)) {
        return;
    }
    if (f & 0x40) {
        v = CURRENT_ENTITY_LIT->scd_pos_z;
        if (!(v & 0x10000)) {
            CURRENT_ENTITY_LIT->scd_pos_z = (v << 1) | 0x10000;
            LIT(u8 *, g_CurrentEntity)[0xD8] |= 0x20;
        }
        f2 = LIT(u8 *, g_CurrentEntity)[0xD8];
        if ((f2 & 0x40) && CURRENT_ENTITY_LIT_FIELD_AS(scd_pos_z, u16) == 0) {
            LIT(u8 *, g_CurrentEntity)[0xD8] = f2 & 0x1C;
        }
    }
    if (LIT(u8 *, g_CurrentEntity)[0xD8] & 0x80) {
        CURRENT_ENTITY_LIT->scd_pos_x = ENT(CURRENT_ENTITY_LIT->aim_target)->pos_x;
        CURRENT_ENTITY_LIT->scd_pos_y = ENT(CURRENT_ENTITY_LIT->aim_target)->pos_y - 0xA28;
        CURRENT_ENTITY_LIT->scd_pos_z = ENT(CURRENT_ENTITY_LIT->aim_target)->pos_z;
    }
    ang = (LIT(u8 *, g_CurrentEntity)[0xD8] & 1) * CURRENT_ENTITY_LIT->scd_pos_x;
    o = ((u8 *) CURRENT_ENTITY_LIT->joints);
    o += LIT(u8 *, g_CurrentEntity)[0xDD] * 0x7C;
    spd = LIT(u8 *, g_CurrentEntity)[0xD9];
    spd2 = LIT(u8 *, g_CurrentEntity)[0xDA];
    if (!(LIT(u8 *, g_CurrentEntity)[0xD8] & 0x20)) {
        v = get_angle_towards_target_l(CURRENT_ENTITY_LIT->scd_pos_x, CURRENT_ENTITY_LIT->scd_pos_z);
        ang = ((v - ((s16) CURRENT_ENTITY_LIT->angle)) & 0xFFF) * (LIT(u8 *, g_CurrentEntity)[0xD8] & 1);
    }
    cur = JOINT(o)->rot_delta_x;
    if (((ang - cur + spd) & 0xFFF) < spd * 2) {
        dy = cur; /* s16 temp reused later for dy; keeps the restore copy (a3) */
        JOINT(o)->rot_delta_x = ang;
        if (((ang + 0x2C8) & 0xFFF) >= 0x591) {
            JOINT(o)->rot_delta_x = dy;
        }
        if ((CURRENT_ENTITY_LIT_FIELD_AS(look_at_flags, s32) & 0x41) == 0x41) {
            CURRENT_ENTITY_LIT->scd_pos_x = 0x1000 - CURRENT_ENTITY_LIT->scd_pos_x;
            CURRENT_ENTITY_LIT->scd_pos_z -= 1;
        }
    } else if (ang == 0) {
        if ((cur & 0xFFF) > 0x800) {
            JOINT(o)->rot_delta_x = (spd + cur) & 0xFFF;
        } else {
            JOINT(o)->rot_delta_x = (cur - spd) & 0xFFF;
        }
    } else if ((s16)((ang - cur) & 0xFFF) < 0x800) {
        JOINT(o)->rot_delta_x = (spd + cur) & 0xFFF;
        if (((JOINT(o)->rot_delta_x - 0x2C8) & 0xFFF) < 0xA70) {
            JOINT(o)->rot_delta_x = 0x2C8;
        }
    } else {
        JOINT(o)->rot_delta_x = (cur - spd) & 0xFFF;
        if (((JOINT(o)->rot_delta_x + 0x2C8) & 0xFFF) >= 0x591) {
            JOINT(o)->rot_delta_x = 0xD38;
        }
    }
    f3 = LIT(u8 *, g_CurrentEntity)[0xD8];
    ang = ({ (f3 & ({ 2; })) != 0; }) * CURRENT_ENTITY_LIT->scd_pos_y;
    if (!(f3 & 0x20)) {
        dx = CURRENT_ENTITY_LIT->scd_pos_x - JOINT(o)->world.t[0];
        dz = CURRENT_ENTITY_LIT->scd_pos_z - JOINT(o)->world.t[2];
        v = SquareRoot0(dx * dx + dz * dz);
        dy = CURRENT_ENTITY_LIT_FIELD_AS(scd_pos_y, u16) - FIELD_AT(o, Joint, ->world.t[1], u16);
        b = ({ (LIT(u8 *, g_CurrentEntity)[0xD8] & ({ 2; })) != 0; });
        r = v;
        ang = b * (dy > 0 ? 0xC00 : 0x400);
        if (r != 0) {
            ang = catan((dy << 12) / r) * ({ (((u8) ENT(g_CurrentEntity)->look_at_flags) & ({ 2; })) != 0; });
        }
    }
    cur2 = JOINT(o)->rot_delta_y;
    if (((ang - cur2 + spd2) & 0xFFF) < spd2 * 2) {
        JOINT(o)->rot_delta_y = ang;
        if (((ang + 0x138) & 0xFFF) >= 0x271) {
            JOINT(o)->rot_delta_y = cur2;
        }
        if ((CURRENT_ENTITY_LIT_FIELD_AS(look_at_flags, s32) & 0x41) == 0x40) {
            LIT(u8 *, g_CurrentEntity)[0xD8] ^= 2;
            CURRENT_ENTITY_LIT->scd_pos_z -= 1;
        }
    } else if (((cur2 - ang) & 0xFFF) > 0x800) {
        JOINT(o)->rot_delta_y = (spd2 + cur2) & 0xFFF;
        if (((JOINT(o)->rot_delta_y - 0x138) & 0xFFF) < 0x6C8) {
            JOINT(o)->rot_delta_y -= spd2;
        }
    } else {
        JOINT(o)->rot_delta_y = (cur2 - spd2) & 0xFFF;
        if (((JOINT(o)->rot_delta_y + 0x138) & 0xFFF) >= 0x939) {
            JOINT(o)->rot_delta_y += spd2;
        }
    }
}

void entity_apply_look_at_rotation(void) {
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
    u8 *p;

    if (((u8) ENT(g_CurrentEntity)->look_at_flags) != 0) {
        do {
            p = (u8 *) ENT(g_CurrentEntity)->joints;
            p += ((u8) ENT(g_CurrentEntity)->look_at_joint_idx) * 0x7C;
            SCRATCH->sv_x = 0;
            SCRATCH->sv_y = ((u16) JOINT(p)->rot_delta_x);
            SCRATCH->sv_z = ((u16) JOINT(p)->rot_delta_y);
            LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
            RotMatrixYXZ(LITP(SVECTOR, sp_svec), LITP(MATRIX, sp_matrix));
        } while (0);
        p += 0x44;
        CompMatrix((MATRIX *)p, LITP(MATRIX, sp_matrix), (MATRIX *)p);
    }
}

void auto_aim_pitch_update(void) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    void GsMulCoord3(MATRIX *m1, MATRIX *m2);
    s16 lo;
    s16 hi;
    s32 i;
    s32 unused1;
    s32 joints;
    s32 unused2;
    s32 unused3;
    void *e;

    e = LIT(void *, g_CurrentEntity);
    joints = ENT(e)->joints;
    RotMatrix((SVECTOR *)&ENT(e)->rot_x, e + 0x20);
    GsMulCoord0(LIT(void *, g_CurrentEntity) + 0x20, (MATRIX *)(joints + 0x24), LITP(MATRIX, sp_matrix));
    GsMulCoord3(LITP(MATRIX, sp_matrix), (MATRIX *)(joints + 0x480));
    GsMulCoord3(LITP(MATRIX, sp_matrix), (MATRIX *)(joints + 0x4FC));
    GsMulCoord3(LITP(MATRIX, sp_matrix), (MATRIX *)(joints + 0x578));
    LIT(u8, g_PlayerEntity) = (LIT(u8, g_PlayerEntity) & 0x1F) | 0x40;
    if (g_PlayerEntity.behavior_flags != 0xA) {
        i = (g_PlayerEntity.id & 1) * 8;
        lo = *(u16 *)((u8 *)&g_AutoAimPitchTbl + i) + (*(s16 *)&g_PlayerEntity.pos_y);
        hi = *(u16 *)((u8 *)&g_AutoAimPitchTbl_Max + i) + (*(s16 *)&g_PlayerEntity.pos_y);
        if (g_PlayerEntity.behavior_flags < 6 && g_PlayerEntity.behavior_flags != 3) {
            lo = *(u16 *)((u8 *)&g_AutoAimPitchTbl_AltMin + i) + (*(s16 *)&g_PlayerEntity.pos_y);
            hi = *(u16 *)((u8 *)&g_AutoAimPitchTbl_AltMax + i) + (*(s16 *)&g_PlayerEntity.pos_y);
        }
        if (SCRATCH->mat_t1 < lo) {
            LIT(u8, g_PlayerEntity) = (LIT(u8, g_PlayerEntity) & 0x1F) | 0x80;
        }
        if (hi < SCRATCH->mat_t1) {
            LIT(u8, g_PlayerEntity) = (LIT(u8, g_PlayerEntity) & 0x1F) | 0x20;
        }
    }
}

u8 weapon_get_ammo(void) {
    u8 item_id;
    u8 slot;
    u8 ammo;
    void *slot_ptr2;
    void *slot_ptr3;
    void *slot_ptr;

    slot = LIT(u8, g_EquippedSlot);
    if (slot != 0) {
        slot_ptr = (void *)((slot * 2) + g_ItemSlotsPointer);
        item_id = ((InventorySlot *)slot_ptr)[-1].item_id;
        if (item_id != 1) {
            if (item_id == 6) {
                return ((InventorySlot *)slot_ptr)[-1].count;
            }
            ammo = ((InventorySlot *)slot_ptr)[-1].count & 0x7F;
            if (ammo == 0) {
                if (Flg_ck(ADDR_g_ScenarioFlags, 0x7E) != 0) {
                    slot_ptr2 = (void *)((LIT(u8, g_EquippedSlot) * 2) + g_ItemSlotsPointer);
                    if (((InventorySlot *)slot_ptr2)[-1].item_id == 0xA) {
                        ((InventorySlot *)slot_ptr2)[-1].count = 4;
                        return 4U;
                    }
                    goto block_9;
                }
block_9:
                ammo = 0;
                if (Flg_ck(ADDR_g_ScenarioFlags, 0x7A) != 0) {
                    slot_ptr3 = (void *)((g_EquippedSlot * 2) + g_ItemSlotsPointer);
                    if (((InventorySlot *)slot_ptr3)[-1].item_id == 5) {
                        ((InventorySlot *)slot_ptr3)[-1].count = 6;
                        return 6U;
                    }
                    goto block_12;
                }

                return ammo;
            }

            return ammo;
        }
    }
block_12:
    ammo = 0;
    return ammo;
}

s32 weapon_has_ammo_item(void) {
    return (get_item_slot(g_PlayerEntity.behavior_flags + 9) + 1) & 0xFF;
}

s32 fire_ammo_volley_gate(void) {
    u8 count;
    void *slot_ptr;

    slot_ptr = (void *)((g_EquippedSlot * 2) + g_ItemSlotsPointer);
    count = ((InventorySlot *)slot_ptr)[-1].count;
    if (!(!(count & 0x80))) {
        return 0;
    }
    ((InventorySlot *)slot_ptr)[-1].count = (u8) (count | 0x80);
    return 1;
}

void weapon_reload_from_stack(void) {
    extern u8 *g_ItemSlotsPointer_g __asm__("g_ItemSlotsPointer");
    void rearrange_item_slots_l() __asm__("rearrange_item_slots");
    extern u8 g_PlayerEntity_a[] __asm__("g_PlayerEntity");
    u8 *e;
    u8 best;
    u8 key;
    u8 thr;
    u8 n;
    u8 i;
    u8 idx;

    best = 0;
    key = g_PlayerEntity_a[2] + 9;
    thr = *((u8 *)&g_ItemMaxQty + ((g_PlayerEntity.behavior_flags - 1) << 2));
    n = 8;
    if ((g_PlayerEntity.id & 3) != 1) {
        n = 6;
    }
    i = 0;
    do {
        e = &g_ItemSlotsPointer_g[i * 2];
        n--;
        if (INV_SLOT(e)->item_id == key && best < INV_SLOT(e)->count) {
            idx = i;
            best = INV_SLOT(e)->count;
        }
        i++;
    } while (n != 0);
    if (thr < best) {
        g_ItemSlotsPointer_g[g_EquippedSlot * 2 - 1] = thr;
        g_ItemSlotsPointer_g[idx * 2 + 1] = best - thr;
        return;
    }
    g_ItemSlotsPointer_g[g_EquippedSlot * 2 - 1] = best;
    g_ItemSlotsPointer_g[idx * 2] = 0;
    rearrange_item_slots_l();
}

typedef struct {
    GsRVIEW2 view;   /* RdtCamera +0x08 */
    u8 pad[0xC];
} RdtCameraView;
typedef struct {
    u8 pad[0x9C];
    RdtCameraView rec[1];
} RdtCameraViews;

s32 weapon_fire_camera_spark(void) {
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
    void VectorNormal(VECTOR *v0, VECTOR *v1);
    long catan(long a);
    int rand(void);
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    s32 effect_create_billboard_l(u8, u8, s32, s32, s32 *, s32) __asm__("effect_create_billboard");
    GsRVIEW2 view;
    VECTOR a;
    VECTOR b;
    VECTOR n;
    u8 flag;
    s32 dx;
    s32 dz;
    s32 h;
    s32 hp;
    u8 *obj;

    flag = 0;
    if (g_PlayerEntity.behavior_flags >= 6) {
        return 0;
    }
    if (g_PlayerEntity.behavior_flags == 3) {
        flag = 1;
    }
    view = ((RdtCameraViews *)g_RoomData)->rec[g_CutId].view;
    dx = view.vrx - view.vpx;
    dz = view.vrz - view.vpz;
    LIT(s32, sp_tmp1C) = (dz > 0) ? 0xC00 : 0x400;
    if (dx != 0) {
        LIT(s32, sp_tmp1C) = ((dx < 0) ? 0x800 : 0x1000) - catan((dz << 12) / dx);
    }
    h = flag * 0xA0;
    /* literal address: the BLKmode MATRIX copy below then depends on this load */
    obj = LIT(u8 *, g_PlayerJoints);
    hp = h + 0xA0;
    LITP(SVECTOR, sp_svec)->vx = hp;
    SCRATCH->sv_y = 0;
    SCRATCH->sv_z = 0;
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_l;
    obj += 0x6C8;
    RotMatrixY(LIT(s32, sp_tmp1C) + 0x400, LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, sp_svec));
    a.vx = (LITP(SVECTOR, sp_svec)->vx + view.vpx - JOINT(obj)->world.t[0]) / 18;
    a.vz = (SCRATCH->sv_z + view.vpz - JOINT(obj)->world.t[2]) / 18;
    a.vy = (hp + view.vpy - JOINT(obj)->world.t[1]) / 18;
    b.vx = (view.vpx - LITP(SVECTOR, sp_svec)->vx - JOINT(obj)->world.t[0]) / 18;
    b.vz = (view.vpz - SCRATCH->sv_z - JOINT(obj)->world.t[2]) / 18;
    b.vy = (h + ({ -0xA0; }) + view.vpy - JOINT(obj)->world.t[1]) / 18;
    LITP(SVECTOR, sp_svec)->vx = flag * 400;
    SCRATCH->sv_y = 1000;
    SCRATCH->sv_z = 0;
    ApplyMatrix((MATRIX *)(&JOINT(obj)->world), LITP(SVECTOR, sp_svec), &n);
    OuterProduct0(&n, &a, &a);
    OuterProduct0(&n, &b, &b);
    if (a.vy & 0x80000000) {
        if (b.vy & 0x80000000) {
            return 0;
        }
        if (!((a.vx ^ b.vx) & 0x80000000)) {
            return 0;
        }
        if ((a.vz ^ b.vz) & 0x80000000) {
            goto action;
        }
    }
    return 0;
action:
            LIT(s32, sp_vec_vx) = view.vrx - view.vpx;
            SCRATCH->vec_y = view.vry - view.vpy;
            SCRATCH->vec_z = view.vrz - view.vpz;
            VectorNormal(LITP(VECTOR, sp_vec), LITP(VECTOR, sp_vec));
            LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vx) / 8 + view.vpx - ((rand() & 0xFF) + ({ -0x80; }));
            SCRATCH->vec_y = SCRATCH->vec_y / 8 + view.vpy - ((rand() & 0xFF) + ({ -0x80; }));
            SCRATCH->vec_z = SCRATCH->vec_z / 8 + view.vpz - ((rand() & 0xFF) + ({ -0x80; }));
            effect_create_billboard_l(5, 0x12, 0, LIT(s32, g_DeadMoveValue), LITP(s32, sp_vec), 0);
            if (flag) {
                LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vx) + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_y = SCRATCH->vec_y + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_z = SCRATCH->vec_z + ({ 0x100; }) - (rand() & 0x1FF);
                effect_create_billboard_l(5, 0x12, 0, LIT(s32, g_DeadMoveValue), LITP(s32, sp_vec), 0);
                LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vx) + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_y = SCRATCH->vec_y + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_z = SCRATCH->vec_z + ({ 0x100; }) - (rand() & 0x1FF);
                effect_create_billboard_l(5, 0x12, 0, LIT(s32, g_DeadMoveValue), LITP(s32, sp_vec), 0);
                LIT(s32, sp_vec_vx) = LIT(s32, sp_vec_vx) + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_y = SCRATCH->vec_y + ({ 0x100; }) - (rand() & 0x1FF);
                SCRATCH->vec_z = SCRATCH->vec_z + ({ 0x100; }) - (rand() & 0x1FF);
                effect_create_billboard_l(5, 0x12, 0, LIT(s32, g_DeadMoveValue), LITP(s32, sp_vec), 0);
            }
            return 1;
}
