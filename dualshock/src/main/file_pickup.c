/* file_pickup.c - the screen shown when a file (document) is picked up: fade, collected flag, file book entry */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Defined earlier in the same file before it was split (tools/splitfile.py): what these
   functions saw when they were one file. */
void file_reader_advance(void *scr);

s32 pickup_fade_update(u32 mode) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    s32 ret;
    s32 x;

    ret = 0;
    switch (mode) {
    case 0:
        x = g_PickupFadeBox_R[0] + 0x20;
        g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = x;
        if (x >= 0xFF) {
            ret = -1;
            g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = 0xFF;
        }
        break;
    case 1:
        x = g_PickupFadeBox_R[0] - 0x20;
        g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = x;
        if (x <= 0) {
            ret = -1;
            g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = 0;
        }
        break;
    case 2:
        g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = 0xFF;
        break;
    case 3:
        g_PickupFadeBox_R[0] = g_PickupFadeBox_G = g_PickupFadeBox_B = 0;
        break;
    }
    GsSortBoxFill(&g_PickupFadeBox, (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 3);
    return ret;
}

void file_pickup_init(void) {
    void menu_reset_status_state(void);
    s32 pickup_screen_init_table_a1() __asm__("pickup_screen_init_table");
    pickup_screen_init_table_a1();
    menu_reset_status_state();
    g_FilePickupScreen_Closing = 1;
}

s32 file_pickup_update(void) {
    s32 pickup_fade_update(u32);
    void pickup_mark_seen(void *scr);
    void file_set_collected_flag(s32);
    void set_message_display();
    void Flg_off(void *, u8);
    extern u8 * g_ItemModelTable_u8p[] __asm__("g_ItemModelTable");
    extern u8 * g_pRoomActionEntry_u8p __asm__("g_pRoomActionEntry");
    extern u8 g_FilePickupScreen_u8[] __asm__("g_FilePickupScreen");
    extern u8 g_FilePickupScreen_Closing_u8 __asm__("g_FilePickupScreen_Closing");
    extern u8 g_FileBookTable_u8[] __asm__("g_FileBookTable");
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");
    int LoadImage(RECT *rect, u_long *p);
    u8 *p;
    u8 *q;
    s32 k;
    u32 i;

    p = g_FilePickupScreen_u8;
    switch (p[0]) {
    case 0:
        if (g_FilePickupScreen_State == 0 && pickup_fade_update(0) != 0) {
            p[0] = 1;
            if (g_GameStateFlags & GAME_ALT_RDT) {
                g_FilePickupScreen_FileSet = 1;
            } else {
                g_FilePickupScreen_FileSet = 0;
            }
            k = SCD_ZONE(ROOM_ACTION(g_pRoomActionEntry_u8p)->rec)->item_id - 0x5F;
            file_set_collected_flag(k);
            pickup_mark_seen(p);
            play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            q = &g_FileBookTable_u8[(s8)p[7] * 16];
            for (i = 0; i < 16; i++) {
                if (*q++ == k) {
                    p[4] = i >> 3;
                    p[5] = i & 7;
                    p[6] = 0;
                    p[9] = 1;
                    p[0x10] = 0;
                }
            }
        }
        if (p[1] == 1) {
            if (pickup_fade_update(1) != 0) {
                p[8] = 1;
            }
            if (p[3] == 2) {
                LIT(s16, sp_rect_x) = 0x180;
                LIT(s16, sp_rect_y) = 0x100;
                LIT(s16, sp_rect_w) = 0x40;
                LIT(s16, sp_rect_h) = 0x100;
                LoadImage(LITP(RECT, sp_rect), LITP(u_long, g_RoomBgBsBuf));
                ENTITY_STATE_WORD(&g_PlayerEntity) = 1;
                g_PlayerEntity.blend_counter = 0;
                g_PlayerEntity.animation_id = 0;
                g_PlayerEntity.animation_frame_id = 0;
                g_PlayerEntity.timing_control = 0;
                joint_move(0, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
                p[3] = 0;
                g_SelectedItemId = SCD_ZONE(ROOM_ACTION(g_pRoomActionEntry_u8p)->rec)->item_id;
                set_message_display(0xC6);
            }
        }
        break;
    case 1:
        file_reader_advance(p);
        if (g_FilePickupScreen_Closing_u8 == 0) {
            if (LIT(u16, g_PadPressed) & PAD_CANCEL) {
                g_FilePickupScreen_NextMenu = 13;
                g_FilePickupScreen_Closing_u8 = 1;
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
            if ((LIT(u16, g_PadPressed) & PAD_CONFIRM) && g_FilePickupScreen_ExitSelected == 1) {
                g_FilePickupScreen_NextMenu = 13;
                g_FilePickupScreen_Closing_u8 = 1;
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            }
        }
        break;
    }
    if (p[8] != 1) {
        return 0;
    }
    if (g_MessageStatus_u8 & 0x80) {
        return 0;
    }
    *g_ItemModelTable_u8p[((u8 *) ROOM_ACTION(g_pRoomActionEntry_u8p)->rec)[0xA]] = 0;
    *g_pRoomActionEntry_u8p = 0;
    Flg_off(LITP(void, g_RoomItemsFlags), SCD_ZONE(ROOM_ACTION(g_pRoomActionEntry_u8p)->rec)->picked_flag);
    g_PickedItemId = SCD_ZONE(ROOM_ACTION(g_pRoomActionEntry_u8p)->rec)->item_id;
    p[8] = 0;
    return -1;
}

void menu_reset_status_state(void) {
    s32 pickup_screen_init_table_a2() __asm__("pickup_screen_init_table");
    pickup_screen_init_table_a2();
    g_FilePickupScreen = 0;
    g_FilePickupScreen_ListPage = 0;
    g_FilePickupScreen_SelectedFile = 0;
    g_FilePickupScreen_FilePage = 0;
    g_FilePickupScreen_FileSet = 0;
    g_FilePickupScreen_CloseRequest = 0;
    g_FilePickupScreen_Closing = 0;
    g_FilePickupScreen_SlideSpeed = 0;
    g_FilePickupScreen_SlideX = 0;
    g_FilePickupScreen_SlideY = 0;
    g_FilePickupScreen_ExitSelected = 0;
    g_FilePickupScreen_ArrowBlink = 0;
    g_FilePickupScreen_BlinkTimer = 0;
    g_FilePickupScreen_HoldTimer = 0;
}

void pickup_mark_seen(void *scr) {
    extern u8 g_FileBookTable_u8[] __asm__("g_FileBookTable");
  s8 *file_set_p;
  u8 *table;
  s32 i;
  u8 *book_table = g_FileBookTable_u8;
  i = 0;
  table = &(*book_table);
  loop_1:
  if ((*((((*((s8 *) (((s8 *) scr) + 7))) * 0x10) + table) + i)) == 0xFE)
  {
    if (file_is_collected(0) != 0)
    {
      file_set_p = (s8 *) (((s8 *) scr) + 7);
      *((((*file_set_p) * 0x10) + table) + i) = 0;
      return;
    }
    if (file_is_collected(1) != 0)
    {
      *((((*((s8 *) (((s8 *) scr) + 7))) * 0x10) + table) + i) = 1;
      return;
    }
    goto block_6;
  }

  block_6:
  i += 1;

  if (i >= 0x10)
  {
    return;
  }
  goto loop_1;
}

void pickup_screen_init_table(void) {
    g_FileBookTable[0][0] = 0xF;
    g_FileBookTable[0][1] = 2;
    g_FileBookTable[0][2] = 0xFE;
    g_FileBookTable[0][3] = 3;
    g_FileBookTable[0][4] = 5;
    g_FileBookTable[0][5] = 6;
    g_FileBookTable[0][6] = 7;
    g_FileBookTable[0][7] = 8;
    g_FileBookTable[0][8] = 9;
    g_FileBookTable[0][9] = 0xA;
    g_FileBookTable[0][10] = 0xC;
    g_FileBookTable[0][11] = 0xD;
    g_FileBookTable[0][12] = 0xE;
    g_FileBookTable[0][13] = 0xFF;
    g_FileBookTable[0][14] = 0xFF;
    g_FileBookTable[0][15] = 0xFF;
    g_FileBookTable[1][0] = 0xF;
    g_FileBookTable[1][1] = 2;
    g_FileBookTable[1][2] = 0xFE;
    g_FileBookTable[1][3] = 3;
    g_FileBookTable[1][4] = 4;
    g_FileBookTable[1][5] = 5;
    g_FileBookTable[1][6] = 6;
    g_FileBookTable[1][7] = 7;
    g_FileBookTable[1][8] = 8;
    g_FileBookTable[1][9] = 9;
    g_FileBookTable[1][10] = 0xA;
    g_FileBookTable[1][11] = 0xB;
    g_FileBookTable[1][12] = 0xC;
    g_FileBookTable[1][13] = 0xD;
    g_FileBookTable[1][14] = 0xE;
    g_FileBookTable[1][15] = 0xFF;
}
