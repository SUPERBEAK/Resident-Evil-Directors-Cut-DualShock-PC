/* enemy_path.c - Enemy path finding over the room's walk zones (zone_*), angle helpers and footstep zones.
   (The inventory / item box code that followed is in inventory.c.) */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void memclr(u32 *p, s32 n);
void load_file(s32 a, s32 b);


s8 zone_path_find(s16 x, s16 z, s16 *out_x, s16 *out_z) {
    s32 zone_walk_cw_l() __asm__("zone_walk_cw");
    s32 zone_walk_ccw_l() __asm__("zone_walk_ccw");
    s8 walk_zone_find_l() __asm__("walk_zone_find");
    s32 walk_zone_shared_edge_l() __asm__("walk_zone_shared_edge");
    s16 tx;
    s16 tz;
    s32 from;
    s32 to;
    s32 next_zone;
    s8 step;
    s32 unused;
    s32 res;
    s8 diff;
    s8 cur_zone;
    s8 dst_zone;
    s8 dir;
    u8 n_zones;
    void *blk;

    tx = x;
    tz = z;
    cur_zone = walk_zone_find_l(ENTITY_FIELD_AS(g_CurrentEntity, pos_x, s16), ENTITY_FIELD_AS(g_CurrentEntity, pos_z, s16));
    if (z != 0) {
        dst_zone = walk_zone_find_l(x, z);
    } else {
        dst_zone = (s8) tx;
        blk = ((dst_zone & 0xFF) * 0xC) + ((u8 *) ROOM->blocks);
        tx = (s16) ((u32) (BLK((u8 *) blk + 2)->x0 + BLK((u8 *) blk + 2)->x1) >> 1);
        tz = (s16) ((u32) (BLK((u8 *) blk + 2)->z0 + BLK((u8 *) blk + 2)->z1) >> 1);
    }
    from = cur_zone & 0xFF;
    to = dst_zone & 0xFF;
    if (from == to) {
        *out_x = tx;
        *out_z = tz;
        return (s8) (cur_zone | 0x10);
    }
    diff = dst_zone - cur_zone;
    n_zones = *((u8 *) ROOM->blocks);
    dir = diff;
    if ((s8) diff < 0) {
        dir = n_zones + diff;
    }
    step = -1;
    if ((s8) dir > (s8) n_zones >> 1) {
        step = 1;
    }
    dir = step;
    if (from == 0) {
        dir = ~dir + 1;
    }
    LIT(s8, sp_path_0) = cur_zone;
    LIT(s8, sp_path_best_0) = cur_zone;
    LIT(u16, sp_path_points_x) = (u16) ENTITY_AS(g_CurrentEntity, pos_x, s16);
    LIT(u16, sp_path_points_z) = (u16) ENTITY_AS(g_CurrentEntity, pos_z, s16);
    if ((dir << 0x18) > 0) {
        LIT(u8, sp_path_1) = 0xFF;
        res = zone_walk_cw_l(from, to, tx, tz) << 0x18;
    } else {
        LIT(u8, sp_path_1) = n_zones;
        res = zone_walk_ccw_l(from, to, tx, tz) << 0x18;
    }
    next_zone = res >> 0x18;
    if ((next_zone + 1) == 0) {
        return -1;
    }
    walk_zone_shared_edge_l(cur_zone & 0xFF, next_zone & 0xFFFF);
    *out_x = (s16) LIT(u16, sp_tmp00);
    *out_z = (s16) LIT(u16, sp_tmp04);
    return next_zone;
}

s8 zone_walk_cw(s32 src, u8 dst, s16 x, s16 z) {
    long SquareRoot0(long a);
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    s32 walk_zone_shared_edge(u16, u16);
    u8 *path;
    u8 *best;
    s16 *pts;
    s32 *pz;
    u32 sum;
    u32 min;
    s8 i;
    s8 j;
    u16 mask;
    s32 ret;

    path = LITP(u8, sp_path);
    best = LITP(u8, sp_path_best);
    pts = LITP(s16, sp_path_points);
    sum = 0;
    min = -1;
    i = 0;
    pz = LITP(s32, sp_tmp04);
    do {
        mask = BLK(((u8 *) ROOM_L->blocks) + path[i] * 12 + 2)->abut;
        if ((mask >> dst) & 1) {
            s16 *q;
            path[i + 1] = dst;
            walk_zone_shared_edge(path[i], dst);
            q = (s16 *)(i * 4 + (s32)pts);
            sum += SquareRoot0((q[0] - LIT(s32, sp_tmp00)) * (q[0] - LIT(s32, sp_tmp00)) + (q[1] - *pz) * (q[1] - *pz));
            sum += SquareRoot0((LIT(s32, sp_tmp00) - x) * (LIT(s32, sp_tmp00) - x) + (*pz - z) * (*pz - z));
            if (sum < min) {
                min = sum;
                j = i + 1;
                do {
                    best[j] = path[j];
                } while (--j != 0);
            }
            if (i != 0) {
                s16 *q2;
                s16 *r2;
                sum -= SquareRoot0((LIT(s32, sp_tmp00) - x) * (LIT(s32, sp_tmp00) - x) + (*pz - z) * (*pz - z));
                q2 = (s16 *)(i * 4 + (s32)pts);
                sum -= SquareRoot0((q2[0] - LIT(s32, sp_tmp00)) * (q2[0] - LIT(s32, sp_tmp00)) + (q2[1] - *pz) * (q2[1] - *pz));
                r2 = (s16 *)((i - 1) * 4 + (s32)pts);
                sum -= SquareRoot0((r2[0] - q2[0]) * (r2[0] - q2[0]) + (r2[1] - q2[1]) * (r2[1] - q2[1]));
            }
        } else if (mask & (-1 << (path[i + 1] + 1))) {
            s16 *q3;
            i++;
            do {
                path[i]++;
            } while (!((mask >> path[i]) & 1));
            for (j = i - 2; j >= 0; j--) {
                if (path[i] == path[j]) {
                    goto next;
                }
            }
            walk_zone_shared_edge(path[i], path[i - 1]);
            q3 = (s16 *)((i - 1) * 4 + (s32)pts);
            sum += SquareRoot0((q3[0] - LIT(s32, sp_tmp00)) * (q3[0] - LIT(s32, sp_tmp00)) + (q3[1] - *pz) * (q3[1] - *pz));
            if (sum >= min) {
                sum -= SquareRoot0((q3[0] - LIT(s32, sp_tmp00)) * (q3[0] - LIT(s32, sp_tmp00)) + (q3[1] - *pz) * (q3[1] - *pz));
            } else {
                pts[i * 2] = LIT(u16, sp_tmp00);
                pts[i * 2 + 1] = *(u16 *)pz;
                path[i + 1] = 0xFF;
                continue;
            }
        } else if (i != 0) {
            s16 *q4;
            s16 *r4;
            r4 = (s16 *)((i - 1) * 4 + (s32)pts);
            q4 = (s16 *)(i * 4 + (s32)pts);
            sum -= SquareRoot0((r4[0] - q4[0]) * (r4[0] - q4[0]) + (r4[1] - q4[1]) * (r4[1] - q4[1]));
        }
next:
        i--;
    } while (i >= 0);
    if (min + 1 != 0) {
        return best[1];
    }
    return -1;
}

typedef struct {
    s16 x;
    s16 z;
} ZonePoint;

s8 zone_walk_ccw(s32 src, u8 dst, s16 x, s16 z) {
    long SquareRoot0(long a);
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    s32 walk_zone_shared_edge(u16, u16);
    long SquareRoot0(long a);
    u8 *path = LITP(u8, sp_path);
    u8 *best = LITP(u8, sp_path_best);
    ZonePoint *pos = LITP(ZonePoint, sp_path_points);
    u32 dist = 0;
    u32 bestDist = -1;
    s8 depth = 0;
    u8 first = *((u8 *) ROOM_L->blocks);
    s8 d;
    s8 i;
    u16 mask;
    u8 *e;
    u8 *n;
    u8 *c;

    do {
        d = depth;
        e = &path[d];
        mask = *(u16 *)(((u8 *) ROOM_L->blocks) + *e * 12 + 0xC);
        if ((mask >> dst) & 1) {
            e[1] = dst;
            walk_zone_shared_edge(e[0], dst);
            {
                s32 dx = pos[d].x - LIT(s32, sp_tmp00);
                s32 dz = pos[d].z - LIT(s32, sp_tmp04);
                dist += SquareRoot0(dx * dx + dz * dz);
            }
            {
                s32 dx = LIT(s32, sp_tmp00) - x;
                s32 dz = LIT(s32, sp_tmp04) - z;
                dist += SquareRoot0(dx * dx + dz * dz);
            }
            if (dist < bestDist) {
                bestDist = dist;
                i = depth + 1;
                do {
                    best[i] = path[i];
                } while (--i != 0);
            }
            if (depth != 0) {
                {
                    s32 dx = LIT(s32, sp_tmp00) - x;
                    s32 dz = LIT(s32, sp_tmp04) - z;
                    dist -= SquareRoot0(dx * dx + dz * dz);
                }
                {
                    s32 dx = pos[depth].x - LIT(s32, sp_tmp00);
                    s32 dz = pos[depth].z - LIT(s32, sp_tmp04);
                    dist -= SquareRoot0(dx * dx + dz * dz);
                }
                {
                    s32 dx = pos[({ depth - 1; })].x - pos[depth].x;
                    s32 dz = pos[({ depth - 1; })].z - pos[depth].z;
                    dist -= SquareRoot0(dx * dx + dz * dz);
                }
            }
        } else if (mask & ((1 << e[1]) - 1)) {
            depth++;
            c = &path[depth];
            do {
                (*c)--;
            } while (!((mask >> *c) & 1));
            for (i = depth - 2; i >= 0; i--) {
                if (path[depth] == path[i]) {
                    goto next;
                }
            }
            n = &path[depth];
            walk_zone_shared_edge(n[0], n[-1]);
            {
                s32 dx = pos[({ depth - 1; })].x - LIT(s32, sp_tmp00);
                s32 dz = pos[({ depth - 1; })].z - LIT(s32, sp_tmp04);
                dist += SquareRoot0(dx * dx + dz * dz);
            }
            if (dist >= bestDist) {
                s32 dx = pos[({ depth - 1; })].x - LIT(s32, sp_tmp00);
                s32 dz = pos[({ depth - 1; })].z - LIT(s32, sp_tmp04);
                dist -= SquareRoot0(dx * dx + dz * dz);
            } else {
                pos[depth].x = LIT(s32, sp_tmp00);
                pos[depth].z = LIT(s32, sp_tmp04);
                n[1] = first;
                continue;
            }
        } else if (d != 0) {
            s32 dx = pos[({ d - 1; })].x - pos[d].x;
            s32 dz = pos[({ d - 1; })].z - pos[d].z;
            dist -= SquareRoot0(dx * dx + dz * dz);
        }
next:
        depth--;
    } while (depth >= 0);
    if (bestDist + 1 != 0) {
        return best[1];
    }
    return -1;
}

s16 zone_edge_heading(s16 x, s16 z, s8 dst, s8 src) {
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    s8 walk_zone_find_l() __asm__("walk_zone_find");
    s16 zone_edge_clamp_l(s16, u16, s16, s16) __asm__("zone_edge_clamp");
    s16 calc_angle_xz_l(s16, s16, s16, s16) __asm__("calc_angle_xz");
    u16 *p;
    u16 *q;
    u16 *pp;
    u16 *qq;
    s32 tbl;
    s8 a;
    s16 r0;
    s16 r1;
    s16 lo;
    s16 hi;

    if (src < 0) {
        a = walk_zone_find_l(x, z);
    } else {
        a = src & 0xF;
    }
    tbl = ROOM_L->blocks;
    p = (u16 *)(a * 12 + tbl);
    pp = p + 1;
    q = (u16 *)(dst * 12 + tbl);
    qq = q + 1;
    if (p[1] == q[3] || p[3] == q[1]) {
        r0 = (p[1] == q[3]) ? p[1] : q[1];
        lo = (pp[1] > qq[1]) ? pp[1] : qq[1];
        hi = (pp[3] < qq[3]) ? pp[3] : qq[3];
        r1 = zone_edge_clamp_l(z, r0, lo, hi);
    } else {
        r1 = (p[2] == q[4]) ? p[2] : q[2];
        lo = (pp[0] > qq[0]) ? pp[0] : qq[0];
        hi = (pp[2] < qq[2]) ? pp[2] : qq[2];
        r0 = zone_edge_clamp_l(x, r1 | 0x8000, lo, hi);
    }
    LIT(s32, sp_tmp00) = r0;
    LIT(s32, sp_tmp04) = r1;
    return calc_angle_xz_l(x, z, r0, r1);
}

s32 zone_edge_clamp(s16 v, s16 edge, s16 start, s16 end) {
    u16 point_collides_l(s32, s32) __asm__("point_collides");
    s16 lo;
    s16 hi;
    s16 t;
    s32 d;

    lo = start;
    hi = end;
    t = edge & 0x7FFF;
    d = ((end - start) >> 3) + 0x280;
    if (edge & 0x8000) {
        if (point_collides_l(start, t)) {
            lo = start + d;
        }
        if (point_collides_l(end, t)) {
            hi = end - d;
        }
    } else {
        if (point_collides_l(t, start)) {
            lo = start + d;
        }
        if (point_collides_l(t, end)) {
            hi = end - d;
        }
    }
    if (lo < hi) {
        if (v < lo) {
            return lo;
        }
        if (hi < v) {
            return hi;
        }
        return v;
    }
    return (lo + hi) >> 1;
}

s32 walk_zone_find(s32 x, s32 z) {
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    typedef struct {
        u16 x0;
        u16 z0;
        u16 x1;
        u16 z1;
        u16 a;
        u16 b;
    } WalkZone;
    u8 *tbl;
    WalkZone *r;
    u8 i;
    u32 d;
    u32 w;

    tbl = ((u8 *) ROOM_L->blocks);
    i = tbl[0];
    r = (WalkZone *)(i * 12 + (s32)tbl + 2);
    do {
        r--;
        i--;
        w = r->x1 - r->x0;
        d = x - r->x0;
        if ((d & 0xFFFF) < (w & 0xFFFF)) {
            w = r->z1 - r->z0;
            d = z - r->z0;
            if ((d & 0xFFFF) < (w & 0xFFFF)) {
                LIT(s32, sp_tmp00) = r->a;
                LIT(s32, sp_tmp04) = r->b;
                return i;
            }
        }
    } while (i != 0);
    return 0xFF;
}

typedef struct {
    u16 id;
    u16 x0;
    u16 z0;
    u16 x1;
    u16 z1;
    u16 pad;
} Blk12_2D008;

s32 walk_zone_shared_edge(u16 zone_a, u16 zone_b) {
    extern u8 *g_RoomData_u8p __asm__("g_RoomData");
    Blk12_2D008 *base;
    Blk12_2D008 *a;
    Blk12_2D008 *b;
    u16 *pa;
    u16 *pb;
    u8 ret;
    u16 v;
    u16 mx;
    u16 mn;

    ret = 0;
    base = ((Blk12_2D008 *) ROOM_P->blocks);
    a = (Blk12_2D008 *)(zone_a * 12 + (s32)base);
    pa = &a->x0;
    b = (Blk12_2D008 *)(zone_b * 12 + (s32)base);
    pb = &b->x0;
    if ((v = a->x0) == b->x1 || (v = b->x0) == a->x1) {
        SCRATCH->tmp00 = v;
    } else {
        ret = 1;
        if ((v = a->z0) == b->z1) {
            SCRATCH->tmp04 = v;
        } else if ((v = b->z0) == a->z1) {
            SCRATCH->tmp04 = v;
        }
    }
    if (ret) {
        if (pa[0] > pb[0]) {
            mx = pa[0];
        } else {
            mx = pb[0];
        }
        if (pa[2] < pb[2]) {
            mn = pa[2];
        } else {
            mn = pb[2];
        }
        SCRATCH->tmp00 = (mx + mn) >> 1;
    } else {
        if (pa[1] > pb[1]) {
            mx = pa[1];
        } else {
            mx = pb[1];
        }
        if (pa[3] < pb[3]) {
            mn = pa[3];
        } else {
            mn = pb[3];
        }
        SCRATCH->tmp04 = (mx + mn) >> 1;
    }
    return ret;
}

s32 point_collides(s16 x, s16 z) {
    LIT(s32, sp_vec_vx) = (s32) x;
    LIT(s32, sp_vec_vy) = 0;
    LIT(s32, sp_vec_vz) = (s32) z;
    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    return room_collision_check_point(ADDR_sp_vec, ADDR_sp_svec) & 0xFFFF;
}

s32 calc_angle_xz(s32 x0, s32 z0, s32 x1, s32 z1) {
    long catan(long a);
    s16 dx = x1 - x0;
    s16 dz = z1 - z0;

    if (dx != 0) {
        return ((dx < 0 ? 0x800 : 0x1000) - catan((dz << 12) / dx)) & 0xFFF;
    }
    return dz > 0 ? 0xC00 : 0x400;
}

s16 get_angle_towards_target(s16 x, s16 z) {
    return calc_angle_xz(ENTITY_FIELD_AS(g_CurrentEntity, pos_x, s16), ENTITY_FIELD_AS(g_CurrentEntity, pos_z, s16), x, z);
}

typedef struct {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u8 id;
    u8 pad;
} FootstepZone;

u8 lookup_footstep_zone(s16 x, s16 y) {
    FootstepZone *r;

    r = (FootstepZone *)((u8 *)((void *) ROOM->floor_sounds) + 2);
    while (1) {
        if ((u32)(x - r->x) < r->w && (u32)(y - r->y) < r->h) {
            return r->id;
        }
        r++;
    }
}
