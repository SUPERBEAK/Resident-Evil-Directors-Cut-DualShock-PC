/* door.c - The door transition: the door script interpreter (door_op_*: model draw order, camera, moves,
   fades, sounds) and the door animation start / update. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


s32 door_op_end(void) {
    g_DoorState = 0;
    return 0;
}

s32 door_op_clear(void) {
    DoorCmdSlot0 *p = g_DoorCmdCur;

    p->active = 0;
    p->loop_depth = 0;
    p->ip = 0;
    p->loop_ip = 0;
    p->loop_count = 0;
    return 0;
}

s32 door_op_wait_free(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    if (g_DoorCmdTable[DOOR_OP(op)->index].active != 0) {
        return 0;
    }
    DOOR_CMD->ip = (void *) (&DOOR_OP(op)->arg0);
    return 1;
}

s32 door_op_yield2(void) {
    g_DoorCmdCur->ip += 2;
    return 0;
}

const char s_bu00[] = "bu00:"; /* memory card device, slot 1 (used by memcard.c) */

s32 door_op_jmp_byte(void) {
    typedef struct {
        u8 pad[4];
        u8 *ip;
    } Scr;
    extern s8 *g_DoorByteVars_l[] __asm__("g_DoorByteVars");
    u8 *p;
    s8 op;
    s8 *var;
    u8 val;

    p = (u8 *)g_DoorCmdCur->ip;
    op = p[3];
    var = g_DoorByteVars_l[(s8)p[2]];
    val = p[4];
    switch (op) {
    case 0:
        if (*var == (s8)val) {
            goto skip;
        }
        break;
    case 1:
        if (*var > (s8)val) {
            goto skip;
        }
        break;
    case 2:
        if (*var >= (s8)val) {
            goto skip;
        }
        break;
    case 3:
        if (*var < (s8)val) {
            goto skip;
        }
        break;
    case 4:
        if (*var <= (s8)val) {
            goto skip;
        }
        break;
    case 5:
        if (*var != (s8)val) {
            goto skip;
        }
        break;
    }
    g_DoorCmdCur->ip += p[1];
skip:
    g_DoorCmdCur->ip += 6;
    return 1;
}

s32 door_op_jmp_short(void) {
    typedef struct {
        u8 pad[4];
        u8 *ip;
    } Scr;
    extern s16 *g_DoorShortVars_l[] __asm__("g_DoorShortVars");
    u8 *p;
    s8 op;
    s16 *var;
    u16 val;

    p = (u8 *)g_DoorCmdCur->ip;
    op = p[3];
    var = g_DoorShortVars_l[(s8)p[2]];
    val = DOOR_OP(p)->arg1;
    switch (op) {
    case 0:
        if (*var == (s16)val) {
            goto skip;
        }
        break;
    case 1:
        if (*var > (s16)val) {
            goto skip;
        }
        break;
    case 2:
        if (*var >= (s16)val) {
            goto skip;
        }
        break;
    case 3:
        if (*var < (s16)val) {
            goto skip;
        }
        break;
    case 4:
        if (*var <= (s16)val) {
            goto skip;
        }
        break;
    case 5:
        if (*var != (s16)val) {
            goto skip;
        }
        break;
    }
    g_DoorCmdCur->ip += DOOR_OP(p)->index;
skip:
    g_DoorCmdCur->ip += 6;
    return 1;
}

s32 door_op_loop_push(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    DOOR_CMD->loop_ip[DOOR_CMD->loop_depth] = (s32) (DOOR_CMD->ip + 4);
    DOOR_CMD->loop_count[DOOR_CMD->loop_depth] = (u16) DOOR_IP->arg0;
    DOOR_CMD->loop_depth = (u16) (DOOR_CMD->loop_depth + 1);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_loop(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
  u16 count;
  void *cmd_at_depth;
  int four;
  cmd_at_depth = g_DoorCmdCur_voidp;
  cmd_at_depth = ((u8 *) cmd_at_depth) + ((*((u16 *) (((s8 *) g_DoorCmdCur_voidp) + 2))) * 2);
  four = 4;
  count = (*((s16 *) (((s8 *) cmd_at_depth) + 0x26))) - 1;
  *((u16 *) (((s8 *) cmd_at_depth) + 0x26)) = count;
  if ((count << 0x10) <= 0)
  {
    *((u16 *) (((s8 *) g_DoorCmdCur_voidp) + 2)) = (u16) ((*((u16 *) (((s8 *) g_DoorCmdCur_voidp) + 2))) - 1);
    *((s32 *) (((s8 *) g_DoorCmdCur_voidp) + four)) = (s32) ((*((s32 *) (((s8 *) g_DoorCmdCur_voidp) + 4))) + 2);
    return 1;
  }
  *((s32 *) (((s8 *) g_DoorCmdCur_voidp) + 4)) = (s32) (*((s32 *) (((s8 *) (g_DoorCmdCur_voidp + ((*((u16 *) (((s8 *) g_DoorCmdCur_voidp) + 2))) * four))) + four)));
  return 0;
}

s32 door_op_activate(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u16 packed;

    packed = DOOR_IP->arg0;
    g_DoorCmdTable[packed & 0xFF].active = 1;
    g_DoorCmdTable[packed & 0xFF].loop_depth = 0;
    g_DoorCmdTable[packed & 0xFF].ip = (void *)g_DoorFile[packed >> 8];
    g_DoorCmdTable[packed & 0xFF].loop_count[0] = 0;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_clear_entry(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    s32 i;

    i = (u16) *((u16 *) DOOR_CMD->ip) >> 8;
    g_DoorCmdTable[i].active = 0;
    g_DoorCmdTable[i].loop_depth = 0;
    g_DoorCmdTable[i].ip = 0;
    g_DoorCmdTable[i].loop_ip[0] = 0;
    g_DoorCmdTable[i].loop_count[0] = 0;
    DOOR_CMD->ip = (u16 *) (((u8 *) DOOR_CMD->ip) + 2);
    return 1;
}

s32 door_op_bytevar_set(void) {
    extern s8 g_DoorByteVars_s8 __asm__("g_DoorByteVars");
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    **(s8 **)(&g_DoorByteVars_s8 + (DOOR_OPB(op)->b1 * 4)) = DOOR_OPB(op)->b2;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_bytevar_add(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u8 *var;
    void *op;

    op = DOOR_CMD->ip;
    var = (u8 *)(((s32 *)&g_DoorByteVars)[DOOR_OPB(op)->b1]);
    *var += DOOR_OPB(op)->b2;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_shortvar_set(void) {
    extern s8 g_DoorShortVars_s8 __asm__("g_DoorShortVars");
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    **(s16 **)(&g_DoorShortVars_s8 + (DOOR_OP(op)->index * 4)) = DOOR_OP(op)->arg0;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_shortvar_add(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u16 *var;
    void *op;

    op = DOOR_CMD->ip;
    var = (u16 *)(((s32 *)&g_DoorShortVars)[DOOR_OP(op)->index]);
    *var += DOOR_OP(op)->arg0;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_fade_in(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    g_FadeTypeId = FIELD_AT(op, DoorOp, ->index, u8);
    g_FadeStep = -(FIELD_AT(op, DoorOp, ->arg0, u16) << 7);
    g_FadeLevel = FIELD_AT(op, DoorOp, ->arg1, u16);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 6);
    return 1;
}

s32 door_op_fade_out(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    g_FadeTypeId = FIELD_AT(op, DoorOp, ->index, u8);
    g_FadeStep = FIELD_AT(op, DoorOp, ->arg0, u16) << 7;
    g_FadeLevel = FIELD_AT(op, DoorOp, ->arg1, u16);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 6);
    return 1;
}

typedef struct {
    u16 flags;
    s16 id;
    GsDOBJ5 dobj;
    GsCOORDINATE2 coord;
    u8 pad2[0x7C - 0x18 - sizeof(GsCOORDINATE2)];
    s32 vert_top;
} DoorModelObject;

s32 door_op_order_setup(void) {
    void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    extern DoorModelObject g_DoorOrderTable_l[] __asm__("g_DoorOrderTable");
    u8 *rec;
    DoorModelObject *obj;
    GsDOBJ5 *dobj;
    u16 f;
    s32 t;
    u8 *g;

    rec = ((u8 *) DCMD(g_DoorCmdCur)->ip);
    obj = &g_DoorOrderTable_l[DOOR_OP(rec)->index];
    if (rec[3] == 0xFF) {
        GsInitCoordinate2(NULL, &obj->coord);
    } else {
        GsInitCoordinate2(&g_DoorOrderTable_l[rec[3]].coord, &obj->coord);
    }
    dobj = &obj->dobj;
    if (DOOR_OP(rec)->arg1 & 0x8000) {
        do {
            GsLinkObject5(g_DoorObjTable, dobj, FIELD_AT(rec, DoorOp, ->arg0, u8));
            g_DoorPacketArea = (s32)GsPresetObject(dobj, (u_long *)g_DoorPacketArea);
        } while (0);
        f = obj->flags;
        obj->dobj.coord2 = &obj->coord;
        if (f & 0x2000) {
            obj->dobj.attribute = 0;
        } else {
            obj->dobj.attribute = 0x40;
        }
    }
    do {
        do {
            t = FIELD_AT(rec, DoorOp, ->arg0, u8);
        } while (0);
    } while (0);
    g = (u8 *)g_DoorCmdCur;
    obj->vert_top = MEMBER_AT(g_DoorFileOrders + ({ t * 0x1C; }), TmdHeader, ->obj0_vert_top, s32);   /* door TMD object t's vertex table */
    obj->flags = DOOR_OP(rec)->arg1;
    obj->id = FIELD_AT(rec, DoorOp, ->arg0, u8);
    DCMD(g)->ip += 6;
    return 1;
}

s32 door_op_cam_matrix(void) {
    s32 GsSetProjection(s32);
    extern s32 g_DoorView_s32 __asm__("g_DoorView");
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    s16 *src;
    s32 *p;

    GsSetProjection(0x101);
    p = &g_DoorView_s32;
    src = (s16 *)(((u8 *) DOOR_CMD->ip) + 2);
    for (; p <= &g_DoorView_s32 + 5; p++) {
        *p = *src++ << DOOR_IP->index;
    }
    do {
        p[0] = 0;
        p[1] = 0;
        DOOR_CMD->ip += 0xE;
    } while (0);
    return 1;
}

s32 door_op_delta_load(void) {
    extern u8 g_DoorViewDelta_u8 __asm__("g_DoorViewDelta");
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u16 *end;
    u16 *src;
    u16 *dst;

    dst = (u16 *)&g_DoorViewDelta_u8;
    end = (u16 *)(&g_DoorViewDelta_u8 + 0xA);
    src = (u16 *)(((s32) DOOR_CMD->ip) + 2);
    if ((u32) end >= (u32) &g_DoorViewDelta_u8) {
        do {
            *dst = *src;
            dst += 1;
            src += 1;
        } while ((u32) end >= (u32) dst);
    }
    DOOR_CMD->ip = (void *) (((s32) DOOR_CMD->ip) + 0xE);
    return 1;
}

typedef struct { s32 v[8]; s16 d[6]; } DoorViewRec;

s32 door_op_matrix_add_delta(void) {
    extern DoorViewRec g_DoorView;
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    s32 *p = g_DoorView.v;
    s16 *q;

    for (q = g_DoorView.d; q <= &g_DoorView.d[5]; q++) {
        *p++ += *q;
    }
    DOOR_CMD->ip += 2;
    return 1;
}

s32 door_op_delta_add(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u16 *delta;
    void *op;

    op = DOOR_CMD->ip;
    delta = (u16 *)((DOOR_OPB(op)->b2 * 2) + &g_DoorViewDelta);
    *delta += (s8) DOOR_OPB(op)->b3;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_order_pos(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;
    void *pos;

    op = DOOR_CMD->ip;
    pos = (DOOR_OP(op)->index << 7) + (u8 *)g_DoorOrderTable[0].coord.coord.t;
    FIELD_AT(pos, DoorPos, ->x, s32) = (s32) ((s16) DOOR_OP(op)->arg0);
    FIELD_AT(pos, DoorPos, ->y, s32) = (s32) ((s16) DOOR_OP(op)->arg1);
    DOOR_POS(pos)->z = (s32) ((s16) DOOR_OP(op)->arg2);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 8);
    return 1;
}

s32 door_op_order_vel(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;
    void *vel;

    op = DOOR_CMD->ip;
    vel = &g_DoorOrderTable[DOOR_OP(op)->index].vel;
    FIELD_AT(vel, DoorVec, ->x, u16) = (u16) DOOR_OP(op)->arg0;
    FIELD_AT(vel, DoorVec, ->y, u16) = (u16) DOOR_OP(op)->arg1;
    DOOR_VEC(vel)->z = (u16) DOOR_OP(op)->arg2;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 8);
    return 1;
}

s32 door_op_order_pos_add_vel(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    s32 ord_off;
    void *vel;
    void *pos;

    ord_off = DOOR_IP->index << 7;
    pos = ord_off + (u8 *)g_DoorOrderTable[0].coord.coord.t;
    vel = ord_off + (u8 *)g_DoorOrderTable[0].vel;
    FIELD_AT(pos, DoorPos, ->x, s32) = (s32) (((s16) DOOR_VEC(vel)->x) + FIELD_AT(pos, DoorPos, ->x, s32));
    FIELD_AT(pos, DoorPos, ->y, s32) = (s32) (((s16) DOOR_VEC(vel)->y) + FIELD_AT(pos, DoorPos, ->y, s32));
    DOOR_POS(pos)->z = (s32) (((s16) DOOR_VEC(vel)->z) + FIELD_AT(pos, DoorPos, ->z, s32));
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 2);
    return 1;
}

s32 door_op_order_vel_add(void) {
    extern u8 *g_DoorCmdCur_p __asm__("g_DoorCmdCur");
    u8 *p;
    u16 *q;

    p = ((u8 *) DCMD(g_DoorCmdCur_p)->ip);
    q = (u16 *)&g_DoorOrderTable[p[1]].vel[p[2]];
    *q += (s8)p[3];
    DCMD(g_DoorCmdCur_p)->ip += 4;
    return 1;
}

s32 door_op_order_rot(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;
    void *rot;

    op = DOOR_CMD->ip;
    rot = &g_DoorOrderTable[DOOR_OP(op)->index].rot;
    FIELD_AT(rot, DoorVec, ->x, u16) = (u16) DOOR_OP(op)->arg0;
    FIELD_AT(rot, DoorVec, ->y, u16) = (u16) DOOR_OP(op)->arg1;
    DOOR_VEC(rot)->z = (u16) DOOR_OP(op)->arg2;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 8);
    return 1;
}

s32 door_op_order_rotvel(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *rotvel;
    void *op;

    op = DOOR_CMD->ip;
    rotvel = &g_DoorOrderTable[DOOR_OPB(op)->b1].rotvel;
    FIELD_AT(rotvel, DoorVec, ->x, s16) = (s16) (s8) DOOR_OPB(op)->b2;
    FIELD_AT(rotvel, DoorVec, ->y, s16) = (s16) (s8) DOOR_OPB(op)->b3;
    MEMBER_AT(rotvel, DoorVec, ->z, s16) = (s16) (s8) DOOR_OPB(op)->b4;
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 6);
    return 1;
}

s32 door_op_order_rot_add_vel(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *rotvel;
    void *rot;

    rot = &g_DoorOrderTable[DOOR_IP->index].rot;
    rotvel = &g_DoorOrderTable[DOOR_IP->index].rotvel;
    FIELD_AT(rot, DoorVec, ->x, u16) = (u16) (FIELD_AT(rot, DoorVec, ->x, u16) + FIELD_AT(rotvel, DoorVec, ->x, u16));
    FIELD_AT(rot, DoorVec, ->y, u16) = (u16) (FIELD_AT(rot, DoorVec, ->y, u16) + FIELD_AT(rotvel, DoorVec, ->y, u16));
    DOOR_VEC(rot)->z = (u16) (FIELD_AT(rot, DoorVec, ->z, u16) + FIELD_AT(rotvel, DoorVec, ->z, u16));
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 2);
    return 1;
}

s32 door_op_order_rotvel_add(void) {
    extern u8 *g_DoorCmdCur_p __asm__("g_DoorCmdCur");
    u8 *p;
    u16 *q;

    p = ((u8 *) DCMD(g_DoorCmdCur_p)->ip);
    q = (u16 *)&g_DoorOrderTable[p[1]].rotvel[p[2]];
    *q += (s8)p[3];
    DCMD(g_DoorCmdCur_p)->ip += 4;
    return 1;
}

s32 door_op_order_vert_set(void) {
    u8 *obj;
    u8 *p;
    u16 *q;

    obj = (u8 *)g_DoorCmdCur;
    p = ((u8 *) DCMD(obj)->ip);
    q = (u16 *)((u8 *)&g_DoorOrderTable[DOOR_OP(p)->index].vert + FIELD_AT(p, DoorOp, ->arg0, u8) * 8);
    q[0] = DOOR_OP(p)->arg1;
    q[1] = DOOR_OP(p)->arg2;
    q[2] = DOOR_OP(p)->arg3;
    DCMD(obj)->ip += 0xA;
    return 1;
}

s32 door_op_order_vert_add(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    u8 *vert;
    void *op;

    op = DOOR_CMD->ip;
    vert = g_DoorOrderTable[DOOR_OP(op)->index].vert;
    vert += FIELD_AT(op, DoorOp, ->arg0, u8) * 8;
    DOOR_VEC(vert)->x = (DOOR_VEC(vert)->x + DOOR_OP(op)->arg1);
    DOOR_VEC(vert)->y = (u16) (DOOR_VEC(vert)->y + DOOR_OP(op)->arg2);
    DOOR_VEC(vert)->z = (u16) (DOOR_VEC(vert)->z + DOOR_OP(op)->arg3);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 0xA);
    return 1;
}

s32 door_op_message(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    set_message_display(DOOR_OP(op)->index, DOOR_OP(op)->arg0);
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_sfx(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    if (g_StatusFlags & STATUS_DOOR_WAIT_BGM) {
        g_DoorState &= ~2;
        return 0;
    }
    op = DOOR_CMD->ip;
        play_sfx(DOOR_OPB(op)->b1, DOOR_OPB(op)->b2, DOOR_OPB(op)->b3);
        DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_order_flags(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    void *op;

    op = DOOR_CMD->ip;
    if (g_DoorOrderTable[DOOR_OP(op)->index].flags != 0) {
        g_DoorOrderTable[DOOR_OP(op)->index].flags = DOOR_OP(op)->arg0;
    }
    DOOR_CMD->ip = (void *) (DOOR_CMD->ip + 4);
    return 1;
}

s32 door_op_order_tpage(void) {
    u16 x;
    u8 *q;
    u8 *r;
    s32 n;

    x = *(u16 *)(((u8 *) DCMD(g_DoorCmdCur)->ip) + 2);
    q = (u8 *)g_DoorOrderTable[x & 0xFF].dobj.packet;
    n = *(s32 *)((u8 *)g_DoorOrderTable[x & 0xFF].dobj.tmd + 0x14) * 2;
    r = q + 8;
    x = (POLY_FT4_P(q)->tpage & 0xFFC0) | (x >> 8);
    goto test;
loop:
    FIELD_AT(r - 8, POLY_FT4, ->tpage, s16) = x;
    r += 0x28;
test:
    if (n-- != 0) {
        goto loop;
    }
    DCMD(g_DoorCmdCur)->ip += 4;
    return 1;
}

s32 door_op_buffer_advance(void) {
    /* g_PacketArea[1] as a symbol of its own: the -O2 code addressed it like a separate global (with
       g_PacketArea[1] GCC keeps the address in a register) */
    extern s32 g_PacketArea_1 __asm__("g_PacketArea+4");
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    g_PacketArea[0] += DOOR_IPF->arg0;
    g_PacketArea_1 += (u16) DOOR_IPF->arg0 >> 1;
    DOOR_CMD_FIELD(ip) = (void *) (DOOR_CMD_FIELD(ip) + 4);
    return 0;
}

s32 door_op_disable_dispatch(void) {
    g_DoorState &= ~2;
    g_DoorCmdCur->ip += 2;
    return 0;
}

s32 door_op_clear_flags2(void) {
    extern void * g_DoorCmdCur_voidp __asm__("g_DoorCmdCur");
    LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
    DOOR_CMD->ip = (void *) (((s32) DOOR_CMD->ip) + 2);
    return 1;
}

void door_anim_init(void) {
    extern s16 g_DoorOrderTable_s16 __asm__("g_DoorOrderTable");
    extern s16 g_DoorView_s16 __asm__("g_DoorView");
    extern u8 g_DoorAnimType_u8 __asm__("g_DoorAnimType");
    extern s16 g_DoorCmdTable_s16 __asm__("g_DoorCmdTable");
    void GsClearOt(u_short offset, u_short point, GsOT *otp);
    void GsMapModelingData(u_long *p);
    void task_sleep_l(s32) __asm__("task_sleep");
    s16 *p;
    s32 *q;
    s32 t;

    if (g_DoorAnimType_u8 & 0x20) {
        g_VSyncMode = 2;
    } else {
        g_VSyncMode = 0;
    }
    g_DoorAnimType_u8 &= 0x1F;
    GsClearOt(0, 0, LITP(GsOT, g_OrderingTables));
    GsClearOt(0, 0, &LITP(GsOT, g_OrderingTables)[1]);
    GsClearOt(0, 0, LITP(GsOT, g_OtMain));
    GsClearOt(0, 0, &LITP(GsOT, g_OtMain)[1]);
    GsClearOt(0, 0, LITP(GsOT, g_OtBg));
    GsClearOt(0, 0, &LITP(GsOT, g_OtBg)[1]);
    g_PacketArea[0] = ADDR_g_DoorPacketBuffer0;
    g_PacketArea[1] = ADDR_g_DoorPacketBuffer1;
    g_DoorState = 3;
    g_DoorByteVar5 = 0;
    LIT(s32, g_GameStateFlags) |= GAME_DOOR_ANIM;
    LIT(s32, g_StatusFlags) |= STATUS_DOOR_WAIT_BGM;
    g_DoorPacketArea = ADDR_g_DoorObjPacketBuf;
    for (p = &g_DoorOrderTable_s16; p <= &g_DoorOrderTable_s16 + 0x2FE; p++) {
        *p = 0;
    }
    for (p = &g_DoorView_s16; p <= &g_DoorView_s16 + 0x15; p++) {
        *p = 0;
    }
    for (p = &g_DoorCmdTable_s16; p <= &g_DoorCmdTable_s16 + 0xDF; p++) {
        *p = 0;
    }
    LIT(s32, g_DoorFile) += ADDR_g_DoorFile;
    t = LIT(s32, g_DoorFileOrders) + ADDR_g_DoorFile;
    LIT(s32, g_DoorFileOrders) = t;
    LIT(s32, g_DoorFileTim) += ADDR_g_DoorFile;
    g_DoorObjTable = t + 4;
    GsMapModelingData((u_long *)(t + 4));
    g_DoorObjTable += 8;
    for (q = LIT(s32 *, g_DoorFile); *q != 0; q++) {
        *q += LIT(s32, g_DoorFile);
    }
    g_DoorCmdTable[0].active = 1;
    g_DoorCmdTable[0].loop_depth = 0;
    g_DoorCmdTable[0].ip = (void *)*g_DoorFile;
    g_DoorCmdTable[0].loop_count[0] = 0;
    task_sleep_l(1);
}

void door_anim_teardown(void) {
    g_VSyncMode = 2;
    g_PacketArea[0] = ADDR_g_PacketBuffer0;
    g_PacketArea[1] = ADDR_g_PacketBuffer1;
    g_FadeLevel = -1;
    LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
    LIT(s32, g_GameStateFlags) ^= GAME_DOOR_ANIM;
}

void door_anim_loop(void) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m);
    void GsSetLightMatrix(MATRIX *mp);
    void GsSetLsMatrix(MATRIX *mp);
    int GsSetRefView2(GsRVIEW2 *pv);
    void GsSortObject5(GsDOBJ5 *objp, GsOT *otp, int shift, u_long *scratch);
    typedef struct {
        u16 active;
        u16 loop_depth;
        u8 *ip;
        u8 pad[0x30];
    } Ent38;
    typedef struct {
        u16 flags;
        u16 pad;
        u32 attribute;
        GsCOORDINATE2 *coord2;
        u32 pad1[3];
        u32 flg;
        MATRIX coord;
        u8 pad3[0x2C];
        SVECTOR rot;
        u8 pad4[0x10];
    } Ent80;
    extern Ent38 *g_DoorCmdCur_l __asm__("g_DoorCmdCur");
    extern Ent38 g_DoorCmdTable_l[] __asm__("g_DoorCmdTable");
    extern s32 (*g_DoorScriptOps_l[])() __asm__("g_DoorScriptOps");
    extern Ent80 g_DoorOrderTable_e[] __asm__("g_DoorOrderTable");
    extern GsRVIEW2 g_DoorView_l __asm__("g_DoorView");
    MATRIX m;
    Ent80 *e;
    s32 t;

    while ((t = g_DoorState) & 1) {
        if (t & 2) {
            for (g_DoorCmdCur_l = g_DoorCmdTable_l; g_DoorCmdCur_l < &g_DoorCmdTable_l[8]; g_DoorCmdCur_l++) {
                if (g_DoorCmdCur_l->active != 0) {
                    while (g_DoorScriptOps_l[*g_DoorCmdCur_l->ip]() != 0) {
                    }
                }
            }
        } else if (!(g_StatusFlags & STATUS_DOOR_WAIT_BGM)) {
            g_DoorState = t | 2;
        }
        GsSetRefView2(&g_DoorView_l);
        for (e = g_DoorOrderTable_e; e < &g_DoorOrderTable_e[12]; e++) {
            if (e->flags & 0x4000) {
                RotMatrix(&e->rot, &e->coord);
                e->flg = 0;
            }
            if (e->flags & 0x8000) {
                GsGetLs(e->coord2, &m);
                if (e->flags & 0x2000) {
                    GsSetLightMatrix(&m);
                }
                GsSetLsMatrix(&m);
                e->attribute &= ~0xE00;
                e->attribute = ((e->flags & 0xF0) << 5) | e->attribute;
                switch (e->flags & 0xF) {
                case 1:
                    GsSortObject5((GsDOBJ5 *)&e->attribute, LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 10, LITP(u_long, sp_tmp00));
                    break;
                case 2:
                    GsSortObject5((GsDOBJ5 *)&e->attribute, LITP(GsOT, g_OtMain) + g_ActiveBuffer, 4, LITP(u_long, sp_tmp00));
                    break;
                case 3:
                    GsSortObject5((GsDOBJ5 *)&e->attribute, LITP(GsOT, g_OtBg) + g_ActiveBuffer, 10, LITP(u_long, sp_tmp00));
                    break;
                }
            }
        }
        task_sleep(1);
    }
}

void door_anim_task(void) {
    s32 door_anim_init_a1() __asm__("door_anim_init");
    s32 door_anim_loop_a1() __asm__("door_anim_loop");
    s32 task_exit();
    door_anim_init_a1();
    door_anim_loop_a1();
    door_anim_teardown();
    task_exit();
}

void apply_screen_shake(void) {
    void GsSetOffset(long offx, long offy);
    int rand(void);
    g_ScreenShakeTimer = rand() % 2;
    g_ScreenShakeAmp = rand() % 2;
    switch (rand() % 4) {
    case 0:
        break;
    case 1:
        g_ScreenShakeAmp *= -1;
        break;
    case 2:
        g_ScreenShakeTimer *= -1;
        break;
    case 3:
        g_ScreenShakeTimer *= -1;
        g_ScreenShakeAmp *= -1;
        break;
    }
    GsSetOffset(g_ScreenShakeTimer + 0xA0, g_ScreenShakeAmp + 0x78);
}

void update_screen_with_borders(void) {
    void ClearImage(RECT *rect, u_char r, u_char g, u_char b);
    int LoadImage(RECT *rect, u_long *p);
    s16 r[4];
    s16 y0;
    u32 *src;
    s32 y;

    r[0] = 0;
    r[2] = 0x140;
    r[3] = 0xF0;
    r[1] = -(LIT(u8, g_ActiveBuffer) == 0) & 0xF0;
    ClearImage((RECT *)r, 0, 0, 0);
    src = LITP(u32, g_DoorFile);
    if (g_BgImageMode == 1) {
        src = LITP(u32, g_DoorFile_TimPixels);
    }
    y0 = 2;
    if ((g_StatusFlags & STATUS_SCREEN_SHAKE) && !(g_GameStateFlags & GAME_MENU_BYTE)) {
        r[0] = (s8)g_ScreenShakeTimer + 2;
        y = g_ScreenShakeAmp;
        y0 = y + 2;
        if (LIT(u8, g_ActiveBuffer) == 0) {
            y0 = y + 0xF2;
        }
    } else {
        r[0] = 2;
        if (g_ActiveBuffer == 0) {
            y0 = 0xF2;
        }
    }
    /* identical arms, found by decomp-permuter: needed for the original scheduling
     * (keep this line layout - GCC 2.6.3 line notes affect the cross-jumped code) */
    do
    {
        if (g_ScreenShakeAmp)
        {
            r[1] = y0;
        }
        else
        {
            r[1] = y0;
        }
    }
    while (0);
    r[2] = 0x13C;
    r[3] = 0xEC;
    LoadImage((RECT *)r, (u_long *)src);
}
