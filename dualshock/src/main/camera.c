/* camera.c - room cameras: camera cuts, background image loading, camera switch zones */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 room_bg_verify_checksum(void);

void cut_set(void) {
    void room_setup_camera(void);
    s32 load_room_bg_image_a1() __asm__("load_room_bg_image");
    s32 room_sprite_apply_hide_mask();
    if (LIT(u8, g_NextCamera) != 0) {
        g_VramTpageCursor = (u16) LIT(s16, g_NextCamera);
        room_load_camera_sprites();
        if (!(g_GameStateFlags & GAME_DOOR_ANIM)) {
            room_setup_camera();
            load_room_bg_image_a1();
            room_sprite_apply_hide_mask();
        }
    }
}

void room_setup_camera(void) {
    s32 GsSetProjection(s32);
    int GsSetRefView2(GsRVIEW2 *pv);
  s32 rdt;
  int cam;
  rdt = LIT(s32, g_RoomData);
  cam = rdt + ((LIT(u8, g_CutId)) * 0x2C);
  GsSetProjection(*((s32 *) (((s8 *) cam) + 0xBC)));
  GsSetRefView2((GsRVIEW2 *)ROOM_CAMERA_VIEW_ADDR(LIT(u8, g_CutId)));
  if (1)
  {
  }
}

void load_room_bg_image(void) {
    int DrawSync(int mode);
    int StoreImage(RECT *rect, u_long *p);
    int MoveImage(RECT *rect, int x, int y);
    int GsGetActiveBuff(void);
    typedef struct {
        u32 size;
        u16 lba;
        u8 lba_hi;
        u8 sum;
    } CdFileEntryView;
    extern CdFileEntryView g_CdFileTable_l[] __asm__("g_CdFileTable");
    extern u8 g_VibrationStrong_l __asm__("g_VibrationStrong");
    extern u8 g_VibrationWeak_l __asm__("g_VibrationWeak");
    extern u8 g_RoomId_l __asm__("g_RoomId");
    s32 room_get_rdt_stage_index_l() __asm__("room_get_rdt_stage_index");
    s32 room_bg_verify_checksum_l() __asm__("room_bg_verify_checksum");
    void load_file_blocking_l(u16, u32) __asm__("load_file_blocking");
    void mdec_decode_image_to_vram_l(RECT *, u32, u32, u32, s32) __asm__("mdec_decode_image_to_vram");
    RECT r;
    u8 save[2];
    u8 st;
    u16 *tbl;
    u16 old;
    u32 v;
    u8 *p;

    r.x = 0;
    r.y = 0;
    r.w = 0x140;
    r.h = 0xF0;
    if (GsGetActiveBuff() != 0) {
        r.y = 0xF0;
    }
    LIT(s32, g_StatusFlags) |= STATUS_BG_LOADING;
    p = &g_VibrationStrong_l;
    save[0] = *p;
    save[1] = g_VibrationWeak_l;
    *p = 0;
    g_VibrationWeak_l = 0;
    st = room_get_rdt_stage_index_l();
    if ((u8)(st - 5) < 2 || (u8)(st - 12) < 2) {
        st -= 5;
    }
    old = g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].lba;
    g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].size = 0x8000;
    v = g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].lba;
    v += LIT(u8, g_CutId) * 16;
    g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].lba_hi += v >> 16;
    tbl = g_RoomFileTable[st];
    g_SkipCdChecksum = 1;
    g_CdFileTable_l[tbl[LIT(u8, g_RoomId)]].lba += LIT(u8, g_CutId) * 16;
    do {
        load_file_blocking_l(tbl[g_RoomId_l], ADDR_g_RoomBgBsBuf);
    } while (room_bg_verify_checksum_l() == 0);
    g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].lba = old;
    g_CdFileTable_l[g_RoomFileTable[st][LIT(u8, g_RoomId)]].lba_hi -= v >> 16;
    g_SkipCdChecksum = 0;
    mdec_decode_image_to_vram_l(&r, ADDR_g_RoomBgBsBuf, ADDR_g_MdecRunLevelBuf, ADDR_g_DoorFile, 0);
    StoreImage(&r, LITP(u_long, g_DoorFile));
    DrawSync(0);
    r.y = -(r.y != 0xF0) & 0xF0;
    MoveImage(&r, 0, -(r.y != 0xF0) & 0xF0);
    DrawSync(0);
    g_VibrationStrong_l = save[0];
    g_VibrationWeak_l = save[1];
    LIT(s32, g_StatusFlags) &= ~STATUS_BG_LOADING;
}

void check_camera_switch(s32 refresh) {
    s32 cut_set_p() __asm__("cut_set");
    void display_room_camera_bg(void);
    s32 is_point_in_quad_a1() __asm__("is_point_in_quad");
  s32 flags;
  u8 *zone;
  int busy;
  zone = g_CameraSwitchZones + 0x14;
  if (!(g_GameStateFlags & GAME_CUT_LOCKED))
  {
    if ((*((u16 *) (((s8 *) g_CameraSwitchZones) + 0x16))) == g_CutId)
    {
      s32 *state_flags = LITP(s32, g_GameStateFlags);
      loop_3:
      if (is_point_in_quad_a1(ADDR_g_PlayerPosX, zone) != 0)
      {
        flags = *state_flags;
        busy = flags & 4;
        g_CutId = *zone;
        if (busy)
        {
          *state_flags = flags | 0x20;
          return;
        }
        display_room_camera_bg();
        return;
      }

      zone += 0x14;
      if ((*((u16 *) (((s8 *) zone) + 2))) != g_CutId)
      {
        goto block_8;
      }
      goto loop_3;
    }
    block_8:
    if (refresh != 0)
    {
      cut_set_p();
    }

  }
}

void display_room_camera_bg(void) {
    void **zp;
    void *zones;
    void *cur;

    zones = ((void *) ROOM->camera_switches);
    g_CameraSwitchZones = zones;
    zp = (void **)0x800C0000;
    if (CAM_ZONE(zones)->cam_from != g_CutId) {
        zp = LITP(void *, g_CameraSwitchZones);
        do {
            cur = LIT(void *, g_CameraSwitchZones);
            LIT(void *, g_CameraSwitchZones) = cur + 0x14;
        } while (FIELD_AT(cur, CamSwitchZone, [1].cam_from, u16) != g_CutId);
    }
    cut_set();
}

s32 is_point_in_quad(s32 *pos, u16 *quad) {
    s32 x0;
    s32 y0;
    s32 ax;
    s32 ay;
    s32 px;
    s32 py;
    s32 bx;
    s32 by;
    s32 cx;
    s32 cy;

    x0 = quad[2];
    y0 = quad[3];
    py = pos[2] - y0;
    ax = quad[4] - x0;
    px = pos[0] - x0;
    ay = quad[5] - y0;
    bx = quad[8] - x0;
    by = quad[9] - y0;
    if (ax * py > ay * px) {
        return 0;
    }
    if (bx * py < by * px) {
        return 0;
    }
    cy = quad[7] - y0;
    cx = quad[6] - x0;
    py -= cy;
    ax -= cx;
    px -= cx;
    ay -= cy;
    bx -= cx;
    by -= cy;
    if (ax * py < ay * px || bx * py > by * px) {
        return 0;
    }
    return 1;
}
