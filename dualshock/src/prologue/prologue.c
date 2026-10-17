/* prologue.c - PROLOGUE.EXE: clears the screen and starts the intro movie (FMV_INTRO). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "overlay_api.h"

/* main-program globals this overlay uses (per-function aliases with other types stay in the functions) */
extern s32 g_GameStateFlags;
extern u8 g_ActiveBuffer;
extern s32 g_LoadPointer;
extern s8 g_SelectedFmvId;
extern s32 g_FmvBuffer;

void prologue_main(void) {
    s32 prologue__main();
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;

    prologue__main();
    (g_ApiCallPtr = api_sound_reinit)();
    g_LoadPointer = ADDR_g_LoadArea;
    rect_x = 0;
    rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
    rect_w = 0x1E0;
    rect_h = 0xF0;
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_ClearImage)(&rect_x, 0, 0, 0);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_ClearImage)(&rect_x, 0, 0, 0);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, 0xF0, 0, 0, 1);
    g_SelectedFmvId = FMV_INTRO;
    g_FmvBuffer = g_LoadPointer;
    g_GameStateFlags |= GAME_FMV_REQUEST;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    g_GameStateFlags |= GAME_GRAPHICS_RESET;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    (g_ApiCallPtr = api_sound_end)();
    (g_ApiCallPtr = api_task_chain)(g_ApiCallPtr = api_game_start);
}
