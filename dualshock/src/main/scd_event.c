/* scd_event.c - Room script events (scripted sequences that run every frame) and the screen fades (fade_update,
   set_fading). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


void scd_event_entry_init(void *ev, s32 script) {
    SCD_EVENT(ev)->active = 1;
    FIELD_AT(ev, ScdEvent, ->state, s16) = 0;
    SCD_EVENT(ev)->pc = (u8 *)(((s32 *)g_RoomEventScripts)[script]);
    SCD_EVENT(ev)->entity = g_CurrentEntity;
    SCD_EVENT(ev)->sp = -1;
}

void scd_event_entry_create(u32 slot, s32 script) {
    void scd_event_entry_init(void *, s32);
    u32 i = slot;

    if (i >= 8) {
        i = 0;
        if (LITP(ScdEvent, g_ScdEventTable)[0].active != 0) {
            while (1) {
                if (i == 7) break;
                i++;
                if (LITP(ScdEvent, g_ScdEventTable)[i].active == 0) break;
            }
        }
    }
    scd_event_entry_init(LITP(u8, g_ScdEventTable) + i * 0x34, script);
}

__asm__("#JTALIGN2");
__asm__("#JTALIGN2");
typedef struct {
    u8 state;
    u8 unused_01;
    u8 active;
    s8 sp;
    u8 *entity;
    u8 *pc;
    u8 *ret[4];
    u8 *call[4];
    s16 cnt[4];
} ScdEventView;

void room_events_check(void) {
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    extern u8 *g_ScdIp_l __asm__("g_ScdIp");
    extern ScdEventView *g_pScdEventCurrent_l __asm__("g_pScdEventCurrent");
    extern s32 (*g_ScdOpcodeTable_l[])(u8, ScdEventView *) __asm__("g_ScdOpcodeTable");
    s32 scd_event_state1_anim_l(u8, ScdEventView *) __asm__("scd_event_state1_anim");
    void scd_event_state2_movement_l(u8, ScdEventView *) __asm__("scd_event_state2_movement");
    s32 scd_event_state3_set_behavior_l(u8, ScdEventView *) __asm__("scd_event_state3_set_behavior");
    void scd_event_cmd_set_entity_l(u8, ScdEventView *) __asm__("scd_event_cmd_set_entity");
    void scd_event_cmd_create_l(u8, ScdEventView *) __asm__("scd_event_cmd_create");
    void scd_event_cmd_run_scd_l(u8, ScdEventView *) __asm__("scd_event_cmd_run_scd");
    void scd_event_cmd_exec_l(u8, ScdEventView *) __asm__("scd_event_cmd_exec");
    void scd_event_cmd_init_l(u8, ScdEventView *) __asm__("scd_event_cmd_init");
    ScdEventView *c;
    s32 op;
    u32 i;

    if (!(g_UpdateFlags & 0x80)) {
        return;
    }
    for (i = 0; i < 8; i++) {
        g_pScdEventCurrent_l = &LITP(ScdEventView, g_ScdEventTable)[i];
        if (LITP(ScdEventView, g_ScdEventTable)[i].active == 0) {
            continue;
        }
    loop:
        c = g_pScdEventCurrent_l;
        op = *c->pc;
        switch (op) {
        case 0xF6:
            LIT(ScdEventView *, g_pScdEventCurrent)->sp++;
            LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
        case 0xF7:
            if (Flg_ck(ADDR_g_GameStateFlags, 0xE) == 0 && !(g_MessageStatus_l & 0x80)) {
                g_pScdEventCurrent_l->pc++;
                g_pScdEventCurrent_l->sp--;
            }
            goto end;
        case 0xF8:
            LIT(ScdEventView *, g_pScdEventCurrent)->sp++;
            LIT(ScdEventView *, g_pScdEventCurrent)->cnt[LIT(ScdEventView *, g_pScdEventCurrent)->sp] = *(u16 *)(LIT(ScdEventView *, g_pScdEventCurrent)->pc + 2);
            LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
        case 0xF9:
            if (--g_pScdEventCurrent_l->cnt[g_pScdEventCurrent_l->sp] == 0) {
                g_pScdEventCurrent_l->pc += 3;
                g_pScdEventCurrent_l->sp--;
            }
            goto end;
        case 0xFA:
            LIT(ScdEventView *, g_pScdEventCurrent)->sp++;
            LIT(ScdEventView *, g_pScdEventCurrent)->cnt[LIT(ScdEventView *, g_pScdEventCurrent)->sp] = *(u16 *)(LIT(ScdEventView *, g_pScdEventCurrent)->pc + 2);
            LIT(ScdEventView *, g_pScdEventCurrent)->ret[LIT(ScdEventView *, g_pScdEventCurrent)->sp] = LIT(ScdEventView *, g_pScdEventCurrent)->pc += 4;
            goto loop;
        case 0xFB:
            if (--g_pScdEventCurrent_l->cnt[g_pScdEventCurrent_l->sp] != 0) {
                g_pScdEventCurrent_l->pc = g_pScdEventCurrent_l->ret[g_pScdEventCurrent_l->sp];
            } else {
                g_pScdEventCurrent_l->pc++;
                g_pScdEventCurrent_l->sp--;
            }
            goto loop;
        case 0xFC:
            LIT(ScdEventView *, g_pScdEventCurrent)->sp++;
            LIT(ScdEventView *, g_pScdEventCurrent)->call[LIT(ScdEventView *, g_pScdEventCurrent)->sp] = LIT(ScdEventView *, g_pScdEventCurrent)->pc + 2;
            LIT(ScdEventView *, g_pScdEventCurrent)->ret[LIT(ScdEventView *, g_pScdEventCurrent)->sp] = LIT(ScdEventView *, g_pScdEventCurrent)->pc = (u8 *)(LIT(ScdEventView *, g_pScdEventCurrent)->pc[1] + (s32)LIT(ScdEventView *, g_pScdEventCurrent)->pc);
            goto loop;
        case 0xFD:
            g_ScdIp_l = LIT(ScdEventView *, g_pScdEventCurrent)->call[LIT(ScdEventView *, g_pScdEventCurrent)->sp];
            if (g_ScdOpcodeTable_l[*g_ScdIp_l](op, c) != 0) {
                LIT(ScdEventView *, g_pScdEventCurrent)->pc = LIT(ScdEventView *, g_pScdEventCurrent)->ret[LIT(ScdEventView *, g_pScdEventCurrent)->sp];
            } else {
                LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
                LIT(ScdEventView *, g_pScdEventCurrent)->sp--;
            }
            goto loop;
        case 0xFF:
            g_pScdEventCurrent_l->active = 0;
            goto inc;
        default:
            switch (c->state) {
            case 0:
                switch (op) {
                case 0:
                    g_pScdEventCurrent_l->pc++;
                    goto loop;
                case 1:
                    LIT(ScdEventView *, g_pScdEventCurrent)->state = 1;
                    LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
                    goto loop;
                case 2:
                    LIT(ScdEventView *, g_pScdEventCurrent)->state = 2;
                    LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
                    LIT(ScdEventView *, g_pScdEventCurrent)->entity[0x85] = 2;
                    LIT(ScdEventView *, g_pScdEventCurrent)->entity[0x86] = 0;
                    LIT(ScdEventView *, g_pScdEventCurrent)->entity[0x87] = 0;
                    goto loop;
                case 3:
                    LIT(ScdEventView *, g_pScdEventCurrent)->state = 2;
                    LIT(ScdEventView *, g_pScdEventCurrent)->pc++;
                    goto loop;
                case 4:
                    scd_event_cmd_set_entity_l(op, c);
                    goto loop;
                case 5:
                    scd_event_cmd_create_l(op, c);
                    goto loop;
                case 6:
                    scd_event_cmd_run_scd_l(op, c);
                    goto loop;
                case 7:
                    scd_event_cmd_exec_l(op, c);
                    goto loop;
                case 8:
                    scd_event_cmd_init_l(op, c);
                    goto end;
                case 9:
                    LITP(ScdEventView, g_ScdEventTable)[LIT(ScdEventView *, g_pScdEventCurrent)->pc[1]].active = 0;
                    LIT(ScdEventView *, g_pScdEventCurrent)->pc += 2;
                    goto loop;
                default:
                    c->active = 0;
                    return;
                }
            case 1:
                if (scd_event_state1_anim_l(op, c) == 0) {
                    goto end;
                }
                goto loop;
            case 2:
                scd_event_state2_movement_l(op, c);
                goto loop;
            case 3:
                if (scd_event_state3_set_behavior_l(op, c) == 0) {
                    goto end;
                }
                goto loop;
            }
            goto loop;
        case 0xFE:
        inc:
            g_pScdEventCurrent_l->pc++;
        }
    end:
        if (*g_pScdEventCurrent_l->pc == 0xFF) {
            g_pScdEventCurrent_l->active = 0;
        }
    }
}

typedef union {
    u16 h;
    struct {
        u8 lo, hi;
    } b;
} ScrHB;
typedef struct {
    /* 0x00 */ u8 status_flags, id;
    /* 0x02 */ u8 pad2[0x82];
    /* 0x84 */ ScrHB state;
    /* 0x86 */ ScrHB action_behavior;
    /* 0x88 */ u8 pad88[2];
    /* 0x8A */ u8 hit_state;
    /* 0x8B */ u8 pad8B;
    /* 0x8C */ u8 blend_counter;
    /* 0x8D */ u8 pad8D[0x2B];
    /* 0xB8 */ void *aim_target;
    /* 0xBC */ u8 padBC;
    /* 0xBD */ u8 animation_id, animation_frame_id, timing_control;
    /* 0xC0 */ u8 padC0[2];
    /* 0xC2 */ u16 move_speed_current;
    /* 0xC4 */ u8 padC4[2];
    /* 0xC6 */ u16 anim_base_x, anim_base_z;
    /* 0xCA */ u8 padCA[2];
    /* 0xCC */ s32 scd_pos_x, scd_pos_y, scd_pos_z;
    /* 0xD8 */ u8 look_at_flags, look_at_yaw_step, look_at_pitch_step, scd_anim_param, collision_flags;
    /* 0xDD */ u8 padDD;
    /* 0xDE */ u16 scd_timer, scd_entity_flags;
} ScrObj2;
typedef struct {
    /* 0x00 */ u8 state, unused_01;
    /* 0x04 */ ScrObj2 *entity;
    /* 0x08 */ u8 *pc;
} ScrCtx2;

s32 scd_event_state1_anim(void) {
    extern ScrCtx2 *g_pScdEventCurrent_c2 __asm__("g_pScdEventCurrent");
    extern void *g_OmodelTable_l[] __asm__("g_OmodelTable");
    extern void *g_ItemModelTable_l[] __asm__("g_ItemModelTable");
    ScrObj2 *obj;
    u8 *pc;
    s16 *q;
    s32 v;
    u8 c;
    s8 t;

    pc = LIT(ScrCtx2 *, g_pScdEventCurrent)->pc;
    switch (*pc) {
    case 0x81:
        LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->look_at_flags = *(u16 *)pc >> 8;
        if ((LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->look_at_flags & 0xF) == 0) {
            break;
        }
        LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 8;
        obj = LIT(ScrCtx2 *, g_pScdEventCurrent)->entity;
        if (obj->look_at_flags == 0x93) {
            q = (s16 *)(&SCD_EV_CMD(pc)->arg0);
            switch (*q++) {
            case 0:
                obj->aim_target = LITP(void, g_PlayerEntity);
                break;
            case 1:
                obj->aim_target = LITP(u8, g_EnemiesList) + *q * ENTITY_SIZE;
                break;
            case 2:
                obj->aim_target = g_OmodelTable_l[*q];
                break;
            case 3:
                obj->aim_target = g_ItemModelTable_l[*q];
                break;
            }
            q += 2;
        } else {
            q = (s16 *)(pc + 8);
            if (obj->look_at_flags & 0x20) {
            q = (s16 *)(&SCD_EV_CMD(pc)->arg0);
            v = *q++;
            if (v < 0) {
                obj->scd_pos_x = v + 0x1000;
            } else {
                obj->scd_pos_x = v;
            }
            if (*q < 0) {
                g_pScdEventCurrent_c2->entity->scd_pos_y = *q++ + 0x1000;
            } else {
                g_pScdEventCurrent_c2->entity->scd_pos_y = *q++;
            }
            g_pScdEventCurrent_c2->entity->scd_pos_z = *q++;
            } else {
            obj->scd_pos_x = SCD_EV_CMD(pc)->arg0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_pos_y = SCD_EV_CMD(pc)->arg1;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_pos_z = SCD_EV_CMD(pc)->arg2;
            }
        }
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->look_at_yaw_step = (*q & 0xFF) ? *(u8 *)q : 0xC0;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->look_at_pitch_step = (*q >> 8) ? (*q >> 8) : 0x40;
        break;
    case 0x82:
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->look_at_flags &= 0xEF;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->pc++;
        break;
    case 0x83:
        {
            u32 a;
            u32 b;

            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->state.h = 8;
            a = *(u16 *)LIT(ScrCtx2 *, g_pScdEventCurrent)->pc >> 8;
            if (a & 0x10) {
                if (LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->collision_flags & 0x80) {
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.b.lo = a & 0xF;
                } else {
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.h = a & 0xF;
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->collision_flags |= 0x80;
                }
            } else {
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.h = a;
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->collision_flags &= 0x7F;
            }
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 2;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->anim_base_x = *(u16 *)LIT(ScrCtx2 *, g_pScdEventCurrent)->pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 2;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->anim_base_z = *(u16 *)LIT(ScrCtx2 *, g_pScdEventCurrent)->pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 2;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_anim_param = *LIT(ScrCtx2 *, g_pScdEventCurrent)->pc++;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_timer = 0x28;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags = 0;
        }
        break;
    case 0x84:
        {
            u32 a;
            u32 b;

            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->state.h = 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.h = 1;
            a = *(u16 *)LIT(ScrCtx2 *, g_pScdEventCurrent)->pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 2;
            b = *(u16 *)LIT(ScrCtx2 *, g_pScdEventCurrent)->pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc += 2;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id = a >> 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_anim_param = b;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags = (b & 0xFF00) >> 6;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_timer = 0;
        }
        break;
    case 0x89:
        {
            u32 a;

            a = *(u16 *)pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_frame_id = a >> 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->timing_control = 0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->blend_counter = 7;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->move_speed_current = 0;
            if (LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags & 0x20) {
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->blend_counter = 0;
            }
            if (LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->id < 0x20) {
                if (LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id >= 0x10) {
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.b.hi = 3;
                } else {
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.b.hi = g_ScdAnimChainTbl[LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id][0] + 1;
                    LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id = g_ScdAnimChainTbl[LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id][1];
                }
            } else {
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.b.hi = 1;
            }
        }
        break;
    case 0x8A:
        {
            u32 a;

            a = *(u16 *)pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_frame_id = a >> 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->timing_control = 0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->blend_counter = 7;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->move_speed_current = 0;
            if (LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags & 0x20) {
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->blend_counter = 0;
            }
        }
        break;
    case 0x85:
        {
            u32 a;
            u32 b;

            a = *(u16 *)pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
            b = ((u16) SCD_EV_CMD(pc)->arg0);
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg1;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->state.h = 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.h = a >> 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->animation_id = b;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_anim_param = b >> 8;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_timer = 0;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags = 0;
        }
        break;
    case 0x86:
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->state.h = 1;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->action_behavior.h = 0;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->hit_state = 0;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->pc++;
        break;
    case 0x87:
        {
            u32 a;
            u32 b;

            a = *(u16 *)pc;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
            b = ((u16) SCD_EV_CMD(pc)->arg0);
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg1;
            switch (a >> 8) {
            case 0:
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags |= b;
                break;
            case 1:
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags = b;
                break;
            case 2:
                LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_entity_flags ^= b;
                break;
            }
        }
        break;
    case 0x88:
        {
            u32 b;

            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg0;
            b = ((u16) SCD_EV_CMD(pc)->arg0);
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = (u8 *)&SCD_EV_CMD(pc)->arg1;
            LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->scd_timer = b;
        }
        break;
    case 0x80:
        LIT(ScrCtx2 *, g_pScdEventCurrent)->entity->state.b.hi = 0;
    case 0x8B:
        LIT(ScrCtx2 *, g_pScdEventCurrent)->unused_01 = 0;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->pc++;
        LIT(ScrCtx2 *, g_pScdEventCurrent)->state = 0;
        break;
    case 0:
        /* do-while(0): weights this use of pc so it is allocated before the cached ctx */
        do {
            LIT(ScrCtx2 *, g_pScdEventCurrent)->pc = &SCD_EV_CMD(pc)->index;
        } while (0);
        break;
    }
    return 1;
}

s32 scd_event_state3_set_behavior(void) {
    u8 *pc;
    void *entity;
    void *ev2;
    void *ev;
    void *ev3;

    ev = LIT(void *, g_pScdEventCurrent);
    g_PadHeld = 0xFF;
    entity = SCD_EVENT(ev)->entity;
    if (ENT(entity)->action_behavior != *SCD_EVENT(ev)->pc) {
        ENT(entity)->action_state = 0;
    }
    ev2 = LIT(void *, g_pScdEventCurrent);
    pc = SCD_EVENT(ev2)->pc;
    SCD_EVENT(ev2)->pc = (u8 *) (pc + 1);
    ENT(SCD_EVENT(ev2)->entity)->action_behavior = (u8) *pc;
    ev3 = LIT(void *, g_pScdEventCurrent);
    SCD_EVENT(ev3)->pc = (u8 *) (SCD_EVENT(ev3)->pc + 1);
    return 1;
}

/* both jump tables (jtbl_800101FC, jtbl_8001022C) are 4-aligned: one marker per table */
__asm__("#JTALIGN2");
__asm__("#JTALIGN2");
typedef struct {
    /* 0x00 */ u8 pad0[0x34];
    /* 0x34 */ s32 pos_x, pos_y, pos_z;
    /* 0x40 */ u8 pad40[0x32];
    /* 0x72 */ u16 rot_x, angle, angle_z;
    /* 0x78 */ s16 speed_vx, speed_vy, speed_vz;
    /* 0x7E */ u16 prev_angle;
    /* 0x80 */ u16 scd_rot_speed_x, scd_rot_speed_y;
    /* 0x84 */ u8 state[4];
    /* 0x88 */ u16 health;
    /* 0x8A */ u8 hit_state;
    /* 0x8B */ u8 pad8B[0x3B];
    /* 0xC6 */ u16 anim_base_x, anim_base_z;
} ScrObj;

typedef struct {
    /* 0x00 */ u8 state;
    /* 0x04 */ ScrObj *entity;
    /* 0x08 */ u8 *pc;
} ScrCtx;

void scd_event_state2_movement(void) {
    extern ScrCtx *g_pScdEventCurrent_c __asm__("g_pScdEventCurrent");
    void *p;

    switch (*g_pScdEventCurrent_c->pc) {
    case 0:
        g_pScdEventCurrent_c->pc++;
        break;
    case 2:
        g_pScdEventCurrent_c->entity->pos_x += g_pScdEventCurrent_c->entity->speed_vx;
        g_pScdEventCurrent_c->entity->pos_y += g_pScdEventCurrent_c->entity->speed_vy;
        g_pScdEventCurrent_c->entity->pos_z += g_pScdEventCurrent_c->entity->speed_vz;
        g_pScdEventCurrent_c->pc++;
        break;
    case 3:
        g_pScdEventCurrent_c->entity->rot_x += g_pScdEventCurrent_c->entity->scd_rot_speed_x;
        g_pScdEventCurrent_c->entity->angle += g_pScdEventCurrent_c->entity->scd_rot_speed_y;
        g_pScdEventCurrent_c->entity->angle_z += *(u16 *)g_pScdEventCurrent_c->entity->state;
        g_pScdEventCurrent_c->pc++;
        break;
    case 4:
        g_pScdEventCurrent_c->entity->pos_x += g_pScdEventCurrent_c->entity->speed_vx;
        g_pScdEventCurrent_c->entity->pos_y += g_pScdEventCurrent_c->entity->speed_vy;
        g_pScdEventCurrent_c->entity->pos_z += g_pScdEventCurrent_c->entity->speed_vz;
        g_pScdEventCurrent_c->entity->rot_x += g_pScdEventCurrent_c->entity->scd_rot_speed_x;
        g_pScdEventCurrent_c->entity->angle += g_pScdEventCurrent_c->entity->scd_rot_speed_y;
        g_pScdEventCurrent_c->entity->angle_z += *(u16 *)g_pScdEventCurrent_c->entity->state;
        g_pScdEventCurrent_c->pc++;
        break;
    case 5:
        g_pScdEventCurrent_c->entity->speed_vx = (s8)g_pScdEventCurrent_c->pc[1];
        g_pScdEventCurrent_c->entity->speed_vy = (s8)g_pScdEventCurrent_c->pc[2];
        g_pScdEventCurrent_c->entity->speed_vz = (s8)g_pScdEventCurrent_c->pc[3];
        g_pScdEventCurrent_c->pc += 4;
        break;
    case 6:
        g_pScdEventCurrent_c->entity->scd_rot_speed_x = (s8)g_pScdEventCurrent_c->pc[1];
        g_pScdEventCurrent_c->entity->scd_rot_speed_y = (s8)g_pScdEventCurrent_c->pc[2];
        *(u16 *)g_pScdEventCurrent_c->entity->state = (s8)g_pScdEventCurrent_c->pc[3];
        g_pScdEventCurrent_c->pc += 4;
        break;
    case 7:
        g_pScdEventCurrent_c->entity->pos_x = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg0, s16);
        g_pScdEventCurrent_c->entity->pos_y = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg1, s16);
        g_pScdEventCurrent_c->entity->pos_z = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg2, s16);
        g_pScdEventCurrent_c->pc += 8;
        break;
    case 11:
        g_pScdEventCurrent_c->entity->rot_x = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg0, u16);
        g_pScdEventCurrent_c->entity->angle = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg1, u16);
        g_pScdEventCurrent_c->entity->angle_z = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg2, u16);
        g_pScdEventCurrent_c->pc += 8;
        break;
    case 8:
        *(u8 *)(LIT(ScrCtx *, g_pScdEventCurrent)->pc[1] + (s32)LIT(ScrCtx *, g_pScdEventCurrent)->entity + 0x84) = LIT(ScrCtx *, g_pScdEventCurrent)->pc[2];
        LIT(ScrCtx *, g_pScdEventCurrent)->pc += 3;
        break;
    case 9:
        LIT(ScrCtx *, g_pScdEventCurrent)->entity->hit_state = LIT(ScrCtx *, g_pScdEventCurrent)->pc[1];
        LIT(ScrCtx *, g_pScdEventCurrent)->pc += 2;
        break;
    case 10:
        switch (*(u16 *)g_pScdEventCurrent_c->pc >> 8) {
        case 0:
            p = &g_pScdEventCurrent_c->entity->pos_x;
            break;
        case 1:
            p = &g_pScdEventCurrent_c->entity->pos_y;
            break;
        case 2:
            p = &g_pScdEventCurrent_c->entity->pos_z;
            break;
        case 3:
            p = &g_pScdEventCurrent_c->entity->health;
            goto half;
        case 4:
            p = &g_pScdEventCurrent_c->entity->anim_base_x;
            goto half;
        case 5:
            p = &g_pScdEventCurrent_c->entity->anim_base_z;
            goto half;
        }
        *(s32 *)p = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg0, s16);
        goto next;
    half:
        *(u16 *)p = FIELD_AT(g_pScdEventCurrent_c->pc, ScdEventCmd, ->arg0, u16);
    next:
        g_pScdEventCurrent_c->pc += 4;
        break;
    case 1:
        LIT(ScrCtx *, g_pScdEventCurrent)->state = 0;
        LIT(ScrCtx *, g_pScdEventCurrent)->pc++;
        break;
    }
}

void scd_event_cmd_set_entity(void) {
    extern s32 g_ItemModelTable_s32[] __asm__("g_ItemModelTable");
    extern s32 g_OmodelTable_s32[] __asm__("g_OmodelTable");
    u8 *obj;
    u8 *p;

    p = SCD_EVENT(g_pScdEventCurrent)->pc;
    SCD_EVENT(g_pScdEventCurrent)->pc = p + 1;
    switch (p[1]) {
    case 0:
        SCD_EVENT(g_pScdEventCurrent)->entity = (void *)ADDR_g_PlayerEntity;
        break;
    case 1:
        SCD_EVENT(g_pScdEventCurrent)->entity = (void *)(p[2] * ENTITY_SIZE + ADDR_g_EnemiesList);
        break;
    case 2:
        SCD_EVENT(g_pScdEventCurrent)->entity = (void *)g_OmodelTable_s32[p[2]];
        break;
    case 3:
        SCD_EVENT(g_pScdEventCurrent)->entity = (void *)g_ItemModelTable_s32[p[2]];
        break;
    }
    SCD_EVENT(g_pScdEventCurrent)->pc += 2;
}

void scd_event_cmd_create(void) {
    s32 scd_event_entry_create_a1(u8, u8) __asm__("scd_event_entry_create");
    void *ev;
    void *pc;
    void *ev2;

    ev = LIT(void *, g_pScdEventCurrent);
    pc = ((void *) SCD_EVENT(ev)->pc);
    SCD_EVENT(ev)->pc = (void *) (pc + 1);
    scd_event_entry_create_a1(SCD_EVENT_CMD_CREATE(pc)->slot, SCD_EVENT_CMD_CREATE(pc)->script);
    ev2 = LIT(void *, g_pScdEventCurrent);
    SCD_EVENT(ev2)->pc = (void *) (((void *) SCD_EVENT(ev2)->pc) + 3);
}

void scd_event_cmd_init(void) {
    scd_event_entry_init(g_pScdEventCurrent, SCD_EVENT(g_pScdEventCurrent)->pc[1]);
}

void scd_event_cmd_run_scd(void) {
  void *ev;
  void *pc;
  scd_run_command_functions((*((void **) (((s8 *) (LIT(void *, g_pScdEventCurrent))) + 8))) + 2);
  ev = LIT(void *, g_pScdEventCurrent);
  pc = *((void **) (((s8 *) ev) + 8));
  *((void **) (((s8 *) ev) + 8)) = (void *) ((*((u8 *) (((s8 *) pc) + 1))) + (*((void **) (((s8 *) ev) + 8))));
}

void scd_event_cmd_exec() {
    extern s32 (*g_ScdOpcodeTable[])();
    void *cmd;
    u16 h;

    cmd = ((void *) SCD_EVENT(g_pScdEventCurrent)->pc);
    h = FIELD_AT(cmd, ScdEventCmdExec, ->head, u16);
    g_ScdIp = cmd + 2;
    SCD_EVENT(g_pScdEventCurrent)->pc = (void *) ((h >> 8) + ((void *) SCD_EVENT(g_pScdEventCurrent)->pc));
    g_ScdOpcodeTable[FIELD_AT(cmd, ScdEventCmdExec, ->opcode, u8)](g_pScdEventCurrent, cmd);
}

void fade_update(void) {
    if ((LIT(s16, g_FadeLevel) <= 0) && (g_FadeStep != 0)) {
        LIT(s16, g_FadeLevel) = -(g_FadeStep < 1) & 0x7FFF;
    }
}

void set_fading(s8 fade_type, s16 step) {
    if (g_FadeLevel <= 0) {
        g_FadeTypeId = fade_type;
        g_FadeStep = step;
        LIT(s32, g_GameStateFlags) |= GAME_FADE_ACTIVE;
    }
}
