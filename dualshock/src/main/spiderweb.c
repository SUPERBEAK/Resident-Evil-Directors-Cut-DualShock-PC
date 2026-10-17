/* spiderweb.c - the spider web room objects (the web in the cave passage that has to be cut): init, idle,
   damage, destruction. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 clut_fade_add(u32 row, s8 id);
s32 is_point_in_quad(s32 *pos, u16 *quad);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);


void spiderweb_update(void) {
    u8 is_point_in_quad_a2() __asm__("is_point_in_quad");
    extern s32 (*g_SpiderWebStates[])();
    void *e2;
    void *e;

    g_SpiderWebStates[CURRENT_ENTITY_LIT->state]();
    e = LIT(void *, g_CurrentEntity);
    ENT(e)->sca_matrix_head = 0;
    CURRENT_ENTITY_LIT->has_enter_switch_zone = is_point_in_quad_a2(&ENT(e)->pos_x, g_CameraSwitchZones);
    e2 = LIT(void *, g_CurrentEntity);
    if (ENT(e2)->has_enter_switch_zone != 0) {
        billboard_draw(&ENT(e2)->pos_x, &ENT(e2)->shadow, 0, ((s16) ENT(e2)->angle));
    }
}

void spiderweb_init(void) {
    void *e2;
    void *e;
    void *e3;

    e = LIT(void *, g_CurrentEntity);
    FIELD_AT(e, Entity, ->state, s32) = 1;
    ENT(e)->rot_x = 0;
    ENT(e)->angle_z = 0;
    reset_joint_transforms();
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_tmp1C) = 0xFFFFFF;
    billboard_init(ADDR_sp_svec, LIT(void *, g_CurrentEntity) + 0xE4, 0xA, 0x3E8);
    e2 = LIT(void *, g_CurrentEntity);
    ENT(e2)->health = 0x37;
    ENT(e2)->scale = 0x1000;
    ENT(e2)->sca_info = (s32) ((s32 *)&g_SpiderWebScaInfo)[(g_PlayerEntity.id & 1)];
    entity_rotate_collision_points(e2);
    CURRENT_ENTITY_LIT->animation_id = 0;
    CURRENT_ENTITY_LIT->animation_frame_id = 0;
    CURRENT_ENTITY_LIT->timing_control = 0;
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e3 = LIT(void *, g_CurrentEntity);
    joint_move(0, ENT(e3)->anim_header, ENT(e3)->anim_base, 0x400);
}

void spiderweb_idle(void) {
    u8 *e;
    u8 *e2;

    e = LIT(u8 *, g_CurrentEntity);
    *e &= 0x1F;
    e2 = LIT(u8 *, g_CurrentEntity);
    *e2 |= 0x40;
}

void spiderweb_damaged(void) {
    u8 *p;
    u8 *e;
    u32 k;

    p = (u8 *)g_CurrentEntity;
    k = ((u8) ENT(p)->hit_state) >> 3;
    e = ((u8 *) ENT(p)->joints);
    FIELD_AT(p, Entity, ->state, s32) = 1;
    if (k == 1 || k >= 6) {
        LIT(s32, sp_tmp1C) = 0;
        if (ENT(p)->health < 50) {
            LIT(s32, sp_tmp1C) = 1;
        }
        if (ENT(p)->health < 40) {
            LIT(s32, sp_tmp1C) = 2;
        }
        if (ENT(p)->health < 30) {
            LIT(s32, sp_tmp1C) = 3;
        }
        if (ENT(p)->health < 20) {
            LIT(s32, sp_tmp1C) = 4;
        }
        if (ENT(p)->health < 10) {
            LIT(s32, sp_tmp1C) = 5;
        }
        while (--LIT(s32, sp_tmp1C) != -1) {
            *(e + 0x26C - LIT(s32, sp_tmp1C) * 0x7C) = 0;
        }
    }
    ENT(g_CurrentEntity)->hit_state = 0;
}

void spiderweb_destroy(void) {
    s32 Flg_on();
    s32 n;
    void *e;

    LIT(s32, sp_tmp1C) = 5;
    do {
        *(s8 *)(CURRENT_ENTITY_LIT->joints + (LIT(s32, sp_tmp1C) * 0x7C)) = 0;
        n = LIT(s32, sp_tmp1C) - 1;
        LIT(s32, sp_tmp1C) = n;
    } while (n != -1);
    e = LIT(void *, g_CurrentEntity);
    ENT(e)->status_flags = (u8) (ENT(e)->status_flags | 2);
    Flg_on(ADDR_g_EnemiesFlags, CURRENT_ENTITY_LIT->death_event_id);
    CURRENT_ENTITY_LIT_FIELD_AS(state, s32) = 4;
}

void spiderweb_gone(void) {
}
