/* options.c - The options menu (controller, sound, screen adjust). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 keyconfig_update(void);
void keyconfig_draw_layout(void);
s32 pad_get_repeat(u8 *timer, u16 *prev_held, u8 rates, s32 mask);
void cut_set(void);
void fade_update(void);
void set_fading(s8 fade_type, s16 step);
void load_file_blocking(void);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
void print_text_8x14(s32 x, s32 y, s32 attr, u8 semi_trans);
void load_and_schedule_overlay(s32 file);
void task_sleep(s16 frames);
void task_exit(void);
void task_suspend(s32 slot);
void task_resume(s32 slot);


typedef struct {
    u8 state;
    u8 keycfg_state;
    u8 keycfg_mode;
    u8 sel;
    u8 pad4[5];
    u8 repeat_timer;
    u16 repeat_prev_pad;
    s8 ctrl_fade_state;
    u8 padD[3];
    TILE tile[2];
    DR_MODE dm[2];
    s16 ctrl_fade_level;
} OptionsWork;

__asm__("#JTALIGN2");
void options_menu(void) {
    u8 keyconfig_update_u(void) __asm__("keyconfig_update");
    void load_file_blocking_l(s32, s32) __asm__("load_file_blocking");
    void joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void print_text_8x14_l(s32, s32, u8, u8) __asm__("print_text_8x14");
    void task_sleep_l(s32) __asm__("task_sleep");
    void options_menu_exit(void);
    void options_menu_draw_boxes(void);
    void options_update_controller_image(u8 *);
    void pad_vibrate_small(s32, s32);
    void pad_vibrate_large(s32, s32, s32);
    void sound_end(void);
    void sound_load_bank3(s32);
    void tim_load_to_vram(s32);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
    void SetTile(TILE *p);
    void SetSemiTrans(void *p, int abe);
    int GetGraphType(void);
    void SsVabClose(short vab_id);
    int sprintf(char *, const char *, ...);
    extern u8 g_OptionsMenu_u8 __asm__("g_OptionsMenu");
    extern OptionsWork g_OptionsMenu_m __asm__("g_OptionsMenu");
    extern u8 g_VramTpageCursor_l __asm__("g_VramTpageCursor");
    OptionsWork *m;
    s32 save;
    s32 flags;
    u8 v;
    s32 i;
    s32 r;
    u8 on;
    u8 *p;
    u8 *q;
    u8 c;
    s32 one;
    u8 st;

    g_VSyncMode = 0;
    if (g_Tasks[1].state == 0) {
        g_OptionsFromTitle = 1;
        sound_init();
        if (LIT(s8, g_SndVabId1) >= 0) {
            SsVabClose(LIT(s8, g_SndVabId1));
            LIT(s8, g_SndVabId1) = -1;
        }
        save = LIT(s32, g_LoadPointer);
        LIT(s32, g_LoadPointer) = ADDR_g_DoorFile;
        sound_load_bank3(0);
        LIT(s32, g_LoadPointer) = save;
    } else {
        g_OptionsFromTitle = 0;
        task_suspend(0);
    }
    m = &g_OptionsMenu_m;
    m->state = 0;
    g_OptionsMenuSel = 0;
    v = LIT(u8, g_KeyConfigType) & 0xEF;
    flags = LIT(s32, g_StatusFlags);
    LIT(u8, g_KeyConfigType) = v;
    if (flags & STATUS_SCREEN_BORDER) {
        LIT(u8, g_KeyConfigType) = v | 0x10;
        LIT(s32, g_StatusFlags) = flags & ~STATUS_SCREEN_BORDER;
    }
    g_FadeTypeId = 2;
    g_FadeStep = -0x800;
    fade_update();
    LIT(u32, g_GameStateFlags) = (LIT(u32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
    load_file_blocking_l(4, ADDR_g_DoorFile);
    g_VramClutRowCursor = 0x1F;
    g_VramTpageCursor_l = 0x15;
    tim_load_to_vram(ADDR_g_DoorFile);
    task_sleep_l(1);
    g_OptionsControllerImage = 0xFF;
    LIT(u32, g_GameStateFlags) = (LIT(u32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    SetTile(&m->tile[0]);
    SetTile(&m->tile[1]);
    SetSemiTrans(&m->tile[0], 1);
    SetSemiTrans(&m->tile[1], 1);
    SetDrawMode(&g_OptionsDrawMode[0], 0, 0, (GetGraphType() == 1 || GetGraphType() == 2) ? 0x300 : 0xC0, NULL);
    SetDrawMode(&g_OptionsDrawMode[1], 0, 0, (GetGraphType() == 1 || GetGraphType() == 2) ? 0x300 : 0xC0, NULL);
    g_OptionsControllerFadeLevel = 0;
    g_OptionsControllerFadeState = 0;
    for (i = 0; i < 2; i++) {
        g_OptionsMenu_m.tile[i].x0 = 0;
        g_OptionsMenu_m.tile[i].y0 = (1 - i) * 240 + 48;
        g_OptionsMenu_m.tile[i].w = 320;
        g_OptionsMenu_m.tile[i].h = 103;
    }
    for (;;) {
        /* loop-invariant 1s: hoisted by loop.c; their order sets the preheader order */
        st = 1;
        one = 1;
        p = &g_OptionsMenuSel;
        q = p + 6;
        options_update_controller_image(&g_OptionsControllerImage);
        switch (g_OptionsMenu_u8) {
        case 0:
            if (g_FadeLevel >= 0) {
                break;
            }
            g_OptionsMenu_u8 = st;
            g_OptionsRepeatPrevPad = 0;
            /* fallthrough */
        case 1:
            if (((g_PadHeldRaw & PSX_SELECT) || !(g_PadTrigger & PSX_START)) && !(g_PadTrigger & PSX_TRIANGLE)) {
                if (g_PadTrigger & PSX_CROSS) {
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                    switch (*p) {
                    case 0:
                        p[-3] = 2;
                        p[-2] = 0;
                        break;
                    case 1:
                        p[-3] = 3;
                        break;
                    case 2:
                        p[-3] = 5;
                        break;
                    case 3:
                        options_menu_exit();
                        break;
                    }
                } else {
                    r = pad_get_repeat(q, (u16 *)(q + 1), 0xE6, 0xA000);
                    if (r & 0xA000) {
                        play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                        if (r & 0x8000) {
                            if (g_OptionsMenuSel == 0) {
                                g_OptionsMenuSel = 4;
                            }
                            g_OptionsMenuSel--;
                        } else {
                            g_OptionsMenuSel++;
                            if (g_OptionsMenuSel == 4) {
                                g_OptionsMenuSel = 0;
                            }
                        }
                    }
                }
            } else {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
                options_menu_exit();
            }
            break;
        case 2:
            while (!keyconfig_update_u()) {
                task_sleep_l(1);
                options_update_controller_image(&g_OptionsControllerImage);
            }
            if (!(g_PadTrigger & PSX_START)) {
                g_OptionsMenu_u8 = 1;
                g_OptionsMenuSel = 0;
            } else {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
                options_menu_exit();
            }
            break;
        case 3:
            if ((g_PadHeldRaw & PSX_SELECT) || !(g_PadTrigger & PSX_START)) {
                if (!(g_PadTrigger & (PSX_TRIANGLE | PSX_CROSS))) {
                    if (g_PadTrigger & (PSX_UP | PSX_DOWN)) {
                        play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                        LIT(u8, g_KeyConfigType) ^= 0x80;
                        sound_set_stereo((LIT(u8, g_KeyConfigType) >> 7) ^ 1);
                    }
                    LITP(GsSPRITE, sp_sprite)->cy = 0x1FF;
                    LITP(GsSPRITE, sp_sprite)->tpage = 0x15;
                    LITP(GsSPRITE, sp_sprite)->y = -0x40;
                    LITP(GsSPRITE, sp_sprite)->w = 0x58;
                    LITP(GsSPRITE, sp_sprite)->cx = 0;
                    LITP(GsSPRITE, sp_sprite)->r = 0;
                    LITP(GsSPRITE, sp_sprite)->g = 0;
                    LITP(GsSPRITE, sp_sprite)->b = 0;
                    LITP(GsSPRITE, sp_sprite)->mx = 0;
                    LITP(GsSPRITE, sp_sprite)->my = 0;
                    LITP(GsSPRITE, sp_sprite)->rotate = 0;
                    LITP(GsSPRITE, sp_sprite)->attribute = 0x1000040;
                    LITP(GsSPRITE, sp_sprite)->x = -0x2C;
                    LITP(GsSPRITE, sp_sprite)->h = 0x12;
                    LITP(GsSPRITE, sp_sprite)->u = 0;
                    LITP(GsSPRITE, sp_sprite)->v = 0xE8;
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
                    LITP(GsSPRITE, sp_sprite)->y = -0x2C;
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
                    on = g_KeyConfigType >> 7;
                    c = on;
                    sprintf(LITP(char, sp_text), "STEREO");
                    print_text_8x14_l(0x88, 0x3B, c * 3, 0);
                    c = on ^ 1;
                    sprintf(LITP(char, sp_text), "MONO");
                    print_text_8x14_l(0x90, 0x4F, c * 3, 0);
                } else {
                    g_OptionsMenu_u8 = 1;
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                }
            } else {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
                options_menu_exit();
            }
            break;
        case 4:
            if (g_OptionsFromTitle != 0) {
                sound_end();
                load_and_schedule_overlay(0x15E);
            }
            if (!(LIT(u32, g_GameStateFlags) & GAME_FADE_ACTIVE)) {
                g_PlayerEntity.blend_counter = 0;
                g_PlayerEntity.animation_frame_id = 0;
                g_PlayerEntity.timing_control = 0;
                g_PlayerEntity.hit_state = 0;
                ENTITY_STATE_WORD(&g_PlayerEntity) = one;
                g_PlayerEntity.animation_id = 0;
                joint_move_l(0, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
                cut_set();
                LIT(u32, g_GameStateFlags) &= ~GAME_MENU_BYTE;
                v = LIT(u8, g_KeyConfigType);
                if (v & 0x10) {
                    LIT(u8, g_KeyConfigType) = v & 0xEF;
                    LIT(u32, g_StatusFlags) |= STATUS_SCREEN_BORDER;
                }
                g_VSyncMode = 2;
                task_resume(0);
                task_exit();
            }
            break;
        case 5:
            if ((g_PadHeldRaw & PSX_SELECT) || !(g_PadTrigger & PSX_START)) {
                if (!(g_PadTrigger & (PSX_TRIANGLE | PSX_CROSS))) {
                    if (g_PadTrigger & (PSX_UP | PSX_DOWN)) {
                        play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                        LIT(u8, g_KeyConfigType) ^= 0x20;
                    }
                    LITP(GsSPRITE, sp_sprite)->cy = 0x1FF;
                    LITP(GsSPRITE, sp_sprite)->tpage = 0x15;
                    LITP(GsSPRITE, sp_sprite)->y = -0x40;
                    LITP(GsSPRITE, sp_sprite)->w = 0x58;
                    LITP(GsSPRITE, sp_sprite)->cx = 0;
                    LITP(GsSPRITE, sp_sprite)->r = 0;
                    LITP(GsSPRITE, sp_sprite)->g = 0;
                    LITP(GsSPRITE, sp_sprite)->b = 0;
                    LITP(GsSPRITE, sp_sprite)->mx = 0;
                    LITP(GsSPRITE, sp_sprite)->my = 0;
                    LITP(GsSPRITE, sp_sprite)->rotate = 0;
                    LITP(GsSPRITE, sp_sprite)->attribute = 0x1000040;
                    LITP(GsSPRITE, sp_sprite)->x = -0x2C;
                    LITP(GsSPRITE, sp_sprite)->h = 0x12;
                    LITP(GsSPRITE, sp_sprite)->u = 0;
                    LITP(GsSPRITE, sp_sprite)->v = 0xE8;
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
                    LITP(GsSPRITE, sp_sprite)->y = -0x2C;
                    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), &LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)], 2);
                    on = (g_KeyConfigType >> 5) & 1;
                    c = on;
                    sprintf(LITP(char, sp_text), "ON");
                    print_text_8x14_l(0x99, 0x3B, c * 3, 0);
                    c = on ^ 1;
                    sprintf(LITP(char, sp_text), "OFF");
                    print_text_8x14_l(0x95, 0x4F, c * 3, 0);
                } else {
                    pad_vibrate_small(7, 0);
                    pad_vibrate_large(9, 0xFF, 0);
                    pad_vibrate_large_ramp(4, 0xFF, 0x3C, 0xA);
                    g_OptionsMenu_u8 = 1;
                    play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                }
            } else {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
                options_menu_exit();
            }
            break;
        }
        if (g_OptionsMenu_u8 != 4) {
            LITP(GsSPRITE, sp_sprite)->cy = 0x1FF;
            LITP(GsSPRITE, sp_sprite)->cx = 0;
            LITP(GsSPRITE, sp_sprite)->tpage = 0x15;
            LITP(GsSPRITE, sp_sprite)->r = 0;
            LITP(GsSPRITE, sp_sprite)->g = 0;
            LITP(GsSPRITE, sp_sprite)->b = 0;
            LITP(GsSPRITE, sp_sprite)->mx = 0;
            LITP(GsSPRITE, sp_sprite)->my = 0;
            LITP(GsSPRITE, sp_sprite)->rotate = 0;
            options_menu_draw_boxes();
            keyconfig_draw_layout();
        }
        task_sleep_l(1);
    }
}

void options_menu_exit(void) {
    g_OptionsMenu = 4;
    set_fading(2, 0x800);
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
}

void options_menu_draw_boxes(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    u8 i;
    u8 *p;
    u16 *tbl;
    s32 c;

    SCRATCH->rect_flags = 0x60000000;
    SCRATCH->rect_x = -0xA0;
    SCRATCH->rect_y = -0x48;
    SCRATCH->rect_w = 0x140;
    SCRATCH->rect_h = 0xC0;
    SCRATCH->rect_r = 0x48;
    SCRATCH->rect_g = 0x48;
    SCRATCH->rect_b = 0x48;
    GsSortBoxFill(LITP(GsBOXF, sp_box), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 3);
    SCRATCH->rect_y = -0x5F;
    SCRATCH->rect_h = 0xE;
    i = 4;
    tbl = g_OptionsMenuBoxX;
    c = 0x30;
    p = &g_OptionsMenuSel;
    do {
        i--;
        SCRATCH->rect_x = tbl[i];
        if (i != 0) {
            SCRATCH->rect_w = 0x2E;
        } else {
            SCRATCH->rect_w = c;
        }
        if (i == *p) {
            if (p[-3] == 3 || p[-3] == 5) {
                continue;
            }
            SCRATCH->rect_flags = 0x50000000;
            SCRATCH->rect_r = 0;
            SCRATCH->rect_g = 0;
            SCRATCH->rect_b = 0x80;
        } else {
            SCRATCH->rect_flags = 0x60000000;
            SCRATCH->rect_r = c;
            SCRATCH->rect_g = c;
            SCRATCH->rect_b = c;
        }
        GsSortBoxFill(LITP(GsBOXF, sp_box), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 2);
    } while (i != 0);
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    s16 w;
    s16 h;
} Tile16;

typedef struct {
    u8 pad0[0xC];
    u8 state;
    u8 flags;
    u8 pad1[2];
    Tile16 tile[2];
    u32 dr[2][3];
    s16 alpha;
} OptionsFade;

/* 4-aligned jump table (tools/jtalign.awk) */
/* The controller picture follows the connected pad by itself: a digital pad shows the standard pad, a
   Dual Shock the Dual Shock picture (seen in game) */
void options_update_controller_image(u8 *shown_type) {
    extern OptionsFade g_OptionsMenu_l __asm__("g_OptionsMenu");
    void load_file_blocking_l(s32, s32) __asm__("load_file_blocking");
    s32 n;

    switch (g_OptionsMenu_l.state) {
    case 0:
        if (*shown_type != g_PadType) {
            if (g_PadType == 7) {
                g_OptionsMenu_l.flags = 2;
                if (*shown_type != 0xFF) {
                    g_OptionsMenu_l.alpha = 0x3F;
                    g_OptionsMenu_l.state++;
                    g_OptionsMenu_l.flags = 0x82;
                } else {
                    g_OptionsMenu_l.state = 3;
                }
                *shown_type = g_PadType;
            } else if (*shown_type == 7 || *shown_type == 0xFF) {
                g_OptionsMenu_l.flags = 1;
                if (*shown_type != 0xFF) {
                    g_OptionsMenu_l.alpha = 0x3F;
                    g_OptionsMenu_l.state++;
                    g_OptionsMenu_l.flags = 0x81;
                } else {
                    g_OptionsMenu_l.state = 3;
                }
                *shown_type = g_PadType;
            }
        }
        break;
    case 1:
        if ((g_OptionsMenu_l.alpha += 0x40) == 0xFF) {
            g_OptionsMenu_l.state++;
        }
        break;
    case 2:
        g_OptionsMenu_l.state++;
        break;
    case 3:
        n = 0x13;
        if ((g_OptionsControllerState & 3) == 1) {
            n = 0x12;
        }
        load_file_blocking_l(n, ADDR_g_DoorFile);
        if (g_OptionsControllerState & 0x80) {
            g_OptionsMenu_l.alpha = 0xFF;
            g_OptionsMenu_l.state++;
        } else {
            g_OptionsMenu_l.state = 0;
        }
        break;
    case 4:
        if ((g_OptionsMenu_l.alpha -= 0x40) == 0x3F) {
            g_OptionsMenu_l.alpha = 0;
            g_OptionsMenu_l.state = 0;
        }
        break;
    }
    AddPrim(LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].org + 1, g_OptionsMenu_l.dr[LIT(u8, g_ActiveBuffer)]);
    g_OptionsMenu_l.tile[LIT(u8, g_ActiveBuffer)].r0 = g_OptionsMenu_l.tile[LIT(u8, g_ActiveBuffer)].g0 = g_OptionsMenu_l.tile[LIT(u8, g_ActiveBuffer)].b0 = g_OptionsMenu_l.alpha;
    AddPrim(LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].org, &g_OptionsMenu_l.tile[LIT(u8, g_ActiveBuffer)]);
}
