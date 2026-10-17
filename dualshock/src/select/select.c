/* select.c - SELECT.EXE: the character select screen (Chris / Jill panels, scrolling, blinking
   arrows). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "overlay_api.h"

/* main-program globals this overlay uses (per-function aliases with other types stay in the functions) */
extern s32 g_GameStateFlags;
extern u8 g_ActiveBuffer;
extern s8 g_VSyncMode;
extern s8 g_RoomId;
extern s16 g_FadeLevel;
extern u8 g_SavePlayerId;
extern GsOT g_OrderingTables[];
extern s16 g_VramTpageCursor;
extern s16 g_FadeStep;
extern s32 g_LoadPointer;
extern u32 g_PadRaw32;
extern s32 g_PadRawPressed32;
extern s8 g_FadeTypeId;
extern s32 g_SelectRtpFlag;
extern s16 g_SelectMatrix;
extern s32 g_SelectRtpInterp;
extern s16 g_SelectRot;
extern s16 g_SelectRot_vy;
extern s16 g_SelectRot_vz;
extern s32 g_SelectTrans;
extern s32 g_SelectTrans_vy;
extern s32 g_SelectTrans_vz;
extern u8 g_SelectCharacter;
extern SVECTOR g_SelectQuadVerts[];
extern u8 g_SelectState;
extern u8 g_SelectSubState;
extern s8 g_SelectUnused;
/* the two character panels (0 = Chris, 1 = Jill): position, scroll speed / acceleration and their
   polygons (per frame buffer) */
typedef struct {
    s16 pos_x, pos_y, pos_z, vel_x;
    u16 vel_y;
    s8 accel_x, accel_y;
    u8 ot_layer, brightness;
    u8 pad[2];
    POLY_FT4 a[2][2];
    POLY_FT4 b[2][5];
} SelPlayer;
extern SelPlayer g_SelectPanels[2];
extern u8 g_SelectPadChoice;
extern u8 g_SelectTimer;

void select_state(void) {
    void select_draw_characters();
    void select_draw_arrows();
    void select_scroll_step();
    void select_scroll_accel();
    void select__main();
    typedef struct { u8 x, y, w, h; } SelUV;
    extern SelUV select_chris_uv_tbl[2];
    extern SelUV select_jill_uv_tbl[2];
    extern SelUV select_backdrop_uv_tbl[5];
    #define setUV4(p,_u0,_v0,_u1,_v1,_u2,_v2,_u3,_v3)     (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1),     (p)->u2 = (_u2), (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3)
    #define setRGB0(p,_r0,_g0,_b0) (p)->r0 = (_r0), (p)->g0 = (_g0), (p)->b0 = (_b0)
    u8 buf;
    u8 quad;

    select__main();
    g_VSyncMode = 0;
    g_SelectState = 0;
    g_SelectSubState = 0;
    g_SelectUnused = 0;
    (g_ApiCallPtr = api_sound_reinit)();
    g_LoadPointer = ADDR_g_LoadArea;
    g_RoomId = 0x1B;
    (g_ApiCallPtr = api_sound_load_bank1)(0xD, ADDR_g_LoadArea);
    (g_ApiCallPtr = api_load_file)(0x18, ADDR_g_DoorFile);
    g_VramTpageCursor = 0xA05;
    (g_ApiCallPtr = api_tim_load_to_vram)(ADDR_g_DoorFile);
    (g_ApiCallPtr = api_load_file)(0x19, ADDR_g_DoorFile);
    g_SelectRot = 0;
    g_SelectRot_vy = 0;
    g_SelectRot_vz = 0;
    g_SelectTrans = 0;
    g_SelectTrans_vy = 0;
    g_SelectTrans_vz = 0;
    (g_ApiCallPtr = api_RotMatrix)(&g_SelectRot, &g_SelectMatrix);
    (g_ApiCallPtr = api_TransMatrix)(&g_SelectMatrix, &g_SelectTrans);
    (g_ApiCallPtr = api_SetRotMatrix)(&g_SelectMatrix);
    (g_ApiCallPtr = api_SetTransMatrix)(&g_SelectMatrix);
    (g_ApiCallPtr = api_SetGeomScreen)(0xF0);
    buf = 2;
    do {
        buf--;
        quad = 2;
        do {
            quad--;
            (g_ApiCallPtr = api_SetPolyFT4)(&g_SelectPanels[0].a[buf][quad]);
            (g_ApiCallPtr = api_SetPolyFT4)(&g_SelectPanels[1].a[buf][quad]);
            setUV4(&g_SelectPanels[0].a[buf][quad], select_chris_uv_tbl[quad].x, select_chris_uv_tbl[quad].y, select_chris_uv_tbl[quad].x + select_chris_uv_tbl[quad].w, select_chris_uv_tbl[quad].y, select_chris_uv_tbl[quad].x, select_chris_uv_tbl[quad].y + select_chris_uv_tbl[quad].h, select_chris_uv_tbl[quad].x + select_chris_uv_tbl[quad].w, select_chris_uv_tbl[quad].y + select_chris_uv_tbl[quad].h);
            setUV4(&g_SelectPanels[1].a[buf][quad], select_jill_uv_tbl[quad].x, select_jill_uv_tbl[quad].y, select_jill_uv_tbl[quad].x + select_jill_uv_tbl[quad].w, select_jill_uv_tbl[quad].y, select_jill_uv_tbl[quad].x, select_jill_uv_tbl[quad].y + select_jill_uv_tbl[quad].h, select_jill_uv_tbl[quad].x + select_jill_uv_tbl[quad].w, select_jill_uv_tbl[quad].y + select_jill_uv_tbl[quad].h);
            g_SelectPanels[0].a[buf][quad].clut = (g_ApiCallPtr = api_GetClut)(0, quad + 0x1EA);
            g_SelectPanels[1].a[buf][quad].clut = (g_ApiCallPtr = api_GetClut)(0, quad * 2 + 0x1EA);
            g_SelectPanels[0].a[buf][quad].tpage = (g_ApiCallPtr = api_GetTPage)(1, 0, 0x140, 0);
            g_SelectPanels[1].a[buf][quad].tpage = (g_ApiCallPtr = api_GetTPage)(1, 0, 0x140, 0);
            (g_ApiCallPtr = api_SetSemiTrans)(&g_SelectPanels[0].a[buf][quad], 0);
            (g_ApiCallPtr = api_SetSemiTrans)(&g_SelectPanels[1].a[buf][quad], 0);
        } while (quad != 0);
        quad = 5;
        do {
            quad--;
            (g_ApiCallPtr = api_SetPolyFT4)(&g_SelectPanels[0].b[buf][quad]);
            (g_ApiCallPtr = api_SetPolyFT4)(&g_SelectPanels[1].b[buf][quad]);
            setUV4(&g_SelectPanels[0].b[buf][quad], select_backdrop_uv_tbl[quad].x, select_backdrop_uv_tbl[quad].y, select_backdrop_uv_tbl[quad].x + select_backdrop_uv_tbl[quad].w, select_backdrop_uv_tbl[quad].y, select_backdrop_uv_tbl[quad].x, select_backdrop_uv_tbl[quad].y + select_backdrop_uv_tbl[quad].h, select_backdrop_uv_tbl[quad].x + select_backdrop_uv_tbl[quad].w, select_backdrop_uv_tbl[quad].y + select_backdrop_uv_tbl[quad].h);
            setUV4(&g_SelectPanels[1].b[buf][quad], select_backdrop_uv_tbl[quad].x, select_backdrop_uv_tbl[quad].y, select_backdrop_uv_tbl[quad].x + select_backdrop_uv_tbl[quad].w, select_backdrop_uv_tbl[quad].y, select_backdrop_uv_tbl[quad].x, select_backdrop_uv_tbl[quad].y + select_backdrop_uv_tbl[quad].h, select_backdrop_uv_tbl[quad].x + select_backdrop_uv_tbl[quad].w, select_backdrop_uv_tbl[quad].y + select_backdrop_uv_tbl[quad].h);
            g_SelectPanels[0].b[buf][quad].clut = (g_ApiCallPtr = api_GetClut)(0, 0x1EA);
            g_SelectPanels[1].b[buf][quad].clut = (g_ApiCallPtr = api_GetClut)(0, 0x1EA);
            g_SelectPanels[0].b[buf][quad].tpage = (g_ApiCallPtr = api_GetTPage)(1, 2, 0x140, 0);
            g_SelectPanels[1].b[buf][quad].tpage = (g_ApiCallPtr = api_GetTPage)(1, 2, 0x140, 0);
            setRGB0(&g_SelectPanels[0].b[buf][quad], 0x40, 0x40, 0x40);
            setRGB0(&g_SelectPanels[1].b[buf][quad], 0x40, 0x40, 0x40);
            (g_ApiCallPtr = api_SetSemiTrans)(&g_SelectPanels[0].b[buf][quad], 1);
            (g_ApiCallPtr = api_SetSemiTrans)(&g_SelectPanels[1].b[buf][quad], 1);
        } while (quad != 0);
    } while (buf != 0);
    g_SelectCharacter = 0;
    g_SelectPanels[0].ot_layer = 0;
    g_SelectPanels[1].ot_layer = 1;
    g_SelectPanels[0].brightness = 0x80;
    g_SelectPanels[1].brightness = 0x50;
    g_SelectPanels[0].pos_x = 0x88;
    g_SelectPanels[0].pos_y = 0x48;
    g_SelectPanels[0].pos_z = 0xF0;
    g_SelectPanels[1].pos_x = 0xC8;
    g_SelectPanels[1].pos_y = 0x68;
    g_SelectPanels[1].pos_z = 0x110;
    g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    g_FadeTypeId = 2;
    g_FadeStep = -0x800;
    (g_ApiCallPtr = api_fade_update)();
do {
    LIT(s32, sp_sprite_attribute) = 0x01000040;
    LIT(s16, sp_sprite_tpage) = 5;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(u8, sp_sprite_r) = 0x80;
    LIT(u8, sp_sprite_g) = 0x80;
    LIT(u8, sp_sprite_b) = 0x80;
    LIT(s16, sp_sprite_mx) = 0;
    LIT(s16, sp_sprite_my) = 0;
    LIT(s32, sp_sprite_rotate) = 0;
    switch (g_SelectState) {
    case 0:
        if (g_FadeLevel >= 0) break;
        g_SelectState = 1;
        g_SelectTimer = 0;
    case 1:
        if ((g_PadRaw32 & 4) && (g_PadRaw32 & 8)) {
            g_SelectState = 7;
            g_SelectSubState = 0;
            g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
            (g_ApiCallPtr = api_set_fading)(2, 0xC00);
            break;
        }
        if (g_PadRawPressed32 & 0x8F0) {
            (g_ApiCallPtr = api_play_sfx)(1, 1, 0);
            g_SelectState = 3;
            g_SelectSubState = 0;
            g_FadeTypeId = 1;
            g_FadeStep = 0x100;
            (g_ApiCallPtr = api_fade_update)();
            break;
        }
        if (!(g_PadRaw32 & 0xA100)) {
            g_SelectTimer--;
            select_draw_arrows();
            break;
        }
        g_SelectState = 2;
        g_SelectSubState = 0;
        g_SelectPadChoice = (g_PadRaw32 >> 0xF) & 1;
        (g_ApiCallPtr = api_play_sfx)(1, 0, 0);
        if (g_SelectPadChoice != g_SelectCharacter) {
            g_SelectPanels[0].accel_x = -2;
            g_SelectPanels[0].accel_y = -1;
        } else {
            g_SelectPanels[0].accel_x = 2;
            g_SelectPanels[0].accel_y = 1;
        }
        g_SelectPanels[1].accel_x = -g_SelectPanels[0].accel_x;
        g_SelectPanels[1].accel_y = -g_SelectPanels[0].accel_y;
        g_SelectPanels[0].vel_x = 0;
        g_SelectPanels[0].vel_y = 0;
        g_SelectPanels[1].vel_x = 0;
        g_SelectPanels[1].vel_y = 0;
        g_SelectTimer = 8;
    case 2:
        g_SelectTimer--;
        switch (g_SelectSubState) {
        case 0:
            if (g_SelectTimer != 0) goto scroll;
            g_SelectSubState = 1;
            *(s8 *)&g_SelectTimer = (g_SelectPadChoice ^ 1) << 2;
            g_SelectPanels[0].vel_x += g_SelectPanels[0].accel_x;
            g_SelectPanels[0].vel_y += g_SelectPanels[0].accel_y;
            g_SelectPanels[1].vel_x += g_SelectPanels[1].accel_x;
            g_SelectPanels[1].vel_y += g_SelectPanels[1].accel_y;
        case 1:
            if (g_SelectTimer != 0) break;
            g_SelectSubState = 2;
            g_SelectTimer = 0xF;
            g_SelectPanels[0].accel_x = -g_SelectPanels[0].accel_x;
            g_SelectPanels[0].accel_y = -g_SelectPanels[0].accel_y;
            g_SelectPanels[1].accel_x = -g_SelectPanels[1].accel_x;
            g_SelectPanels[1].accel_y = -g_SelectPanels[1].accel_y;
        case 2:
            if (g_SelectPanels[0].vel_x == 0) {
                g_SelectSubState = 3;
                g_SelectPanels[0].ot_layer ^= 1;
                g_SelectPanels[1].ot_layer ^= 1;
            }
        case 3:
            g_SelectPanels[g_SelectCharacter].pos_z += 2;
            g_SelectPanels[g_SelectCharacter ^ 1].pos_z -= 2;
            g_SelectPanels[g_SelectCharacter].brightness -= 3;
            g_SelectPanels[g_SelectCharacter ^ 1].brightness += 3;
            if (g_SelectTimer != 0) goto scroll;
            g_SelectSubState = 4;
            g_SelectTimer = g_SelectPadChoice << 2;
            g_SelectPanels[0].vel_x += g_SelectPanels[0].accel_x;
            g_SelectPanels[0].vel_y += g_SelectPanels[0].accel_y;
            g_SelectPanels[1].vel_x += g_SelectPanels[1].accel_x;
            g_SelectPanels[1].vel_y += g_SelectPanels[1].accel_y;
        case 4:
            if (g_SelectTimer != 0) break;
            g_SelectSubState = 5;
            g_SelectTimer = 7;
            g_SelectPanels[0].accel_x = -g_SelectPanels[0].accel_x;
            g_SelectPanels[0].accel_y = -g_SelectPanels[0].accel_y;
            g_SelectPanels[1].accel_x = -g_SelectPanels[1].accel_x;
            g_SelectPanels[1].accel_y = -g_SelectPanels[1].accel_y;
        case 5:
            if (g_SelectTimer == 0) {
                g_SelectState = 1;
                g_SelectCharacter ^= 1;
            }
        default:
        scroll:
            select_scroll_accel();
        }
        select_scroll_step();
        break;
    case 3:
        if (g_FadeLevel >= 0) break;
        g_SelectState = 4;
        g_FadeTypeId = 1;
        g_FadeStep = -0x100;
        (g_ApiCallPtr = api_fade_update)();
        g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
        break;
    case 4:
        if (g_FadeLevel >= 0) break;
        g_SelectState = 5;
        g_SelectTimer = 0xF0;
        break;
    case 5:
        g_SelectTimer--;
        if (g_SelectTimer != 0) break;
    case 6:
        g_SavePlayerId = g_SelectCharacter;
        if (g_SelectCharacter != 0) {
            g_GameStateFlags |= GAME_ALT_RDT;
        }
        g_VSyncMode = 2;
        (g_ApiCallPtr = api_sound_end)();
        (g_ApiCallPtr = api_load_and_schedule_overlay)(0x155);
        break;
    case 7:
        if (!(g_GameStateFlags & GAME_FADE_ACTIVE)) {
            g_VSyncMode = 2;
            (g_ApiCallPtr = api_sound_end)();
            (g_ApiCallPtr = api_load_and_schedule_overlay)(0x15E);
        }
    }
    if (g_SelectState < 4) {
        select_draw_characters();
    }
    (g_ApiCallPtr = api_task_sleep)(1);
    } while (1);
}

void select_draw_characters(void) {
    #define setRGB0(p,_r0,_g0,_b0) (p)->r0 = (_r0), (p)->g0 = (_g0), (p)->b0 = (_b0)
    u8 panel;
    u8 quad;
    u8 depth_off;

    panel = 2;
    do {
        panel--;
        g_SelectQuadVerts[0].vx = g_SelectPanels[panel].pos_x - 0x60;
        g_SelectQuadVerts[0].vy = g_SelectPanels[panel].pos_y - 0x20;
        g_SelectQuadVerts[0].vz = g_SelectPanels[panel].pos_z;
        g_SelectQuadVerts[1].vx = g_SelectQuadVerts[0].vx + 0x50;
        g_SelectQuadVerts[1].vy = g_SelectQuadVerts[0].vy;
        g_SelectQuadVerts[1].vz = g_SelectQuadVerts[0].vz;
        g_SelectQuadVerts[2].vx = g_SelectQuadVerts[0].vx;
        g_SelectQuadVerts[2].vy = g_SelectQuadVerts[0].vy + 0x7F;
        g_SelectQuadVerts[2].vz = g_SelectQuadVerts[0].vz;
        g_SelectQuadVerts[3].vx = g_SelectQuadVerts[1].vx;
        g_SelectQuadVerts[3].vy = g_SelectQuadVerts[2].vy;
        g_SelectQuadVerts[3].vz = g_SelectQuadVerts[0].vz;
        (g_ApiCallPtr = api_RotTransPers4)(&g_SelectQuadVerts[0], &g_SelectQuadVerts[1], &g_SelectQuadVerts[2], &g_SelectQuadVerts[3], (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][0].x0, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][0].x1, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][0].x2, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][0].x3, &g_SelectRtpInterp, &g_SelectRtpFlag);
        g_SelectQuadVerts[0].vx = g_SelectQuadVerts[1].vx;
        g_SelectQuadVerts[1].vx = g_SelectQuadVerts[0].vx + 0x70;
        g_SelectQuadVerts[2].vx = g_SelectQuadVerts[0].vx;
        g_SelectQuadVerts[3].vx = g_SelectQuadVerts[1].vx;
        (g_ApiCallPtr = api_RotTransPers4)(&g_SelectQuadVerts[0], &g_SelectQuadVerts[1], &g_SelectQuadVerts[2], &g_SelectQuadVerts[3], (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][1].x0, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][1].x1, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][1].x2, (s32 *)&g_SelectPanels[panel].a[g_ActiveBuffer][1].x3, &g_SelectRtpInterp, &g_SelectRtpFlag);
        quad = 2;
        do {
            quad--;
            setRGB0(&g_SelectPanels[panel].a[g_ActiveBuffer][quad], g_SelectPanels[panel].brightness, g_SelectPanels[panel].brightness, g_SelectPanels[panel].brightness);
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].x0 -= 0xA0;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].y0 -= 0x78;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].x1 -= 0xA0;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].y1 -= 0x78;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].x2 -= 0xA0;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].y2 -= 0x78;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].x3 -= 0xA0;
            g_SelectPanels[panel].a[g_ActiveBuffer][quad].y3 -= 0x78;
            (g_ApiCallPtr = api_AddPrim)(LITP(GsOT, g_OrderingTables)[g_ActiveBuffer].org + (g_SelectPanels[panel].ot_layer + 4), &g_SelectPanels[panel].a[g_ActiveBuffer][quad]);
        } while (quad != 0);
        depth_off = (g_SelectPanels[panel].pos_z - 0xF0) >> 3;
        g_SelectQuadVerts[0].vx = g_SelectPanels[panel].pos_x - 0x57;
        g_SelectQuadVerts[0].vx -= depth_off;
        g_SelectQuadVerts[0].vy = g_SelectPanels[panel].pos_y + 0x5C;
        g_SelectQuadVerts[0].vy -= depth_off;
        g_SelectQuadVerts[0].vz = g_SelectPanels[panel].pos_z;
        g_SelectQuadVerts[1].vx = g_SelectQuadVerts[0].vx + 0xC;
        g_SelectQuadVerts[1].vy = g_SelectQuadVerts[0].vy;
        g_SelectQuadVerts[1].vz = g_SelectQuadVerts[0].vz;
        g_SelectQuadVerts[2].vx = g_SelectQuadVerts[0].vx;
        g_SelectQuadVerts[2].vy = g_SelectQuadVerts[0].vy + 0xC;
        g_SelectQuadVerts[2].vz = g_SelectQuadVerts[0].vz;
        g_SelectQuadVerts[3].vx = g_SelectQuadVerts[1].vx;
        g_SelectQuadVerts[3].vy = g_SelectQuadVerts[2].vy;
        g_SelectQuadVerts[3].vz = g_SelectQuadVerts[0].vz;
        (g_ApiCallPtr = api_RotTransPers4)(&g_SelectQuadVerts[0], &g_SelectQuadVerts[1], &g_SelectQuadVerts[2], &g_SelectQuadVerts[3], (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][0].x0, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][0].x1, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][0].x2, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][0].x3, &g_SelectRtpInterp, &g_SelectRtpFlag);
        g_SelectQuadVerts[0].vx += 0xB4;
        g_SelectQuadVerts[1].vx += 0xB4;
        g_SelectQuadVerts[2].vx += 0xB4;
        g_SelectQuadVerts[3].vx += 0xB4;
        (g_ApiCallPtr = api_RotTransPers4)(&g_SelectQuadVerts[0], &g_SelectQuadVerts[1], &g_SelectQuadVerts[2], &g_SelectQuadVerts[3], (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][1].x0, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][1].x1, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][1].x2, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][1].x3, &g_SelectRtpInterp, &g_SelectRtpFlag);
        g_SelectQuadVerts[0].vy -= 0x73;
        g_SelectQuadVerts[1].vy -= 0x73;
        g_SelectQuadVerts[2].vy -= 0x73;
        g_SelectQuadVerts[3].vy -= 0x73;
        (g_ApiCallPtr = api_RotTransPers4)(&g_SelectQuadVerts[0], &g_SelectQuadVerts[1], &g_SelectQuadVerts[2], &g_SelectQuadVerts[3], (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][2].x0, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][2].x1, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][2].x2, (s32 *)&g_SelectPanels[panel].b[g_ActiveBuffer][2].x3, &g_SelectRtpInterp, &g_SelectRtpFlag);
        g_SelectPanels[panel].b[g_ActiveBuffer][3].x0 = g_SelectPanels[panel].b[g_ActiveBuffer][0].x1;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].y0 = g_SelectPanels[panel].b[g_ActiveBuffer][0].y1;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].x1 = g_SelectPanels[panel].b[g_ActiveBuffer][1].x0;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].y1 = g_SelectPanels[panel].b[g_ActiveBuffer][1].y0;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].x2 = g_SelectPanels[panel].b[g_ActiveBuffer][0].x3;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].y2 = g_SelectPanels[panel].b[g_ActiveBuffer][0].y3;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].x3 = g_SelectPanels[panel].b[g_ActiveBuffer][1].x2;
        g_SelectPanels[panel].b[g_ActiveBuffer][3].y3 = g_SelectPanels[panel].b[g_ActiveBuffer][1].y2;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].x0 = g_SelectPanels[panel].b[g_ActiveBuffer][2].x2;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].y0 = g_SelectPanels[panel].b[g_ActiveBuffer][2].y2;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].x1 = g_SelectPanels[panel].b[g_ActiveBuffer][2].x3;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].y1 = g_SelectPanels[panel].b[g_ActiveBuffer][2].y3;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].x2 = g_SelectPanels[panel].b[g_ActiveBuffer][1].x0;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].y2 = g_SelectPanels[panel].b[g_ActiveBuffer][1].y0;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].x3 = g_SelectPanels[panel].b[g_ActiveBuffer][1].x1;
        g_SelectPanels[panel].b[g_ActiveBuffer][4].y3 = g_SelectPanels[panel].b[g_ActiveBuffer][1].y1;
        quad = 5;
        do {
            quad--;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].x0 -= 0xA0;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].y0 -= 0x78;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].x1 -= 0xA0;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].y1 -= 0x78;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].x2 -= 0xA0;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].y2 -= 0x78;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].x3 -= 0xA0;
            g_SelectPanels[panel].b[g_ActiveBuffer][quad].y3 -= 0x78;
            (g_ApiCallPtr = api_AddPrim)(LITP(GsOT, g_OrderingTables)[g_ActiveBuffer].org + (g_SelectPanels[panel].ot_layer + 4), &g_SelectPanels[panel].b[g_ActiveBuffer][quad]);
        } while (quad != 0);
    } while (panel != 0);
}

void select_draw_arrows(void) {
    LIT(s16, sp_sprite_x) = -0x88;
    LIT(s16, sp_sprite_y) = -0x16;
    LIT(u8, sp_sprite_u) = 0xC0;
    LIT(u8, sp_sprite_v) = 0x30;
    LIT(s16, sp_sprite_w) = 0xC;
    LIT(s16, sp_sprite_h) = 0xB;
    LIT(s16, sp_sprite_cy) = 0x1EA;
    if (!(g_SelectTimer & 0x30 & 0xFF)) {
        LIT(s8, sp_sprite_v) = (LIT(s32, sp_sprite_v) + 0x20);
    }
    (g_ApiCallPtr = api_GsSortFlipSprite)(ADDR_sp_sprite, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 4);
    LIT(s16, sp_sprite_x) = 0x4C;
    LIT(s8, sp_sprite_v) = (u8) (LIT(s32, sp_sprite_v) + 0x10);
    (g_ApiCallPtr = api_GsSortFlipSprite)(ADDR_sp_sprite, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 4);
}

void select_scroll_step(void) {
    g_SelectPanels[0].pos_x += g_SelectPanels[0].vel_x;
    g_SelectPanels[0].pos_y += g_SelectPanels[0].vel_y;
    g_SelectPanels[1].pos_x += g_SelectPanels[1].vel_x;
    g_SelectPanels[1].pos_y += g_SelectPanels[1].vel_y;
}

void select_scroll_accel(void) {
    g_SelectPanels[0].vel_x += g_SelectPanels[0].accel_x;
    g_SelectPanels[0].vel_y += g_SelectPanels[0].accel_y;
    g_SelectPanels[1].vel_x += g_SelectPanels[1].accel_x;
    g_SelectPanels[1].vel_y += g_SelectPanels[1].accel_y;
}
