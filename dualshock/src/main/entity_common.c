/* entity_common.c - Shared entity helpers: movement and turning, sight and range checks, enemy-player
   collision and grabs, joint effects and tints, vertex jitter, line of sight, path finding, random numbers. */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 is_point_in_quad(s32 *pos, u16 *quad);

s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);

s16 get_angle_towards_target(s16 x, s16 z);

void entity_extract_anim_vertex(u8 *ent, u8 *src, u8 *anim_tbl, u8 reverse);

s32 check_entity_collision(u8 *a, u8 *b);

void pad_vibrate_small(s32 duration, s32 delay);

void pad_vibrate_large(s32 duration, s32 vol, s32 delay);

void billboard_init(SVECTOR *pos, u8 *bb, s16 half_w, s16 half_d);

void billboard_draw(VECTOR *pos, u8 *obj, s16 y, s16 ang);

u32 boundary_get_cell(void *pos, void *ofs, s32 split_x, s32 split_z);

s32 check_room_collision(VECTOR *pos, u16 radius);

s32 room_check_sight_blocked(VECTOR *dir, s32 group);

void sound_play_entity_se(u8 se);

s16 entity_ballistic_step(s16 speed, s32 fall_speed, s16 gravity, s16 floor_y) {
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    SVECTOR sv;
    MATRIX m;
    void **pp;
    u8 *obj;
    s16 t;

    pp = LITP(void *, g_CurrentEntity);
    sv.vz = 0;
    sv.vx = 0;
    sv.vy = ENT(*pp)->angle;
    RotMatrix(&sv, &m);
    sv.vx = speed;
    sv.vz = 0;
    sv.vy = 0;
    ApplyMatrixSV(&m, &sv, &sv);
    obj = *pp;
    ENT(obj)->pos_x += sv.vx;
    ENT(obj)->pos_z += sv.vz;
    t = fall_speed + gravity * ((u8) ENT(obj)->death_timer);
    ENT(obj)->pos_y -= t;
    if (floor_y < ENT(obj)->pos_y) {
        ENT(obj)->pos_y = floor_y;
        return t;
    }
    ENT(obj)->death_timer++;
    return 0;
}

s32 joint_severed_limb_step(u8 *joint, s16 gravity, s16 floor_y, u8 ref_index) {
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    SVECTOR sv;
    u16 d;
    s32 t;

    d = FIELD_AT(joint + ref_index * 0x7C, Joint, ->world.t[1], u16);
    d = d - FIELD_AT(joint, Joint, ->world.t[1], u16) + 200;
    if (JOINT(joint)->world.t[1] >= floor_y && (JOINT(joint)->fx_state & 0x80)) {
        JOINT(joint)->fx_state |= 1;
        return 0;
    }
    if (d >= 0x191 || !(JOINT(joint)->fx_state & 0x80)) {
        sv.vy = 0x80 - (JOINT(joint)->fx_state & 0x80) * 2;
        sv.vx = (0x20 - (JOINT(joint)->fx_state & 0x80)) >> 1;
        sv.vz = 0;
        RotMatrix(&sv, LITP(MATRIX, sp_matrix));
        MulMatrix((MATRIX *)(&JOINT(joint)->world), LITP(MATRIX, sp_matrix));
    }
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
    RotMatrixY(((s16) CURRENT_ENTITY_LIT->angle), LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), (SVECTOR *)(&JOINT(joint)->rotation), (SVECTOR *)(&JOINT(joint)->rotation));
    JOINT(joint)->world.t[0] += JOINT(joint)->rotation.vx;
    JOINT(joint)->world.t[2] += JOINT(joint)->rotation.vz;
    JOINT(joint)->rotation.vy += gravity * JOINT(joint)->fx_frame;
    JOINT(joint)->world.t[1] -= JOINT(joint)->rotation.vy;
    if (JOINT(joint)->world.t[1] >= floor_y) {
        JOINT(joint)->world.t[1] = -0x15E;
        if (JOINT(joint)->fx_state & 0x80) {
            JOINT(joint)->world.t[1] = floor_y;
        }
        JOINT(joint)->fx_frame = 3;
        JOINT(joint)->fx_state = 0x80;
        t = CURRENT_ENTITY_LIT->id;
        LIT(s32, sp_tmp00) = t;
        if (t - 2 < 0 || t == 0x11) {
            JOINT(joint)->flags &= 0xFC;
            FIELD_AT(joint, Joint, [1].flags, u8) &= 0xFC;
        }
    }
    JOINT(joint)->fx_frame++;
    return 0;
}

s32 entity_update_wander_turn(u32 speed, u8 *turn_flags, u8 *counter, s16 turn, u8 limit) {
    s32 get_angle_towards_target_l(s32, s32) __asm__("get_angle_towards_target");
    s32 unused[2];
    u8 *obj;
    u8 *p; /* separate from obj: the original gives them different registers */
    s32 *sc;
    s32 ret;
    s32 d;
    u16 ang;
    u16 cur;
    u8 f;
    s16 a; /* s16 copy: kept raw in s0, sign-extended at each use */

    f = *turn_flags;
    a = turn;
    if (f & 0x80) {
        obj = (u8 *)g_CurrentEntity;
        ENT(obj)->angle += turn - (s16)turn * ((f >> 5) & 2);
        if (ENT(obj)->move_speed_current * 3 / 4 < speed) {
            if (--*counter == 0) {
                *turn_flags = 0;
                *counter = 0;
            }
        }
        return 1;
    }
    if (speed < ENT(g_CurrentEntity)->move_speed_current * 2 / 3) {
        if (limit < ++*counter) {
            *turn_flags |= 0x80;
            *turn_flags |= g_RandSeed & 0x40;
            *counter = limit / 6;
        }
    } else {
        *turn_flags = 0;
        *counter = 0;
    }
    ret = get_angle_towards_target_l(((s16) CURRENT_ENTITY_LIT->waypoint_x), ((s16) CURRENT_ENTITY_LIT->waypoint_z));
    sc = LITP(s32, sp_tmp1C);
    *sc = ret;
    if (a >> 15) {
        a = a * -1;
        *sc = (ret + 0x800) & 0xFFF;
    }
    p = LIT(u8 *, g_CurrentEntity);
    ang = *(u16 *)sc;
    cur = ENT(p)->angle;
    d = (u16)(a + (u16)(ang - cur)) & 0xFFF;
    if (d < a * 2) {
        ENT(p)->angle = ang;
    } else {
        ENT(p)->angle = cur - a;
        if ((u32)d < 0x801) {
            ENT(p)->angle += a << 1;
        }
    }
    return 0;
}

s16 turn_toward_target(void *target, s16 max_turn) {
    s32 get_angle_towards_target_p(s32, s32) __asm__("get_angle_towards_target");
  long new_var;
  s16 turn;
  s32 diff;
  turn = (max_turn + (get_angle_towards_target_p(*((s32 *) (((s8 *) target) + 0)), *((s32 *) (((s8 *) target) + 8))) - (*((u16 *) (((s8 *) g_CurrentEntity) + 0x74))))) & 0xFFF;
  diff = turn;
  new_var = (u32) diff;
  turn = 0;
  if (diff >= (max_turn * 2))
  {
    if (new_var >= 0x801U)
    {
      return (s16) (max_turn * (-1));
    }
    turn = max_turn;
    return max_turn;
  }
  return turn;
}

void entity_rotate_toward_target(s32 *target, s16 max_turn) {
    s32 get_angle_towards_target_l(s32, s32) __asm__("get_angle_towards_target");
    s32 unused[2];
    s32 r;
    s32 *sc;
    u8 *obj;
    u16 ang;
    u16 cur;
    u16 nv;
    s32 d;

    r = get_angle_towards_target_l(target[0], target[2]);
    sc = LITP(s32, sp_tmp1C);
    *sc = r;
    /* sign test written as a shift: the original tests the sll result, not the sra */
    if (max_turn >> 15) {
        max_turn = max_turn * -1;
        *sc = (r + 0x800) & 0xFFF;
    }
    obj = (u8 *)g_CurrentEntity;
    ang = *(u16 *)sc;
    cur = ENT(obj)->angle;
    d = (u16)(max_turn + (u16)(ang - cur)) & 0xFFF;
    if (d < max_turn * 2) {
        ENT(obj)->angle = ang;
    } else {
        nv = cur - max_turn;
        ENT(obj)->angle = nv;
        if ((u32)d < 0x801) {
            ENT(obj)->angle = nv + (max_turn << 1);
        }
    }
}

s16 entity_swerve_around_obstacle(s32 max_turn, u8 room_hit, s16 *swerve_angle, u8 *swerve_latch) {
    s32 get_angle_towards_target_l() __asm__("get_angle_towards_target");
    VECTOR v;
    s32 r;
    void *e;
    s16 ang;
    s32 q;
    s16 cur;
    s16 h;
    s32 d;
    s32 x;
    u16 diff;
    s16 ang0;

    v = LIT(VECTOR, sp_vec);
    r = get_angle_towards_target_l(v.vx, v.vz);
    ang0 = max_turn;
    if (room_hit == 0) {
        if (!(*swerve_latch & 0x80)) {
            *swerve_latch |= 0x80;
        }
    } else if (!(*swerve_latch & 0x80)) {
        if (*swerve_angle == 0) {
            *swerve_angle = get_angle_towards_target_l(v.vx, CURRENT_ENTITY_LIT->pos_z);
            if (!(*(u8 *)LIT(void *, g_CurrentEntity) & 0x10)) {
                *swerve_angle = get_angle_towards_target_l(CURRENT_ENTITY_LIT->pos_x, v.vz);
            }
            h = *swerve_angle;
            diff = (ang0 + (h - CURRENT_ENTITY_LIT->angle)) & 0xFFF;
            *swerve_angle = ang0 * -1;
            if (diff < 0x801) {
                *swerve_angle = ang0;
            }
        }
        *swerve_latch = LIT(u8, sp_tmp1C);
        e = g_CurrentEntity;
        ang = ENT(e)->angle;
        do {
            q = (ang & 0xFFF) / 1024;
            cur = *swerve_angle;
            d = (ang + cur) & 0xFFF;
        } while (0);
        if (q != d / 1024) {
            LIT(s32, sp_tmp08) = cur - d % 1024;
            cur = *swerve_angle;
            if (cur & 0x8000) {
                x = ENT(e)->angle & 0xFFF;
                LIT(s32, sp_tmp08) = cur + (s16)(x % 1024);
            }
            return LIT(s16, sp_tmp08);
        }
        return cur;
    }
    if (*swerve_latch & 0x7F) {
        (*swerve_latch)--;
        h = *swerve_angle;
        diff = (ang0 + (h - g_CurrentEntity->angle)) & 0xFFF;
        *swerve_angle = ang0 * -1;
        if (diff < 0x801) {
            *swerve_angle = ang0;
        }
        return *swerve_angle;
    }
    *swerve_angle = 0;
    *swerve_latch = 0;
    d = (u16)(ang0 + (r - g_CurrentEntity->angle)) & 0xFFF;
    if (ang0 * 2 >= d) {
        return 0;
    }
    if ((u32)d < 0x801) {
        return ang0;
    }
    return ang0 * -1;
}

s32 entity_check_angular_los(s32 half_fov, s32 *target) {
    s32 room_check_sight_blocked_a1() __asm__("room_check_sight_blocked");
    s32 get_angle_towards_target_q(s32, s32) __asm__("get_angle_towards_target");
    VECTOR v;
    s32 t;

    t = (half_fov + (get_angle_towards_target_q(target[0], target[2]) - g_CurrentEntity->angle)) & 0xFFF;
    if (((s32) (half_fov << 0x10) >> 0xF) < t) {
        return 1;
    }
    v.vx = target[0] - g_CurrentEntity->pos_x;
    v.vz = target[2] - g_CurrentEntity->pos_z;
    v.vy = 0;
    return room_check_sight_blocked_a1(&v, g_CurrentEntity->sight_check) & 0xFF;
}

s32 entity_check_view_and_distance(s16 ang, s16 dist, VECTOR *pos) {
    MATRIX *RotMatrixY(long r, MATRIX *m);
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    MATRIX m;
    VECTOR d;
    VECTOR r1;
    VECTOR r2;
    VECTOR v = { 2000, 0, 0 }; /* rotated by +-angle: the edges of the view cone */
    s32 ax;
    s32 az;

    d.vx = pos->vx - CURRENT_ENTITY_LIT->pos_x;
    ax = __builtin_abs(d.vx);
    d.vz = pos->vz - CURRENT_ENTITY_LIT->pos_z;
    d.vy = 0;
    az = __builtin_abs(d.vz);
    if (dist >= ax + az) {
        m = g_IdentityMatrix_l;
        RotMatrixY(((s16) CURRENT_ENTITY_LIT->angle) - ang, &m);
        ApplyMatrixLV(&m, &v, &r1);
        RotMatrixY(ang * 2, &m);
        ApplyMatrixLV(&m, &v, &r2);
        OuterProduct0(&r1, &d, &r1);
        OuterProduct0(&r2, &d, &r2);
        return (u32)(r1.vy & 0x80000000) < (u32)(r2.vy & 0x80000000);
    }
    return 0;
}

u8 entity_resolve_enemy_player_collisions(void) {
    extern s16 g_PlayerPositionVx_s16 __asm__("g_PlayerPositionVx");
    extern s16 g_PlayerPositionVz_s16 __asm__("g_PlayerPositionVz");
    s32 unused[4];
    u8 r;
    u8 n;
    u8 *p;
    u8 *flag;
    s32 dx;
    s32 dz;
    s32 d;

    r = 0;
    n = g_EnemyCount;
    p = LITP(u8, g_EnemiesList);
    while (n != 0) {
        if (*p != 0) {
            n--;
            if ((u8 *)g_CurrentEntity != p) {
                r |= check_entity_collision(p, (u8 *)g_CurrentEntity);
            }
        }
        p += ENTITY_SIZE;
    }
    flag = LITP(u8, g_PlayerEntity);
    dx = g_PlayerPosX - g_PlayerPositionVx_s16;
    d = __builtin_abs(dx);
    dz = g_PlayerPosZ - g_PlayerPositionVz_s16;
    d += __builtin_abs(dz);
    LIT(s32, sp_tmp00) = d;
    if (d >= 0x1C3 && !(*flag & 2) && (g_EnemySlot0_Id == 0xD || g_EnemySlot0_Id == 0x12)) {
        LIT(s32, sp_tmp00) = dx >> 4;
        LIT(s32, sp_tmp04) = dz >> 4;
        g_PlayerPosX = g_PlayerPositionVx_s16 + LIT(s32, sp_tmp00);
        g_PlayerPosZ = g_PlayerPositionVz_s16 + LIT(s32, sp_tmp04);
    }
    return r;
}

void snap_player_to_grab_position(void *player) {
    s32 entity_extract_anim_vertex_a1(void *, s32, s32, s32) __asm__("entity_extract_anim_vertex");
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    entity_extract_anim_vertex_a1(e, ENT(e)->anim_header, ENT(e)->anim_base, 0);
    LIT(MATRIX, sp_matrix) = *(MATRIX *)(&g_IdentityMatrix);
    RotMatrixY((s32) ((s16) CURRENT_ENTITY_LIT->angle), LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, sp_svec));
    e2 = LIT(void *, g_CurrentEntity);
    ENT(e2)->anim_base_x = (u16) (FIELD_AT(e2, Entity, ->pos_x, u16) - LIT(u16, sp_svec_vx));
    ENT(e2)->anim_base_z = (u16) (FIELD_AT(e2, Entity, ->pos_z, u16) - LIT(u16, sp_svec_vz));
    ENT(player)->anim_base_x = (u16) ENT(e2)->anim_base_x;
    ENT(player)->anim_base_z = (u16) ENT(e2)->anim_base_z;
}

void entity_apply_anim_vertex(void *ent, s32 anim_header, s32 anim_base) {
    s32 entity_extract_anim_vertex_a2() __asm__("entity_extract_anim_vertex");
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    entity_extract_anim_vertex_a2(ent, anim_header, anim_base, 0);
    LIT(MATRIX, sp_matrix) = *(MATRIX *)(&g_IdentityMatrix);
    RotMatrixY((s32) FIELD_AT(ent, Entity, ->angle, s16), LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, sp_svec));
    FIELD_AT(ent, Entity, ->pos_x, s32) = (s32) (FIELD_AT(ent, Entity, ->anim_base_x, u16) + LIT(s16, sp_svec_vx));
    FIELD_AT(ent, Entity, ->pos_z, s32) = (s32) (FIELD_AT(ent, Entity, ->anim_base_z, u16) + LIT(s16, sp_svec_vz));
}

s32 get_player_input_masked(void) {
    return g_PadTrigger & 0xF0F0;
}

s32 entity_is_target_on_side(VECTOR *target) {
    MATRIX *RotMatrixY(long r, MATRIX *m);
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    VECTOR d;
    VECTOR v = { 5000, 0, 0 }; /* rotated by angle + 0x400 for the left/right test */
    u8 *e;

    e = LIT(u8 *, g_CurrentEntity);
    d.vx = target->vx - ENT(e)->pos_x;
    d.vz = target->vz - ENT(e)->pos_z;
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
    d.vy = 0;
    RotMatrixY(*(s16 *)(LIT(u8 *, g_CurrentEntity) + 0x74) + 0x400, LITP(MATRIX, sp_matrix));
    ApplyMatrixLV(LITP(MATRIX, sp_matrix), &v, &v);
    OuterProduct0(&d, &v, &v);
    return (u32)v.vy >> 31;
}

s32 is_facing_toward_entity(void *other) {
    return (((((s16) ENT(other)->angle) - ((s16) g_CurrentEntity->angle)) + 0x400) & 0xFFF) < 0x800;
}

void joint_setup_attack_effect(void *joint, s8 duration, s16 fall_speed, s16 stride) {
    s32 joint_enable_special_effect_a1(void *, s32, s32, s32) __asm__("joint_enable_special_effect");
    s32 g8;
    s32 b8;
    s32 r8;
    s32 r;
    s32 g;
    s32 b;
    void *mirror;
    void *e;

    if (!(JOINT(joint)->fx_frame & 0x80)) {
        r = 0x60;
        g = 0x28;
        b = 0x28;
        if ((u32) (CURRENT_ENTITY_LIT->id - 3) < 2U) {
            r = 0x18;
            g = 0x30;
            b = 0x18;
        }
        r8 = r & 0xFF;
        g8 = g & 0xFF;
        b8 = b & 0xFF;
        joint_enable_special_effect_a1(joint, r8, g8, b8);
        JOINT(joint)->fx_state = duration;
        JOINT(joint)->vel_x = fall_speed;
        JOINT(joint)->vel_y = stride;
        if (g_GameStateFlags & GAME_MIRROR_ON) {
            e = LIT(void *, g_CurrentEntity);
            mirror = (joint - ENT(e)->joints) + ENT(e)->mirror_joints;
            LIT(void *, sp_tmp18) = mirror;
            joint_enable_special_effect_a1(mirror, r8, g8, b8);
            JOINT(mirror)->fx_state = duration;
            JOINT(mirror)->vel_x = fall_speed;
            JOINT(mirror)->vel_y = stride;
        }
    }
}

void joint_enable_special_effect(void *joint, u8 r, u8 g, u8 b) {
    void SetPolyGT3(POLY_GT3 *p);
    POLY_GT3 *p;
    u16 i;
    u8 flags;
    u8 *q;

    flags = JOINT(joint)->flags;
    if (flags & 1) {
        i = 0;
        JOINT(joint)->flags = (flags & 0xFE) | 0x28;
        q = ((u8 *) JOINT(joint)->anim_object);
        LIT(s32, sp_tmp00) = TMD_OBJ(((void *) JOINT(joint)->anim_slot))->n_primitive * 2;
        for (p = (POLY_GT3 *)(q + 8); i < LIT(s32, sp_tmp00); p++, i++) {
            SetPolyGT3(p);
            p->r2 = r;
            p->r1 = r;
            p->r0 = r;
            p->g2 = g;
            p->g1 = g;
            p->g0 = g;
            p->b2 = b;
            p->b1 = b;
            p->b0 = b;
        }
    }
}

typedef struct {
    u8 pad0[0xC];
    u32 c0;
    u8 pad10[8];
    u32 c1;
    u8 pad1C[8];
    u32 c2;
} Prim28;
typedef struct {
    u8 flags;
    u8 pad1[0x13];
    u8 *hdr;
    Prim28 *prims;
} Obj1C;

void joint_apply_color_tint(Obj1C *obj, u32 col0, u32 col1, u32 col2) {
    Obj1C *o;
    Prim28 *p;
    u16 i;
    u8 *h;

    obj->flags |= 0x80;
    p = obj->prims;
    h = obj->hdr;
    LIT(s32, sp_tmp00) = TMD_OBJ(h)->n_primitive * 2;
    for (i = 0; i < LIT(s32, sp_tmp00); i++) {
        {
            u32 t = p->c0 & 0xFF000000;
            p->c0 = (col0 & 0xFFFFFF) | t;
        }
        {
            u32 t = p->c1 & 0xFF000000;
            p->c1 = (col1 & 0xFFFFFF) | t;
        }
        {
            u32 t = p->c2 & 0xFF000000;
            p->c2 = (col2 & 0xFFFFFF) | t;
        }
        p++;
    }
    if (g_GameStateFlags & GAME_MIRROR_ON) {
        do {
            o = (Obj1C *)((u8 *)obj - ((u8 *) ENT(g_CurrentEntity)->joints) + ENT(g_CurrentEntity)->mirror_joints);
        } while (0);
        LIT(Obj1C *, sp_tmp18) = o;
        o->flags |= 0x80;
        p = o->prims;
        h = o->hdr;
        LIT(s32, sp_tmp00) = TMD_OBJ(h)->n_primitive * 2;
        for (i = 0; i < LIT(s32, sp_tmp00); i++) {
            {
                u32 t = p->c0 & 0xFF000000;
                p->c0 = (col0 & 0xFFFFFF) | t;
            }
            {
                u32 t = p->c1 & 0xFF000000;
                p->c1 = (col1 & 0xFFFFFF) | t;
            }
            {
                u32 t = p->c2 & 0xFF000000;
                p->c2 = (col2 & 0xFFFFFF) | t;
            }
            p++;
        }
    }
}

void joint_path_anim_step(u8 *joint) {
    void SetRotMatrix(MATRIX *m);
    void SetTransMatrix(MATRIX *m);
    long RotAverage3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *flag);
    void AddPrim(void *ot, void *p);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    typedef struct { u8 pad0[0x12]; u16 a; u8 pad14[2]; u16 b; u8 pad18[2]; u16 c; } Face1C;
    /* each triangle's packet is in the model's packet area twice (one per frame buffer): prim points to
       this frame's copy, so the next triangle is 0x50 bytes on */
    typedef struct { POLY_GT3 p; POLY_GT3 other_buffer; } Gt3Pair;
    s32 is_point_in_quad_l(void *, s32) __asm__("is_point_in_quad");
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    MATRIX m;
    MATRIX lm;
    long p;
    long flag;
    u8 *model;
    Face1C *f;
    Gt3Pair *prim;
    u16 *v;
    s32 a;
    s32 b;
    u32 z;
    u8 t;
    u8 *base;

    model = ((u8 *) JOINT(joint)->anim_slot);
    base = ((u8 *) JOINT(joint)->anim_object);
    f = ((Face1C *) TMD_OBJ(model)->primitive_top);
    lm = g_IdentityMatrix_MATRIX;
    LIT(s32, sp_tmp04) = TMD_OBJ(model)->n_primitive;
    t = JOINT(joint)->fx_frame;
    LIT(s32, sp_tmp00) = t;
    LIT(s32, sp_tmp08) = JOINT(joint)->fx_state - t;
    prim = (Gt3Pair *)(g_ActiveBuffer * 0x28 + (s32) base + 8);
    LIT(s32, sp_tmp0C) = JOINT(joint)->vel_y;
    JOINT(joint)->world.t[1] += JOINT(joint)->vel_x * t;
    if (JOINT(joint)->world.t[1] > 10000) {
        JOINT(joint)->world.t[1] = 10000;
    }
    if (is_point_in_quad_l(joint + 0x58, (s32)g_CameraSwitchZones) != 0) {
        GsMulCoord0(&g_WorldScreenMatrix, (MATRIX *)(&JOINT(joint)->world), &m);
        prim += ({ LIT(s32, sp_tmp04) - 1; });
        f += LIT(s32, sp_tmp04) - 1;
        while (--LIT(s32, sp_tmp04) != -1) {
            v = (u16 *)(f->a * 8 + TMD_OBJ(model)->vert_top);
            if (LIT(s32, sp_tmp04) % LIT(s32, sp_tmp0C) == 0) {
                a = LIT(s32, sp_tmp00);
                b = LIT(s32, sp_tmp08);
                lm.t[0] = ((s16) v[0] >> 1) * a + ((s16) v[0] >> 4) * b;
                lm.t[1] = ((s16) v[1] >> 1) * a + ((s16) v[1] >> 4) * b;
                lm.t[2] = ((s16) v[2] >> 1) * a + ((s16) v[2] >> 4) * b;
                GsMulCoord0(&m, &lm, LITP(MATRIX, sp_matrix));
                SetTransMatrix(LITP(MATRIX, sp_matrix));
                SetRotMatrix(LITP(MATRIX, sp_matrix));
            }
            z = RotAverage3((SVECTOR *)v, (SVECTOR *)(f->b * 8 + TMD_OBJ(model)->vert_top),
                            (SVECTOR *)(f->c * 8 + TMD_OBJ(model)->vert_top),
                            (long *)&prim->p.x0, (long *)&prim->p.x1, (long *)&prim->p.x2, &p, &flag);
            if (z >= 0xFF0) {
                z = 0xFF0;
            }
            AddPrim(LITP(GsOT, g_OtMain)[g_ActiveBuffer].org + (z >> 4), prim--);
            f--;
        }
    }
    if (JOINT(joint)->fx_state < JOINT(joint)->fx_frame++) {
        JOINT(joint)->fx_frame = 0x80;
        JOINT(joint)->flags &= 0xDE;
    }
}

void model_jitter_vertices(u8 *obj) {
    int rand(void);
    u8 *t;
    s32 n;
    u8 *base;

    t = ((u8 *) MODEL_OBJ(obj)->tmd);
    n = TMD_OBJ(t)->n_vert;
    base = *(u8 **)t;
    while (n-- != 0) {
        if (!(rand() & 0xF)) {
            SVEC(base)->vy += 0x1E;
        }
        base += 8;
    }
}

void model_scroll_selected_uvs(u8 *joint) {
    s32 n;
    s32 i;
    u8 *p;
    s32 j;

    i = 0;
    n = TMD_OBJ(((u8 *) JOINT(joint)->anim_slot))->n_primitive;
    p = ((u8 *) JOINT(joint)->anim_object) + g_ActiveBuffer * 0x28;
    j = 0;
    while (n-- != 0) {
        if (i == g_UvScrollPrimIndices[j]) {
            j++;
            p[0x14] += 2;
            p[0x20] += 2;
            p[0x2C] += 2;
        }
        i++;
        p += 0x50;
    }
}

u8 check_line_of_sight(s32 *target) {
    u32 boundary_get_cell(void *pos, void *ofs, s32 split_x, s32 split_z);
    s32 room_check_sight_blocked_a2() __asm__("room_check_sight_blocked");
    VECTOR v;
    s16 *p;
    u8 r;

    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vx) = 0;
    p = ROOM_FIELD_AS(collision, s16 *);
    r = boundary_get_cell(target, LITP(void, sp_svec), p[0], p[1]);
    v.vx = target[0] - g_CurrentEntity->pos_x;
    v.vy = target[1] - g_CurrentEntity->pos_y;
    v.vz = target[2] - g_CurrentEntity->pos_z;
    LIT(s32, sp_tmp04) = room_check_sight_blocked_a2(&v, r);
    return LIT(u8, sp_tmp04);
}

u8 check_weapon_line_of_sight(void *target) {
    s32 room_check_sight_blocked_a3() __asm__("room_check_sight_blocked");
    s32 delta[3];
    u8 i;
    u8 r;

    delta[0] = VEC(target)->vx - g_CurrentEntity->pos_x;
    r = 0;
    delta[1] = VEC(target)->vy - g_CurrentEntity->pos_y;
    i = 3;
    delta[2] = VEC(target)->vz - g_CurrentEntity->pos_z;
    do {
        r |= room_check_sight_blocked_a3(delta, i);
    } while (i--);
    return r;
}

void entity_add_speed_xz(s16 angle_ofs) {
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    void *e;

    LIT(u16, sp_svec_vx) = ((u16) CURRENT_ENTITY_LIT->move_speed_current);
    LIT(s16, sp_svec_vz) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(MATRIX, sp_matrix) = *(MATRIX *)(&g_IdentityMatrix);
    RotMatrixY(((s16) CURRENT_ENTITY_LIT->angle) + angle_ofs, LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LIT(void *, g_CurrentEntity) + 0x78);
    e = LIT(void *, g_CurrentEntity);
    ENT(e)->pos_x = (s32) (ENT(e)->speed_vx + ENT(e)->pos_x);
    ENT(e)->pos_y = (s32) (ENT(e)->speed_vy + ENT(e)->pos_y);
    ENT(e)->pos_z = (s32) (ENT(e)->speed_vz + ENT(e)->pos_z);
}

void entity_clone(u8 n, s32 unused_arg, u8 idx, Entity **out) {
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    int rand(void);
    Entity *p;

    LIT(s32, sp_tmp00) = idx * 0x1C + JOINT(((u8 *) CURRENT_ENTITY_LIT->joints))->anim_slot;
    *out = LIT(Entity *, g_LoadPointer);
    LIT(u8 *, g_LoadPointer) += n * ENTITY_SIZE;
    p = *out;
    LIT(MATRIX, sp_matrix) = *(MATRIX *)((u8 *)LIT(Entity *, g_CurrentEntity) + 0x20);
    do {
        *p = *g_CurrentEntity;
        FIELD_AT(p, Entity, ->coord_m, MATRIX) = LIT(MATRIX, sp_matrix);
        p->model_load_buffer = SCRATCH->tmp00;
        p->angle += n << 8;
        p->packet = LIT(s32, g_LoadPointer);
        GsLinkObject5(p->model_load_buffer, (GsDOBJ5 *)((u8 *)p + 0xC), 0);
        LIT(u_long *, g_LoadPointer) = GsPresetObject((GsDOBJ5 *)((u8 *)p + 0xC), (u_long *)p->packet);
        p->state = 0;
        p->action_state = (rand() & 3) == 0;
        p++;
    } while (--n);
}

__asm__("#JTALIGN2");
void web_spinner_update_webs(s32 count) {
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    s32 is_point_in_quad_l(void *, s32) __asm__("is_point_in_quad");
    void billboard_draw_l(u8 *, u8 *, s32, s16) __asm__("billboard_draw");
    void billboard_init_l(void *, u8 *, s32, s32) __asm__("billboard_init");
    void effect_create_billboard_l(s32, s32, s32, u8 *, void *, s32) __asm__("effect_create_billboard");
    void sound_play_entity_se_l(s32, s32) __asm__("sound_play_entity_se");
    void check_room_collision_l(u8 *, u16) __asm__("check_room_collision");
    extern u8 g_PlayerActionBehavior_l __asm__("g_PlayerActionBehavior");
    u8 *save;
    u8 *p1;
    u8 *p2;
    u8 *p3;
    u8 *p4;
    u8 *p5;
    u8 *p6;
    u8 *p7;
    u8 *p8;
    u8 *p9;
    u8 *p10;
    u8 *p11;
    u8 *p12;
    u8 *p13;
    u8 *p14;
    u8 *p15;
    u8 *p16;
    u8 *p17;
    u8 *ls;
    s32 r;
    s16 t;
    u8 k;
    u16 c;

    save = LIT(u8 *, g_CurrentEntity);
    LIT(u8 *, g_CurrentEntity) = ((u8 *) ENT(save)->aim_target);
    do {
        LIT(u8 *, g_CurrentEntity)[3] = is_point_in_quad_l(LIT(u8 *, g_CurrentEntity) + 0x34, (s32)g_CameraSwitchZones);
        p1 = LIT(u8 *, g_CurrentEntity);
        if (ENT(p1)->status_flags == 0) {
            if (ENT(p1)->has_enter_switch_zone != 0) {
                billboard_draw_l((u8 *)&ENT(p1)->pos_x, (u8 *)&ENT(p1)->shadow, 0, ((s16) ENT(p1)->angle));
            }
            LIT(u8 *, g_CurrentEntity) += ENTITY_SIZE;
            continue;
        } else {
            if (g_UpdateFlags & 4) {
                LIT(s32, sp_tmp00) = __builtin_abs(g_PlayerPosX - ENT(p1)->pos_x)
                                   + __builtin_abs(g_PlayerPosZ - ENT(p1)->pos_z);
                if (WS_WEB(p1)->se_cooldown != 0) {
                    WS_WEB(p1)->se_cooldown--;
                }
                if (LIT(s32, sp_tmp00) < 0x190 && g_PlayerMoveSpeedCurrent != 0) {
                    p2 = LIT(u8 *, g_CurrentEntity);
                    if (ENT(p2)->state != 5) {
                        s32 *vec = LITP(s32, sp_vec);

                        ENT(p2)->status_flags = 0;
                        SCRATCH->tmp1C = 0xFF55DF;
                        SCRATCH->sv_z = 0;
                        SCRATCH->sv_y = 0;
                        SCRATCH->sv_x = 0;
                        billboard_init_l(LITP(void, sp_svec), LIT(u8 *, g_CurrentEntity) + 0xE4, 300, 300);
                        SCRATCH->vec_z = 0;
                        SCRATCH->vec_y = 0;
                        *vec = 0;
                        effect_create_billboard_l(0, 10, 0, LIT(u8 *, g_CurrentEntity) + 0x20, vec, 0);
                        if (WS_WEB_CUR->se_cooldown == 0) {
                            sound_play_entity_se_l(6, 0);
                            WS_WEB_CUR->se_cooldown = 0x2D;
                            pad_vibrate_small(1, 0);
                            pad_vibrate_large(4, 0x5A, 0);
                        }
                        LIT(u8 *, g_CurrentEntity)[0x84] = 6;
                    }
                }
                switch (ENT(g_CurrentEntity)->state) {
                case 0:
                    CURRENT_ENTITY_LIT_FIELD_AS(action_ticks_counter, s16) = rand() & 0x3F;
                    WS_WEB_CUR->drift = rand() & 0x3F;
                    r = rand() & 1;
                    p3 = LIT(u8 *, g_CurrentEntity);
                    ENT(p3)->state = 1;
                    WS_WEB(p3)->drift = -WS_WEB(p3)->drift * r;
                    r = rand();
                    p4 = LIT(u8 *, g_CurrentEntity);
                    ENT(p4)->move_speed_current = (r & 0x1F) + 100;
                    WS_WEB(p4)->swing = 0x80;
                case 1:
                    t = turn_toward_target(LITP(void, g_PlayerPosX), 0x3E);
                    LIT(s32, sp_tmp1C) = t;
                    if (g_PlayerMoveSpeedCurrent != 0 && ENT(g_CurrentEntity)->action_state != 0) {
                        LIT(s32, sp_tmp1C) = -t;
                    }
                    ENT(g_CurrentEntity)->angle += LIT(u16, sp_tmp1C) + WS_WEB(g_CurrentEntity)->drift;
                    ENT(g_CurrentEntity)->angle += ((u16) WS_WEB(g_CurrentEntity)->swing);
                    WS_WEB(g_CurrentEntity)->swing = -((u16) WS_WEB(g_CurrentEntity)->swing);
                    c = ENT(g_CurrentEntity)->action_ticks_counter;
                    ENT(g_CurrentEntity)->action_ticks_counter = ({ c - 1; });
                    if (c == 0) {
                        k = 2;
                        if (LIT(s32, sp_tmp1C) == 0) {
                            k = 4;
                        }
                        ENT(g_CurrentEntity)->state = k;
                    }
                    break;
                case 2:
                    r = rand();
                    p6 = LIT(u8 *, g_CurrentEntity);
                    ENT(p6)->action_ticks_counter = r & 0x1F;
                    ENT(p6)->state = 3;
                    p7 = LIT(u8 *, g_CurrentEntity);
                    ENT(p7)->move_speed_current = 0;
                    WS_WEB(p7)->swing = 0;
                case 3:
                    c = ENT(g_CurrentEntity)->action_ticks_counter;
                    ENT(g_CurrentEntity)->action_ticks_counter = ({ c - 1; });
                    if (c == 0) {
                        ENT(g_CurrentEntity)->state = 0;
                    }
                    break;
                case 4:
                    r = rand();
                    p9 = LIT(u8 *, g_CurrentEntity);
                    ENT(p9)->move_speed_current = (r & 0x1F) + 0x8C;
                    ENT(p9)->state = 5;
                    LIT(u8 *, g_CurrentEntity)[0xBC] = 0;
                    r = rand();
                    p10 = LIT(u8 *, g_CurrentEntity);
                    WS_WEB(p10)->drift = (r & 0x7F) + 100;
                    WS_WEB(p10)->hit_done = 0;
                    WS_WEB(p10)->swing = 0;
                case 5:
                    p11 = LIT(u8 *, g_CurrentEntity);
                    if (entity_ballistic_step(ENT(p11)->move_speed_current, ((s16) WS_WEB(p11)->drift), -30, 0) != 0) {
                        LIT(u8 *, g_CurrentEntity)[0x84] = 2;
                    }
                    if (SCRATCH->tmp00 < 0x320) {
                        p12 = LIT(u8 *, g_CurrentEntity);
                        if (WS_WEB(p12)->hit_done == 0) {
                            s32 *vec = LITP(s32, sp_vec);

                            WS_WEB(p12)->hit_done = 1;
                            ENT(p12)->angle += 0x400;
                            SCRATCH->vec_z = 0;
                            SCRATCH->vec_y = 0;
                            *vec = 0;
                            effect_create_billboard_l(0, 0, 0, p12 + 0x20, vec, 0);
                            CURRENT_ENTITY_LIT_FIELD_AS(move_speed_current, u16) -= 0x3C;
                            g_PlayerHealth -= 3;
                            pad_vibrate_small(3, 0);
                            if ((s16)g_PlayerHealth < 0) {
                                g_PlayerHealth = 1;
                            }
                            g_PlayerActionBehavior_l = 100;
                            g_PlayerHitState = 1;
                        }
                    }
                    break;
                }
                p13 = LIT(u8 *, g_CurrentEntity);
                RotMatrix((SVECTOR *)(p13 + 0x72), (MATRIX *)(p13 + 0x20));
                p14 = LIT(u8 *, g_CurrentEntity);
                if (ENT(p14)->state < 6) {
                    entity_add_speed_xz(WS_WEB(p14)->swing);
                }
            }
            p15 = LIT(u8 *, g_CurrentEntity);
            check_room_collision_l((u8 *)&ENT(p15)->pos_x, SCA(((u8 *) ENT(p15)->sca_info))->radius);
            p16 = LIT(u8 *, g_CurrentEntity);
            ENT(p16)->position_vx = FIELD_AT(p16, Entity, ->pos_x, u16);
            ENT(p16)->position_vy = FIELD_AT(p16, Entity, ->pos_y, u16);
            ENT(p16)->position_vz = FIELD_AT(p16, Entity, ->pos_z, u16);
            GsMulCoord0(&g_WorldScreenMatrix, (MATRIX *)(p16 + 0x20), LITP(MATRIX, sp_matrix));
            LIT(u8 *, sp_tmp38) = CURRENT_ENTITY_LIT_FIELD_AS(model_load_buffer, u8 *);
            GsSetLsMatrix(LITP(MATRIX, sp_matrix));
            p17 = LIT(u8 *, g_CurrentEntity);
            if (ENT(p17)->has_enter_switch_zone != 0) {
                ls = LIT(u8 *, sp_tmp38);
                GsPrstTG3NL(((s32) TMD_OBJ(ls)->primitive_top), ((s32) TMD_OBJ(ls)->vert_top), ((s32) TMD_OBJ(ls)->normal_top),
                            LIT(u8, g_ActiveBuffer) * 0x28 + ENT(p17)->packet + 8, TMD_OBJ(ls)->n_primitive, 4,
                            LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain);
            }
        }
        LIT(u8 *, g_CurrentEntity) += ENTITY_SIZE;
    } while (--count & 0xFF);
    g_CurrentEntity = (Entity *)save;
}

s32 entity_pathfind_update(void) {
    s32 entity_check_angular_los_l() __asm__("entity_check_angular_los");
    s32 out_of_sight;
    u8 sight2;
    u8 next_sight;
    u8 sight;
    void *e;
    void *e2;
    void *e3;
    void *e4;

    e = LIT(void *, g_CurrentEntity);
    sight = FIELD_AT(e, Entity, ->sight_check, u8);
    if ((u32) (sight & 0x1F) < 4U) {
        out_of_sight = entity_check_angular_los_l(0x5E8, ADDR_g_PlayerPosX);
        e2 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e2, Entity, ->sight_check, u8) |= out_of_sight << 5;
        e3 = LIT(void *, g_CurrentEntity);
        sight2 = FIELD_AT(e3, Entity, ->sight_check, u8);
        if ((sight2 & 0x1F) == 3) {
            if (!(sight2 & 0x20)) {
                FIELD_AT(e3, Entity, ->waypoint_x, u16) = (u16) (*(u16 *)&g_PlayerEntity.pos_x);
                FIELD_AT(e3, Entity, ->waypoint_z, u16) = (u16) (*(u16 *)&g_PlayerEntity.pos_z);
                FIELD_AT(e3, Entity, ->sight_check, u8) = (u8) (FIELD_AT(e3, Entity, ->sight_check, u8) + 1);
                e3 = LIT(void *, g_CurrentEntity);
                FIELD_AT(e3, Entity, ->sight_check, u8) &= 0xDF;
                return 1;
            }
            FIELD_AT(e3, Entity, ->sight_check, u8) = (u8) (sight2 + 1);
            e3 = LIT(void *, g_CurrentEntity);
            FIELD_AT(e3, Entity, ->sight_check, u8) &= 0xDF;
            return 0;
        }
        FIELD_AT(e3, Entity, ->sight_check, u8) = (u8) (sight2 + 1);
        return 2;
    }
    next_sight = sight + 1;
    FIELD_AT(e, Entity, ->sight_check, u8) = next_sight;
    if ((u32) (next_sight & 0x1F) >= 0x10U) {
        e4 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e4, Entity, ->sight_check, u8) = (u8) (FIELD_AT(e4, Entity, ->sight_check, u8) & 0xC0);
    }
    return 2;
}

s32 joint_check_player_in_range(MATRIX *mat, VECTOR *offset, s32 range, VECTOR *target) {
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    MATRIX m;
    u16 d;

    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_l;
    LITP(MATRIX, sp_matrix)->t[0] = offset->vx;
    LITP(MATRIX, sp_matrix)->t[1] = offset->vy;
    LITP(MATRIX, sp_matrix)->t[2] = offset->vz;
    GsMulCoord0(mat, LITP(MATRIX, sp_matrix), &m);
    d = m.t[0] - range;
    d = target->vx - d;
    if (d <= (s16)range * 2) {
        d = m.t[2] - range;
        d = target->vz - d;
        return d <= (s16)range * 2;
    }
    return 0;
}

u8 rand_lfsr_next(void) {
  u16 seed;
  int new_var;
  u32 shifted;
  seed = LIT(u16, g_RandSeed);
  shifted = ((u16) (LIT(u16, g_RandSeed))) >> 1;
  LIT(u16, g_RandSeed) = (u16) shifted;
  new_var = 8;
  if ((((u32) (seed & 0x202)) >> new_var) == (seed & 2))
  {
    LIT(u16, g_RandSeed) = shifted | 0x8000;
  }
  return (u8) (seed = LIT(u16, g_RandSeed));
}
