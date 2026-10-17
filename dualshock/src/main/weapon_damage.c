/* weapon_damage.c - Weapon hits and damage: hit detection (guns, knife, projectiles), damage and the enemies' hit
   reactions. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

s32 effect_projectile_hit_check(s32 radius, s16 x, s16 z) {
    int rand(void);
    extern u8 g_RoomId_h6 __asm__("g_RoomId");
    s32 effect_distance_to_player(s16, s16, void *);
    s32 effect_distance_to_entity(s16, s16, void *);
    s32 check_weapon_line_of_sight_h6(void *) __asm__("check_weapon_line_of_sight");
    void pad_vibrate_small(s32, s32);
    void pad_vibrate_large(s32, s32, s32);
    u8 *base = LITP(u8, g_PlayerEntity);
    u8 *stage = LITP(u8, g_EnemiesList);
    u8 *ent;
    s32 r;
    s16 hp;
    u8 v;

    LIT(s32, sp_tmp00) = radius;
    ent = effect_distance_to_player(x, z, LITP(void, g_PlayerEntity)) ? base : NULL;
    if (ent != NULL && check_weapon_line_of_sight_h6(&ENT(ent)->pos_x) == 0) {
        if (((u8) ENT(ent)->hit_state) != 0) {
            return 1;
        }
        if ((u8) (g_EnemySlot0_Id - 3) < 2) {
            r = rand();
            if ((r & 1) * (rand() & 1)) {
                g_PlayerWork.poison_timer = 150;
                g_PlayerEntity.collision_flags |= 2;
            }
        }
        hp = ((u16) ENT(ent)->health);
        ENT(ent)->health = hp - 5;
        if (ENT(stage)->id == 3) {
            ENT(ent)->health = hp - 10;
        }
        if (ENT(stage)->id == 4) {
            ENT(ent)->health = ENT(ent)->health - 7;
        }
        ENT(ent)->hit_state = 1;
        FIELD_AT(ent, Entity, ->state, s16) = 2;
        FIELD_AT(ent, Entity, ->action_behavior, s16) = 100;
        if ((s16) ((u16) ENT(ent)->health) < 0) {
            FIELD_AT(ent, Entity, ->state, s32) = 3;
        }
        pad_vibrate_small(2, 0);
        pad_vibrate_large(7, 200, 0);
        return 1;
    }
    /* the cave room with the spider web across the passage (stage 3 room 0x0C, enemy slot 1 = the web,
       type 0x13; the player has to cut it to pass - seen in game): a projectile reaching the web breaks it */
    if (g_EnemySlot1_Id != 0x13) {
        return 0;
    }
    v = g_StageId;
    if (v != 2) {
        return 0;
    }
    if (g_RoomId_h6 != 0xC) {
        return 0;
    }
    if (effect_distance_to_entity(x, z, LITP(void, g_EnemySlot1)) == 0) {
        return 0;
    }
    if (g_EnemySlot1_HitState != 0) {
        return 1;
    }
    g_EnemySlot1_HitState = 9;
    g_EnemySlot1_State = 3;
    g_EnemySlot1_Health -= 5;
    if (g_EnemySlot1_Health >= 0) {
        g_EnemySlot1_State = v;
    }
    return 1;
}

s32 effect_distance_to_player(s16 x, s16 z, void *ent) {
    long SquareRoot0();
    s32 dx;
    s32 dz;

    dx = ENT(ent)->pos_x - x;
    dz = ENT(ent)->pos_z - z;
    return SquareRoot0((dx * dx) + (dz * dz), z) < LIT(s32, sp_tmp00);
}

s32 effect_distance_to_entity(s16 x, s16 z, void *ent) {
    long SquareRoot0();
    s32 dx;
    s32 dz;

    dx = ENT(ent)->pos_x - x;
    dz = ENT(ent)->pos_z - z;
    return SquareRoot0((dx * dx) + (dz * dz), z) < (LIT(s32, sp_tmp00) + 0x1F4);
}

u32 entity_check_visual_range(u32 range) {
    long SquareRoot0();
    s32 dx_sq;
    s32 dx;
    s32 dz;
    u32 dist;
    void *ent;
    void *ent2;

    ent = LIT(void *, g_CurrentEntity);
    dx = g_PlayerEntity.pos_x - ENT(ent)->pos_x;
    dx_sq = dx * dx;
    dz = g_PlayerEntity.pos_z - ENT(ent)->pos_z;
    dist = SquareRoot0(dx_sq + (dz * dz), dx_sq);
    if (dist < range) {
        ent2 = LIT(void *, g_CurrentEntity);
        ENT(ent2)->status_flags = (u8) (ENT(ent2)->status_flags | 0x20);
    }
    return dist;
}

u32 entity_check_alert_range(u32 range) {
    long SquareRoot0();
    s32 dx_sq;
    s32 dx;
    s32 dz;
    u32 dist;
    void *ent;
    void *ent2;

    ent = LIT(void *, g_CurrentEntity);
    dx = g_PlayerEntity.pos_x - ENT(ent)->pos_x;
    dx_sq = dx * dx;
    dz = g_PlayerEntity.pos_z - ENT(ent)->pos_z;
    dist = SquareRoot0(dx_sq + (dz * dz), dx_sq);
    if (dist < range) {
        ent2 = LIT(void *, g_CurrentEntity);
        ENT(ent2)->status_flags = (u8) (ENT(ent2)->status_flags | 0x80);
    }
    return dist;
}

u8 apply_weapon_damage(s32 weapon) {
    int rand(void);
    extern s32 (*g_WeaponHitDetectFuncs_l[])(s32, u8 *) __asm__("g_WeaponHitDetectFuncs");
    extern void (*g_WeaponPostHitFuncs_l[])(u8 *) __asm__("g_WeaponPostHitFuncs");
    typedef struct {
        s16 fx_x;
        s16 fx_y;
        s16 fx_z;
        s16 damage;
        u8 fx_type;
        u8 fx_variant;
        u8 hit_state;
        u8 pad;
    } AtkEnt;
    extern AtkEnt g_WeaponHitRecords_l[3][20][10] __asm__("g_WeaponHitRecords");
    u8 buf[32];
    u8 *p;
    u8 *found;
    s32 ret;
    u8 n;
    u8 type;
    u8 r;
    s32 k;

    ret = LIT(u8, g_EnemyCount);
    found = NULL;
    if (ret != 0) {
        p = LITP(u8, g_EnemiesList);
        n = LIT(u8, g_EnemyCount);
        SCRATCH->tmp1C = 0;
        SCRATCH->tmp18 = n;
        do {
            if (*p != 0) {
                buf[--LIT(s32, sp_tmp18)] = LIT(u8, sp_tmp1C);
            }
            if (LIT(s32, sp_tmp18) == 0) {
                break;
            }
            p += ENTITY_SIZE;
        } while (++LIT(s32, sp_tmp1C) < 30);
        p = LITP(u8, g_EnemiesList) + buf[0] * ENTITY_SIZE;
        SCRATCH->tmp00 = 0x7FFFFFFF;
        LIT(s32, sp_tmp18) = weapon;
        type = LIT(u8, sp_tmp18) - 1;
        LIT(s32, sp_tmp08) = type;
        weapon = g_WeaponHitRange[g_PlayerEntity.id & 1][type];
        if ((g_StatusFlags & STATUS_ADVANCED) && type == 1 && g_AdvancedBerettaCritTable[rand() & 0xF] != 0) {
            type = 3;
            LIT(s32, sp_tmp08) = 3;
            LIT(s32, sp_tmp18) = 4;
        }
        while (n-- != 0) {
            if ((*p & g_PlayerEntity.status_flags & 0xE0) && p[0x8A] == 0 && g_WeaponHitDetectFuncs_l[type](weapon, p) != 0) {
                found = p;
            }
            p = LITP(u8, g_EnemiesList) + buf[n] * ENTITY_SIZE;
        }
        if (found == NULL || (check_weapon_line_of_sight(&ENT(found)->pos_x) != 0 && type < 5)) {
            ret = 0;
        } else {
            ret = ENT(found)->id < 20;
            if (ret) {
                r = ENT(found)->id;
                LIT(s32, sp_tmp04) = r;
                LIT(s32, sp_tmp08) = type;
                SCRATCH->vec_x = g_WeaponHitRecords_l[0][r][type].fx_x;
                SCRATCH->vec_y = g_WeaponHitRecords_l[0][r][type].fx_y;
                SCRATCH->vec_z = g_WeaponHitRecords_l[0][r][type].fx_z;
                SCRATCH->tmp0C = g_WeaponHitRecords_l[0][r][type].fx_type;
                {
                    s32 t9 = g_WeaponHitRecords_l[0][r][type].fx_variant;
                    LIT(s32, sp_tmp14) = ENT(found)->health;
                    SCRATCH->tmp10 = t9;
                }
                ENT(found)->hit_state = g_WeaponHitRecords_l[0][r][type].hit_state;
                if (g_StatusFlags & STATUS_TRAINING) {
                    ENT(found)->health -= g_WeaponHitRecords_l[1][r][type].damage;
                } else if ((g_StatusFlags & STATUS_ADVANCED) && !(g_StatusFlags & STATUS_ADVANCED_HOLD)) {
                    ENT(found)->health -= g_WeaponHitRecords_l[2][r][type].damage;
                } else {
                    ENT(found)->health -= g_WeaponHitRecords_l[0][r][type].damage;
                }
                k = g_PlayerEntity.status_flags >> 5;
                if (k != 1) {
                    ENT(found)->hit_state += k;
                }
                ENT(found)->hit_state |= SCRATCH_AS(tmp18, u8) << 3;
                g_WeaponPostHitFuncs_l[type](found);
                FIELD_AT(found, Entity, ->state, s32) = 3;
                if (ENT(found)->health >= 0) {
                    FIELD_AT(found, Entity, ->state, s32) = 2;
                }
                ret = 1;
            }
        }
    }
    return ret;
}

void weapon_post_hit_knife(s32 ent) {
    extern s32 (*g_EnemyHitReactionFuncs[])();
    /* per character (Chris, Jill): offset of the knife-hit billboard from joint 14 */
    s16 t[2][3] = { { 153, 0, 0 }, { -380, 0, 0 } };
    s32 base;
    s32 n;
    s16 *v;

    base = g_PlayerEntity.joints;
    sound_play_se_3d(1, 1, 0, ADDR_g_PlayerPosX);
    g_EnemyHitReactionFuncs[SCRATCH->tmp04](ent);
    n = SCRATCH->tmp0C;
    if (n != 1) {
        v = t[g_PlayerEntity.id & 1];
        LIT(s32, sp_vec_vx) = v[0];
        SCRATCH->vec_y = v[1];
        SCRATCH->vec_z = v[2];
        effect_create_billboard(n, SCRATCH->tmp10, 0, base + 0x70C, ADDR_sp_vec, 0);
    }
}

void weapon_post_hit_reaction(void) {
    extern s32 (*g_EnemyHitReactionFuncs[])();
    g_EnemyHitReactionFuncs[LIT(s32, sp_tmp04)]();
}

void weapon_post_hit_shotgun(void *ent) {
    extern s32 (*g_EnemyHitReactionFuncs[])();
    if ((LIT(s32, sp_tmp08) == 2) && (LIT(s32, sp_tmp00) >= 0x2329)) {
        ENT(ent)->hit_state = (u8) (((u8) ENT(ent)->hit_state) + 0xFF);
        ENT(ent)->health = (u16) (((u16) ENT(ent)->health) + 0xA);
    }
    g_EnemyHitReactionFuncs[LIT(s32, sp_tmp04)]();
}

void weapon_post_hit_blood(void *ent) {
    s32 effect_create_billboard_l() __asm__("effect_create_billboard");
    s32 joint_apply_color_tint_l() __asm__("joint_apply_color_tint");
    extern s16 g_PlayerAngle_l __asm__("g_PlayerAngle");
    void *base;
    void *pos;
    s32 v;

    base = ((void *) ENT(ent)->joints);
    if (SCRATCH->tmp0C == 1) {
        return;
    }
    if (*(s16 *)((u8 *)&g_WeaponHitRecords + ((SCRATCH->tmp04 * 0x78) + (SCRATCH->tmp08 * 0xC))) == 0x96
        && (g_PlayerEntity.status_flags & 0x20)) {
        SCRATCH->vec_y += 0x3E8;
        if (FIELD_AT(base, Joint, [1].world.t[1], s32) >= -0x3E7) {
            SCRATCH->vec_y = -0x12C;
        }
    }
    pos = (u8 *)ent + 0x20;
    effect_create_billboard_l(0xE, SCRATCH->tmp10, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    v = SCRATCH->tmp04;
    if (v != 7 && v != 0xA && v != 8) {
        SCRATCH->vec_y -= 0x1F4;
        effect_create_billboard_l(0xE, 3, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
        effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    }
    if (ENT(ent)->health >= 0 || SCRATCH->tmp0C != 0xE) {
        return;
    }
    v = LIT(s32, sp_tmp04);
    if (v != 5 && v != 7 && v != 0xA) {
        SCRATCH->vec_y = 0;
        LIT(s32, sp_vec_vx) = -0x64;
        SCRATCH->vec_z = -0x12C;
        effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
        effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
        LIT(s32, sp_vec_vx) = 0x64;
        SCRATCH->vec_z = 0x12C;
        effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
        effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
        if (*(s16 *)((u8 *)&g_WeaponHitRecords + ((LIT(s32, sp_tmp04) * 0x78) + (SCRATCH->tmp08 * 0xC))) == 0x96) {
            SCRATCH->vec_y = -0x3E8;
            LIT(s32, sp_vec_vx) = -0x64;
            SCRATCH->vec_z = -0x12C;
            effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = 0x64;
            SCRATCH->vec_z = 0x12C;
            effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
        }
    }
    LIT(s32, sp_tmp1C) = ENT(ent)->joint_count;
    g_CurrentEntity = ent;
    while (--LIT(s32, sp_tmp1C) != -1) {
        joint_apply_color_tint_l((u8 *)base + LIT(s32, sp_tmp1C) * 0x7C, 0x202020, 0x101010, 0x303030);
        if (SCRATCH->tmp04 == 0) {
            joint_apply_color_tint_l((u8 *)base + LIT(s32, sp_tmp1C) * 0x7C, 0x202020, 0x101010, 0xA0A0A);
        }
    }
    g_CurrentEntity = ent;
}

void weapon_post_hit_blood2(void *ent) {
    s32 effect_create_billboard_a2() __asm__("effect_create_billboard");
    s32 joint_setup_attack_effect();
    extern s16 g_PlayerAngle_s16 __asm__("g_PlayerAngle");
    s32 use_joint_list;
    s32 i;
    s32 i2;
    s32 i3;
    s32 i4;
    s32 i5;
    void *pos;
    void *joints;
    void *saved_entity;

    joints = ((void *) ENT(ent)->joints);
    if (LIT(s32, sp_tmp0C) != 1) {
        if ((*(s16 *)((u8 *)&g_WeaponHitRecords + (((LIT(s32, sp_tmp04) * 0x78) + (LIT(s32, sp_tmp08) * 0xC)))) == 0x96) && (g_PlayerEntity.status_flags & 0x20)) {
            SCRATCH->vec_y = (s32) (SCRATCH->vec_y + 0x3E8);
            if (FIELD_AT(joints, Joint, [1].world.t[1], s32) >= -0x3E7) {
                SCRATCH->vec_y = -0x12C;
            }
        }
        pos = ent + 0x20;
        effect_create_billboard_a2(0xE, 7, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
        effect_create_billboard_a2(9, 0xD, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
        effect_create_billboard_a2(9, 0xD, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
        if (LIT(s32, sp_tmp04) != 8) {
            LIT(s32, sp_tmp1C) = 2;
            SCRATCH->vec_y = (s32) (SCRATCH->vec_y - 0xC8);
            do {
                effect_create_billboard_a2(9, 0xD, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
                i = LIT(s32, sp_tmp1C) - 1;
                LIT(s32, sp_tmp1C) = i;
            } while (i != -1);
            if ((ENT(ent)->health < 0) && (LIT(s32, sp_tmp0C) == 0) && (*(u8 *)((u8 *)&g_EnemyHitJointLists + ((LIT(s32, sp_tmp04) * 6))) != 0)) {
                SCRATCH->vec_y = -0x1F4;
                LIT(s32, sp_tmp1C) = 2;
                do {
                    effect_create_billboard_a2(9, 0xD, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
                    i2 = LIT(s32, sp_tmp1C) - 1;
                    LIT(s32, sp_tmp1C) = i2;
                } while (i2 != -1);
                use_joint_list = (*(u16 *)((u8 *)&g_WeaponHitRecords_Dmg + (((LIT(s32, sp_tmp04) * 0x78) + (LIT(s32, sp_tmp08) * 0xC)))) & 1) * (ENT(ent)->behavior_flags & 2);
                saved_entity = LIT(void *, g_CurrentEntity);
                LIT(void *, g_CurrentEntity) = ent;
                LIT(s32, sp_tmp14) = use_joint_list;
                if (g_PlayerEntity.status_flags & 0x20) {
                    if (use_joint_list == 0) {
                        i3 = ENT(ent)->joint_count - 1;
                        LIT(s32, sp_tmp1C) = i3;
                        if (i3 >= (ENT(ent)->joint_count - 5)) {
                            do {
                                joint_setup_attack_effect(joints + (LIT(s32, sp_tmp1C) * 0x7C), 0x1E, 2, 3);
                                i4 = LIT(s32, sp_tmp1C) - 1;
                                LIT(s32, sp_tmp1C) = i4;
                            } while (i4 >= (ENT(ent)->joint_count - 5));
                        }
                    } else {
                        goto block_20;
                    }
                } else {
block_20:
                    LIT(s32, sp_tmp1C) = 5;
                    do {
                        joint_setup_attack_effect(joints + (*(({ (LIT(s32, sp_tmp04) * 6) + &g_EnemyHitJointLists; }) + LIT(s32, sp_tmp1C)) * 0x7C), 0x1E, 2, 3);
                        i5 = LIT(s32, sp_tmp1C) - 1;
                        LIT(s32, sp_tmp1C) = i5;
                    } while (i5 != -1);
                }
                g_CurrentEntity = saved_entity;
            }
        }
    }
}

void weapon_post_hit_sparks(void *ent) {
    s32 effect_create_billboard_a3() __asm__("effect_create_billboard");
    s32 joint_apply_color_tint_a1() __asm__("joint_apply_color_tint");
    extern u8 g_WeaponHitRecords_u8 __asm__("g_WeaponHitRecords");
    extern s16 g_PlayerAngle_s16 __asm__("g_PlayerAngle");
    s32 i;
    s32 id;
    s32 unused2;
    s32 unused3;
    u8 unused1;
    void *pos;
    void *joints;

    joints = ((void *) ENT(ent)->joints);
    if (LIT(s32, sp_tmp0C) != 1) {
        if (*(s16 *)((u8 *)&g_WeaponHitRecords_u8 + (((LIT(s32, sp_tmp04) * 0x78) + (LIT(s32, sp_tmp08) * 0xC)))) == 0x96) {
            if (g_PlayerEntity.status_flags & 0x20) {
                SCRATCH->vec_y = (s32) (SCRATCH->vec_y + 0x3E8);
                if (FIELD_AT(joints, Joint, [1].world.t[1], s32) >= -0x3E7) {
                    SCRATCH->vec_y = -0x12C;
                }
            }
        }
        LIT(s32, sp_tmp1C) = 2;
        do {
            effect_create_billboard_a3(9, 0, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
            i = LIT(s32, sp_tmp1C) - 1;
            LIT(s32, sp_tmp1C) = i;
        } while (i != -1);
        if ((ENT(ent)->health < 0) && (LIT(s32, sp_tmp0C) == 9)) {
            id = LIT(s32, sp_tmp04);
            if ((id != 5) && (id != 7) && (id != 0xA) && (id != 8)) {
                SCRATCH->vec_y = 0;
                LIT(s32, sp_vec_vx) = -0x64;
                pos = ent + 0x20;
                SCRATCH->vec_z = -0x12C;
                effect_create_billboard_a3(9, 0, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
                effect_create_billboard_a3(9, 1, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
                LIT(s32, sp_vec_vx) = 0x64;
                SCRATCH->vec_z = 0x12C;
                effect_create_billboard_a3(9, 0, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
                effect_create_billboard_a3(9, 1, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
            }
            LIT(s32, sp_tmp1C) = ENT(ent)->joint_count;
            g_CurrentEntity = ent;
            while (--LIT(s32, sp_tmp1C) != -1) {
                joint_apply_color_tint_a1(joints + (LIT(s32, sp_tmp1C) * 0x7C), 0x4040, 0x1010, 0x3030);
                if (LIT(s32, sp_tmp04) == 0) {
                    joint_apply_color_tint_a1(joints + (LIT(s32, sp_tmp1C) * 0x7C), 0x2020, 0x1010, 0xA0A);
                }
            }
            g_CurrentEntity = ent;
        }
    }
}

void weapon_post_hit_blood3(void *ent) {
    s32 effect_create_billboard_l() __asm__("effect_create_billboard");
    s32 joint_apply_color_tint_l() __asm__("joint_apply_color_tint");
    extern s16 g_PlayerAngle_l __asm__("g_PlayerAngle");
    void *base;
    void *pos;
    s32 v;

    base = ((void *) ENT(ent)->joints);
    if (SCRATCH->tmp0C == 1) {
        return;
    }
    if (*(s16 *)((u8 *)&g_WeaponHitRecords + ((SCRATCH->tmp04 * 0x78) + (SCRATCH->tmp08 * 0xC))) == 0x96
        && (g_PlayerEntity.status_flags & 0x20)) {
        SCRATCH->vec_y += 0x3E8;
        if (FIELD_AT(base, Joint, [1].world.t[1], s32) >= -0x3E7) {
            SCRATCH->vec_y = -0x12C;
        }
    }
    pos = (u8 *)ent + 0x20;
    effect_create_billboard_l(0xE, SCRATCH->tmp10, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    v = SCRATCH->tmp04;
    if (v != 7 && v != 0xA && v != 8) {
        SCRATCH->vec_y -= 0x1F4;
        effect_create_billboard_l(0xE, 3, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
        effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, pos, ADDR_sp_vec, 0);
    }
    if (ENT(ent)->health < 0 && SCRATCH->tmp0C != 2) {
        v = LIT(s32, sp_tmp04);
        if (v != 5 && v != 7 && v != 0xA) {
            SCRATCH->vec_y = 0;
            LIT(s32, sp_vec_vx) = -0x64;
            SCRATCH->vec_z = -0x12C;
            effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            LIT(s32, sp_vec_vx) = 0x64;
            SCRATCH->vec_z = 0x12C;
            effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            if (*(s16 *)((u8 *)&g_WeaponHitRecords + ((LIT(s32, sp_tmp04) * 0x78) + (SCRATCH->tmp08 * 0xC))) == 0x96) {
                SCRATCH->vec_y = -0x3E8;
                LIT(s32, sp_vec_vx) = -0x64;
                SCRATCH->vec_z = -0x12C;
                effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
                effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
                LIT(s32, sp_vec_vx) = 0x64;
                SCRATCH->vec_z = 0x12C;
                effect_create_billboard_l(0xE, 6, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
                effect_create_billboard_l(9, 0xD, (g_PlayerAngle_l - ((s16) ENT(ent)->angle)) + 0x800, (u8 *)ent + 0x20, ADDR_sp_vec, 0);
            }
        }
        LIT(s32, sp_tmp1C) = ENT(ent)->joint_count;
        g_CurrentEntity = ent;
        while (--LIT(s32, sp_tmp1C) != -1) {
            joint_apply_color_tint_l((u8 *)base + LIT(s32, sp_tmp1C) * 0x7C, 0x202020, 0x101010, 0x303030);
            if (SCRATCH->tmp04 == 0) {
                joint_apply_color_tint_l((u8 *)base + LIT(s32, sp_tmp1C) * 0x7C, 0x202020, 0x101010, 0xA0A0A);
            }
        }
        g_CurrentEntity = ent;
    }
    weapon_post_hit_blood2(ent);
}

void enemy_hit_reaction_zombie(u8 *ent) {
    extern u8 g_RoomId_l __asm__("g_RoomId");
    extern u8 g_WeaponHitRecords_bytes[] __asm__("g_WeaponHitRecords");
    void *old;
    u8 *m;
    s32 off;
    u16 t;
    u8 n;

    if (LIT(s32, sp_tmp04) != 2) {
        if ((LIT(s32, sp_tmp08) == 2 && LIT(s32, sp_tmp00) < 3000 && (g_PlayerEntity.status_flags >> 6) != 0)
            || ((u32)(LIT(s32, sp_tmp08) - 3) < 2 && (g_PlayerEntity.status_flags & 0x40))) {
            if (!(g_StatusFlags & STATUS_ADVANCED) || g_StageId != 6 || g_RoomId_l != 0x1C) {
                ENT(ent)->health = -300;
                old = LIT(void *, g_CurrentEntity);
                LIT(void *, g_CurrentEntity) = ent;
                Flg_on(ADDR_g_EnemiesFlags, ENT(ent)->death_event_id);
                m = ((u8 *) ENT(ent)->joints);
                joint_setup_attack_effect(m + 0xF8, 0x1E, 2, 3);
                sound_play_entity_se(6, 0);
                LIT(void *, g_CurrentEntity) = old;
                LIT(s32, sp_vec_vx) = 100;
                SCRATCH->vec_y = -600;
                SCRATCH->vec_z = 0;
                effect_create_billboard(0, 3, 0, m + 0x44, ADDR_sp_vec, 0);
                off = LIT(s32, sp_tmp08) * 12 + LIT(s32, sp_tmp04) * 120;
                LIT(s32, sp_vec_vx) = *(s16 *)(g_WeaponHitRecords_bytes + off);
                SCRATCH->vec_y = *(s16 *)(g_WeaponHitRecords_bytes + off + 2);
                SCRATCH->vec_z = *(s16 *)(g_WeaponHitRecords_bytes + off + 4);
                SCRATCH->vec_y -= 120;
                effect_create_billboard(3, 0, ((s16) g_PlayerEntity.angle) - ((s16) ENT(ent)->angle) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
                SCRATCH->vec_y += 120;
            }
        }
        if (g_PlayerEntity.status_flags & 0x20) {
            SCRATCH->vec_y = -600;
            if (ENT(ent)->behavior_flags & 2) {
                SCRATCH->vec_y = -500;
            }
        }
    }
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        (*(u16 *)&ENT(ent)->health)++;
    }
    if ((g_PlayerEntity.id & 1) && LIT(s32, sp_tmp08) == 1) {
        t = ((u16) ENT(ent)->health);
        ENT(ent)->health = t - 3;
        if (LIT(s32, sp_tmp04) == 2) {
            ENT(ent)->health = t - 7;
        }
    }
    if (LIT(s32, sp_tmp0C) == 4) {
        if (LIT(s32, sp_tmp00) < 9000) {
            n = 4;
        loop:
            effect_create_billboard(LIT(s32, sp_tmp0C), n,
                            ((s16) g_PlayerEntity.angle) - ((s16) ENT(ent)->angle) + 0x500 + (n << 8),
                            ent + 0x20, ADDR_sp_vec, 0);
            if (n--) {
                goto loop;
            }
        }
        LIT(s32, sp_tmp0C) = 0;
    }
    effect_create_billboard(LIT(s32, sp_tmp0C), LIT(s32, sp_tmp10), ((s16) g_PlayerEntity.angle) - ((s16) ENT(ent)->angle) + 0x800,
                    ent + 0x20, ADDR_sp_vec, 0);
}

void enemy_hit_reaction_basic(void *ent) {
    effect_create_billboard(LIT(s32, sp_tmp0C), LIT(s32, sp_tmp10), (((s16) g_PlayerEntity.angle) - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
}

void enemy_hit_reaction_head(u8 *ent) {
    s32 effect_create_billboard_a5() __asm__("effect_create_billboard");
    extern s16 g_PlayerAngle_s16 __asm__("g_PlayerAngle");
    u8 *p;

    p = ((u8 *) ENT(ent)->joints);
    if (g_PlayerEntity.status_flags & 0x20) {
        SCRATCH->vec_y += 0x3E8;
        if (FIELD_AT(p, Joint, [1].world.t[1], s32) >= -0x31F) {
            SCRATCH->vec_y = -0x12C;
        }
    }
    effect_create_billboard_a5(3, 8, (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
    effect_create_billboard_a5(LIT(s32, sp_tmp0C), LIT(s32, sp_tmp10), (g_PlayerAngle_s16 - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
}

void enemy_hit_reaction_blood(void *ent) {
    s32 rand();
    if (LIT(s32, sp_tmp08) != 0) {
        if ((LIT(s32, sp_tmp00) >= 0x1B59) && (rand() & 1)) {
            ENT(ent)->health = (u16) LIT(u16, sp_tmp14);
        }
        effect_create_billboard(0, 0x18, (((s16) g_PlayerEntity.angle) - ((s16) ENT(ent)->angle)) + 0x800, ent + 0x20, ADDR_sp_vec, 0);
    }
}

void enemy_hit_reaction_none(void) {
}

s32 weapon_hit_detect_gun(s32 range, u8 *ent) {
    s32 weapon_check_entity_in_cone_l(SVECTOR *, SVECTOR *, u8 *) __asm__("weapon_check_entity_in_cone");
    SVECTOR a;
    SVECTOR b;
    SVECTOR c;
    SVECTOR d;
    u16 h;
    s32 nh;
    s32 ret;

    h = *(u16 *)(((u8 *) ENT(ent)->sca_info) + 0xA);
    if (ENT(ent)->id < 2 || (((s8) ENT(ent)->id) & 0xFF) == 0x11) {
        if ((g_PlayerEntity.status_flags & 0x80) && LIT(s32, sp_tmp08) != 2) {
            return 0;
        }
    }
    if ((((s8) ENT(ent)->id) & 0xFF) == 4) {
        range -= 1000;
    }
    if ((((s8) ENT(ent)->id) & 0xFF) == 8) {
        range += 2000;
    }
    LIT(s16, sp_svec_vx) = 50;
    if (g_PlayerEntity.status_flags & 0x20) {
        LIT(s16, sp_svec_vx) = 0;
    }
    SCRATCH->sv_y = 0;
    SCRATCH->sv_z = h + 200;
    rotate_vector_y(((s16) g_PlayerEntity.angle), LITP(SVECTOR, sp_svec), &a);
    nh = -h;
    SCRATCH->sv_z = nh - 200;
    rotate_vector_y(((s16) g_PlayerEntity.angle), LITP(SVECTOR, sp_svec), &b);
    LIT(s16, sp_svec_vx) = 0x28A;
    SCRATCH->sv_z = h + range;
    rotate_vector_y(((s16) g_PlayerEntity.angle), LITP(SVECTOR, sp_svec), &c);
    SCRATCH->sv_z = nh - range;
    rotate_vector_y(((s16) g_PlayerEntity.angle), LITP(SVECTOR, sp_svec), &d);
    SCRATCH->sv_z = 0;
    rotate_vector_y(((s16) g_PlayerEntity.angle), LITP(SVECTOR, sp_svec), LITP(SVECTOR, sp_svec));
    if (weapon_check_entity_in_cone_l(&a, &b, ent) != 0) {
        ret = 1;
    } else {
        ret = weapon_check_entity_in_cone_l(&c, &d, ent) != 0;
    }
    return ret;
}

s32 weapon_check_entity_in_cone(s16 *side_a, s16 *side_b, u8 *ent) {
    long SquareRoot0(long a);
    s32 cross_product_2d(s32, s32, s32, s32);
    s16 *pos = LITP(s16, sp_svec);
    s32 dx;
    s32 dz;
    u32 dist;

    dx = ENT(ent)->pos_x - g_PlayerEntity.pos_x;
    dz = ENT(ent)->pos_z - g_PlayerEntity.pos_z;
    if (cross_product_2d(*pos, pos[2], dx - side_a[0], dz - side_a[2]) > 0) {
        return 0;
    }
    if (cross_product_2d(*pos, pos[2], dx - side_b[0], dz - side_b[2]) < 0) {
        return 0;
    }
    if (cross_product_2d(side_a[0] - side_b[0], side_a[2] - side_b[2], dx - *pos, dz - pos[2]) > 0) {
        return 0;
    }
    dist = SquareRoot0(dx * dx + dz * dz);
    if (dist < LIT(u32, sp_tmp00)) {
        LIT(u32, sp_tmp00) = dist;
        return 1;
    }
    return 0;
}

s32 weapon_hit_detect_knife(s32 range, u8 *ent) {
    long SquareRoot0(long a);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    /* per character (Chris, Jill): offset of the knife-hit billboard from joint 14 */
    s16 t[2][3] = { { 153, 0, 0 }, { -380, 0, 0 } };
    MATRIX m;
    s16 *v;
    s32 dx;
    s32 dz;
    u32 d;
    u8 kind;
    u8 *flags;

    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_l;
    flags = LITP(u8, g_PlayerEntity);
    v = t[g_PlayerEntity.id & 1];
    SCRATCH->mat_t0 = v[0];
    SCRATCH->mat_t1 = v[1];
    SCRATCH->mat_t2 = v[2];
    GsMulCoord0((MATRIX *)(g_PlayerEntity.joints + 0x70C), LITP(MATRIX, sp_matrix), &m);
    dx = ENT(ent)->pos_x - m.t[0];
    LIT(s32, sp_tmp1C) = SCA(((u8 *) ENT(ent)->sca_info))->radius + range;
    dz = ENT(ent)->pos_z - m.t[2];
    kind = ENT(ent)->id;
    if (kind < 2 || ENT(ent)->id == 0x11) {
        if (*flags & 0x80) {
            return 0;
        }
    }
    if ((ENT(ent)->id == 2 && ENT(ent)->pos_y >= -0x18F) || ENT(ent)->id == 3) {
        if (g_PlayerEntity.status_flags & 0x40) {
            return 0;
        }
    }
    kind = ENT(ent)->id;
    if (kind == 5 || kind == 7 || kind == 9) {
        if (ENT(ent)->pos_y < -0x12C0) {
            return 0;
        }
    }
    if (ENT(ent)->id == 4) {
        LIT(s32, sp_tmp1C) -= 0x320;
    }
    if (ENT(ent)->id == 8) {
        LIT(s32, sp_tmp1C) += 0x7D0;
    }
    if (ENT(ent)->id == 7 || ENT(ent)->id == 10) {
        LIT(s32, sp_tmp1C) += 0x64;
    }
    d = SquareRoot0(dx * dx + dz * dz);
    if (d < LIT(u32, sp_tmp1C) && d < LIT(u32, sp_tmp00)) {
        LIT(u32, sp_tmp00) = d;
        return 1;
    }
    return 0;
}

s32 weapon_hit_detect_projectile(s32 r, u8 *obj) {
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u32 d;
    u32 lim;

    dx = ENT(obj)->pos_x - LIT(s32, sp_vec_vx);
    lim = *(u16 *)(((u8 *) ENT(obj)->sca_info) + 0xA) + r;
    dz = ENT(obj)->pos_z - LIT(s32, sp_vec_vz);
    LIT(u32, sp_tmp1C) = lim;
    if (ENT(obj)->id == 4) {
        LIT(u32, sp_tmp1C) = lim - 1000;
    }
    d = SquareRoot0(dx * dx + dz * dz);
    if (d < LIT(u32, sp_tmp1C) && d < LIT(u32, sp_tmp00)) {
        LIT(u32, sp_tmp00) = d;
        return 1;
    }
    return 0;
}

void debug_print_wall_hits(void) {
    int FntPrint();
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    s32 cross_product_2d(s32, s32, s32, s32);
    s32 line_solve_x(s16);
    s32 line_solve_z(s16);
    s32 count;
    u32 *tbl;
    u16 *p;
    u16 *end;
    u32 k;
    u16 hx;
    u16 hz;
    s32 pz;
    s32 px;
    u16 A;
    u16 B;
    u16 C;
    u16 D;
    s32 A2;
    s32 B2;
    s32 C2;
    s32 D2;
    s32 r;
    u32 quad;

    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vx) = 0x32;
    rotate_vector_y(((s16) g_PlayerEntity.angle), ADDR_sp_svec, ADDR_sp_svec);
    count = 0;
    k = 3;
    hx = g_PlayerEntity.pos_x >> 3;
    hz = g_PlayerEntity.pos_z >> 3;
    pz = (s16)hz;
    px = (s16)hx;
    do {
        tbl = &((u32 *)((u8 *) ROOM_L->collision))[k];
        p = (u16 *)tbl[0];
        end = (u16 *)tbl[1];
        if (p < end) {
            do {
                if (p[5] & 0x100) {
                    C = p[2] >> 3;
                    D = p[3] >> 3;
                    A = p[0] >> 3;
                    B = p[1] >> 3;
                    D2 = D;
                    C2 = C;
                    quad = ((pz < (s32)(B + D2) >> 1) << 1) | (px < (s32)(A + C2) >> 1);
                    B2 = B;
                    A2 = A;
                    switch (quad) {
                    case 0:
                        if (cross_product_2d(LIT(s16, sp_svec_vx), LIT(s16, sp_svec_vz), A2 - px, B2 - pz) < 0) {
                            FntPrint("right ");
                            r = line_solve_z(A - hx) + pz;
                            FntPrint("%d %d\n", count, r);
                            if (D2 < r && r < B2 && A2 < px) {
                                FntPrint("%d Atari!\n", count);
                            }
                        } else {
                            FntPrint("left ");
                            r = line_solve_x(B - hz) + px;
                            FntPrint("%d %d\n", count, r);
                            if (C2 < r && r < A2 && B2 < pz) {
                                FntPrint("%d Atari!\n", count);
                            }
                        }
                        break;
                    case 1:
                        if (cross_product_2d(LIT(s16, sp_svec_vx), LIT(s16, sp_svec_vz), C2 - px, B2 - pz) < 0) {
                            FntPrint("right ");
                            r = line_solve_x(B - hz) + px;
                            FntPrint("%d %d\n", count, r);
                            if (C2 < r && r < A2 && B2 < pz) {
                                FntPrint("%d Atari!\n", count);
                            }
                        } else {
                            FntPrint("left ");
                            r = line_solve_z(C - hx) + pz;
                            FntPrint("%d %d\n", count, r);
                            if (D2 < r && r < B2 && px < C2) {
                                FntPrint("%d Atari!\n", count);
                            }
                        }
                        break;
                    case 2:
                        if (cross_product_2d(LIT(s16, sp_svec_vx), LIT(s16, sp_svec_vz), A2 - px, D2 - pz) < 0) {
                            FntPrint("right ");
                            r = line_solve_x(D - hz) + px;
                            FntPrint("%d %d\n", count, r);
                            if (C2 < r && r < A2 && pz < D2) {
                                FntPrint("%d Atari!\n", count);
                            }
                        } else {
                            FntPrint("left ");
                            r = line_solve_z(A - hx) + pz;
                            FntPrint("%d %d\n", count, r);
                            if (D2 < r && r < B2 && A2 < px) {
                                FntPrint("%d Atari!\n", count);
                            }
                        }
                        break;
                    case 3:
                        if (cross_product_2d(LIT(s16, sp_svec_vx), LIT(s16, sp_svec_vz), C2 - px, D2 - pz) < 0) {
                            FntPrint("right ");
                            r = line_solve_z(C - hx) + pz;
                            FntPrint("%d %d\n", count, r);
                            if (D2 < r && r < B2 && px < C2) {
                                FntPrint("%d Atari!\n", count);
                            }
                        } else {
                            FntPrint("left ");
                            r = line_solve_x(D - hz) + px;
                            FntPrint("%d %d\n", count, r);
                            if (C2 < r && r < A2 && pz < D2) {
                                FntPrint("%d Atari!\n", count);
                            }
                        }
                        break;
                    }
                }
                count++;
                p += 6;
            } while (p < end);
        }
        k++;
    } while (k < 4);
}

s32 line_solve_x(s16 dz) {
    s16 *p;

    p = &SCRATCH->sv_x;
    if (SCRATCH->sv_z == 0) {
        SCRATCH->sv_z = 1;
    }
    return (*p * dz) / SCRATCH->sv_z;
}

s32 line_solve_z(s16 dx) {
    if (LIT(s16, sp_svec_vx) == 0) {
        LIT(s16, sp_svec_vx) = 1;
    }
    return (LIT(s16, sp_svec_vz) * dx) / LIT(s16, sp_svec_vx);
}

s32 cross_product_2d(s32 a, s32 b, s32 c, s32 d) {
    return a * d - b * c;
}

s32 dot_product_2d_swapped(s32 a, s32 b, s32 c, s32 d) {
    return a * d + b * c;
}

/* rodata padding up to the next file's original start (tools/splitc.py --pad) */
__asm__(".section .rodata\n\t.space 2\n\t.text");
