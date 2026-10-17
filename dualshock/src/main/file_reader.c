/* file_reader.c - The document (file) screens: the FILE book, the page reader and page textures.
   (The pickup screen: file_pickup.c.) */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


s32 menu_file_state_machine(void) {
    extern u8 g_FilePickupScreen_u8 __asm__("g_FilePickupScreen");
    extern u8 g_FilePickupScreen_Closing_u8 __asm__("g_FilePickupScreen_Closing");
    void file_book_advance_l(u8 *) __asm__("file_book_advance");
    void file_reader_advance_l(u8 *) __asm__("file_reader_advance");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    u8 *p = &g_FilePickupScreen_u8;
    u8 mode;

    mode = p[0];
    switch (mode) {
    case 0:
        if (g_FilePickupScreen_Closing_u8 == 0 && (g_PadPressed & PAD_CANCEL)) {
            if ((u32)(g_FilePickupScreen_OpenRequest - 1) >= 2) {
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
            g_FilePickupScreen_NextMenu = 2;
            g_FilePickupScreen_Closing_u8 = 1;
        }
        file_book_advance_l(p);
        if (p[9] == 0 && (g_PadPressed & PAD_CONFIRM) && p[2] == 1 && p[5] != 0xFF) {
            p[3] = 1;
            p[9] = 1;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
        }
        break;
    case 1:
        file_reader_advance_l(p);
        if (g_FilePickupScreen_Closing_u8 == 0) {
            if (LIT(u16, g_PadPressed) & PAD_CANCEL) {
                g_FilePickupScreen_NextMenu = 0xD;
                g_FilePickupScreen_Closing_u8 = 1;
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
            if ((LIT(u16, g_PadPressed) & PAD_CONFIRM) && g_FilePickupScreen_ExitSelected == mode) {
                g_FilePickupScreen_NextMenu = 0xD;
                g_FilePickupScreen_Closing_u8 = 1;
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
        }
        break;
    }
    if (p[8] != 0) {
        p[8] = 0;
        return -1;
    }
    return 0;
}

void file_book_advance(void *scr) {
    s32 file_book_init_a1(void *) __asm__("file_book_init");
    s32 file_book_input_a1(void *) __asm__("file_book_input");
    u8 state;

    state = FILESCR(scr)->state;
    switch (state) {
    case 0:
        file_book_init_a1(scr);
        FILESCR(scr)->state = (u8) (FILESCR(scr)->state + 1);
        return;
    case 1:
        file_book_input_a1(scr);
        return;
    }
}

void file_book_init(u8 *scr) {
    extern u8 g_PickupFadeBox_R_u8 __asm__("g_PickupFadeBox_R");
    void pickup_mark_seen_l(u8 *) __asm__("pickup_mark_seen");
    s32 file_is_collected(u8);
    void file_load_texture_l(u16, s32, s32) __asm__("file_load_texture");
    void file_book_draw(u8 *);
    s32 i;
    s8 n;

    FIELD_AT(scr, FileScreen, ->slide_speed, s16) = 0x80;
    FIELD_AT(scr, FileScreen, ->close_request, s8) = 0;
    FIELD_AT(scr, FileScreen, ->list_page, s8) = 0;
    FIELD_AT(scr, FileScreen, ->slide_y, s16) = -0x7F;
    FILESCR(scr)->slide_x = 0;
    if (g_GameStateFlags & GAME_ALT_RDT) {
        FIELD_AT(scr, FileScreen, ->file_set, s8) = 1;
    } else {
        FIELD_AT(scr, FileScreen, ->file_set, s8) = 0;
    }
    pickup_mark_seen_l(scr);
    for (i = 0; i < 8; i++) {
        if (file_is_collected(g_FileBookTable[FIELD_AT(scr, FileScreen, ->file_set, s8)][i]) != 0) {
            FIELD_AT(scr, FileScreen, ->selected, s8) = i;
            break;
        }
        if (i == 7) {
            FIELD_AT(scr, FileScreen, ->selected, u8) = 0xFF;
        }
    }
    g_PickupFadeBox_B = 0;
    g_PickupFadeBox_G = 0;
    g_PickupFadeBox_R_u8 = 0;
    n = FIELD_AT(scr, FileScreen, ->list_page, s8);
    file_load_texture_l(g_FileBookTexFiles[n], n + 0x90, 0);
    file_book_draw(scr);
}

void file_book_input(u8 *p) {
    extern u8 g_FileBookTable_l[][2][8] __asm__("g_FileBookTable");
    s32 file_is_collected(u8);
    void file_book_draw(u8 *);
    s32 dir;
    s32 i;
    s32 j;
    s8 c;
    s32 t;

    if (FILESCR(p)->busy == 0) {
        if (((u8) FILESCR(p)->selected) != 0xFF) {
            if ((g_PadHeldRaw & PSX_UP) || (g_PadHeldRaw & PSX_DOWN)) {
                FILESCR(p)->hold_timer++;
            } else {
                FILESCR(p)->hold_timer = 0;
            }
            t = g_PadTrigger & PSX_UP;
            dir = t != 0;
            if (g_PadTrigger & PSX_DOWN) {
                dir = 2;
            }
            c = FILESCR(p)->hold_timer;
            if (c > 20 && c % 3 == 0) {
                if (g_PadHeldRaw & PSX_UP) {
                    dir = 1;
                }
                if (g_PadHeldRaw & PSX_DOWN) {
                    dir = 2;
                }
            }
            if (dir == 1) {
                for (i = 0; i < 8; i++) {
                    j = ((u8) FILESCR(p)->selected) - i - 1;
                    if (j < 0) {
                        break;
                    }
                    if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + j])) {
                        FILESCR(p)->selected = j;
                        play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                        break;
                    }
                }
            }
            if (dir == 2) {
                for (i = 0; i < 8; i++) {
                    j = ((u8) FILESCR(p)->selected) + i + 1;
                    if (j >= 8) {
                        break;
                    }
                    if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + j])) {
                        FILESCR(p)->selected = j;
                        play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                        break;
                    }
                }
            }
        }
        if (g_PadTrigger & PSX_RIGHT) {
            if ((s8)p[4] + 1 >= 2) {
                FILESCR(p)->list_page = 1;
            } else {
                FILESCR(p)->step = 3;
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            }
        }
        if (g_PadTrigger & PSX_LEFT) {
            if ((s8)p[4] - 1 < 0) {
                FILESCR(p)->list_page = 0;
            } else {
                FILESCR(p)->step = 8;
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            }
        }
    }
    file_book_draw(p);
}

/* 4-aligned jump table followed by a pad word (tools/jtalign.awk) */
__asm__("#JTALIGN2PAD");
void file_book_draw(u8 *p) {
    extern u8 g_FileBookTable_l[][2][8] __asm__("g_FileBookTable");
    extern s16 g_ScreenOffsetX_l[] __asm__("g_ScreenOffsetX");
    extern s16 g_ScreenOffsetY_l __asm__("g_ScreenOffsetY");
    s32 file_is_collected(u8);
    void file_load_texture_l(u16, s32, s32) __asm__("file_load_texture");
    s32 pickup_fade_update(u32);
    s32 draw_item_name_l() __asm__("draw_item_name");
    s32 i;
    s32 j;
    s32 k;
    s32 yo;
    s32 off;
    s8 n;
    u8 c;
    u8 id;
    s32 m;
    u8 (*tbl)[2][8];

    switch (FIELD_AT(p, FileScreen, ->step, u8)) {
    case 0:
        FILESCR(p)->slide_speed /= 2;
        FILESCR(p)->slide_y += FILESCR(p)->slide_speed;
        if (FILESCR(p)->slide_y >= 0) {
            FILESCR(p)->slide_y = 0;
            FILESCR(p)->slide_speed = 1;
            FIELD_AT(p, FileScreen, ->step, u8)++;
        }
        break;
    case 1:
        FIELD_AT(p, FileScreen, ->busy, u8) = 0;
        FILESCR(p)->slide_speed = 1;
        break;
    case 2:
        FILESCR(p)->slide_y -= FILESCR(p)->slide_speed;
        FILESCR(p)->slide_speed *= 2;
        if (FILESCR(p)->slide_y < -0x67) {
            FIELD_AT(p, FileScreen, ->mode, s32) = 0;
            FIELD_AT(p, FileScreen, ->close_request, u8) = 1;
        }
        break;
    case 3:
        FIELD_AT(p, FileScreen, ->busy, u8) = 1;
        FILESCR(p)->slide_speed = 1;
        FIELD_AT(p, FileScreen, ->step, u8)++;
        /* fallthrough */
    case 4:
        FILESCR(p)->slide_x -= FILESCR(p)->slide_speed;
        FILESCR(p)->slide_speed *= 2;
        if (FILESCR(p)->slide_x < -0x77) {
            FILESCR(p)->slide_x = 0x7F;
            FILESCR(p)->slide_speed = 0x80;
            FIELD_AT(p, FileScreen, ->step, u8)++;
        }
        break;
    case 5:
        FIELD_AT(p, FileScreen, ->list_page, u8)++;
        for (i = 0; i < 8; i++) {
            if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + i]) != 0) {
                FIELD_AT(p, FileScreen, ->selected, u8) = i;
                break;
            }
            if (i == 7) {
                FIELD_AT(p, FileScreen, ->selected, u8) = 0xFF;
            }
        }
        FIELD_AT(p, FileScreen, ->step, u8)++;
        break;
    case 6:
        n = FIELD_AT(p, FileScreen, ->list_page, u8);
        file_load_texture_l(g_FileBookTexFiles[n], n + 0x90, 0);
        FIELD_AT(p, FileScreen, ->step, u8)++;
        /* fallthrough */
    case 7:
        FILESCR(p)->slide_speed /= 2;
        FILESCR(p)->slide_x -= FILESCR(p)->slide_speed;
        if (FILESCR(p)->slide_x <= 0) {
            FILESCR(p)->slide_x = 0;
            FIELD_AT(p, FileScreen, ->step, u8) = 1;
        }
        break;
    case 8:
        FIELD_AT(p, FileScreen, ->busy, u8) = 1;
        FILESCR(p)->slide_speed = 1;
        FIELD_AT(p, FileScreen, ->step, u8)++;
        /* fallthrough */
    case 9:
        FILESCR(p)->slide_x += FILESCR(p)->slide_speed;
        FILESCR(p)->slide_speed *= 2;
        if (FILESCR(p)->slide_x >= 0x78) {
            FILESCR(p)->slide_x = -0x7F;
            FILESCR(p)->slide_speed = 0x80;
            FIELD_AT(p, FileScreen, ->step, u8)++;
        }
        break;
    case 10:
        FIELD_AT(p, FileScreen, ->list_page, u8)--;
        for (i = 0; i < 8; i++) {
            if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + i]) != 0) {
                FIELD_AT(p, FileScreen, ->selected, u8) = i;
                break;
            }
            if (i == 7) {
                FIELD_AT(p, FileScreen, ->selected, u8) = 0xFF;
            }
        }
        FIELD_AT(p, FileScreen, ->step, u8)++;
        break;
    case 11:
        n = FIELD_AT(p, FileScreen, ->list_page, u8);
        file_load_texture_l(g_FileBookTexFiles[n], n + 0x90, 0);
        FIELD_AT(p, FileScreen, ->step, u8)++;
        /* fallthrough */
    case 12:
        FILESCR(p)->slide_speed /= 2;
        FILESCR(p)->slide_x += FILESCR(p)->slide_speed;
        if (FILESCR(p)->slide_x >= 0) {
            FILESCR(p)->slide_x = 0;
            FIELD_AT(p, FileScreen, ->step, u8) = 1;
        }
        break;
    }
    c = FIELD_AT(p, FileScreen, ->open_request, u8);
    switch (c) {
    case 0:
        break;
    case 1:
        if (pickup_fade_update(0) != 0) {
            FIELD_AT(p, FileScreen, ->mode, s32) = c;
        }
        FIELD_AT(p, FileScreen, ->busy, u8) = 1;
        break;
    case 2:
        if (pickup_fade_update(1) != 0) {
            FILESCR(p)->slide_x = 0;
            FILESCR(p)->slide_y = 0;
            FIELD_AT(p, FileScreen, ->open_request, u8) = 0;
        }
        FIELD_AT(p, FileScreen, ->busy, u8) = 1;
        break;
    }
    if (FIELD_AT(p, FileScreen, ->step, u8) == 1) {
        FILESCR(p)->blink_timer++;
        if (!(FILESCR(p)->blink_timer & 0x30)) {
            FIELD_AT(p, FileScreen, ->arrow_blink, u8) = 1;
        } else {
            FIELD_AT(p, FileScreen, ->arrow_blink, u8) = 0;
        }
        g_FileBookPrevPageArrow_U[0] = (s8)p[0x11] * 8 - 0x50;
        g_FileBookNextPageArrow_U = (s8)p[0x11] * 8 - 0x40;
        k = (s8)p[4];
        if (k != 0) {
            GsSortFastSprite((GsSPRITE *)(g_FileBookPrevPageArrow_U - 0xE), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 0xF);
        }
        if ((s8)p[4] != 1) {
            GsSortFastSprite((GsSPRITE *)(g_FileBookPrevPageArrow_U + 0x16), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 0xF);
        }
    }
    g_FileBookListSprite_X[0] = FILESCR(p)->slide_x + 0x20;
    g_FileBookListSprite_Y = FILESCR(p)->slide_y + 0x18;
    GsSortFastSprite((GsSPRITE *)(g_FileBookListSprite_X - 2), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 0x10);
    if (FIELD_AT(p, FileScreen, ->selected, u8) == 0xFF) {
        return;
    }
    i = 0;
    tbl = g_FileBookTable_l;
    for (off = 0, yo = 0x29; i < 8; off += 0x24, i++, yo += 9) {
        if (file_is_collected(tbl[(s8)p[7]][0][(s8)p[4] * 8 + i]) != 0) {
            ((GsSPRITE *)((u8 *)g_FileBookEntrySprites + off))->x = FILESCR(p)->slide_x + ({ i / 2 + 0x8C; });
            ((GsSPRITE *)((u8 *)g_FileBookEntrySprites + off))->y = FILESCR(p)->slide_y + yo;
            GsSortFastSprite((GsSPRITE *)((u8 *)g_FileBookEntrySprites + off), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 0xF);
        }
    }
    g_FileBookCursorSprite_X[0] = FILESCR(p)->slide_x + ({ (FIELD_AT(p, FileScreen, ->selected, u8) >> 1) + 0x8C; });
    g_FileBookCursorSprite_Y = FILESCR(p)->slide_y + ({ FIELD_AT(p, FileScreen, ->selected, u8) * 9 + 0x29; });
    GsSortFastSprite((GsSPRITE *)(g_FileBookCursorSprite_X - 2), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 0xE);
    if (FIELD_AT(p, FileScreen, ->step, u8) != 1) {
        return;
    }
    id = g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + FIELD_AT(p, FileScreen, ->selected, u8)];
    draw_item_name_l(id + 0x5F, 0x40, g_FileNameX[id] - g_ScreenOffsetX_l[0] + 0x1E, 0xC1 - g_ScreenOffsetY_l);
    do {
        i = 0;
        g_FileBookUpArrow_U = (s8)p[0x11] * 8 - 0x30;
        m = 0x67;
    } while (0);
    m -= (u16)g_ScreenOffsetX_l[0];
    g_FileBookUpArrow_Y = 0xB7 - g_ScreenOffsetY_l;
    g_FileBookUpArrow_X = m;
    g_FileBookDownArrow_U = (s8)p[0x11] * 8 - 0x20;
    g_FileBookDownArrow_X = m;
    g_FileBookDownArrow_Y = 0xCF - g_ScreenOffsetY_l;
    for (; i < 8; i++) {
        j = FIELD_AT(p, FileScreen, ->selected, u8) - i - 1;
        if (j < 0) {
            break;
        }
        if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + j]) != 0) {
            GsSortFastSprite(&g_FileBookUpArrow, LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        j = FIELD_AT(p, FileScreen, ->selected, u8) + i + 1;
        if (j >= 8) {
            break;
        }
        if (file_is_collected(g_FileBookTable_l[(s8)p[7]][0][(s8)p[4] * 8 + j]) != 0) {
            GsSortFastSprite(&g_FileBookDownArrow, LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 3);
            break;
        }
    }
}

void file_reader_advance(void *scr) {
    void file_reader_init(u8 *scr);
    s32 file_reader_input_a1(void *) __asm__("file_reader_input");
    s32 pickup_fade_update_a1(s32) __asm__("pickup_fade_update");
    u8 state;

    pickup_fade_update_a1(2);
    state = FILESCR(scr)->state;
    switch (state) {
    case 0:
        file_reader_init(scr);
        FILESCR(scr)->state = (u8) (FILESCR(scr)->state + 1);
        return;
    case 1:
        file_reader_input_a1(scr);
        return;
    }
}

void file_reader_init(u8 *scr) {
    FILESCR(scr)->file_page = 0;
    FILESCR(scr)->exit_selected = 0;
    file_reader_draw(scr);
}

void file_reader_input(s8 *scr) {
    void file_reader_draw_l() __asm__("file_reader_draw");
    u32 max;

    max = g_FilePageCount[g_FileBookTable[FILESCR(scr)->file_set][FILESCR(scr)->list_page * 8 + ((u8) FILESCR(scr)->selected)]];
    if (FILESCR(scr)->busy == 0) {
        if ((g_PadHeldRaw & PSX_RIGHT) || (g_PadHeldRaw & PSX_LEFT)) {
            FIELD_AT(scr, FileScreen, ->hold_timer, s8)++;
        } else {
            FIELD_AT(scr, FileScreen, ->hold_timer, s8) = 0;
        }
        if ((g_PadTrigger & PSX_RIGHT) || ((g_PadHeldRaw & PSX_RIGHT) && FIELD_AT(scr, FileScreen, ->hold_timer, s8) >= 21)) {
            if (FILESCR(scr)->file_page + 1 >= max) {
                if (FILESCR(scr)->exit_selected == 0) {
                    FILESCR(scr)->exit_selected = 1;
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                }
            } else {
                FILESCR(scr)->busy = 1;
                FILESCR(scr)->step = 5;
                play_sfx(SFX_BANK_SYSTEM, 8, 0);
            }
        }
        if ((g_PadTrigger & PSX_LEFT) || ((g_PadHeldRaw & PSX_LEFT) && FIELD_AT(scr, FileScreen, ->hold_timer, s8) >= 21)) {
            if (FILESCR(scr)->exit_selected == 1) {
                FILESCR(scr)->exit_selected = 0;
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            } else if (FILESCR(scr)->file_page - 1 < 0) {
                FILESCR(scr)->file_page = 0;
            } else {
                FILESCR(scr)->busy = 1;
                FILESCR(scr)->step = 8;
                play_sfx(SFX_BANK_SYSTEM, 8, 0);
            }
        }
    }
    file_reader_draw_l(scr);
}

__asm__("#JTALIGN2");
void file_reader_draw(u8 *scr) {
    typedef struct {
        s16 x;
        s16 y;
        u16 w;
        u16 h;
        u16 tpage;
        u8 u;
        u8 v;
        s16 cx;
        s16 cy;
    } SprTail;
    extern SprTail g_FileReaderPageSprite_X_s __asm__("g_FileReaderPageSprite_X");
    void file_load_texture_l(u16, s32, s32) __asm__("file_load_texture");
    s32 file_load_page_l() __asm__("file_load_page");
    s32 t; /* NOTE: read uninitialised in case 0 - the original passes a stale register */

    switch (FILESCR(scr)->step) {
    case 0:
        file_load_texture_l(g_FileReaderFrameFile, t + 0x96, 1);
        FILESCR(scr)->slide_x = 0x128;
        FILESCR(scr)->slide_y = 0;
        FILESCR(scr)->busy = 1;
        FILESCR(scr)->step++;
        goto inc;
    case 2:
        if (!(((u8) FILESCR(scr)->file_page) % 2)) {
            t = g_FilePageStart[g_FileBookTable[FILESCR(scr)->file_set][FILESCR(scr)->list_page * 8 + ((u8) FILESCR(scr)->selected)]] + FILESCR(scr)->file_page / 2;
            file_load_page_l(g_FilePageFiles[t], t + 0xB4, 2);
        }
        FILESCR(scr)->slide_x = 0x128;
        FILESCR(scr)->slide_speed = 0x12C;
        FILESCR(scr)->step++;
    case 3:
        FILESCR(scr)->slide_speed /= 2;
        FILESCR(scr)->slide_x -= FILESCR(scr)->slide_speed;
        if (FILESCR(scr)->slide_x <= 0) {
            FILESCR(scr)->slide_x = 0;
            FILESCR(scr)->slide_speed = 1;
            FILESCR(scr)->step++;
        }
        break;
    case 4:
        FILESCR(scr)->busy = 0;
        break;
    case 5:
        FILESCR(scr)->slide_x = 0;
        FILESCR(scr)->slide_speed = 1;
        FILESCR(scr)->step++;
    case 6:
        FILESCR(scr)->slide_x -= FILESCR(scr)->slide_speed;
        FILESCR(scr)->slide_speed *= 2;
        if (FILESCR(scr)->slide_x < -0x127) {
            FILESCR(scr)->step++;
            FILESCR(scr)->file_page++;
        }
        break;
    case 7:
        FILESCR(scr)->step = 2;
        break;
    case 8:
        FILESCR(scr)->slide_x = 0;
        FILESCR(scr)->slide_speed = 1;
        FILESCR(scr)->step++;
    case 9:
        FILESCR(scr)->slide_x += FILESCR(scr)->slide_speed;
        FILESCR(scr)->slide_speed *= 2;
        if (FILESCR(scr)->slide_x >= 0x128) {
            FILESCR(scr)->step++;
            FILESCR(scr)->file_page--;
        }
        break;
    case 11:
        if (((u8) FILESCR(scr)->file_page) % 2) {
            t = g_FilePageStart[g_FileBookTable[FILESCR(scr)->file_set][FILESCR(scr)->list_page * 8 + ((u8) FILESCR(scr)->selected)]] + FILESCR(scr)->file_page / 2;
            file_load_page_l(g_FilePageFiles[t], t + 0xB4, 2);
        }
        FILESCR(scr)->slide_x = -0x128;
        FILESCR(scr)->slide_speed = 0x12C;
        FILESCR(scr)->step++;
    case 12:
        FILESCR(scr)->slide_speed /= 2;
        FILESCR(scr)->slide_x += FILESCR(scr)->slide_speed;
        if (FILESCR(scr)->slide_x >= 0) {
            FILESCR(scr)->slide_x = 0;
            FILESCR(scr)->step = 4;
        }
        break;
    case 13:
        FILESCR(scr)->slide_x = 0;
        FILESCR(scr)->slide_speed = 1;
        FILESCR(scr)->step++;
    case 14:
        FILESCR(scr)->slide_x -= FILESCR(scr)->slide_speed;
        FILESCR(scr)->slide_speed *= 2;
        if (FILESCR(scr)->slide_x < -0x127) {
            FILESCR(scr)->step++;
        }
        break;
    case 17:
        file_load_texture_l(g_FileBookTexFiles[FILESCR(scr)->list_page], FILESCR(scr)->list_page + 0x90, 0);
    case 1:
    case 10:
    case 15:
    case 16:
    inc:
        FILESCR(scr)->step++;
        break;
    case 18:
        *(s32 *)scr = 0x02010100;
        break;
    }
    g_FileReaderPageSprite_X_s.x = FILESCR(scr)->slide_x + 0x18;
    g_FileReaderPageSprite_X_s.y = FILESCR(scr)->slide_y + 0x18;
    g_FileReaderPageSprite_X_s.tpage = FILESCR(scr)->file_page % 2 + 0x15;
    g_FileReaderPageSprite_X_s.cy = FILESCR(scr)->file_page % 2 + 0x1FD;
    GsSortFastSprite((GsSPRITE *)((u8 *)&g_FileReaderPageSprite_X_s - 4), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
    if (FILESCR(scr)->step == 4) {
        if (!(++FILESCR(scr)->blink_timer & 0x30)) {
            FILESCR(scr)->arrow_blink = 1;
        } else {
            FILESCR(scr)->arrow_blink = 0;
        }
        if (FILESCR(scr)->exit_selected == 1) {
            FILESCR(scr)->arrow_blink = 1;
            FILESCR(scr)->blink_timer = 0;
        }
        g_FileReaderPrevArrow_U[0] = FILESCR(scr)->arrow_blink * 8;
        g_FileReaderPrevArrow_U[0x24] = FILESCR(scr)->arrow_blink * 8 + 0x10;
        if (FILESCR(scr)->file_page != 0) {
            GsSortFastSprite((GsSPRITE *)&g_FileReaderPrevArrow_U[-0xE], LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
        }
        GsSortFastSprite((GsSPRITE *)&g_FileReaderPrevArrow_U[0x16], LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
        if (FILESCR(scr)->file_page == g_FilePageCount[g_FileBookTable[FILESCR(scr)->file_set][FILESCR(scr)->list_page * 8 + ((u8) FILESCR(scr)->selected)]] - 1) {
            if (FILESCR(scr)->exit_selected == 1) {
                g_FileReaderExitIcon_R = g_FileReaderExitIcon_G = g_FileReaderExitIcon_B = 0x50;
            } else {
                g_FileReaderExitIcon_R = g_FileReaderExitIcon_G = g_FileReaderExitIcon_B = 0x20;
            }
            GsSortFastSprite(&g_FileReaderExitIcon, LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
        }
    }
}

/* VRAM place of each file texture slot: tim_upload(buffer, &rect, clut_row) */
typedef struct { RECT rect; s32 clut_row; } FileTexRect;
extern FileTexRect g_FileTexRects[];
/* Where each file page lies inside archive file 0x9E (file_load_page) */
typedef struct { s32 sector_off; s32 size; } FilePageEntry;
extern FilePageEntry g_FilePageArchive[];

void file_load_texture(s32 file_id, s32 res_id, s32 tex_slot) {
    s32 load_file_blocking(s32, s32);
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    RECT r;
    u8 *p = &g_MenuWork.loaded_res_id;

    if (*p != res_id) {
        *p = res_id;
        DrawSync(0);
        load_file_blocking(file_id, ADDR_g_MenuTexBuf);
        if (tex_slot == 2) {
            r.x = 0;
            r.w = 0x10;
            r.h = 1;
            r.y = g_FileClutRow + 0x1E0;
            LoadImage(&r, LITP(u_long, g_MenuTexBuf));
            DrawSync(0);
            r.x = 0;
            r.w = 0x10;
            r.h = 1;
            r.y = g_FileClutRow + 0x1E1;
            LoadImage(&r, LITP(u_long, g_MenuTexBuf_Clut1));
            DrawSync(0);
            LoadImage(&g_FileClutRect, LITP(u_long, g_MenuTexBuf_Pixels));
            DrawSync(0);
            return;
        }
        tim_upload(ADDR_g_MenuTexBuf, &g_FileTexRects[tex_slot], g_FileTexRects[tex_slot].clut_row);
    }
}

void file_load_page(s32 page, s32 res_id, s32 tex_slot) {
    s32 load_file_blocking();
    s32 tim_upload();
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    u32 t;
    u8 *flag;
    s16 clut_rect[4];
    extern s32 g_FilePageArchive_0_Size __asm__("g_FilePageArchive+4");  /* -O2: [page].size read as its own symbol */
    s32 page_off;
    u16 saved_lba_lo;
    u8 saved_lba_hi;

    flag = &g_MenuWork.loaded_res_id;
    if (*flag != res_id) {
        *flag = res_id;
        DrawSync(0);
        saved_lba_hi = g_CdFileTable_9E_LbaHi;
        saved_lba_lo = g_CdFileTable_9E_Lba[0];
        page_off = page << 3;
        t = saved_lba_lo + (saved_lba_hi << 0x10) + g_FilePageArchive[page].sector_off;
        g_CdFileTable_9E_Lba[0] = t;
        g_CdFileTable_9E_LbaHi = t >> 0x10;
        g_CdFileTable_9E_Size = *(s32 *)((u8 *)&g_FilePageArchive_0_Size + (page_off));
        g_CdFileTable_9E_Sum = *(u8 *)((u8 *)&g_FilePageChecksums + (page));
        load_file_blocking(0x9E, ADDR_g_MenuTexBuf);
        g_CdFileTable_9E_Lba[0] = saved_lba_lo;
        g_CdFileTable_9E_LbaHi = saved_lba_hi;
        if (tex_slot == 2) {
            clut_rect[0] = 0;
            clut_rect[2] = 0x10;
            clut_rect[3] = 1;
            clut_rect[1] = g_FileClutRow + 0x1E0;
            LoadImage((RECT *) clut_rect, LITP(u_long, g_MenuTexBuf));
            DrawSync(0);
            clut_rect[0] = 0;
            clut_rect[2] = 0x10;
            clut_rect[3] = 1;
            clut_rect[1] = g_FileClutRow + 0x1E1;
            LoadImage((RECT *) clut_rect, LITP(u_long, g_MenuTexBuf_Clut1));
            DrawSync(0);
            LoadImage(&g_FileClutRect, LITP(u_long, g_MenuTexBuf_Pixels));
            DrawSync(0);
            return;
        }
        tim_upload(ADDR_g_MenuTexBuf, &g_FileTexRects[tex_slot], g_FileTexRects[tex_slot].clut_row);
    }
}
