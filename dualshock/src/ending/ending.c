/* ending.c - ENDING.EXE: picks the ending (ending_select_id), plays it with the staff roll and
   epilogue texts, shows the results (time, saves), then restarts a cleared game with its reward items. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "overlay_api.h"

/* main-program globals this overlay uses (per-function aliases with other types stay in the functions) */
extern s32 g_GameStateFlags;
extern s32 g_StatusFlags;
extern u8 g_ActiveBuffer;
extern s8 g_VSyncMode;
extern s8 g_StageId;
extern u8 g_CutId;
extern u8 g_TotalHeldItems;
extern s16 g_FadeLevel;
extern s16 g_SavedHealth;
extern u32 g_PlayTime;
extern u8 g_SaveCount;
extern s8 g_SaveDcMode;
extern s32 g_ItemBoxSlots;
extern u8 g_KeyConfigType;
extern u16 g_UpdateFlags;
extern s8 g_VramTpageCursor;
extern s8 g_VramClutRowCursor;
extern s16 g_FadeStep;
extern s32 *g_CameraSwitchZones;
extern u8 *g_LoadPointer;
extern s8 g_SelectedFmvId;
extern s16 *g_FmvBuffer;
extern u16 g_PadTrigger;
extern s8 g_FadeTypeId;
extern s32 ending_text_page_tbl;
extern s32 ending_picture_seq_tbl;
extern s32 ending_camera_zones;
extern s32 ending_room_block_4c;
extern u8 *g_EndingStrBuf;
extern s32 g_EndingWorkBuf;
extern u8 *g_EndingSmallBuf;
extern s16 *g_EndingPlayerPicBuf;
extern s16 *g_EndingModelBuf;
extern s16 *g_EndingModelTmd;
extern s16 *g_EndingPacketBuf;
extern u8 *g_EndingLoadBase;
extern s32 g_EndingTextCursor;
extern s32 g_EndingStripImage;
extern void *g_EndingSlotCur;
extern void *g_EndingBgSlotA;
extern void *g_EndingBgSlotB;
extern void *g_EndingPictureSlot;
extern void *g_EndingTextSlot;
extern void *g_EndingEpilogueSlots[5];   /* the epilogue's slots (EndingSlot; views END_EPI0..4) */
extern u8 g_EndingTextPage;
extern s32 g_EndingSceneUnused;
extern u32 g_EndingStripScroll;
extern u32 g_EndingStripLastSpawn;
extern s32 g_EndingStripLimit;
extern s32 g_EndingStrDone;
extern s16 g_EndingScrollMode;
extern s16 g_EndingStripIndex;
extern u16 g_EndingScrollSpeed;
extern u16 g_EndingStrFrame;
extern u16 g_EndingTextPageShown;
extern s32 g_EndingView;
extern s32 g_EndingView_vpy;
extern s32 g_EndingView_vpz;
extern s32 g_EndingView_vrx;
extern s32 g_EndingView_vry;
extern s32 g_EndingView_vrz;
extern s32 g_EndingView_rz;
extern s32 g_EndingView_super;
extern s32 g_EndingEffectPos;
extern s32 g_EndingEffectPos_vy;
extern s32 g_EndingEffectPos_vz;
extern s32 g_EndingCoordRoot;
extern s16 g_EndingCoordRoot_coord;
extern s32 g_EndingCoordRoot_tx;
extern s32 g_EndingCoordRoot_ty;
extern s32 g_EndingCoordRoot_tz;
extern s32 g_EndingCoordSpin;
extern s32 g_EndingCoordSpin_coord;
extern s32 g_EndingCoordLeaf;
extern s16 g_EndingCoordLeaf_coord;
extern s32 g_EndingModelObj;
extern s32 *g_EndingModelObj_coord2;
extern s32 g_EndingLight;
extern s32 g_EndingLight_vy;
extern s32 g_EndingLight_vz;
extern u8 g_EndingLight_r;
extern u8 g_EndingLight_g;
extern u8 g_EndingLight_b;
extern s16 *g_EndingPacketCur;
extern s32 g_EndingSpinAngles;
extern s16 g_EndingSpinAngles_vy;
extern u16 g_EndingSpinAngles_vz;
extern u8 g_EndingHasRocketLauncher;
extern s8 g_EndingId;
extern s8 g_EndingUnderTimeLimit;
extern u8 g_EndingHadTimeReward;
extern u8 g_EndingHadAdvReward;
extern u8 g_EndingTextSet;
extern s8 g_EndingStep;
extern u8 g_EndingSubStep;
extern u8 g_EndingStepDone;
extern u16 g_EndingStepTimer;
extern u16 g_EndingFrame;
extern s8 g_EndingUnusedC8;
extern s8 g_EndingUnusedCC;
extern s8 g_EndingUnusedD0;
extern u8 g_EndingPictureState;
extern s8 g_EndingUnusedD8;
extern s8 g_EndingUnusedDC;
extern s32 g_EndingScrollZ;
extern s32 g_EndingScrollZVel;
extern s32 g_EndingScrollZAcc;
extern s32 *g_EndingCopySrc;
extern s32 *g_EndingCopyDst;
extern s32 g_EndingRoomData;
extern s32 g_EndingSavedStatusFlags;
extern s32 g_EndingBoxA;
extern s16 g_EndingBoxA_x;
extern s16 g_EndingBoxA_y;
extern s16 g_EndingBoxA_w;
extern s16 g_EndingBoxA_h;
extern u8 g_EndingBoxA_r;
extern u8 g_EndingBoxA_g;
extern u8 g_EndingBoxA_b;
extern s32 g_EndingBoxB;
extern s16 g_EndingBoxB_x;
extern s16 g_EndingBoxB_y;
extern s16 g_EndingBoxB_w;
extern s16 g_EndingBoxB_h;
extern s8 g_EndingBoxB_r;
extern s8 g_EndingBoxB_g;
extern s8 g_EndingBoxB_b;
extern Entity g_PlayerEntity __asm__("g_PlayerEntity");

void ending_state(void) {
    extern struct { u8 a; u8 b; u8 c; u8 d; } ending_def_tbl[];
    void ending_scene_reset();
    void ending_bg_slot_setup();
    void ending_slot_alloc();
    void ending_slots_clear();
    void ending_strip_slot_setup();
    void ending_nop();
    void ending_picture_sequence_update();
    void ending_text_page_update();
    void ending_result_draw();
    void ending_select_id();
    void ending_epilogue_update();
    void ending_scroll_slot_update();
    void ending_result_update();
    s32 ending__main();
    extern s8 g_SaveCount_s8 __asm__("g_SaveCount");
    extern u8 * g_EndingWorkBuf_u8p __asm__("g_EndingWorkBuf");
    extern u8 * g_EndingPlayerPicBuf_u8p __asm__("g_EndingPlayerPicBuf");
    extern u8 * g_EndingModelBuf_u8p __asm__("g_EndingModelBuf");
    extern u8 * g_EndingModelTmd_u8p __asm__("g_EndingModelTmd");
    extern u8 * g_EndingPacketBuf_u8p __asm__("g_EndingPacketBuf");
    extern struct { u16 a; u16 b; u8 p4[0x14]; s16 x18; u8 rest[0x2A]; } g_EndingSlots[];
    extern s16 g_EndingScrollSpeed_s16 __asm__("g_EndingScrollSpeed");
    extern s16 g_EndingTextPageShown_s16 __asm__("g_EndingTextPageShown");
    extern u8 g_EndingId_u8 __asm__("g_EndingId");
    extern u8 g_EndingUnderTimeLimit_u8 __asm__("g_EndingUnderTimeLimit");
    extern u8 g_EndingStep_u8 __asm__("g_EndingStep");
    extern s8 g_EndingSubStep_s8 __asm__("g_EndingSubStep");
    extern s8 g_EndingStepDone_s8 __asm__("g_EndingStepDone");
    extern s16 g_EndingStepTimer_s16 __asm__("g_EndingStepTimer");
    extern s8 g_EndingPictureState_s8 __asm__("g_EndingPictureState");
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;
    u8 inv_slot;
    s32 saved_flags;
    u16 i;
    s32 str_status;

    ending__main();
    g_EndingSavedStatusFlags = g_StatusFlags;
    g_StatusFlags &= 0x20080000;
    g_GameStateFlags |= GAME_GRAPHICS_RESET;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    (g_ApiCallPtr = api_sound_reinit)();
    (g_ApiCallPtr = api_sound_load_bank1)((s16 *)0xE, LITP(s16, g_LoadArea));
    g_LoadPointer = LITP(u8, g_LoadArea);
    g_LoadPointer += 0x10000;
    g_EndingWorkBuf_u8p = g_LoadPointer;
    g_LoadPointer += 0x40000;
    g_EndingSmallBuf = g_LoadPointer;
    g_LoadPointer += 0x1000;
    g_EndingPlayerPicBuf_u8p = g_LoadPointer;
    g_LoadPointer += 0x25800;
    g_EndingLoadBase = g_LoadPointer;
    g_LoadPointer += 0x10000;
    g_EndingStrBuf = g_LoadPointer;
    g_LoadPointer += 0x30000;
    g_EndingModelBuf_u8p = g_LoadPointer;
    g_EndingModelTmd_u8p = g_EndingModelBuf_u8p + 0x10220;
    g_LoadPointer += 0x30000;
    g_EndingPacketBuf_u8p = g_LoadPointer;
    ending_select_id();
    g_LoadPointer = LITP(u8, g_LoadArea);
    if (ending_def_tbl[g_EndingId_u8 - 1].d != 0) {
        (g_ApiCallPtr = api_load_file)((s16 *)0xA, g_LoadPointer);
    } else {
        (g_ApiCallPtr = api_load_file)((s16 *)9, g_LoadPointer);
    }
    (g_ApiCallPtr = api_load_file)((s16 *)0x11A, g_EndingModelBuf_u8p);
    if (g_PlayerEntity.id & 3 & 0xFF) {
        (g_ApiCallPtr = api_load_file)((s16 *)0x10, g_EndingPlayerPicBuf_u8p);
    } else {
        (g_ApiCallPtr = api_load_file)((s16 *)3, g_EndingPlayerPicBuf_u8p);
    }
    g_VramTpageCursor = 0x1B;
    g_VramClutRowCursor = 0x1B;
    (g_ApiCallPtr = api_tim_load_to_vram)(g_LoadPointer);
    g_LoadPointer = g_EndingLoadBase;
    (g_ApiCallPtr = api_load_shoot_direction_data)();
    (g_ApiCallPtr = api_memclr)(LITP(s16, g_EffectPool), (s16 *)0x840);
    (g_ApiCallPtr = api_VSync)(NULL);
    rect_x = 0;
    rect_y = 0;
    rect_w = 0x1E0;
    rect_h = 0x1E0;
    (g_ApiCallPtr = api_ClearImage)(&rect_x, NULL, 0, NULL);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    g_SelectedFmvId = FMV_ENDING_BASE;
    g_FmvBuffer = (s16 *)g_EndingStrBuf;
    g_GameStateFlags |= GAME_FMV_REQUEST;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    rect_x = 0;
    rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
    rect_w = 0x1E0;
    rect_h = 0xF0;
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_ClearImage)(&rect_x, NULL, 0, NULL);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_ClearImage)(&rect_x, NULL, 0, NULL);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_VSync)(NULL);
    (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, (s16 *)0xF0, 0, NULL, 1);
    (g_ApiCallPtr = api_GsInit3D)();
    if (ending_def_tbl[g_EndingId_u8 - 1].b == 0) {
        (g_ApiCallPtr = api_load_file)(NULL, g_EndingWorkBuf_u8p);
        g_VramClutRowCursor = 0x1A;
        g_VramTpageCursor = 0xF;
        (g_ApiCallPtr = api_tim_load_to_vram)(g_EndingWorkBuf_u8p);
    }
    if (ending_def_tbl[g_EndingId_u8 - 1].b != 0) {
        (g_ApiCallPtr = api_load_file)((s16 *)0x1A, g_EndingWorkBuf_u8p);
    } else {
        (g_ApiCallPtr = api_load_file)((s16 *)0x1A, g_EndingWorkBuf_u8p);
    }
    rect_x = 0;
    rect_y = 0x1FF;
    rect_w = 0x100;
    rect_h = 1;
    (g_ApiCallPtr = api_LoadImage)(&rect_x, g_EndingWorkBuf_u8p);
    (g_ApiCallPtr = api_DrawSync)(NULL);
    g_EndingWorkBuf_u8p += 0x200;
    g_SelectedFmvId = g_EndingId_u8 + FMV_ENDING_BASE;
    g_FmvBuffer = (s16 *)g_EndingStrBuf;
    g_GameStateFlags |= GAME_FMV_REQUEST;
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, (s16 *)0xF0, 0, NULL, 0);
    (g_ApiCallPtr = api_GsInit3D)();
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    ending_scene_reset();
    ending_slots_clear();
    g_EndingScrollMode = (s16) ending_def_tbl[g_EndingId_u8 - 1].b;
    g_EndingStep_u8 = (u8) g_EndingScrollMode ^ 1;
    g_EndingSubStep_s8 = 0;
    g_EndingStepDone_s8 = 0;
    g_EndingStepTimer_s16 = 0;
    g_EndingFrame = 0;
    if (g_EndingStep_u8 == 1) {
        ending_bg_slot_setup();
    }
    g_EndingUnusedC8 = 0;
    g_EndingUnusedCC = 0;
    g_EndingUnusedD0 = 0;
    g_EndingPictureState_s8 = 0;
    g_EndingUnusedD8 = 0;
    g_EndingUnusedDC = 0;
    g_EndingStripScroll = 0;
    g_EndingStripLastSpawn = 0;
    g_EndingStripLimit = 0x0E100000;
    g_GameStateFlags |= GAME_SCREEN_CLEAR;
    (g_ApiCallPtr = api_cd_set_mix_volume)((s16 *)0x80);
    (g_ApiCallPtr = api_cd_set_stereo)((s16 *) ((((u8) g_KeyConfigType >> 7) ^ 1) & 0xFF));
    (g_ApiCallPtr = api_SsSetSerialAttr)(NULL, NULL, 1);
loop_20:
    str_status = (g_ApiCallPtr = api_str_open)((s16 *)0x3D6, (s16 *)2, 0xF73, g_EndingStrBuf, 0x1E, g_EndingStrBuf + 0xF000, g_EndingStrBuf + 0x1D000, g_EndingStrBuf + 0x2B000, 0);
    if (str_status == 1) {
        (g_ApiCallPtr = api_str_close)();
        goto loop_20;
    }
    g_EndingStripScroll = 0;
    g_EndingScrollSpeed_s16 = 0;
    g_EndingTextPageShown_s16 = 0;
    g_EndingStrDone = 0;
    while (1) {
    if ((g_EndingStrDone == 0) && ((u16) g_EndingFrame >= 0x3DE1U)) {
        (g_ApiCallPtr = api_cd_set_mix_volume)(NULL);
        g_EndingStrDone = 1;
    }
    switch (g_EndingStep_u8) {
    case 0:
        if (((u32) g_EndingStripScroll <= 0x0E100000U) && ((u16) g_EndingFrame >= 0x12CU)) {
            if ((u32) (g_EndingStripScroll - g_EndingStripLastSpawn) > 0xFFFFFU) {
                ending_slot_alloc();
                ending_strip_slot_setup();
                g_EndingStripLastSpawn = g_EndingStripScroll;
            }
            g_EndingStripScroll += 0x4000;
        }
        if ((u16) g_EndingFrame >= 0x3B4CU) {
            for (i = 0; i < 0x15U; i++) {
                if ((g_EndingSlots[i].a != 0) && (g_EndingSlots[i].b == 0)) {
                    g_EndingSlots[i].b = 1;
                }
            }
        }
        if (g_EndingFrame == 0x3D0E) {
            for (i = 0; i < 0x15U; i++) {
                if (g_EndingSlots[i].a != 0) {
                    g_EndingSlots[i].x18 = -0x800;
                }
            }
        }
        if ((u16) g_EndingFrame >= 0x3DE1U) {
            g_EndingStep_u8 = 0x63;
            g_EndingSubStep_s8 = 0;
            g_EndingUnderTimeLimit_u8 = 0;
            ending_slots_clear();
        }
        if ((g_PadTrigger & 0x9F0 & 0xFFFF) && ((u16) g_EndingFrame >= 0x12DU)) {
            g_EndingStep_u8 = 0x63;
        }
        g_EndingFrame++;
        break;
    case 1:
        (g_ApiCallPtr = api_DrawSync)(NULL);
        if (g_EndingStrDone == 0) {
            g_EndingStrDone = (g_ApiCallPtr = api_str_decode_next_frame)((s16 *)0x240, NULL);
            (g_ApiCallPtr = api_str_wait_decode)();
            g_EndingStrFrame = (g_ApiCallPtr = api_str_get_frame)();
            if (g_EndingStrDone != 0) {
                (g_ApiCallPtr = api_cd_set_mix_volume)(NULL);
                g_EndingStep_u8 = 2;
                g_EndingSubStep_s8 = 0;
                g_EndingUnderTimeLimit_u8 = 0;
                ending_slots_clear();
            }
        }
        ending_text_page_update();
        ending_nop();
        ending_picture_sequence_update();
        if ((g_PadTrigger & 0x9F0 & 0xFFFF) && ((u16) g_EndingStrFrame >= 0x4CU)) {
            g_EndingStep_u8 = 0x63;
        }
        if ((u16) g_EndingStrFrame >= 0x691U) {
            g_EndingStep_u8 = 2;
            g_EndingSubStep_s8 = 0;
            g_EndingUnderTimeLimit_u8 = 0;
            ending_slots_clear();
        }
        g_EndingFrame += 4;
        break;
    case 2:
        ending_scroll_slot_update();
        g_EndingFrame += 2;
        break;
    case 3:
    default:
        break;
    }
    ending_result_draw();
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    if ((u8) g_EndingStep_u8 >= 4U) {
        break;
    }
    }
    (g_ApiCallPtr = api_str_close)();
    (g_ApiCallPtr = api_SsSetSerialAttr)(NULL, NULL, 0);
    if (ending_def_tbl[g_EndingId_u8 - 1].a == 1) {
        (g_ApiCallPtr = api_task_sleep)((s16 *)0x96);
        rect_x = 0;
        rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
        rect_w = 0x1E0;
        rect_h = 0xF0;
        (g_ApiCallPtr = api_VSync)(NULL);
        (g_ApiCallPtr = api_ClearImage)(&rect_x, NULL, 0, NULL);
        (g_ApiCallPtr = api_DrawSync)(NULL);
        (g_ApiCallPtr = api_task_sleep)((s16 *)1);
        rect_y = ((g_ActiveBuffer != 0) ? 0xF0 : 0);
        (g_ApiCallPtr = api_VSync)(NULL);
        (g_ApiCallPtr = api_ClearImage)(&rect_x, NULL, 0, NULL);
        (g_ApiCallPtr = api_DrawSync)(NULL);
        (g_ApiCallPtr = api_VSync)(NULL);
        (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, (s16 *)0xF0, 0, NULL, 1);
        (g_ApiCallPtr = api_GsInit3D)();
        g_SelectedFmvId = FMV_ENDING_BASE + 8;
        g_FmvBuffer = (s16 *)g_EndingStrBuf;
        g_GameStateFlags |= GAME_FMV_REQUEST;
        (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    }
    (g_ApiCallPtr = api_GsInitGraph2)((s16 *)0x140, (s16 *)0xF0, 0, NULL, 0);
    (g_ApiCallPtr = api_GsInit3D)();
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    g_EndingStep_u8 = 0;
    g_EndingSubStep_s8 = 0;
    while (g_EndingStep_u8 == 0) {
        ending_epilogue_update();
        ending_result_draw();
        (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    }
    g_EndingHasRocketLauncher = 0;
    for (i = 0; i < g_TotalHeldItems; i++) {
        if (*(u8 *)(ADDR_g_InventorySlots + (i * 2)) == 0x38) {
            g_EndingHasRocketLauncher++;
            break;
        }
    }
    i = 0;
    for (; i < 0x30U; i++) {
        if (*(u8 *)(ADDR_g_ItemBoxSlots + (i * 2)) == 0x38) {
            g_EndingHasRocketLauncher++;
            break;
        }
    }
    if (g_EndingSavedStatusFlags & 0x20000) {
        g_EndingHasRocketLauncher++;
    }
    if ((ending_def_tbl[g_EndingId_u8 - 1].c == 1) && (g_EndingHasRocketLauncher == 0)) {
        g_EndingStepTimer_s16 = 0;
        g_EndingStep_u8 = 0;
        g_EndingSubStep_s8 = 0;
        g_EndingStepDone_s8 = 0;
        while (g_EndingStep_u8 == 0) {
            ending_result_update();
            (g_ApiCallPtr = api_task_sleep)((s16 *)1);
        }
        g_EndingHasRocketLauncher++;
    }
    (g_ApiCallPtr = api_task_sleep)((s16 *)1);
    g_EndingHadAdvReward = 0;
    g_EndingHadTimeReward = 0;
    if ((g_ApiCallPtr = api_Flg_ck)(LITP(s16, g_ScenarioFlags), SCN_FLAG_TIME_REWARD) != 0) {
        g_EndingHadTimeReward = 1;
    }
    if ((g_ApiCallPtr = api_Flg_ck)(LITP(s16, g_ScenarioFlags), SCN_FLAG_ADV_REWARD) != 0) {
        g_EndingHadAdvReward = 1;
    }
    (g_ApiCallPtr = api_load_file)((s16 *)1, g_EndingWorkBuf_u8p);
    g_EndingCopySrc = (s32 *)g_EndingWorkBuf_u8p;
    g_EndingCopyDst = LITP(s32, g_SaveData);
    for (i = 0; i < 0x107U; i++) {
        *g_EndingCopyDst++ = *g_EndingCopySrc++;
    }
    ending_slots_clear();
    g_SaveCount_s8 = 0;
    g_StageId = 0;
    g_PlayerEntity.collision_flags = 0;
    g_PlayerEntity.health = g_SavedHealth = 0x8C - ((g_PlayerEntity.id & 1 & 0xFF) * 0x2C);
    if (g_EndingSavedStatusFlags & 0x40000) {
        g_SavedHealth = g_PlayerEntity.health = 0xB4 - ((g_PlayerEntity.id & 1 & 0xFF) * 0x1E);
        g_ItemBoxSlots = 0x1E0B1E0B;
    }
    if (g_EndingSavedStatusFlags & 0x20000) {
        g_SavedHealth = g_PlayerEntity.health = 0x64 - ((g_PlayerEntity.id & 1 & 0xFF) * 0x1E);
        g_StatusFlags |= STATUS_ADVANCED;
        if (g_EndingSavedStatusFlags & 0x10000) {
            g_ItemBoxSlots = 0x1E0B1E0B;
        }
    }
    (g_ApiCallPtr = api_set_initial_items)();
    g_StatusFlags &= ~STATUS_ADVANCED;
    inv_slot = g_TotalHeldItems;
    if ((g_EndingUnderTimeLimit_u8 != 0) || (g_EndingHadTimeReward != 0)) {
        (g_ApiCallPtr = api_Flg_on)(LITP(s16, g_ScenarioFlags), SCN_FLAG_TIME_REWARD);
        *(u8 *)(ADDR_g_InventorySlots + (inv_slot * 2)) = 0xA;
        ((InventorySlot *)((inv_slot * 2) + ADDR_g_InventorySlots))->count = 1;
        inv_slot++;
        g_TotalHeldItems++;
    }
    if (g_EndingSavedStatusFlags & 0x20000) {
        if ((g_EndingId_u8 == 6) || (g_EndingId_u8 == 7) || (g_EndingHadAdvReward != 0)) {
            (g_ApiCallPtr = api_Flg_on)(LITP(s16, g_ScenarioFlags), SCN_FLAG_ADV_REWARD);
            *(u8 *)(ADDR_g_InventorySlots + (inv_slot * 2)) = 5;
            ((InventorySlot *)((inv_slot * 2) + ADDR_g_InventorySlots))->count = 1;
            g_TotalHeldItems++;
        }
    } else if (g_EndingHasRocketLauncher != 0) {
        *(u8 *)(ADDR_g_InventorySlots + (inv_slot * 2)) = 0x38;
        ((InventorySlot *)((inv_slot * 2) + ADDR_g_InventorySlots))->count = 1;
        g_TotalHeldItems++;
    }
    (g_ApiCallPtr = api_Flg_on)(LITP(s16, g_ScenarioFlags), SCN_FLAG_CLEARED);
    if (g_EndingSavedStatusFlags & 0x40000) {
        g_SaveDcMode = 1;
    }
    if (g_EndingSavedStatusFlags & 0x20000) {
        g_SaveDcMode = 2;
    }
    if (g_EndingSavedStatusFlags & 0x10000) {
        g_SaveDcMode = 3;
    }
    saved_flags = g_StatusFlags;
    g_StatusFlags = g_EndingSavedStatusFlags;
    (g_ApiCallPtr = api_memcard_screen)(NULL, g_EndingWorkBuf_u8p, 0, (s16 *)1);
    g_StatusFlags = saved_flags;
    g_GameStateFlags = (g_GameStateFlags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
    g_VSyncMode = 2;
    (g_ApiCallPtr = api_sound_end)();
    (g_ApiCallPtr = api_load_and_schedule_overlay)((s16 *)0x154);
}

void ending_scene_reset(void) {
    s32 unused[4];
    g_EndingSceneUnused = 0;
    g_VSyncMode = 0;
    g_EndingTextCursor = g_EndingWorkBuf;
    g_EndingStripIndex = 0;
}

void ending_bg_slot_setup(void) {
    void ending_slot_alloc();
    ending_slot_alloc();
    g_EndingBgSlotA = g_EndingSlotCur;
    END_BG_A->active = 1;
    END_BG_A_AS(type, s16) = 1;
    END_BG_A->ot_pri = 4;
    END_BG_A->x = 0;
    END_BG_A->y = 0;
    END_BG_A_AS(fade_level, s16) = 0;
    END_BG_A->fade_vel = 0;
    END_BG_A->tex_flags = 0x02000000;
    END_BG_A->tex_width = 0x100;
    END_BG_A->tex_height = 0xF0;
    END_BG_A->tex_page = (g_ApiCallPtr = api_GetTPage)(2, 0, 0x240, 0);
    END_BG_A->tex_u = 0;
    END_BG_A_AS(tex_v, s8) = 0;
    ending_slot_alloc();
    g_EndingBgSlotB = g_EndingSlotCur;
    END_BG_B->active = 1;
    END_BG_B_AS(type, s16) = 1;
    END_BG_B->ot_pri = 3;
    END_BG_B->x = 0x01000000;
    END_BG_B->y = 0;
    END_BG_B_AS(fade_level, s16) = -0x8000;
    END_BG_B->fade_vel = 0;
    END_BG_B->tex_flags = 0x02000000;
    END_BG_B->tex_width = 0x40;
    END_BG_B->tex_height = 0xF0;
    END_BG_B->tex_page = (g_ApiCallPtr = api_GetTPage)(2, 0, 0x340, 0);
    END_BG_B->tex_u = 0;
    END_BG_B_AS(tex_v, s8) = 0;
}

void ending_slot_alloc(void) {
    extern struct { u16 h0; u8 pad[0x42]; } g_EndingSlots_h[] __asm__("g_EndingSlots");
    u16 i;
    u16 found;

    for (i = 0; i < 0x15U; i++) {
        if (g_EndingSlots_h[i].h0 == 0) {
            g_EndingSlotCur = &g_EndingSlots_h[i];
            found = i;
            return;
        }
        g_EndingSlotCur = NULL;
    }
}

void ending_slots_clear(void) {
    extern struct { s16 h0; u8 pad[0x42]; } g_EndingSlots_s[] __asm__("g_EndingSlots");
    u16 i;

    for (i = 0; i < 0x15U; i++) {
        g_EndingSlots_s[i].h0 = 0;
    }
}

void ending_strip_slot_setup(void) {
    void ending_text_next_block();
    extern u16 g_EndingStripIndex_u16 __asm__("g_EndingStripIndex");
    u16 rect_x;
    u16 rect_y;
    s16 rect_w;
    s16 rect_h;
    u16 unused;

    END_SLOT->active = 1;
    END_SLOT_AS(type, s16) = 0;
    END_SLOT->ot_pri = 3;
    END_SLOT->vram_x = 0x180U;
    END_SLOT->vram_y = (u16) (g_EndingStripIndex_u16 * 0x10);
    END_SLOT->x = 0x200000;
    END_SLOT->y = (s32) ((0x100 - (((u32) g_EndingStripScroll >> 0x10) & 0xF)) << 0x10);
    END_SLOT_AS(fade_level, s16) = -0x8000;
    END_SLOT->fade_vel = 0;
    END_SLOT->tex_flags = 0x01000000;
    END_SLOT->tex_width = 0x100;
    END_SLOT->tex_height = 0x10;
    END_SLOT->clut_x = 0;
    END_SLOT->clut_y = 0x1FF;
    END_SLOT->tex_page = (g_ApiCallPtr = api_GetTPage)(NULL, 0, END_SLOT->vram_x, END_SLOT->vram_y);
    END_SLOT->tex_u = 0;
    END_SLOT->tex_v = ((u8) END_SLOT->vram_y);
    rect_x = END_SLOT->vram_x;
    rect_y = END_SLOT->vram_y;
    rect_w = 0x80;
    rect_h = 0x10;
    ending_text_next_block();
    (g_ApiCallPtr = api_LoadImage)(&rect_x, g_EndingStripImage);
    if (++g_EndingStripIndex_u16 >= 0x12) {
        g_EndingStripIndex_u16 = 0;
    }
}

void ending_text_next_block(void) {
    s32 unused0;
    u16 n;
    s32 unused8;
    s8 *dst;

    LIT(void *, g_EndingTextBlock) = LIT(void *, g_EndingTextCursor);
    LIT(u16 *, g_EndingTextRuns) = LIT(void *, g_EndingTextCursor) + END_TBLK->runs_ofs;
    LIT(u8 *, g_EndingTextBytes) = LIT(void *, g_EndingTextCursor) + END_TBLK->bytes_ofs;
    LIT(void *, g_EndingTextCursor) += END_TBLK->size;
    if (END_TBLK->bytes_ofs == 0xFFFF) {
        LIT(u16 *, g_EndingStripImage) = LIT(u16 *, g_EndingTextRuns);
        return;
    }
    dst = LIT(s8 *, g_EndingSmallBuf);
    while (*LIT(u16 *, g_EndingTextRuns) != 0) {
        n = 0;
        while (n < (u16) *LIT(u16 *, g_EndingTextRuns)) {
                        *dst++ = (u8) *LIT(u8 *, g_EndingTextBytes);
            n++;
        }
        LIT(u16 *, g_EndingTextRuns)++;
        LIT(u8 *, g_EndingTextBytes)++;
    }
    LIT(u16 *, g_EndingStripImage) = LIT(u16 *, g_EndingSmallBuf);
    return;
}

void ending_nop(void) {

}

void ending_picture_sequence_update(void) {
    extern EndingPictureCue *g_EndingPicturePtr;
    switch (g_EndingPictureState) {
    case 0:
        g_EndingPicturePtr = (EndingPictureCue *)(*(s32 *)((u8 *)&ending_picture_seq_tbl + ((g_EndingTextSet * 4))));
        g_EndingPictureState = 1;
        ending_slot_alloc();
        g_EndingPictureSlot = g_EndingSlotCur;
        END_PIC->active = 1;
        END_PIC->type = 7;
        END_PIC->fade_level = 0;
        END_PIC->fade_vel = 0;
        END_PIC->box_vel_x = 0U;
        END_PIC->box_vel_y = 0U;
        g_EndingBoxA = 0;
        g_EndingBoxA_x = -0xA0;
        g_EndingBoxA_y = -0xF0;
        g_EndingBoxA_w = 0x140;
        g_EndingBoxA_h = 0x78;
        g_EndingBoxA_b = 0x80;
        g_EndingBoxA_g = 0x80;
        g_EndingBoxA_r = 0x80;
        g_EndingBoxB = 0;
        g_EndingBoxB_x = -0x140;
        g_EndingBoxB_y = -0x78;
        g_EndingBoxB_w = 0xA0;
        g_EndingBoxB_h = 0xF0;
        g_EndingBoxB_b = 0;
        g_EndingBoxB_g = 0;
        g_EndingBoxB_r = 0;
        /* fallthrough */
    case 1:
        if ((u16) g_EndingStrFrame >= (u16) END_CUE->show_frame) {
            END_PIC->fade_vel = 0;
            END_PIC->fade_level = 0;
            END_PIC->box_vel_x = 0U;
            END_PIC->box_vel_y = 0U;
            g_EndingBoxA_y = END_CUE->box_y - 0x78;
            g_EndingBoxB_x = END_CUE->box_x - 0xA0;
            g_EndingPictureState = 2;
        }
        return;
    case 2:
        if ((u16) g_EndingStrFrame >= (u16) END_CUE->move_frame) {
            END_PIC->box_vel_x = (u16) END_CUE->box_vel_x;
            END_PIC->box_vel_y = (u16) END_CUE->box_vel_y;
            g_EndingPicturePtr++;
            if (END_CUE->show_frame == 0xFFFF) {
                g_EndingPictureState = 0x63;
            } else {
                g_EndingPictureState = 1;
            }
        }
        return;
    }
    return;
}

void ending_text_page_update(void) {
    void ending_text_layer_setup();
    extern u16 g_EndingStripIndex_u16 __asm__("g_EndingStripIndex");
    extern BYTES(6) *g_EndingTextPagePtr;   /* steps 6-byte records (show_frame, hide_frame, x); the
                                               EndingTextPage view type is 8 bytes long */
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;
    u16 i;

    switch (g_EndingSubStep) {
    case 0:
        g_EndingTextPageShown = 0;
        g_EndingStripIndex_u16 = 0;
        g_EndingTextPage = 0;
        g_EndingTextPagePtr = *(void **)((u8 *)&ending_text_page_tbl + ((g_EndingTextSet * 4)));
        ending_text_layer_setup();
        END_TEXT->x = (s32) (END_PAGE->x << 0x10);
        g_EndingSubStep++;
        g_EndingStepDone = 0;
        i = 0;
        while (i < 0xFU) {
            ending_text_next_block();
            rect_x = (g_EndingTextPage * 0x80) + 0x140;
            rect_y = g_EndingStripIndex_u16 * 0x10;
            rect_w = 0x80;
            rect_h = 0x10;
            (g_ApiCallPtr = api_LoadImage)(&rect_x, LIT(s32, g_EndingStripImage));
            (g_ApiCallPtr = api_DrawSync)(NULL);
            g_EndingStripIndex_u16++;
            i++;
        }
        /* fallthrough */
    case 1:
        if (((u16) g_EndingStripIndex_u16 < 0xFU) && (g_EndingTextPageShown == 0)) {
            ending_text_next_block();
            rect_x = (g_EndingTextPage * 0x80) + 0x140;
            rect_y = g_EndingStripIndex_u16 * 0x10;
            rect_w = 0x80;
            rect_h = 0x10;
            (g_ApiCallPtr = api_LoadImage)(&rect_x, LIT(s32, g_EndingStripImage));
            g_EndingStripIndex_u16++;
        }
        switch (g_EndingStepDone) {
        case 0:
            if ((u16) g_EndingStrFrame >= (u16) END_PAGE->show_frame) {
                g_EndingTextPage ^= 1;
                END_TEXT->tex_page = (g_ApiCallPtr = api_GetTPage)((s16 *)1, 0, (((g_EndingTextPage ^ 1) & 0xFF) * 0x80) + 0x140, 0);
                END_TEXT->x = (s32) (END_PAGE->x << 0x10);
                END_TEXT->fade_level = 0U;
                END_TEXT->fade_vel = 0x1100;
                if (END_PAGE_FIELD(last) == 0xFFFF) {
                    g_EndingTextPageShown = 1;
                } else {
                    g_EndingStripIndex_u16 = 0;
                }
                g_EndingStepDone++;
            }
            break;
        case 1:
            if (END_TEXT->fade_level == 0x8000) {
                END_TEXT->tex_flags = (s32) (END_TEXT->tex_flags & 0xBFFFFFFF);
            }
            if ((u16) g_EndingStrFrame >= (u16) END_PAGE->hide_frame) {
                END_TEXT->tex_flags = (s32) (END_TEXT->tex_flags | 0x40000000);
                END_TEXT->fade_vel = -0x1100;
                g_EndingTextPagePtr++;
                if (g_EndingTextPageShown != 0) {
                    g_EndingSubStep = 0x63;
                } else {
                    g_EndingStepDone = 0;
                }
            }
            break;
        }
        return;
    }
    return;
}

void ending_text_layer_setup(void) {
    s16 rect_x;
    s16 rect_y;
    s16 rect_w;
    s16 rect_h;

    ending_slot_alloc();
    g_EndingTextSlot = g_EndingSlotCur;
    END_TEXT->active = 1;
    END_TEXT->type = 2;
    END_TEXT->x = 0;
    END_TEXT->y = 0;
    END_TEXT->fade_level = 0;
    END_TEXT->fade_vel = 0;
    END_TEXT->tex_flags = 0x51000000;
    END_TEXT->tex_width = 0x100;
    END_TEXT->tex_height = 0xF0;
    END_TEXT->tex_page = (g_ApiCallPtr = api_GetTPage)((s16 *)1, 0, (((g_EndingTextPage ^ 1) & 0xFF) * 0x80) + 0x140, 0);
    END_TEXT->tex_u = 0;
    END_TEXT->tex_v = 0;
    END_TEXT->clut_x = 0;
    END_TEXT->clut_y = 0x1FF;
    rect_x = 0x140;
    rect_y = 0;
    rect_w = 0x100;
    rect_h = 0xF0;
    (g_ApiCallPtr = api_ClearImage)(&rect_x, 0, 0, 0);
    (g_ApiCallPtr = api_DrawSync)(NULL);
}

void ending_result_draw(void) {
    void ending_draw_text();
    extern s32 g_EndingSlots_s32 __asm__("g_EndingSlots");
    extern EndingSlot *g_EndingDrawCursor;
    u16 i;

    g_EndingDrawCursor = (EndingSlot *)&g_EndingSlots_s32;
    i = 0;
    while (i < 0x15U) {
        if (END_DRAW_AS(active, u16) != 0) {
            END_DRAW->tex_screen_x = (u16) (((s32) END_DRAW->x >> 0x10) - 0xA0);
            END_DRAW->tex_screen_y = (s16) (((s32) END_DRAW->y >> 0x10) - 0x78);
            if (END_DRAW->fade_vel != 0) {
                END_DRAW->fade_level = (u16) (END_DRAW->fade_level + (u16) END_DRAW->fade_vel);
                if ((s16) END_DRAW->fade_level < 0) {
                    if (END_DRAW->fade_vel > 0) {
                        END_DRAW_AS(fade_level, s16) = -0x8000;
                    } else {
                        END_DRAW->fade_level = 0U;
                    }
                    END_DRAW->fade_vel = 0;
                }
            }
            END_DRAW->color_r = (s8) ((s16) END_DRAW->fade_level >> 8);
            END_DRAW->color_g = (s8) ((s16) END_DRAW->fade_level >> 8);
            END_DRAW->color_b = (s8) ((s16) END_DRAW->fade_level >> 8);
            __asm__(".section .rodata\n\t.word 0\n\t.text");
            switch ((END_DRAW->type)) {
            case 0:
                END_DRAW->tex_screen_x = (u16) (END_DRAW->tex_screen_x + g_EndingScrollSpeed);
                if ((s16) END_DRAW->fade_level != 0) {
                    (g_ApiCallPtr = api_GsSortFastSprite)((u8 *) g_EndingDrawCursor + 0x20, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, END_DRAW_AS(ot_pri, u16));
                }
                END_DRAW->y = (s32) (END_DRAW->y - 0x4000);
                if (END_DRAW->y < -0x100000) {
                    END_DRAW_AS(active, u16) = 0U;
                }
                break;
            case 1:
                if ((s16) END_DRAW->fade_level != 0) {
                    (g_ApiCallPtr = api_GsSortFastSprite)((u8 *) g_EndingDrawCursor + 0x20, (g_ActiveBuffer * 0x14) + ADDR_g_OtBg, 0xFU);
                }
                break;
            case 2:
                if ((s16) END_DRAW->fade_level != 0) {
                    (g_ApiCallPtr = api_GsSortFastSprite)((u8 *) g_EndingDrawCursor + 0x20, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 2U);
                }
                break;
            case 3:
                END_DRAW->tex_page = 0x1E;
                END_DRAW->tex_width = 8;
                END_DRAW->tex_height = 0xE;
                END_DRAW->clut_x = 0x100;
                END_DRAW->clut_y = 0x1E0;
                (g_ApiCallPtr = api_sprintf)(LITP(s32, sp_text), "%02d:%02d%1c%02d", g_PlayTime / 216000, (g_PlayTime % 216000) / 3600, 0x5E, (g_PlayTime % 3600) / 60);
                if ((s16) END_DRAW->fade_level != 0) {
                    ending_draw_text();
                }
                break;
            case 5:
                END_DRAW->tex_page = 0x1E;
                END_DRAW->tex_width = 8;
                END_DRAW->tex_height = 0xE;
                END_DRAW->clut_x = 0x100;
                END_DRAW->clut_y = 0x1E0;
                if (g_SaveCount == 0) {
                    (g_ApiCallPtr = api_sprintf)(LITP(s32, sp_text), "%02d", g_SaveCount);
                } else {
                    (g_ApiCallPtr = api_sprintf)(LITP(s32, sp_text), "%02d", g_SaveCount - 1);
                }
                if ((s16) END_DRAW->fade_level != 0) {
                    ending_draw_text();
                }
                break;
            case 6:
                (g_ApiCallPtr = api_GsSortSprite)((u8 *) g_EndingDrawCursor + 0x20, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 2U);
                break;
            case 7:
                g_EndingBoxA_r = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxA_g = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxA_b = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxB_r = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxB_g = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxB_b = (s8) ((s16) END_DRAW->fade_level >> 8);
                g_EndingBoxA_y += END_DRAW->box_vel_y;
                g_EndingBoxB_x += END_DRAW->box_vel_x;
                if (((s16) g_EndingBoxB_x < -0x140) || ((s16) g_EndingBoxB_x >= 0xA1)) {
                    END_DRAW->box_vel_x = 0U;
                    END_DRAW->box_vel_y = 0U;
                }
                (g_ApiCallPtr = api_GsSortBoxFill)(&g_EndingBoxA, (g_ActiveBuffer * 0x14) + ADDR_g_OtBg, 0xEU);
                (g_ApiCallPtr = api_GsSortBoxFill)(&g_EndingBoxB, (g_ActiveBuffer * 0x14) + ADDR_g_OtBg, 0xEU);
                break;
            }
        }
        g_EndingDrawCursor++;
        i++;
    }
}


void ending_select_id(void) {
    if ((g_ApiCallPtr = api_Flg_ck)(ADDR_g_ScenarioFlags2, 0xC0) != 0) {
        if ((g_ApiCallPtr = api_Flg_ck)(ADDR_g_ScenarioFlags2, 0x4B) != 0) {
            g_EndingId = 3;
            return;
        } else if (!(g_PlayerEntity.id & 3)) {
            g_EndingId = 1;
            return;
        } else {
            g_EndingId = 2;
            return;
        }
    }
    if ((g_ApiCallPtr = api_Flg_ck)(ADDR_g_ScenarioFlags2, 0x4B) == 0) {
        if (!(g_PlayerEntity.id & 3)) {
            g_EndingId = 4;
            return;
        } else {
            g_EndingId = 5;
            return;
        }
    }
    if (!(g_PlayerEntity.id & 3)) {
        g_EndingId = 6;
        return;
    } else {
        g_EndingId = 7;
        return;
    }
}

void ending_epilogue_update(void) {
    void ending_epilogue_build();
    void ending_check_clear_time_under_3h();
    extern u8 g_EndingId_u8 __asm__("g_EndingId");
    extern u8 g_EndingUnderTimeLimit_u8 __asm__("g_EndingUnderTimeLimit");
    switch (g_EndingSubStep) {
    case 0:
        g_UpdateFlags |= 8;
        g_VSyncMode = 2;
        g_EndingScrollMode = 1;
        g_EndingStepTimer = 0;
        ending_slots_clear();
        ending_bg_slot_setup();
        END_BG_A->fade_level = 0;
        END_BG_A->fade_vel = 0x800;
        END_BG_B->fade_level = 0;
        END_BG_B->fade_vel = 0x800;
        ending_epilogue_build();
        g_EndingSubStep = 1;
        break;
    case 1:
        g_EndingStepTimer++;
        if ((u16) g_EndingStepTimer >= 0x3DU) {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 2;
            END_EPI0->active = 1;
            END_EPI0->fade_vel = 0x800;
            END_EPI1->active = 1;
            END_EPI1->fade_vel = 0x800;
        }
        break;
    case 2:
        g_EndingStepTimer++;
        if (END_EPI0->fade_level == 0x8000) {

        }
        if ((u16) g_EndingStepTimer >= 0x3DU) {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 3;
            END_EPI2->active = 1;
            END_EPI2->fade_vel = 0x800;
            END_EPI3->active = 1;
            END_EPI3->fade_vel = 0x800;
        }
        break;
    case 3:
        g_EndingStepTimer++;
        if (END_EPI2->fade_level == 0x8000) {

        }
        if ((u16) g_EndingStepTimer >= 0x5BU) {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 4;
        }
        break;
    case 4:
        g_EndingStepTimer++;
        if (((u16) g_EndingStepTimer < 0x709U) && !(g_PadTrigger & 0x9F0 & 0xFFFF)) {

        } else {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 5;
            if (g_EndingId_u8 == 6) {
                g_EndingSubStep = 6;
            }
        }
        break;
    case 5:
        g_EndingStepTimer = 0;
        g_EndingSubStep = 0xA;
        END_BG_A->fade_vel = -0x800;
        END_BG_B->fade_vel = -0x800;
        END_EPI0->fade_vel = -0x800;
        END_EPI1->fade_vel = -0x800;
        END_EPI2->fade_vel = -0x800;
        END_EPI3->fade_vel = -0x800;
        break;
    case 6:
        g_EndingStepTimer = 0;
        g_EndingSubStep = 7;
        END_EPI0->fade_vel = -0x800;
        END_EPI1->fade_vel = -0x800;
        END_EPI2->fade_vel = -0x800;
        END_EPI3->fade_vel = -0x800;
        break;
    case 7:
        g_EndingStepTimer++;
        if ((u16) g_EndingStepTimer >= 0x5BU) {
            END_EPI4->active = 1;
            END_EPI4->fade_vel = 0x800;
            END_EPI4->tex_flags = (s32) (END_EPI4->tex_flags | 0x40000000);
            g_EndingStepTimer = 0;
            g_EndingSubStep = 8;
        }
        break;
    case 8:
        g_EndingStepTimer++;
        if (END_EPI4->fade_level == 0x8000) {
            END_EPI4->tex_flags = (s32) (END_EPI4->tex_flags & 0xBFFFFFFF);
        }
        if (((u16) g_EndingStepTimer < 0x97U) && !(g_PadTrigger & 0x9F0 & 0xFFFF)) {

        } else {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 9;
            END_BG_A->fade_vel = -0x800;
            END_BG_B->fade_vel = -0x800;
        }
        break;
    case 9:
        g_EndingStepTimer++;
        if (((u16) g_EndingStepTimer < 0x97U) && !(g_PadTrigger & 0x9F0 & 0xFFFF)) {

        } else {
            g_EndingStepTimer = 0;
            g_EndingSubStep = 0xA;
            END_EPI4->fade_vel = -0x800;
            END_EPI4->tex_flags = (s32) (END_EPI4->tex_flags | 0x40000000);
        }
        break;
    case 10:
        g_EndingStepTimer++;
        if ((u16) g_EndingStepTimer >= 0x1FU) {
            ending_check_clear_time_under_3h();
            g_EndingEffectPos = 0;
            g_EndingEffectPos_vy = 0;
            g_EndingEffectPos_vz = 0;
            END_BG_A->active = 0;
            END_BG_B->active = 0;
            END_EPI0->active = 0;
            END_EPI1->active = 0;
            END_EPI2->active = 0;
            END_EPI3->active = 0;
            END_EPI4->active = 0;
            if (g_EndingUnderTimeLimit_u8 != 0) {
                (g_ApiCallPtr = api_effect_create_billboard)(0xB, 0xA, 0, 0, &g_EndingEffectPos, 0);
                (g_ApiCallPtr = api_play_sfx)(1, 0, 0);
                (g_ApiCallPtr = api_play_sfx)(1, 1, 0);
            }
            g_EndingSubStep = 0xB;
            g_EndingStepTimer = 0;
        }
        break;
    case 11:
        g_EndingStepTimer++;
        if ((u16) g_EndingStepTimer >= 0x33U) {
            (g_ApiCallPtr = api_effect_free_matching)(3, 0xB, 0xA, 0, NULL);
            g_EndingSubStep = 0xC;
        }
        break;
    case 12:
        g_EndingStepTimer = 0;
        g_EndingStep = 0x63;
        break;
    }
    if (g_EndingUnderTimeLimit_u8 != 0) {
        (g_ApiCallPtr = api_update_2d_effects)();
    }
}

/* Capcom's source file aligned this jump table to 8 within its own .rodata: the 4 zero
   bytes after it (see docs: rodata of the stage overlays). */
__asm__(".section .rodata\n\t.word 0\n\t.text");


/* used by ending_epilogue_build and ending_result_build */
extern struct { u16 f0; s16 f2; s16 f4; u8 f6; u8 f7; } ending_sprite_layout_tbl[];

void ending_epilogue_build(void) {
    extern struct { u8 b0, b1, b2, b3; } ending_def_tbl_b[] __asm__("ending_def_tbl");
    extern struct { u8 pad[0x9C]; struct { MATRIX m; s32 x20; u8 pad24[8]; } arr[1]; } *g_RoomData_s __asm__("g_RoomData");
    extern u8 g_EndingId_u8 __asm__("g_EndingId");
    u16 i;
    u16 first;
    u16 count;
    u32 hours;
    struct { s16 x, y, w, h; } rect;

    if (ending_def_tbl_b[g_EndingId_u8 - 1].b3 != 0) {
        first = 0;
        count = 5;
    } else {
        first = 7;
        count = 5;
    }
    i = 0;
    while (i < count) {
        ending_slot_alloc();
        END_SLOT->active = 1;
        END_SLOT->type = ending_sprite_layout_tbl[first + i].f0;
        END_SLOT->ot_pri = 3;
        END_SLOT->x = ending_sprite_layout_tbl[first + i].f2 << 0x10;
        if ((g_PlayerEntity.id & 3) && (ending_def_tbl_b[g_EndingId_u8 - 1].b3 != 0)) {
            END_SLOT->x += 0x9C0000;
        }
        END_SLOT->y = ending_sprite_layout_tbl[first + i].f4 << 0x10;
        END_SLOT->fade_vel = 0;
        END_SLOT->fade_level = 0;
        END_SLOT->tex_flags = 0x10000000;
        END_SLOT->tex_u = 0;
        END_SLOT->tex_v = ending_sprite_layout_tbl[first + i].f6;
        END_SLOT->tex_width = 0x100;
        END_SLOT->tex_height = ending_sprite_layout_tbl[first + i].f7;
        END_SLOT->clut_x = 0;
        END_SLOT->clut_y = 0x1FB;
        END_SLOT->tex_page = 0x1B;
        if (END_SLOT->type == 3) {
            for (hours = g_PlayTime / 216000; (hours / 100) != 0; hours /= 10) {
                END_SLOT->x -= 0x40000;
            }
        }
        g_EndingEpilogueSlots[i] = g_EndingSlotCur;
        i++;
    }
    i = 0;
    while (i < count) {
        *(s16 *)g_EndingEpilogueSlots[i] = 0;
        i++;
    }
    ((GsRVIEW2 *)&g_EndingView)->vpx = 0x5DC;
    g_EndingView_vpy = -0x5DC;
    g_EndingView_vpz = -0x5DC;
    g_EndingView_vrx = 0x1770;
    g_EndingView_vry = 0;
    g_EndingView_vrz = 0;
    g_EndingView_rz = 0;
    g_EndingView_super = 0;
    (g_ApiCallPtr = api_GsSetProjection)((s16 *)0x82);
    (g_ApiCallPtr = api_GsSetRefView2)(&g_EndingView);
    g_CutId = 0;
    g_CameraSwitchZones = &ending_camera_zones;
    g_RoomData_s = (void *) &g_EndingRoomData;
    g_RoomData_s->arr[g_CutId].m = *(MATRIX *)&g_EndingView;
    g_RoomData_s->arr[g_CutId].x20 = 0x82;
    rect.x = 0x240;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xF0;
    (g_ApiCallPtr = api_DrawSync)(NULL);
    if (ending_def_tbl_b[g_EndingId_u8 - 1].b3 != 0) {
        (g_ApiCallPtr = api_LoadImage)(&rect, g_EndingPlayerPicBuf);
        return;
    }
    (g_ApiCallPtr = api_ClearImage)(&rect, 0, 0, 0);
}

void ending_check_clear_time_under_3h(void) {
    extern void *g_RoomData;
    if ((u32) g_PlayTime <= 0x9E340U) {
        g_EndingUnderTimeLimit = 1;
        ROOM->collision = (s32)&ending_room_block_4c;
        return;
    }
    g_EndingUnderTimeLimit = 0;
}

void ending_scroll_slot_update(void) {
    switch (g_EndingSubStep) {
    case 0:
        g_VSyncMode = 0;
        ending_slots_clear();
        ending_slot_alloc();
        END_SLOT->active = 1;
        END_SLOT->type = 6;
        END_SLOT->x = 0xA00000;
        END_SLOT->y = 0x780000;
        END_SLOT->fade_level = 0;
        END_SLOT->fade_vel = 0x300;
        END_SLOT->tex_flags = 0;
        END_SLOT->tex_width = 0x100;
        END_SLOT->tex_height = 0x44;
        END_SLOT->clut_x = 0;
        END_SLOT->clut_y = 0x1FA;
        END_SLOT->tex_page = 0xF;
        END_SLOT->tex_u = 0;
        END_SLOT->tex_v = 0;
        END_SLOT->pivot_x = 0x80;
        END_SLOT->pivot_y = 0x22;
        END_SLOT->rotate = 0;
        END_SLOT->scale_x = -0x8000;
        END_SLOT->scale_y = -0x8000;
        g_EndingTextSlot = g_EndingSlotCur;
        g_EndingSubStep = 1;
        g_EndingScrollZ = 0x2000;
        g_EndingScrollZVel = 0x300;
        g_EndingScrollZAcc = -4;
        g_EndingStepTimer = 0;
        return;
    case 1:
        g_EndingScrollZ += g_EndingScrollZVel;
        g_EndingScrollZVel += g_EndingScrollZAcc;
        END_TEXT->scale_x = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        END_TEXT->scale_y = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        if (g_EndingScrollZ > 0x97FF) {
            g_EndingSubStep = 2;
        }
        g_EndingStepTimer++;
        return;
    case 2:
        g_EndingScrollZ += g_EndingScrollZVel;
        g_EndingScrollZVel += g_EndingScrollZAcc;
        END_TEXT->scale_x = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        END_TEXT->scale_y = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        if (g_EndingScrollZ > 0xFFFF) {
            END_TEXT->fade_vel = -0x200;
            g_EndingSubStep = 3;
        }
        g_EndingStepTimer++;
        return;
    case 3:
        g_EndingScrollZ += g_EndingScrollZVel;
        g_EndingScrollZVel += g_EndingScrollZAcc;
        END_TEXT->scale_x = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        END_TEXT->scale_y = (s16) (0x10000000 / (s32) g_EndingScrollZ);
        if ((g_EndingScrollZ <= 0x177FF) && (g_EndingScrollZVel > 0)) {

        } else {
            END_TEXT->active = 0;
            g_EndingSubStep = 4;
        }
        g_EndingStepTimer++;
        return;
    case 4:
        g_EndingStepTimer++;
        if ((u16) g_EndingStepTimer >= 0x12CU) {
            g_EndingStep = 0x63;
            g_EndingStepTimer = 0;
        }
        break;
    }
}

void ending_result_update(void) {
    void ending_result_build();
    s32 ls_mat;
    s32 unused[5];

    switch (g_EndingSubStep) {
    case 0:
        g_VSyncMode = 2;
        ending_slots_clear();
        ending_bg_slot_setup();
        END_BG_A->fade_level = 0x6000;
        END_BG_B->fade_level = 0x6000;
        ending_result_build();
        g_FadeLevel = -1;
        g_FadeTypeId = 2;
        g_FadeStep = -0x800;
        (g_ApiCallPtr = api_fade_update)();
        g_EndingStepTimer = 0;
        g_EndingSubStep++;
        (g_ApiCallPtr = api_play_sfx)((s32 *)1, (s32 *)2, 0);
        (g_ApiCallPtr = api_play_sfx)((s32 *)1, (s32 *)3, 0);
        /* fallthrough */
    case 1:
        g_EndingStepTimer++;
        if (((u16) g_EndingStepTimer >= 0x10EU) || (g_PadTrigger & 0x9F0 & 0xFFFF)) {
            g_FadeTypeId = 2;
            g_FadeStep = 0x800;
            (g_ApiCallPtr = api_fade_update)();
            g_EndingSubStep++;
        }
        break;
    case 2:
        if (g_FadeLevel >= 0x7681) {
            g_EndingStep++;
        }
        break;
    }
    ending_result_draw();
    g_EndingSpinAngles_vz += 0x20;
    (g_ApiCallPtr = api_RotMatrix)(&g_EndingSpinAngles, &g_EndingCoordSpin_coord);
    g_EndingCoordSpin = 0;
    (g_ApiCallPtr = api_GsGetLs)(g_EndingModelObj_coord2, &ls_mat);
    (g_ApiCallPtr = api_GsSetLightMatrix)(&ls_mat);
    (g_ApiCallPtr = api_GsSetLsMatrix)(&ls_mat);
    (g_ApiCallPtr = api_GsSortObject5)(&g_EndingModelObj, (g_ActiveBuffer * 0x14) + ADDR_g_OtMain, 4, ADDR_sp_tmp00);
}

void ending_result_build(void) {
    extern s16 g_EndingCoordSpin_coord_s16 __asm__("g_EndingCoordSpin_coord");
    extern s16 * g_EndingModelObj_coord2_s16p __asm__("g_EndingModelObj_coord2");
    extern s16 g_EndingSpinAngles_s16 __asm__("g_EndingSpinAngles");
    extern s16 g_EndingSpinAngles_vz_s16 __asm__("g_EndingSpinAngles_vz");
    u16 i;
    s32 pad14[10];
    s32 *tmd;
    struct { s16 x, y, w, h; } rect;

    (g_ApiCallPtr = api_load_file)((s16 *)0x17, g_EndingPlayerPicBuf);
    rect.x = 0x240;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xF0;
    (g_ApiCallPtr = api_DrawSync)(NULL);
    (g_ApiCallPtr = api_LoadImage)(&rect, g_EndingPlayerPicBuf);
    i = 5;
    while (i < 7U) {
        ending_slot_alloc();
        END_SLOT->active = 1;
        END_SLOT->type = (u16) ending_sprite_layout_tbl[i].f0;
        END_SLOT->ot_pri = 3;
        END_SLOT->x = (s32) (ending_sprite_layout_tbl[i].f2 << 0x10);
        END_SLOT->y = (s32) (ending_sprite_layout_tbl[i].f4 << 0x10);
        END_SLOT->fade_vel = 0;
        END_SLOT_AS(fade_level, s16) = -0x8000;
        END_SLOT->tex_flags = 0;
        END_SLOT->tex_u = 0;
        END_SLOT->tex_v = (u8) ending_sprite_layout_tbl[i].f6;
        END_SLOT->tex_width = 0x100;
        END_SLOT->tex_height = (s16) ending_sprite_layout_tbl[i].f7;
        END_SLOT->clut_x = 0;
        END_SLOT->clut_y = 0x1FB;
        END_SLOT->tex_page = 0x1B;
        g_EndingEpilogueSlots[i] = g_EndingSlotCur;
        i++;
    }
    g_VramClutRowCursor = 0x1C;
    g_VramTpageCursor = 0x15;
    (g_ApiCallPtr = api_tim_load_to_vram)(g_EndingModelBuf);
    (g_ApiCallPtr = api_tmd_relocate_texture)((s16 *)2, g_EndingModelTmd, 0x15, 0x1C);
    (g_ApiCallPtr = api_GsInitCoordinate2)(NULL, &g_EndingCoordRoot);
    (g_ApiCallPtr = api_GsInitCoordinate2)(&g_EndingCoordRoot, &g_EndingCoordSpin);
    (g_ApiCallPtr = api_GsInitCoordinate2)(&g_EndingCoordSpin, &g_EndingCoordLeaf);
    g_EndingView = 0x2EE0;
    g_EndingView_vpy = 0;
    g_EndingView_vpz = 0;
    g_EndingView_vrx = 0;
    g_EndingView_vry = 0;
    g_EndingView_vrz = 0;
    g_EndingView_rz = 0;
    g_EndingView_super = 0;
    (g_ApiCallPtr = api_GsSetRefView2)(&g_EndingView);
    (g_ApiCallPtr = api_GsSetProjection)((s16 *)0x1A4);
    g_EndingLight = 0x64;
    g_EndingLight_vy = 0x64;
    g_EndingLight_vz = 0x64;
    g_EndingLight_r = 0xFF;
    g_EndingLight_g = 0xFF;
    g_EndingLight_b = 0xFF;
    (g_ApiCallPtr = api_GsSetFlatLight)(NULL, &g_EndingLight);
    (g_ApiCallPtr = api_GsSetAmbient)((s16 *)0xFFF, (s16 *)0xFFF, 0xFFF);
    (g_ApiCallPtr = api_GsSetLightMode)(NULL);
    tmd = (s32 *)g_EndingModelTmd;
    tmd++;
    (g_ApiCallPtr = api_GsMapModelingData)(tmd);
    tmd += 2;
    (g_ApiCallPtr = api_GsLinkObject5)(tmd, &g_EndingModelObj, 0);
    g_EndingModelObj = 0;
    g_EndingModelObj_coord2_s16p = (s16 *)&g_EndingCoordLeaf;
    g_EndingPacketCur = g_EndingPacketBuf;
    g_EndingPacketCur = (s16 *)(g_ApiCallPtr = api_GsPresetObject)(&g_EndingModelObj, g_EndingPacketCur);
    g_EndingSpinAngles_s16 = 0xAA;
    g_EndingSpinAngles_vy = 0;
    g_EndingSpinAngles_vz_s16 = 0;
    (g_ApiCallPtr = api_RotMatrix)(&g_EndingSpinAngles_s16, &g_EndingCoordLeaf_coord);
    g_EndingCoordLeaf = 0;
    g_EndingSpinAngles_s16 = 0;
    g_EndingSpinAngles_vy = 0;
    g_EndingSpinAngles_vz_s16 = 0;
    (g_ApiCallPtr = api_RotMatrix)(&g_EndingSpinAngles_s16, &g_EndingCoordSpin_coord_s16);
    g_EndingCoordSpin = 0;
    g_EndingSpinAngles_s16 = -0x6A4;
    g_EndingSpinAngles_vy = 0;
    g_EndingSpinAngles_vz_s16 = 0;
    (g_ApiCallPtr = api_RotMatrix)(&g_EndingSpinAngles_s16, &g_EndingCoordRoot_coord);
    g_EndingCoordRoot_tx = 0;
    g_EndingCoordRoot_ty = 0x1F4;
    g_EndingCoordRoot_tz = 0;
    g_EndingCoordRoot = 0;
    g_EndingSpinAngles_s16 = 0;
    g_EndingSpinAngles_vy = 0;
    g_EndingSpinAngles_vz_s16 = 0;
}

void ending_draw_text(void) {
    extern EndingSlot *g_EndingDrawCursor;
    u8 ch;
    u8 *str;

    str = LITP(u8, sp_text);
    for (;;) {
    ch = *str;
    if (ch != 0x20) {
        END_DRAW->tex_u = (s8) (((*str % 18) & 0xFF) * 8);
        END_DRAW->tex_v = (s8) (((*str / 18) & 0xFF) * 0xE);
        (g_ApiCallPtr = api_GsSortFastSprite)((u8 *) g_EndingDrawCursor + 0x20, (g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables, 2);
    }
    END_DRAW->tex_screen_x = (u16) (END_DRAW->tex_screen_x + 8);
    str++;
        if (*str == 0) {
            return;
        }
    }
}
