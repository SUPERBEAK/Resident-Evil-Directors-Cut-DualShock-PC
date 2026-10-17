/* keyconfig.c - the controller key configuration screen (options menu) */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 room_bg_verify_checksum(void);

s32 keyconfig_update(void) {
    void keyconfig_draw_frame(void);
    void keyconfig_draw_layout(void);
    u8 keyconfig_input(void);
    u8 *state = &g_KeyConfigState;

    LIT(s32, sp_sprite_attribute) = 0x01000040;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(s16, sp_sprite_cy) = 0x1FF;
    LIT(s16, sp_sprite_tpage) = 0x15;
    LIT(s8, sp_sprite_r) = 0;
    LIT(s8, sp_sprite_g) = 0;
    LIT(s8, sp_sprite_b) = 0;
    LIT(s16, sp_sprite_mx) = 0;
    LIT(s16, sp_sprite_my) = 0;
    LIT(s32, sp_sprite_rotate) = 0;
    switch (*state) {
    case 0:
        *state = 1;
        g_KeyConfigWork = 0;
        g_OptionsRepeatPrevPad = 0;
        g_OptionsMenuSel = g_KeyConfigType & 3;
        /* fallthrough */
    case 1:
        if (!(g_PadHeldRaw & PSX_SELECT) && (g_PadTrigger & PSX_START)) {
            return 1;
        }
        if (!keyconfig_input()) {
            return 1;
        }
        break;
    }
    keyconfig_draw_frame();
    keyconfig_draw_layout();
    return 0;
}

void keyconfig_draw_frame(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern u8 g_KeyConfigWork_u8 __asm__("g_KeyConfigWork");
    u8 i;
    u8 *p;

    switch (g_KeyConfigWork_u8) {
    case 1:
    case 2:
    case 3:
        if (g_KeyConfigCursor < 12) {
            LITP(GsSPRITE, sp_sprite)->attribute = 0x41000040;
            LITP(GsSPRITE, sp_sprite)->x = g_KeyCfgSlotX[g_KeyConfigCursor];
            LITP(GsSPRITE, sp_sprite)->y = g_KeyCfgSlotY[g_KeyConfigCursor >> 1];
            LITP(GsSPRITE, sp_sprite)->w = 0x46;
            LITP(GsSPRITE, sp_sprite)->h = 0xE;
            LITP(GsSPRITE, sp_sprite)->u = 0;
            LITP(GsSPRITE, sp_sprite)->v = 0x20;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 4);
        } else {
            LITP(GsBOXF, sp_box)->attribute = 0x40000000;
            if (g_KeyConfigCursor & 1) {
                LITP(GsBOXF, sp_box)->x = 0x79;
            } else {
                LITP(GsBOXF, sp_box)->x = -0x1B;
            }
            LITP(GsBOXF, sp_box)->w = 0xE;
            LITP(GsBOXF, sp_box)->h = 0xE;
            LITP(GsBOXF, sp_box)->r = 0x98;
            LITP(GsBOXF, sp_box)->g = 0x98;
            LITP(GsBOXF, sp_box)->b = 0x20;
            LITP(GsBOXF, sp_box)->y = ((LIT(u8, g_KeyConfigCursor) - 12) / 2) * 16 + 0x27;
            GsSortBoxFill(LITP(GsBOXF, sp_box), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 4);
        }
    case 0:
        p = &g_OptionsMenuSel;
        LIT(u32, sp_box_attribute) = 0x50000000;
        LITP(GsBOXF, sp_box)->x = g_KeyCfgTypeBoxX[*p];
        LITP(GsBOXF, sp_box)->y = -0x5F;
        if (*p < 3) {
            LITP(GsBOXF, sp_box)->w = 0xE;
        } else {
            LITP(GsBOXF, sp_box)->w = 0x2E;
        }
        LITP(GsBOXF, sp_box)->h = 0xE;
        LITP(GsBOXF, sp_box)->r = 0;
        LITP(GsBOXF, sp_box)->g = 0;
        LITP(GsBOXF, sp_box)->b = 0x80;
        GsSortBoxFill(LITP(GsBOXF, sp_box), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
        LIT(u32, sp_box_attribute) = 0x60000000;
        LITP(GsBOXF, sp_box)->w = 0x2E;
        LITP(GsBOXF, sp_box)->r = 0x30;
        LITP(GsBOXF, sp_box)->g = 0x30;
        LITP(GsBOXF, sp_box)->b = 0x30;
        if (g_OptionsMenuSel != 4) {
            LITP(GsBOXF, sp_box)->x = 0x5C;
            GsSortBoxFill(LITP(GsBOXF, sp_box), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
        }
        break;
    }
    i = 4;
    do {
        i--;
        if (i != (g_KeyConfigType & 3)) {
            LITP(GsBOXF, sp_box)->x = g_KeyCfgTypeBoxX[i];
            GsSortBoxFill(LITP(GsBOXF, sp_box), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
        }
        if (i == 3) {
            LITP(GsBOXF, sp_box)->w = 0xE;
        }
    } while (i != 0);
    LITP(GsSPRITE, sp_sprite)->attribute = 0x1000040;
    LITP(GsSPRITE, sp_sprite)->x = -9;
    LITP(GsSPRITE, sp_sprite)->y = -0x60;
    LITP(GsSPRITE, sp_sprite)->w = 0x32;
    LITP(GsSPRITE, sp_sprite)->h = 0x10;
    LITP(GsSPRITE, sp_sprite)->u = 0;
    LITP(GsSPRITE, sp_sprite)->v = 0xC8;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 4);
    LITP(GsSPRITE, sp_sprite)->x = 0x2A;
    LITP(GsSPRITE, sp_sprite)->w = 0x30;
    LITP(GsSPRITE, sp_sprite)->v = 0xD8;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 4);
    LITP(GsSPRITE, sp_sprite)->x = -0xC;
    LITP(GsSPRITE, sp_sprite)->y = -0x6A;
    LITP(GsSPRITE, sp_sprite)->w = 0x20;
    LITP(GsSPRITE, sp_sprite)->h = 8;
    LITP(GsSPRITE, sp_sprite)->v = 0xC0;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 4);
}

void keyconfig_draw_layout(void) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern u8 g_KeyCfgActionListA_l[] __asm__("g_KeyCfgActionListA");
    extern u8 g_KeyCfgActionListB_l[] __asm__("g_KeyCfgActionListB");
    extern u8 *g_KeyCfgActionNamesA_l[] __asm__("g_KeyCfgActionNamesA");
    extern u8 *g_KeyCfgActionNamesB_l[] __asm__("g_KeyCfgActionNamesB");
    extern u8 g_KeyCfgActionNameXA_l[] __asm__("g_KeyCfgActionNameXA");
    extern u8 g_KeyCfgActionNameXB_l[] __asm__("g_KeyCfgActionNameXB");
    void print_formatted_text_l(u16, u16, u8, u8 *) __asm__("print_formatted_text");
    u8 i;
    u8 n;
    u16 bits;
    u16 mask;
    u16 *tbl;
    u8 *sel;
    u16 x;
    u16 y;

    i = 12;
    do {
        i--;
        LITP(GsSPRITE, sp_sprite)->attribute = 0x01000040;
        LITP(GsSPRITE, sp_sprite)->x = g_KeyCfgSlotX[i] + 0x1C;
        LITP(GsSPRITE, sp_sprite)->y = g_KeyCfgSlotY[i >> 1];
        LITP(GsSPRITE, sp_sprite)->w = 0xE;
        LITP(GsSPRITE, sp_sprite)->h = 0xE;
        LITP(GsSPRITE, sp_sprite)->u = (i & 0x7E) * 8;
        LITP(GsSPRITE, sp_sprite)->v = (i & 1) * 0x10;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
    } while (i != 0);
    LITP(GsSPRITE, sp_sprite)->attribute = 0x40;
    LITP(GsSPRITE, sp_sprite)->w = 8;
    LITP(GsSPRITE, sp_sprite)->cx = 0x100;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1E0;
    LITP(GsSPRITE, sp_sprite)->h = 0xE;
    LITP(GsSPRITE, sp_sprite)->tpage = 0x1F;
    tbl = g_KeyConfigPresets[g_KeyConfigType & 3];
    i = 8;
    do {
        i--;
        print_formatted_text_l(g_KeyCfgViewerLabelX[i], y = (i >> 1) * 16 + 0xA0, 0x30, g_KeyCfgViewerLabels[i]);
        bits = tbl[g_KeyCfgActionSlots[i]];
        if (bits != 0) {
            LITP(GsSPRITE, sp_sprite)->y--;
            if (bits & 0xF000) {
                LITP(GsSPRITE, sp_sprite)->u = 0x70;
                mask = 0x1000;
            } else {
                mask = 1;
                LITP(GsSPRITE, sp_sprite)->u = 0;
            }
            LITP(GsSPRITE, sp_sprite)->v = 0x8C;
            LITP(GsSPRITE, sp_sprite)->w = 0xE;
            for (n = 0; !(bits & (mask << n)); n++) {
            }
            LITP(GsSPRITE, sp_sprite)->x = (i & 1) ? 0x79 : -0x1B;
            LITP(GsSPRITE, sp_sprite)->cx = 0x100;
            LITP(GsSPRITE, sp_sprite)->cy = 0x1E0;
            LITP(GsSPRITE, sp_sprite)->tpage = 0x1E;
            LITP(GsSPRITE, sp_sprite)->u += n * 14;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
        }
    } while (i != 0);
    sel = g_KeyCfgActionListA_l;
    if ((g_KeyConfigType & 0x40) && (g_KeyConfigType & 3) == 3) {
        sel = g_KeyCfgActionListB_l;
    }
    i = 12;
loop3:
    i--;
    bits = g_KeyCfgSlotBits[i];
    n = 7;
    if ((g_KeyConfigType & 0x40) && (g_KeyConfigType & 3) == 3) {
        n = 8;
    }
    while (n != 0 && !(bits & tbl[sel[n - 1] & 0xF])) {
        n--;
    }
    x = g_KeyCfgSlotX[i] + 0xA0;
    y = g_KeyCfgSlotY[i >> 1] + 0x79;
    mask = 0x33;
    if (n != 0) {
        mask = 0x30;
    }
    if ((g_KeyConfigType & 0x40) && (g_KeyConfigType & 3) == 3) {
        x += g_KeyCfgActionNameXB_l[n];
        print_formatted_text_l(x, y, mask, g_KeyCfgActionNamesB_l[n]);
    } else {
        x += g_KeyCfgActionNameXA_l[n];
        print_formatted_text_l(x, y, mask, g_KeyCfgActionNamesA_l[n]);
    }
    if (i != 0) {
        goto loop3;
    }
}


typedef struct {
    u8 state;
    u8 preset;
    u8 slot_cursor;
    u8 list_cursor;
    u8 pad4[3];
    u8 rep;
    u16 prev;
} KeyConfigWork;

u8 keyconfig_input(void) {
    extern KeyConfigWork g_KeyConfigWork_l __asm__("g_KeyConfigWork");
    extern u8 g_KeyCfgActionListA_l[] __asm__("g_KeyCfgActionListA");
    extern u8 g_KeyCfgActionListB_l[] __asm__("g_KeyCfgActionListB");
    void play_sfx_l() __asm__("play_sfx");
    s32 pad_get_repeat(u8 *, u16 *, u8, s32);
    u8 keyconfig_button_to_index(u16);
    void keyconfig_draw_action_list(void);
    void keyconfig_draw_button_list(void);
    void pad_set_default_bindings(void);
    void pad_set_default_bindings_b(void);
    s32 r;
    u8 k;
    u8 n;
    u8 *tbl;
    u8 i;
    u8 v;
    u8 b;
    u16 val;

    switch (g_KeyConfigWork_l.state) {
    case 0:
        if (g_PadTrigger & PSX_TRIANGLE) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            return 0;
        }
        if (g_PadTrigger & PSX_CROSS) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            if (g_KeyConfigWork_l.preset == 4) {
                return 0;
            }
            if (g_KeyConfigWork_l.preset == 3 && (g_KeyConfigType & 3) == g_KeyConfigWork_l.preset) {
                g_KeyConfigWork_l.state = 1;
                g_KeyConfigWork_l.slot_cursor = 0;
                g_KeyConfigWork_l.prev = 0;
            }
            LIT(u8, g_KeyConfigType) &= 0xFC;
            LIT(u8, g_KeyConfigType) |= g_KeyConfigWork_l.preset;
            break;
        }
        r = pad_get_repeat(&g_KeyConfigWork_l.rep, &g_KeyConfigWork_l.prev, 0xE6, 0xA000);
        if (r & 0xA000) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            if (r & 0x8000) {
                if (g_KeyConfigWork_l.preset != 0) {
                    g_KeyConfigWork_l.preset--;
                    break;
                }
                g_KeyConfigWork_l.preset = 4;
                break;
            }
            if (g_KeyConfigWork_l.preset == 4) {
                g_KeyConfigWork_l.preset = 0;
                break;
            }
            g_KeyConfigWork_l.preset++;
            break;
        }
        break;
    case 1:
        if (g_PadTrigger & PSX_TRIANGLE) {
            g_KeyConfigWork_l.state = 0;
            g_KeyConfigWork_l.prev = 0;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            break;
        }
        if (g_PadTrigger & PSX_SELECT) {
            if ((LIT(volatile u16, g_PadHeldRaw) & (PSX_R2 | PSX_R1)) == 0xA && !(LIT(volatile u16, g_PadHeldRaw) & ~(PSX_R2 | PSX_R1 | PSX_SELECT))) {
                LIT(u8, g_KeyConfigType) ^= 0x40;
            }
            if (g_KeyConfigType & 0x40) {
                pad_set_default_bindings_b();
                break;
            }
            pad_set_default_bindings();
            break;
        }
        if (g_PadTrigger & PSX_CROSS) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            g_KeyConfigWork_l.list_cursor = 0;
            g_KeyConfigWork_l.prev = 0;
            g_KeyConfigWork_l.state = ({ g_KeyConfigWork_l.slot_cursor >= 0xC; }) + 2;
            break;
        }
        r = pad_get_repeat(&g_KeyConfigWork_l.rep, &g_KeyConfigWork_l.prev, 0xE6, 0xF000);
        if ((u16)r == 0) {
            break;
        }
        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
        if ((u32)(g_KeyConfigWork_l.slot_cursor - 4) >= 8) {
            if (r & 0xA000) {
                g_KeyConfigWork_l.slot_cursor ^= 1;
            }
            if (!(r & 0x5000)) {
                break;
            }
            if (r & 0x1000) {
                if (g_KeyConfigWork_l.slot_cursor & 0xFE) {
                    g_KeyConfigWork_l.slot_cursor -= 2;
                    break;
                }
                g_KeyConfigWork_l.slot_cursor += 0x12;
                break;
            }
            g_KeyConfigWork_l.slot_cursor += 2;
            if (g_KeyConfigWork_l.slot_cursor >= 0x14) {
                g_KeyConfigWork_l.slot_cursor &= 1;
                break;
            }
            break;
        }
        k = keyconfig_button_to_index((u16)r);
        if (k == 0) {
            break;
        }
        k--;
        v = g_KeyConfigWork_l.slot_cursor - 4;
        g_KeyConfigWork_l.slot_cursor = g_KeyCfgCursorMove[v][k];
        break;
    case 2:
        n = 8;
        if (g_KeyConfigType & 0x40) {
            tbl = g_KeyCfgActionListB_l;
        } else {
            n = 7;
            tbl = g_KeyCfgActionListA_l;
        }
        if (g_PadTrigger & (PSX_TRIANGLE | PSX_CROSS)) {
            if (g_PadTrigger & PSX_CROSS) {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                i = n;
                r = g_KeyCfgSlotBits[g_KeyConfigWork_l.slot_cursor];
                b = tbl[g_KeyConfigWork_l.list_cursor];
                do {
                    i--;
                    v = tbl[i];
                    if ((r & 0xFFFF) == LITP(u16, g_KeyConfig)[v & 0xF]) {
                        val = 0;
                        if (b != 0xFE) {
                            val = LITP(u16, g_KeyConfig)[b & 0xF];
                        }
                        do {
                            LITP(u16, g_KeyConfig)[v & 0xF] = val;
                            v >>= 4;
                        } while (v != 0);
                        i = 0;
                    }
                } while (i != 0);
                if (b != 0xFE) {
                    i = b;
                    do {
                        LITP(u16, g_KeyConfig)[i & 0xF] = r;
                        i >>= 4;
                    } while (i != 0);
                }
            } else {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
            g_KeyConfigWork_l.state = 1;
            g_KeyConfigWork_l.prev = 0;
        } else {
            r = pad_get_repeat(&g_KeyConfigWork_l.rep, &g_KeyConfigWork_l.prev, 0xE6, 0x5000);
            if (r & 0x5000) {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                if (r & 0x1000) {
                    if (g_KeyConfigWork_l.list_cursor != 0) {
                        g_KeyConfigWork_l.list_cursor--;
                    } else {
                        g_KeyConfigWork_l.list_cursor = n;
                    }
                } else if (g_KeyConfigWork_l.list_cursor != n) {
                    g_KeyConfigWork_l.list_cursor++;
                } else {
                    g_KeyConfigWork_l.list_cursor = 0;
                }
            }
        }
        keyconfig_draw_action_list();
        break;
    case 3:
        if (g_PadTrigger & (PSX_TRIANGLE | PSX_CROSS)) {
            if (g_PadTrigger & PSX_CROSS) {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                i = 0xC;
                r = g_KeyCfgButtonOrder[g_KeyConfigWork_l.list_cursor];
                b = g_KeyCfgActionListB_l[g_KeyConfigWork_l.slot_cursor];
                do {
                    i--;
                    if ((r & 0xFFFF) == LITP(u16, g_KeyConfig)[g_KeyCfgActionSlots[i]]) {
                        LITP(u16, g_KeyConfig)[g_KeyCfgActionSlots[i]] = LITP(u16, g_KeyConfig)[b];
                        i = 0;
                    }
                } while (i != 0);
                LITP(u16, g_KeyConfig)[b] = r;
            } else {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
            g_KeyConfigWork_l.state = 1;
            g_KeyConfigWork_l.prev = 0;
        } else {
            r = pad_get_repeat(&g_KeyConfigWork_l.rep, &g_KeyConfigWork_l.prev, 0xE6, 0xF000);
            if ((u16)r != 0) {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                if (r & 0xA000) {
                    g_KeyConfigWork_l.list_cursor ^= 1;
                }
                if (r & 0x5000) {
                    if (r & 0x1000) {
                        if (!(g_KeyConfigWork_l.list_cursor & 0xFE)) {
                            g_KeyConfigWork_l.list_cursor = (g_KeyConfigWork_l.list_cursor & 1) + 0xC;
                        }
                        g_KeyConfigWork_l.list_cursor -= 2;
                    } else {
                        g_KeyConfigWork_l.list_cursor += 2;
                        if (g_KeyConfigWork_l.list_cursor >= 0xC) {
                            g_KeyConfigWork_l.list_cursor &= 1;
                        }
                    }
                }
            }
        }
        keyconfig_draw_button_list();
        break;
    }
    return 1;
}

void keyconfig_draw_action_list(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s8 *actions;
    s32 color;
    s32 *names;
    s32 sel_y;
    s32 y_cur;
    s32 idx;
    s32 i;
    s32 y;
    s32 n;
    s32 x;
    u8 *name_x;
    s32 v;

    if (g_KeyConfigType & 0x40) {
        actions = (s8 *)&g_KeyCfgActionListB;
        names = &g_KeyCfgActionNamesB;
        name_x = &g_KeyCfgActionNameXB;
        n = 8;
    } else {
        actions = (s8 *)&g_KeyCfgActionListA;
        names = &g_KeyCfgActionNamesA;
        name_x = &g_KeyCfgActionNameXA;
        n = 7;
    }
    x = 0xC1;
    if (g_KeyConfigCursor & 1) {
        x = 0x39;
    }
    y = 0x4D;
    if (g_KeyConfigType & 0x40) {
        y = 0x47;
    }
    i = 0;
    do {
        n -= 1;
        color = 0x30;
        if (*(u16 *)(LITP(u8, g_KeyConfig) + (((actions[i & 0xFF] & 0xF) * 2))) != 0) {
            color = 0x33;
        }
        i += 1;
        y_cur = y & 0xFFFF;
        y += 0x10;
        idx = i & 0xFF;
        print_formatted_text(x + name_x[idx], y_cur, color, names[idx]);
    } while (n & 0xFF);
    print_formatted_text(x + *name_x, y & 0xFFFF, 0x33, *names);
    SCRATCH->rect_flags = 0x50000000;
    SCRATCH->rect_x = x - 0xA0;
    sel_y = g_KeyConfigListSel * 0x10;
    SCRATCH->rect_y = sel_y - 0x2C;
    if (LIT(u8, g_KeyConfigType) & 0x40) {
        SCRATCH->rect_y = (s16) (sel_y - 0x32);
    }
    SCRATCH->rect_w = 0x46;
    SCRATCH->rect_h = 0xE;
    SCRATCH->rect_r = 0;
    SCRATCH->rect_g = 0;
    SCRATCH->rect_b = 0x80;
    GsSortBoxFill(LITP(GsBOXF, sp_box), (GsOT *)((LIT(u8, g_ActiveBuffer) * 0x14) + ADDR_g_OrderingTables), 3U);
    SCRATCH->spr_flags = 0x01000040;
    SCRATCH->spr_clut_y = 0x1FF;
    SCRATCH->spr_tpage = 0x15;
    SCRATCH->spr_x = x - 0xAD;
    SCRATCH->spr_y = 3;
    SCRATCH->spr_v = 0x63;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_u = 0;
    SCRATCH->spr_w = 0x60;
    SCRATCH->spr_h = 0x5D;
    if (LIT(u8, g_KeyConfigType) & 0x40) {
        SCRATCH->spr_y = -3;
        v = 0x153; /* stored as a byte (0x53): the original loads 339, so not a folded constant */
        SCRATCH->spr_v = v;
        SCRATCH->spr_h = 0x6D;
    }
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((LIT(u8, g_ActiveBuffer) * 0x14) + ADDR_g_OrderingTables), 3U);
    SCRATCH->spr_v = 0x30;
    SCRATCH->spr_h = 0x43;
    SCRATCH->spr_y = (u16) (SCRATCH->spr_y - 0x43);
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((LIT(u8, g_ActiveBuffer) * 0x14) + ADDR_g_OrderingTables), 3U);
}

void keyconfig_draw_button_list(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 x;
    u8 i;
    u8 j;
    u16 code;
    u16 mask;
    s32 flag;
    u8 *p;

    x = 0x21;
    if (g_KeyConfigCursor & 1) {
        x = -0x3F;
    }
    i = 12;
    SCRATCH->spr_flags = 0x40;
    SCRATCH->spr_w = 0xE;
    SCRATCH->spr_h = 0xE;
    SCRATCH->spr_clut_y = 0x1E0;
    SCRATCH->spr_tpage = 0x1E;
    do {
        i--;
        SCRATCH->spr_x = x + (i & 1) * 16;
        SCRATCH->spr_y = (i >> 1) * 16 - 2;
        code = g_KeyCfgButtonOrder[i];
        if (code & 0xF000) {
            SCRATCH->spr_u = 0x70;
            mask = 0x1000;
        } else {
            mask = 1;
            SCRATCH->spr_u = 0;
        }
        SCRATCH->spr_v = 0x8C;
        j = 0;
        while (!(code & (mask << j))) {
            j++;
        }
        flag = 0;
        SCRATCH->spr_u += j * 14;
        j = 8;
        do {
            j--;
            if (*(u16 *)(LITP(u8, g_KeyConfig) + g_KeyCfgActionSlots[j] * 2) == code) {
                flag = 0x30;
            }
        } while (j != 0 && flag == 0);
        SCRATCH->spr_clut_x = flag | 0x100;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 3);
    } while (i != 0);
    SCRATCH->rect_flags = 0x50000000;
    SCRATCH->rect_w = 0xE;
    SCRATCH->rect_h = 0xE;
    SCRATCH->rect_r = 0;
    SCRATCH->rect_g = 0;
    SCRATCH->rect_b = 0x80;
    p = &g_KeyConfigListSel;
    SCRATCH->rect_x = x + (*p & 1) * 16;
    SCRATCH->rect_y = (*p >> 1) * 16 - 2;
    GsSortBoxFill(LITP(GsBOXF, sp_box), (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OrderingTables), 3);
    SCRATCH->spr_flags = 0x01000040;
    SCRATCH->spr_clut_y = 0x1FF;
    SCRATCH->spr_tpage = 0x15;
    SCRATCH->spr_x = x - 0xD;
    SCRATCH->spr_y = -0x16;
    SCRATCH->spr_u = 0x60;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_v = 0;
    SCRATCH->spr_w = 0x20;
    SCRATCH->spr_h = 0x80;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OrderingTables), 3);
    SCRATCH->spr_v = 0x80;
    SCRATCH->spr_w = 0x18;
    SCRATCH->spr_x += 0x20;
    GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OrderingTables), 3);
}

u8 keyconfig_button_to_index(u16 code) {
    u8 i;

    i = 8;
    do {
        i -= 1;
        if ((code >> 8) == g_KeyCfgButtonCodes[i]) {
            return i + 1;
        }
    } while (i != 0);
    return 0;
}

s32 pad_get_repeat(u8 *timer, u16 *prev_held, u8 rates, s32 mask) {
    s32 trig;
    u16 res;

    trig = mask & g_PadTrigger;
    res = trig;
    if (mask & (LIT(u16, g_PadHeldRaw) & *prev_held)) {
        (*timer)--;
        if (*timer == 0) {
            res = trig | (mask & LIT(u16, g_PadHeldRaw));
            *timer = rates & 0xF;
        }
    } else {
        *timer = rates >> 4;
    }
    *prev_held = g_PadHeldRaw;
    return res;
}
