/* clut_fade.c - CLUT (palette) fades: capture, step and restore the colour tables of the screen. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 room_bg_verify_checksum(void);


typedef struct { u8 a, vol, b, c, d, e; s16 f; u16 g; } ClutFadeView;

void clut_fade_reset(void) {
    extern ClutFadeView g_ClutFadeEntries_Rec156EC[4] __asm__("g_ClutFadeEntries");
    ClutFadeView *p;
    u8 i;

    i = 0;
    p = g_ClutFadeEntries_Rec156EC;
    g_ClutFadeSrc = 0;
    g_ClutFadeDst = 0;
    g_ClutFadeCount = 0;
    g_ClutFadeRows = 0;
    do {
        p->c = 0;
        p->d = 0;
        p->e = 0;
        p->f = 0;
        p->g = 0x100;
        p->vol = 0;
        p++;
        i++;
    } while (i < 4);
}

void clut_fade_capture(s32 row_end) {
    extern u16 g_ClutFadeRows_u16 __asm__("g_ClutFadeRows");
    extern u32 * g_ClutFadeSrc_u32p __asm__("g_ClutFadeSrc");
    extern u32 * g_ClutFadeDst_u32p __asm__("g_ClutFadeDst");
    int DrawSync(int mode);
    int StoreImage(RECT *rect, u_long *p);
    RECT r;
    u8 *buf;
    u16 i;

    if (g_ClutFadeCount != 0) {
        g_ClutFadeRows_u16 = row_end - 0xA;
        r.x = 0;
        r.y = 0x1EA;
        buf = LIT(u8 *, g_LoadPointer);
        r.w = 0x100;
        g_ClutFadeSrc_u32p = (u32 *)buf;
        g_ClutFadeDst_u32p = (u32 *)((g_ClutFadeRows_u16 << 9) + (s32)buf);
        LIT(s32, g_LoadPointer) = (g_ClutFadeRows_u16 << 10) + (s32)buf;
        r.h = g_ClutFadeRows_u16;
        StoreImage(&r, (u_long *)buf);
        DrawSync(0);
        for (i = 0; i < (g_ClutFadeRows_u16 << 7); i++) {
            g_ClutFadeDst_u32p[i] = g_ClutFadeSrc_u32p[i];
        }
    }
}

s32 clut_fade_add(u32 row, s8 id) {
    s32 idx;

    if ((u16) g_ClutFadeCount >= 4U) {
        return -1;
    }
    g_ClutFadeEntries[g_ClutFadeCount].x0 = id;
    g_ClutFadeEntries[g_ClutFadeCount].row = row + 0xF6;
    idx = g_ClutFadeCount << 0x18;
    g_ClutFadeCount += 1;
    return idx >> 0x18;
}

typedef struct {
    u8 model_id;
    u8 active;
    u8 row;
    u8 dr;
    u8 dg;
    u8 db;
    u16 start;
    u16 count;
} PalFade;

void clut_fade_update(void) {
    int LoadImage(RECT *rect, u_long *p);
    extern PalFade g_ClutFadeEntries_l[] __asm__("g_ClutFadeEntries");
    extern u8 *g_ClutFadeSrc_l __asm__("g_ClutFadeSrc");
    extern u8 *g_ClutFadeDst_l __asm__("g_ClutFadeDst");
    extern u16 g_ClutFadeRows_l __asm__("g_ClutFadeRows");
    RECT rc;
    u16 changed;
    u16 i;
    u16 j;
    u16 *src;
    u16 *dst;
    u16 c;
    s16 r;
    s16 g;
    s16 b;
    PalFade *e;
    s8 d;
    s8 d1; /* own variable for the red step: a block-local pseudo (4 qtys -> real qsort in local-alloc) */
    s32 row;

    changed = 0;
    e = g_ClutFadeEntries_l;
    if (g_ClutFadeCount != 0) {
        for (i = 0; i < 4; i++, e++) {
            if (e->active) {
                j = e->start;
                changed = 1;
                e->active = 0;
                row = e->row << 9;
                src = (u16 *)(row + (s32)g_ClutFadeSrc_l);
                dst = (u16 *)(row + (s32)g_ClutFadeDst_l);
                for (; j < e->start + e->count; j++) {
                    c = src[j];
                    if (c != 0) {
                        do {
                            g = c & 0x3E0;
                            b = c & 0x7C00;
                        } while (0);
                        d1 = e->dr;
                        r = c & 0x1F;
                        r += d1;
                        if (r & ~0x1F) {
                            r = (d1 > 0) ? 0x1F : 1;
                        }
                        d = e->dg;
                        g += d << 5;
                        if (g & ~0x3E0) {
                            g = (d > 0) ? 0x3E0 : 1;
                        }
                        d = e->db;
                        b += d << 10;
                        if (b & ~0x7C00) {
                            b = (d > 0) ? 0x7C00 : 1;
                        }
                        dst[j] = r | (b | g) | (c & 0x8000);
                    }
                }
            }
        }
        if (changed) {
            rc.x = 0;
            rc.y = 0x1EA;
            rc.w = 0x100;
            rc.h = g_ClutFadeRows_l;
            LoadImage(&rc, (u_long *)g_ClutFadeDst_l);
        }
    }
}

void clut_fade_add_delta(s32 dr, s32 dg, s32 db, s16 start, u16 count, u8 id) {
    extern u8 g_ClutFadeEntries_u8[] __asm__("g_ClutFadeEntries");
    u8 i;
    u8 *r;

    for (r = g_ClutFadeEntries_u8, i = 0; i < 4; r += 10, i++) {
        if (FIELD_AT(r, ClutFadeEntry, ->x0, u8) == id) {
            FIELD_AT(r, ClutFadeEntry, ->start, s16) = start;
            FIELD_AT(r, ClutFadeEntry, ->count, u16) = count;
            FIELD_AT(r, ClutFadeEntry, ->dr, u8) = dr + FIELD_AT(r, ClutFadeEntry, ->dr, u8);
            FIELD_AT(r, ClutFadeEntry, ->dg, u8) = dg + FIELD_AT(r, ClutFadeEntry, ->dg, u8);
            FIELD_AT(r, ClutFadeEntry, ->db, u8) = db + FIELD_AT(r, ClutFadeEntry, ->db, u8);
            if ((s8)r[3] < -0x1F) {
                FIELD_AT(r, ClutFadeEntry, ->dr, s8) = -0x1F;
            }
            if ((s8)r[3] >= 0x20) {
                FIELD_AT(r, ClutFadeEntry, ->dr, u8) = 0x1F;
            }
            if ((s8)r[4] < -0x1F) {
                FIELD_AT(r, ClutFadeEntry, ->dg, s8) = -0x1F;
            }
            if ((s8)r[4] >= 0x20) {
                FIELD_AT(r, ClutFadeEntry, ->dg, u8) = 0x1F;
            }
            if ((s8)r[5] < -0x1F) {
                FIELD_AT(r, ClutFadeEntry, ->db, s8) = -0x1F;
            }
            if ((s8)r[5] >= 0x20) {
                FIELD_AT(r, ClutFadeEntry, ->db, u8) = 0x1F;
            }
            FIELD_AT(r, ClutFadeEntry, ->active, u8) = 1;
            return;
        }
    }
}

void clut_fade_set(s8 dr, s8 dg, s8 db, s16 start, u16 count, u8 id) {
  s32 i;
  u8 *e;
  int one;
  u8 *active_p;
  e = (u8 *)g_ClutFadeEntries;
  i = 0;
  one = 1;
  active_p = (u8 *)g_ClutFadeEntries + one;
  loop_1:
  if ((*e) == id)
  {
    *((s8 *) (((s8 *) active_p) + 2)) = dr;
    *((s8 *) (((s8 *) active_p) + 3)) = dg;
    *((s8 *) (((s8 *) active_p) + 4)) = db;
    *((s16 *) (((s8 *) active_p) + 5)) = start;
    *((u16 *) (((s8 *) active_p) + 7)) = count;
    *((u8 *) (((s8 *) active_p) + 0)) = one;
    return;
  }

  active_p += 0xA;
  i += 1;
  e += 0xA;
  if (((u32) (i & 0xFF)) >= 4U)
  {
 do { } while (0);
    return;
  }
  goto loop_1;
}

typedef struct { u8 a, vol, b, c, d, e; s16 f; u16 g; } ClutFadeView2;

void clut_fade_clear(u8 id, s16 start, s16 count) {
    extern ClutFadeView2 g_ClutFadeEntries_Rec15C08[4] __asm__("g_ClutFadeEntries");
    ClutFadeView2 *p;
    u8 i;

    for (p = g_ClutFadeEntries_Rec15C08, i = 0; i < 4; p++, i++) {
        if (p->a == id) {
            p->c = 0;
            p->d = 0;
            p->e = 0;
            p->f = start;
            p->g = count;
            p->vol = 1;
            return;
        }
    }
}
