/* texture.c - TMD texture relocation, TIM uploads to VRAM and item icons. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


s32 tmd_relocate_texture(u8 mode, u32 *tmd, s32 tpage_ofs, s32 clut_y_ofs) {
    s32 GsMapModelingData(void *);
    typedef struct {
        u32 *vert_top;
        u32 n_vert;
        u32 *normal_top;
        u32 n_normal;
        u32 *primitive_top;
        u32 n_primitive;
        s32 scale;
    } TmdObjectView;
    s32 ret;
    TmdObjectView *obj;
    u32 nobj;
    u32 i;
    u32 j;
    u32 *prim;
    u32 hdr;
    u32 len;
    u32 nprim;

    ret = tmd[0];
    if (tmd[1] == 0) {
        GsMapModelingData(tmd + 1);
    }
    nobj = tmd[2];
    tmd += 3;
    for (i = 0; i < nobj; i++, tmd += 7) {
        prim = (u32 *)tmd[4];
        nprim = tmd[5];
        for (j = 0; j < nprim; j++) {
            hdr = *prim;
            len = hdr >> 8;
            if (hdr & 0x04000000) {
                switch (mode) {
                case 2:
                    prim[2] += tpage_ofs << 16;
                case 1:
                    prim[1] += clut_y_ofs << 22;
                    break;
                case 0:
                    prim[2] += tpage_ofs << 16;
                    break;
                }
            }
            prim += (len & 0xFF) + 1;
        }
    }
    return ret;
}

void tmd_link_object(void *tmd, void *dobj, s32 coord) {
    s32 GsLinkObject5(void *, void *, s32);
    s32 GsMapModelingData(void *);
    s32 GsPresetObject(void *, s32);
    if (((s32) TMD_HDR(tmd)->flags) == 0) {
        GsMapModelingData(&TMD_HDR(tmd)->flags);
    }
    GsLinkObject5(&TMD_HDR(tmd)->obj0_vert_top, dobj, 0);
    DOBJ5(dobj)->coord2 = (GsCOORDINATE2 *)coord;
    DOBJ5(dobj)->attribute = 0;
    LIT(s32, g_LoadPointer) = GsPresetObject(dobj, LIT(s32, g_LoadPointer));
}

void tim_load_to_vram(s32 tim_data) {
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    void GsGetTimInfo(u_long *im, GsIMAGE *tim);
    RECT rect;
    GsIMAGE img;
    s32 x;
    s32 w;

    GsGetTimInfo((u_long *)(tim_data + 4), &img);
    x = LIT(u8, g_VramTpageCursor) << 6;
    if (LIT(u8, g_VramTpageCursor) >= 0x10) {
        x -= 0x400;
    }
    rect.x = x;
    rect.y = (LIT(u8, g_VramTpageCursor) >= 0x10) << 8;
    rect.w = img.pw;
    rect.h = img.ph;
    LoadImage(&rect, img.pixel);
    w = img.pw + 0x3F;
    if (w < 0) {
        w = img.pw + 0x7E;
    }
    LIT(u8, g_VramTpageCursor) += w >> 6;
    DrawSync(0);
    if ((img.pmode >> 3) & 1) {
        rect.x = img.cx;
        rect.y = LIT(u8, g_VramClutRowCursor) + 0x1E0;
        rect.w = img.cw;
        rect.h = img.ch;
        LoadImage(&rect, img.clut);
        LIT(u8, g_VramClutRowCursor) += (u8)img.ch;
        DrawSync(0);
    }
}

void item_icon_upload(s32 icon, s32 slot, s32 icons) {
    int LoadImage(RECT *rect, u_long *p);
    LIT(s16, sp_rect_x) = 0x36C;
    LIT(s16, sp_rect_y) = (slot << 5) + 0x100;
    LIT(s16, sp_rect_w) = 0x14;
    LIT(s16, sp_rect_h) = 0x1E;
    LoadImage(LITP(RECT, sp_rect), (u_long *)(icons + (icon * 0x4B0)));
}

void itembox_icon_upload(s32 icon, u32 slot) {
    int LoadImage(RECT *rect, u_long *p);
    LIT(s16, sp_rect_x) = ((slot & 1) * 0x14) + 0x140;
    LIT(s16, sp_rect_y) = ((slot >> 1) << 5) + 0x180;
    LIT(s16, sp_rect_w) = 0x14;
    LIT(s16, sp_rect_h) = 0x1E;
    LoadImage(LITP(RECT, sp_rect), (u_long *)((icon * 0x4B0) + ADDR_g_DoorFile));
}
