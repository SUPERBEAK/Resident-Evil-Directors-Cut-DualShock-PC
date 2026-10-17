/* message.c - text printing (string tables, item names, 8x8 / 8x14 fonts) and the message window */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

void print_string_table_entry(u16 idx) {
    void GsSortFlipSprite();
    extern u16 g_ScreenOffsetX_l[] __asm__("g_ScreenOffsetX");
    u8 *p;
    u8 n;
    u8 y;

    SCRATCH->spr_flags = 0x40;
    SCRATCH->spr_w = 8;
    SCRATCH->spr_h = 0xE;
    SCRATCH->spr_r = 0x80;
    SCRATCH->spr_g = 0x80;
    SCRATCH->spr_b = 0x80;
    SCRATCH->spr_pivot_x = 0;
    SCRATCH->spr_pivot_y = 0;
    SCRATCH->spr_rotate = 0;
    p = g_StringTablePtrs[idx];
    do {
        n = *p++;
        SCRATCH->spr_clut_x = ((*p & 3) << 4) + 0x100;
        SCRATCH->spr_clut_y = (*p++ >> 2) + 0x1E0;
        SCRATCH->spr_x = *p++ * 2 - g_ScreenOffsetX_l[0];
        SCRATCH->spr_y = *p++ * 2 - g_ScreenOffsetX_l[1];
        do {
            switch (*p) {
            case 0xFF:
                SCRATCH->spr_x += 4;
                break;
            case 0xF8:
                p++;
                y = (u8)(*p / 18) * 14 - 0x2E;
                goto set;
            case 0xF9:
                p++;
                SCRATCH->spr_tpage = 0x1F;
                SCRATCH->spr_v = (u8)(*p / 18) * 14;
                goto draw;
            case 0xFA:
                p++;
                SCRATCH->spr_tpage = 0x1F;
                SCRATCH->spr_v = (u8)(*p / 18) * 14 - 0x3C;
                goto draw;
            default:
                y = (u8)(*p / 18) * 14 + 0x1C;
            set:
                SCRATCH->spr_v = y;
                SCRATCH->spr_tpage = 0x1E;
            draw:
                SCRATCH->spr_u = (u8)(*p % 18) * 8;
                GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 2);
            case 0:
                SCRATCH->spr_x += 8;
                break;
            }
            p++;
        } while (--n);
    } while (*p != 0);
}

void draw_item_name(u8 item_id, u8 attr, u16 screen_x, s16 screen_y) {
    void GsSortFlipSprite();
    u8 *message_item_name_lookup_l(s32) __asm__("message_item_name_lookup");
    u32 n;
    s16 x;
    s16 h;
    u8 *p;
    u8 y;

    if (item_id != 0) {
        n = attr >> 4;
        if (n == 0) {
            n = 2;
        }
        x = attr & 3;
        h = (attr & 0xF) >> 2;
        SCRATCH->spr_flags = 0x40;
        SCRATCH->spr_w = 8;
        SCRATCH->spr_h = 0xE;
        SCRATCH->spr_x = screen_x;
        SCRATCH->spr_y = screen_y;
        SCRATCH->spr_clut_x = (x << 4) + 0x100;
        SCRATCH->spr_clut_y = h + 0x1E0;
        SCRATCH->spr_r = 0x80;
        SCRATCH->spr_g = 0x80;
        SCRATCH->spr_b = 0x80;
        SCRATCH->spr_pivot_x = 0;
        SCRATCH->spr_pivot_y = 0;
        SCRATCH->spr_rotate = 0;
        p = message_item_name_lookup_l(item_id); /* arg0 passed straight through in a0 */
        while (*p != 7) {
            switch (*p) {
            case 0xF8:
                p++;
                SCRATCH->spr_tpage = 0x1E;
                y = (u8)(*p / 18) * 14 - 0x2E;
                break;
            case 0xF9:
                p++;
                SCRATCH->spr_tpage = 0x1F;
                y = (u8)(*p / 18) * 14;
                break;
            case 0xFA:
                p++;
                SCRATCH->spr_tpage = 0x1F;
                y = (u8)(*p / 18) * 14 - 0x3C;
                break;
            default:
                SCRATCH->spr_tpage = 0x1E;
                y = (u8)(*p / 18) * 14 + 0x1C;
                break;
            }
            SCRATCH->spr_v = y;
            SCRATCH->spr_u = (u8)(*p % 18) * 8;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), n);
            p++;
            SCRATCH->spr_x += 8;
        }
    }
}

s32 message_item_name_lookup(s32 item_id) {
  s32 flag_idx;
  s32 offset;
  s32 name;
  s32 unused;
  u32 idx;
  u8 examine_idx;
  idx = (item_id - 1) & 0xFF;
  offset = idx * 4;
  name = *((s32 *) (((u8 *) (&g_ItemNames)) + offset));
  if (idx < 0x4DU)
  {
    examine_idx = *((u8 *) (((u8 *) (&g_ItemExamineIdx)) + offset));
    unused = name;
    if (!(examine_idx & 0x80))
    {
      flag_idx = examine_idx & 0xFF;
      unused = name;
      if (Flg_ck(ADDR_g_ItemExaminedFlags, flag_idx) == 0)
      {
        name = *((s32 *) (((u8 *) (&g_ItemAltNames)) + (flag_idx * 4)));
        goto block_4;
      }
    }
  }
  else
  {
    block_4:
    ;

  }
  return name;
}

void print_formatted_text(s32 x, s32 y, s32 attr, u8 *text) {
    void GsSortFlipSprite();
    u32 scale;
    u8 *p;
    s32 v;
    u8 clut;

    p = text;
    scale = (u32) (attr & 0xFF) >> 4;
    if (scale == 0) {
        scale = 2;
    }
    clut = attr & 0xF;
    LIT(s32, sp_sprite_attribute) = 0x40;
    SCRATCH->spr_w = 8;
    SCRATCH->spr_h = 0xE;
    SCRATCH->spr_clut_x = ((clut & 3) << 4) + 0x100;
    SCRATCH->spr_clut_y = ((u32) clut >> 2) + 0x1E0;
    SCRATCH->spr_r = 0x80;
    SCRATCH->spr_g = 0x80;
    SCRATCH->spr_b = 0x80;
    SCRATCH->spr_pivot_x = 0;
    SCRATCH->spr_pivot_y = 0;
    SCRATCH->spr_rotate = 0;
    SCRATCH->spr_x = x - g_ScreenOffsetX;
    SCRATCH->spr_y = y - g_ScreenOffsetY;
    while (*p != 1) {
        if (*p == 0) {
            goto adv8;
        }
        switch (*p) {
        case 0xFF:
            SCRATCH->spr_x += 4;
            break;
        case 0xF8:
            p++;
            v = (u8) (*p / 18) * 0xE - 0x2E;
            goto setv;
        case 0xF9:
            p++;
            SCRATCH->spr_tpage = 0x1F;
            SCRATCH->spr_v = (u8) (*p / 18) * 0xE;
            goto draw;
        case 0xFA:
            p++;
            SCRATCH->spr_tpage = 0x1F;
            SCRATCH->spr_v = (u8) (*p / 18) * 0xE - 0x3C;
            goto draw;
        default:
            v = (u8) (*p / 18) * 0xE + 0x1C;
        setv:
            SCRATCH->spr_v = v;
            SCRATCH->spr_tpage = 0x1E;
        draw:
            SCRATCH->spr_u = (u8) (*p % 18) * 8;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), scale);
        adv8:
            SCRATCH->spr_x += 8;
            break;
        case 0xFB:
            break;
        case 1:
        case 7:
            return;
        }
        p++;
    }
}

void print_text_8x8(s32 x, s32 y, s32 style, s32 semi_trans) {
    s32 *attr;
    u32 pri;
    u8 col;
    u8 *s;
    u8 c;
    s8 t;
    s32 k;

    pri = (u32)(style & 0xFF) >> 4;
    if (pri == 0) {
        pri = 2;
    }
    col = style & 0xF;
    attr = LITP(s32, sp_sprite);
    k = semi_trans & 0xFF;
    *attr = k ? 0x40000040 : 0x40;
    SCRATCH->spr_w = 8;
    SCRATCH->spr_h = 8;
    SCRATCH->spr_tpage = 0x1E;
    SCRATCH->spr_r = 0x80;
    SCRATCH->spr_g = 0x80;
    SCRATCH->spr_b = 0x80;
    SCRATCH->spr_pivot_x = 0;
    SCRATCH->spr_pivot_y = 0;
    SCRATCH->spr_rotate = 0;
    SCRATCH->spr_x = x - g_ScreenOffsetX;
    SCRATCH->spr_y = y - g_ScreenOffsetY;
    if (semi_trans & 0xFF) {
        col += 8;
    }
    k = col & 3; /* k reused (set twice): no sched1 boost, so the andi is scheduled first */
    SCRATCH->spr_clut_x = k * 0x10 + 0x100;
    SCRATCH->spr_clut_y = (col >> 2) + 0x1E0;
    s = LITP(u8, sp_text);
    do {
        c = *s++;
        if (c != ' ') {
            t = c + 0xE0;
            SCRATCH->spr_u = (t & 0x1F) * 8;
            SCRATCH->spr_v = (t & 0xE0) >> 2;
            GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), pri);
        }
        SCRATCH->spr_x += 8;
    } while (*s != 0);
}

void print_text_8x14(s32 x, s32 y, s32 attr, u8 semi_trans) {
    void GsSortFlipSprite();
    u8 clut;
    u32 scale;
    u8 *p;
    s16 c;

    scale = (u32) (attr & 0xFF) >> 4;
    if (scale == 0) {
        scale = 2;
    }
    clut = attr & 0xF;
    LIT(s32, sp_sprite_attribute) = (semi_trans) ? 0x40000040 : 0x40;
    SCRATCH->spr_w = 8;
    SCRATCH->spr_h = 0xE;
    SCRATCH->spr_tpage = 0x1E;
    SCRATCH->spr_r = 0x80;
    SCRATCH->spr_g = 0x80;
    SCRATCH->spr_b = 0x80;
    SCRATCH->spr_pivot_x = 0;
    SCRATCH->spr_pivot_y = 0;
    SCRATCH->spr_rotate = 0;
    SCRATCH->spr_x = x - g_ScreenOffsetX;
    SCRATCH->spr_y = y - g_ScreenOffsetY;
    if (semi_trans) {
        clut += 8;
    }
    SCRATCH->spr_clut_x = ((clut & 3) << 4) + 0x100;
    SCRATCH->spr_clut_y = ((u32) clut >> 2) + 0x1E0;
    p = LITP(u8, sp_text);
    do {
        c = *p;
        if (c != ' ') {
            SCRATCH->spr_u = (*p % 18) * 8;
            SCRATCH->spr_v = (*p / 18) * 0xE;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, scale);
        }
        p += 1;
        SCRATCH->spr_x = (u16) (SCRATCH_AS(spr_x, s32) + 8);
    } while (*p != 0);
}

s32 set_message_display(u8 msg, s16 msg_id) {
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    u16 old;
    u16 *base;

    if (LIT(u8, g_MessageStatus) & 0x80) {
        return 1;
    }
    old = LIT(u16, g_UpdateFlags);
    LIT(u8, g_MessageStatus) = 0x80;
    g_MessageId = msg_id;
    g_MessageFlagsSaved = old;
    LIT(u16, g_UpdateFlags) = old & ~msg_id;
    if (msg & 0x40) {
        g_MessageText = g_GlobalMessages[msg & 0x3F];
    } else {
        base = ((u16 *) ROOM_P->messages);
        LIT(u16 *, g_MessageText) = base;
        LIT(s32, g_MessageText) = *(u16 *)((u8 *)base + ({ (msg & 0x3F) * 2; })) + (s32)base;
    }
    g_MessageState = 0;
    if (g_GameStateFlags & GAME_MENU_BYTE) {
        g_MessageY = 0xBA - g_ScreenOffsetY;
    } else {
        g_MessageY = 0xB5 - g_ScreenOffsetY;
    }
    g_MessageFastForward = msg & 0x80;
    return 0;
}

s32 set_item_description_message(u16 item_idx, s16 msg_id) {
    extern u8 g_MessageFastForward_u8 __asm__("g_MessageFastForward");
    u16 old;

    if (LIT(u8, g_MessageStatus) & 0x80) {
        return 1;
    }
    old = LIT(s16, g_UpdateFlags);
    LIT(u8, g_MessageStatus) = 0x80;
    g_MessageFastForward_u8 = 0x80;
    g_MessageId = msg_id;
    g_MessageState = 0;
    g_MessageFlagsSaved = old;
    LIT(u16, g_UpdateFlags) = old & ~msg_id;
    g_MessageText = g_ItemDescriptions[item_idx];
    g_MessageY = 0xBA - g_ScreenOffsetY;
    return 0;
}

/* jtbl_800103DC is 4-aligned (tools/jtalign.awk) */
__asm__("#JTALIGN2");
void update_message_display(void) {
    void GsSortFlipSprite();
    extern u8 g_MessageState_l __asm__("g_MessageState");
    extern u8 g_MessageFastForward_l __asm__("g_MessageFastForward");
    extern u8 *g_MessageText_l __asm__("g_MessageText");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    extern u8 g_VSyncMode_l __asm__("g_VSyncMode");
    extern u16 g_MessageY_l __asm__("g_MessageY");
    extern u16 g_ScreenOffsetX_u16 __asm__("g_ScreenOffsetX");
    void handle_message_post_action_l(void) __asm__("handle_message_post_action");
    void message_render_chars(void);
    u8 c;
    s32 v;
    u8 **ppc;
    u8 *p;
    u8 *pspd;
    u8 *pwait;
    u8 *q;

    LITP(GsSPRITE, sp_sprite)->r = 0x80;
    LITP(GsSPRITE, sp_sprite)->g = 0x80;
    LITP(GsSPRITE, sp_sprite)->b = 0x80;
    LITP(GsSPRITE, sp_sprite)->mx = 0;
    LITP(GsSPRITE, sp_sprite)->my = 0;
    LITP(GsSPRITE, sp_sprite)->rotate = 0;
    switch (g_MessageState_l) {
    case 0:
        g_MessageState_l = 1;
        LIT(u8, g_MessageCharDelay) = (LIT(u8, g_VSyncMode) == 0) ? 4 : 2;
        LIT(u8, g_MessageCharTimer) = (LIT(u8, g_VSyncMode) == 0) ? 2 : 1;
        g_MessageColor = 0;
        g_MessageNextColor = 0;
        g_MessageCursor = g_MessageText_l;
    case 1:
        v = LIT(u8, g_MessageCharTimer);
        LIT(u8, g_MessageCharTimer) = v - 1;
        if (g_MessageFastForward_l != 0) {
            if (LIT(u8, g_MessageCharTimer) == 0) {
                goto scan;
            }
            if (g_PadHeld & PAD_CONFIRM) {
                LIT(u8, g_MessageCharTimer) = v - 2;
            }
        }
        if (g_MessageCharTimer != 0) {
            break;
        }
    scan:
        if (*g_MessageCursor != 0) {
            ppc = LITP(u8 *, g_MessageCursor);
            pspd = LITP(u8, g_MessageCharTimer);
            pwait = LITP(u8, g_MessageCharDelay);
            do {
                p = *ppc;
                if ((u8)(*p - 12) < 0xEC) {
                    goto out;
                }
                switch (*p) {
                case 1:
                    *ppc = p + 1;
                    if (p[1] != 0) {
                        u8 sp;

                        g_MessageState_l = 6;
                        sp = p[1];
                        if (g_VSyncMode_l == 0) {
                            sp <<= 1;
                        }
                        *pspd = sp;
                    } else {
                        g_MessageState_l = 5;
                    }
                    goto end;
                case 3:
                    *ppc = p + 1;
                    if (p[1] != 0) {
                        u8 sp;

                        g_MessageState_l = 3;
                        sp = p[1];
                        if (g_VSyncMode_l == 0) {
                            sp <<= 1;
                        }
                        *pspd = sp;
                    } else {
                        g_MessageState_l = 2;
                    }
                    (*ppc)++;
                    goto end;
                case 8:
                    g_MessageState_l = 4;
                    goto end;
                case 4:
                    *ppc = p + 1;
                    if (p[1] == 0) {
                        *ppc = p + 2;
                        if (p[2] != 4) {
                            s32 k = 4;

                            do {
                                switch (**ppc) {
                                case 5:
                                case 6:
                                case 0xF8:
                                case 0xF9:
                                case 0xFA:
                                    (*ppc)++;
                                    break;
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                    break;
                                case 8:
                                case 9:
                                case 10:
                                    break;
                                }
                                q = *ppc;
                                *ppc = q + 1;
                            } while (q[1] != k);
                        }
                        (*ppc)++;
                    }
                    {
                        u8 d;
                        u8 *t;

                        t = *ppc;
                        d = *t;
                        if (g_VSyncMode_l == 0) {
                            d <<= 1;
                        }
                        *pwait = d;
                        *ppc = t + 1;
                    }
                    break;
                case 0xF8:
                case 0xF9:
                case 0xFA:
                    (*ppc)++;
                    goto out;
                case 5:
                    *ppc = p + 1;
                    g_MessageNextColor = p[1];
                case 2:
                    (*ppc)++;
                    break;
                case 6: {
                    u8 w;

                    g_MessageReturnPtr = p;
                    *ppc = p + 1;
                    if (p[1] != 0) {
                        w = p[1];
                    } else {
                        w = g_SelectedItemId;
                    }
                    g_MessageCursor = (u8 *)message_item_name_lookup(w);
                    break;
                }
                case 7:
                    *ppc = g_MessageReturnPtr + 2;
                    break;
                }
            } while (*g_MessageCursor != 0);
        }
    out:
        LIT(u8 *, g_MessageCursor)++;
        g_MessageCharTimer = g_MessageCharDelay;
        break;
    case 2:
        if (g_PadPressed & (PAD_CONFIRM | PAD_CANCEL)) {
            u8 *len = LITP(u8, g_MessageCharTimer);

            g_MessageState_l = 1;
            g_MessageText_l = g_MessageCursor;
            *len = (g_VSyncMode_l == 0) ? 2 : 1;
            g_MessageColor = g_MessageNextColor;
            break;
        }
        {
            s32 t;

            LIT(u8, g_MessageCharTimer)--;
            t = LIT(u8, g_MessageCharTimer);
            if (g_VSyncMode_l == 0) {
                if (t & 0x30) {
                    goto draw2;
                }
                break;
            }
            if (!(t & 0x18)) {
                break;
            }
        }
    draw2:
        LITP(GsSPRITE, sp_sprite)->attribute = 0x40;
        LITP(GsSPRITE, sp_sprite)->w = 8;
        LITP(GsSPRITE, sp_sprite)->h = 0xE;
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1E;
        LITP(GsSPRITE, sp_sprite)->u = 0x58;
        LITP(GsSPRITE, sp_sprite)->v = 0x1C;
        LITP(GsSPRITE, sp_sprite)->cx = 0x100;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E0;
        LITP(GsSPRITE, sp_sprite)->x = 0x99 - g_ScreenOffsetX_u16;
        LITP(GsSPRITE, sp_sprite)->y = g_MessageY_l + 0x1E;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[g_ActiveBuffer], 2);
        break;
    case 3: {
        u8 *len = LITP(u8, g_MessageCharTimer);

        (*len)--;
        if (*len != 0) {
            break;
        }
        g_MessageState_l = 1;
        c = g_MessageCharDelay;
        g_MessageText_l = g_MessageCursor;
        if (g_VSyncMode_l == 0) {
            c <<= 1;
        }
        *len = c;
        g_MessageColor = g_MessageNextColor;
        break;
    }
    case 4:
        if (g_PadPressed & PAD_CONFIRM) {
            LIT(u8, g_MessageStatus) &= 0x7F;
            g_UpdateFlags = g_MessageFlagsSaved;
            handle_message_post_action_l();
            return;
        }
        if (g_PadTrigger & (PSX_RIGHT | PSX_LEFT)) {
            g_MessageCharTimer = 0;
            LIT(u8, g_MessageStatus) ^= 1;
        }
        {
            s32 t;

            LIT(u8, g_MessageCharTimer)--;
            t = LIT(u8, g_MessageCharTimer);
            if (g_VSyncMode_l == 0) {
                if (t & 0x30) {
                    goto draw4;
                }
                goto text;
            }
            if (!(t & 0x18)) {
                goto text;
            }
        }
    draw4:
        LITP(GsSPRITE, sp_sprite)->attribute = 0x40;
        if (g_MessageStatus_l & 1) {
            LITP(GsSPRITE, sp_sprite)->x = 0xF8 - g_ScreenOffsetX_u16;
        } else {
            LITP(GsSPRITE, sp_sprite)->x = 0xD0 - g_ScreenOffsetX_u16;
        }
        LITP(GsSPRITE, sp_sprite)->w = 8;
        LITP(GsSPRITE, sp_sprite)->h = 0xE;
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1E;
        LITP(GsSPRITE, sp_sprite)->u = 0x10;
        LITP(GsSPRITE, sp_sprite)->v = 0x1C;
        LITP(GsSPRITE, sp_sprite)->cx = 0x100;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E0;
        LITP(GsSPRITE, sp_sprite)->y = g_MessageY_l + 0x10;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[g_ActiveBuffer], 2);
    text:
        sprintf(LITP(char, sp_text), "Yes  No");
        print_text_8x14(0xD8, (u16)(g_MessageY_l + ({ g_ScreenOffsetY + 0x10; })), 0, 0);
        break;
    case 5:
        if (!(g_PadPressed & (PAD_CONFIRM | PAD_CANCEL))) {
            break;
        }
        LIT(u8, g_MessageStatus) &= 0x7F;
        if (!(LIT(u16, g_UpdateFlags) & 1)) {
            LIT(u16, g_PadHeld) &= 0xF000;
            LIT(u16, g_PadHeldPrev) &= 0xF000;
        }
        LIT(u16, g_UpdateFlags) = g_MessageFlagsSaved;
        return;
    case 6:
        LIT(u8, g_MessageCharTimer)--;
        if (LIT(u8, g_MessageCharTimer) != 0) {
            break;
        }
        LIT(u8, g_MessageStatus) &= 0x7F;
        if (!(LIT(u16, g_UpdateFlags) & 1)) {
            LIT(u16, g_PadHeld) &= 0xF000;
            LIT(u16, g_PadHeldPrev) &= 0xF000;
        }
        LIT(u16, g_UpdateFlags) = g_MessageFlagsSaved;
        return;
    }
end:
    message_render_chars();
}

void handle_message_post_action() {
    s32 set_message_display_a4() __asm__("set_message_display");
    extern s32 (*g_MessageActionFuncs[])();
    u8 action;
    void *next;
    void *branch;
    void *cur;

    cur = LIT(void *, g_MessageCursor);
    next = cur + 1;
    LIT(void *, g_MessageCursor) = next;
    branch = (void *)((*(u8 *)((u8 *)cur + 1) * (g_MessageStatus & 1)) + (s32)next);
    LIT(void *, g_MessageCursor) = branch + 1;
    action = *(u8 *)((u8 *)branch + 1);
    LIT(void *, g_MessageCursor) = branch + 2;
    switch (action) {
    case 8:
        break;
    case 9:
        set_message_display_a4(*(u8 *)((u8 *)branch + 2), g_MessageId);
        return;
    case 10:
        g_MessageActionFuncs[*(u8 *)((u8 *)branch + 2)]();
        return;
    }
}

void message_render_chars(void) {
    void GsSortFlipSprite();
    u8 *message_item_name_lookup_l(s32) __asm__("message_item_name_lookup");
    u8 *p;
    u8 *save;
    s32 v;
    u8 c;

    SCRATCH->spr_flags = 0x40;
    SCRATCH->spr_w = 8;
    SCRATCH->spr_h = 14;
    p = (u8 *)g_MessageText;
    SCRATCH->spr_x = 0x30 - g_ScreenOffsetX;
    SCRATCH->spr_y = g_MessageY;
    SCRATCH->spr_clut_x = ((LIT(u8, g_MessageColor) & 3) << 4) + 0x100;
    SCRATCH->spr_clut_y = (LIT(u8, g_MessageColor) >> 2) + 0x1E0;
    while (p != g_MessageCursor) {
        if (*p != 0) {
        switch (*p) {
        case 3:
        case 4:
            p++;
        case 1:
            p++;
            continue;
        case 5:
            p++;
            SCRATCH->spr_clut_x = ((*p & 3) << 4) + 0x100;
            SCRATCH->spr_clut_y = (*p >> 2) + 0x1E0;
            p++;
            continue;
        case 2:
            SCRATCH->spr_x = 0x30 - g_ScreenOffsetX;
            SCRATCH->spr_y += 0x10;
            p++;
            continue;
        case 0xF8:
            p++;
            v = (u8)(*p / 18) * 14 - 0x2E;
            goto setv;
        case 0xF9:
            p++;
            SCRATCH->spr_tpage = 0x1F;
            SCRATCH->spr_v = (u8)(*p / 18) * 14;
            goto draw;
        case 0xFA:
            p++;
            SCRATCH->spr_tpage = 0x1F;
            SCRATCH->spr_v = (u8)(*p / 18) * 14 - 0x3C;
            goto draw;
        case 6:
            save = p;
            p++;
            if (*p != 0) {
                c = *p;
            } else {
                c = g_SelectedItemId;
            }
            p = message_item_name_lookup_l(c);
            continue;
        case 7:
            p = save + 2;
            continue;
        default:
            v = (u8)(*p / 18) * 14 + 0x1C;
        setv:
            SCRATCH->spr_v = v;
            SCRATCH->spr_tpage = 0x1E;
        draw:
            SCRATCH->spr_u = (u8)(*p % 18) * 8;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
        }
        }
        SCRATCH->spr_x += 8;
        p++;
    }
}

void message_unused_stub(void) {
}
