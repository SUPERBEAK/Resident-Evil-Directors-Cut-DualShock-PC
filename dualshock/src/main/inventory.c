/* inventory.c - the inventory (starting items, slots, item pickup / return) and the item box screen */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void memclr(u32 *p, s32 n);

void load_file(s32 a, s32 b);

typedef struct {
    u8 a;
    u8 b;
} InvSlotPair;

void set_initial_items(void) {
    extern u8 g_RebeccaItemSlots_0_Qty __asm__("g_RebeccaItemSlots+1");  /* -O2: [0] quantity as its own symbol */
    /* (item id, count) pairs ending in 0 */
    s8 a[5] = { 0x01, 0x00, 0x41, 0x01, 0x00 }; /* Chris: knife, lighter */
    s8 b[7] = { 0x01, 0x00, 0x02, 0x0F, 0x41, 0x01, 0x00 }; /* Jill: knife, Beretta with 15 rounds, lockpick */
    s32 flag;
    u8 *p;
    u8 i;
    u8 max;

    flag = g_StatusFlags & STATUS_ADVANCED;
    if (flag != 0) {
        b[2] = 4;
    }
    g_RoomItemsFlags[0] = 0xBFFFFFFF;
    g_RoomItemsFlags[1] = -1;
    g_RoomItemsFlags[2] = -1;
    g_RoomItemsFlags[3] = -1;
    g_RoomItemsFlags[4] = -1;
    g_RoomItemsFlags[6] = -1;
    g_RoomItemsFlags[7] = -1;
    g_PickupQtyB = 0xF0;
    g_PickupQtyC = 0xF0;
    g_RoomItemsFlags[5] = 0xFFF7FFFF;
    g_PickupQtyA = 7;
    if (g_PlayerEntity.id & 3) {
        p = (u8 *)b;
        max = 8;
    } else {
        p = (u8 *)a;
        max = 6;
        g_RebeccaItemSlots = (flag != 0) ? 4 : 2;
        g_RebeccaItemSlots_0_Qty = 0xF;
    }
    i = 0;
    while (*p != 0) {
        LITP(InvSlotPair, g_InventorySlots)[i].a = *p++;
        LITP(InvSlotPair, g_InventorySlots)[i].b = *p++;
        g_ItemSlotIconIndex[i] = i;
        i++;
    }
    g_ItemIconRowMask = (1 << i) - 1;
    g_TotalHeldItems = i;
    for (; i < max; i++) {
        LITP(InvSlotPair, g_InventorySlots)[i].a = 0;
        LITP(InvSlotPair, g_InventorySlots)[i].b = 0;
    }
}

void inventory_remove_slot(u8 *p) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    u8 n;

    n = ((u32)(g_ItemSlotsPointer_u8p + g_TotalHeldItems * 2 - p) >> 1) - 1;
    for (; n != 0; n--) {
        p[0] = p[2];
        p[1] = p[3];
        p += 2;
    }
    p[0] = 0;
    p[1] = 0;
}

void rearrange_item_slots(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    u8 sel;
    u8 n;
    u8 total;
    u8 j;
    u8 k;
    u8 i;

    sel = g_EquippedSlot - 1;
    n = 8;
    if ((g_PlayerEntity.id & 3) != 1) {
        n = 6;
    }
    total = n;
    j = 0;
    k = 0;
    do {
        i = j;
        if (g_ItemSlotsPointer_u8p[i * 2] != 0) {
            if (i != k) {
                g_ItemSlotsPointer_u8p[k * 2] = g_ItemSlotsPointer_u8p[i * 2];
                g_ItemSlotsPointer_u8p[k * 2 + 1] = g_ItemSlotsPointer_u8p[i * 2 + 1];
                g_ItemSlotIconIndex[k] = g_ItemSlotIconIndex[i];
                if (i == sel) {
                    sel = k;
                }
            }
            k++;
        } else {
            if (i < LIT(u8, g_TotalHeldItems)) {
                g_ItemIconRowMask &= ~(1 << g_ItemSlotIconIndex[i]);
            }
        }
        j++;
    } while (--n);
    total -= k;
    g_EquippedSlot = sel + 1;
    g_TotalHeldItems = k;
    while (total) {
        i = k;
        g_ItemSlotsPointer_u8p[i * 2] = 0;
        g_ItemSlotsPointer_u8p[i * 2 + 1] = 0;
        k++;
        total--;
    }
}

void count_held_items(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    u8 max;
    u8 n;

    max = 8;
    if ((g_PlayerEntity.id & 3) != 1) {
        max = 6;
    }
    for (n = 0; g_ItemSlotsPointer_u8p[n * 2] != 0; n++) {
        if (n >= max) {
            break;
        }
    }
    g_TotalHeldItems = n;
}

void load_held_items_images(s32 img_buf) {
    s32 load_file_p() __asm__("load_file");
    s32 count_held_items_p() __asm__("count_held_items");
    s32 item_icon_upload();
    extern u8 g_ItemImageTable_u8[] __asm__("g_ItemImageTable");
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    u8 i;
    s32 j;

    load_file_p(0xE, img_buf);
    count_held_items_p();
    i = g_TotalHeldItems;
    g_ItemIconRowMask = (1 << i) - 1;
    if (i != 0) {
        i--;
    loop:
        do {
            j = i;
            g_ItemSlotIconIndex[j] = i;
            item_icon_upload(g_ItemImageTable_u8[({ g_ItemSlotsPointer_u8p[j * 2] - 1; }) * 4] - 1, j, img_buf);
        } while (0);
        i--;
        if (j != 0) {
            goto loop;
        }
    }
}

u32 get_item_slot(s32 item_id) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    
    u32 i;

    for (i = 0; i < g_TotalHeldItems; i++) {
        if (item_id == g_ItemSlotsPointer_u8p[i * 2]) {
            g_FoundItemSlot = &g_ItemSlotsPointer_u8p[i * 2];
            return i;
        }
    }
    g_FoundItemSlot = LITP(u8, g_DefaultItemSlot);
    return -1;
}

void room_event_item_pickup(void) {
    void Flg_off(void *, u8);
    s32 item_icon_upload();
    extern u8 * g_ItemModelTable_u8p[] __asm__("g_ItemModelTable");
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    extern u8 g_ItemIconRowMask_u8 __asm__("g_ItemIconRowMask");
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    u8 *s;
    u8 *p;
    s32 amt;
    u8 id;
    u8 n;
    u8 i;
    u8 bit;
    u8 limit;
    u32 t;

    s = g_pRoomActionEntry;
    ROOM_ACTION(s)->type = 0;
    *g_ItemModelTable_u8p[((u8 *) ROOM_ACTION(s)->rec)[0xA]] = 0;
    if (MODEL_OBJ(g_ItemModelTable_u8p[((u8 *) ROOM_ACTION(s)->rec)[0xA]])->billboard != 0) {
        LIT(u8, g_FreeEffectSlots)++;
        memclr((u32 *)(&LITP(Effect, g_EffectPool)[MODEL_OBJ(g_ItemModelTable_u8p[((u8 *) ROOM_ACTION(s)->rec)[0xA]])->billboard]), 0x21);
    }
    Flg_off(LITP(void, g_RoomItemsFlags), ((u8 *) ROOM_ACTION(s)->rec)[0x14]);
    g_PickedItemId = ((u8 *) ROOM_ACTION(s)->rec)[8];
    p = ((u8 *) ROOM_ACTION(s)->rec);
    amt = p[9];
    id = p[8];
    if (g_SelectedItemId == 0x2F) {
        amt = 2;
        if (g_StatusFlags & STATUS_ADVANCED) {
            amt = 3;
        }
    }
    if ((g_StatusFlags & STATUS_TRAINING) || (g_StatusFlags & STATUS_ADVANCED_HOLD)) {
        if (LIT(volatile u8, g_SelectedItemId) - 11 < 8U || LIT(u8, g_SelectedItemId) == 0x2F) {
            amt = (u8)amt * 2;
        }
    }
    n = 8;
    if (LIT(volatile u8, g_SelectedItemId) - 11 < 8U || LIT(u8, g_SelectedItemId) == 0x2F) {
        if ((g_PlayerEntity.id & 3) != 1) {
            n = 6;
        }
        i = 0;
        limit = n;
        do {
            p = &g_ItemSlotsPointer_u8p[i * 2];
            if (INV_SLOT(p)->item_id == id) {
                t = (u8)amt + INV_SLOT(p)->count;
                if (t < 251) {
                    INV_SLOT(p)->count = t;
                    return;
                }
                if (LIT(u8, g_TotalHeldItems) < limit) {
                    INV_SLOT(p)->count = 250;
                    amt = t + 6;
                    break;
                }
            }
            i++;
        } while (--n);
    }
    g_ItemSlotsPointer_u8p[LIT(u8, g_TotalHeldItems) * 2] = id;
    g_ItemSlotsPointer_u8p[LIT(u8, g_TotalHeldItems) * 2 + 1] = amt;
    bit = 0;
    while ((g_ItemIconRowMask_u8 >> bit) & 1) {
        bit++;
    }
    do {
        t = 1;
        g_ItemSlotIconIndex[LIT(u8, g_TotalHeldItems)] = bit;
    } while (0);
    g_ItemIconRowMask_u8 |= t << bit;
    item_icon_upload(((u8 *) ROOM_ACTION(g_pRoomActionEntry)->rec)[0xA], bit, ROOM_P->raw_images, amt);
    LIT(u8, g_TotalHeldItems)++;
}

void room_event_item_return(void) {
    s32 inventory_remove_slot_a1() __asm__("inventory_remove_slot");
    s32 Flg_on();
    extern s8 g_ItemModelTable_s8 __asm__("g_ItemModelTable");
    void *act;

    act = g_pRoomActionEntry;
    ROOM_ACTION(act)->type = 0;
    ITEM_REC->item_count = (u8) FOUND_SLOT->count;
    **(s8 **)(&g_ItemModelTable_s8 + (ITEM_REC->model * 4)) = 1;
    Flg_on(ADDR_g_RoomItemsFlags, ITEM_REC->flag);
    g_UsedItemId = ITEM_REC->item_id;
    FOUND_SLOT->item_id = 0;
    FOUND_SLOT->count = 0U;
    inventory_remove_slot_a1(g_FoundItemSlot);
    LIT(s8, g_TotalHeldItems) -= 1;
}

typedef struct {
    s8 b4;
    u8 b5;
} ItemboxScrollView;

s32 itembox_update(void) {
    extern u8 g_ItemImageTable_l[] __asm__("g_ItemImageTable");
    extern s16 g_ScreenOffsetY_l __asm__("g_ScreenOffsetY");
    extern ItemboxScrollView g_ItemboxScrollDir_l __asm__("g_ItemboxScrollDir");
    extern u8 g_ItemboxScroll_l __asm__("g_ItemboxScroll");
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    extern u8 g_ItemIconRowMask_l __asm__("g_ItemIconRowMask");
    void draw_item_name_l(u8, s32, s32, s32) __asm__("draw_item_name");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    void menu_draw_cursor(void);
    void menu_update_selected_item(void);
    void item_icon_upload_l(s32, u8, s32) __asm__("item_icon_upload");
    void itembox_icon_upload_l(s32, s32) __asm__("itembox_icon_upload");
    u8 k;
    u8 a3v;
    u8 a2v;
    s32 az;
    s32 n;
    u8 *st;
    u8 *bb1;
    u8 *bb2;
    u8 *bb3;
    u8 *b0;
    u8 *sw;
    u8 *bc1;
    u8 *bc2;
    u8 *tbl;
    u16 *pad;

    if (g_PadTrigger & PSX_START) {
        goto cancel;
    }
    st = &g_MenuWork.sub_state;
    switch (*st) {
    case 0:
        *st = 1;
        g_ItemboxScroll_l = 0;
        g_MenuCursorBlinkTimer = 0;
        menu_update_selected_item();
    case 1:
        if (g_PadPressed & PAD_CANCEL) {
        cancel:
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            return 1;
        }
        if (g_PadPressed & PAD_CONFIRM) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            if (!(g_MenuWork.cursor & 0xF8)) {
                return 1;
            }
            g_MenuWork.sub_state = 2;
            g_MenuWork.submenu_state = 0;
            g_MenuCursorBlinkTimer = 0;
            g_MenuSubTimer[0] = 0xF;
            g_MenuSubTimer[1] = 0;
            g_ItemMoveAnim = 0;
            break;
        }
        if (g_PadTrigger & PSX_DPAD) {
            if (g_PadTrigger & (PSX_RIGHT | PSX_LEFT)) {
                bb1 = &g_MenuWork.cursor;
                if (*bb1 & 0xF8) {
                    *bb1 ^= 2;
                }
            }
            if (g_PadTrigger & PSX_UP) {
                bb2 = &g_MenuWork.cursor;
                *bb2 -= 4;
                if (!(*bb2 & 0xFC)) {
                    *bb2 &= 2;
                    *bb2 |= (g_MenuWork.slot_count + 2) * 2;
                }
            } else if (g_PadTrigger & PSX_DOWN) {
                bb3 = &g_MenuWork.cursor;
                *bb3 += 4;
                if ((g_MenuWork.slot_count + 3) * 2 < *bb3) {
                    *bb3 = (*bb3 & 2) | 4;
                }
            }
            menu_update_selected_item();
            g_MenuCursorBlinkTimer = 0;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
        } else {
            b0 = &g_MenuCursorBlinkTimer;
            (*b0)--;
        }
        break;
    case 2:
        if (g_PadPressed & PAD_CANCEL) {
            *st = 1;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            break;
        }
        if (g_PadPressed & PAD_CONFIRM) {
            tbl = LITP(u8, g_ItemBoxSlots);
            if ((g_MenuWork.item_id | tbl[g_ItemboxScroll_l * 2]) == 0) {
                break;
            }
            *st = 1;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            k = (g_MenuWork.cursor >> 1) - 4;
            if (LIT(u8, g_EquippedSlot) - 1 == k) {
                LIT(u8, g_EquippedSlot) = 0;
            }
            do {
                sw = &tbl[g_ItemboxScroll_l * 2];
                a3v = *sw;
                a2v = sw[1];
                *sw = g_ItemSlotsPointer_l[k * 2];
                LITP(u8, g_ItemBoxSlots)[g_ItemboxScroll_l * 2 + 1] = g_ItemSlotsPointer_l[k * 2 + 1];
                g_ItemSlotsPointer_l[k * 2] = a3v;
                g_ItemSlotsPointer_l[k * 2 + 1] = a2v;
            } while (0);
            if (k < g_TotalHeldItems) {
                az = a3v;
            } else {
                n = 0;
                while ((g_ItemIconRowMask_l >> n) & 1) {
                    n++;
                }
                g_ItemSlotIconIndex[k] = n;
                g_ItemIconRowMask_l |= 1 << n;
                do {
                    az = a3v;
                } while (0);
            }
            if (az != 0) {
                item_icon_upload_l(g_ItemImageTable_l[({ az - 1; }) * 4] - 1, g_ItemSlotIconIndex[k], ADDR_g_DoorFile);
            }
            rearrange_item_slots();
            menu_update_selected_item();
            bc1 = &g_ItemboxScroll_l;
            if (LITP(u8, g_ItemBoxSlots)[*bc1 * 2] != 0) {
                itembox_icon_upload_l(g_ItemImageTable_l[({ LITP(u8, g_ItemBoxSlots)[*bc1 * 2] - 1; }) * 4] - 1, *bc1 & 7);
            }
            break;
        }
        pad = LITP(u16, g_PadHeldRaw);
        if (!(g_PadTrigger & (PSX_UP | PSX_DOWN)) && g_ItemMoveAnim == 0) {
            break;
        }
        if (!(*pad & 0x5000)) {
            g_MenuSubTimer[0] = 0xF;
            g_MenuSubTimer[1] = 0;
            break;
        }
        *st = 3;
        g_ItemMoveAnim = 1;
        play_sfx_l(2, 0x21, 0);
        if (*pad & 0x1000) {
            g_ItemboxScrollDir_l.b5 = 0xF;
            g_ItemboxScrollDir_l.b4 = -1;
            if (g_ItemboxScroll_l != 0) {
                g_ItemboxScroll_l--;
            } else {
                g_ItemboxScroll_l = 0x2F;
            }
            bc2 = &g_ItemboxScroll_l;
            if (LITP(u8, g_ItemBoxSlots)[*bc2 * 2] != 0) {
                itembox_icon_upload_l(g_ItemImageTable_l[({ LITP(u8, g_ItemBoxSlots)[*bc2 * 2] - 1; }) * 4] - 1, *bc2 & 7);
            }
        } else {
            g_ItemboxScrollDir_l.b5 = 0;
            g_ItemboxScrollDir_l.b4 = 1;
            n = g_ItemboxScroll_l + 1;
            if ((u8)n == 0x30) {
                n = 0;
            }
            if (LITP(u8, g_ItemBoxSlots)[(u8)n * 2] != 0) {
                itembox_icon_upload_l(g_ItemImageTable_l[({ LITP(u8, g_ItemBoxSlots)[(u8)n * 2] - 1; }) * 4] - 1, n & 7);
            }
        }
        if ((g_PadHeldRaw >> 8) & g_MenuSubTimer[1]) {
            if (g_MenuSubTimer[0] == 0) {
                if (g_ItemboxScrollDir > 0) {
                    g_ItemboxScrollDir = 3;
                } else {
                    g_ItemboxScrollDir = -3;
                }
            }
        } else {
            goto reset;
        }
    case 3:
        if ((g_PadHeldRaw >> 8) & g_MenuSubTimer[1]) {
            if (g_MenuSubTimer[0] != 0) {
                g_MenuSubTimer[0]--;
            }
        } else {
        reset:
            g_MenuSubTimer[0] = 0xF;
            if (g_PadHeldRaw & (PSX_UP | PSX_DOWN)) {
                if (g_PadHeldRaw & PSX_UP) {
                    g_MenuSubTimer[1] = 0x10;
                } else {
                    g_MenuSubTimer[1] = 0x40;
                }
            } else {
                g_MenuSubTimer[1] = 0;
            }
        }
        g_ItemboxScrollDir_l.b5 += g_ItemboxScrollDir_l.b4;
        if (g_ItemboxScrollDir_l.b5 == 0xF) {
            g_ItemboxScrollDir_l.b5 = 0;
        }
        if (g_ItemboxScrollDir_l.b5 == 0) {
            g_MenuWork.sub_state = 2;
            if (g_ItemboxScrollDir_l.b4 > 0) {
                if (g_ItemboxScroll_l != 0x2F) {
                    g_ItemboxScroll_l++;
                } else {
                    g_ItemboxScroll_l = 0;
                }
            }
            g_ItemboxScrollDir_l.b4 = 0;
        }
        break;
    }
    menu_draw_cursor();
    draw_item_name_l(g_MenuWork.item_id, 0, 0x30 - g_ScreenOffsetX, 0xBA - g_ScreenOffsetY_l);
    return 0;
}

void itembox_load_assets(void) {
    s32 load_file_c2(s32, s32) __asm__("load_file");
    s32 task_sleep_a8(s32) __asm__("task_sleep");
    s32 tim_load_to_vram_a1(s32) __asm__("tim_load_to_vram");
    s32 itembox_icon_upload(s32, s32);
    extern u8 g_ItemBoxSlots_u8 __asm__("g_ItemBoxSlots");
    extern s8 g_VramTpageCursor_s8 __asm__("g_VramTpageCursor");
    load_file_c2(0xD, ADDR_g_DoorFile);
    g_VramClutRowCursor = 0x1C;
    g_VramTpageCursor_s8 = 0x15;
    tim_load_to_vram_a1(ADDR_g_DoorFile);
    task_sleep_a8(1);
    load_file_c2(0xE, ADDR_g_DoorFile);
    if (g_ItemBoxSlots_u8 != 0) {
        itembox_icon_upload(*(u8 *)((u8 *)&g_ItemImageTable + (((g_ItemBoxSlots_u8 - 1) << 2))) - 1, 0);
    }
    g_ItemboxScroll = 0;
    g_MenuWork.loaded_res_id = 0;
    g_ItemboxScrollDir = 0;
}

void itembox_compact(void) {
    u8 n;
    u8 cnt;
    u8 i;
    u8 j;
    u8 *tbl;
    u8 *tbl2;
    s32 a;
    s32 b;
    s32 c;

    n = 0x30;
    cnt = 0x30;
    i = 0;
    j = 0;
    do {
        tbl = LITP(u8, g_ItemBoxSlots);
        a = i * 2;
        if (tbl[a] != 0) {
            if (i != j) {
                b = j * 2;
                tbl[b] = tbl[a];
                LITP(u8, g_ItemBoxSlots)[b + 1] = LITP(u8, g_ItemBoxSlots)[a + 1];
            }
            j++;
        }
        i++;
    } while (--n);
    cnt -= j;
    if (cnt) {
        do {
            tbl2 = LITP(u8, g_ItemBoxSlots);
            c = j * 2;
            tbl2[c] = 0;
            LITP(u8, g_ItemBoxSlots)[c + 1] = 0;
            j++;
        } while (--cnt);
    }
}

void itembox_draw(void) {
    extern u8 *g_CurrentMenuFramesDataPtr_a[] __asm__("g_CurrentMenuFramesDataPtr");
    extern u8 g_ItemboxScroll_l __asm__("g_ItemboxScroll");
    extern u8 g_ItemboxScroll_a[] __asm__("g_ItemboxScroll");
    u8 n;
    u8 i;
    u8 item;
    s32 i2;
    s16 voff;
    s32 y;
    s16 ys;
    s32 t;
    s32 base;
    s32 k;
    s32 c31;

    SCRATCH->spr_flags = 0x01000040;
    SCRATCH->spr_clut_y = 0x1FC;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_tpage = 0x15;
    g_CurrentMenuFramesDataPtr = g_ItemboxFrameRecs;
    do {
        menu_pop_sprite_record();
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
    } while (g_CurrentMenuFramesDataPtr > g_ItemboxFrameRecs - 0x3C);

    SCRATCH->rect_flags = 0;
    SCRATCH->rect_r = 0;
    SCRATCH->rect_g = 0;
    SCRATCH->rect_b = 0;
    g_CurrentMenuFramesDataPtr_a[0] = g_ItemViewerActionFuncs;
    do {
        SCRATCH->rect_h = ((u16 *)g_CurrentMenuFramesDataPtr)[-1];
        SCRATCH->rect_w = ((u16 *)g_CurrentMenuFramesDataPtr)[-2];
        SCRATCH->rect_y = ((u16 *)g_CurrentMenuFramesDataPtr)[-3];
        SCRATCH->rect_x = ((u16 *)g_CurrentMenuFramesDataPtr)[-4];
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
    } while ((g_CurrentMenuFramesDataPtr_a[0] -= 8) > g_ItemboxFrameRecs2);

    SCRATCH->rect_flags = 0x60000000;
    SCRATCH->rect_r = 0x30;
    SCRATCH->rect_g = 0x30;
    SCRATCH->rect_b = 0x30;
    SCRATCH->rect_x = 0x2A;
    SCRATCH->rect_y = 0x22;
    SCRATCH->rect_w = 0x7E;
    SCRATCH->rect_h = 0xF;
    GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
    SCRATCH->rect_y = 0x41;
    GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
    if (g_MenuWork.sub_state < 2) {
        SCRATCH->rect_y = 0x22;
        SCRATCH->rect_h = 0x2E;
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
        SCRATCH->rect_x = 0x5C;
        SCRATCH->rect_y = 0x58;
        SCRATCH->rect_w = 0x28;
        SCRATCH->rect_h = 0x1E;
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
    }

    n = 1;
    if (g_MenuLoadedResourceId != 0) {
        n = 2;
    }
    t = g_MenuLoadedResourceId * 2;
    y = 0x58 - t;
    voff = t;
    SCRATCH->spr_y = 0x58;
    i = g_ItemboxScroll_l;
    SCRATCH->spr_h = 0x1E - t;
    do {
        SCRATCH->spr_x = 0x5C;
        SCRATCH->spr_w = 0x28;
        SCRATCH->spr_clut_y = 0x1E6;
        i2 = i * 2;
        item = LITP(u8, g_ItemBoxSlots)[i2];
        if (item != 0) {
            SCRATCH->spr_tpage = 0x15;
            SCRATCH->spr_u = (i & 1) * 40;
            SCRATCH->spr_v = voff + (((i << 4) & 0x60) + ({ -0x80; }));
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 5);
            SCRATCH->spr_y = y;
            SCRATCH->spr_tpage = 0x1C;
            display_item_qty(item, (LITP(u8, g_ItemBoxSlots) + 1)[i2], 4);  /* the slot's quantity byte */
        } else {
            SCRATCH->spr_u = 0x90;
            SCRATCH->spr_tpage = 0x1C;
            SCRATCH->spr_v = voff - 0x42;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 5);
        }
        do {
            y += 0x1E;
        } while (0);
        SCRATCH->spr_h = 0x1E;
        SCRATCH->spr_y = y;
        voff = 0;
        if (i != 0x2F) {
            i++;
        } else {
            i = 0;
        }
    } while (--n != 0);

    n = 3;
    if (g_MenuLoadedResourceId != 0) {
        n = 4;
    }
    if (g_ItemboxScroll_l != 0) {
        i = g_ItemboxScroll_l - 1;
    } else {
        i = 0x2F;
    }
    ys = 0x23 - g_MenuLoadedResourceId;
    sprintf(LITP(char, sp_text), "_Nothing_");
    do {
        item = LITP(u8, g_ItemBoxSlots)[i * 2];
        if (item != 0) {
            draw_item_name(item, 0x50, 0x2A, ys);
        } else {
            print_text_8x14(0x2A, ys, 0x53, 0);
        }
        ys += 0xF;
        if (i != 0x2F) {
            i++;
        } else {
            i = 0;
        }
    } while (--n != 0);

    if ((u32)(g_ItemboxScroll_a[0] - 1) >= 0x2D) {
        k = g_MenuLoadedResourceId;
        SCRATCH->line_r = 0xFF;
        do {
            SCRATCH->line_g = 0xEF;
            SCRATCH->line_x0 = 0x2A;
            SCRATCH->line_x1 = 0xA8;
            c31 = 0x31;
            SCRATCH->line_flags = 0;
        } while (0);
        SCRATCH->line_b = 0;
        do {
            base = c31 - k;
        } while (0);
        y = (s16)base;
        if (g_ItemboxScroll_a[0] != 0) {
            y = base + (0x30 - g_ItemboxScroll_a[0]) * 15;
        }
        SCRATCH->line_y0 = y;
        SCRATCH->line_y1 = y;
        GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 5);
    }

    n = 3;
    if (g_ItemboxScroll_a[0] != 0) {
        i = g_ItemboxScroll_a[0] - 1;
    } else {
        i = 0x2F;
    }
    if (g_ItemboxScrollDir < 0) {
        i++;
    }
    SCRATCH->spr_flags = 0x01000040;
    SCRATCH->spr_x = 0xB0;
    SCRATCH->spr_u = 0x40;
    SCRATCH->spr_v = 0x38;
    SCRATCH->spr_w = 6;
    SCRATCH->spr_h = 1;
    SCRATCH->spr_clut_y = 0x1FC;
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_tpage = 0x15;
    do {
        if (i >= 0x30) {
            i = 0;
        }
        SCRATCH->spr_y = i + 0x21;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 5);
        i++;
        n--;
    } while (n != 0);

    g_CurrentMenuFramesDataPtr = g_ItemboxFrameRecs2;
    do {
        menu_pop_sprite_record();
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 5);
    } while (g_CurrentMenuFramesDataPtr > g_ItemboxFrameRecs2 - 0x24);
}
