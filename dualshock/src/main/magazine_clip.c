/* magazine_clip.c - Ejected magazines: the player's detached clip joint falls, bounces and lies on the floor. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
void update_entity_lighting(s32 *pos);


void clear_anim_timing() {
    ClipJoint *p;

    p = (ClipJoint *)((u8 *) g_PlayerEntity.joints);
    p = (ClipJoint *)((u8 *)p + 0x744);
    p->vel_z = 0;
    p->rot_delta_x = 0;
}

void player_update_detached_joint(void) {
    s32 update_entity_lighting_a1() __asm__("update_entity_lighting");
    extern s32 (*g_ClipStateFuncs[])();
    MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void SetLightMatrix(MATRIX *m);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    void GsSetLsMatrix(MATRIX *mp);
    void GsPrstTG3L();
    MATRIX mtx;
    MATRIX *transform;
    MATRIX *world;
    s32 vel;
    void *tmd;
    void *clip;

    clip = (void *)g_PlayerEntity.joints;

    clip = (u8 *) clip + 0x744;
    if ((g_PlayerEntity.has_enter_switch_zone & 0x7F) && (JOINT(clip)->vel_z != 0)) {
        JOINT(clip)->world.t[1] += JOINT(clip)->vel_x;
        vel = ((u16) JOINT(clip)->vel_x);
        JOINT(clip)->vel_y = (u16) (((u16) JOINT(clip)->vel_y) + vel);
        g_ClipStateFuncs[JOINT(clip)->rot_delta_x](clip, vel);
        update_entity_lighting_a1(ADDR_g_PlayerPosX);
        transform = &JOINT(clip)->transform;
        RotMatrix(&JOINT(clip)->rotation, transform);
        world = &JOINT(clip)->world;
        GsMulCoord0(world, transform, &mtx);
        CompMatrix((MATRIX *)g_pWorldScreenMatrix, &mtx, LITP(MATRIX, sp_matrix));
        MulMatrix0((MATRIX *)g_pLightMatrix, world, &mtx);
        LIT(void *, sp_tmp38) = ((void *) JOINT(clip)->anim_slot);
        SetLightMatrix(&mtx);
        GsSetLsMatrix(LITP(MATRIX, sp_matrix));
        tmd = LIT(void *, sp_tmp38);
        GsPrstTG3L(((s32) TMD_OBJ(tmd)->primitive_top), ((s32) TMD_OBJ(tmd)->vert_top), ((s32) TMD_OBJ(tmd)->normal_top), (LIT(u8, g_ActiveBuffer) * 0x28) + ((s32) JOINT(clip)->anim_object) + 8, TMD_OBJ(tmd)->n_primitive, 4, (LIT(u8, g_ActiveBuffer) * 0x14) + ADDR_g_OtMain);
    }
}

void player_clip_state_0_spawn(u8 *p) {
    u8 *q;

    FIELD_AT(p, Joint, ->rot_delta_x, u16) += 1;
    q = ((u8 *) g_PlayerEntity.joints);
    q += 0x6C8;
    FIELD_AT(p, Joint, ->world, MATRIX) = FIELD_AT(q, Joint, ->world, MATRIX);
    FIELD_AT(p, Joint, ->transform.t[0], s32) = 0;
    FIELD_AT(p, Joint, ->transform.t[1], s32) = 0;
    FIELD_AT(p, Joint, ->transform.t[2], s32) = 0;
    FIELD_AT(p, Joint, ->rotation, SVECTOR) = FIELD_AT(q, Joint, ->rotation, SVECTOR);
    FIELD_AT(p, Joint, ->rotation.vz, s16) = 0x400;
    FIELD_AT(p, Joint, ->rotation.vx, s16) = 0;
    FIELD_AT(p, Joint, ->rotation.vy, s16) = 0;
    FIELD_AT(p, Joint, ->vel_x, s16) = 0x50;
    FIELD_AT(p, Joint, ->rot_delta_y, s16) = 0;
    FIELD_AT(p, Joint, ->vel_y, s16) = FIELD_AT(p, Joint, ->transform.t[1], u16) + FIELD_AT(p, Joint, ->world.t[1], u16);
    FIELD_AT(p, Joint, ->rot_delta_z, u16) = (*(u16 *)&g_PlayerEntity.pos_y);
}

void player_clip_state_1_fall(u8 *p) {
    void effect_create_billboard_h6(s32, s32, s32, void *, void *, s32) __asm__("effect_create_billboard");

    FIELD_AT(p, Joint, ->vel_x, u16) = (s16) ((FIELD_AT(p, Joint, ->vel_x, u16) + 0x14) - (FIELD_AT(p, Joint, ->rot_delta_y, s16) * 0x14));
    FIELD_AT(p, Joint, ->rotation.vx, u16) = (s16) (FIELD_AT(p, Joint, ->rotation.vx, u16) + 0x10 + (FIELD_AT(p, Joint, ->rot_delta_y, s16) * 0x10));
    if ((FIELD_AT(p, Joint, ->rot_delta_y, s16) == 0) && (g_StatusFlags & STATUS_IN_WATER) && (ENT(g_OmodelTable)->pos_y < FIELD_AT(p, Joint, ->vel_y, s16))) {
        LIT(VECTOR, sp_vec) = *(VECTOR *)(LIT(u8 *, g_DeadMoveValue) + 0x14);
        effect_create_billboard_h6(0x17, 8, 0, LIT(void *, g_DeadMoveValue), p + 0x58, 0);
        FIELD_AT(p, Joint, ->rot_delta_y, s16) = 1;
        return;
    }
    if (FIELD_AT(p, Joint, ->vel_y, s16) > FIELD_AT(p, Joint, ->rot_delta_z, s16)) {
        FIELD_AT(p, Joint, ->vel_x, s16) = -0x50;
        FIELD_AT(p, Joint, ->rot_delta_x, u16) += 1;
        FIELD_AT(p, Joint, ->world.t[1], s32) = -FIELD_AT(p, Joint, ->transform.t[1], s32) + FIELD_AT(p, Joint, ->rot_delta_z, s16);
        FIELD_AT(p, Joint, ->vel_y, s16) = FIELD_AT(p, Joint, ->rot_delta_z, s16);
        SCRATCH_VEC->x = FIELD_AT(p, Joint, ->transform.t[0], s32) + FIELD_AT(p, Joint, ->world.t[0], s32);
        LIT(s32, sp_vec_vy) = FIELD_AT(p, Joint, ->transform.t[1], s32) + FIELD_AT(p, Joint, ->world.t[1], s32);
        LIT(s32, sp_vec_vz) = FIELD_AT(p, Joint, ->transform.t[2], s32) + FIELD_AT(p, Joint, ->world.t[2], s32);
        sound_play_se_3d(2, 0x15, 0, LITP(void, sp_vec));
    }
}

void player_clip_state_2_bounce(void *p) {
    JOINT(p)->world.t[0] = (s32) (JOINT(p)->world.t[0] + 0x14);
    JOINT(p)->world.t[2] = (s32) (JOINT(p)->world.t[2] + 0xA);
    JOINT(p)->rotation.vy = (s16) ((((u16) JOINT(p)->rotation.vy) + 0x80) - (JOINT(p)->rot_delta_y * 0x40));
    JOINT(p)->rotation.vx = (s16) ((((u16) JOINT(p)->rotation.vx) + 0x100) - (JOINT(p)->rot_delta_y * 0x80));
    JOINT(p)->vel_x = (s16) (((u16) JOINT(p)->vel_x) + 0xF + (JOINT(p)->rot_delta_y * 0xF));
    if (JOINT(p)->rot_delta_z < JOINT(p)->vel_y) {
        JOINT(p)->vel_z = 0;
    }
}

void player_clip_eject_start(void) {
    u8 *base = ((u8 *) g_PlayerEntity.joints);
    ClipJoint *p;

    base += 0x744;
    p = (ClipJoint *)base;
    if (p->vel_z == 0) {
        p->vel_z = 1;
        p->rot_delta_x = 0;
    }
}
