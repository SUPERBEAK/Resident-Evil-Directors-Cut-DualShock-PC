/* map.c - The MAP tab of the status menu: map browser and display, visited rooms, known areas, collected files. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void load_file_blocking(void);


s32 menu_map_state_machine(void) {
    void map_tab_update(void *ms);
    s32 map_display_update_a1(s32 *) __asm__("map_display_update");
    u8 *p;

    p = g_MapScreen;
    if ((g_PadPressed & PAD_CANCEL) && (g_MapScreen_ZoomState >= 7U)) {
        p[0] = 0;
    }
    switch (p[0]) {
    case 0:
        if (LIT(u16, g_PadPressed) & PAD_CANCEL) {
            *(s32 *)p = 0;
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            return -1;
        }
        map_tab_update(p);
        if (LIT(u16, g_PadPressed) & PAD_CONFIRM) {
            *(s32 *)p = 1;
        }
        return 0;
    case 1:
        map_display_update_a1((s32 *)p);
        if ((g_PadPressed & PAD_CONFIRM) && ((p[3] == 0) || (p[3] >= 7U))) {
            if (p[3] == 8) {
                p[3] = 3;
            }
            if (p[3] == 9) {
                p[3] = 2;
            }
            play_sfx(SFX_BANK_SYSTEM, 9, 0);
        }
        if (g_PadPressed & PAD_CANCEL) {
            p[3] = 7;
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
        }
        return 0;
    }
    return 0;
}

void map_tab_update(void *ms) {
    s32 map_tab_load_map_a1(void *) __asm__("map_tab_load_map");
    s32 map_tab_navigate_a1(void *) __asm__("map_tab_navigate");
    u8 substate;

    substate = MAPSCR(ms)->substate;
    switch (substate) {
    case 0:
        map_tab_load_map_a1(ms);
        MAPSCR(ms)->substate = (u8) (MAPSCR(ms)->substate + 1);
        return;
    case 1:
        map_tab_navigate_a1(ms);
        return;
    }
}

void map_tab_load_map(u8 *ms) {
    s32 load_file_blocking_p() __asm__("load_file_blocking");
    s32 map_update_layout_state_a1() __asm__("map_update_layout_state");
    s32 tim_upload_a2() __asm__("tim_upload");
    u32 i;
    u32 sel;
    u8 *cur;
    s32 bits;

    sel = 0;
    bits = ms[0xD];
    for (i = 0; i < 4; i++) {
        if ((bits >> i) & 1) {
            sel = i;
        }
    }
    cur = &g_MenuWork.loaded_res_id;
    if (*cur != sel + 0x80) {
        *cur = sel + 0x80;
        load_file_blocking_p(g_MapTabFiles[sel], ADDR_g_MenuTexBuf);
        g_MapUploadRect[0] = 0x01000140;
        g_MapUploadRect_WH = 0x880080;
        tim_upload_a2(ADDR_g_MenuTexBuf, g_MapUploadRect, 0x1C);
    }
    map_update_layout_state_a1(ms);
}

void map_tab_navigate(u8 *ms) {
    void map_update_layout_state(u8 *);
    s32 cur;

    if (!(MAPSCR(ms)->mode & 1)) {
        if ((g_PadTrigger & PSX_UP) && (MAPSCR(ms)->move_flags & 1)) {
            (ms + (s8)ms[4])[8]++;
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
        }
        if ((g_PadTrigger & PSX_DOWN) && (MAPSCR(ms)->move_flags & 2)) {
            (ms + (s8)ms[4])[8]--;
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
        }
        if (g_PadTrigger & PSX_LEFT) {
            if (MAPSCR(ms)->layout_mask & 8) {
                switch ((s8)ms[4]) {
                case 0:
                    MAPSCR(ms)->layout = 3;
                    break;
                case 1:
                    MAPSCR(ms)->layout = 2;
                    break;
                case 2:
                    MAPSCR(ms)->layout = 2;
                    break;
                case 3:
                    MAPSCR(ms)->layout = 1;
                    break;
                }
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            } else {
                cur = ((s8) MAPSCR(ms)->layout);
                if (cur != 2 && ((MAPSCR(ms)->layout_mask >> (cur + 1)) & 1)) {
                    MAPSCR(ms)->layout++;
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                }
            }
        }
        if (g_PadTrigger & PSX_RIGHT) {
            if (MAPSCR(ms)->layout_mask & 8) {
                switch ((s8)ms[4]) {
                case 1:
                    MAPSCR(ms)->layout = 3;
                    break;
                case 2:
                    MAPSCR(ms)->layout = 1;
                    break;
                case 0:
                case 3:
                    MAPSCR(ms)->layout = 0;
                    break;
                }
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            } else {
                cur = ((s8) MAPSCR(ms)->layout);
                if (cur != 0 && ((MAPSCR(ms)->layout_mask >> (cur - 1)) & 1)) {
                    MAPSCR(ms)->layout--;
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                }
            }
        }
    }
    map_update_layout_state(ms);
}

void map_update_layout_state(u8 *ms) {
    s32 map_area_known_l(s32) __asm__("map_area_known");
    void map_draw_browser(u8 *);
    s8 bit;
    u32 i;

    MAPSCR(ms)->area = g_MapAreaFloors[(s8)ms[4]][(ms + (s8)ms[4])[8]];
    MAPSCR(ms)->move_flags = 0;
    i = (ms + (s8)ms[4])[8];
    if (i + 1 != g_MapAreaFloorCount[(s8)ms[4]]) {
        bit = g_MapAreaFloors[(s8)ms[4]][i + 1];
        if (((((u16) MAPSCR(ms)->area_mask) >> bit) & 1) || map_area_known_l(bit) != 0) {
            MAPSCR(ms)->move_flags |= 1;
        }
    }
    if (i != 0) {
        bit = g_MapAreaFloors[(s8)ms[4]][i - 1];
        if (((((u16) MAPSCR(ms)->area_mask) >> bit) & 1) || map_area_known_l(bit) != 0) {
            MAPSCR(ms)->move_flags |= 2;
        }
    }
    for (i = 0; i < 4; i++) {
        if (i == (s8)ms[4]) {
            g_MapBrowserSprites[i + 1].r = g_MapBrowserSprites[i + 1].g = g_MapBrowserSprites[i + 1].b = 0x80;
        } else {
            g_MapBrowserSprites[i + 1].r = g_MapBrowserSprites[i + 1].g = g_MapBrowserSprites[i + 1].b = 0x40;
        }
    }
    g_MapBrowserSprites[0].r = g_MapBrowserSprites[0].g = g_MapBrowserSprites[0].b = 0x80;
    for (i = 5; i < 9; i++) {
        g_MapBrowserSprites[i].r = g_MapBrowserSprites[i].g = g_MapBrowserSprites[i].b = 0x80;
    }
    map_draw_browser(ms);
}

void map_draw_browser(u8 *ms) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 i;

    u8 t;

    GsSortFastSprite(&g_MapBrowserSprites[0], (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OtMain), 0x10);
    for (i = 0; i < 4; i++) {
        if ((ms[0xD] >> i) & 1) {
            GsSortFastSprite(&g_MapBrowserSprites[i + 1], (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OtMain), 0xF);
        }
    }
    g_MapBrowserSprites[5].v = (s8)ms[4] * 0x10 + 0x48;
    GsSortFastSprite(&g_MapBrowserSprites[5], (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain), 0xF);
    i = (s8)ms[4];
    g_MapBrowserSprites[6].v = 0x60 - (g_MapAreaFloorLabelBase[i] + (ms + i)[8]) * 0x10;
    GsSortFastSprite(&g_MapBrowserSprites[6], (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain), 0xF);
    t = ms[0x11] + 1;
    i = (t & 0x30) != 0; /* reuses i: a multiply-set pseudo gets no sched1 boost, so the store follows */
    ms[0x11] = t;
    if (ms[0xC] & 1) {
        g_MapBrowserSprites[7].u = i * 8 - 0x60;
        GsSortFastSprite(&g_MapBrowserSprites[7], (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain), 0xF);
    }
    if (ms[0xC] & 2) {
        g_MapBrowserSprites[8].u = i * 8 - 0x50;
        GsSortFastSprite(&g_MapBrowserSprites[8], (GsOT *)(LIT(u8, g_ActiveBuffer) * 0x14 + ADDR_g_OtMain), 0xF);
    }
}

void map_display_update(void *ms) {
    s32 map_draw_browser_a1() __asm__("map_draw_browser");
    s32 map_display_load_textures_a1() __asm__("map_display_load_textures");
    s32 map_display_animate_a1() __asm__("map_display_animate");
    s32 r_off;
    s8 *base;
    s32 i;
    s8 *spr;
    s8 c30;

    switch (MAPSCR(ms)->substate) {
    case 0:
        map_display_load_textures_a1(ms);
        MAPSCR(ms)->substate = (u8) (MAPSCR(ms)->substate + 1);
        break;
    case 1:
        map_display_animate_a1(ms);
        break;
    }
    i = 1;
    c30 = 0x30;
    base = &g_MapBrowserSprites[0].r;
    spr = base + 0x10;
    r_off = 0x24;
    g_MapBrowserSprites[0].b = 0x40;
    g_MapBrowserSprites[0].g = 0x40;
    *base = 0x40;
loop:
    ((GsSPRITE *)spr)->b = c30;
    ((GsSPRITE *)spr)->g = c30;
    spr += 0x24;
    (&g_MapBrowserSprites[0].r)[r_off] = c30;
    r_off += 0x24;
    i += 1;
    if (i < 9) {
        goto loop;
    }
    map_draw_browser_a1(ms);
}

void map_display_load_textures(u8 *ms) {
    extern u8 g_StageRoomFlagBase_u8[] __asm__("g_StageRoomFlagBase");
    extern RECT g_MapUploadRect_RECT __asm__("g_MapUploadRect");
    void load_file_blocking_l(u16, void *) __asm__("load_file_blocking");
    s32 map_area_known_l(s32) __asm__("map_area_known");
    void tim_upload_l(void *, RECT *, s32) __asm__("tim_upload");
    void map_display_animate(u8 *);
    u8 *flag = &g_MenuWork.loaded_res_id;
    s16 *p;
    s32 half;
    s32 n;
    u32 i;

    half = 0;
    if (*flag != (s8)ms[6] + 0x84) {
        *flag = (s8)ms[6] + 0x84;
        load_file_blocking_l(g_MapFloorFiles[(s8)ms[6]], LITP(void, g_MenuTexBuf));
        p = LITP(s16, g_MenuTexBuf_MapRoomClut);
        if (map_area_known_l((s8)ms[6]) == 0) {
            for (i = 6; i != 0; i--) {
                *p++ = 0;
            }
        } else {
            half = 1;
            p = LITP(s16, g_MenuTexBuf_MapRoomClutB);
        }
        for (i = 0; i < g_MapAreaRoomCount[g_MapFloorStage[(s8)ms[6]]]; i++) {
            if (Flg_ck(LITP(void, g_RoomFlags), i + g_StageRoomFlagBase_u8[g_MapFloorStage[(s8)ms[6]]]) == 0) {
                if (half) {
                    p++;
                    for (n = 3; n != 0; n--) {
                        *p++ = 0;
                    }
                    p += 2;
                } else {
                    for (n = 6; n != 0; n--) {
                        *p++ = 0;
                    }
                }
            } else {
                p += 6;
            }
        }
        g_MapUploadRect_RECT.x = 0x140;
        g_MapUploadRect_RECT.y = 0x188;
        g_MapUploadRect_RECT.w = 0x80;
        g_MapUploadRect_RECT.h = 0x70;
        tim_upload_l(LITP(void, g_MenuTexBuf), &g_MapUploadRect_RECT, 0x1F);
    }
    if ((MAPSCR(ms)->mode == 2 && MAPSCR(ms)->area == 0) || (MAPSCR(ms)->mode == 4 && MAPSCR(ms)->area == 6)) {
        MAPSCR(ms)->highlight_state = 1;
    } else {
        MAPSCR(ms)->highlight_state = 0;
    }
    map_display_animate(ms);
}

void map_display_animate(u8 *ms) {
    int LoadImage(RECT *rect, u_long *p);
    /* -O2: most blocks below addressed these sprite fields as separate symbols (g_MapSprites[i].f keeps
       &g_MapSprites in a register there and compiles differently); sprite 0's scale is read as s16
       through its address (that keeps the address in a register) */
    extern s16 g_MapSprites_0_X __asm__("g_MapSprites+0x04");
    extern s16 g_MapSprites_1_X __asm__("g_MapSprites+0x28");
    extern s16 g_MapSprites_2_X __asm__("g_MapSprites+0x4C");
    extern s16 g_MapSprites_1_Y __asm__("g_MapSprites+0x2A");
    extern s16 g_MapSprites_2_Y __asm__("g_MapSprites+0x4E");
    extern u16 g_MapSprites_0_W __asm__("g_MapSprites+0x08");
    extern u16 g_MapSprites_1_W __asm__("g_MapSprites+0x2C");
    extern u16 g_MapSprites_2_W __asm__("g_MapSprites+0x50");
    extern u16 g_MapSprites_0_ScaleX __asm__("g_MapSprites+0x1C");
    extern s16 g_MapSprites_1_ScaleX __asm__("g_MapSprites+0x40");
    extern s16 g_MapSprites_2_ScaleX __asm__("g_MapSprites+0x64");
    extern u16 g_MapSprites_0_ScaleY __asm__("g_MapSprites+0x1E");
    extern s16 g_MapSprites_1_ScaleY __asm__("g_MapSprites+0x42");
    extern s16 g_MapSprites_2_ScaleY __asm__("g_MapSprites+0x66");
    extern RECT g_MapUploadRect_l __asm__("g_MapUploadRect");
    s32 map_display_draw_l() __asm__("map_display_draw");

    if (g_MapFloorStage[MAPSCR(ms)->area] == MAPSCR(ms)->area_group) {
        switch (MAPSCR(ms)->blink_state) {
        case 0:
            g_MapBlinkLevel = 0x50;
            MAPSCR(ms)->blink_state++;
            break;
        case 1:
            if ((u8)(g_MapBlinkLevel = g_MapBlinkLevel + 2) > 0xA0) {
                MAPSCR(ms)->blink_state++;
            }
            break;
        case 2:
            if ((u8)(g_MapBlinkLevel = g_MapBlinkLevel - 2) < 0x50) {
                MAPSCR(ms)->blink_state = 1;
            }
            break;
        }
        g_MapBlinkColor = (g_MapBlinkLevel >> 3) | 0x8000;
        g_MapUploadRect_l.x = MAPSCR(ms)->room * 6 + 0x10;
        g_MapUploadRect_l.y = 0x1FF;
        g_MapUploadRect_l.w = 1;
        g_MapUploadRect_l.h = 1;
        LoadImage(&g_MapUploadRect_l, (u_long *)&g_MapBlinkColor);
    }
    switch (MAPSCR(ms)->zoom_state) {
    case 0:
        *(s16 *)&g_MapSprites_0_ScaleX = g_MapSprites_1_ScaleX = g_MapSprites_2_ScaleX = 1;
        *(s16 *)&g_MapSprites_0_ScaleY = g_MapSprites_1_ScaleY = g_MapSprites_2_ScaleY = 200;
        *(u32 *)&g_MapSprites &= ~0x10000000;
        MAPSCR(ms)->zoom_state++;
        play_sfx(SFX_BANK_SYSTEM, 9, 0);
    case 1:
        g_MapSprites[0].y = 2;
        g_MapSprites[0].w = 0xA1;
        g_MapSprites[1].y = 0;
        g_MapSprites[2].y = 0;
        g_MapSprites[0].x = 0;
        g_MapSprites[1].x = 0;
        g_MapSprites[2].x = 0;
        g_MapSprites[2].w = 0x51;
        g_MapSprites[1].w = 0x51;
        MAPSCR(ms)->zoom_state++;
        break;
    case 2:
        *(s16 *)&g_MapSprites_0_ScaleX *= 3;
        g_MapSprites_1_ScaleX *= 3;
        g_MapSprites_2_ScaleX *= 3;
        if (*(s16 *)&g_MapSprites_0_ScaleX >= 0xFFF) {
            *(s16 *)&g_MapSprites_0_ScaleX = g_MapSprites_1_ScaleX = g_MapSprites_2_ScaleX = 0xFFF;
            MAPSCR(ms)->zoom_state++;
        }
        break;
    case 3:
        *(s16 *)&g_MapSprites_0_ScaleY += 900;
        g_MapSprites_1_ScaleY += 900;
        g_MapSprites_2_ScaleY += 900;
        if (*(s16 *)&g_MapSprites_0_ScaleY >= 0x15DC) {
            *(s16 *)&g_MapSprites_0_ScaleY = g_MapSprites_1_ScaleY = g_MapSprites_2_ScaleY = 0x15DC;
            MAPSCR(ms)->zoom_state++;
        }
        break;
    case 4:
        *(s16 *)&g_MapSprites_0_ScaleY -= 300;
        g_MapSprites_1_ScaleY -= 300;
        g_MapSprites_2_ScaleY -= 300;
        if (*(s16 *)&g_MapSprites_0_ScaleY <= 0x1000) {
            g_MapSprites[0].w = 0xA0;
            g_MapSprites[2].w = 0x50;
            g_MapSprites[1].w = 0x50;
            *(s16 *)&g_MapSprites_0_ScaleY = g_MapSprites_1_ScaleY = g_MapSprites_2_ScaleY = 0x1000;
            g_MapSprites[0].scalex = g_MapSprites[1].scalex = g_MapSprites[2].scalex = 0x1000;
            g_MapSprites[0].x = 0x70;
            g_MapSprites[1].x = 0x70;
            g_MapSprites[2].x = 0x70;
            g_MapSprites[1].y = 0x4C;
            g_MapSprites[2].y = 0x4C;
            g_MapSprites[0].y = 0x4E;
            g_MapBlinkTimer = 8;
            MAPSCR(ms)->zoom_state++;
        }
        break;
    case 5:
        if (--g_MapBlinkTimer == 0) {
            g_MapSprites[0].attribute |= 0x10000000;
            MAPSCR(ms)->zoom_state++;
            g_MapBlinkTimer = 10;
        }
        break;
    case 6:
        if (MAPSCR(ms)->highlight_state == 1) {
            if (--g_MapBlinkTimer == 0) {
                MAPSCR(ms)->highlight_state = 2;
            }
        }
        break;
    case 7:
        g_MapSprites_0_W = 0xA1;
        g_MapSprites_2_W = 0x51;
        g_MapSprites_1_W = 0x51;
        *(s16 *)&g_MapSprites_0_ScaleX = g_MapSprites_1_ScaleX = g_MapSprites_2_ScaleX = 0xFFF;
        g_MapSprites_1_Y = 0;
        g_MapSprites_2_Y = 0;
        g_MapSprites_0_X = 0;
        g_MapSprites_1_X = 0;
        g_MapSprites_2_X = 0;
        *(u32 *)&g_MapSprites &= ~0x10000000;
        MAPSCR(ms)->arrow_anim = 0;
        g_MapSprites[0].y = 2;
        if (MAPSCR(ms)->highlight_state != 0) {
            MAPSCR(ms)->highlight_state = 1;
        }
        MAPSCR(ms)->zoom_state++;
    case 8:
        *(s16 *)&g_MapSprites_0_ScaleY -= 650;
        g_MapSprites_1_ScaleY -= 650;
        g_MapSprites_2_ScaleY -= 650;
        if (*(s16 *)&g_MapSprites_0_ScaleY < 2) {
            *(s16 *)&g_MapSprites_0_ScaleY = g_MapSprites_1_ScaleY = g_MapSprites_2_ScaleY = 200;
            MAPSCR(ms)->zoom_state++;
        }
        break;
    case 9:
        *(s16 *)&g_MapSprites_0_ScaleX /= 2;
        g_MapSprites_1_ScaleX /= 2;
        g_MapSprites_2_ScaleX /= 2;
        if (*(s16 *)&g_MapSprites_0_ScaleX < 2) {
            *(s16 *)&g_MapSprites_0_ScaleX = g_MapSprites_1_ScaleX = g_MapSprites_2_ScaleX = 1;
            if (MAPSCR(ms)->mode & 1) {
                *(s32 *)ms = 0x106;
            } else {
                *(s32 *)ms = 0x100;
            }
        }
        break;
    }
    map_display_draw_l(ms);
}

void map_display_draw(u8 *ms) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    /* the three arrow sprites (-O2: addressed as their own symbols, not through g_MapSprites) */
    extern GsSPRITE g_MapSprites_3 __asm__("g_MapSprites+0x6C");
    extern GsSPRITE g_MapSprites_4 __asm__("g_MapSprites+0x90");
    extern GsSPRITE g_MapSprites_5 __asm__("g_MapSprites+0xB4");
    GsSPRITE *sp;
    s32 i;
    u8 st;

    for (i = 0; i < 3; i++) {
        GsSortSprite(&g_MapSprites[i], &LITP(GsOT, g_OtMain)[g_ActiveBuffer], 14);
    }
    if (ms[3] == 6) {
        if (ms[0xE] < 0xD0) {
            do {
                g_MapSprites_3.y = (ms[0xE] >> 1) + 0x18;
                g_MapSprites_3.v = (ms[0xE] >> 1) + 0x84;
            } while (0);
            g_MapSprites_4.y = (ms[0xE] >> 1) + 0x18;
            g_MapSprites_4.v = (ms[0xE] >> 1) + 0x88;
            g_MapSprites_5.y = (ms[0xE] >> 1) + 0x18;
            g_MapSprites_5.v = (ms[0xE] >> 1) + 0x88;
            sp = &g_MapSprites_3;
            for (i = 0; i < 3; i++) {
                GsSortFastSprite(sp, &LITP(GsOT, g_OtMain)[g_ActiveBuffer], 13);
                sp++;
            }
        }
        ms[0xE]++;
    }
    st = ms[0x10];
    if (st >= 2 && ms[0xF] != 0) {
        i = (ms[0x15] / 3) & 7;
        switch (st) {
        case 2:
            play_sfx(SFX_BANK_SYSTEM, 0xA, 0);
            ms[0x10] = 3;
            /* fallthrough */
        case 3:
            ms[0x15] += 2;
            if (ms[0x15] >= 0x17) {
                ms[0x10] = 4;
                ms[0x15] = 0x17;
            }
            break;
        case 4:
            if (--ms[0x15] == 0) {
                ms[0x10] = 5;
                ms[0x15] = 0x14;
            }
            break;
        case 5:
            i = 0;
            if (--ms[0x15] == 0) {
                ms[0x15] = 0;
                ms[0x10] = 2;
            }
            break;
        }
        if ((u8)(ms[0xF] / 3) == 0) {
            g_MapSprites[6].x = 0x2A;
            g_MapSprites[6].y = 0x3A;
        } else {
            g_MapSprites[6].x = 0x86;
            g_MapSprites[6].y = 0x40;
        }
        g_MapSprites[6].u = i * 8;
        GsSortFastSprite(&g_MapSprites[6], &LITP(GsOT, g_OtMain)[g_ActiveBuffer], 12);
    }
}

void menu_init_map_screen(void) {
    typedef struct {
        s32 state_word;
        u8 layout;
        s8 area_group;
        s8 area;
        u8 room;
        u8 room_idx[4];
        u8 move_flags, layout_mask, arrow_anim, mode;
        u8 highlight_state;
    } MapScreenView;
    extern MapScreenView g_MapScreen_l __asm__("g_MapScreen");
    extern u8 g_RoomId_l __asm__("g_RoomId");
    extern s8 g_MapAreaFloors_l[4][4] __asm__("g_MapAreaFloors");
    void map_build_area_mask_l(MapScreenView *) __asm__("map_build_area_mask");
    void map_update_objective_highlight_l() __asm__("map_update_objective_highlight");
    MapScreenView *p = &g_MapScreen_l;
    u32 r;
    s32 i;
    s32 j;

    g_MapScreen_l.room_idx[0] = 1;
    g_MapScreen_l.room_idx[1] = 1;
    g_MapScreen_l.room_idx[2] = 1;
    g_MapScreen_l.room_idx[3] = 3;
    g_MapScreen_l.state_word = 0;
    g_MapScreen_l.room = g_RoomId_l;
    g_MapScreen_l.area_group = g_StageId % 5;
    switch (g_MapScreen_l.area_group) {
    case 0:
        if (g_RoomId_l == 7 && g_CutId >= 3 && g_CutId < 7) {
            p->area = 0;
            p->room = 0x1D;
        } else if (g_RoomId_l == 15 && g_CutId >= 3 && g_CutId < 5) {
            p->area = 0;
            p->room = 0x1E;
        } else if (g_RoomId_l == 16 && g_CutId == 0) {
            p->area = 2;
            p->room = 0x1E;
            p->area_group = 1;
        } else {
            p->area = 0;
        }
        break;
    case 1:
        if (g_RoomId_l == 15 && (g_CutId == 2 || g_CutId == 5)) {
            p->area = 1;
            p->room = 0x1D;
        } else if (g_RoomId_l == 12 && g_CutId >= 4 && g_CutId < 8) {
            p->area = 0;
            p->room = 0x1F;
            p->area_group = 0;
        } else if (g_RoomId_l < 0x1A) {
            p->area = 1;
        } else {
            p->area = 2;
        }
        break;
    case 2:
        if (g_RoomId_l == 11 && g_CutId >= 5 && g_CutId < 7) {
            p->area = 4;
            p->room = 0x11;
        } else if (g_RoomId_l == 15 && g_CutId == 6) {
            p->area = 4;
            p->room = 0x12;
        } else if (g_RoomId_l < 6) {
            p->area = 3;
        } else {
            p->area = 4;
        }
        break;
    case 3:
        if (g_RoomId_l < 13) {
            p->area = 5;
        } else {
            p->area = 6;
        }
        break;
    case 4:
        r = g_RoomId_l;
        if (r == 7 && g_CutId < 4) {
            p->area = 9;
            p->room = 0x16;
        } else if (r < 2) {
            p->area = 7;
        } else if (r < 5) {
            p->area = 8;
        } else if (r < 0x13) {
            p->area = 9;
        } else {
            p->area = 10;
        }
        break;
    }
    map_build_area_mask_l(p);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (g_MapAreaFloors_l[i][j] == p->area) {
                p->room_idx[i] = j;
            }
        }
    }
    map_update_objective_highlight_l();
    if (p->mode != 0) {
        p->mode++;
    }
    p->layout = g_MapFloorArea[p->area];
    p->highlight_state = 0;
}

void map_build_area_mask(u8 *ms) {
    extern u8 g_StageRoomFlagBase_u8[] __asm__("g_StageRoomFlagBase");
    s32 map_area_known_l(s32) __asm__("map_area_known");
    u32 i;
    u32 j;
    s32 flags;

    MAPSCR(ms)->area_mask = 0;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < g_MapAreaRoomCount[i]; j++) {
            if (Flg_ck(LITP(void, g_RoomFlags), j + g_StageRoomFlagBase_u8[i]) != 0) {
                switch (i) {
                case 0:
                    MAPSCR(ms)->area_mask |= 1;
                    break;
                case 1:
                    if (j < 0x1A || j == 0x1D) {
                        MAPSCR(ms)->area_mask |= 2;
                    } else {
                        MAPSCR(ms)->area_mask |= 4;
                    }
                    break;
                case 2:
                    if (j < 6 || j == 0x10) {
                        MAPSCR(ms)->area_mask |= 8;
                    } else {
                        MAPSCR(ms)->area_mask |= 0x10;
                    }
                    break;
                case 3:
                    if (j < 0xD) {
                        MAPSCR(ms)->area_mask |= 0x20;
                    } else {
                        MAPSCR(ms)->area_mask |= 0x40;
                    }
                    break;
                case 4:
                    if (j < 2 || j == 0x16) {
                        MAPSCR(ms)->area_mask |= 0x80;
                    } else if (j < 5) {
                        MAPSCR(ms)->area_mask |= 0x100;
                    } else if (j < 0x13) {
                        MAPSCR(ms)->area_mask |= 0x200;
                    } else {
                        MAPSCR(ms)->area_mask |= 0x400;
                    }
                    break;
                }
            }
        }
    }
    for (i = 0; i < 11; i++) {
        if (map_area_known_l(i) != 0) {
            MAPSCR(ms)->area_mask |= 1 << i;
        }
    }
    flags = ((u16) MAPSCR(ms)->area_mask);
    MAPSCR(ms)->layout_mask = 0;
    for (i = 0; i < 11; i++) {
        if ((flags >> i) & 1) {
            MAPSCR(ms)->layout_mask |= 1 << g_MapFloorArea[i];
        }
    }
}

void room_set_visited_flag(u32 stage, s32 room) {
    s32 Flg_on();
    Flg_on(ADDR_g_RoomFlags, *(u8 *)((u8 *)&g_StageRoomFlagBase + ((stage % 5))) + room);
}

void map_set_area_known(s32 map_id) {
    s32 Flg_on(s32, s32);
    Flg_on(ADDR_g_RoomFlags, map_id + 0x7C);
}

s32 map_area_known(u32 a, s32 n) {
    switch (a) {
    case 0:
        n = 0;
        break;
    case 1:
        n = 1;
        break;
    case 3:
        n = 2;
        break;
    case 4:
        n = 3;
        break;
    case 5:
    case 6:
        n = 4;
        break;
    case 8:
    case 9:
        n = 5;
        break;
    case 2:
    case 7:
    case 10:
        return 0;
    }
    return Flg_ck(ADDR_g_RoomFlags, n + 0x7C);
}

void file_set_collected_flag(s32 file_id) {
    s32 Flg_on(s32, s32);
    Flg_on(ADDR_g_RoomFlags, file_id + 0x82);
}

s32 file_is_collected(u8 file_id) {
    if (file_id == 0xFF) {
        return 0;
    }
    if (file_id == 0xFE) {
        return 0;
    }
    return Flg_ck(ADDR_g_RoomFlags, file_id + 0x82);
}

void tim_upload(u8 *tim, RECT *rect, s32 y) {
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    void GsGetTimInfo(u_long *im, GsIMAGE *tim);
    RECT r;
    GsIMAGE img;

    GsGetTimInfo((u_long *)(tim + 4), &img);
    LoadImage(rect, img.pixel);
    DrawSync(0);
    if ((img.pmode >> 3) & 1) {
        r.x = 0;
        r.y = y + 0x1E0;
        r.w = img.cw;
        r.h = img.ch;
        LoadImage(&r, img.clut);
    }
    DrawSync(0);
}

s32 menu_update_map_animation(void) {
    typedef struct {
        u8 state, substate, blink_state, zoom_state;
        u8 pad4[0xC];
        u8 highlight_state, highlight_timer, timer, unused_13, exit_flag;
    } MapScreenView;
    void map_tab_update_l(MapScreenView *) __asm__("map_tab_update");
    void map_display_update_l(MapScreenView *) __asm__("map_display_update");
    MapScreenView *p = (MapScreenView *)&g_MapScreen;

    switch (p->state) {
    case 0:
        p->timer = 10;
        p->state++;
    case 1:
        if (p->timer-- != 0) {
            break;
        }
        p->timer = 5;
        p->state++;
        play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
        break;
    case 2:
        map_tab_update_l(p);
        if (p->timer-- != 0) {
            break;
        }
        p->timer = 0x14;
        p->zoom_state = 0;
        p->blink_state = 0;
        p->substate = 0;
        p->state++;
        break;
    case 3:
        map_display_update_l(p);
        if (p->zoom_state != 6) {
            break;
        }
        if (p->timer-- != 0) {
            break;
        }
        p->highlight_state = 2;
        p->state++;
        break;
    case 4:
        map_display_update_l(p);
        if ((g_PadPressed & PAD_CANCEL) || (g_PadTrigger & PSX_START)) {
            p->zoom_state = 7;
            p->state++;
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
        }
        break;
    case 5:
        map_display_update_l(p);
        break;
    case 6:
        p->timer = 5;
        p->state++;
        play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
    case 7:
        map_tab_update_l(p);
        if (p->timer-- != 0) {
            break;
        }
        p->timer = 5;
        p->state++;
        break;
    case 8:
        if (p->timer-- != 0) {
            break;
        }
        p->exit_flag = 1;
        p->state++;
        play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
        break;
    }
    return -(p->exit_flag != 0);
}

void menu_init_map_display(void) {
    s32 map_build_area_mask_a1() __asm__("map_build_area_mask");
    s32 map_update_objective_highlight_a1() __asm__("map_update_objective_highlight");
    u8 area;
    s8 *map_screen = (s8 *)&g_MapScreen;

    map_update_objective_highlight_a1();
    MAPSCR(map_screen)->state = 0;
    if (g_MapScreen_Mode == 1) {
        area = g_MapFloorArea[0];
        g_MapScreen_AreaGroup = 1;
        g_MapScreen_AreaFloor0 = 1;
        g_MapScreen_AreaFloor1 = 1;
        g_MapScreen_AreaFloor2 = 1;
        g_MapScreen_Floor = 0;
        g_MapScreen_Room = 0;
        g_MapScreen_AreaFloor3 = 3;
        g_MapScreen_Area = area;
    } else {
        area = g_MapFloorArea[6];
        g_MapScreen_AreaGroup = 5;
        g_MapScreen_Floor = 6;
        g_MapScreen_AreaFloor0 = 1;
        g_MapScreen_AreaFloor1 = 1;
        g_MapScreen_Room = 0;
        g_MapScreen_AreaFloor2 = 0;
        g_MapScreen_AreaFloor3 = 3;
        g_MapScreen_Area = area;
    }
    MAPSCR(map_screen)->move_flags = 0;
    MAPSCR(map_screen)->layout_mask = 0;
    MAPSCR(map_screen)->arrow_anim = 0;
    MAPSCR(map_screen)->highlight_state = 0;
    MAPSCR(map_screen)->highlight_timer = 0;
    MAPSCR(map_screen)->countdown = 0;
    MAPSCR(map_screen)->unused_13 = 0;
    MAPSCR(map_screen)->exit_flag = 0;
    MAPSCR(map_screen)->highlight_frame = 0;
    MAPSCR(map_screen)->area_mask = 0;
    map_build_area_mask_a1(map_screen);
}

void map_update_objective_highlight(void) {
    void *f2 = LITP(void, g_ScenarioFlags);
    void *f1 = LITP(void, g_ScenarioFlags2);
    u8 *q = g_MapScreen;

    if (!(g_GameStateFlags & GAME_ALT_RDT)) {
        if (Flg_ck(ADDR_g_ScenarioFlags2, 0x2D) != 0 && Flg_ck(ADDR_g_ScenarioFlags, 0x49) == 0) {
            g_MapScreen_Mode = 1;
            return;
        }
        if (Flg_ck(f1, 0x2E) != 0 && Flg_ck(f2, 0x4A) == 0) {
            q[0xF] = 1;
            return;
        }
        if (Flg_ck(f1, 0x38) != 0 && Flg_ck(f2, 0x48) == 0) {
            q[0xF] = 3;
            return;
        }
        q[0xF] = 0;
    } else {
        if (Flg_ck(ADDR_g_ScenarioFlags2, 0x23) != 0 && Flg_ck(ADDR_g_ScenarioFlags, 0x49) == 0) {
            g_MapScreen_Mode = 1;
            return;
        }
        q[0xF] = 0;
    }
}
