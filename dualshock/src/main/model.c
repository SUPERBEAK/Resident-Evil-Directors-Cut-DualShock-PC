/* model.c - Entity models and skeletal animation: joint setup and animation steps, weapon parts, mirror
   reflections, joint world matrices and rendering. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 is_point_in_quad(s32 *pos, u16 *quad);
void load_entity_emd(u8 *ent, u8 id);
void update_entity_lighting(s32 *pos);


void load_entity_model(void) {
    s32 load_entity_emd_a1(s32, s32) __asm__("load_entity_emd");
    void init_anim_structure();
    s32 setup_joint_structures_a1(s32) __asm__("setup_joint_structures");
    s32 reset_joint_transforms_a2() __asm__("reset_joint_transforms");
    void entity_set_joints();
    s32 saved_load_ptr;

    saved_load_ptr = LIT(s32, g_LoadPointer);
    g_PlayerEntity.model_load_buffer = ADDR_g_PlayerEmdBuf;
    LIT(s32, g_LoadPointer) = ADDR_g_PlayerEmdBuf;
    load_entity_emd_a1(LIT(s32, g_CurrentEntity), g_PlayerEntity.id & 3);
    entity_set_joints(LIT(s32, g_CurrentEntity), 0x7C);
    LIT(s32, g_LoadPointer) = saved_load_ptr;
    init_anim_structure(g_PlayerModelLoadBuffer);
    g_PlayerEntity.joint_count += 1;
    setup_joint_structures_a1(ADDR_g_PlayerObjPacketBuf);
    g_PlayerJointCount -= 1;
    reset_joint_transforms_a2();
}

void init_anim_structure(s32 tmd) {
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    s32 GsMapModelingData(s32);
    void *e;

    GsMapModelingData(tmd + 4);
    GsLinkObject5(tmd + 0xC, LIT(void *, g_CurrentEntity) + 0xC, 0);
    e = LIT(void *, g_CurrentEntity);
    MODEL_OBJ(e)->coord2 = (GsCOORDINATE2 *)((s32) (&ENT(e)->sca_matrix_head));
    MODEL_OBJ(e)->attribute = 0x100;
    ENT(e)->blend_counter = 0;
}

s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    long LoadAverageShort12(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2);
    u8 *obj;
    u16 *hdr;
    u8 *frm;
    s16 *data;
    u8 *part;
    s32 half;
    u8 t;
    u8 be;
    u8 n;

    obj = (u8 *)g_CurrentEntity;
    if (((u8) ENT(obj)->timing_control) >= 2) {
        ENT(obj)->timing_control--;
        return 0;
    }
    data = (s16 *)((s32 *)skel + skel[1] / 4);
    half = skel[3] / 2;
    SCRATCH->tmp00 = half;
    hdr = (u16 *)(anim_tbl + ((u8) ENT(obj)->animation_id) * 4);
    frm = anim_tbl + (hdr[1] & 0xFFFC);
    frm += ENT(obj)->animation_frame_id * 4;
    if (reverse) {
        frm += (hdr[0] - ENT(obj)->animation_frame_id * 2) * 4 - 4;
    }
    part = ((u8 *) ENT(obj)->joints);
    data += *(u16 *)frm * half;
    t = ((u8) ENT(obj)->blend_counter);
    JOINT(part)->transform.t[0] = *data;
    n = ENT(obj)->joint_count;
    if (t != 0) {
        data++;
        JOINT(part)->transform.t[1] = (blend_step * JOINT(part)->transform.t[1] * t) / 4096 +
                                       (*data * blend_step * (0x1000 / blend_step - t)) / 4096;
        data++;
        JOINT(part)->transform.t[2] = *data;
        data += 4;
        while (n-- != 0) {
            if (*part & 0x10) {
                data += 3;
            } else {
                SCRATCH->sv_x = *data++;
                SCRATCH->sv_y = *data++;
                SCRATCH->sv_z = *data;
                if (t == 0x1000 / blend_step - 1) {
                    SCRATCH->tmp1C = 2;
                    do {
                        s32 i = LIT(s32, sp_tmp1C);
                        s32 s = LITP(u16, sp_svec)[i];
                        u16 *r = (u16 *)((u8 *)(i * 2 + (s32)part) + 4);
                        u16 a = *r;
                        s32 d = s - a + 0x800;
                        s32 v;

                        if ((u16)d > 0x1000) {
                            v = (s16)a;
                            *r = (d & 0x8000) ? v - 0x1000 : v + 0x1000;
                        }
                    } while (--LIT(s32, sp_tmp1C) != -1);
                }
                LoadAverageShort12((SVECTOR *)(&JOINT(part)->rotation), LITP(SVECTOR, sp_svec), blend_step * t,
                                   blend_step * (0x1000 / blend_step - t), (SVECTOR *)(&JOINT(part)->rotation));
                data++;
                RotMatrix((SVECTOR *)(&JOINT(part)->rotation), (MATRIX *)(&JOINT(part)->transform));
            }
            part += 0x7C;
        }
        ENT(g_CurrentEntity)->blend_counter--;
    } else {
        data++;
        JOINT(part)->transform.t[1] = *data;
        data++;
        JOINT(part)->transform.t[2] = *data;
        data += 4;
        while (n-- != 0) {
            if (*part & 0x10) {
                data += 3;
                part += 0x7C;
            } else {
                JOINT(part)->rotation.vx = *data++;
                JOINT(part)->rotation.vy = *data++;
                JOINT(part)->rotation.vz = *data++;
                RotMatrix((SVECTOR *)(&JOINT(part)->rotation), (MATRIX *)(&JOINT(part)->transform));
                part += 0x7C;
            }
        }
    }
    LIT(u8 *, g_CurrentEntity)[0xBF] = frm[2];
    be = LIT(u8 *, g_CurrentEntity)[0xBE] + 1;
    LIT(u8 *, g_CurrentEntity)[0xBE] = be;
    if (hdr[0] - 1 < be) {
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        return 1;
    }
    return 0;
}

u32 *setup_joint_structures(u32 *packet) {
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    s32 unused[6];
    u8 *rec;
    u8 *obj;
    u32 handle;
    u8 n;
    u8 i;

    rec = ((u8 *) g_CurrentEntity->joints);
    n = g_CurrentEntity->joint_count;
    handle = ((u32) g_CurrentEntity->model_load_buffer);
    for (i = 0; i < n; i++) {
        obj = (u8 *)&JOINT(rec)->anim_field;
        GsLinkObject5(handle, (GsDOBJ5 *)obj, i);
        JOINT(rec)->index = i;
        JOINT(rec)->flags = 3;
        FIELD_AT(obj, GsDOBJ5, ->coord2, u8 *) = (u8 *)&JOINT(rec)->scale_flag;
        FIELD_AT(obj - 0xC, Joint, ->scale_flag, s32) = 1;
        FIELD_AT(obj, GsDOBJ5, ->id, s32) = 0;
        FIELD_AT(obj, GsDOBJ5, ->packet, s32) = 0;
        JOINT(rec)->fx_frame = 0;
        *(u32 *)obj = 0x100;
        packet = (u32 *)GsPresetObject((GsDOBJ5 *)obj, (u_long *)packet);
        rec += 0x7C;
    }
    return packet;
}

typedef struct {
    u8 pad0[4];
    s16 rot[3];
    u8 padA[0x1A];
    MATRIX m;
    u8 pad44[0x32];
    s16 v[3];
} JointPart;

void reset_joint_transforms(void) {
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    u8 *e;
    s16 *src;
    JointPart *p;
    u8 n;
    u8 i;

    e = LIT(u8 *, g_CurrentEntity);
    src = ((s16 *) ENT(e)->anim_header);
    n = ENT(e)->joint_count;
    ENT(e)->look_at_joint_idx = 1;
    p = *(JointPart **)(LIT(u8 *, g_CurrentEntity) + 0x98);
    src += 4;
    for (i = 0; i < n; i++) {
        p->m = g_IdentityMatrix_MATRIX;
        p->m.t[0] = *src++;
        p->m.t[1] = *src++;
        p->m.t[2] = *src++;
        p->rot[0] = 0;
        p->rot[1] = 0;
        p->rot[2] = 0;
        p->v[0] = 0;
        p->v[1] = 0;
        p->v[2] = 0;
        p++;
    }
}

void entity_part_reset_offset(s32 part) {
    u8 *w;
    u8 *a1;
    u8 *a2;
    s32 i;

    w = (u8 *)g_CurrentEntity;
    i = part & 0xFF;
    a1 = ((u8 *) ENT(w)->anim_header);
    a2 = ((u8 *) ENT(w)->joints);
    a2 += i * 0x7C;
    a1 += i * 6 + 8;
    JOINT(a2)->flags = (JOINT(a2)->flags & 0xEB) | 2;
    JOINT(a2)->transform.t[0] = *(s16 *)a1;
    a1 += 2;
    JOINT(a2)->transform.t[1] = *(s16 *)a1;
    JOINT(a2)->transform.t[2] = ((s16 *)a1)[1];
}

void entity_extract_anim_vertex(u8 *ent, u8 *src, u8 *anim_tbl, u8 reverse) {
    u16 *s;
    u16 h;
    u8 *e;
    u8 *p;
    u8 t;

    s = (u16 *)((s32 *)src + *(s16 *)((u8 *)src + 2) / 4);
    h = *(s16 *)((u8 *)src + 6) / 2;
    src = anim_tbl;
    e = LIT(u8 *, g_CurrentEntity);
    p = src + ent[0xBD] * 4;
    SCRATCH->tmp1C = ENT(e)->animation_frame_id;
    if (((u8) ENT(e)->timing_control) >= 2) {
        t = ENT(e)->animation_frame_id - 1;
        ENT(e)->animation_frame_id = t;
        if (t == 0xFF) {
            LIT(u8 *, g_CurrentEntity)[0xBE] = p[0] - 1;
        }
    }
    src += *(u16 *)((u8 *)p + 2) & 0xFFFC;
    if (reverse) {
        src += (*(u16 *)p - ent[0xBE]) * 4 - 4;
    } else {
        src += ent[0xBE] * 4;
    }
    s += h * *(u16 *)src;
    s += 3;
    SCRATCH->sv_x = *s++;
    SCRATCH->sv_y = *s;
    SCRATCH->sv_z = s[1];
    ENT(g_CurrentEntity)->animation_frame_id = SCRATCH_AS(tmp1C, u8);
}

void entity_set_joints(void *ent, s32 joint_size) {
    s32 buf;

    ENT(ent)->joint_count = EMR(ENT(ent)->anim_header)->joint_count;
    buf = LIT(s32, g_LoadPointer);
    ENT(ent)->joints = buf;
    LIT(s32, g_LoadPointer) = (((u32) (joint_size & 0xFFFF) >> 2) * ENT(ent)->joint_count * 4) + buf;
}

/* the camera view mirrored in the plane at v (x = v when sel, else z = v) */
void mirror_reflect_view(GsRVIEW2 *src, GsRVIEW2 *dst, u8 sel, u16 v) {
    *dst = *src;
    if (sel) {
        dst->vpx = (v - src->vpx) * 2 + src->vpx;
        dst->vrx = (v - src->vrx) * 2 + src->vrx;
    } else {
        dst->vpz = (v - src->vpz) * 2 + src->vpz;
        dst->vrz = (v - src->vrz) * 2 + src->vrz;
    }
}

s32 mirror_point_visible(s32 view, s32 axis, s32 pt) {
    s32 view_along;
    s32 along_ofs;
    s32 view_dist;
    s32 pt_dist;
    s32 across_ofs;
    s32 cross;
    s32 qv;

    along_ofs = (axis * 8) & 0x3F8;
    across_ofs = along_ofs - 8;
    pt_dist = *(s32 *)(pt - across_ofs) - g_MirrorPlane;
    view_dist = *(s32 *)(view - across_ofs) - g_MirrorPlane;
    if ((pt_dist ^ view_dist) >= 0) {
        qv = *(s32 *)(along_ofs + pt);
        view_along = *(s32 *)(along_ofs + view);
        cross = ((s32) ((qv - view_along) * view_dist) / (s32) (pt_dist + view_dist)) + view_along;
        LIT(s32, sp_tmp14) = cross;
        return (u32) (cross - g_MirrorRangeMin) < (u32) (g_MirrorRangeMax - g_MirrorRangeMin);
    }
    return 0;
}

void entity_render_mirror(void) {
    void GsMulCoord2(MATRIX *m1, MATRIX *m2);
    int GsSetRefView2(GsRVIEW2 *pv);
    extern MATRIX g_IdentityMatrix_MATRIX __asm__("g_IdentityMatrix");
    void mirror_reflect_view_l() __asm__("mirror_reflect_view");
    void mirror_update_parts_l() __asm__("mirror_update_parts");
    void render_entity_l() __asm__("render_entity");
    GsRVIEW2 rv;
    u8 *e;
    s32 f;
    s32 w;
    s32 *save;
    u8 **pe;
    u8 **pv;
    u8 *pi;

    mirror_update_parts_l();
    save = LITP(s32, sp_tmp18);
    pe = LITP(u8 *, g_CurrentEntity);
    pv = LITP(u8 *, g_RoomData);
    pi = LITP(u8, g_CutId);
    e = *pe;
    *save = ENT(e)->joints;
    ENT(e)->joints = ENT(e)->mirror_joints;
    f = g_GameStateFlags & GAME_MIRROR_AXIS;
    mirror_reflect_view_l(*pv + *pi * 0x2C + 0x9C, &rv, f != 0, g_MirrorPlane);
    GsSetRefView2(&rv);
    LIT(MATRIX, sp_matrix) = g_IdentityMatrix_MATRIX;
    w = LITP(MATRIX, sp_matrix)->m[0][0];
    LITP(MATRIX, sp_matrix)->m[0][0] = -w;
    GsMulCoord2(LITP(MATRIX, sp_matrix), &g_WorldScreenMatrix);
    render_entity_l(*pe);
    GsSetRefView2((GsRVIEW2 *)(*pv + *pi * 0x2C + 0x9C));
    *(s32 *)(*pe + 0x98) = *save;
}

void reverse_anim_frame_data(void *dobj) {
    /* step one 0x1C-byte TMD primitive at a time (raw byte blocks, as compiled) */
    BYTES(0x1C) *b32;
    BYTES(0x1C) *a32;
    s32 unused;
    s32 last_ofs;
    s32 tmp32;
    u16 *b16;
    u16 *a16;
    u16 tmp16;
    u16 n;
    void *tmd;

    tmd = ((void *) DOBJ5(dobj)->tmd);
    a32 = ((void *) TMD_OBJ(tmd)->primitive_top);
    n = FIELD_AT(tmd, TmdObject, ->n_primitive, u16);
    last_ofs = (n - 1) * 0x1C;
    a16 = (u16 *)a32;
    a32 = (void *)((u8 *) a32 + ({ last_ofs + 0x14; }));
    a16 = (u16 *)((u8 *) a16 + ({ last_ofs + 8; }));
    b32 = (void *)a32;
    b32 = (void *)((u8 *) b32 + 4);
    b16 = a16 + 2;
    do {
        tmp16 = *a16;
        n--;
        *a16 = *b16;
        *b16 = tmp16;
        b16 -= 14;
        tmp32 = *(s32 *)a32;
        a16 -= 14;
        *(s32 *)a32 = *(s32 *)b32;
        *(s32 *)b32 = tmp32;
        b32--;
        a32--;
    } while (n & 0xFFFF);
}

typedef struct {
    u8 pad0[0xC];
    GsDOBJ5 dobj;
    GsCOORDINATE2 coord;
    u8 pad70[0x7C - 0x20 - sizeof(GsCOORDINATE2)];
} JointView;

void setup_entity_joint_animation(void) {
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    void reverse_anim_frame_data_l(GsDOBJ5 *) __asm__("reverse_anim_frame_data");
    Entity *ent;
    u8 cnt;
    u8 n;
    JointView *e;
    s32 size;
    u8 *src;
    u_long *t;
    u32 len;
    u_long *v;
    s32 i;
    GsDOBJ5 *d;

    ent = g_CurrentEntity;
    cnt = ent->joint_count;
    e = (JointView *)(ent->mirror_joints = LIT(s32, g_LoadPointer));
    n = cnt;
    size = n * 0x7C;
    LIT(s32, g_LoadPointer) = size + (s32)e;
    src = (u8 *)ent->joints;
    t = ((u_long *) JOINT(src)->anim_slot);
    len = *t - (s32)t;
    mem_copy((u8 *)e + size, t, len);
    mem_copy(e, src, size);
    v = LIT(u_long *, g_LoadPointer);
    e->dobj.tmd = v;
    LIT(s32, g_LoadPointer) = (len >> 2) * 4 + (s32)v;
    len = (s32)e->dobj.tmd - JOINT(src)->anim_slot;
    LIT(u_long *, sp_tmp20) = v;
    i = 0;
    if (n != 0) {
        do {
            d = &e->dobj;
            GsLinkObject5(LIT(u_long, sp_tmp20), d, i & 0xFF);
            i++;
            e->dobj.coord2 = &e->coord;
            TMD_OBJ(e->dobj.tmd)->primitive_top += len;
            reverse_anim_frame_data_l(d);
            e++;
            LIT(u_long *, g_LoadPointer) = GsPresetObject(d, LIT(u_long *, g_LoadPointer));
        } while ((u32)(i & 0xFF) < cnt);
    }
}

void entity_alloc_weapon_buffers(void) {
    s32 tmd_buf;
    s32 packet_buf;

    g_CurrentEntity->mirror_weapon_tmd = tmd_buf = LIT(s32, g_LoadPointer);
    packet_buf = tmd_buf + 0x7A00;
    LIT(s32, g_LoadPointer) = packet_buf;
    g_CurrentEntity->mirror_weapon_packet = packet_buf;
    LIT(s32, g_LoadPointer) = packet_buf + 0x1A00;
}

void entity_setup_weapon_part(s32 part) {
    s32 reverse_anim_frame_data_p() __asm__("reverse_anim_frame_data");
    s32 mem_copy();
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    GsDOBJ5 *dobj;
    s32 *tmd;
    s32 prim_top;
    s32 ofs;
    u32 new_tmd;
    void *mirror;
    void *joint;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    mirror = (void *)ENT(e)->mirror_joints;
    joint = (void *)ENT(e)->joints;
    ofs = (u8)part * 0x7C;
    joint += ofs;
    tmd = ((s32 *) JOINT(joint)->anim_slot);
    mirror += ofs;
    mem_copy(((u32) ENT(e)->mirror_weapon_tmd), tmd, *tmd - (s32) tmd);
    e2 = LIT(void *, g_CurrentEntity);
    dobj = (GsDOBJ5 *)&JOINT(mirror)->anim_field;
    JOINT(mirror)->anim_slot = (u32) ((u32) ENT(e2)->mirror_weapon_tmd);
    JOINT(mirror)->anim_object = (u32 *) ((u32 *) ENT(e2)->mirror_weapon_packet);
    GsLinkObject5(((u32) ENT(e2)->mirror_weapon_tmd), dobj, 0);
    new_tmd = ((u32) JOINT(mirror)->anim_slot);
    prim_top = ((s32) TMD_OBJ(new_tmd)->primitive_top);
    TMD_OBJ(new_tmd)->primitive_top = (u8 *)((s32) ((new_tmd - JOINT(joint)->anim_slot) + prim_top));
    reverse_anim_frame_data_p(dobj, prim_top);
    GsPresetObject(dobj, (u_long *)((u32 *) CURRENT_ENTITY_LIT->mirror_weapon_packet));
}

void mirror_update_parts(void) {
    s32 mirror_point_visible_l(s32, s32, void *) __asm__("mirror_point_visible");
    u8 *src;
    u8 *dst;
    u8 n;
    s32 t;
    s32 a;

    src = ((u8 *) g_CurrentEntity->joints);
    n = g_CurrentEntity->joint_count;
    dst = ((u8 *) g_CurrentEntity->mirror_joints);
    src += n * 0x7C - 0x7C;
    dst += n * 0x7C - 0x7C;
    do {
        JOINT(dst)->flags = JOINT(src)->flags;
        JOINT(dst)->world = JOINT(src)->world;
        if (JOINT(dst)->flags & 0x40) {
            a = (s32)ROOM_CAMERA_VIEW(g_CutId);
            t = g_GameStateFlags & GAME_MIRROR_AXIS;
            if ((u8)mirror_point_visible_l(a, t != 0,
                                    ((MATRIX *)(&JOINT(dst)->world))->t)) {
                JOINT(dst)->flags |= 1;
            } else {
                JOINT(dst)->flags &= 0xFE;
            }
            if (!(JOINT(src)->flags & 1)) {
                JOINT(dst)->flags &= 0xFE;
            }
        }
        src -= 0x7C;
        dst -= 0x7C;
    } while (--n);
}

void entity_compute_joint_world_matrices(s32 scale) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    void rotate_entity_l() __asm__("rotate_entity");
    u8 *data;
    u8 *t;
    u8 *idx;
    u8 *obj;
    void *p;
    u8 v;
    u8 n;

    v = g_CurrentEntity->id;
    if (((v != 0xD) && (v != 0x12)) || (g_CurrentEntity->behavior_flags != 1)) {
        p = LIT(void *, g_CurrentEntity);
        data = ((u8 *) ENT(p)->anim_header);
        obj = ((u8 *) ENT(p)->joints);
        t = data;
        data = (u8 *)((*(u16 *)t & 0xFFFC) + (u32)t);
        idx = data + ((u16 *)data)[1];
        if (!(ENT(p)->has_enter_switch_zone & 0x80)) {
            RotMatrix((SVECTOR *)((u8 *)p + 0x72), (MATRIX *)((u8 *)p + 0x20));
        }
        if (scale != 0) {
            LIT(s32, sp_vec_vz) = scale;
            LIT(s32, sp_vec_vy) = scale;
            LIT(s32, sp_vec_vx) = scale;
            ScaleMatrix((MATRIX *)((u8 *)LIT(void *, g_CurrentEntity) + 0x20), LITP(VECTOR, sp_vec));
        }
        if ((*(s32 *)obj & 0x48) == 8) {
            GsMulCoord0((MATRIX *)((u8 *)LIT(void *, g_CurrentEntity) + 0x20), (MATRIX *)(&JOINT(obj)->transform), (MATRIX *)(&JOINT(obj)->world));
            JOINT(obj)->fx_frame = 0;
            JOINT(obj)->flags = (JOINT(obj)->flags & 0xF5) | 0x50;
        }
        if (JOINT(obj)->flags & 2) {
            GsMulCoord0((MATRIX *)((u8 *)LIT(void *, g_CurrentEntity) + 0x20), (MATRIX *)(&JOINT(obj)->transform), (MATRIX *)(&JOINT(obj)->world));
        }
        n = data[0];
        while (n--) {
            rotate_entity_l(&JOINT(obj)->world, data, *idx, obj);
            idx++;
        }
    }
}

void rotate_entity(MATRIX *parent_mtx, u8 *arm, u8 idx, u8 *parent) {
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    u8 *entry;
    u8 *obj;
    u8 *list;
    u8 n;

    obj = idx * 0x7C + ((u8 *) ENT(g_CurrentEntity)->joints);
    entry = arm + idx * 4;
    list = arm + EMR_ARM(entry)->children_offset;
    if ((FIELD_AT(obj, Joint, ->flags, s32) & 0x48) == 8) {
        GsMulCoord0(parent_mtx, (MATRIX *)(&JOINT(obj)->transform), (MATRIX *)(&JOINT(obj)->world));
        JOINT(obj)->fx_frame = 0;
        JOINT(obj)->flags = (JOINT(obj)->flags & 0xF5) | 0x50;
    }
    if (JOINT(obj)->flags & 2) {
        GsMulCoord0(parent_mtx, (MATRIX *)(&JOINT(obj)->transform), (MATRIX *)(&JOINT(obj)->world));
    }
    n = EMR_ARM(entry)->child_count;
    while (n--) {
        rotate_entity((MATRIX *)(&JOINT(obj)->world), arm, *list, obj);
        list++;
    }
}

void render_entity(Entity *obj) {
    MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    void SetLightMatrix(MATRIX *m);
    void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
    void GsSetLsMatrix(MATRIX *mp);
    void GsPrstTG3L();
    void GsPrstTG3NL();
    void update_entity_lighting_l() __asm__("update_entity_lighting");
    s32 is_point_in_quad_l() __asm__("is_point_in_quad");
    MATRIX ls;
    MATRIX tmp;
    MATRIX unused;
    u8 *p;
    u8 n;
    u8 c;
    u8 *m;
    void *d;
    s32 t;

    if ((g_CurrentEntity->id == 0xD || g_CurrentEntity->id == 0x12) && g_CurrentEntity->behavior_flags == 1) {
        return;
    }
    LIT(s32, sp_tmp1C) = 0;
    if (!(obj->has_enter_switch_zone & 0x7F)) {
        LIT(s32, sp_tmp1C) = 1;
    }
    n = obj->joint_count - 1;
    p = (u8 *)obj->joints;
    p += n * 0x7C;
    update_entity_lighting_l(&obj->pos_x);
    do {
        c = JOINT(p)->flags;
        if (obj->id == 0xD || obj->id == 0x12) {
            update_entity_lighting_l(p + 0x58);
        }
        if (c & 4) {
            LIT(s16, sp_svec_vy) = 0x1E;
            LIT(s16, sp_svec_vx) = 0;
            LIT(s16, sp_svec_vz) = 0;
            FIELD_AT(p, Joint, ->rotation.vx, s16) = -0x14;
            FIELD_AT(p, Joint, ->rotation.vz, s16) = 0;
            FIELD_AT(p, Joint, ->rotation.vy, s16) = 0xC8;
            joint_severed_limb_step(p, -0x23, -0x64, 1);
        }
        if (!(c & 1)) {
            if (c & 0x20) {
                joint_path_anim_step(p);
            }
            goto next;
        }
        m = (u8 *)&JOINT(p)->world;
        tmp = g_WorldScreenMatrix;
        GsMulCoord0(&g_WorldScreenMatrix, (MATRIX *)m, &ls);
        MulMatrix0((MATRIX *)&g_LightMatrix, (MATRIX *)m, LITP(MATRIX, sp_matrix));
        if (SCRATCH->tmp1C != 0) {
            if (c & 0x74) {
                goto check;
            }
            goto next;
        }
        if (c & 0x74) {
        check:
            if (is_point_in_quad_l(p + 0x58, g_CameraSwitchZones) == 0) {
            next:
                p -= 0x7C;
                continue;
            }
        }
        LIT(void *, sp_tmp38) = FIELD_AT(p, Joint, ->anim_slot, void *);
        SetLightMatrix(LITP(MATRIX, sp_matrix));
        GsSetLsMatrix(&ls);
        if (c & 0x80) {
            t = FIELD_AT(p, Joint, ->anim_object, s32);
            d = LIT(void *, sp_tmp38);
            GsPrstTG3NL(((s32) TMD_OBJ(d)->primitive_top), *(s32 *)d, ((s32) TMD_OBJ(d)->normal_top),
                        LIT(u8, g_ActiveBuffer) * 0x28 + t + 8, TMD_OBJ(d)->n_primitive, 4,
                        LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain);
            p -= 0x7C;
        } else {
            t = FIELD_AT(p, Joint, ->anim_object, s32);
            d = LIT(void *, sp_tmp38);
            GsPrstTG3L(((s32) TMD_OBJ(d)->primitive_top), *(s32 *)d, ((s32) TMD_OBJ(d)->normal_top),
                       LIT(u8, g_ActiveBuffer) * 0x28 + t + 8, TMD_OBJ(d)->n_primitive, 4,
                       LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain);
            p -= 0x7C;
        }
    } while (n-- != 0);
}

void matrix_relative(MATRIX *base, MATRIX *mtx, MATRIX *out) {
    VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
    MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
    MATRIX m;
    VECTOR v;

    TransposeMatrix(base, &m);
    MulMatrix0(&m, mtx, out);
    v.vx = mtx->t[0] - base->t[0];
    v.vy = mtx->t[1] - base->t[1];
    v.vz = mtx->t[2] - base->t[2];
    ApplyMatrixLV(&m, &v, &v);
    out->t[0] = v.vx;
    out->t[1] = v.vy;
    out->t[2] = v.vz;
}
