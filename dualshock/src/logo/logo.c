/* logo.c - LOGO.EXE: streams the logo movie (STR) until it ends or a button is pressed. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "overlay_api.h"

/* main-program globals this overlay uses (per-function aliases with other types stay in the functions) */
extern s32 g_GameStateFlags;
extern u8 g_KeyConfigType;
extern u16 g_LogoStrDecodeX;
extern s8 g_LogoSkipped;
extern s32 g_LogoStrVlcBuf0;
extern s32 g_LogoStrVlcBuf1;
extern s32 g_LogoStrRingBuf;
extern s32 g_LogoStrSliceBuf;

void logo_main(void) {
    s32 logo__main();
    s32 active_buf;
    s32 pad2C;
    s32 pad30;
    s32 str_end;
    s32 frame;
    s32 pad3C;
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;

    logo__main();
    (g_ApiCallPtr = api_sound_reinit)();
    g_LogoStrSliceBuf = ADDR_g_LogoStrSliceArea;
    g_LogoStrRingBuf = ADDR_g_LogoStrRingArea;
    g_LogoStrVlcBuf0 = ADDR_g_LogoStrVlcArea0;
    g_LogoStrVlcBuf1 = ADDR_g_LogoStrVlcArea1;
    g_LogoStrDecodeX = 0;
    g_LogoSkipped = 0;
    (g_ApiCallPtr = api_GsInitGraph)(0x140U, 0xF0, 0, 0, 1);
    (g_ApiCallPtr = api_cd_set_mix_volume)(0x40U);
    (g_ApiCallPtr = api_cd_set_stereo)((((u8) g_KeyConfigType >> 7) ^ 1) & 0xFF);
    (g_ApiCallPtr = api_SsSetSerialAttr)(0U, 0, 1);
    str_end = (g_ApiCallPtr = api_str_open)(0x3BFU, 1, 0x92, g_LogoStrRingBuf, 0x18, g_LogoStrVlcBuf0, g_LogoStrVlcBuf1, g_LogoStrSliceBuf, 1);
    while (str_end == 0) {
        active_buf = (g_ApiCallPtr = api_GsGetActiveBuff)();
        str_end = (g_ApiCallPtr = api_str_decode_next_frame)(g_LogoStrDecodeX, (active_buf != 0) ? 0xF0 : 0);
        (g_ApiCallPtr = api_str_wait_decode)();
        frame = (g_ApiCallPtr = api_str_get_frame)();
        if ((g_ApiCallPtr = api_pad_get_raw_buttons)(0U) & 0xDF0) {
            g_LogoSkipped = 1;
            break;
        }
        (g_ApiCallPtr = api_VSync)(0U);
        (g_ApiCallPtr = api_GsSwapDispBuff)();
    }
    rect_y = 0;
    rect_x = 0;
    rect_w = 0x1E0;
    rect_h = 0x1E0;
    (g_ApiCallPtr = api_ClearImage)(&rect_x, 0, 0, 0);
    (g_ApiCallPtr = api_str_close)();
    (g_ApiCallPtr = api_SsSetSerialAttr)(0U, 0, 0);
    g_GameStateFlags &= ~GAME_SCREEN_MODE;
    (g_ApiCallPtr = api_sound_end)();
    (g_ApiCallPtr = api_load_and_schedule_overlay)(0x15EU);
}
