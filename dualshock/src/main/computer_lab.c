/* computer_lab.c - The computer-lab terminal's drawing and animation: the password typer, keyboard, windows,
   LEDs, the camera / fade steps and the logo animation (its state machine: computer_lab_terminal.c). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void voice_command(s32 cmd, s32 clip);
void check_camera_switch(s32 refresh);
void display_room_camera_bg(void);
void print_text_8x8(s32 x, s32 y, s32 style, s32 semi_trans);
s32 set_message_display(u8 msg, s16 msg_id);
void Flg_off(s32 flags, u32 bit);


void computer_typer_update(void) {
    extern s32 (*computer_typer_step_tbl[])();
    u8 *typer = &g_ClTyper;
    g_ClTyperPtr = &(*typer);
    if ((*typer) != 0) {
        computer_typer_step_tbl[g_ClTyperState]();
    }
}

void computer_typer_begin(void) {
    CL_TYPER->state = (u8) (CL_TYPER->state + 1);
    CL_TYPER->char_idx = 0;
}

void computer_typer_press(void) {
    extern u8 g_ClText0Buf_u8[] __asm__("g_ClText0Buf");
    extern u8 * g_ClTyperPtr_u8p __asm__("g_ClTyperPtr");
    int rand(void);
    u8 v;
    u8 a;
    u8 b;

    CL_TYPER_B->state++;
    v = ({ CL_TYPER_B->line * 18 + g_ClText0Buf_u8; })[CL_TYPER_B->char_idx];
    if (v == 0) {
        CL_TYPER_B->state = 3;
        g_EnemySlot0_BehaviorFlags = 0x46;
        return;
    }
    a = g_ClArmForKey[v];
    if (a & 0x80) {
        g_EnemySlot1_BehaviorFlags = (a & 0x7F) | 0x40;
    } else {
        g_EnemySlot0_BehaviorFlags = (a & 0x7F) | 0x40;
    }
    b = g_ClArmForKey[(g_ClText0Buf + 2)[CL_TYPER_B->line * 18 + CL_TYPER_B->char_idx]];
    if (g_PlayerEntity.id & 1) {
        if ((a & 0x80) == (b & 0x80)) {
            CL_TYPER_B->timer = 0xD;
        } else {
            CL_TYPER_B->timer = 6;
        }
    } else {
        CL_TYPER_B->timer = (rand() & 0xF) + 0xA;
    }
}

void computer_typer_wait(void) {
    extern u8 * g_ClTyperPtr_u8p __asm__("g_ClTyperPtr");
    extern u8 g_ClText0Buf_u8[] __asm__("g_ClText0Buf");
    u8 *obj;
    s32 n;

    obj = g_ClTyperPtr_u8p;
    if ((--CL_TYP(obj)->timer << 16) == 0) {
        CL_TYP(obj)->char_idx += 1 + (g_PlayerEntity.id & 1);
        n = CL_TYP(obj)->char_idx;
        if (({ (CL_TYP(obj)->line * 0x12) + g_ClText0Buf_u8; })[n] == 0) {
            CL_TYP(obj)->state = 3;
            g_EnemySlot0_BehaviorFlags = 0x46;
        } else {
            CL_TYP(obj)->state = 1;
        }
    }
}

void computer_typer_finish(void) {
    if (g_EnemySlot0_BehaviorFlags & 0x20) {
        CL_TYPER->active = 0;
        CL_TYPER->state = 0;
    }
}

s16 computer_keyboard_input(void) {
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    s32 unused[1];
    s16 cur;
    s16 r;
    s32 oldx;
    s32 oldy;

    {
        u8 *f = &g_ClKeyboardFlags;

        CL_WIN(f)->enabled &= 0x7F;
    }
    cur = ISCREEN_B->cursor_row * 8 + ISCREEN_B->cursor_col;
    if (g_PadPressed & PAD_CONFIRM) {
        r = cur;
        switch (cur) {
        case 0:
            r = -2;
            break;
        case 0x17:
            r = -1;
            break;
        case 0x1D:
            r = -3;
            break;
        default:
            if (r >= 0x17) {
                r--;
            }
            break;
        }
        {
            u8 *f = &g_ClKeyboardFlags;

            *f |= 0x80;
        }
        return r;
    }
    if (g_ClPadRepeatLatch == (g_PadHeldRaw & PSX_DPAD) && g_ClPadRepeatLatch != 0) {
        if (g_ClPadRepeatTimer != 0) {
            g_ClPadRepeatTimer--;
            return 0;
        }
        g_ClPadRepeatTimer = 15;
    } else {
        g_ClPadRepeatTimer = 8;
    }
    g_ClPadRepeatLatch = g_PadTrigger & PSX_DPAD;
    {
        u8 *p = g_InteractiveScreenStatePtr_u8p;

        oldx = ISCR(p)->cursor_col;
        oldy = ISCR(p)->cursor_row;
        if ((g_PadHeldRaw & PSX_UP) && oldy > 0) {
            ISCR(p)->cursor_row = oldy - 1;
        }
    }
    if (g_PadHeldRaw & PSX_DOWN) {
        u8 *p = g_InteractiveScreenStatePtr_u8p;

        if (ISCR(p)->cursor_row < 3) {
            ISCR(p)->cursor_row = ISCR(p)->cursor_row + 1;
        }
    }
    if (g_PadHeldRaw & PSX_LEFT) {
        u8 *p = g_InteractiveScreenStatePtr_u8p;

        if (--ISCR(p)->cursor_col < 0) {
            ISCR(p)->cursor_col = 7;
        }
    }
    if (g_PadHeldRaw & PSX_RIGHT) {
        u8 *p = g_InteractiveScreenStatePtr_u8p;
        s32 t = ISCR(p)->cursor_col;

        ISCR(p)->cursor_col = t + 1;
        if (cur == 0x1D) {
            ISCR(p)->cursor_col = t + 2;
        }
        if (ISCREEN_B->cursor_col >= 8) {
            ISCREEN_B->cursor_col = 0;
        }
    }
    {
        u8 *p = g_InteractiveScreenStatePtr_u8p;
        s32 y = ISCR(p)->cursor_row;
        s16 c2 = y * 8 + ISCR(p)->cursor_col;

        if (c2 == 0x1F) {
            ISCR(p)->cursor_row = y - 1;
        }
        if (c2 == 0x1E) {
            ISCREEN_B->cursor_col = ISCREEN_B->cursor_col - 1;
        }
    }
    if (oldx != ISCREEN_B->cursor_col || oldy != ISCREEN_B->cursor_row) {
        u8 *f = &g_ClKeyboardFlags;

        *f |= 0x80;
    }
    return 0;
}

void computer_text_append(s16 ch, u8 *buf) {
    u8 *pos;
    u8 n;

    if ((u16)(ch + 2) >= 2) {
        pos = &g_ClCaretPos;
        n = *pos;
        if (ch == -3) {
            if (n != 0 && (n < 7 || buf[n] == 0)) {
                (*pos)--;
            }
            buf[g_ClCaretPos] = 0;
        } else {
            buf[n] = ch;
            if (n < 7) {
                (*pos)++;
            }
        }
    }
}

void computer_text_begin(s32 rec) {
    extern u8 g_ClText0X_u8 __asm__("g_ClText0X");
    extern u8 g_ClText0Y_u8 __asm__("g_ClText0Y");
    extern u8 g_ClText0Buf_u8[] __asm__("g_ClText0Buf");
    u8 *p;
    u8 i;
    s32 t;

    i = 0;
    p = &g_ClText0Buf_u8[(rec & 0xFF) * 0x12];
    do {
        p[i] = 0;
        i++;
    } while (i < 9);
    t = (rec & 0xFF) * 0x12;
    g_ClCaretWinIdx = *(&g_ClText0WinIdx + t);
    g_ClCaretBaseX = *(u16 *)(&g_ClText0X_u8 + t);
    g_ClCaretBaseY = *(u16 *)(&g_ClText0Y_u8 + t);
    g_ClCaretPos = 0;
    g_ClBlinkCounter = 0;
    g_ClTextRecIdx = rec;
}

typedef struct {
    u8 enabled;
    u8 row_count;
    s16 x;
    s16 y;
    s16 cols;
    u16 rows;
    s16 pad0A;
    s32 row_data;
    u8 anim_cmd;
    u8 anim_state;
    s16 alpha;
    s16 visible;
    u8 pad16[6];
} ClWindowInit; /* size 0x1C */

void computer_window_setup(u8 win, s16 x, s16 y, s16 cols, u16 rows, u8 row_count, s32 row_data) {
    extern ClWindowInit g_ClWindowsInit_l[] __asm__("g_ClWindows");
    g_ClWindowsInit_l[win].x = x;
    g_ClWindowsInit_l[win].y = y;
    g_ClWindowsInit_l[win].cols = cols;
    g_ClWindowsInit_l[win].alpha = 0x80;
    g_ClWindowsInit_l[win].visible = 0;
    g_ClWindowsInit_l[win].anim_state = 0;
    g_ClWindowsInit_l[win].rows = rows;
    g_ClWindowsInit_l[win].row_count = row_count;
    g_ClWindowsInit_l[win].row_data = row_data;
}

void computer_draw_keyboard(void) {
    extern u8 g_ClKeyboardFlags_u8 __asm__("g_ClKeyboardFlags");
    extern u16 g_ClBlinkCounter_u16 __asm__("g_ClBlinkCounter");
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void computer_draw_quads_l() __asm__("computer_draw_quads");
    u8 *flags = &g_ClKeyboardFlags_u8;
    u8 *q;
    u8 r;
    u8 g;
    u8 b;
    s16 t;

    if ((*flags & 0x7F) && g_ClKeyboardAlpha != 0) {
        q = g_InteractiveScreenStatePtr_u8p;
        LIT(s32, sp_sprite_attribute) = 0x01000000;
        LIT(u16, sp_sprite_tpage) = 9;
        LIT(s16, sp_sprite_cy) = 0x1EC;
        LIT(s16, sp_sprite_cx) = 0;
        SCRATCH->spr_w = 0x18;
        SCRATCH->spr_h = 0x18;
        LIT(s16, sp_sprite_mx) = 0;
        LIT(s16, sp_sprite_my) = 0;
        do { /* scheduling barrier: keeps the r/g/b saves in the load-delay gaps */
            LIT(s16, sp_sprite_x) = FIELD_AT(q, InteractiveScreen, ->cursor_col, s32) * 0x18 - 0x58;
            LIT(s16, sp_sprite_y) = FIELD_AT(q, InteractiveScreen, ->cursor_row, s32) * 0x18 + 2;
            LIT(u8, sp_sprite_u) = FIELD_AT(q, InteractiveScreen, ->cursor_col, s32) * 0x18 + 0x10;
            r = g_ClColorR;
            g = g_ClColorG;
            b = g_ClColorB;
        } while (0);
        g_ClColorR = g_ClKeyboardAlpha;
        g_ClColorG = g_ClKeyboardAlpha;
        g_ClColorB = g_ClKeyboardAlpha;
        LIT(u8, sp_sprite_r) = g_ClKeyboardAlpha;
        LIT(u8, sp_sprite_g) = g_ClKeyboardAlpha;
        LIT(u8, sp_sprite_b) = g_ClKeyboardAlpha;
        LIT(u8, sp_sprite_v) = ISCR(q)->cursor_row * 0x18 + 0x20;
        t = FIELD_AT(q, InteractiveScreen, ->cursor_row, s32) * 8 + FIELD_AT(q, InteractiveScreen, ->cursor_col, u16);
        if (t == 0x17) {
            SCRATCH->spr_h = 0x30;
        }
        if (t == 0x1D) {
            SCRATCH->spr_w = 0x30;
        }
        if ((g_ClBlinkCounter_u16 & 0xFF10) != 0x10) {
            GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
        }
        if (*flags & 0x80) {
            g_ClBlinkHold = 0x14;
        }
        g_ClBlinkHold -= g_ClBlinkHold != 0;
        computer_draw_quads_l(-0x68, -0xE, 0x83, 0xA, &g_ClKeyboardFrame, 9);
        g_ClColorR = r;
        g_ClColorG = g;
        g_ClColorB = b;
    }
}

void computer_draw_windows(void) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    typedef struct {
        u16 x, y;
        u8 u, v;
        u16 w, h;
        u8 flags, cnt;
    } ClQuadView;
    typedef struct {
        u8 u;
        u8 pad1[3];
        u8 v;
        u8 pad5[3];
        s16 len;
        s16 h;
        u16 tpage;
        u8 padE[2];
    } Line;
    typedef struct {
        u8 active, count;
        u16 x, y, w, h;
        u8 padA[2];
        Line *lines;
        u8 pad10[2];
        s16 color;
        s16 flag;
        u8 pad16[6];
    } Box;
    extern Box g_ClWindows_l[] __asm__("g_ClWindows");
    extern ClQuadView g_ClFrameQuads_l[] __asm__("g_ClFrameQuads");
    extern struct { u8 r, pad1[3], g, pad5[3], b, pad9[3]; } g_ClColorR_l __asm__("g_ClColorR");
    void computer_draw_quads_w(s16, s16, s32, s32, ClQuadView *, s32) __asm__("computer_draw_quads");
    ClQuadView *t;
    ClQuadView *q;
    Box *b;
    Line *ln;
    s16 i;
    s16 sr, sg, sb;
    s16 j, k;
    s32 f;
    u16 x, y;
    s32 pri;
    s32 ot;

    b = g_ClWindows_l;
    t = &g_ClFrameQuads_l[1];
    q = g_ClFrameQuads_l;
    ot = ADDR_g_OrderingTables;
    sr = g_ClColorR_l.r;
    sg = g_ClColorR_l.g;
    sb = g_ClColorR_l.b;
    for (i = 0; i < 3; i++) {
        if (b->active == 0 || b->color == 0) {
            b++;
            continue;
        }
        t[0].x = (b->w + 1) << 4;
        t[6].x = (b->w + 1) << 4;
        t[1].y = (b->h + 1) << 4;
        t[4].y = (b->h + 1) << 4;
        t[2].x = (b->w + 1) << 4;
        t[2].y = (b->h + 1) << 4;
        t[3].cnt = b->w;
        t[4].cnt = b->w;
        t[5].cnt = b->h;
        t[6].cnt = b->h;
        x = b->x;
        y = b->y;
        f = b->flag;
        g_ClColorR_l.r = b->color;
        g_ClColorR_l.g = b->color;
        g_ClColorR_l.b = b->color;
        computer_draw_quads_w(x, y, f != 0 ? 0x84 : 4, 8, q, 10);
        LITP(GsSPRITE, sp_sprite)->attribute = b->flag != 0 ? 0x41000000 : 0x01000000;
        LITP(GsSPRITE, sp_sprite)->tpage = 9;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1EC;
        LITP(GsSPRITE, sp_sprite)->cx = 0;
        LITP(GsSPRITE, sp_sprite)->w = 0x10;
        LITP(GsSPRITE, sp_sprite)->h = 0x10;
        LITP(GsSPRITE, sp_sprite)->mx = 0;
        LITP(GsSPRITE, sp_sprite)->my = 0;
        LITP(GsSPRITE, sp_sprite)->r = b->color;
        LITP(GsSPRITE, sp_sprite)->g = b->color;
        LITP(GsSPRITE, sp_sprite)->b = b->color;
        ln = b->lines;
        LITP(GsSPRITE, sp_sprite)->y = y + 0x10;
        for (j = 0; j < b->count; ln++, j++) {
            LITP(GsSPRITE, sp_sprite)->x = x + 0x10;
            if (ln->len != 0) {
                LITP(GsSPRITE, sp_sprite)->u = ln->u;
                LITP(GsSPRITE, sp_sprite)->v = ln->v;
                LITP(GsSPRITE, sp_sprite)->w = ({ ln->len << 3; });
                LITP(GsSPRITE, sp_sprite)->h = ({ ln->h << 4; });
                LITP(GsSPRITE, sp_sprite)->tpage = ln->tpage;
                LITP(GsSPRITE, sp_sprite)->cy = ((ln->tpage - 7) >> 1) + 0x1EB;
                GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ot), 4);
            }
            if (ln->len < b->w * 2) {
                LITP(GsSPRITE, sp_sprite)->x = x + (ln->len + 2) * 8;
                LITP(GsSPRITE, sp_sprite)->u = 0xE0;
                LITP(GsSPRITE, sp_sprite)->v = 0x30;
                LITP(GsSPRITE, sp_sprite)->w = 8;
                LITP(GsSPRITE, sp_sprite)->h = 0x10;
                LITP(GsSPRITE, sp_sprite)->tpage = 9;
                LITP(GsSPRITE, sp_sprite)->cy = 0x1EC;
                for (k = 0; k < b->w * 2 - ln->len; k++) {
                    GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ot), 4);
                    LITP(GsSPRITE, sp_sprite)->x += 8;
                }
            }
            LITP(GsSPRITE, sp_sprite)->y += LITP(GsSPRITE, sp_sprite)->h;
        }
        b++;
    }
    g_ClColorR_l.r = sr;
    g_ClColorR_l.g = sg;
    g_ClColorR_l.b = sb;
}

void computer_draw_quads(u16 x, u16 y, u16 flags, u8 n, u8 *rec, u16 tpage) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    u16 big;
    u8 i;
    u8 j;
    u8 cnt;
    s32 t;

    big = flags & 0x40;
    i = 0;
    LITP(GsSPRITE, sp_sprite)->cx = 0;
    LITP(GsSPRITE, sp_sprite)->mx = 0;
    LITP(GsSPRITE, sp_sprite)->my = 0;
    LITP(GsSPRITE, sp_sprite)->tpage = tpage;
    LITP(GsSPRITE, sp_sprite)->cy = (s32)(tpage - 7) / 2 + 0x1EB;
    for (; i < n; rec += 12, i++) {
        t = (flags & 0x80) << 23;
        t |= ((CL_QUAD(rec)->flags & 0xC0) << 16) | ({ 0x01000000; });
        LITP(GsSPRITE, sp_sprite)->attribute = t;
        LITP(GsSPRITE, sp_sprite)->x = x + CL_QUAD(rec)->x;
        LITP(GsSPRITE, sp_sprite)->y = y + CL_QUAD(rec)->y;
        LITP(GsSPRITE, sp_sprite)->u = CL_QUAD(rec)->u;
        LITP(GsSPRITE, sp_sprite)->v = CL_QUAD(rec)->v;
        LITP(GsSPRITE, sp_sprite)->w = CL_QUAD(rec)->w;
        LITP(GsSPRITE, sp_sprite)->h = CL_QUAD(rec)->h;
        LITP(GsSPRITE, sp_sprite)->r = g_ClColorR;
        LITP(GsSPRITE, sp_sprite)->g = g_ClColorG;
        LITP(GsSPRITE, sp_sprite)->b = g_ClColorB;
        cnt = CL_QUAD(rec)->count + (CL_QUAD(rec)->count == 0);
        if (!(CL_QUAD(rec)->flags & 1)) {
            for (j = 0; j < cnt; j++) {
                if (!big) {
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), flags & 0x3F);
                } else {
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OtMain), 0x3FF);
                }
                LITP(GsSPRITE, sp_sprite)->x += CL_QUAD(rec)->w;
            }
        } else {
            for (j = 0; j < cnt; j++) {
                if (!big) {
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), flags & 0x3F);
                } else {
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OtMain), 0x3FF);
                }
                LITP(GsSPRITE, sp_sprite)->y += CL_QUAD(rec)->h;
            }
        }
    }
}

void computer_draw_text(void) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    int sprintf();
    typedef struct {
        u8 enabled;
        u8 win_idx;
        s16 x;
        s16 y;
        u8 text[9];
        u8 masked;
        u8 pad[2];
    } ClTextEntry;
    typedef struct {
        u8 enabled;
        u8 row_count;
        u16 x;
        u16 y;
        u8 unused_06[0xA];
        u8 anim_cmd;
        u8 anim_state;
        s16 alpha;
        s16 visible;
        u8 pad[6];
    } ClWindowView;
    extern ClTextEntry g_ClTextFields_l[] __asm__("g_ClTextFields");
    extern ClWindowView g_ClWindows_view[] __asm__("g_ClWindows");
    ClTextEntry *e;
    s32 n;
    u8 i;
    u8 j;
    u8 k;
    u16 x;
    u16 y;
    u16 bx;
    u16 by;

    e = g_ClTextFields_l;
    LITP(GsSPRITE, sp_sprite)->tpage = 9;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1EC;
    LITP(GsSPRITE, sp_sprite)->cx = 0;
    LITP(GsSPRITE, sp_sprite)->w = 16;
    LITP(GsSPRITE, sp_sprite)->h = 16;
    LITP(GsSPRITE, sp_sprite)->mx = 0;
    LITP(GsSPRITE, sp_sprite)->my = 0;
    sprintf(LITP(char, sp_text), "*");
    for (i = 0; i < 2; e++, i++) {
        n = e->win_idx;
        LIT(u32, sp_sprite_attribute) = g_ClWindows_view[n].visible ? 0x41000000 : 0x01000000;
        if (e->enabled != 0 && g_ClWindows_view[n].enabled != 0 && g_ClWindows_view[n].anim_cmd == 0 && g_ClWindows_view[n].alpha != 0) {
            bx = g_ClWindows_view[n].x;
            by = g_ClWindows_view[n].y;
            LITP(GsSPRITE, sp_sprite)->x = bx + ({ e->x * 8 + 16; });
            LITP(GsSPRITE, sp_sprite)->y = by + ({ e->y * 16 + 16; });
            for (j = 0; j < 8; j++) {
                k = e->text[j];
                if (k != 0) {
                    k--;
                    LITP(GsSPRITE, sp_sprite)->u = (k & 0xF) * 16;
                    LITP(GsSPRITE, sp_sprite)->v = k & 0x10;
                    if (e->masked != 0) {
                        x = LITP(GsSPRITE, sp_sprite)->x;
                        y = LITP(GsSPRITE, sp_sprite)->y;
                        sprintf(LITP(char, sp_text), "*");
                        print_text_8x8((s16)x + 0xA4, (s16)y + 0x7C, 0, 0);
                        LIT(u32, sp_sprite_attribute) = g_ClWindows_view[n].visible ? 0x41000000 : 0x01000000;
                        LITP(GsSPRITE, sp_sprite)->tpage = 9;
                        LITP(GsSPRITE, sp_sprite)->cy = 0x1EC;
                        LITP(GsSPRITE, sp_sprite)->w = 16;
                        LITP(GsSPRITE, sp_sprite)->h = 16;
                        LITP(GsSPRITE, sp_sprite)->u = 0xB0;
                        LITP(GsSPRITE, sp_sprite)->x = x;
                        LITP(GsSPRITE, sp_sprite)->y = y;
                        LITP(GsSPRITE, sp_sprite)->cx = 0;
                        LITP(GsSPRITE, sp_sprite)->v = 0xF;
                    }
                    GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
                }
                LITP(GsSPRITE, sp_sprite)->x += 16;
            }
        }
    }
}

typedef struct {
    u8 on;
    u8 pad;
    u16 x;
    u16 y;
    u8 rest[0x16];
} Win1C;

void computer_draw_caret(void) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern Win1C g_ClWindows_win[] __asm__("g_ClWindows");
    extern u8 g_ClText0Buf_l[][18] __asm__("g_ClText0Buf");
    extern u8 g_ClInputEnabled_l __asm__("g_ClInputEnabled");
    extern u8 g_ClCaretWinIdx_l __asm__("g_ClCaretWinIdx");
    extern u8 g_ClCaretMode_l __asm__("g_ClCaretMode");
    extern u8 g_ClCaretBarWidth_l __asm__("g_ClCaretBarWidth");
    extern u8 g_ClTextRecIdx_l __asm__("g_ClTextRecIdx");
    extern u8 g_ClBlinkCounter_l __asm__("g_ClBlinkCounter");
    u8 win;
    u8 col;
    u8 cnt;
    u8 c;
    u8 i;
    u16 y;

    win = g_ClCaretWinIdx_l;
    col = g_ClCaretPos;
    if (g_ClInputEnabled_l == 0 || g_ClWindows_win[win].on == 0) {
        return;
    }
    LIT(s32, sp_sprite_attribute) = 0x01000040;
    SCRATCH->spr_tpage = 9;
    SCRATCH->spr_clut_y = 0x1EC;
    SCRATCH->spr_w = 0x10;
    SCRATCH->spr_h = 0x10;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_pivot_x = 0;
    SCRATCH->spr_pivot_y = 0;
    cnt = ++g_ClBlinkCounter_l;
    if (g_ClCaretMode_l == 0) {
        SCRATCH->spr_u = 0xB0;
        SCRATCH->spr_v = 0x10;
        SCRATCH->spr_x = g_ClWindows_win[win].x + ({ (g_ClCaretBaseX + col * 2) * 8 + 0x10; });
        y = g_ClWindows_win[win].y + ({ g_ClCaretBaseY * 16 + 0x10; });
        SCRATCH->spr_y = y;
        c = *(({ (u8 *)g_ClText0Buf_l + g_ClTextRecIdx_l * 18; }) + col);
        if (cnt & 0x10) {
            if (c != 0) {
                return;
            }
            SCRATCH->spr_v = 0xF;
        }
        if (col == 7 && c != 0) {
            SCRATCH->spr_v = 0x1F;
            SCRATCH->spr_h = 1;
            SCRATCH->spr_y = y + 0xF;
        }
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
    } else if (!(cnt & 0x10)) {
        LIT(s32, sp_sprite_attribute) = 0x41000040;
        SCRATCH->spr_u = 0xC0;
        SCRATCH->spr_v = 0x10;
        SCRATCH->spr_x = g_ClWindows_win[win].x + ({ g_ClCaretBaseX * 8 + 8; });
        SCRATCH->spr_y = g_ClWindows_win[win].y + ({ (col + g_ClCaretBaseY) * 16 + 0xE; });
        for (i = 0; i < g_ClCaretBarWidth_l; i++) {
            GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
            SCRATCH->spr_x += 0x10;
        }
    }
}

void computer_draw_status(void) {
    s32 computer_draw_quads_a5(s32, s32, s32, s32, s32 *, s32) __asm__("computer_draw_quads");
    extern s8 g_ClStripA_s8 __asm__("g_ClStripA");
    extern s8 g_ClStripB_s8 __asm__("g_ClStripB");
    extern u8 g_ClMsgFlags_u8 __asm__("g_ClMsgFlags");
    extern u8 g_ClStatusIdx_u8 __asm__("g_ClStatusIdx");
    s32 n_quads;
    s32 msg;

    if (g_ClStatusIdx_u8 != 0) {
        n_quads = 1;
        if (g_ClStatusIdx_u8 == 4) {
            n_quads = 2;
        }
        computer_draw_quads_a5(-0x30, -0x3D, 0x40, n_quads, (s32 *)((g_ClStatusIdx_u8 * 0xC) + &g_ClStatusBanners), 7);
    }
    msg = g_ClMsgFlags_u8 & 3;
    if (msg != 0) {
        if (msg == 2) {
            if (g_ClMsgFlags_u8 & 0x10) {
                computer_draw_quads_a5(-0x3C, -0x5F, 0x40, 2, (s32 *)&g_ClStripA_s8, 7);
            }
            if (g_ClMsgFlags_u8 & 0x20) {
                computer_draw_quads_a5(-0x3C, -0x5F, 0x40, 3, (s32 *)&g_ClStripB_s8, 7);
            }
        }
        computer_draw_quads_a5(-0x3C, -0x5F, (g_ClMsgFlags_u8 & 0x80) | 0x40, 1, (s32 *)(((g_ClMsgFlags_u8 & 3) * 0xC) + &g_ClMsgPictures), 7);
    }
}

void computer_draw_door_picture(void) {
    s32 computer_draw_quads_a6() __asm__("computer_draw_quads");
    extern u8 g_ClDoorOrigin0_u8[] __asm__("g_ClDoorOrigin0");
    extern u8 g_ClDoorMarker0_u8[] __asm__("g_ClDoorMarker0");
    extern u8 g_ClDoorOrigin1_u8[] __asm__("g_ClDoorOrigin1");
    extern u8 g_ClDoorMarker1_u8[] __asm__("g_ClDoorMarker1");
    u8 sel;
    u8 *p;
    u8 *q;
    s32 idx;
    u8 i;
    u8 x;

    if (g_ClDoorSel & 0x80) {
        idx = g_ClDoorSel & 0x7F;
        sel = g_ClDoorSel;
        if (idx == 0) {
            /* both arms identical: found by decomp-permuter, needed for the original code */
            if (sel) {
                p = g_ClDoorOrigin0_u8;
                q = g_ClDoorMarker0_u8;
            } else {
                p = g_ClDoorOrigin0_u8;
                q = g_ClDoorMarker0_u8;
            }
        } else {
            p = g_ClDoorOrigin1_u8;
            q = g_ClDoorMarker1_u8;
        }
        q += 0xC;
        x = *p++;
        computer_draw_quads_a6((s16)(x + 0xFFC4), (s16)(p[0] + 0xFFA1), 0x40, p[1], q, 0xA);
        computer_draw_quads_a6(-0x18, -0x1F, 0x40, idx + 1, g_ClDoorPictureQuads + idx * 0xC, 0xA);
        computer_draw_quads_a6(-0x20, -0x57, 0x40, 4, g_ClDoorPictureFrame, 8);
        for (i = 0; i < 6; i++) {
            computer_draw_quads_a6(-0x20, i * 0x10 - 0x57, 0x40, 1, g_ClDoorPictureFrame - 0xC, 0xA);
        }
    }
}

void computer_draw_leds(void) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    int rand(void);
    LIT(s32, sp_sprite_attribute) = 0x41000040;
    LIT(s16, sp_sprite_tpage) = 7;
    LIT(s16, sp_sprite_cy) = 0x1EB;
    LIT(s16, sp_sprite_w) = 6;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(s16, sp_sprite_mx) = 0;
    LIT(s16, sp_sprite_my) = 0;
    LIT(s16, sp_sprite_h) = 4;
    if (g_ClLedFlags & 1) {
        LIT(s16, sp_sprite_x) = 0x6F;
        LIT(s16, sp_sprite_y) = -0x28;
        LIT(u8, sp_sprite_u) = 0xE8;
        LIT(u8, sp_sprite_v) = 0x88;
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 4U);
    }
    if ((g_ClLedFlags & 2) && (rand() & 1)) {
        LIT(s16, sp_sprite_x) = 0x6F;
        LIT(s16, sp_sprite_y) = -0x23;
        LIT(u8, sp_sprite_u) = 0xE8;
        LIT(u8, sp_sprite_v) = 0x98;
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 4U);
    }
}

s32 computer_fade_step(s32 step, s8 msg_flags) {
  s32 done;
  done = 1;
  if ((*((s16 *) (((s8 *) g_InteractiveScreenStatePtr) + 0x10))) != 2)
  {
    if ((*((s16 *) (((s8 *) g_InteractiveScreenStatePtr) + 0x12))) == 0)
    {
      g_ClColorR -= step;
      g_ClColorG -= step;
      g_ClColorB -= step;
      if (((g_ClColorR == 0) || (g_ClColorG == 0)) || ((done = 0, g_ClColorB == 0)))
      {
        g_ClColorB = 0;
        g_ClColorG = 0;
        g_ClColorR = 0;
        g_ClMsgFlags = msg_flags;
        *((s16 *) (((s8 *) g_InteractiveScreenStatePtr) + 0x12)) = (s16) (((u16) ((struct 
        {
          char m2c_pad[(0x12 > 0) ? (0x12) : (0)];
          typeof(*((s16 *) 0)) m2c_f;
        } *) ((0x12 < 0) ? ((void *) (((s8 *) g_InteractiveScreenStatePtr) + 0x12)) : ((void *) g_InteractiveScreenStatePtr)))->m2c_f) + 1);
      }
      done = 0;
      return done;
    }
    g_ClColorR += step;
    g_ClColorG += step;
    g_ClColorB += step;
    if (((((u8) g_ClColorR) >= 0x80U) || (((u8) g_ClColorG) >= 0x80U)) || ((done = 0, (((u8) g_ClColorB) < 0x80U) == 0)))
    {
      g_ClColorB = 0x80;
      g_ClColorG = 0x80;
      g_ClColorR = 0x80;
      *((s16 *) (((s8 *) g_InteractiveScreenStatePtr) + 0x12)) = (s16) (((u16) ((struct 
      {
        char m2c_pad[(0x12 > 0) ? (0x12) : (0)];
        typeof(*((s16 *) 0)) m2c_f;
      } *) ((0x12 < 0) ? ((void *) (((s8 *) g_InteractiveScreenStatePtr) + 0x12)) : ((void *) g_InteractiveScreenStatePtr)))->m2c_f) + 1);
      return 1;
    }
    return done;
  }
  return done;
}

void computer_camera_step(void) {
    extern u16 g_ClPendingCamDelay_u16 __asm__("g_ClPendingCamDelay");
    if (g_ClPendingCamera & 0x80) {
        if (g_ClPendingCamDelay_u16 != 0) {
            g_ClPendingCamDelay_u16 -= 1;
            return;
        }
        g_CutId = (u8) g_ClPendingCamera & 0x7F;
        check_camera_switch(1);
        g_ClPendingCamDelay_u16 = 0;
        g_ClPendingCamera &= 0x7F;
    }
}

void computer_logo_update(void) {
    s32 * computer_logo_alloc();
    extern s32 (*computer_logo_behaviour_tbl[])();
    extern s8 * g_ClLogoCur_s8p __asm__("g_ClLogoCur");
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 *logo;
    s32 i;

    if (g_ClIntroLogoState == 0) {
        g_ClLogoCur_s8p = (s8 *)&g_ClLogoSlots;
        i = 0;
        do {
            CL_LOGO_S_FIELD_AS(active, u8) = 0U;
            CL_LOGO_S_FIELD_AS(substep, s8) = 0;
            i += 1;
            g_ClLogoCur_s8p += 0x30;
        } while ((u32) (i & 0xFF) < 9U);
        logo = computer_logo_alloc();
        g_ClLogoCur_s8p = (s8 *)logo;
        CLLOGO(logo)->behaviour = 0;
        CL_LOGO_S_FIELD(x) = 0;
        CL_LOGO_S_FIELD(y) = 0xFFD50000;
        voice_command(1, 0xB7);
        LIT(s32, g_GameStateFlags) |= GAME_VOICE_PLAYING;
        g_ClIntroLogoState++;
    }
    g_ClLogoCur_s8p = (s8 *)&g_ClLogoSlots;
    i = 0;
    do {
        i += 1;
        if (CL_LOGO_S_FIELD_AS(active, u8) != 0) {
            computer_logo_behaviour_tbl[CL_LOGO_S_FIELD(behaviour)]();
        }
        g_ClLogoCur_s8p += 0x30;
    } while ((u32) (i & 0xFF) < 9U);
    g_ClLogoCur_s8p = (s8 *)&g_ClLogoSlots;
    LIT(s32, sp_sprite_attribute) = 0x01000040;
    LIT(s16, sp_sprite_cx) = 0;
    i = 0;
    do {
        i += 1;
        if (CL_LOGO_S_FIELD_AS(active, u8) != 0) {
            LIT(s16, sp_sprite_tpage) = 0xB;
            LIT(s16, sp_sprite_cy) = 0x1ED;
            LIT(u16, sp_sprite_mx) = CL_LOGO_S_FIELD_AS(pivot_x, u16);
            LIT(u16, sp_sprite_my) = CL_LOGO_S_FIELD_AS(pivot_y, u16);
            LIT(u8, sp_sprite_u) = CL_LOGO_S_FIELD_AS(tex_u, u8);
            LIT(u8, sp_sprite_v) = CL_LOGO_S_FIELD_AS(tex_v, u8);
            LIT(u16, sp_sprite_w) = CL_LOGO_S_FIELD_AS(width, u16);
            LIT(u16, sp_sprite_h) = CL_LOGO_S_FIELD_AS(height, u16);
            LIT(u16, sp_sprite_scalex) = CL_LOGO_S_FIELD_AS(scale_x, u16);
            LIT(u16, sp_sprite_scaley) = CL_LOGO_S_FIELD_AS(scale_y, u16);
            LIT(s32, sp_sprite_rotate) = CL_LOGO_S_FIELD(angle) << 0xC;
            LIT(s16, sp_sprite_x) = CL_LOGO_S_FIELD(x) >> 16;
            LIT(s16, sp_sprite_y) = CL_LOGO_S_FIELD(y) >> 16;
            GsSortSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), CL_LOGO_S_FIELD_AS(ot_pri, u16));
        }
        g_ClLogoCur_s8p += 0x30;
    } while ((u32) (i & 0xFF) < 9U);
}

void computer_logo_hub(void) {
    extern s32 (*computer_logo_hub_step_tbl[])();
    s32 angle;

    computer_logo_hub_step_tbl[CL_LOGO->substep]();
    angle = CL_LOGO->angle + 0xF;
    CL_LOGO->angle = angle;
    if (angle == 0x168) {
        CL_LOGO->angle = 0;
    }
}

void computer_logo_hub_init(void) {
    CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
    CL_LOGO->tex_u = 0x20;
    CL_LOGO->tex_v = 0x18;
    CL_LOGO->width = 0x20;
    CL_LOGO->pivot_x = 0x10;
    CL_LOGO->scale_x = 0x1000;
    CL_LOGO->scale_y = 0x1000;
    CL_LOGO->height = 0x28;
    CL_LOGO->ot_pri = 5;
    CL_LOGO->pivot_y = 0x28;
    CL_LOGO->angle = 0;
    CL_LOGO->timer = 3;
}

void computer_logo_hub_spin_up(void) {
    void computer_logo_hub_spawn(void);
    s32 * computer_logo_alloc();
    u16 timer;
    void *child;

    if (CL_LOGO->angle == 0) {
        timer = CL_LOGO->timer - 1;
        CL_LOGO->timer = timer;
        if ((timer << 0x10) == 0) {
            CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
            CL_LOGO->timer = 4U;
            computer_logo_hub_spawn();
            child = computer_logo_alloc();
            CLLOGO(child)->behaviour = 3;
            CLLOGO(child)->parent = (void *) g_ClLogoCur;
        }
    }
}

void computer_logo_hub_spawn(void) {
    s32 * computer_logo_alloc();
    s32 angle;
    s8 behaviour;
    u16 timer;
    void *child;

    angle = CL_LOGO->angle;
    if (angle == ((CL_LOGO->angle / 90) * 0x5A)) {
        timer = CL_LOGO->timer - 1;
        CL_LOGO->timer = timer;
        if ((timer << 0x10) == 0) {
            CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
            CL_LOGO->timer = 3U;
            CL_LOGO->phase_signal = 0;
            child = computer_logo_alloc();
            behaviour = 2;
        } else {
            child = computer_logo_alloc(angle, g_ClLogoCur);
            behaviour = 1;
        }
        CLLOGO(child)->behaviour = behaviour;
        CLLOGO(child)->x = (s32) CL_LOGO->x;
        CLLOGO(child)->y = (s32) CL_LOGO->y;
        CLLOGO(child)->angle = (s32) CL_LOGO->angle;
        CLLOGO(child)->parent = (void *) g_ClLogoCur;
    }
}

void computer_logo_hub_hold(void) {
    u16 timer;

    if (CL_LOGO->angle == 0) {
        timer = CL_LOGO->timer - 1;
        CL_LOGO->timer = timer;
        if ((timer << 0x10) == 0) {
            CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
            CL_LOGO->phase_signal = 1;
        }
    }
}

void computer_logo_move_out(void) {
  int x_min;
  s32 x;
  x = *((s32 *) (((s8 *) g_ClLogoCur) + 8));
  x_min = 0xFFD60000;
  if (x <= x_min)
  {
    *((u8 *) (((s8 *) g_ClLogoCur) + 2)) = (u8) ((*((u8 *) (((s8 *) g_ClLogoCur) + 2))) + 1);
    *((s32 *) (((s8 *) g_ClLogoCur) + 8)) = -0x2A0000;
    return;
  }
  *((s32 *) (((s8 *) g_ClLogoCur) + 8)) = (s32) (x + 0xFFFD0000);
  *((u16 *) (((s8 *) g_ClLogoCur) + 0x28)) = (u16) ((*((s16 *) (((s8 *) g_ClLogoCur) + 0x28))) - 0xD0);
  *((u16 *) (((s8 *) g_ClLogoCur) + 0x2A)) = (u16) ((*((u16 *) (((s8 *) g_ClLogoCur) + 0x2A))) - 0xD0);
}

void computer_logo_hub_end(void) {
  s8 *substep_p;
  s32 angle;
  if (1)
  {
    angle = *((s32 *) (((s8 *) g_ClLogoCur) + 0x2C));
    if ((angle + 0xF) == (((angle + 0xF) / 90) * 0x5A))
    {
      void *logo;
      logo = g_ClLogoCur;
      *((s8 *) (((s8 *) g_ClLogoCur) + 0)) = 0;
      substep_p = ((s8 *) g_ClLogoCur) + 2;
      *((s16 *) (((s8 *) logo) + 0x16)) = 2;
      *((s8 *) substep_p) = 0;
    }
  }
}

void computer_logo_wedge_a(void) {
    extern s32 (*computer_logo_wedge_a_step_tbl[])();
    computer_logo_wedge_a_step_tbl[CL_LOGO->substep]();
}

void computer_logo_wedge_a_init(void) {
    CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
    CL_LOGO->tex_u = 0x20;
    CL_LOGO->tex_v = 0x18;
    CL_LOGO->height = 0x28;
    CL_LOGO->width = 0x20;
    CL_LOGO->pivot_x = 0x10;
    CL_LOGO->scale_x = 0x1000;
    CL_LOGO->scale_y = 0x1000;
    CL_LOGO->pivot_y = 0x28;
    CL_LOGO->ot_pri = 5;
}

void computer_logo_wedge_a_seed(void) {
    s32 * computer_logo_alloc();
    u8 *t;
    u8 *d;

    t = ((u8 *) CLLOGO(g_ClLogoCur)->parent);
    if (CLLOGO(t)->angle == 0x10E) {
        CLLOGO(g_ClLogoCur)->substep++;
        CLLOGO(g_ClLogoCur)->phase_signal = 0;
        t = (u8 *)computer_logo_alloc(t);
        CLLOGO(t)->behaviour = 2;
        CLLOGO(t)->x = CLLOGO(g_ClLogoCur)->x;
    CLLOGO(t)->y = CLLOGO(g_ClLogoCur)->y;
    CLLOGO(t)->angle = CLLOGO(g_ClLogoCur)->angle;
    CLLOGO(t)->parent = g_ClLogoCur;
    }
}

void computer_logo_wedge_a_spin(void) {
    u8 *obj;
    u8 *t;
    s32 v;

    obj = g_ClLogoCur;
    t = ((u8 *) CLLOGO(obj)->parent);
    v = CLLOGO(obj)->angle + 0xF;
    CLLOGO(obj)->angle = v;
    if (v == 0x168) {
        CLLOGO(obj)->angle = 0;
    }
    if (CLLOGO(t)->phase_signal == 1) {
        CLLOGO(g_ClLogoCur)->substep++;
        CLLOGO(g_ClLogoCur)->phase_signal = 1;
    }
}

void computer_logo_spin_move_out(void) {
    void computer_logo_move_out();
    s32 angle;

    angle = CL_LOGO->angle + 0xF;
    CL_LOGO->angle = angle;
    if (angle == 0x168) {
        CL_LOGO->angle = 0;
    }
    computer_logo_move_out();
}

void computer_logo_wedge_retire(void) {
    u8 *obj;
    u8 *t;
    s32 v;

    obj = g_ClLogoCur;
    t = ((u8 *) CLLOGO(obj)->parent);
    v = CLLOGO(obj)->angle + 0xF;
    CLLOGO(obj)->angle = v;
    if (v == 0x168) {
        CLLOGO(obj)->angle = 0;
    }
    if (CLLOGO(t)->phase_signal == 2) {
        u8 *p = g_ClLogoCur;

        CLLOGO(p)->active = 0;
        CLLOGO(p)->phase_signal = 2;
        CLLOGO(g_ClLogoCur)->substep = 0;
    }
}


void computer_logo_wedge_b(void) {
    extern s32 (*computer_logo_wedge_b_step_tbl[])();
    computer_logo_wedge_b_step_tbl[CL_LOGO->substep]();
}

void computer_logo_wedge_b_init(void) {
    CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
    CL_LOGO->tex_u = 0;
    CL_LOGO->tex_v = 0x18;
    CL_LOGO->height = 0x28;
    CL_LOGO->width = 0x20;
    CL_LOGO->pivot_x = 0x10;
    CL_LOGO->scale_x = 0x1000;
    CL_LOGO->scale_y = 0x1000;
    CL_LOGO->pivot_y = 0x28;
    CL_LOGO->ot_pri = 5;
}

void computer_logo_wedge_b_wait_gap(void) {
    if (CL_LOGO_PARENT->angle == (CL_LOGO->angle + 0x2D)) {
        CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
    }
}

void computer_logo_wedge_b_spin(void) {
    u8 *obj;
    u8 *t;
    s32 v;

    obj = g_ClLogoCur;
    t = ((u8 *) CLLOGO(obj)->parent);
    v = CLLOGO(obj)->angle + 0xF;
    CLLOGO(obj)->angle = v;
    if (v == 0x168) {
        CLLOGO(obj)->angle = 0;
    }
    if (CLLOGO(t)->phase_signal == 1) {
        CLLOGO(g_ClLogoCur)->substep++;
    }
}

void computer_logo_wordmark(void) {
    extern s32 (*computer_logo_wordmark_step_tbl[])();
    computer_logo_wordmark_step_tbl[CL_LOGO->substep]();
}

void computer_logo_wordmark_init(void) {
    CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
    CL_LOGO->tex_u = 0x18;
    CL_LOGO->tex_v = 0;
    CL_LOGO->height = 0x18;
    CL_LOGO->scale_x = 0x1000;
    CL_LOGO->scale_y = 0x1000;
    CL_LOGO->x = 0x3B0000;
    CL_LOGO->y = 0xFFC90000;
    CL_LOGO->width = 0;
    CL_LOGO->pivot_x = 0;
    CL_LOGO->pivot_y = 0;
    CL_LOGO->angle = 0;
    CL_LOGO->ot_pri = 4;
}

void computer_logo_wordmark_wipe_in(void) {
    s32 x;
    s32 xi;

    x = CL_LOGO->x + 0xFFFEE000;
    CL_LOGO->x = x;
    xi = x >> 0x10;
    CL_LOGO->width = (s16) (0x3B - xi);
    if (xi < -0x1E) {
        CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
        CL_LOGO->x = -0x1E0000;
    }
}

void computer_logo_wordmark_wait_ring(void) {
    if (CL_LOGO_PARENT->phase_signal == 2) {
        CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
        CL_LOGO->x = 0xFFCA0000;
        CL_LOGO->width = 0x70;
        CL_LOGO->tex_u = 0;
        CL_LOGO->timer = 0x5A;
    }
}

void computer_logo_wordmark_hold(void) {
    s16 timer;
    s16 left;

    timer = ((s16) CL_LOGO->timer);
    left = timer - (timer > 0);
    CL_LOGO->timer = left;
    if (((left << 0x10) <= 0) && !(g_GameStateFlags & GAME_VOICE_PLAYING)) {
        CL_LOGO->substep = (u8) (CL_LOGO->substep + 1);
        CL_LOGO->width = 0x70;
        CL_LOGO->x = 0x20000;
        CL_LOGO->y = (s32) (CL_LOGO->y + 0xC0000);
        CL_LOGO->tex_u = 0;
        CL_LOGO->pivot_x = 0x38;
        CL_LOGO->pivot_y = 0xC;
        CL_LOGO->scale_x = 0x1000;
        CL_LOGO->scale_y = 0x1000;
    }
}

void computer_logo_wordmark_shrink(void) {
    u8 *p;
    s16 x;
    u16 y;
    s32 z;

    p = g_ClLogoCur;
    if (CLLOGO(p)->scale_x >= 0xAAB) {
        CLLOGO(p)->scale_x -= 0x32;
        CLLOGO(p)->scale_y -= 0x32;
    } else {
        CLLOGO(p)->scale_x = 0xAAA;
        CLLOGO(p)->scale_y = 0xAAA;
    }
    z = CL_LOGO->y + 0x10000;
    CL_LOGO->y = z;
    if ((z >> 0x10) >= -0x12) {
        CL_LOGO->y = -0x120000;
        CL_LOGO->active = 0;
        g_ClIntroLogoState = 2;
    }
}

s32 *computer_logo_alloc(void) {
  unsigned int one;
  s8 *slot;
  s32 i;
  slot = (s8 *)&g_ClLogoSlots;
  i = 0;
  one = 1U;
  loop_1:
  if ((*((u8 *) (((s8 *) slot) + 0))) != 0)
  {
    i += 1;
    slot += 0x30;
    if (((u32) (i & 0xFF)) >= 9U)
    {
      return 0;
    }
    goto loop_1;
    return (s32 *)1;
  }

  *((u8 *) (((s8 *) slot) + 0)) = one;
  *((s8 *) (((s8 *) slot) + 2)) = 0;
  return (s32 *)slot;
}

void lab_slides_display(void) {
    extern s32 (*lab_slides_state_tbl[])();
    extern u8 g_InteractiveScreenState_u8 __asm__("g_InteractiveScreenState");
    if (g_UpdateFlags & 0x80) {
        lab_slides_state_tbl[g_InteractiveScreenState_u8]();
    }
}

void lab_slides_start(void) {
    s32 bgm_track_play();
    s32 bgm_track_stop();
    s32 bgm_track_set_volume();
    extern s8 g_InteractiveScreenState_s8 __asm__("g_InteractiveScreenState");
    extern s8 g_LabSlidesAnimState_s8 __asm__("g_LabSlidesAnimState");
    extern s32 g_LabSlidesScrollX_s32 __asm__("g_LabSlidesScrollX");
    extern s8 g_LabSlidesSlideIndex_s8 __asm__("g_LabSlidesSlideIndex");
    extern s8 g_LabSlidesLoopDone_s8 __asm__("g_LabSlidesLoopDone");
    extern s8 g_LabSlidesMsgId_s8 __asm__("g_LabSlidesMsgId");
    extern s8 g_LabSlidesCountdown_s8 __asm__("g_LabSlidesCountdown");
    u8 cut;
    u16 *update_flags = LITP(u16, g_UpdateFlags);

    g_InteractiveScreenState_s8 = 1;
    g_LabSlidesAnimState_s8 = 0;
    g_VSyncMode = 0;
    *update_flags &= 0xFEFF;
    cut = LIT(u8, g_CutId);
    LIT(u8, g_CutId) = 4;
    g_SavedCutId = cut;
    display_room_camera_bg();
    g_LabSlidesSlideIndex_s8 = 0;
    g_LabSlidesLoopDone_s8 = 0;
    g_LabSlidesMsgId_s8 = 0;
    g_LabSlidesCountdown_s8 = 0x10;
    bgm_track_stop(0);
    bgm_track_play(1);
    bgm_track_set_volume(2, 0x14, 0x14);
    bgm_track_play(2);
    g_LabSlidesScrollX_s32 = -0x60;
    g_LabSlidesScrollY = -0x42;
}

void lab_slides_update(void) {
    extern s32 g_LabSlidesScrollX_s __asm__("g_LabSlidesScrollX");
    s32 set_message_display_a10(u8, s32) __asm__("set_message_display");
    s32 lab_slides_draw_a1() __asm__("lab_slides_draw");
    s32 play_sfx_a12(s32, s32, s32) __asm__("play_sfx");
    extern s8 g_InteractiveScreenState_s8 __asm__("g_InteractiveScreenState");
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");
    s32 scroll_in;
    s32 scroll_out;

    switch (g_LabSlidesAnimState[0]) {
    case 0:
        g_LabSlidesCountdown--;
        if (g_LabSlidesCountdown == 0) {
            g_LabSlidesAnimState[0] = 1;
            g_LabSlidesScrollX[0] = 0x66;
        case 1:
            scroll_in = g_LabSlidesScrollX[0] - 0x12;
            g_LabSlidesScrollX[0] = scroll_in;
            if (scroll_in == 0x1E) {
                play_sfx_a12(2, 0x1B, 0);
            }
            if (g_LabSlidesScrollX[0] == -0x60) {
                g_LabSlidesAnimState[0] = 2;
                set_message_display_a10(g_LabSlidesMsgId[0], 0);
            }
        }
        break;
    case 2:
        if (!(g_MessageStatus_u8 & 0x80)) {
            g_LabSlidesAnimState[0] = 3;
            play_sfx_a12(2, 0x1A, 0);
        }
        break;
    case 3:
        scroll_out = g_LabSlidesScrollX_s - 0x12;
        g_LabSlidesScrollX_s = scroll_out;
        if (scroll_out < -0x127) {
            g_LabSlidesAnimState[0] = 0;
            g_LabSlidesCountdown = 0x10;
            if ((u8) g_LabSlidesSlideIndex < 5U) {
                g_LabSlidesSlideIndex += 1;
            } else if (g_LabSlidesLoopDone != 0) {
                g_InteractiveScreenState_s8 = 2;
            } else {
                g_LabSlidesLoopDone = 1;
            }
            g_LabSlidesMsgId[0] += 1;
        }
        break;
    }
    lab_slides_draw_a1();
}

typedef struct {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} LabSlideRect;

/* g_LabSlidesScrollX (0x800B9D24), read like the original: through the state byte's symbol + 3 (its own
   symbol compiles differently) */
#define LAB_SCROLL_X(T) (*(T *)(g_LabSlidesAnimState + 3))

void lab_slides_draw(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern LabSlideRect g_BgmBankFiles_l[] __asm__("g_BgmBankFiles");
    LabSlideRect *p;
    u32 t;

    SCRATCH->rect_flags = 0x60000000;
    p = g_BgmBankFiles_l;
    SCRATCH->rect_r = 0x18;
    SCRATCH->rect_g = 0x18;
    SCRATCH->rect_b = 0x18;
    do {
        SCRATCH->rect_h = p[-1].h;
        SCRATCH->rect_w = p[-1].w;
        SCRATCH->rect_y = p[-1].y;
        SCRATCH->rect_x = p[-1].x;
        p--;
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
    } while (g_BgmBankFiles_l - 4 < p);
    if (g_LabSlidesAnimState[0] != 0) {
        SCRATCH->rect_y = -0x42;
        SCRATCH->rect_h = 0x7F;
        SCRATCH->spr_u = 0;
        if ((u32)(LAB_SCROLL_X(s32) + 0x67) < 0xE) {
            SCRATCH->rect_w = 0xBF;
            SCRATCH->rect_x = LAB_SCROLL_X(u16);
        } else if (g_LabSlidesAnimState[0] != 1) {
            SCRATCH->rect_x = -0x67;
            SCRATCH->rect_w = LAB_SCROLL_X(u16) + 0x126;
            SCRATCH->spr_u = -0x67 - LAB_SCROLL_X(s32);
        } else {
            SCRATCH->rect_x = LAB_SCROLL_X(u16);
            SCRATCH->rect_w = 0x66 - LAB_SCROLL_X(u16);
        }
        if (g_LabSlidesLoopDone != 0 || g_LabSlidesSlideIndex != 5) {
            /* Statement order chosen for GCC 2.6.3 scheduling/allocation (all independent). */
            SCRATCH->spr_y = ((u16) SCRATCH->rect_y);
            SCRATCH->rect_r = 0x80;
            SCRATCH->rect_g = 0x80;
            SCRATCH->rect_b = 0x80;
            SCRATCH->spr_flags = 0x41000040;
            t = (g_LabSlidesSlideIndex >> 1) * 0xC0;
            SCRATCH->spr_x = ((u16) SCRATCH->rect_x);
            SCRATCH->spr_u += t & 0x7F;
            SCRATCH->spr_v = (g_LabSlidesSlideIndex & 1) << 7;
            SCRATCH->spr_w = ((u16) SCRATCH->rect_w);
            SCRATCH->spr_h = ((u16) SCRATCH->rect_h);
            SCRATCH->spr_clut_x = 0;
            SCRATCH->spr_clut_y = g_LabSlidesSlideIndex + 0x1ED;
            SCRATCH->spr_tpage = (t >> 7) + 9;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
        } else {
            SCRATCH->rect_r = 0x40;
            SCRATCH->rect_g = 0x40;
            SCRATCH->rect_b = 0x40;
        }
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
    }
}

void lab_slides_finish(void) {
    void display_room_camera_bg();
    s32 bgm_track_play(s32);
    s32 bgm_track_stop(s32);
    g_CutId = g_SavedCutId;
    display_room_camera_bg();
    Flg_off(ADDR_g_ScenarioFlags, 0x20);
    bgm_track_stop(1);
    bgm_track_stop(2);
    bgm_track_play(0);
    g_VSyncMode = 2;
    LIT(u16, g_UpdateFlags) |= 0x100;
    LIT(s32, g_GameStateFlags) &= ~GAME_SCREEN_DIM;
}

void set_bg_image_buffer_params(s32 buf_a, s32 buf_b, s32 mode) {
    g_BgImageBufA = buf_a;
    g_BgImageBufB = buf_b;
    g_BgImageMode = mode;
}

void bg_image_upload(s16 y) {
    int LoadImage(RECT *rect, u_long *p);
    s16 dst_rect[4];
    u32 *pixels;

    pixels = LITP(u32, g_DoorFile);
    if (g_BgImageMode == 1) {
        pixels = LITP(u32, g_DoorFile_TimPixels);
    }
    dst_rect[2] = 0x140;
    dst_rect[1] = y;
    dst_rect[0] = 0;
    dst_rect[3] = 0xF0;
    LoadImage((RECT *) dst_rect, (u_long *)pixels);
}

/* rodata padding up to the next file's original start (tools/splitc.py --pad) */
__asm__(".section .rodata\n\t.space 2\n\t.text");
