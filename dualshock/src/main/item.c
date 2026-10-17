/* item.c - The item viewer: the 3D item model of the status / pickup menus (rotate, zoom, examine). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void load_file_blocking(void);



s32 item_viewer_update(void) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
    void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
    void GsMapModelingData(u_long *p);
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    int GsSetFlatLight(int id, GsF_LIGHT *lt);
    extern s32 (*g_ItemViewerActionFuncs_l[])() __asm__("g_ItemViewerActionFuncs");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    void item_viewer_draw_model_l() __asm__("item_viewer_draw_model");
    s32 set_message_display(u8, s16);
    u32 *p;
    u32 tmd;
    u8 n;
    u8 k;
    u8 m;
    u8 *e;
    u8 t;
    u8 *c;

    switch (g_MenuWork.viewer_state) {
    case 0:
        g_MenuWork.viewer_state = 1;
        GsMapModelingData(LITP(u_long, g_ItemModelTmdFlags));
        tmd = ADDR_g_ItemModelTmdObjs;
        g_MenuWork.obj_count = g_ItemModelTmdNobj;
        c = (u8 *)&g_MenuWork.coord[0];
        GsInitCoordinate2(NULL, (GsCOORDINATE2 *)&g_MenuWork.coord[2]);
        p = LITP(u32, g_PlayerObjPacketBuf);
        GsInitCoordinate2((GsCOORDINATE2 *)&g_MenuWork.coord[2], (GsCOORDINATE2 *)&g_MenuWork.coord[1]);
        GsInitCoordinate2((GsCOORDINATE2 *)&g_MenuWork.coord[1], (GsCOORDINATE2 *)c);
        n = g_MenuWork.obj_count;
        k = 2 - n;
        do {
            do {
                n--;
                GsLinkObject5(tmd, (GsDOBJ5 *)&g_MenuWork.obj[k], n);
                g_MenuWork.obj[k].attribute = 0;
                g_MenuWork.obj[k].coord2 = &g_MenuWork.coord[n];
                p = (u32 *)GsPresetObject((GsDOBJ5 *)&g_MenuWork.obj[k], (u_long *)p);
                do {
                    do {
                        g_MenuWork.obj_rot[k].vx = 0;
                        g_MenuWork.obj_rot[k].vy = 0;
                        g_MenuWork.obj_rot[k].vz = 0;
                        g_MenuWork.coord[k].coord.t[0] = 0;
                        g_MenuWork.coord[k].coord.t[1] = 0;
                        g_MenuWork.coord[k].coord.t[2] = 0;
                    } while (0);
                } while (0);
                k++;
            } while (0);
        } while (n != 0);
        g_MenuWork.rot.vx = 0;
        g_MenuWork.rot.vy = 0;
        g_MenuWork.rot.vz = 0;
        g_MenuWork.coord[2].coord.t[0] = 0xFFFF5100;
        g_MenuWork.coord[2].coord.t[1] = 0;
        g_MenuWork.coord[2].coord.t[2] = 0;
        g_MenuWork.spin_input_x = 0;
        g_MenuWork.spin_input_y = 0;
        g_MenuWork.spin_input_z = 0;
        RotMatrix(&g_MenuWork.rot, &g_MenuWork.coord[2].coord);
        g_MenuWork.timer = 0x40;
    case 1:
        g_MenuWork.coord[2].coord.t[0] += 0x322;
        g_MenuWork.rot.vy += 0xC0;
        g_MenuWork.timer--;
        g_MenuWork.rot.vz += 0x80;
        if (g_MenuWork.timer == 0) {
            switch (g_MenuWork.mode) {
            case MENU_MODE_STATUS:
                g_MenuWork.viewer_state = 2;
                g_MenuWork.viewer_action = 0;
                break;
            case MENU_MODE_PICKUP:
                g_MenuWork.viewer_state = 3;
                t = g_MenuWork.item_id;
                LIT(u8, g_SelectedItemId) = t;
                if (g_TotalHeldItems < g_MenuWork.slot_count) {
                    set_message_display(0xC0, 0);
                    break;
                }
                if ((u32)(t - 0xB) < 8 || LIT(u8, g_SelectedItemId) == 0x2F) {
                    m = *(u8 *)(((u8 *) ROOM_ACTION(g_pRoomActionEntry)->rec) + 9);
                    if (LIT(u8, g_SelectedItemId) == 0x2F) {
                        m = 2;
                        if (g_StatusFlags & STATUS_ADVANCED) {
                            m = 3;
                        }
                    }
                    if ((g_StatusFlags & STATUS_TRAINING) || (g_StatusFlags & STATUS_ADVANCED_HOLD)) {
                        m *= 2;
                    }
                    k = 0;
                    n = g_MenuWork.slot_count;
                    do {
                        e = (u8 *)(g_ItemSlotsPointer + k * 2);
                        n--;
                        if (INV_SLOT(e)->item_id == g_SelectedItemId && (u32)(m + INV_SLOT(e)->count) < 0xFB) {
                            set_message_display(0xC0, 0);
                            break;
                        }
                        k++;
                    } while (n != 0);
                }
                set_message_display(0xC2, 0);
                break;
            case MENU_MODE_GOT_ITEM:
                g_MenuWork.viewer_state = 3;
                g_SelectedItemId = g_MenuWork.item_id;
                set_message_display(0xC1, 0);
                break;
            case MENU_MODE_KEY_USED_UP_VIEW:
                g_MenuWork.viewer_state = 4;
                set_message_display(0xC7, 0);
                break;
            }
        }
        g_ClearColorB = 0x20 - (g_MenuWork.timer >> 1);
        n = (0x40 - g_MenuWork.timer) * 4 - 1;
        g_ViewerLights[0].r = n;
        g_ViewerLights[0].g = n;
        g_ViewerLights[0].b = n;
        g_ViewerLights[1].r = n;
        g_ViewerLights[1].g = n;
        g_ViewerLights[1].b = n;
        g_ViewerLights[2].r = n;
        g_ViewerLights[2].g = n;
        g_ViewerLights[2].b = n;
        GsSetFlatLight(0, &g_ViewerLights[0]);
        GsSetFlatLight(1, &g_ViewerLights[1]);
        GsSetFlatLight(2, &g_ViewerLights[2]);
        break;
    case 2:
        g_ItemViewerActionFuncs_l[g_MenuWork.viewer_action]();
        break;
    case 4:
        g_MenuWork.rot.vy -= 0x10;
    case 3:
        if (g_MessageStatus_l & 0x80) {
            if (g_MenuWork.mode != MENU_MODE_GOT_ITEM && *g_MessageCursor == 8 && (g_PadTrigger & (PSX_RIGHT | PSX_LEFT))) {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            }
        } else {
            g_MenuWork.viewer_state = 5;
            g_MenuWork.timer = 0x40;
            if (g_MenuWork.mode == MENU_MODE_GOT_ITEM) {
                room_event_take_item();
            } else if (g_MessageStatus_l & 1) {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            } else {
                play_sfx(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            }
        }
        break;
    case 5:
        g_MenuWork.timer--;
        if (g_MenuWork.timer == 0) {
            return 1;
        }
        g_MenuWork.coord[2].coord.t[0] -= 0x322;
        g_MenuWork.rot.vy -= 0xC0;
        n = g_MenuWork.timer * 4 - 1;
        g_MenuWork.rot.vz -= 0x80;
        g_ClearColorB = g_MenuWork.timer >> 1;
        g_ViewerLights[0].r = n;
        g_ViewerLights[0].g = n;
        g_ViewerLights[0].b = n;
        g_ViewerLights[1].r = n;
        g_ViewerLights[1].g = n;
        g_ViewerLights[1].b = n;
        g_ViewerLights[2].r = n;
        g_ViewerLights[2].g = n;
        g_ViewerLights[2].b = n;
        GsSetFlatLight(0, &g_ViewerLights[0]);
        GsSetFlatLight(1, &g_ViewerLights[1]);
        GsSetFlatLight(2, &g_ViewerLights[2]);
        break;
    }
    if (g_MenuWork.viewer_state != 0) {
        item_viewer_draw_model_l();
    }
    if (g_MenuWork.mode == MENU_MODE_STATUS && g_MenuWork.viewer_action == 0) {
        draw_item_name(g_MenuWork.item_id, 0, (u16)(0x30 - g_ScreenOffsetX), (u16)(0xBA - g_ScreenOffsetY));
    }
    return 0;
}

void item_viewer_action_display(void) {
    s32 item_viewer_update_rotation_a1() __asm__("item_viewer_update_rotation");
    s32 item_viewer_draw_arrows_a1() __asm__("item_viewer_draw_arrows");
    u8 item_viewer_check_examine_a1() __asm__("item_viewer_check_examine");
    s32 draw_item_name_a1(u8, s32, s32, s32) __asm__("draw_item_name");
    s32 set_item_description_message_a1(s32, s32) __asm__("set_item_description_message");
    extern u16 g_ScreenOffsetX_u16 __asm__("g_ScreenOffsetX");
    s32 zoom;
    s32 zoom2;

    if (g_PadPressed & PAD_CONFIRM) {
        if (item_viewer_check_examine_a1()) {
            if (g_MenuWork.viewer_action == 1) {
                draw_item_name_a1(g_MenuWork.item_id, 0, (0x30 - g_ScreenOffsetX_u16) & 0xFFFF, (0xBA - g_ScreenOffsetY) & 0xFFFF);
            }
        } else {
            g_MenuWork.viewer_action = 2;
            set_item_description_message_a1((g_MenuWork.item_id - 1) & 0xFFFF, 0);
        }
    } else {
        if (g_PadPressed & PAD_CANCEL) {
            g_MenuWork.viewer_state = 5;
            g_MenuWork.timer = 0x40;
            g_MenuWork.spin_input_y = 0;
            goto block_28;
        }
        if (g_PadHeld & 0x3000) {
            if (g_PadHeld & 0x1000) {
                zoom = g_ViewerZoom[0] + 0x64;
                g_ViewerZoom[0] = zoom;
                if (zoom >= 0x2921) {
                    g_ViewerZoom[0] = 0x2920;
                }
            } else {
                zoom2 = g_ViewerZoom[0] - 0x64;
                g_ViewerZoom[0] = zoom2;
                if (zoom2 < 0x1980) {
                    g_ViewerZoom[0] = 0x1980;
                }
            }
        }
        if (g_PadHeld & 0xC00) {
            if (g_PadHeld & 0x800) {
                g_MenuWork.spin_input_x = 0x20;
            } else {
                g_MenuWork.spin_input_x = -0x20;
            }
        } else {
            g_MenuWork.spin_input_x = 0;
        }
        if (g_PadHeldRaw & (PSX_RIGHT | PSX_LEFT)) {
            if (g_PadHeldRaw & PSX_LEFT) {
                g_MenuWork.spin_input_y = 0x20;
            } else {
                g_MenuWork.spin_input_y = -0x20;
            }
        } else {
            g_MenuWork.spin_input_y = 0;
        }
        if (g_PadHeldRaw & (PSX_UP | PSX_DOWN)) {
            if (g_PadHeldRaw & PSX_UP) {
                g_MenuWork.spin_input_z = -0x20;
            } else {
                g_MenuWork.spin_input_z = 0x20;
            }
        } else {
block_28:
            g_MenuWork.spin_input_z = 0;
        }
        if ((g_PadHeldRaw & PSX_DPAD) || (g_PadHeld & 0xC00)) {
            item_viewer_update_rotation_a1();
        }
        item_viewer_draw_arrows_a1();
    }
}

void item_viewer_action_examine_spin(void) {
    s32 draw_item_name_a2(u8, s32, s32, s32) __asm__("draw_item_name");
    s32 set_message_display_a3(u8, s32) __asm__("set_message_display");
    extern u16 g_ScreenOffsetX_u16 __asm__("g_ScreenOffsetX");
    extern u16 g_ViewerExamineSpin_u16 __asm__("g_ViewerExamineSpin");
    u8 *p;
    u8 s;
    s16 t;

    p = &g_MenuWork.timer;
    (*p)++;
    s = *p * 4;
    if (s > 0x20) {
        s = 0x20;
    }
    t = (s + g_ViewerExamineSpin_u16) & 0xFFF;
    g_ViewerExamineSpin_u16 = t;
    g_ViewerObjRot0Y = (g_ViewerObjRot0Y - s * 2) & 0xFFF;
    if (t >= 0x301) {
        g_MenuWork.viewer_action = 2;
        set_message_display_a3(g_ItemExamineMessages[g_ItemExamineIdx[({ g_MenuWork.item_id - 1; }) * 4]], 0);
    } else {
        draw_item_name_a2(g_MenuWork.item_id, 0, (0x30 - g_ScreenOffsetX_u16) & 0xFFFF, (0xBA - g_ScreenOffsetY) & 0xFFFF);
    }
}

void item_viewer_action_examine_wait(void) {
    s32 Flg_on(s32, s32);
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");
  s32 item_id;
  if (!(g_MessageStatus_u8 & 0x80))
  {
    /* items 0x3E..0x40: the Doom Books - turned in the viewer they open and show the crest inside,
       needed to open the way to the lab (seen in game) */
    if (g_MenuWork.item_id != 0x3E)
    {
      if ((((item_id = (s32) g_MenuWork.item_id) >= 0x3E) && (item_id < 0x41)) && (g_ViewerExamineSpin != 0))
      {
        g_MenuWork.viewer_action = 3;
        g_MenuWork.timer = 0x40;
        return;
      }
      goto block_8;
    }
    if (g_ViewerExamineSpin != 0)
    {
      Flg_on(ADDR_g_ItemExaminedFlags, 0xD);
    }
    block_8:
    g_MenuWork.viewer_action = 0;

  }
}

void item_viewer_action_book_to_medal(void) {
    extern s32 g_ViewerZoom_s32 __asm__("g_ViewerZoom");
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    int GsSetFlatLight(int id, GsF_LIGHT *lt);
    void load_file_blocking_l(s32, s32) __asm__("load_file_blocking");
    void item_icon_upload(s32, s32, s32);
    void menu_load_item_model(void);
    u8 v;
    u8 idx;
    u8 *cnt;

    cnt = &g_MenuWork.timer;
    (*cnt)--;
    g_ViewerZoom_s32 -= 0x322;
    g_ViewerRotY -= 0xC0;
    v = *cnt * 4 - 1;
    g_ViewerRotZ -= 0x80;
    g_ClearColorB = *cnt >> 1;
    g_ViewerLights[0].r = v;
    g_ViewerLights[0].g = v;
    g_ViewerLights[0].b = v;
    g_ViewerLights[1].r = v;
    g_ViewerLights[1].g = v;
    g_ViewerLights[1].b = v;
    g_ViewerLights[2].r = v;
    g_ViewerLights[2].g = v;
    g_ViewerLights[2].b = v;
    GsSetFlatLight(0, &g_ViewerLights[0]);
    GsSetFlatLight(1, &g_ViewerLights[1]);
    GsSetFlatLight(2, &g_ViewerLights[2]);
    if (*cnt == 0) {
        g_MenuWork.item_id += 0xC1;
        idx = (g_MenuWork.cursor >> 1) - 4;
        load_file_blocking_l(0x11, ADDR_g_DoorFile);
        item_icon_upload(g_MenuWork.item_id, g_ItemSlotIconIndex[idx], ADDR_g_DoorFile);
        g_MenuWork.item_id += 0x24;
        g_ItemSlotsPointer_u8p[idx * 2] = g_MenuWork.item_id;
        g_ItemSlotsPointer_u8p[idx * 2 + 1] = 1;
        menu_load_item_model();
        g_MenuWork.viewer_state = 0;
    }
}

typedef struct {
    s32 flg;
    MATRIX coord;
    MATRIX workm;
    u8 pad[0xC];
} ViewerCoord; /* size 0x50 */
typedef struct {
    u32 attribute;
    GsCOORDINATE2 *coord2;
    u32 *tmd;
    u32 *packet;
    u32 id;
} ViewerDobj; /* size 0x14 */
typedef struct {
    SVECTOR angs[2];
    SVECTOR rot;
    ViewerDobj obj[2];
    ViewerCoord coords[3];
} ViewerModel;

void item_viewer_draw_model(void) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m);
    void GsSetLightMatrix(MATRIX *mp);
    void GsSetLsMatrix(MATRIX *mp);
    void GsSortObject5(GsDOBJ5 *objp, GsOT *otp, int shift, u_long *scratch);
    extern ViewerModel g_ViewerObjRot_g __asm__("g_ViewerObjRot");
    s32 i;
    s32 n;
    s32 j;

    g_ViewerObjRot_g.rot.vx &= 0xFFF;
    g_ViewerObjRot_g.rot.vy &= 0xFFF;
    g_ViewerObjRot_g.rot.vz &= 0xFFF;
    RotMatrix(&g_ViewerObjRot_g.rot, &g_ViewerObjRot_g.coords[2].coord);
    j = 2;
    g_ViewerObjRot_g.coords[2].flg = 0;
    n = g_MenuWork.obj_count;
    n--;
    do {
        j--;
        i = j & 0xFF;
        RotMatrix(&g_ViewerObjRot_g.angs[i], &g_ViewerObjRot_g.coords[i].coord);
        g_ViewerObjRot_g.coords[i].flg = 0;
        GsGetLs(g_ViewerObjRot_g.obj[i].coord2, LITP(MATRIX, sp_mat180));
        GsSetLightMatrix(LITP(MATRIX, sp_mat180));
        GsSetLsMatrix(LITP(MATRIX, sp_mat180));
        GsSortObject5((GsDOBJ5 *)&g_ViewerObjRot_g.obj[i], &LITP(GsOT, g_OtMain)[g_ActiveBuffer], 4, LITP(u_long, sp_tmp00));
    } while (n-- & 0xFF);
}

/* The item viewer's model block around g_ViewerZoom (PC: Sca[2]). Its base matrix's translation x IS
   g_ViewerZoom, and the original keeps &g_ViewerZoom in a register and reaches the rest at negative offsets,
   so VIEWER_FIELD keeps that form: &g_ViewerZoom + (field offset - offset of base.t[0]). */
typedef struct ViewerBlock {
    /* 0x00 */ SVECTOR model_rot;           /* RotMatrix input (0x800BC5E4) */
    /* 0x08 */ u8 pad08[0x10];
    /* 0x18 */ SVECTOR view_rot;            /* vy = g_ViewerRotY, vz = g_ViewerRotZ */
    /* 0x20 */ u8 pad20[0xEC - 0x20];
    /* 0xEC */ MATRIX base;                 /* t[0] = g_ViewerZoom, t[1] = g_ViewerTransY, t[2] = g_ViewerTransZ */
} ViewerBlock;
#define VIEWER_FIELD(path, T) (*(T *)((s8 *)&g_ViewerZoom_s8 + (PATH_OFF(ViewerBlock, path) - PATH_OFF(ViewerBlock, ->base.t[0]))))

void item_viewer_update_rotation(void) {
    extern s8 g_ViewerZoom_s8 __asm__("g_ViewerZoom");
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
    long SquareRoot12(long a);
    long ratan2(long y, long x);
    s32 t;
    s32 x;
    s32 y;
    s32 z;

    SCRATCH->mat180_t0 = VIEWER_FIELD(->base.t[0], s32);
    SCRATCH->mat180_t1 = g_ViewerTransY;
    SCRATCH->mat180_t2 = g_ViewerTransZ;
    RotMatrix((SVECTOR *)(&g_ViewerZoom_s8 - 0x100), LITP(MATRIX, sp_mat180));
    MulMatrix2(LITP(MATRIX, sp_mat180), (MATRIX *)(&g_ViewerZoom_s8 - 0x14));
    x = g_ViewerBaseM02;
    t = x * x;
    y = g_ViewerBaseM12;
    t += y * y;
    z = g_ViewerBaseM22;
    t += z * z;
    t = (x << 0xC) / (t / 4096);
    VIEWER_FIELD(->view_rot.vx, s16) = ratan2(-y, z);
    g_ViewerRotY = ratan2(t, SquareRoot12(0x1000 - t * t / 4096));
    g_ViewerRotZ = 0;
    SCRATCH->mat180_t0 = 0;
    SCRATCH->mat180_t1 = 0;
    SCRATCH->mat180_t2 = 0;
    RotMatrix((SVECTOR *)(&g_ViewerZoom_s8 - 0xE8), LITP(MATRIX, sp_mat180));
    TransposeMatrix(LITP(MATRIX, sp_mat180), LITP(MATRIX, sp_mat1A0));
    SCRATCH->svec1D0_x = VIEWER_FIELD(->base.m[0][0], u16);
    SCRATCH->svec1D0_y = g_ViewerBaseM10;
    SCRATCH->svec1D0_z = g_ViewerBaseM20;
    ApplyMatrix(LITP(MATRIX, sp_mat1A0), LITP(SVECTOR, sp_svec1D0), LITP(VECTOR, sp_vec1C0));
    g_ViewerRotZ = ratan2(SCRATCH->vec1C0_y, SCRATCH->vec1C0_x);
}

void item_viewer_draw_arrows(void) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 i;
    s32 n;
    s32 t;
    u32 mask;
    u16 *xs;

    SCRATCH->spr_flags = 0x01000040;
    mask = 0x8000;
    n = 4;
    xs = (u16 *)g_ViewerArrowX;
    SCRATCH->spr_tpage = 0x1C;
    SCRATCH->spr_v = 0x7C;
    SCRATCH->spr_w = 9;
    SCRATCH->spr_h = 9;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_clut_y = 0x1E4;
    do {
        n--;
        i = n & 0xFF;
        t = i * 2;
        SCRATCH->spr_x = xs[i];
        SCRATCH->spr_y = *(u16 *)(g_ViewerArrowY + t);
        SCRATCH->spr_u = 0x80;
        if (!(mask & g_PadHeldRaw)) {
            SCRATCH->spr_u = 0x8C;
        }
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
        SCRATCH->spr_v += 0xF4;
        mask >>= 1;
    } while (i != 0);
}

s32 item_viewer_check_examine(void) {
    s32 set_message_display(u8, s16);
    extern u8 g_ItemExamineMessages_g __asm__("g_ItemExamineMessages");
    s32 item_viewer_check_angle_l() __asm__("item_viewer_check_angle");
    u8 *b;
    s32 item;
    s32 id;
    u8 r;
    u8 e;
    u8 id8;
    u8 *pos;

    b = &g_MenuWork.item_id;
    item = *b;
    id8 = *((u8 *)&g_ItemExamineIdx + ((item - 1) << 2));
    if (!(id8 & 0x80)) {
        id = id8;
        e = g_ItemExamineTypes[id];
        if (e & 0xF0) {
            g_CurrentMenuFramesDataPtr = (e & 0xF) * 0xC + &g_ItemExamineCombos;
            e >>= 4;
            pos = b + 0x49;
            do {
                do {
                    r = item_viewer_check_angle_l(SVEC(pos)->vx, 1);
                    e--;
                } while (0);
                r |= item_viewer_check_angle_l(SVEC(pos)->vy, 2);
                r |= item_viewer_check_angle_l(SVEC(pos)->vz, 4);
            } while (e != 0 && r != 7);
            if (r != 7) {
                return 0;
            }
            switch (pos[-0x49]) {
            case 0x3E:
                if (*(s16 *)(pos - 6) != 0) {
                    break;
                }
                /* fallthrough */
            case 0x3F:
            case 0x40:
                g_MenuWork.viewer_action = 1;
                g_MenuWork.timer = 0;
                return 1;
            }
            g_MenuWork.viewer_action = 2;
            Flg_on(ADDR_g_ItemExaminedFlags, id8);
            set_message_display((&g_ItemExamineMessages_g)[id8], 0);
            return 1;
        }
        g_MenuWork.viewer_action = 2;
        Flg_on(ADDR_g_ItemExaminedFlags, id);
        set_message_display((&g_ItemExamineMessages_g)[id], 0);
        return 1;
    }
    if (item < 10) {
        if (item < 7 && item != 5) {
            return 0;
        }
    } else if (item == 0x13) {
        goto snd19;
    } else {
        return 0;
    }
    if (*(u8 *)((((u8)((g_MenuWork.cursor >> 1) - 4)) * 2) + g_ItemSlotsPointer + 1) != 0) {
        g_MenuWork.viewer_action = 2;
        g_SelectedItemId = g_MenuWork.item_id + 9;
        set_message_display(0xEF, 0);
        return 1;
    }
    return 0;
snd19:
    if (Flg_ck(ADDR_g_ScenarioFlags, 0x13) != 0 || Flg_ck(ADDR_g_ScenarioFlags, 0) != 0) {
        g_MenuWork.viewer_action = 2;
        set_message_display(0xF0, 0);
        return 1;
    }
    return 0;
}

s32 item_viewer_check_angle(s16 angle, u8 ret) {
    s16 **pp;
    s16 **pq;
    s16 *p;
    s16 a;
    u16 t;
    u16 b;
    s32 d;

    p = (s16 *)g_CurrentMenuFramesDataPtr;
    a = p[0];
    if (a != 0) {
        /* do/while(0) + pointer-to-global: forces the reload of g_CurrentMenuFramesDataPtr like the original */
        do {
            b = p[1];
            pp = (s16 **)&g_CurrentMenuFramesDataPtr;
            *pp += 2;
        } while (0);
        t = (angle + a) & 0xFFF; /* narrowed to 16 bits: adds the HImode copy of a directly */
        d = (t - (s16) b) & 0xFFF;
        if (a * 2 < d) {
            return 0;
        }
        return ret;
    }
    /* a second pointer variable (pseudo numbering decides the allocation tie) */
    pq = (s16 **)&g_CurrentMenuFramesDataPtr;
    *pq += 2;
    return ret;
}
