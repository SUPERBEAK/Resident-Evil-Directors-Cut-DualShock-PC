/* title.c - TITLE.EXE: the title screen: title movie, menu pages (new game / load / options) and the
   Director's Cut mode choice (Standard / Training / Advanced); the loop runs while g_TitleRunning. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "overlay_api.h"

/* main-program globals this overlay uses (per-function aliases with other types stay in the functions) */
extern s32 g_GameStateFlags;
extern s32 g_StatusFlags;
extern u8 g_ActiveBuffer;
extern s8 g_VSyncMode;
extern s8 g_OpenMenuFlag;
extern s16 g_FadeLevel;
extern s16 g_Fade2Level;
extern s8 g_SaveDcMode;
extern u8 g_VramTpageCursor;
extern u8 g_VramClutRowCursor;
extern s16 g_FadeStep;
extern s32 g_LoadPointer;
extern s8 g_SelectedFmvId;
extern s32 g_FmvBuffer;
extern s32 g_PadRaw32;
extern s32 g_PadRawPressed32;
extern s8 g_FadeTypeId;
/* per title-screen sprite TIM (title_init loads them in order): its CLUT row and texture page in VRAM */
extern u16 g_TitleSpriteClutRow[8];
extern u16 g_TitleSpriteTpage[8];
extern s8 g_TitleMenuChoice;
extern s8 g_TitleOptionPad;
extern s8 g_TitleRunning;
extern u8 g_TitlePage;
extern u8 g_TitleState;

void title_state(void) {
    void title_init();
    void title_menu_update();
    s32 title__main();
    extern u8 g_TitleMenuChoice_u8 __asm__("g_TitleMenuChoice");
    extern u8 g_TitleRunning_u8 __asm__("g_TitleRunning");
    extern s8 g_TitlePage_s8 __asm__("g_TitlePage");
    extern s8 g_TitleState_s8 __asm__("g_TitleState");
    s32 pad10[2];
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;
    s32 pad28[6];

    title__main();
    (g_ApiCallPtr = api_sound_reinit)();
    g_LoadPointer = ADDR_g_LoadArea;
    (g_ApiCallPtr = api_sound_load_bank1)((s16 *)0xC, ADDR_g_LoadArea);
    g_FadeLevel = -1;
    g_Fade2Level = -1;
    g_TitleRunning_u8 = 0;
    g_TitlePage_s8 = 0;
    g_TitleState_s8 = 0;
    g_OpenMenuFlag = 0;
    g_TitleOptionPad = 0;
    title_init();
    (g_ApiCallPtr = api_VSync)(NULL);
    rect_x = 0;
    rect_y = 0;
    rect_w = 0x1E0;
    rect_h = 0x1E0;
    (g_ApiCallPtr = api_ClearImage)(&rect_x, 0, 0, 0);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, 0xF0, 0, 0, 1);
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    g_SelectedFmvId = FMV_TITLE;
    g_FmvBuffer = g_LoadPointer;
    g_GameStateFlags |= GAME_FMV_REQUEST;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    do {
        title_menu_update();
        (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    } while (g_TitleRunning_u8 != 0);
    switch (g_TitleMenuChoice_u8) {
    case 0:
        (g_ApiCallPtr = api_sound_end)();
        g_StatusFlags |= STATUS_ATTRACT_DEMO;
        (g_ApiCallPtr = api_task_chain)(g_ApiCallPtr = api_game_start);
        (g_ApiCallPtr = api_load_and_schedule_overlay)((s16 *)0x154);
        return;
    case 1:
    case 4:
    case 5:
    case 6:
    case 7:
        (g_ApiCallPtr = api_sound_end)();
        (g_ApiCallPtr = api_load_and_schedule_overlay)((s16 *)0x156);
        /* fallthrough */
    case 2:
        if ((g_ApiCallPtr = api_memcard_screen)((s16 *)1, ADDR_g_TitleMemcardWorkBuf, 0, 1) != 0) {
            g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
            (g_ApiCallPtr = api_sound_end)();
            (g_ApiCallPtr = api_load_and_schedule_overlay)((s16 *)0x15E);
        }
        g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
        (g_ApiCallPtr = api_sound_end)();
        (g_ApiCallPtr = api_task_chain)(g_ApiCallPtr = api_game_start);
        return;
    case 3:
        g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
        (g_ApiCallPtr = api_sound_end)();
        (g_ApiCallPtr = api_task_chain)(g_ApiCallPtr = api_options_menu);
        break;
    }
}

void title_init(void) {
    g_VSyncMode = 0;
    (g_ApiCallPtr = api_set_bg_image_buffer_params)(0x140, 0xF0, 0);
    (g_ApiCallPtr = api_load_file)(0x1C, ADDR_g_DoorFile);
    (g_ApiCallPtr = api_load_file)(0x1D, ADDR_g_TitleMojiBuf);
    g_VramClutRowCursor = 0x14;
    g_VramTpageCursor = 8;
    g_TitleSpriteTpage[0] = g_VramTpageCursor;
    g_TitleSpriteClutRow[0] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf);
    g_TitleSpriteTpage[1] = g_TitleSpriteTpage[2] = g_TitleSpriteTpage[3] = g_VramTpageCursor;
    g_TitleSpriteClutRow[1] = g_TitleSpriteClutRow[2] = g_TitleSpriteClutRow[3] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf_Tim1);
    g_TitleSpriteTpage[4] = g_VramTpageCursor;
    g_TitleSpriteClutRow[4] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf_Tim2);
    g_TitleSpriteTpage[5] = g_VramTpageCursor;
    g_TitleSpriteClutRow[5] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf_Tim3);
    g_TitleSpriteTpage[6] = g_VramTpageCursor;
    g_TitleSpriteClutRow[6] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf_Tim4);
    g_TitleSpriteTpage[7] = g_VramTpageCursor;
    g_TitleSpriteClutRow[7] = g_VramClutRowCursor;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_TitleMojiBuf_Tim5);
    g_TitleRunning = 1;
    g_TitleMenuChoice = 1;
    g_GameStateFlags &= ~GAME_SCREEN_MODE;
}

void title_menu_update(void) {
    void title_draw_option_k() __asm__("title_draw_option");
    void title_draw_mode_option(u8 bright, u8 option);
    void title_request_exit();
    extern u8 g_TitleMenuChoice_u8 __asm__("g_TitleMenuChoice");
    extern TitleTimers g_TitleTimers;
    s32 unused[4];
    switch (g_TitlePage) {
    case 0:
        switch (g_TitleState) {
        case 0:
            g_TitleState = 1;
            TITLE_TMR->timer = 0x80U;
            /* fallthrough */
        case 1:
            TITLE_TMR->timer = (u16) (TITLE_TMR_AS(timer, s16) - 4);
            title_draw_option_k((-0x80 - (u8) TITLE_TMR->timer) & 0xFF, 0U);
            if ((s16) TITLE_TMR->timer == 0) {
                g_TitleState = 2;
                TITLE_TMR->timer = 0x708U;
            }
            if (g_PadRawPressed32 & 0x9FF) {
                g_TitleState = 2;
                TITLE_TMR->timer = 0x708U;
            }
            break;
        case 2:
            TITLE_TMR_AS(timer, s16)--;
            title_draw_option_k(0x80, 0U);
            if ((s16) TITLE_TMR->timer == 0) {
                g_TitleState = 3;
                g_FadeTypeId = 2;
                g_FadeStep = 0x400;
                (g_ApiCallPtr = api_fade_update)();
            }
            if (g_PadRawPressed32 & 0x9FF) {
                g_TitlePage = 1;
                g_TitleState = 2;
            }
            break;
        case 3:
            if (g_FadeLevel >= 0x7B81) {
                g_TitleMenuChoice_u8 = 0;
                g_FadeLevel = 0x7FFF;
                g_FadeStep = 0;
                g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
                title_request_exit();
            }
            if (g_PadRawPressed32 & 0x9FF) {
                g_TitleState = 2;
                g_FadeLevel = -1;
                TITLE_TMR->timer = 0x708U;
            }
            title_draw_option_k(0x80, 0U);
            break;
        case 4:
            title_draw_option_k(0x80, 0U);
            if (g_FadeLevel >= 0x7B81) {
                g_FadeLevel = 0x7FFF;
                g_FadeStep = 0;
                g_TitlePage = 1;
                g_TitleState = 0;
                g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
            }
        }
        return;
    case 1:
        __asm__(".section .rodata\n\t.word 0\n\t.text");
        switch (g_TitleState) {
        case 0:
            g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
            g_TitleState = 1;
            g_FadeTypeId = 2;
            g_FadeStep = -0x400;
            (g_ApiCallPtr = api_fade_update)();
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                g_TitleMenuChoice_u8 = 1;
            }
            return;
        case 1:
            if (g_FadeLevel < 0) {
                g_TitleState = 2;
                TITLE_TMR->timer = 0x708U;
            }
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
            return;
        case 2:
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
            if (g_PadRawPressed32 & 0x8F0) {
                if (g_TitleMenuChoice_u8 == 1) {
                    g_TitleMenuChoice_u8 = 4;
                    g_TitleState = 0xA;
                    TITLE_TMR->timer = 0x708U;
                    TITLE_TMR->hold_timer = 0x5A;
                } else {
                    g_StatusFlags &= ~STATUS_TRAINING;
                    g_StatusFlags &= ~STATUS_ADVANCED;
                    g_StatusFlags &= ~STATUS_ADVANCED_HOLD;
                    g_SaveDcMode = 0;
                    (g_ApiCallPtr = api_play_sfx)((s32 *)1, 0, 0);
                    (g_ApiCallPtr = api_play_sfx)((s32 *)1, 1, 0);
                    g_TitleState = 6;
                    g_FadeTypeId = 1;
                    g_FadeStep = 0x7F00;
                    (g_ApiCallPtr = api_fade_update)();
                    g_VSyncMode = 2;
                    if (g_TitleMenuChoice_u8 == 2) {
                        (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)3, 0);
                        (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)4, 0xFF, 0);
                        (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)2, 0xFF, 0x3C, 0xA);
                    }
                }
                return;
            }
            if (g_PadRawPressed32 & 0x5100) {
                if (g_PadRawPressed32 & 0x1000) {
                    g_TitleMenuChoice_u8--;
                    if (g_TitleMenuChoice_u8 == 0) {
                        g_TitleMenuChoice_u8 = 3;
                    }
                } else {
                    g_TitleMenuChoice_u8++;
                    if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                        g_TitleMenuChoice_u8 = 1;
                    }
                }
                TITLE_TMR->timer = 0x708U;
            }
            TITLE_TMR_AS(timer, s16)--;
            if ((s16) TITLE_TMR->timer == 0) {
                g_TitleState = 3;
                g_FadeTypeId = 2;
                g_FadeStep = 0x400;
                (g_ApiCallPtr = api_fade_update)();
            }
            return;
        case 3:
            if (g_FadeLevel < 0) {
                g_TitleMenuChoice_u8 = 0;
                title_request_exit();
                return;
            }
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
            if (g_PadRawPressed32 & 0x9FF) {
                g_TitleState = 0;
                g_FadeStep = -0x1000;
            }
            return;
        case 4:
            if (g_FadeLevel < 0) {
                title_request_exit();
                g_GameStateFlags &= GAME_SCREEN_MODE;
                return;
            }
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
        case 6:
            if (g_FadeLevel < 0) {
                g_TitleState = 7;
                g_FadeTypeId = 1;
                g_FadeStep = -0x4000;
                (g_ApiCallPtr = api_fade_update)();
                if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                    title_draw_option_k(0x80, g_TitleMenuChoice_u8);
                } else {
                    title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
                }
                return;
            }
        case 7:
            if (g_FadeLevel < 0) {
                g_TitleState = 8;
                g_FadeTypeId = 1;
                g_FadeStep = -0x8000;
                (g_ApiCallPtr = api_fade_update)();
                if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                    title_draw_option_k(0x80, g_TitleMenuChoice_u8);
                } else {
                    title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
                }
                return;
            }
        case 8:
            if (g_FadeLevel < 0) {
                g_TitleState = 9;
                g_FadeTypeId = 1;
                g_FadeStep = -0x800;
                (g_ApiCallPtr = api_fade_update)();
                if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                    title_draw_option_k(0x80, g_TitleMenuChoice_u8);
                } else {
                    title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
                }
                return;
            }
        case 9:
            if (g_FadeLevel < 0) {
                g_TitleState = 4;
                g_FadeTypeId = 2;
                g_FadeStep = 0x270;
                (g_ApiCallPtr = api_fade_update)();
                if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                    title_draw_option_k(0x80, g_TitleMenuChoice_u8);
                } else {
                    title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
                }
                return;
            }
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
            return;
        case 10:
            if (g_PadRawPressed32 & 0x5100) {
                if (g_PadRawPressed32 & 0x1000) {
                    g_TitleMenuChoice_u8--;
                    if ((u8) g_TitleMenuChoice_u8 < 4U) {
                        g_TitleMenuChoice_u8 = 6;
                    }
                } else {
                    g_TitleMenuChoice_u8++;
                    if ((u8) g_TitleMenuChoice_u8 >= 7U) {
                        g_TitleMenuChoice_u8 = 4;
                    }
                }
                TITLE_TMR->timer = 0x708U;
            }
            if (g_PadRawPressed32 & 0x8F0) {
                (g_ApiCallPtr = api_pad_vibrate_small)((s32 *)3, 0);
                (g_ApiCallPtr = api_pad_vibrate_large)((s32 *)4, 0xFF, 0);
                (g_ApiCallPtr = api_pad_vibrate_large_ramp)((s32 *)2, 0xFF, 0x3C, 0xA);
                g_StatusFlags &= ~STATUS_TRAINING;
                g_StatusFlags &= ~STATUS_ADVANCED;
                g_StatusFlags &= ~STATUS_ADVANCED_HOLD;
                g_SaveDcMode = 0;
                if (g_TitleMenuChoice_u8 == 5) {
                    g_StatusFlags |= STATUS_TRAINING;
                    g_SaveDcMode = 1;
                }
                if (g_TitleMenuChoice_u8 == 6) {
                    g_StatusFlags |= STATUS_ADVANCED;
                    g_SaveDcMode = 2;
                    if (TITLE_TMR->hold_timer == 0) {
                        g_TitleMenuChoice_u8 = 7;
                        g_StatusFlags |= STATUS_ADVANCED_HOLD;
                        g_SaveDcMode = 3;
                    }
                }
                (g_ApiCallPtr = api_play_sfx)((s32 *)1, 0, 0);
                (g_ApiCallPtr = api_play_sfx)((s32 *)1, 1, 0);
                g_TitleState = 6;
                g_FadeTypeId = 1;
                g_FadeStep = 0x7F00;
                (g_ApiCallPtr = api_fade_update)();
                g_VSyncMode = 2;
                return;
            }
            if (g_TitleMenuChoice_u8 == 6) {
                if (g_PadRaw32 & 0x2000) {
                    TITLE_TMR->hold_timer--;
                    if (TITLE_TMR->hold_timer < 0) {
                        TITLE_TMR->hold_timer = 0;
                    }
                }
                if (TITLE_TMR->hold_timer == 0) {
                    g_TitleMenuChoice_u8 = 7;
                }
            }
            if ((u8) g_TitleMenuChoice_u8 >= 4U) {
                title_draw_option_k(0x80, g_TitleMenuChoice_u8);
            } else {
                title_draw_mode_option(0x80, g_TitleMenuChoice_u8);
            }
            if (g_TitleMenuChoice_u8 == 7) {
                g_TitleMenuChoice_u8 = 6;
            }
            TITLE_TMR_AS(timer, s16)--;
            if ((s16) TITLE_TMR->timer == 0) {
                g_TitleState = 3;
                g_FadeTypeId = 2;
                g_FadeStep = 0x400;
                (g_ApiCallPtr = api_fade_update)();
            }
            if ((g_PadRaw32 & 4) && (g_PadRaw32 & 8)) {
                g_TitleState = 0;
            }
            break;
        }
    }
}

void title_nop(void) {

}

void title_draw_option(u8 bright, u8 option) {
    s32 unused;
    s32 unused2;
    if (bright == 0x80) {
        LIT(s32, sp_sprite_attribute) = 0x10000000;
    } else {
        LIT(s32, sp_sprite_attribute) = 0x50000000;
    }
    LIT(s16, sp_sprite_x) = -0x82;
    LIT(s16, sp_sprite_y) = 0x26;
    LIT(s16, sp_sprite_w) = 0x100;
    LIT(s16, sp_sprite_h) = 0x50;
    LIT(u16, sp_sprite_tpage) = g_TitleSpriteTpage[option];
    LIT(s8, sp_sprite_u) = 0;
    LIT(s8, sp_sprite_v) = 0;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(s16, sp_sprite_cy) = g_TitleSpriteClutRow[option] + 0x1E0;
    LIT(u8, sp_sprite_r) = bright;
    LIT(u8, sp_sprite_g) = bright;
    LIT(u8, sp_sprite_b) = bright;
    (g_ApiCallPtr = api_GsSortFastSprite)(ADDR_sp_sprite, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 2);
}

void title_draw_mode_option(u8 bright, u8 option) {
    s32 unused;
    s32 unused2;
    if (bright == 0x80) {
        LIT(s32, sp_sprite_attribute) = 0x10000000;
    } else {
        LIT(s32, sp_sprite_attribute) = 0x50000000;
    }
    LIT(s16, sp_sprite_x) = -0x82;
    LIT(s16, sp_sprite_y) = 0x26;
    LIT(s16, sp_sprite_w) = 0x100;
    LIT(s16, sp_sprite_h) = 0x50;
    LIT(u16, sp_sprite_tpage) = g_TitleSpriteTpage[1];
    LIT(s8, sp_sprite_u) = 0;
    LIT(s8, sp_sprite_v) = 0;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(s16, sp_sprite_cy) = g_TitleSpriteClutRow[1] + 0x1DF + option;
    LIT(u8, sp_sprite_r) = bright;
    LIT(u8, sp_sprite_g) = bright;
    LIT(u8, sp_sprite_b) = bright;
    (g_ApiCallPtr = api_GsSortFastSprite)(ADDR_sp_sprite, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 2);
}

void title_request_exit(void) {
    g_TitleRunning = 0;
    g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
}
