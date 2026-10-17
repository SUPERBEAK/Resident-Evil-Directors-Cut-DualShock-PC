/* pad.c - Controller input: key bindings, pad_update (raw buttons -> game action bits g_PadHeld / g_PadPressed,
   analog stick turning) and the vibration lists. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


void pad_set_default_bindings(void) {
    g_PadHeld = 0;
    g_KeyConfig[0] = 0x1000;
    g_KeyConfig[1] = 0x2000;
    g_KeyConfig[2] = 0x4000;
    g_KeyConfig[3] = 0x8000;
    g_KeyConfig[4] = 0x1000;
    g_KeyConfig[5] = 0x4000;
    g_KeyConfig[6] = 0x80;
    g_KeyConfig[7] = 0x80;
    g_KeyConfig[8] = 8;
    g_KeyConfig[9] = 0x20;
    g_KeyConfig[10] = 8;
    g_KeyConfig[11] = 4;
    g_KeyConfig[12] = 0x20;
    g_KeyConfig[13] = 0x10;
    g_KeyConfig[14] = 0x40;
    g_KeyConfig[15] = 0x80;
}

void pad_set_default_bindings_b(void) {
    g_PadHeld = 0;
    g_KeyConfig[0] = 0x1000;
    g_KeyConfig[1] = 0x2000;
    g_KeyConfig[2] = 0x4000;
    g_KeyConfig[3] = 0x8000;
    g_KeyConfig[4] = 0x1000;
    g_KeyConfig[5] = 0x4000;
    g_KeyConfig[6] = 0x80;
    g_KeyConfig[7] = 0x40;
    g_KeyConfig[8] = 8;
    g_KeyConfig[9] = 0x20;
    g_KeyConfig[10] = 8;
    g_KeyConfig[11] = 4;
    g_KeyConfig[12] = 0x20;
    g_KeyConfig[13] = 0x10;
    g_KeyConfig[14] = 0x40;
    g_KeyConfig[15] = 0x80;
}

void pad_update(void) {
    int PadGetState(int port);
    int PadInfoMode(int port, int term, int offs);
    void PadSetAct(int port, u_char *table, int len);
    int PadSetActAlign(int port, u_char *table);
    extern s16 g_PlayerTurnSpeedStand_l __asm__("g_PlayerTurnSpeedStand");
    extern s16 g_PlayerTurnSpeedWalk_l __asm__("g_PlayerTurnSpeedWalk");
    extern u8 g_VibrationStrong_l[] __asm__("g_VibrationStrong");
    extern u8 g_VibrationWeak_l __asm__("g_VibrationWeak");
    extern u16 g_DemoTimer_a[] __asm__("g_DemoTimer");
    extern u8 g_VibrationSmallList_l[] __asm__("g_VibrationSmallList");
    extern u8 g_VibrationLargeList_l[] __asm__("g_VibrationLargeList");
    extern u8 g_OpenMenuFlag_l __asm__("g_OpenMenuFlag");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    extern u32 g_PadRawPressed32_l __asm__("g_PadRawPressed32");
    u8 pad_vibration_update_list_l() __asm__("pad_vibration_update_list");
    u32 state;
    u8 mode2;
    s32 ax;
    u8 ay;
    u8 flag;
    u8 force;
    u8 a;
    u8 b;
    u16 bits;
    u16 mask;
    u8 i;
    u16 *tbl;
    u16 btn;
    u16 btn2;
    u16 cur;
    u32 nw;
    s32 chk;
    u32 *pa;

    force = 0;
    if (g_PadBuf1 == 0xFF) {
        g_PadBuf1Btn0 = g_PadBuf1Btn1 = 0xFF;
    }
    if (g_PadBuf2 == 0xFF) {
        g_PadBuf2Btn0 = g_PadBuf2Btn1 = 0xFF;
    }
    state = PadGetState(0);
    if (state >= 2) {
        g_PadType = PadInfoMode(0, 1, 0);
        mode2 = PadInfoMode(0, 2, 0);
        if (g_PadTypePrev != g_PadType) {
            g_PadActAlignState = 0;
        }
        if (g_PadType != 4) {
            if (g_PadType != 7) {
                goto reset;
            }
            ax = g_PadBuf1StickX;
            ay = g_PadBuf1StickY;
        }
        if (g_PadActAlignState == 0) {
            PadSetAct(0, g_VibrationStrong_l, 2);
            if (state == 2) {
                g_PadActAlignState = 1;
            } else if (state == 6 && PadSetActAlign(0, g_PadActAlign) != 0) {
                g_PadActAlignState = 2;
            }
        }
    } else {
    reset:
        g_PadBuf1Btn1 = 0xFF;
        g_PadBuf1Btn0 = 0xFF;
        g_PadBuf2Btn1 = 0xFF;
        g_PadBuf2Btn0 = 0xFF;
        g_PadActAlignState = 0;
        g_PadType = 0;
    }
    g_VibrationWeak_l = 0;
    g_VibrationStrong_l[0] = 0;
    g_PadTypePrev = g_PadType;
    if (mode2 != 0) {
        if ((g_GameStateFlags & GAME_OPTIONS_REQUEST) ||
            ((!(g_MessageStatus_l & 0x80) || (g_GameStateFlags & GAME_SCREEN_DIM)) && !(g_StatusFlags & STATUS_ATTRACT_DEMO) &&
             !(g_StatusFlags & STATUS_BG_LOADING) && g_OpenMenuFlag_l == 0)) {
            g_VibrationStrong_l[0] = pad_vibration_update_list_l(g_VibrationSmallList_l);
            g_VibrationWeak_l = pad_vibration_update_list_l(g_VibrationLargeList_l);
        }
    } else {
        if ((g_GameStateFlags & GAME_OPTIONS_REQUEST) ||
            ((!(g_MessageStatus_l & 0x80) || (g_GameStateFlags & GAME_SCREEN_DIM)) && !(g_StatusFlags & STATUS_ATTRACT_DEMO) &&
             !(g_StatusFlags & STATUS_BG_LOADING) && g_OpenMenuFlag_l == 0)) {
            a = pad_vibration_update_list_l(g_VibrationSmallList_l);
            b = pad_vibration_update_list_l(g_VibrationLargeList_l);
            if (a != 0 || b != 0) {
                g_VibrationStrong_l[0] = 0x40;
                g_VibrationWeak_l = 0xFF;
            }
        }
    }
    bits = 0;
    if (LIT(u8, g_KeyConfigType) & 0x20) {
        g_VibrationWeak_l = 0;
        g_VibrationStrong_l[0] = 0;
    }
    mask = 0x8000;
    i = 16;
    tbl = g_KeyConfigPresets[LIT(u8, g_KeyConfigType) & 3];
    btn = g_PadHeldRaw;
    do {
        i--;
        if (btn & tbl[i]) {
            bits |= mask;
        }
        mask >>= 1;
    } while (i != 0);
    if (bits & 1) {
        bits &= 0xFFFB;
    }
    if (bits & 2) {
        bits &= 0xFFF7;
    }
    g_PadHeldPrev = g_PadHeld;
    g_PadRawPressed32_l = LIT(u32, g_PadRaw32);
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        if ((g_UpdateFlags & 0x200) && g_DemoTimer_a[0] != 0) {
            LIT(u32, g_PadRaw32) = g_DemoTimer_a[g_DemoTimer_a[0] + 17];
        } else {
            g_PadRaw32 = 0;
        }
    } else {
        LIT(u32, g_PadRaw32) = ~(g_PadBuf1Btn1 | ((g_PadBuf2Btn0 << 24) + (g_PadBuf2Btn1 << 16) + (g_PadBuf1Btn0 << 8)));
    }
    if (!(g_StatusFlags & STATUS_ATTRACT_DEMO)) {
        if (g_PadType == 7) {
            g_AnalogTurnLevel = 0;
            flag = 0;
            if (ay < 0x30) {
                pa = LITP(u32, g_PadRaw32);
                *pa |= 0x1000;
            }
            if (ay > 0xD0) {
                pa = LITP(u32, g_PadRaw32);
                *pa |= 0x4000;
            }
            if ((u8)ax < 0x50) {
                if ((u8)ax != 0) {
                    ax = 0x50 - ax;
                    g_AnalogTurnLevel = (ax & 0xFF) / 8;
                } else {
                    g_AnalogTurnLevel = 10;
                }
                flag |= 1;
                pa = LITP(u32, g_PadRaw32);
                *pa |= 0x8000;
            }
            if ((u8)ax > 0xB0) {
                if ((u8)ax < 0xFF) {
                    ax += 0x50;
                    g_AnalogTurnLevel = (ax & 0xFF) / 8;
                } else {
                    g_AnalogTurnLevel = 10;
                }
                flag |= 1;
                pa = LITP(u32, g_PadRaw32);
                *pa |= 0x2000;
            }
            if (g_PadHeldRaw & PSX_R3) {
                pa = LITP(u32, g_PadRaw32);
                *pa |= 0x800;
            }
        }
    } else {
        force = 1;
    }
    g_PadHeld = 0;
    nw = (LIT(u32, g_PadRawPressed32) ^ *(pa = LITP(u32, g_PadRaw32))) & LIT(u32, g_PadRaw32);
    cur = *(u16 *)pa;
    mask = 0x8000;
    i = 16;
    LIT(u32, g_PadRawPressed32) = nw;
    btn2 = g_PadHeldRaw = cur;
    g_PadHeldRawHi = LIT(u32, g_PadRaw32) >> 16;
    g_PadTriggerHi = nw >> 16;
    g_PadTrigger = LIT(u16, g_PadRawPressed32);
    do {
        i--;
        if (btn2 & tbl[i]) {
            LIT(u16, g_PadHeld) |= mask;
        }
        mask >>= 1;
    } while (i != 0);
    chk = g_PadHeld & (PAD_RIGHT | PAD_LEFT);
    if (chk != 0) {
        force = 1;
    }
    if (!(g_StatusFlags & STATUS_ATTRACT_DEMO) && g_PadType == 7) {
        if (flag) {
            if ((u8)ax != 0xFF) {
                LIT(u32, g_PadRaw32) &= ~PSX_RIGHT;
                LIT(u32, g_PadRawPressed32) &= ~PSX_RIGHT;
                LIT(u16, g_PadHeldRaw) &= ~PSX_RIGHT;
                LIT(u16, g_PadTrigger) &= ~PSX_RIGHT;
            }
            if ((u8)ax != 0) {
                LIT(u32, g_PadRaw32) &= ~PSX_LEFT;
                LIT(u32, g_PadRawPressed32) &= ~PSX_LEFT;
                LIT(u16, g_PadHeldRaw) &= ~PSX_LEFT;
                LIT(u16, g_PadTrigger) &= ~PSX_LEFT;
            }
        } else if (chk != 0) {
            g_AnalogTurnLevel = 10;
        }
    }
    if (force) {
        g_PlayerTurnSpeedStand_l = 0x60;
        g_PlayerTurnSpeedWalk_l = 0x28;
        g_PlayerTurnSpeedRun = 0x30;
    } else {
        g_PlayerTurnSpeedStand_l = g_AnalogTurnSpeedTbl[g_AnalogTurnLevel] * 0x60 / 100;
        g_PlayerTurnSpeedWalk_l = ({ (s32)g_AnalogTurnSpeedTbl[g_AnalogTurnLevel]; }) >> 1;
        g_PlayerTurnSpeedRun = g_AnalogTurnSpeedTbl[g_AnalogTurnLevel] * 0x30 / 70;
    }
    if (LIT(u16, g_PadHeld) & PAD_FORWARD) {
        LIT(u16, g_PadHeld) &= ~PAD_BACK;
    }
    if (LIT(u16, g_PadHeld) & PAD_RIGHT) {
        LIT(u16, g_PadHeld) &= ~PAD_LEFT;
    }
    g_PadPressed = (bits & LIT(u16, g_PadHeld)) ^ LIT(u16, g_PadHeld);
}

void pad_vibrate_small(s32 duration, s32 delay) {
    u8 *pad_vibration_find_free();
    u32 pad_vibration_count_active(void *);
    extern u8 g_VibrationSmallList_u8[] __asm__("g_VibrationSmallList");
    s8 *p;

    if (pad_vibration_count_active(g_VibrationSmallList_u8) < 0x20 && duration != 0) {
        g_VibrationSmallListPtr = (s32 *)pad_vibration_find_free(g_VibrationSmallList_u8);
        VIB(g_VibrationSmallListPtr)->active = 1;
        p = (s8 *)g_VibrationSmallListPtr;
        VIB(p)->vol = (duration != 0);
        VIB(p)->count = duration;
        VIB(g_VibrationSmallListPtr)->delay = delay;
        VIB(g_VibrationSmallListPtr)->step = 0;
    }
}

void pad_vibrate_large(s32 duration, s32 vol, s32 delay) {
    u8 *pad_vibration_find_free();
    u32 pad_vibration_count_active(void *);
    s8 *entry;

    if ((pad_vibration_count_active(&g_VibrationLargeList) < 0x20U) && (duration != 0)) {
        entry = pad_vibration_find_free(&g_VibrationLargeList);
        g_VibrationLargeListPtr = (s32 *)entry;
        *entry = 1;
        VIB(g_VibrationLargeListPtr)->count = duration;
        VIB(g_VibrationLargeListPtr)->vol = vol;
        VIB(g_VibrationLargeListPtr)->delay = delay;
        VIB(g_VibrationLargeListPtr)->step = 0;
    }
}

s32 pad_vibrate_large_ramp(s32 duration, s32 vol_start, s32 vol_end, s32 delay) {
    u8 *pad_vibration_find_free();
    u32 pad_vibration_count_active();
    s8 *entry;

    if (((u32)pad_vibration_count_active(&g_VibrationLargeList) < 0x20U) && (duration != 0)) {
        entry = pad_vibration_find_free(&g_VibrationLargeList);
        g_VibrationLargeListPtr = (s32 *)entry;
        *entry = 1;
        VIB(g_VibrationLargeListPtr)->count = duration;
        VIB(g_VibrationLargeListPtr)->delay = delay;
        VIB(g_VibrationLargeListPtr)->acc = (s16) (vol_start << 7);
        VIB(g_VibrationLargeListPtr)->step = (s16) ((s32) ((vol_end - vol_start) << 7) / duration);
    }
}

void pad_vibration_reset(void) {
    s32 off;
    u32 i;

    i = 0;
    off = 0;
    do {
        *(u8 *)((u8 *)&g_VibrationSmallList + (off)) = 0;
        *(u8 *)((u8 *)&g_VibrationLargeList + (off)) = 0;
        off += 0xA;
        i += 1;
    } while (i < 0x21U);
    g_VibrationSmallListPtr = &g_VibrationSmallList;
    g_VibrationLargeListPtr = &g_VibrationLargeList;
    g_VibrationWeak = 0;
    g_VibrationStrong = 0;
}

typedef struct { u8 active; u8 vol; u16 delay; u16 count; s16 step; u16 acc; } VibrationView;

u8 pad_vibration_update_list(VibrationView *r) {
    u8 max = 0;
    u32 i = 0;
    u16 t;

    do {
        if (r->active != 0) {
            if (r->delay != 0) {
                r->delay--;
            } else {
                r->count--;
                if (r->count == 0) {
                    r->active = 0;
                }
                if (r->step != 0) {
                    t = r->acc + r->step;
                    r->acc = t;
                    r->vol = t >> 7;
                }
                if (max < r->vol) {
                    max = r->vol;
                }
            }
        }
        i++;
        r++;
    } while (i < 32);
    return max;
}

u8 *pad_vibration_find_free(u8 *list) {
    u32 i;
    u8 *p;

    p = list;
    i = 0;
loop_1:
    if (*p != 0) {
        i += 1;
        p += 0xA;
        if (i < 0x20U) {
            goto loop_1;
        }
    }
    return p;
}

u32 pad_vibration_count_active(void *list) {
    s32 n;
    u32 i;
    u8 *p;

    p = list;
    n = 0;
    i = 0;
    do {
        i += 1;
        if (*p != 0) {
            n += 1;
        }
        p += 0xA;
    } while (i < 0x20U);
    return n;
}

s32 pad_get_raw_buttons(void) {
    if (g_PadBuf1 != 0) {
        return 0;
    }
    return ~(g_PadBuf1Btn1 | (((g_PadBuf2Btn0 << 0x18) + (g_PadBuf2Btn1 << 0x10)) | (g_PadBuf1Btn0 << 8)));
}
