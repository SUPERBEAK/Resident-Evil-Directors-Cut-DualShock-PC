/* room_collision.c - Room collision: the boundary shapes, point / circle checks against walls, line-of-sight blocking. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


void room_setup_collision_callbacks(void) {
    void collision_push_rect(s16 *box, VECTOR *pos, SVECTOR *old);
    s32 collision_push_circle();
    void collision_flag_set(void);
    s32 *q;
    u8 *data;
    s32 n;
    s32 t;
    s32 c;

    q = ((s32 *) ROOM->collision);
    data = (u8 *)q + 0x18;
    q++;
    n = *q;
    *q++ = (s32)data;
    LIT(s32, sp_tmp00) = 0;
    do {
        t = *q;
        *q++ = (s32)(data + n * 0xC);
        c = LIT(s32, sp_tmp00) + 1;
        LIT(s32, sp_tmp00) = c;
        n += t;
    } while (c < 4);
    g_CollisionShapeFuncs[1] = &collision_push_rect;
    g_CollisionShapeFuncs[3] = &collision_push_circle;
    g_CollisionShapeFuncs[4] = &collision_flag_set;
    g_CollisionShapeFuncs[5] = &collision_push_rect;
}

s32 boundary_point_outside(s16 *, s32, s32, s32, s32);

s32 boundary_classify(s16 *ofs, u16 *box, u16 radius) {
    if (boundary_point_outside(ofs, box[0] + radius, box[1] + radius, box[2] - radius, box[3] - radius) == 0) {
        return FIELD_AT(box, ScaBoundary, ->type, u8);
    }
    return -1;
}

s16 boundary_classify_flags(s16 *ofs, u16 *box, u16 radius) {
    if (boundary_point_outside(ofs, box[0] + radius, box[1] + radius, box[2] - radius, box[3] - radius) == 0) {
        return box[5] & 0xFF00;
    }
    return -1;
}

u32 boundary_get_cell(void *pos, void *ofs, s32 split_x, s32 split_z) {
    return (u32) ((((SVEC(ofs)->vz + VEC(pos)->vz) - split_z) & 0x80000000) | ((u32) (((SVEC(ofs)->vx + VEC(pos)->vx) - split_x) & 0x80000000) >> 1)) >> 0x1E;
}

s32 boundary_point_outside(s16 *ofs, s32 min_x, s32 min_z, s32 max_x, s32 max_z) {
    s32 y;
    s32 x;
    u32 r;

    y = ofs[2] + LIT(s32, sp_vec_vz);
    r = ((min_z - y) & 0x80000000) | ((u32)((y - max_z) & 0x80000000) >> 1);
    x = ofs[0] + LIT(s32, sp_vec_vx);
    r |= (u32)((min_x - x) & 0x80000000) >> 2;
    return r | ((u32)((x - max_x) & 0x80000000) >> 3);
}

s32 check_room_collision(VECTOR *pos, u16 radius) {
    long SquareRoot0(long a);
    s16 boundary_get_cell_l(void *, void *, s32, s32) __asm__("boundary_get_cell");
    s16 boundary_classify_l() __asm__("boundary_classify");
    extern void (*g_CollisionShapeFuncs_l[])(u32, s32 *, u16 *) __asm__("g_CollisionShapeFuncs");
    long SquareRoot0(long a);
    SVECTOR sv = { 0, 0, 0, 0 };
    Entity *obj;
    u32 *q;
    u32 *e;
    u32 p;
    u16 *h;
    s16 r;
    s16 hit;
    s16 res;
    u16 lo;
    u16 hi;
    u32 b;
    s32 *flag;
    s32 dx;
    s32 dz;

    hit = 0;
    res = 0;
    LIT(Entity *, g_CurrentEntity)->collision_flags &= 0xF7;
    if (LIT(Entity *, g_CurrentEntity)->status_flags & 4) {
        return 0;
    }
    r = boundary_get_cell_l(pos, &sv, (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[0], (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[1]);
    q = (u32 *)(({ r * 4 + 4; }) + *(s32 *)(LIT(u8 *, g_RoomData) + 0x4C));
    LIT(s16, sp_svec_vx) = LIT(s16, sp_svec_vy) = LIT(s16, sp_svec_vz) = 0;
    LIT(VECTOR, sp_vec) = *pos;
    LIT(s32, sp_tmp1C) = 0;
    e = q + 1;
    p = *q;
    if (p < *e) {
        h = (u16 *)(&SCA_BOUND(p)->floor);
        do {
            if ((LIT(Entity *, g_CurrentEntity)->collision_flags & 4) && !(*h & 0x100)) {
                lo = *h & 0xFF;
                hi = h[-1];
                h += 6;
                p += 12;
                res = ((s16)(((hi & 0x7F00) >> 8) * 1000 + lo * 100) * -(hi & 0x8000) >> 15) | 1;
            } else {
                if ((u8)h[-1] != 5 || !(LIT(Entity *, g_CurrentEntity)->collision_flags & 0x10)) {
                    r = boundary_classify_l(LITP(s16, sp_svec), p, radius);
                    if (r != -1) {
                        obj = LIT(Entity *, g_CurrentEntity);
                        g_CollisionShapeFuncs_l[r](p, &obj->pos_x, &obj->position_vx);
                        hit |= (*h & 0x300) >> 8;
                    }
                }
                h += 6;
                p += 12;
            }
        } while (p < *e);
    }
    if (hit == 0) {
        dx = LIT(Entity *, g_CurrentEntity)->pos_x - (s16)LIT(Entity *, g_CurrentEntity)->position_vx;
        dz = LIT(Entity *, g_CurrentEntity)->pos_z - (s16)LIT(Entity *, g_CurrentEntity)->position_vz;
        LIT(s32, sp_tmp18) = SquareRoot0(dx * dx + dz * dz);
        LIT(Entity *, g_CurrentEntity)->position_vx = LIT(Entity *, g_CurrentEntity)->pos_x;
        LIT(Entity *, g_CurrentEntity)->position_vy = LIT(Entity *, g_CurrentEntity)->pos_y;
        LIT(Entity *, g_CurrentEntity)->position_vz = LIT(Entity *, g_CurrentEntity)->pos_z;
        if (res != 0) {
            LIT(s32, sp_tmp1C) = res;
            return 3;
        }
        return 0;
    }
    LIT(VECTOR, sp_vec) = *(VECTOR *)&LIT(Entity *, g_CurrentEntity)->pos_x;
    SCRATCH->tmp1C = 0;
    hit = 0;
    p = e[-1];
    while (p < *e) {
        if (FIELD_AT(p, ScaBoundary, ->type, u8) == 4) {
            goto next;
        }
        if (FIELD_AT(p, ScaBoundary, ->type, u8) == 5 && (LIT(Entity *, g_CurrentEntity)->collision_flags & 0x10)) {
            p += 12;
            continue;
        }
        if ((LIT(Entity *, g_CurrentEntity)->collision_flags & 4) && !(SCA_BOUND(p)->floor & 0x100)) {
            p += 12;
            continue;
        }
        r = boundary_classify_l(LITP(s16, sp_svec), p, radius);
        if (r & 0x8000) {
            goto next;
        }
        b = (u32)({ SCA_BOUND(p)->floor & 0x300; }) >> 8;
        hit |= b;
        if (r == 3 && b != 0) {
            LIT(s32, sp_tmp1C) = 1;
        }
    next:
        p += 12;
    }
    flag = LITP(s32, sp_tmp1C);
    if (*flag != 0) {
        dx = LIT(Entity *, g_CurrentEntity)->pos_x - (s16)LIT(Entity *, g_CurrentEntity)->position_vx;
        dz = LIT(Entity *, g_CurrentEntity)->pos_z - (s16)LIT(Entity *, g_CurrentEntity)->position_vz;
        LIT(s32, sp_tmp18) = SquareRoot0(dx * dx + dz * dz);
        LIT(Entity *, g_CurrentEntity)->position_vx = LIT(Entity *, g_CurrentEntity)->pos_x;
        LIT(Entity *, g_CurrentEntity)->position_vy = LIT(Entity *, g_CurrentEntity)->pos_y;
        LIT(Entity *, g_CurrentEntity)->position_vz = LIT(Entity *, g_CurrentEntity)->pos_z;
        if (res != 0) {
            *flag = res;
            return 3;
        }
        return 0;
    }
    if (hit == 0) {
        dx = LIT(Entity *, g_CurrentEntity)->pos_x - (s16)LIT(Entity *, g_CurrentEntity)->position_vx;
        dz = LIT(Entity *, g_CurrentEntity)->pos_z - (s16)LIT(Entity *, g_CurrentEntity)->position_vz;
        LIT(s32, sp_tmp18) = SquareRoot0(dx * dx + dz * dz);
        LIT(Entity *, g_CurrentEntity)->position_vx = LIT(Entity *, g_CurrentEntity)->pos_x;
        LIT(Entity *, g_CurrentEntity)->position_vy = LIT(Entity *, g_CurrentEntity)->pos_y;
        LIT(Entity *, g_CurrentEntity)->position_vz = LIT(Entity *, g_CurrentEntity)->pos_z;
        if (res != 0) {
            *flag = res;
            return 3;
        }
        return 1;
    }
    LIT(s32, sp_tmp18) = 0;
    LIT(Entity *, g_CurrentEntity)->pos_x = (s16)LIT(Entity *, g_CurrentEntity)->position_vx;
    LIT(Entity *, g_CurrentEntity)->position_vy = LIT(Entity *, g_CurrentEntity)->pos_y;
    LIT(Entity *, g_CurrentEntity)->pos_z = (s16)LIT(Entity *, g_CurrentEntity)->position_vz;
    if (res != 0) {
        *flag = res;
        return 3;
    }
    return 2;
}

typedef struct {
    /* 0x00 */ u8 status_flags;
    /* 0x04 */ u8 *sca_info;
    /* 0x08 */ u8 pad8[0x2C];
    /* 0x34 */ s32 pos_x, pos_y, pos_z;
    /* 0x40 */ u8 pad40[0x2C];
    /* 0x6C */ s16 position_vx, position_vy, position_vz, rot_x;
    /* 0x74 */ s16 angle;
    /* 0x76 */ u8 pad76[8];
    /* 0x7E */ s16 prev_angle;
} CollisionEntity;
typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ u8 type;
    /* 0x0A */ u16 floor;
} BoundaryView;

u8 check_room_collision_two_point(SVECTOR *pt0, SVECTOR *pt1) {
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    MATRIX *RotMatrixY(long r, MATRIX *m);
    extern CollisionEntity *g_CurrentEntity_l __asm__("g_CurrentEntity");
    extern MATRIX g_IdentityMatrix_l __asm__("g_IdentityMatrix");
    extern s16 (*g_CollisionShapeFuncs_v[])(BoundaryView *, VECTOR *, SVECTOR *) __asm__("g_CollisionShapeFuncs");
    s32 boundary_get_cell_v(VECTOR *, SVECTOR *, s16, s16) __asm__("boundary_get_cell");
    s16 boundary_classify_l() __asm__("boundary_classify");
    BoundaryView **tbl[2];
    s16 hit[2] = {0, 0};
    VECTOR v[2];
    VECTOR save;
    SVECTOR sv[2];
    s32 unused[2];
    BoundaryView *p;
    BoundaryView **q;
    BoundaryView **e;
    s16 r;
    s32 c;
    u8 i;

    if (LIT(CollisionEntity *, g_CurrentEntity)->status_flags & 4) {
        return 0;
    }
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_l;
    RotMatrixY(LIT(CollisionEntity *, g_CurrentEntity)->angle, LITP(MATRIX, sp_matrix));
    i = 1;
    ApplyMatrix(LITP(MATRIX, sp_matrix), pt0, &v[0]);
    ApplyMatrix(LITP(MATRIX, sp_matrix), pt1, &v[1]);
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_l;
    RotMatrixY(LIT(CollisionEntity *, g_CurrentEntity)->prev_angle, LITP(MATRIX, sp_matrix));
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), pt0, &sv[0]);
    ApplyMatrixSV(LITP(MATRIX, sp_matrix), pt1, &sv[1]);
    sv[0].vx += LIT(CollisionEntity *, g_CurrentEntity)->position_vx;
    sv[0].vz += LIT(CollisionEntity *, g_CurrentEntity)->position_vz;
    sv[1].vx += LIT(CollisionEntity *, g_CurrentEntity)->position_vx;
    sv[1].vz += LIT(CollisionEntity *, g_CurrentEntity)->position_vz;
    save = v[1];
    do {
        v[i].vx += g_CurrentEntity_l->pos_x;
        v[i].vz += g_CurrentEntity_l->pos_z;
        LITP(SVECTOR, sp_svec)->vx = LITP(SVECTOR, sp_svec)->vy = LITP(SVECTOR, sp_svec)->vz = 0;
        LIT(s32, sp_tmp00) = boundary_get_cell_v(&v[i], LITP(SVECTOR, sp_svec),
                                             (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[0],
                                             (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[1]);
        tbl[i] = (BoundaryView **)(LIT(s32, sp_tmp00) * 4 + *(u8 **)(LIT(u8 *, g_RoomData) + 0x4C) + 8);
        LIT(VECTOR, sp_vec) = v[i];
        p = tbl[i][-1];
        while (p < tbl[i][0]) {
            c = p->type;
            if (c == 4 || (u32)(c - 4) < 2) {
                p++;
                continue;
            }
            r = boundary_classify_l(LITP(SVECTOR, sp_svec), p, FIELD_AT(g_CurrentEntity_l->sca_info, ScaInfo, ->radius, u16));
            LIT(s32, sp_tmp00) = r;
            if (r != -1) {
                g_CollisionShapeFuncs_v[r](p, &v[i], &sv[i]);
                hit[i] |= (p->floor & 0x300) >> 8;
            }
            p++;
        }
    } while (i-- != 0);
    if (hit[0] == 0) {
        g_CurrentEntity_l->position_vx = g_CurrentEntity_l->pos_x;
        g_CurrentEntity_l->position_vy = g_CurrentEntity_l->pos_y;
        g_CurrentEntity_l->position_vz = g_CurrentEntity_l->pos_z;
        g_CurrentEntity_l->prev_angle = g_CurrentEntity_l->angle;
        return hit[1];
    }
    save.vx += g_CurrentEntity_l->pos_x;
    save.vz += g_CurrentEntity_l->pos_z;
    do {
        e = tbl[1];
    } while (0);
    LIT(VECTOR, sp_vec) = v[1];
    p = e[-1];
    q = e;
    goto test;
loop:
    c = p->type;
    if (c != 4 && (u32)(c - 4) >= 2) {
        if (!(boundary_classify_l(LITP(SVECTOR, sp_svec), p, FIELD_AT(g_CurrentEntity_l->sca_info, ScaInfo, ->radius, u16)) & 0x8000)) {
            hit[1] |= (p->floor & 0x300) >> 8;
        }
    }
    p++;
    q = tbl[1];
test:
    if (p < *q) {
        goto loop;
    }
    if (hit[1] != 0) {
        g_CurrentEntity_l->pos_x = g_CurrentEntity_l->position_vx;
        g_CurrentEntity_l->position_vy = g_CurrentEntity_l->pos_y;
        g_CurrentEntity_l->pos_z = g_CurrentEntity_l->position_vz;
        g_CurrentEntity_l->angle = g_CurrentEntity_l->prev_angle;
        return 0x80;
    }
    g_CurrentEntity_l->position_vx = g_CurrentEntity_l->pos_x;
    g_CurrentEntity_l->position_vy = g_CurrentEntity_l->pos_y;
    g_CurrentEntity_l->position_vz = g_CurrentEntity_l->pos_z;
    g_CurrentEntity_l->prev_angle = g_CurrentEntity_l->angle;
    return 1;
}

u16 room_collision_check_point(VECTOR *pos, s16 *ofs) {
    s16 *t;
    u32 *q;
    u32 p;
    u32 *e;
    s16 r;
    s16 res;
    s32 bits;
    s32 acc;

    t = *(s16 **)(LIT(u8 *, g_RoomData) + 0x4C);
    r = boundary_get_cell(pos, ofs, t[0], t[1]);
    acc = 0;
    q = &((u32 *)*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[r + 1];
    LIT(VECTOR, sp_vec) = *pos;
    p = q[0];
    e = &q[1];
    for (; p < *e; p += 12) {
        res = boundary_classify_flags(ofs, (u16 *)p, 0);
        if (res != -1) {
            bits = res & 0x300;
            acc |= bits;
            if (bits == 0x300) {
                return 1;
            }
        }
    }
    return acc;
}

s32 room_collision_check_point_radius(VECTOR *pos, void *ofs, u16 radius) {
    s16 boundary_get_cell_l(void *, void *, s32, s32) __asm__("boundary_get_cell");
    s16 boundary_classify_flags_l(void *, u32, u16) __asm__("boundary_classify_flags");
    u32 *tbl;
    u32 *end;
    u32 p;
    s16 r;
    u16 flags;

    tbl = (u32 *)(*(u8 **)(LIT(u8 *, g_RoomData) + 0x4C) + 4)
        + boundary_get_cell_l(pos, ofs, (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[0], (*(s16 **)(LIT(u8 *, g_RoomData) + 0x4C))[1]);
    flags = 0;
    LIT(VECTOR, sp_vec) = *pos;
    end = tbl + 1;
    for (p = *tbl; p < *end; p += 0xC) {
        r = boundary_classify_flags_l(ofs, p, radius);
        if (r != -1) {
            flags |= r & 0x300;
            if ((r & 0x300) == 0x300) {
                return 1;
            }
        }
    }
    return flags;
}

s32 room_check_sight_blocked(VECTOR *dir, s32 group) {
    void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
    void VectorNormal(VECTOR *v0, VECTOR *v1);
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    VECTOR v1;
    VECTOR v2;
    u16 **q;
    u16 *p;
    u16 a, b, c, d;
    s32 ax, az;
    s32 px, pz;

    px = ENT(g_CurrentEntity)->pos_x / 18;
    pz = ENT(g_CurrentEntity)->pos_z / 18;
    ax = dir->vx / 18;
    q = &((u16 **)(((u8 *) ROOM_L->collision) + 8))[group & 0x1F];
    az = dir->vz / 18;
    p = q[-1];
    g_LastSightBoundaryGroup = p;
    for (; p < *q; p += 6) {
        a = p[0] / 18;
        b = p[1] / 18;
        c = p[2] / 18;
        d = p[3] / 18;
        if ((p[5] &= 0x300) != 0x300 && ENT(g_CurrentEntity)->id != 13 && ENT(g_CurrentEntity)->id != 18) {
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
            LIT(s32, sp_vec_vx) = x0 - x1;
            LIT(s32, sp_vec_vz) = z0 - z1;
            v1.vx = px + ax - x1;
            v1.vz = pz + az - z1;
            v2.vx = px - x1;
            v2.vz = pz - z1;
            v2.vy = 0;
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
            LIT(s32, sp_vec_vx) = x0 - x1;
            LIT(s32, sp_vec_vz) = z0 - z1;
            v1.vx = px + ax - x1;
            v1.vz = pz + az - z1;
            v2.vx = px - x1;
            v2.vz = pz - z1;
            v2.vy = 0;
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

void collision_push_rect(s16 *box, VECTOR *pos, SVECTOR *old) {
    extern u8 *g_CurrentEntity_u8p __asm__("g_CurrentEntity");
    s16 dx;
    s16 dz;
    s16 t;
    u8 side;

    dx = box[0] + ((s16) SCA(((u8 *) CURRENT_ENTITY_LIT->sca_info))->radius) - pos->vx + 0x12;
    LIT(s32, sp_tmp00) = dx;
    t = box[2] - ((s16) SCA(((u8 *) CURRENT_ENTITY_LIT->sca_info))->radius) - pos->vx - 0x12;
    LIT(s32, sp_tmp04) = t;
    if (-t < dx) {
        dx = t;
        LIT(s32, sp_tmp08) = t;
    }
    dz = box[1] + ((s16) SCA(((u8 *) CURRENT_ENTITY_LIT->sca_info))->radius) - pos->vz + 0x12;
    LIT(s32, sp_tmp0C) = dz;
    t = box[3] - ((s16) SCA(((u8 *) CURRENT_ENTITY_LIT->sca_info))->radius) - pos->vz - 0x12;
    LIT(s32, sp_tmp10) = t;
    if (-t < dz) {
        dz = t;
        LIT(s32, sp_tmp14) = t;
    }
    side = (((dx ^ (pos->vx - old->vx)) & 0x8000) >> 15) | (((dz ^ (pos->vz - old->vz)) & 0x8000) >> 14);
    if (pos->vx == old->vx && pos->vz == old->vz) {
        LIT(s32, sp_tmp1C) = 0;
    }
    switch (side) {
    case 0:
        pos->vx = old->vx;
        pos->vz = old->vz;
        break;
    case 1:
        if ((u16)(dx + 400) < 801) {
            ENT(g_CurrentEntity_u8p)->pos_x += dx;
            *g_CurrentEntity_u8p &= 0xEF;
            break;
        }
        goto both;
    case 2:
        if ((u16)(dz + 400) < 801) {
            ENT(g_CurrentEntity_u8p)->pos_z += dz;
            *g_CurrentEntity_u8p |= 0x10;
            break;
        }
        goto both;
    case 3:
    both:
        if ((dx < 0 ? -dx : dx) < (dz < 0 ? -dz : dz)) {
            ENT(g_CurrentEntity_u8p)->pos_x += dx;
            *g_CurrentEntity_u8p &= 0xEF;
        } else {
            ENT(g_CurrentEntity_u8p)->pos_z += dz;
            *g_CurrentEntity_u8p |= 0x10;
        }
        break;
    }
}

s32 collision_push_circle(void *box, void *pos) {
    long SquareRoot0(long a);
    s32 new3c;
    s32 amx;
    void *q;
    s32 old3c;
    s32 amz;
    s32 half;
    s32 dx;
    s32 dz;
    s32 root;
    s32 new34;
    s32 mx;
    s32 dist;
    s32 mz;
    u16 w2;
    u16 rad;
    void *obj;

    w2 = SCA_BOUND(box)->x2;
    q = LIT(void *, g_CurrentEntity);
    q = (void *) ENT(q)->sca_info;
    rad = SCA(q)->radius;
    half = (s32) ((SCA_BOUND(box)->x1 - w2) + (rad * 2)) / 2;
    dz = VEC(pos)->vz - ((SCA_BOUND(box)->z2 - rad) + half);
    dx = VEC(pos)->vx - ((w2 - rad) + half);
    root = SquareRoot0((dz * dz) + (dx * dx));
    dist = (root < 0) ? -root : root;
    half -= dist;
    if (half > 0) {
        do {
            mx = (s32) (half * dx) / dist;
            mz = (s32) (half * dz) / dist;
        } while (0);
        do {
            obj = LIT(void *, g_CurrentEntity);
            new34 = mx + ENT(obj)->pos_x;
            old3c = ENT(obj)->pos_z;
        } while (0);
        amx = (mx < 0) ? -mx : mx;
        do {
            ENT(obj)->pos_x = new34;
        } while (0);
        new3c = mz + old3c;
        amz = (mz < 0) ? -mz : mz;
        ENT(obj)->pos_z = new3c;
        if (amx < amz) {
            ENT(obj)->status_flags |= 0x10;
        } else {
            ENT(obj)->status_flags &= 0xEF;
        }
        return 1;
    }
    return 0;
}

void collision_flag_set(void) {
    g_CurrentEntity->collision_flags = (u8) (g_CurrentEntity->collision_flags | 8);
}
