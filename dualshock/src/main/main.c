/* main.c - Program entry: main() (the per-frame display loop: fades, screen dim, FMV requests, soft reset),
   graphics init, global assets and the attract-demo timer. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void cd_init(void);
void xa_update(void);
void clut_fade_update(void);
void memcard_init_events(void);
void apply_screen_shake(void);
void update_screen_with_borders(void);
void fade_update(void);
void load_file(s32 a, s32 b);


void main(void) {
    void AddPrim(void *ot, void *p);
    void DrawOTag(u_long *p);
    int DrawSync(int mode);
    int GetGraphType(void);
    DRAWENV *PutDrawEnv(DRAWENV *env);
    int ResetGraph(int mode);
    DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
    void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
    void GsClearOt(u_short offset, u_short point, GsOT *otp);
    void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
    int GsGetActiveBuff(void);
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp);
    void GsSwapDispBuff(void);
    int VSync(int mode);
    void init_and_start_game(void);
    void pad_update(void);
    void update_demo_timer(void);
    void task_kill(s32);
    void init_graphics(s32);
    void vram_clear_rect(s16, s16, s16, s16);
    void task_execute_l(s32, void (*)(void)) __asm__("task_execute");
    void soft_reset_task(void);
    void sound_fade_update(void);
    void task_suspend(s32);
    void task_resume(s32);
    void task_scheduler_update(void);
    void update_message_display(void);
    void sound_poll_voice_status(void);
    void bg_image_upload(s16);
    void fmv_play_l(u8, s32, s32) __asm__("fmv_play");
    DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
    void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
    int GetGraphType(void);
    void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
    void GsClearOt(u_short offset, u_short point, GsOT *otp);
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    int DrawSync(int mode);
    int VSync(int mode);
    void DrawOTag(u_long *p);
    int GsGetActiveBuff(void);
    void AddPrim(void *ot, void *p);
    int ResetGraph(int mode);
    void GsSwapDispBuff(void);
    void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp);
    DRAWENV *PutDrawEnv(DRAWENV *env);
    extern u8 g_VSyncMode_l __asm__("g_VSyncMode");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    extern u8 g_Fade2BlendMode_l __asm__("g_Fade2BlendMode");
    extern u8 g_FadeTypeId_l __asm__("g_FadeTypeId");
    extern POLY_F4 g_ScreenDimPolys_l2[2][2] __asm__("g_ScreenDimPolys");
    extern s32 g_PacketArea_l[] __asm__("g_PacketArea");
    DRAWENV env[2];
    DR_MODE dm[2];
    u16 noreset;
    u16 acc;
    u16 sum;
    u16 cur;
    u16 step;
    u16 *fade0;
    u16 *fade1;
    u16 cur1;
    POLY_F4 *poly;
    POLY_F4 *poly1;
    u8 buf;
    u8 level;
    u8 level1;
    u8 *level_p;
    s32 shade;

    init_and_start_game();
    SetDefDrawEnv(&env[0], 0, 0xF0, 0x140, 0xF0);
    SetDefDrawEnv(&env[1], 0, 0, 0x140, 0xF0);
    SetDrawMode(&dm[0], 0, 0, (GetGraphType() == 1 || GetGraphType() == 2) ? 0x300 : 0xC0, NULL);
    SetDrawMode(&dm[1], 0, 0, (GetGraphType() == 1 || GetGraphType() == 2) ? 0x300 : 0xC0, NULL);
    while (1) {
        pad_update();
        update_demo_timer();
        if ((g_GameStateFlags & GAME_MENU_BYTE) == 0x8000) {
            if ((g_PadHeldRaw & (PSX_SELECT | PSX_START)) == 0x900) {
                LIT(u8, g_SoftResetCounter) += g_VSyncMode_l ? 2 : 1;
                if (LIT(u8, g_SoftResetCounter) >= 0x79) {
                    LIT(u8, g_SoftResetCounter) = 0;
                    task_kill(0);
                    task_kill(1);
                    task_kill(2);
                    g_MessageStatus_l = 0;
                    g_FadeLevel = g_Fade2Level = -1;
                    init_graphics(0);
                    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
                    vram_clear_rect(0, 0, 0x1E0, 0x1E0);
                    task_execute_l(0, soft_reset_task);
                }
            } else {
                g_SoftResetCounter = 0;
            }
        }
        if (LIT(s32, g_GameStateFlags) & GAME_GRAPHICS_RESET) {
            init_graphics(1);
        }
        if (g_SndFadeActive != 0) {
            sound_fade_update();
        }
        if (LIT(s32, g_GameStateFlags) & GAME_FADE_ACTIVE) {
            if (g_FadeLevel < 0) {
                g_FadeBox.attribute = (g_FadeTypeId_l << 28) | 0x40000000;
                acc = 0;
                if (g_ActiveBuffer == 0) {
                    GsDefDispBuff(0, 0, 0, 0);
                } else {
                    GsDefDispBuff(0, 0xF0, 0, 0xF0);
                }
                noreset = 0;
                g_FadeLevel = -({ g_FadeStep < 1; }) & 0x7FFF;
                if (g_Tasks[0].state & 0x40) {
                    noreset = g_Tasks[0].state;
                } else {
                    task_suspend(0);
                }
            }
            step = LIT(u16, g_FadeStep);
            acc += step;
            sum = acc;
            g_FadeBox.r = g_FadeBox.g = g_FadeBox.b = step >> 7;
            if ((sum >> 10) == 1) {
                g_FadeBox.r = g_FadeBox.g = g_FadeBox.b = 8;
            }
            acc = sum & 0x2FF;
            GsSetWorkBase(g_PacketArea_l[LIT(u8, g_ActiveBuffer)]);
            GsClearOt(0, 0, LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer));
            GsSortBoxFill(&g_FadeBox, LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 0);
            task_scheduler_update();
            DrawSync(0);
            VSync(0);
            DrawOTag((u_long *)LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].tag);
            LIT(s16, g_FadeLevel) += LIT(s16, g_FadeStep);
            if (LIT(s16, g_FadeLevel) >= 0) {
                continue;
            }
            LIT(s16, g_FadeStep) = 0;
            LIT(s16, g_FadeLevel) = 0;
            LIT(s32, g_GameStateFlags) &= ~GAME_FADE_ACTIVE;
            GsDefDispBuff(0, 0, 0, 0xF0);
            if (noreset == 0) {
                task_resume(0);
            }
        }
        LIT(u8, g_ActiveBuffer) = GsGetActiveBuff();
        g_DispBufferY = -({ LIT(u8, g_ActiveBuffer) == 0; }) & 0xF0;
        GsSetWorkBase(g_PacketArea_l[LIT(u8, g_ActiveBuffer)]);
        GsClearOt(0, 0, LITP(GsOT, g_OtMain) + LIT(u8, g_ActiveBuffer));
        GsClearOt(0, 0, LITP(GsOT, g_OtBg) + LIT(u8, g_ActiveBuffer));
        GsClearOt(0, 0, LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer));
        task_scheduler_update();
        if (g_MessageStatus_l & 0x80) {
            update_message_display();
        }
        fade0 = LITP(u16, g_FadeLevel);
        cur = *fade0;
        if ((s16)cur >= 0) {
            g_FadeBox.attribute = (g_FadeTypeId_l << 28) | 0x40000000;
            g_FadeBox.r = g_FadeBox.g = g_FadeBox.b = (s16)cur >> 7;
            *fade0 = cur + g_FadeStep;
            GsSortBoxFill(&g_FadeBox, LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 0);
        }
        if (g_GameStateFlags & GAME_SCREEN_DIM) {
            if (LIT(u8, g_ScreenDimLevel) < 0xF0) {
                LIT(u8, g_ScreenDimLevel) += 0x10;
            }
        } else {
            if (LIT(u8, g_ScreenDimLevel) >= 0x10) {
                LIT(u8, g_ScreenDimLevel) -= 0x10;
            }
        }
        buf = LIT(u8, g_ActiveBuffer);
        level_p = LITP(u8, g_ScreenDimLevel);
        level = *level_p;
        poly = &g_ScreenDimPolys[buf];
        poly->r0 = level;
        poly->g0 = level;
        g_ScreenDimPolys[buf].b0 = level;
        buf = LIT(u8, g_ActiveBuffer);
        level1 = *level_p;
        poly1 = &g_ScreenDimPolys[buf];
        poly1[2].r0 = level1;
        poly1[2].g0 = level1;
        g_ScreenDimPolys_l2[1][buf].b0 = level1;
        if (!(g_GameStateFlags & (GAME_MENU_OPEN | GAME_DOOR_ANIM))) {
            if (*level_p != 0) {
                AddPrim(LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].org + 4, &dm[LIT(u8, g_ActiveBuffer)]);
                AddPrim(LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].org + 3, &g_ScreenDimPolys[LIT(u8, g_ActiveBuffer)]);
                AddPrim(LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].org + 3, &g_ScreenDimPolys[LIT(u8, g_ActiveBuffer) + 2]);
            }
            fade1 = LITP(u16, g_Fade2Level);
            cur1 = *fade1;
            if ((s16)cur1 >= 0) {
                shade = (s16)cur1 >> 7;
                g_FadeBox.attribute = (g_Fade2BlendMode_l << 28) | 0x40000000;
                g_FadeBox.r = g_FadeBox.g = g_FadeBox.b = shade;
                g_FadeBox.r = shade & g_Fade2MaskR;
                g_FadeBox.g = shade & g_Fade2MaskG;
                g_FadeBox.b = shade & g_Fade2MaskB;
                *fade1 = cur1 + g_SpecialRoomLightDelta;
                GsSortBoxFill(&g_FadeBox, LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 5);
            }
        }
        clut_fade_update();
        DrawSync(0);
        VSync(g_VSyncMode_l);
        sound_poll_voice_status();
        ResetGraph(1);
        GsSwapDispBuff();
        if (g_GameStateFlags & GAME_SCREEN_CLEAR) {
            GsSortClear(g_ClearColorR, g_ClearColorG, g_ClearColorB, LITP(GsOT, g_OtBg) + g_ActiveBuffer);
        } else if (g_GameStateFlags < 0) {
            if (g_StatusFlags & STATUS_SCREEN_BORDER) {
                update_screen_with_borders();
            } else {
                bg_image_upload(-({ g_ActiveBuffer == 0; }) & 0xF0);
            }
        }
        if ((g_StatusFlags & STATUS_SCREEN_SHAKE) && !(g_GameStateFlags & GAME_MENU_BYTE)) {
            apply_screen_shake();
        }
        DrawSync(0);
        if (LIT(s32, g_GameStateFlags) & GAME_FMV_REQUEST) {
            fmv_play_l(g_SelectedFmvId, g_FmvBuffer, 1);
            g_PadPressed = 0;
            g_PadHeld = 0;
            LIT(s32, g_GameStateFlags) &= ~GAME_FMV_REQUEST;
        }
        DrawOTag((u_long *)LITP(GsOT, g_OtBg)[LIT(u8, g_ActiveBuffer)].tag);
        DrawOTag((u_long *)LITP(GsOT, g_OtMain)[LIT(u8, g_ActiveBuffer)].tag);
        DrawOTag((u_long *)LITP(GsOT, g_OrderingTables)[LIT(u8, g_ActiveBuffer)].tag);
        PutDrawEnv(&env[LIT(u8, g_ActiveBuffer)]);
        xa_update();
    }
}

void init_and_start_game(void) {
    void memcard_init_events(void);
    void init_graphics(s32);
    void task_scheduler_init(void);
    void task_execute(s32, void (*)(void));
    void pad_set_default_bindings(void);
    void pad_vibration_reset(void);
    void clear_game_state_flags(void);
    void init_sound_and_fade_state(void);
    void InitGeom(void);
    void load_global_assets(void);
    void init_geom_task(void);
    extern u8 g_PadBuf1_u8[] __asm__("g_PadBuf1");
    int SetGraphDebug(int level);
    void SetPolyF4(POLY_F4 *p);
    void SetSemiTrans(void *p, int abe);
    void GsClearDispArea(u_char r, u_char g, u_char b);
    void *ResetCallback(void);
    long SetRCnt(u_long spec, u_short target, long mode);
    long StartRCnt(u_long spec);
    void _bu_init(void);
    void InitCARD_INIT(long val);
    long StartCARD_INIT(void);
    void PadInitDirect(u_char *pad1, u_char *pad2);
    void PadStartCom_PADENTRY(void);
    s32 i;

    ResetCallback();
    SetGraphDebug(0);
    InitGeom();
    InitCARD_INIT(0);
    StartCARD_INIT();
    _bu_init();
    PadInitDirect(g_PadBuf1_u8, g_PadBuf1_u8 + 0x24);
    PadStartCom_PADENTRY();
    init_graphics(0);
    GsClearDispArea(0, 0, 0);
    memcard_init_events();
    SetRCnt(0xF4000001, 1, 0x2000);
    StartRCnt(0xF4000001);
    g_VSyncMode = 2;
    g_OtMain[0].length = 0xA;
    g_OtMain[0].org = LITP(GsOT_TAG, g_OtMainTags_0);
    g_OtMain[1].length = 0xA;
    g_OtMain[1].org = LITP(GsOT_TAG, g_OtMainTags_1);
    g_OtBg[0].length = 4;
    g_OtBg[0].org = LITP(GsOT_TAG, g_OtBgTags_0);
    g_OtBg[1].length = 4;
    g_OtBg[1].org = LITP(GsOT_TAG, g_OtBgTags_1);
    g_OrderingTables[0].length = 4;
    g_OrderingTables[0].org = LITP(GsOT_TAG, g_OrderingTablesTags_0);
    g_OrderingTables[1].length = 4;
    g_OrderingTables[1].org = LITP(GsOT_TAG, g_OrderingTablesTags_1);
    pad_vibration_reset();
    clear_game_state_flags();
    init_sound_and_fade_state();
    g_PacketArea[0] = ADDR_g_PacketBuffer0;
    g_PacketArea[1] = ADDR_g_PacketBuffer1;
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
    task_scheduler_init();
    pad_set_default_bindings();
    g_KeyConfigType = 0;
    for (i = 0; i < 2; i++) {
        SetPolyF4(&g_ScreenDimPolys[i]);
        SetPolyF4(&g_ScreenDimPolys[i + 2]);
        SetSemiTrans(&g_ScreenDimPolys[i], 1);
        SetSemiTrans(&g_ScreenDimPolys[i + 2], 1);
    }
    task_execute(0, load_global_assets);
    task_execute(1, init_geom_task);
}

void init_graphics(s32 no_reset) {
    void reset_graphics(void);
    void center_screen_origin(void);
    void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
    void GsInit3D(void);
    void GsInitGraph(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
    void GsInitGraph2(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
    if (no_reset == 0) {
        GsInitGraph(0x140U, 0xF0U, 0U, 0U, 0U);
    } else {
        GsInitGraph2(0x140U, 0xF0U, 0U, 0U, 0U);
    }
    GsDefDispBuff(0U, 0U, 0U, 0xF0U);
    GsInit3D();
    reset_graphics();
    LIT(s32, g_GameStateFlags) &= ~GAME_GRAPHICS_RESET;
    center_screen_origin();
}

void reset_graphics(void) {
    s32 DecDCTReset(s32);
    s32 ResetGraph(s32);
    ResetGraph(0);
    DecDCTReset(0);
    cd_init();
}

void load_global_assets(void) {
    s32 fade_update_p() __asm__("fade_update");
    s32 load_file_p() __asm__("load_file");
    void task_execute(s32, void (*)(void));
    s32 task_sleep();
    s32 task_get_state();
    void load_global_textures(void);
    int DrawSync(int mode);
    int PadSetMainMode(int socket, int offs, int lock);
    s32 unused;
    u32 i;
    u32 j;
    u32 k;
    void *unused2;

    load_file_p(0xC, ADDR_g_DoorFile);
    task_execute(1, &load_global_textures);
    task_sleep(1);
    g_BgImageMode = 1;
    g_FadeTypeId = 2;
    g_FadeStep = -0x2000;
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    i = 0;
    fade_update_p();
    do {
        task_sleep(1);
        i += 1;
    } while (i < 4U);
    j = 0;
    do {
        task_sleep(1);
        j += 1;
    } while (j < 0x78U);
    g_FadeTypeId = 2;
    g_FadeStep = 0x2000;
    k = 0;
    fade_update_p();
    do {
        task_sleep(1);
        k += 1;
    } while (k < 4U);
    g_BgImageMode = 0;
    LIT(s32, g_GameStateFlags) = (s32) ((LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR);
    task_sleep(2);
    while (task_get_state(1) != 0) {
        task_sleep(1);
    }
    PadSetMainMode(0, 1, 2);
    DrawSync(0);
    load_and_schedule_overlay(0x154);
}

void center_screen_origin(void) {
    if (g_ScreenOffsetX == 0xA0) {
        g_FadeBox_X = -0xA0;
        g_FadeBox_Y = -0x78;
        return;
    }
    g_FadeBox_Y = 0;
    g_FadeBox_X = 0;
}

void update_demo_timer(void) {
    extern u16 g_DemoTimer_u16[] __asm__("g_DemoTimer");
    if ((g_StatusFlags & STATUS_ATTRACT_DEMO) && (g_UpdateFlags & 0x200) && (g_DemoTimer_u16[0] != 0)) {
        g_DemoTimer_u16[0] += 1;
        if ((s32) g_DemoTimer_u16[0] >= (g_DemoLength - 1)) {
            g_DemoTimer_u16[0] = 0;
        }
    }
}

void soft_reset_task(void) {
    s32 sound_shutdown();
    void init_sound_and_fade_state(void);
    s32 saved_flag;

    bgm_fade_out_all();
    sound_shutdown();
    saved_flag = LIT(s32, g_StatusFlags) & STATUS_NEW_GAME;
    mem_clear_words(ADDR_g_GameStateFlags, ADDR_g_KeyConfig);
    init_sound_and_fade_state();
    g_PacketArea[0] = ADDR_g_PacketBuffer0;
    g_PacketArea[1] = ADDR_g_PacketBuffer1;
    LIT(s32, g_StatusFlags) |= saved_flag;
    load_and_schedule_overlay(0x15E);
}

void load_global_textures(void) {
    s32 load_file_c3(s32, s32) __asm__("load_file");
    s32 task_exit();
    load_file_c3(0xB, ADDR_g_OverlayArea);
    LIT(s16, g_VramTpageCursor) = 0x1E;
    tim_load_to_vram(ADDR_g_OverlayArea);
    load_file_c3(0x1B, ADDR_g_OverlayArea);
    LIT(s16, g_VramTpageCursor) = 0x41C;
    tim_load_to_vram(ADDR_g_OverlayArea);
    task_exit();
}

void init_geom_task(void) {
    s32 task_exit();
    void InitGeom();             /* (Sony, LIBGTE; Capcom passes 0 although it takes no argument) */
    InitGeom(0);
    task_exit();
}
