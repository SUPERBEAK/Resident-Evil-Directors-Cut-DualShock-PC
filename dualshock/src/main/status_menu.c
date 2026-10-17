/* status_menu.c - The status menu (main_menu): inventory, health / EKG display, the item submenus (equip, use, check,
   combine) and the tabs; the item use functions. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void cut_set(void);
void load_equipped_weapon_animation(s32 weapon, s32 joint_index, s32 buf, u32 *anim_object);
void fade_update(void);
void set_fading(s8 fade_type, s16 step);
s32 menu_file_state_machine(void);
void file_pickup_init(void);
s32 file_pickup_update(void);
void menu_reset_status_state(void);
void load_file(s32 a, s32 b);
void load_file_blocking(void);
void rearrange_item_slots(void);
s32 itembox_update(void);
void itembox_load_assets(void);
void itembox_draw(void);
s32 item_viewer_update(void);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
u32 *setup_joint_structures(u32 *packet);
void entity_setup_weapon_part(s32 part);
void center_screen_origin(void);
s32 menu_map_state_machine(void);
void menu_init_map_screen(void);
s32 menu_update_map_animation(void);
void menu_init_map_display(void);
void draw_item_name(u8 item_id, u8 attr, u16 screen_x, s16 screen_y);
s32 set_message_display(u8 msg, s16 msg_id);
void task_sleep(s16 frames);
void task_exit(void);
void task_suspend(s32 slot);
void task_resume(s32 slot);
void Flg_on(s32 flags, u32 bit);
void Flg_off(s32 flags, u32 bit);
s32 Flg_ck(s32 *flags, u32 bit);
void sound_load_bank1(s32 bank, u8 *vb_buf);
void play_sfx(u8 type, u8 idx);



/* The status menu task: picks the mode from the requests in g_GameStateFlags (MENU_REQUEST bits; the
   file pickup screen for GAME_FILE_PICKUP), then runs the inventory / item box / pickup / map screens. */
void main_menu(void) {
    void SetGeomOffset(long ofx, long ofy);
    int StoreImage(RECT *rect, u_long *p);
    int ResetGraph(int mode);
    void GsInitGraph2(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
    void GsSetAmbient(long r, long g, long b);
    void GsSetLightMode(int mode);
    int GsSetRefView2(GsRVIEW2 *pv);
    extern MenuWork g_MenuWork_l __asm__("g_MenuWork");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    void task_sleep_l(s32) __asm__("task_sleep");
    void itembox_load_assets_l(u8 *) __asm__("itembox_load_assets");
    void menu_load_item_model_l(u8 *) __asm__("menu_load_item_model");
    s32 Flg_ck_l(s32, s32) __asm__("Flg_ck");
    void set_message_display_l(u16, s32) __asm__("set_message_display");
    void menu_handle_input(void);
    void menu_draw_health_bar(void);
    void menu_exit_cleanup(void);
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    u8 item_viewer_update_l(void) __asm__("item_viewer_update");
    u8 itembox_update_l(void) __asm__("itembox_update");
    void setup_joint_structures_l(s32) __asm__("setup_joint_structures");
    void joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void menu_update_equipped_weapon(void);
    void load_equipped_weapon_animation_l(u8, s32, s32, s32) __asm__("load_equipped_weapon_animation");
    void menu_restore_game_state(void);
    void menu_draw_inventory(void);
    s32 t;
    s32 c;
    s32 v;
    u32 m;

    g_VSyncMode = 0;
    task_suspend(0);
    ResetGraph(1);
    GsInitGraph2(0x140, 0xF0, 4, 0, 0);
    SetGeomOffset(0x70, 0x4C);
    center_screen_origin();
    LITP(RECT, sp_rect)->x = 0x180;
    LITP(RECT, sp_rect)->y = 0x100;
    LITP(RECT, sp_rect)->w = 0x40;
    LITP(RECT, sp_rect)->h = 0x100;
    StoreImage(LITP(RECT, sp_rect), LITP(u_long, g_RoomBgBsBuf));
    g_FadeTypeId = 2;
    g_ClearColorR = 0;
    g_ClearColorG = 0;
    g_ClearColorB = 0;
    g_FadeStep = -0x1800;
    fade_update();
    task_sleep_l(1);
    if (g_GameStateFlags & GAME_FILE_PICKUP) {
        g_MenuWork_l.state = 8;
    } else {
        g_MenuWork_l.mode = MENU_MODE_MAP;
        m = 0x4000;
        if (!(g_GameStateFlags & MENU_REQUEST(MENU_MODE_MAP))) {
            g_MenuWork_l.mode = MENU_MODE_GOT_ITEM;
            if (!(g_GameStateFlags & MENU_REQUEST(MENU_MODE_GOT_ITEM))) {
                do {
                    g_MenuWork_l.mode--;
                } while (g_MenuWork_l.mode != MENU_MODE_STATUS && !((m >> g_MenuWork_l.mode) & LIT(s32, g_GameStateFlags)));
            }
        }
        g_MenuWork_l.state = 0;
        g_MenuWork_l.viewer_state = 0;
        g_MenuWork_l.viewer_action = 0;
        g_MenuWork_l.cursor = 8;
        g_MenuWork_l.loaded_res_id = 0;
        g_MenuWork_l.model_loaded = 0;
        switch (g_MenuWork_l.mode) {
        case MENU_MODE_PICKUP:
        case MENU_MODE_GOT_ITEM:
            g_MenuWork_l.item_id = (((u8 *) ROOM_ACTION(g_pRoomActionEntry)->rec))[8];
            menu_load_item_model_l(&g_MenuWork_l.state);
            break;
        case MENU_MODE_ITEMBOX:
            itembox_load_assets_l(&g_MenuWork_l.state);
            break;
        case MENU_MODE_KEY_USED_UP:
            if (LIT(u8, g_SelectedItemId) >= 0x33) {
                g_MenuWork_l.mode = MENU_MODE_KEY_USED_UP_VIEW;
                g_MenuWork_l.item_id = LIT(u8, g_SelectedItemId);
                menu_load_item_model_l(&g_MenuWork_l.state);
            }
            break;
        }
        c = g_PlayerId & 3;
        t = (c + 1) & 2;
        g_MenuWork_l.char_flags = t;
        g_MenuWork_l.slot_count = t + 6;
        g_MenuWork_l.char_flags = t | (g_PlayerId & 1);
        if (c == 3) {
            g_MenuWork_l.has_radio = 0;
            if (Flg_ck_l(ADDR_g_ScenarioFlags, 0x7F) == 0 && Flg_ck_l(ADDR_g_ScenarioFlags2, 0x38) != 0
                && Flg_ck_l(ADDR_g_ScenarioFlags2, 0x22) == 0) {
                g_MenuWork_l.has_radio = 1;
            }
        } else {
            g_MenuWork_l.has_radio = Flg_ck_l(ADDR_g_ScenarioFlags, 0x7F);
        }
        g_MenuWork_l.action_result = 0;
        g_MenuWork_l.flags = 0;
        GsSetProjection(0xC0);
        g_View.vpx = 0x3A98;
        g_View_Vpy = 0;
        g_View_Vpz = 0;
        g_View_Vrx = 0;
        g_View_Vry = 0;
        g_View_Vrz = 0;
        g_View_Rz = 0;
        g_View_Super = 0;
        GsSetRefView2(&g_View);
        g_ViewerLights[0].vx = 0x64;
        g_ViewerLights[0].vy = 0x50;
        g_ViewerLights[0].vz = 0x50;
        g_ViewerLights[1].vx = -0x64;
        g_ViewerLights[1].vy = 0x50;
        g_ViewerLights[1].vz = 0x50;
        g_ViewerLights[2].vx = 0;
        g_ViewerLights[2].vy = -0x64;
        g_ViewerLights[2].vz = 0x50;
        GsSetAmbient(0x199, 0x199, 0x199);
        GsSetLightMode(0);
    }
    while (1) {
        LITP(GsSPRITE, sp_sprite)->r = 0x80;
        LITP(GsSPRITE, sp_sprite)->g = 0x80;
        LITP(GsSPRITE, sp_sprite)->b = 0x80;
        LITP(GsSPRITE, sp_sprite)->mx = 0;
        LITP(GsSPRITE, sp_sprite)->my = 0;
        LITP(GsSPRITE, sp_sprite)->rotate = 0;
        switch (g_MenuWork_l.state) {
        case 0:
            if (g_FadeLevel < 0) {
                switch (g_MenuWork_l.mode) {
                case MENU_MODE_STATUS:
                    g_MenuWork_l.state = 1;
                    g_MenuWork_l.sub_state = 0;
                    menu_init_map_screen();
                    file_pickup_init();
                    break;
                case MENU_MODE_KEY_USED_UP:
                    g_MenuWork_l.state = 2;
                    set_message_display_l(0xC4, 0);
                    break;
                case MENU_MODE_PICKUP:
                case MENU_MODE_GOT_ITEM:
                    g_MenuWork_l.state = 4;
                    g_MenuWork_l.viewer_state = 0;
                    break;
                case MENU_MODE_ITEMBOX:
                    g_MenuWork_l.state = 3;
                    g_MenuWork_l.sub_state = 0;
                    break;
                case MENU_MODE_MAP:
                    g_MenuWork_l.state = 5;
                    menu_init_map_display();
                    break;
                case MENU_MODE_KEY_USED_UP_VIEW:
                    g_MenuWork_l.state = 6;
                    menu_init_map_screen();
                    file_pickup_init();
                    g_MenuWork_l.viewer_state = 0;
                    break;
                }
                g_MenuWork_l.health_anim_state = 0;
                g_MenuWork_l.face_anim = 0xFF;
            }
            break;
        case 1:
            if ((g_PadPressed & PAD_CANCEL) && g_MenuWork_l.sub_state == 1) {
                goto quit;
            }
            if (!(g_PadHeldRaw & PSX_SELECT) && (g_PadTrigger & PSX_START) && g_MenuWork_l.viewer_action == 0 && g_MenuWork_l.action_result == 0) {
                goto quit;
            }
            menu_handle_input();
            if (g_MenuWork_l.flags & 0x41) {
                goto quit;
            }
            if (g_MenuWork_l.state != 7) {
                menu_draw_health_bar();
            }
            if (!(g_MenuWork_l.flags & 0x41)) {
                break;
            }
        quit:
            menu_exit_cleanup();
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
            g_MessageStatus_l = 0;
            break;
        case 2:
            if (g_MessageStatus_l & 0x80) {
                menu_draw_health_bar();
            } else {
                menu_exit_cleanup();
            }
            break;
        case 4:
            if (item_viewer_update_l() != 0) {
                menu_exit_cleanup();
            } else {
                menu_draw_health_bar();
            }
            break;
        case 3:
            if (itembox_update_l() != 0) {
                menu_exit_cleanup();
            } else {
                menu_draw_health_bar();
            }
            break;
        case 5:
            if (menu_update_map_animation() != 0) {
                menu_exit_cleanup();
            } else {
                menu_draw_health_bar();
            }
            break;
        case 6:
            if (item_viewer_update_l() != 0) {
                g_MenuWork_l.state = 1;
                g_MenuWork_l.mode = MENU_MODE_STATUS;
                g_MenuWork_l.sub_state = 0;
            }
            menu_draw_health_bar();
            break;
        case 7:
            if (g_MenuWork_l.model_loaded & 0x80) {
                g_CurrentEntity = (Entity *)ADDR_g_PlayerEntity;
                setup_joint_structures_l(ADDR_g_PlayerObjPacketBuf);
            }
            g_PlayerEntity.blend_counter = 0;
            g_PlayerEntity.animation_frame_id = 0;
            g_PlayerEntity.timing_control = 0;
            g_PlayerEntity.hit_state = 0;
            if ((g_MenuWork_l.flags & 0x80) && (v = g_UsedItemId) < 0x1F && v >= ({ 0x1D; })) {
                g_PlayerState = 0x10008;
                g_PlayerAnimationId = 0x37;
                g_PlayerScdEntityFlags = 0x20;
                g_PlayerScdAnimParam = 0x20;
                joint_move_l(0, g_PlayerWork.action_anim_header, g_PlayerWork.action_anim_base, 0x400);
            } else {
                g_PlayerState = 1;
                g_PlayerEntity.animation_id = 0;
                joint_move_l(0, g_PlayerEntity.anim_header, g_PlayerEntity.anim_base, 0x400);
            }
            switch (g_MenuWork_l.mode) {
            case MENU_MODE_STATUS:
                if (g_MenuWork_l.flags & 0x80) {
                    switch (g_UsedItemId) {
                    case 0x1D:
                    case 0x1E:
                        g_PlayerEntity.behavior_flags = 0;
                        g_MenuWork_l.model_loaded = -1;
                        break;
                    case 0x4D:
                        g_PlayerEntity.behavior_flags = 0xB;
                        g_MenuWork_l.model_loaded = -1;
                        break;
                    default:
                        menu_update_equipped_weapon();
                        break;
                    }
                    break;
                }
                menu_update_equipped_weapon();
                break;
            case MENU_MODE_KEY_USED_UP:
            case MENU_MODE_ITEMBOX:
                menu_update_equipped_weapon();
                break;
            }
            if (g_MenuWork_l.model_loaded != 0) {
                load_equipped_weapon_animation_l(g_PlayerEntity.behavior_flags, 0xE, ADDR_g_WeaponAnimBuf, ADDR_g_WeaponObjPacketBuf);
            }
            if (LIT(s32, g_GameStateFlags) & GAME_MIRROR_ON) {
                entity_setup_weapon_part(0xE);
            }
            while ((LIT(s32, g_GameStateFlags) & GAME_FADE_ACTIVE) || g_ActiveBuffer != 0) {
                task_sleep_l(1);
            }
            menu_restore_game_state();
            task_exit();
        case 8:
            menu_reset_status_state();
            while (file_pickup_update() == 0) {
                task_sleep_l(1);
            }
            if (g_ActiveBuffer != 0) {
                task_sleep_l(1);
            }
            menu_restore_game_state();
            task_exit();
            break;
        }
        if (g_MenuWork_l.state != 7) {
            menu_draw_inventory();
            if (g_MenuWork_l.mode == MENU_MODE_ITEMBOX) {
                itembox_draw();
            }
        }
        task_sleep_l(1);
    }
}

void menu_exit_cleanup(void) {
    int LoadImage(RECT *rect, u_long *p);
    g_MenuWork.state = 7;
    set_fading(2, 0xC00);
    LIT(s16, sp_rect_x) = 0x180;
    LIT(s16, sp_rect_y) = 0x100;
    LIT(s16, sp_rect_w) = 0x40;
    LIT(s16, sp_rect_h) = 0x100;
    LoadImage(LITP(RECT, sp_rect), LITP(u_long, g_RoomBgBsBuf));
    g_ClearColorR = 0;
    g_ClearColorG = 0;
    g_ClearColorB = 0;
}

void menu_restore_game_state(void) {
    s32 task_resume_a1() __asm__("task_resume");
    int ResetGraph(int mode);
    void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
    void GsInit3D(void);
    void GsInitGraph2(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
    void GsSetAmbient(long r, long g, long b);
    int GsSetFlatLight(int id, GsF_LIGHT *lt);
    void GsSetLightMode(int mode);
    s32 i;
    s32 t;

    ResetGraph(1);
    GsInitGraph2(0x140U, 0xF0U, 0U, 0U, 0U);
    GsDefDispBuff(0U, 0U, 0U, 0xF0U);
    GsInit3D();
    i = 3;
    center_screen_origin();
    cut_set();
    do {
        i--;
        t = i & 0xFF;
        GsSetFlatLight(t, (t * 0x14) + 0xC + g_RoomData);
    } while (t != 0);
    GsSetAmbient((s32) ROOM->ambient_r, (s32) ROOM->ambient_g, (s32) ROOM->ambient_b);
    GsSetLightMode(0);
    g_VSyncMode = 2;
    LIT(s32, g_GameStateFlags) &= ~GAME_MENU_BYTE;
    task_resume_a1(0);
}

void menu_update_equipped_weapon(void) {
    s32 sound_load_bank1_a3(u8, s32) __asm__("sound_load_bank1");
    void *slot_ptr;

    if (g_EquippedSlot != 0) {
        slot_ptr = (void *)((g_EquippedSlot * 2) + g_ItemSlotsPointer);
        if (g_PlayerBehaviorFlags != INV_SLOT(slot_ptr)[-1].item_id) {
            g_PlayerBehaviorFlags = INV_SLOT(slot_ptr)[-1].item_id;
            if ((g_StatusFlags & STATUS_ADVANCED) && (g_PlayerBehaviorFlags == 4)) {
                g_PlayerBehaviorFlags = 2;
            }
            sound_load_bank1_a3(g_PlayerBehaviorFlags, ADDR_g_DoorFile);
            goto block_8;
        }
    } else if (g_PlayerBehaviorFlags != 0) {
        g_PlayerBehaviorFlags = 0;
block_8:
        g_MenuWork.model_loaded = -1;
    }
}


void menu_draw_inventory(void) {
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern u16 g_InvSlotPosX_l[] __asm__("g_InvSlotPosX");
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void menu_pop_sprite_record(void);
    void display_item_qty(u8, u8, u8);
    u8 n;
    u8 j;
    u8 k;
    s32 c;
    u8 cnt;
    s16 dx;
    s16 dy;
    u8 *e;
    u8 *p;
    u8 *q;
    u8 *r;
    u8 **pp1;
    u8 **pp2;
    u8 **pp3;
    u8 **pp4;

    LITP(GsSPRITE, sp_sprite)->attribute = 0x01000040;
    LITP(GsSPRITE, sp_sprite)->cx = 0;
    LITP(GsSPRITE, sp_sprite)->tpage = 0x1C;
    if (LIT(u8, g_EquippedSlot) != 0) {
        LITP(GsSPRITE, sp_sprite)->u = 0x58;
        LITP(GsSPRITE, sp_sprite)->v = g_ItemSlotIconIndex[LIT(u8, g_EquippedSlot) - 1] << 5;
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1D;
    } else {
        LITP(GsSPRITE, sp_sprite)->u = 0x90;
        LITP(GsSPRITE, sp_sprite)->v = 0xBE;
    }
    LITP(GsSPRITE, sp_sprite)->x = 0xA0;
    LITP(GsSPRITE, sp_sprite)->y = 0x92;
    LITP(GsSPRITE, sp_sprite)->w = 0x28;
    LITP(GsSPRITE, sp_sprite)->h = 0x1E;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1E6;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
    if (LIT(u8, g_EquippedSlot) != 0) {
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1C;
        e = (u8 *)(g_ItemSlotsPointer + LIT(u8, g_EquippedSlot) * 2);
        display_item_qty(e[-2], e[-1], 3);
    }

    n = g_TotalHeldItems;
    j = (n + 4) << 1;
    if (!(g_MenuWork.char_flags & 2)) {
        j += 4;
    }
    while (n != 0) {
        n--;
        LITP(GsSPRITE, sp_sprite)->y = g_InvSlotPosX_l[j -= 1];
        LITP(GsSPRITE, sp_sprite)->x = g_InvSlotPosX_l[--j];
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1D;
        LITP(GsSPRITE, sp_sprite)->w = 0x28;
        LITP(GsSPRITE, sp_sprite)->h = 0x1E;
        LITP(GsSPRITE, sp_sprite)->u = 0x58;
        LITP(GsSPRITE, sp_sprite)->v = g_ItemSlotIconIndex[n] << 5;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E6;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1C;
        e = (u8 *)(g_ItemSlotsPointer + n * 2);
        display_item_qty(INV_SLOT(e)->item_id, INV_SLOT(e)->count, 3);
    }

    LITP(GsSPRITE, sp_sprite)->w = 0x28;
    LITP(GsSPRITE, sp_sprite)->h = 0x1E;
    LITP(GsSPRITE, sp_sprite)->u = 0x90;
    LITP(GsSPRITE, sp_sprite)->v = 0xBE;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1E6;
    n = g_MenuWork.slot_count - g_TotalHeldItems;
    j = 0x18;
    while (n != 0) {
        n--;
        LITP(GsSPRITE, sp_sprite)->y = g_InvSlotPosX_l[j -= 1];
        LITP(GsSPRITE, sp_sprite)->x = g_InvSlotPosX_l[--j];
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
    }

    LITP(GsSPRITE, sp_sprite)->x = 0x16;
    LITP(GsSPRITE, sp_sprite)->y = 0x92;
    LITP(GsSPRITE, sp_sprite)->w = 0x1E;
    LITP(GsSPRITE, sp_sprite)->h = 0x1E;
    LITP(GsSPRITE, sp_sprite)->v = 0xE0;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1E5;
    LITP(GsSPRITE, sp_sprite)->u = (g_PlayerEntity.id & 3) << 5;
    GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
    n = 8;
    LITP(GsSPRITE, sp_sprite)->cy = 0x1E4;
    LITP(GsSPRITE, sp_sprite)->r = 0x50;
    LITP(GsSPRITE, sp_sprite)->g = 0x50;
    LITP(GsSPRITE, sp_sprite)->b = 0x50;
    pp1 = &g_MenuWork.frames_ptr;
    *pp1 = g_InvFrameStripRecs;
    do {
        n -= 2;
        menu_pop_sprite_record();
        LIT(u32, sp_sprite_attribute) = 0x01000000;
        if (g_MenuWork.mode == MENU_MODE_ITEMBOX) {
            if ((g_MenuWork.cursor | 2) == n) {
                LIT(u32, sp_sprite_attribute) = 0x01000040;
            } else if (n == 4) {
                LITP(GsSPRITE, sp_sprite)->r = 0x20;
                LITP(GsSPRITE, sp_sprite)->g = 0x20;
                LITP(GsSPRITE, sp_sprite)->b = 0x20;
            }
        } else if (g_MenuWork.state == 1 && g_MenuWork.cursor == n && g_MenuWork.sub_state < 2) {
            LIT(u32, sp_sprite_attribute) = 0x01000040;
        }
        if (n == 4 && g_MenuWork.has_radio == 0) {
            LITP(GsSPRITE, sp_sprite)->v = 0x40;
        }
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 3);
    } while (n != 0);

    LITP(GsSPRITE, sp_sprite)->attribute = 0x01000040;
    g_MenuWork.frames_ptr = g_InvMenuTabRecs;
    do {
        menu_pop_sprite_record();
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
    } while (g_MenuWork.frames_ptr > g_InvMenuTabRecs - 0x108);

    g_MenuWork.frames_ptr = g_InvSlotFrameTiles6;
    pp2 = &g_MenuWork.frames_ptr;
    do {
        *pp2 -= 2;
        q = g_MenuWork.frames_ptr;
        LITP(GsSPRITE, sp_sprite)->attribute = ((*(s16 *)q << 16) & 0xC00000) | 0x01000040;
        do {
            c = *(u16 *)q;
            c <<= 16;
            c >>= 24;
        } while (0);
        cnt = c;
        menu_pop_sprite_record();
        c &= 0x80;
        if (c != 0) {
            dy = LITP(GsSPRITE, sp_sprite)->h;
            dx = 0;
        } else {
            dx = LITP(GsSPRITE, sp_sprite)->w;
            dy = 0;
        }
        cnt &= 0x7F;
        do {
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
            cnt--;
            LITP(GsSPRITE, sp_sprite)->x += dx;
            LITP(GsSPRITE, sp_sprite)->y += dy;
        } while (cnt != 0);
    } while (g_MenuWork.frames_ptr > g_InvFrameStripRecs);

    if (g_MenuWork.char_flags & 2) {
        g_MenuWork.frames_ptr = g_InvSlotFrameTiles8;
    } else {
        g_MenuWork.frames_ptr = g_InvSlotFrameTiles6;
    }
    do {
        n = 9;
    } while (0);
    g_MenuWork.frames_ptr += 0x7E;
    pp3 = &g_MenuWork.frames_ptr;
    do {
        *pp3 -= 2;
        r = g_MenuWork.frames_ptr;
        LITP(GsSPRITE, sp_sprite)->attribute = ((*(s16 *)r << 16) & 0xC00000) | 0x01000040;
        do {
            c = *(u16 *)r;
            n--;
            c <<= 16;
            c >>= 24;
        } while (0);
        cnt = c;
        menu_pop_sprite_record();
        c &= 0x80;
        if (c != 0) {
            dx = 0;
            dy = LITP(GsSPRITE, sp_sprite)->h;
        } else {
            dx = LITP(GsSPRITE, sp_sprite)->w;
            dy = 0;
        }
        cnt &= 0x7F;
        do {
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 4);
            cnt--;
            LITP(GsSPRITE, sp_sprite)->x += dx;
            LITP(GsSPRITE, sp_sprite)->y += dy;
        } while (cnt != 0);
    } while (n != 0);

    LITP(GsBOXF, sp_box)->attribute = 0;
    LITP(GsBOXF, sp_box)->r = 0;
    LITP(GsBOXF, sp_box)->g = 0;
    LITP(GsBOXF, sp_box)->b = 0;
    g_MenuWork.frames_ptr = (u8 *)g_InvSlotPosX_l;
    pp4 = &g_MenuWork.frames_ptr;
    do {
        p = g_MenuWork.frames_ptr;
        LITP(GsBOXF, sp_box)->h = *(u16 *)(p - 2);
        LITP(GsBOXF, sp_box)->w = *(u16 *)(p - 4);
        LITP(GsBOXF, sp_box)->y = *(u16 *)(p - 6);
        LITP(GsBOXF, sp_box)->x = *(u16 *)(p - 8);
        k = g_ActiveBuffer;
        GsSortBoxFill(LITP(GsBOXF, sp_box), (GsOT *)(k * 0x14 + ADDR_g_OrderingTables), 4);
    } while ((*pp4 -= 8) > g_InvBackdropBoxes);
}

void menu_pop_sprite_record(void) {
    u8 *p = g_CurrentMenuFramesDataPtr;
    u8 **pp;

    LIT(u8, sp_sprite_v) = *(u8 *)(p - 2);
    LIT(u8, sp_sprite_u) = *(u8 *)(p - 4);
    LIT(u16, sp_sprite_h) = *(u16 *)(p - 6);
    LIT(u16, sp_sprite_w) = *(u16 *)(p - 8);
    LIT(u16, sp_sprite_y) = *(u16 *)(p - 10);
    LIT(u16, sp_sprite_x) = *(u16 *)(p - 12);
    pp = &g_CurrentMenuFramesDataPtr;
    *pp -= 12;
}

extern u8 g_ActiveBuffer;

void display_item_qty(u8 type, u8 num, u8 pri) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 Flg_ck_l(s32, s32) __asm__("Flg_ck");
    u8 *s;
    u8 shown;
    u8 n;
    s32 d;
    s32 small;
    u8 u;

    if ((type >= 0x13 || type == 1) && type != 0x2F) {
        return;
    }
    SCRATCH->spr_clut_y = 0x1E4;
    SCRATCH->spr_h = 8;
    SCRATCH->spr_y += 0x14;
    if ((Flg_ck_l(ADDR_g_ScenarioFlags, 0x7E) == 0 || type != 0xA) &&
        (Flg_ck_l(ADDR_g_ScenarioFlags, 0x7A) == 0 || type != 5)) {
        if (type != 6 && type < 0xB) {
            num &= 0x7F;
        }
        sprintf(LITP(char, sp_text), "%03d", num);
        if (type >= 0xB) {
            SCRATCH->spr_x += 0xE;
        } else {
            SCRATCH->spr_x += 4;
        }
        SCRATCH->spr_w = 8;
        switch (type) {
        case 8:
        case 13:
        case 17:
            u = 0x88;
            break;
        case 9:
        case 18:
            u = 0xC0;
            break;
        default:
            u = 0x80;
            break;
        }
        SCRATCH->spr_u = u;
        s = LITP(u8, sp_text);
        shown = 0;
        n = 3;
        small = type < 0xB;
        do {
            d = *s & 0xF;
            n--;
            if (d != 0 || shown || n == 0) {
                SCRATCH->spr_v = d * 8;
                GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), pri);
                shown = 1;
            }
            if (shown || !small) {
                SCRATCH->spr_x += 8;
            }
            s++;
        } while (n);
    } else {
        SCRATCH->spr_w = 0xA;
        SCRATCH->spr_u = 0x98;
        SCRATCH->spr_v = 0x70;
        SCRATCH->spr_x += 6;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), pri);
    }
}

void menu_load_item_model(void) {
    s32 load_file_blocking_p(u16, s32) __asm__("load_file_blocking");
    s32 tmd_relocate_texture_a3(s32, s32, s32, s32) __asm__("tmd_relocate_texture");
    s32 tim_load_to_vram_a4(s32) __asm__("tim_load_to_vram");
    extern u8 g_ItemImageTable_u8 __asm__("g_ItemImageTable");
    extern s8 g_VramTpageCursor_s8 __asm__("g_VramTpageCursor");
    s32 model_id;
    u8 *sel_item = &g_ItemMenuSelectedItemId;

    if ((*sel_item) != g_MenuLoadedResourceId) {
        do {
            g_MenuLoadedResourceId = (*sel_item);
        } while (0);
        model_id = *(&g_ItemImageTable_u8 + (({ (*sel_item) - 1; }) * 4));
        if ((g_MenuModelLoaded & ~0x80) != model_id) {
            g_MenuModelLoaded = (s8) model_id;
            load_file_blocking_p(*(s32 *)((u8 *)&g_ItemModelFiles + (((s32) (model_id << 0x18) >> 0x17))), ADDR_g_DoorFile);
            g_VramClutRowCursor = 0x1C;
            g_VramTpageCursor_s8 = 0x15;
            tim_load_to_vram_a4(ADDR_g_DoorFile);
            tmd_relocate_texture_a3(2, ADDR_g_ItemModelTmd, 0x15, 0x1C);
            g_MenuModelLoaded = (u8) g_MenuModelLoaded | 0x80;
        }
    }
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
/* status: 0 Danger, 1 Caution, 2 and 3 Fine, 4 Poison - the status screen's only labels (seen in game) */
void menu_ekg_draw_status_label(u8 status, u8 *frame) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    u8 menu_ekg_wave_index_l(u8 *) __asm__("menu_ekg_wave_index");
    GsOT *ot;
    u8 t; /* ({ }) keeps the full-width constant (0x830) GCC would narrow for the u8 store */

    switch (status) {
    case 0:
        *frame = menu_ekg_wave_index_l(g_EkgDangerLabelSteps);
        t = g_EkgDangerLabelFrames[*frame];
        *frame = t;
        if (*frame != 0) {
            ot = LITP(GsOT, g_OrderingTables);
            LIT(u8, sp_sprite_v) = ({ t * 8 + 0x830; });
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), ot + g_ActiveBuffer, 4);
        }
        break;
    case 1:
        *frame = menu_ekg_wave_index_l(g_EkgCautionLabelSteps);
        t = g_EkgCautionLabelFrames[*frame];
        *frame = t;
        if (*frame != 0) {
            ot = LITP(GsOT, g_OrderingTables);
            LIT(u8, sp_sprite_v) = ({ t * 8 + 0x818; });
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), ot + g_ActiveBuffer, 4);
        }
        break;
    case 2:
    case 3:
        LIT(u8, sp_sprite_v) = 0;
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
        break;
    case 4:
        *frame = menu_ekg_wave_index_l(g_EkgDangerLabelSteps);
        t = g_EkgDangerLabelFrames[*frame];
        *frame = t;
        if (*frame != 0) {
            ot = LITP(GsOT, g_OrderingTables);
            LIT(u8, sp_sprite_v) = ({ t * 8 + 0x840; });
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), ot + g_ActiveBuffer, 4);
        }
        break;
    }
}

void menu_draw_health_bar(void) {
    void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri);
    int rand(void);
    extern u8 g_EkgColor2G_s __asm__("g_EkgColor2G");
    extern u8 g_EkgColor2B_l __asm__("g_EkgColor2G+1");
    extern s16 g_EkgPrimaryX1_l[] __asm__("g_EkgPrimaryX1");
    void menu_ekg_draw_segment(s32, s32, s32);
    u8 cnt;
    u8 *st;
    s32 t;
    u8 v;
    u8 *e0;
    s32 r;
    s16 x0;
    s16 x1;
    s8 *p;
    s8 e;

    st = &g_HealthBarAnimState;
    switch (*st) {
    case 0:
        *st = 1;
        g_EkgPrimaryX1_l[0] = 0x84;
        g_EkgSecondaryX1 = 0x84;
        /* fallthrough */
    case 1:
        SCRATCH->line_flags = 0;
        SCRATCH->gline_flags = 0;
        if (++g_EkgPrimaryX1_l[0] >= 0x84) {
            g_EkgPrimaryX1_l[0] = 0x35;
            if (g_PlayerEntity.health != 0) {
                g_HealthStatus = (g_PlayerEntity.health - 1) / (g_PlayerWork.max_health >> 2);
                if (g_HealthStatus >= 4) {
                    g_HealthStatus = 3;
                }
            } else {
                g_HealthStatus = 0;
            }
            g_EkgColorR[0] = g_EkgColorTable[g_HealthStatus][0];
            g_EkgColorR[1] = g_EkgColorTable[g_HealthStatus][1];
            g_EkgColorR[2] = g_EkgColorTable[g_HealthStatus][2];
            if ((g_PlayerEntity.collision_flags & 0x22) && (rand() & 1)) {
                g_HealthStatus = 4;
            }
            t = rand() & 3;
            e0 = g_HealthBarFaceAnim;
            v = t;
            if ((*e0 & 3) == v) {
                v = (t + 1) & 3;
            }
            v |= g_HealthStatus << 2;
            *e0 = v;
            g_EkgWavePrimary = g_EkgWaveTable[v];
        }
        /* non-struct store (not g_EkgPrimaryX0[0]): sched2 must keep it before the g_EkgSecondaryX1 store */
        *(s16 *)(void *)g_EkgPrimaryX0 = g_EkgPrimaryX0[1] + 0xF;
        if (++g_EkgSecondaryX1 >= 0x84) {
            g_EkgSecondaryX1 = g_EkgPrimaryX0[1] - 0x1F;
            g_EkgWaveSecondary = g_EkgWavePrimary;
            g_EkgColor2R = g_EkgColorR[0];
            g_EkgColor2G[0] = g_EkgColorR[1];
            g_EkgColor2G[1] = g_EkgColorR[2];
        }
        g_EkgSecondaryX0 = g_EkgSecondaryX1 + 0x1F;
        if (g_PlayerEntity.collision_flags & 0x22) {
            g_HealthStatus = 4;
        }
        SCRATCH->spr_flags = 0x1000040;
        SCRATCH->spr_x = 0x64;
        SCRATCH->spr_y = 0xA8;
        SCRATCH->spr_u = 0x60;
        SCRATCH->spr_w = 0x20;
        SCRATCH->spr_h = 8;
        SCRATCH->spr_clut_y = 0x1E4;
        SCRATCH->spr_clut_x = 0;
        SCRATCH->spr_tpage = 0x1C;
        menu_ekg_draw_status_label(g_HealthStatus, &cnt);
        if (g_EkgPrimaryX0[0] >= 0x54 && g_EkgPrimaryX0[1] < 0x84) {
            SCRATCH->line_r = g_EkgColorR[0];
            SCRATCH->line_g = g_EkgColorR[1];
            SCRATCH->line_b = g_EkgColorR[2];
            x0 = g_EkgPrimaryX0[0];
            if (g_EkgPrimaryX0[0] >= 0x84) {
                x0 = 0x83;
            }
            x1 = g_EkgPrimaryX0[1];
            if (g_EkgPrimaryX0[1] < 0x54) {
                x1 = 0x54;
            }
            p = g_EkgWavePrimary;
            while (x0 < p[0] + 0x54) {
                p += 4;
            }
            SCRATCH->line_y1 = p[2] + 0xA2;
            SCRATCH->line_y0 = p[2] + p[3] * (({ x0 - 0x54; }) - p[0]) + 0xA2;
            SCRATCH->line_x0 = x0;
            e = p[0];
            if (x1 >= (s16)(e + 0x54)) {
                SCRATCH->line_x1 = x1;
                GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
            } else {
                SCRATCH->line_x1 = e + 0x54;
                GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
                if (p[0] != 0) {
                    p += 4;
                    while (x1 < p[0] + 0x54) {
                        SCRATCH->line_x0 = p[1] + 0x54;
                        SCRATCH->line_x1 = p[0] + 0x54;
                        SCRATCH->line_y0 = p[-2] + 0xA2;
                        SCRATCH->line_y1 = p[2] + 0xA2;
                        GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
                        p += 4;
                    }
                    SCRATCH->line_x0 = p[1] + 0x54;
                    SCRATCH->line_x1 = x1;
                    SCRATCH->line_y0 = p[-2] + 0xA2;
                    SCRATCH->line_y1 = p[2] + p[3] * (({ x1 - 0x54; }) - p[0]) + 0xA2;
                    GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
                }
            }
        }
        if (g_EkgSecondaryX0 >= 0x54 && g_EkgSecondaryX1 < 0x84) {
            u8 dr;
            u8 dg;
            u8 db;

            x0 = g_EkgSecondaryX0;
            dr = g_EkgColor2R >> 5;
            dg = g_EkgColor2G_s >> 5;
            db = g_EkgColor2B_l >> 5;
            if (g_EkgSecondaryX0 >= 0x84) {
                r = g_EkgSecondaryX0 - 0x83;
                SCRATCH->gline_r0 = g_EkgColor2R - dr * r;
                SCRATCH->gline_g0 = g_EkgColor2G_s - dg * r;
                SCRATCH->gline_b0 = g_EkgColor2B_l - db * r;
                x0 = 0x83;
            } else {
                SCRATCH->gline_r0 = g_EkgColor2R;
                SCRATCH->gline_g0 = g_EkgColor2G_s;
                SCRATCH->gline_b0 = g_EkgColor2B_l;
            }
            x1 = g_EkgSecondaryX1;
            if (g_EkgSecondaryX1 < 0x54) {
                x1 = 0x54;
            }
            p = g_EkgWaveSecondary;
            while (x0 < p[0] + 0x54) {
                p += 4;
            }
            SCRATCH->gline_y1 = p[2] + 0xA2;
            SCRATCH->gline_y0 = p[2] + p[3] * (({ x0 - 0x54; }) - p[0]) + 0xA2;
            SCRATCH->gline_x0 = x0;
            e = p[0];
            if (x1 >= (s16)(e + 0x54)) {
                SCRATCH->gline_x1 = x1;
                menu_ekg_draw_segment(dr, dg, db);
                return;
            }
            SCRATCH->gline_x1 = e + 0x54;
            menu_ekg_draw_segment(dr, dg, db);
            if (p[0] == 0) {
                return;
            }
            p += 4;
            while (x1 < p[0] + 0x54) {
                SCRATCH->gline_x0 = p[1] + 0x54;
                SCRATCH->gline_x1 = p[0] + 0x54;
                SCRATCH->gline_y0 = p[-2] + 0xA2;
                SCRATCH->gline_y1 = p[2] + 0xA2;
                menu_ekg_draw_segment(dr, dg, db);
                p += 4;
            }
            SCRATCH->gline_x0 = p[1] + 0x54;
            SCRATCH->gline_x1 = x1;
            SCRATCH->gline_y0 = p[-2] + 0xA2;
            SCRATCH->gline_y1 = p[2] + p[3] * (({ x1 - 0x54; }) - p[0]) + 0xA2;
            menu_ekg_draw_segment(dr, dg, db);
        }
        return;
    case 2:
        *st = 3;
        g_EkgPrimaryX0[0] = 0xAF;
        /* fallthrough */
    case 3:
        SCRATCH->line_flags = 0x50000000;
        SCRATCH->line_x0 = 0x54;
        SCRATCH->line_x1 = 0x83;
        SCRATCH->line_r = 0;
        SCRATCH->line_g = 0;
        SCRATCH->line_b = 0;
        if (g_EkgPrimaryX0[0] >= 0x84) {
            if (g_EkgPrimaryX0[0] < 0x92) {
                /* FAKEMATCH: volatile store: CSE must not record it, so the next read of cnt
                   is a real reload (original: sb v0,16(sp); lbu v1,16(sp)) */
                *(volatile u8 *)&cnt = 0x92 - g_EkgPrimaryX0[0];
                SCRATCH->line_y0 = 0x92;
                g_HealthStatus = ~(cnt << 4);
                cnt = 0xF - cnt;
            } else {
                g_HealthStatus = 0xFF;
                SCRATCH->line_y0 = g_EkgPrimaryX0[0];
                cnt = 0xF;
            }
            if (g_MenuFlags & 0x20) {
                SCRATCH->line_g = g_HealthStatus;
                if (g_MenuFlags & 0x10) {
                    SCRATCH->line_r = g_HealthStatus;
                    SCRATCH->line_b = g_HealthStatus;
                }
            } else {
                SCRATCH->line_b = g_HealthStatus;
            }
            do {
                SCRATCH->line_y1 = SCRATCH->line_y0;
                GsSortLine(LITP(GsLINE, sp_line), LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 4);
                if (SCRATCH->line_r != 0) {
                    SCRATCH->line_r -= 0x10;
                }
                if (SCRATCH->line_g != 0) {
                    SCRATCH->line_g -= 0x10;
                }
                if (SCRATCH->line_b != 0) {
                    SCRATCH->line_b -= 0x10;
                }
                SCRATCH->line_y0++;
                cnt--;
            } while (SCRATCH->line_y0 < 0xB0 && cnt != 0);
            g_EkgPrimaryX0[0] -= 2;
        } else {
            g_HealthBarAnimState = 0;
            g_MenuActionResult = 0;
            g_MenuFlags &= 0xCF;
        }
        break;
    }
}

s32 menu_ekg_wave_index(u8 *steps) {
    s32 idx;
    u32 x;
    u8 *p;

    p = steps;
    x = (g_EkgPrimaryX1 - 0x35) & 0xFF;
    idx = 0;
    if (x >= (u8) *p) {
        do {
            p += 1;
            idx += 1;
        } while (x >= (u8) *p);
    }
    return idx & 0xFF;
}

void menu_ekg_draw_segment(s32 dr, s32 dg, s32 db) {
    void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri);
    s32 len;

    len = SCRATCH_AS(gline_x0, u8) - SCRATCH_AS(gline_x1, u8);
    SCRATCH->gline_r1 = SCRATCH->gline_r0 - (dr * len);
    SCRATCH->gline_g1 = SCRATCH->gline_g0 - (dg * len);
    SCRATCH->gline_b1 = SCRATCH->gline_b0 - (db * len);
    GsSortGLine(LITP(GsGLINE, sp_gline), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 4U);
    SCRATCH->gline_r0 = SCRATCH->gline_r1;
    SCRATCH->gline_g0 = SCRATCH->gline_g1;
    SCRATCH->gline_b0 = SCRATCH->gline_b1;
}

void menu_handle_input(void) {
    void draw_item_name_l(s32, s32, u16, u16) __asm__("draw_item_name");
    s32 Flg_ck_l(s32, s32) __asm__("Flg_ck");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    void menu_draw_cursor(void);
    void menu_update_selected_item(void);
    void menu_item_submenu(void);
    extern void (*menu_tab_func_tbl[])(void);
    extern u16 g_ScreenOffsetX_u16 __asm__("g_ScreenOffsetX");
    extern u8 g_ItemboxScrollDir_u8 __asm__("g_ItemboxScrollDir");
    extern u8 g_ItemboxScroll_u8 __asm__("g_ItemboxScroll");
    u8 *st;
    u16 *pad;

    st = &g_MenuWork.sub_state;
    switch (*st) {
    case 0:
        *st = 1;
        g_MenuCursorBlinkTimer = 0;
        menu_update_selected_item();
        /* fallthrough */
    case 1:
        if (g_PadPressed & PAD_CONFIRM) {
            u8 *sel = &g_MenuWork.cursor;

            if (*sel & 0xF8) {
                if (g_MenuWork.item_id == 0) {
                    goto dec;
                }
                g_ItemboxScrollDir_u8 = g_MenuWork.item_id < 11;
            } else if (*sel == 4) {
                if (g_MenuWork.has_radio == 0) {
                dec:
                    g_MenuCursorBlinkTimer--;
                    goto draw;
                }
                if (Flg_ck_l((s32)&g_ItemUseFlags, 0x3F) == 0) {
                    g_MenuWork.viewer_state = 0;
                    g_MenuWork.item_id = 0x4D;
                    menu_load_item_model();
                }
            }
            g_MenuWork.sub_state = 2;
            g_MenuWork.submenu_state = 0;
            g_ItemboxScroll_u8 = 0;
            g_MenuCursorBlinkTimer = 0;
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
        } else {
            pad = LITP(u16, g_PadTrigger);
            if (*pad & 0xF000) {
                if (*pad & 0xA000) {
                    u8 *sel = &g_MenuWork.cursor;

                    *sel ^= 2;
                }
                if (*pad & 0x1000) {
                    u8 *sel = &g_MenuWork.cursor;

                    if (*sel & 0xFC) {
                        *sel -= 4;
                    } else {
                        *sel += (g_MenuWork.slot_count + 2) * 2;
                    }
                } else if (*pad & 0x4000) {
                    u8 *sel = &g_MenuWork.cursor;

                    *sel += 4;
                    if ((g_MenuWork.slot_count + 3) * 2 < *sel) {
                        *sel &= 2;
                    }
                }
                menu_update_selected_item();
                g_MenuCursorBlinkTimer = 0;
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            } else {
                u8 *cnt = &g_MenuCursorBlinkTimer;

                (*cnt)--;
            }
        }
    draw:
        menu_draw_cursor();
        draw_item_name_l(g_MenuWork.item_id, 0, 0x30 - g_ScreenOffsetX_u16, 0xBA - g_ScreenOffsetY);
        break;
    case 2:
        if (g_MenuWork.cursor & 0xF8) {
            menu_item_submenu();
        } else {
            menu_tab_func_tbl[g_MenuWork.cursor >> 1]();
        }
        if (!(g_MenuWork.flags & 0x41) && g_MenuWork.state != 7) {
            menu_draw_cursor();
        }
        break;
    }
}

void menu_draw_cursor(void) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 unused[2];
    u8 v;
    u16 y;
    GsOT *ot;
    u8 *menu_cursor = &g_MenuWork.cursor;

    SCRATCH->spr_flags = 0x01000040;
    SCRATCH->spr_tpage = 0x1C;
    SCRATCH->spr_x = ((s16 *)&g_InvSlotPosX)[(*menu_cursor)];
    y = ((s16 *)&g_InvSlotPosY)[(*menu_cursor)];
    SCRATCH->spr_clut_x = 0;
    SCRATCH->spr_clut_y = 0x1E4;
    SCRATCH->spr_y = y;
    if ((*menu_cursor) & 0xF8) {
        SCRATCH->spr_w = 0x28;
        SCRATCH->spr_h = 0x1E;
        SCRATCH->spr_u = 0x80;
        SCRATCH->spr_v = 0xE0;
        if (!(g_MenuCursorBlinkTimer & 0x20)) {
            SCRATCH->spr_u = 0xA8;
        }
        SCRATCH->rect_x = 0xCE;
        if (!(g_MenuWork.char_flags & 2)) {
            SCRATCH->spr_y = (u16) (y + 0x1E);
        }
        ot = LITP(GsOT, g_OrderingTables);
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), ot + g_ActiveBuffer, 3U);
        v = 0x90;
    } else {
        SCRATCH->spr_w = 0x30;
        SCRATCH->spr_h = 0x10;
        SCRATCH->spr_v = 0x50;
        SCRATCH->spr_u = 0;
        if (g_MenuWork.state == 3) {
            SCRATCH->spr_x = (u16) *(u16 *)((u8 *)&g_InvSlotPosX + ((((*menu_cursor) | 2) * 2)));
        }
        ot = LITP(GsOT, g_OrderingTables);
        GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), ot + g_ActiveBuffer, 4U);
        v = 0x98;
    }
    SCRATCH->spr_v = v;
}

void menu_update_selected_item(void) {
    u32 slot;

    slot = (((u8) g_MenuWork.cursor >> 1) - 4) & 0xFF;
    if (slot < (u8) g_TotalHeldItems) {
        g_MenuWork.item_id = *(s32 *)((slot * 2) + g_ItemSlotsPointer);
        return;
    }
    g_MenuWork.item_id = 0;
}


void menu_item_submenu(void) {
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    extern void (*menu_item_action_tbl_l[])() __asm__("menu_item_action_tbl");
    void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void draw_item_name_l(s32, s32, u16, u16) __asm__("draw_item_name");
    void play_sfx_l() __asm__("play_sfx");
    void menu_item_toggle_equip_l() __asm__("menu_item_toggle_equip");
    void menu_update_selected_item_l() __asm__("menu_update_selected_item");
    u8 t;
    u8 i;
    u8 v;
    s32 x;

    switch (g_MenuWork.submenu_state) {
    case 0:
        g_MenuWork.submenu_state = 1;
        g_MenuWork.submenu_timer = 0x80;
        g_MenuWork.move_anim[0] = 0;
    case 1:
        if (++g_MenuWork.submenu_timer & 8) {
            g_MenuWork.submenu_state = 2;
            g_MenuWork.flags &= 0xFD;
        }
        draw_item_name_l(g_MenuWork.item_id, 0, (u16)(0x30 - g_ScreenOffsetX), (u16)(0xBA - g_ScreenOffsetY));
        break;
    case 2:
        if (!(LIT(u16, g_PadPressed) & PAD_CANCEL)) {
            draw_item_name_l(g_MenuWork.item_id, 0, (u16)(0x30 - g_ScreenOffsetX), (u16)(0xBA - g_ScreenOffsetY));
            if (LIT(u16, g_PadPressed) & PAD_CONFIRM) {
                switch (g_MenuWork.submenu_row) {
                case 0:
                    if (g_MenuWork.item_id < 0xB) {
                        menu_item_toggle_equip_l();
                        g_MenuWork.submenu_state = 3;
                        g_MenuWork.submenu_timer &= 0x7F;
                        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                    } else {
                        g_MenuWork.submenu_state = 4;
                        g_MenuWork.action_state = 0;
                    }
                    break;
                case 1:
                case 2:
                    g_MenuWork.submenu_state = 4;
                    g_MenuWork.action_state = 0;
                    g_MenuWork.flags &= 0xFB;
                    play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
                    break;
                }
                break;
            }
            if (g_PadTrigger & (PSX_UP | PSX_DOWN)) {
                if (g_PadTrigger & PSX_UP) {
                    if (g_MenuWork.submenu_row != 0) {
                        g_MenuWork.submenu_row--;
                    }
                } else if (g_MenuWork.submenu_row < 2) {
                    g_MenuWork.submenu_row++;
                }
                play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
            }
            if (!(g_MenuWork.flags & 2)) {
                break;
            }
            g_MenuWork.submenu_timer &= 0x7F;
        }
        g_MenuWork.submenu_state = 3;
        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
    case 3:
        if (!((g_MenuWork.submenu_timer -= 1) & 0x7F)) {
            g_MenuWork.sub_state = 1;
            menu_update_selected_item_l();
            if (g_MenuWork.flags & 0x80) {
                g_MenuWork.flags |= 1;
                return;
            }
        }
        draw_item_name_l(g_MenuWork.item_id, 0, (u16)(0x30 - g_ScreenOffsetX), (u16)(0xBA - g_ScreenOffsetY));
        break;
    case 4:
        menu_item_action_tbl_l[g_MenuWork.submenu_row]();
        if (g_MenuWork.flags & 0x40) {
            return;
        }
        if (g_MenuWork.flags & 2) {
            g_MenuWork.submenu_state = 3;
            g_MenuWork.submenu_timer &= 0x7F;
        } else if (g_MenuWork.flags & 4) {
            g_MenuWork.submenu_state = 2;
        }
        break;
    }
    if (g_MenuWork.submenu_timer != 0 && !(g_MenuWork.move_anim[0] & 8)) {
        LITP(GsSPRITE, sp_sprite)->attribute = 0x01000040;
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1C;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E4;
        LITP(GsSPRITE, sp_sprite)->cx = 0;
        LITP(GsSPRITE, sp_sprite)->x = 0x90;
        if (!(g_MenuWork.submenu_state & 1) && g_MenuWork.move_anim[0] == 0) {
            LITP(GsSPRITE, sp_sprite)->u = 0x30;
            LITP(GsSPRITE, sp_sprite)->v = 0x60;
            LITP(GsSPRITE, sp_sprite)->w = 0x30;
            LITP(GsSPRITE, sp_sprite)->h = 0x18;
            LITP(GsSPRITE, sp_sprite)->y = g_MenuWork.submenu_row * 0x18 + 0x39;
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 5);
        }
        LITP(GsSPRITE, sp_sprite)->y = 0x39;
        LITP(GsSPRITE, sp_sprite)->u = 0x30;
        if (g_MenuWork.is_weapon != 0) {
            LITP(GsSPRITE, sp_sprite)->v = 0;
        } else {
            LITP(GsSPRITE, sp_sprite)->v = 0x18;
        }
        t = g_MenuWork.submenu_timer;
        i = 0;
        if (!(t & 0x80)) {
            LITP(GsSPRITE, sp_sprite)->x += (8 - g_MenuWork.submenu_timer) * 6;
            LITP(GsSPRITE, sp_sprite)->y += (8 - g_MenuWork.submenu_timer) * 3;
            LITP(GsSPRITE, sp_sprite)->u += (8 - g_MenuWork.submenu_timer) * 6;
            LITP(GsSPRITE, sp_sprite)->v += (8 - g_MenuWork.submenu_timer) * 3;
        }
        LITP(GsSPRITE, sp_sprite)->w = (t & 0x7F) * 6;
        LITP(GsSPRITE, sp_sprite)->h = (t & 0x7F) * 3;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E4;
        do {
            if ((g_MenuWork.submenu_timer & 0x80) && g_MenuWork.move_anim[0] != 0) {
                x = g_MenuWork.move_anim[0] - i;
                LITP(GsSPRITE, sp_sprite)->x = (s16)x * 8;
                if (x * 8 < 0) {
                    LITP(GsSPRITE, sp_sprite)->x = 0x90;
                } else {
                    LITP(GsSPRITE, sp_sprite)->x += 0x90;
                }
            }
            GsSortFlipSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)(g_ActiveBuffer * 0x14 + ADDR_g_OrderingTables), 5);
            v = LITP(GsSPRITE, sp_sprite)->v;
            LITP(GsSPRITE, sp_sprite)->v = v + 0x18;
            if (i == 0 && g_MenuWork.is_weapon != 0) {
                LITP(GsSPRITE, sp_sprite)->v = v + 0x30;
            }
            i++;
            LITP(GsSPRITE, sp_sprite)->y += 0x18;
        } while (i < 3);
    }
}

void menu_tab_map(void) {
    s32 menu_map_state_machine();
    if (menu_map_state_machine() != 0) {
        g_MenuWork.sub_state = 1;
        g_MenuWork.model_loaded &= 0x80;
    }
}

void menu_tab_file(void) {
    s32 menu_file_state_machine_a1() __asm__("menu_file_state_machine");
    if (menu_file_state_machine_a1() != 0) {
        g_MenuWork.sub_state = 1;
        g_MenuWork.model_loaded &= 0x80;
    }
}

void menu_tab_radio(void) {
    s32 item_viewer_update_a1() __asm__("item_viewer_update");
    s32 Flg_ck_a13() __asm__("Flg_ck");
    s32 play_sfx_a13() __asm__("play_sfx");
    if (Flg_ck_a13(&g_ItemUseFlags, 0x3F) != 0) {
        g_MenuWork.sub_state = 1;
        g_UsedItemId = 0x4D;
        g_MenuWork.flags |= 0x81;
        play_sfx_a13(SFX_BANK_SYSTEM, 7, 0);
        return;
    }
    if (item_viewer_update_a1() != 0) {
        g_MenuWork.sub_state = 1;
        g_MenuWork.item_id = 0;
    }
}

void menu_tab_exit(void) {
    void menu_exit_cleanup();
    g_MenuWork.sub_state = 1;
    menu_exit_cleanup();
}

void menu_item_toggle_equip(void) {
    u8 slot;

    slot = ((u8) g_MenuWork.cursor >> 1) - 3;
    if (LIT(u8, g_EquippedSlot) != (slot & 0xFF)) {
        LIT(u8, g_EquippedSlot) = slot;
        return;
    }
    LIT(u8, g_EquippedSlot) = 0;
}

void menu_item_use_item(void) {
    typedef struct {
        u8 flags;
        u8 pad1[7];
        u8 item_id;
        u8 pad9[6];
        u8 action_state;
    } MenuWorkFrom3;
    extern MenuWorkFrom3 g_MenuFlags_s __asm__("g_MenuFlags");
    extern u8 (*menu_item_use_tbl_l[])(u8) __asm__("menu_item_use_tbl");
    extern u16 g_ScreenOffsetX_l __asm__("g_ScreenOffsetX");
    extern u8 g_ItemActionMenuState_l __asm__("g_ItemActionMenuState");
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    void draw_item_name_u8(u8, s32, u16, u16) __asm__("draw_item_name");
    /* u16 first parameter: the argument is (u8)i + 0xF6 without a final mask */
    void set_message_display_l(u16, s32) __asm__("set_message_display");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    u8 i;
    u8 a;

    if (g_MenuFlags_s.action_state == 0) {
        a = (g_MenuWork.cursor >> 1) - 4;
        for (i = 9; g_MenuFlags_s.item_id < g_ItemUseTypeRanges[i]; i--) {
        }
        if (menu_item_use_tbl_l[i](a)) {
            play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
            if (g_ItemSlotsPointer_l[a * 2 + 1] != 0) {
                g_ItemSlotsPointer_l[a * 2 + 1]--;
                if (g_ItemSlotsPointer_l[a * 2 + 1] == 0) {
                    g_ItemSlotsPointer_l[a * 2] = 0;
                    rearrange_item_slots();
                    menu_update_selected_item();
                }
            }
            g_MenuFlags_s.flags |= 2;
            draw_item_name_u8(g_MenuWork.item_id, 0, 0x30 - g_ScreenOffsetX_l, 0xBA - g_ScreenOffsetY);
        } else {
            g_ItemActionMenuState_l = 1;
            set_message_display_l(i + 0xF6, 0);
            g_MenuFlags &= 0xFB;
        }
    } else if (!(g_MessageStatus_l & 0x80)) {
        g_MenuFlags |= 4;
    }
}

s32 menu_item_use_none(void) {
    return 0;
}

s32 menu_item_use_none_t1(void) {
    return 0;
}

s32 menu_item_use_if_flag(void) {
    s32 Flg_ck_a14(s32 *, s32) __asm__("Flg_ck");
    u8 *sel_item = &g_MenuWork.item_id;
    if (Flg_ck_a14(&g_ItemUseFlags, (*sel_item) - 0x1B) != 0) {
        g_UsedItemId = (*sel_item);
        g_MenuWork.flags |= 0x80;
        return 1;
    }
    return 0;
}

s32 menu_item_use_if_flag_t4(void) {
    s32 Flg_ck_a15(s32 *, s32) __asm__("Flg_ck");
    u8 *sel_item = &g_MenuWork.item_id;
    if (Flg_ck_a15(&g_ItemUseFlags, (*sel_item) - 0x1B) != 0) {
        g_UsedItemId = (*sel_item);
        g_MenuWork.flags |= 0x80;
        return 1;
    }
    return 0;
}

s32 menu_item_use_red_book(void) {
    s32 Flg_ck_a16(s32 *, s32) __asm__("Flg_ck");
    u8 *sel_item = &g_MenuWork.item_id;
    if ((*sel_item) == 0x3E) {
        if (Flg_ck_a16(&g_ItemUseFlags, 0x23) != 0) {
            g_UsedItemId = (*sel_item);
            g_MenuWork.flags |= 0x80;
            return 1;
        }

        return 0;
    }
    return 0;
}

u8 menu_item_use_heal(void) {
    void Flg_off_l(void *, u8) __asm__("Flg_off");
    extern s16 g_PlayerHealth_l __asm__("g_PlayerHealth");
    extern u8 g_PlayerMaxHealth_l __asm__("g_PlayerMaxHealth");
    u8 b;
    u8 lo;
    u8 hi;
    u8 r;
    u8 *p;
    u8 *p2;
    u8 f;

    b = g_ItemHealTable[g_MenuWork.item_id];
    r = 0;
    if (b != 0) {
        lo = b & 0xF;
        hi = b & 0xF0;
        if (lo != 0 && g_PlayerHealth_l < g_PlayerMaxHealth_l) {
            g_MenuWork.flags |= 0x20;
            r = 1;
            switch (lo) {
            case 1:
                g_PlayerHealth_l += (u8)(g_PlayerMaxHealth_l / 3);
                break;
            case 2:
                g_PlayerHealth_l += g_PlayerMaxHealth_l * 2 / 3;
                break;
            case 3:
                g_PlayerHealth_l = g_PlayerWork.max_health;
                break;
            }
            if (g_PlayerEntity.health > g_PlayerWork.max_health) {
                g_PlayerHealth_l = g_PlayerWork.max_health;
            }
        }
        if (hi != 0 && ((f = g_PlayerEntity.collision_flags) & 0x22)) {
            switch (hi) {
            case 0x10:
                if (f & 0x20) {
                    Flg_off_l(LITP(void, g_ScenarioFlags2), 0x43);
                    p = &g_MenuWork.flags;
                    g_PlayerCollisionFlags &= 0xDF;
                    *p |= 0x10;
                    r = 1;
                }
                break;
            case 0x20:
                if (f & 2) {
                    r = 1;
                    g_PlayerCollisionFlags = f & 0xFD;
                    p2 = &g_MenuWork.flags;
                    *p2 |= 0x10;
                }
                break;
            }
        }
    }
    if (r != 0) {
        g_MenuWork.health_anim_state = 2;
        g_MenuWork.action_result = 0xFF;
    }
    return r;
}

s32 menu_item_use_always(void) {
    return 1;
}

s32 menu_item_use_none_t9(void) {
    return 0;
}

void menu_item_view_model(void) {
    s32 item_viewer_update_a2() __asm__("item_viewer_update");
    s32 draw_item_name_a3() __asm__("draw_item_name");
    s32 menu_load_item_model_a1() __asm__("menu_load_item_model");
    extern u16 g_ScreenOffsetX_u16 __asm__("g_ScreenOffsetX");
    u8 *p;
    u8 *q;

    switch (g_ItemActionMenuState[0]) {
    case 0:
        g_ItemActionMenuState[0] = 1;
        menu_load_item_model_a1();
        /* fallthrough */
    case 1:
        p = &g_ItemMoveAnim;
        if (!(++(*p) & 8)) {
            break;
        }
        g_ItemActionMenuState[0] = 2;
        g_ItemViewerState = 0;
        /* fallthrough */
    case 2:
        if (!(item_viewer_update_a2() & 0xFF)) {
            return;
        }
        g_ItemActionMenuState[0] = 3;
        /* fallthrough */
    case 3:
        q = &g_ItemMoveAnim;
        (*q)--;
        if (*q == 0) {
            g_MenuFlags |= 4;
        }
        break;
    default:
        return;
    }
    /* Always true, both arms identical: found by decomp-permuter. Even the line layout of
     * this if/else matters for GCC 2.6.3 (position of `move a1,zero` before the call). */
    if (g_ItemActionMenuState)
    {
        draw_item_name_a3(g_ItemMenuSelectedItemId, 0, (0x30 - g_ScreenOffsetX_u16) & 0xFFFF, (0xBA - g_ScreenOffsetY) & 0xFFFF);
    }
    else
    {
        draw_item_name_a3(g_ItemMenuSelectedItemId, 0, (0x30 - g_ScreenOffsetX_u16) & 0xFFFF, (0xBA - g_ScreenOffsetY) & 0xFFFF);
    }
}

/* two 4-aligned jump tables, the first followed by a pad word (tools/jtalign.awk) */
__asm__("#JTALIGN2PAD");
__asm__("#JTALIGN2");
void menu_item_move_item(void) {
    extern u8 g_MenuActionResult_l __asm__("g_MenuActionResult");
    extern u8 g_ItemActionMenuState_l __asm__("g_ItemActionMenuState");
    extern u8 g_MenuSubTimer_l __asm__("g_MenuSubTimer");
    extern u8 g_ItemMoveAnim_l[] __asm__("g_ItemMoveAnim");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    extern u16 g_InvSlotPosX_l[] __asm__("g_InvSlotPosX");
    extern u16 g_InvSlotPosY_l[] __asm__("g_InvSlotPosY");
    extern u16 g_ScreenOffsetX_l __asm__("g_ScreenOffsetX");
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    void draw_item_name_l(s32, s32, u16, u16) __asm__("draw_item_name");
    s32 draw_item_name_p(u8, s32, u16, u16) __asm__("draw_item_name");
    s32 set_message_display_i() __asm__("set_message_display");
    void play_sfx_l() __asm__("play_sfx");
    s32 menu_item_check_combine(void);
    void menu_item_apply_combine_l() __asm__("menu_item_apply_combine");
    u8 n;
    u16 y;
    u8 *p1;
    u8 *p2;
    u8 *p3;
    u8 *p4;
    u8 *p5;
    u8 *p6;
    u8 *p7;
    u8 *p8;
    s32 unused[2];

    if (g_ItemActionMenuState_l != 0) {
        n = (g_ItemMoveSrcCursor >> 1) - 4;
    } else {
        n = (g_MenuCursor >> 1) - 4;
    }
    switch (g_ItemActionMenuState_l) {
    case 0:
        g_ItemActionMenuState_l = 1;
        g_ItemMoveAnim_l[1] = 0;
        g_ItemMoveAnim_l[2] = 0;
        g_MenuSubTimer_l = 0xFF;
        g_ItemMoveSrcCursor = g_MenuCursor;
        break;
    case 1:
        p1 = &g_ItemMoveAnim_l[1];
        *p1 += 4;
        g_ItemMoveAnim_l[2] += 3;
        if (*p1 != 0x10) {
            break;
        }
        g_ItemActionMenuState_l = 2;
        g_MenuFlags &= 0xF7;
        /* fallthrough */
    case 2:
        if (!(g_PadPressed & PAD_CANCEL)) {
            if (g_PadPressed & PAD_CONFIRM) {
                if (g_MenuCursor != g_ItemMoveSrcCursor && n < g_TotalHeldItems
                    && g_ItemCombineIdx[({ g_ItemMenuSelectedItemId - 1; }) * 4] >= 0) {
                    switch ((u8)menu_item_check_combine()) {
                    case 4:
                        g_ItemActionMenuState_l = 3;
                        g_MenuFlags |= 8;
                        menu_update_selected_item();
                        break;
                    case 1:
                        g_ItemActionMenuState_l = 4;
                        g_MenuActionResult_l = 1;
                        set_message_display(0xF1, 0);
                        break;
                    case 2:
                        g_ItemActionMenuState_l = 4;
                        g_MenuActionResult_l = 4;
                        set_message_display(0xF4, 0);
                        break;
                    case 3:
                        g_ItemActionMenuState_l = 4;
                        g_MenuActionResult_l = 5;
                        set_message_display(0xF5, 0);
                        break;
                    }
                    g_MenuSubTimer_l = 0;
                    break;
                }
            } else {
                if (LIT(u16, g_PadTrigger) & PSX_DPAD) {
                    if (LIT(u16, g_PadTrigger) & (PSX_RIGHT | PSX_LEFT)) {
                        p2 = &g_ItemMoveSrcCursor;
                        *p2 ^= 2;
                    }
                    if (LIT(u16, g_PadTrigger) & PSX_UP) {
                        p3 = &g_ItemMoveSrcCursor;
                        *p3 -= 4;
                        if (!(*p3 & 0xF8)) {
                            *p3 &= 2;
                            *p3 |= (g_TotalInventorySlots + 2) * 2;
                        }
                    } else if (LIT(u16, g_PadTrigger) & PSX_DOWN) {
                        p4 = &g_ItemMoveSrcCursor;
                        *p4 += 4;
                        if ((g_TotalInventorySlots + 3) * 2 < *p4) {
                            *p4 = (*p4 & 2) | 8;
                        }
                    }
                    g_MenuSubTimer_l = 0;
                    play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_MOVE, 0);
                }
            }
            p5 = &g_MenuSubTimer_l;
            (*p5)--;
            break;
        }
        g_ItemActionMenuState_l = 3;
        g_MenuSubTimer_l = 0;
        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
        /* fallthrough */
    case 3:
        p6 = &g_ItemMoveAnim_l[1];
        *p6 -= 4;
        g_ItemMoveAnim_l[2] -= 3;
        if (*p6 != 0) {
            break;
        }
        if (g_MenuActionResult_l != 0) {
            g_ItemActionMenuState_l = 4;
            set_message_display_i(LIT(u8, g_MenuActionResult) + 0xF0, 0);
            break;
        }
        if (g_MenuFlags & 8) {
            g_MenuFlags |= 2;
        } else {
            g_MenuFlags |= 4;
        }
        break;
    case 4:
        if (g_MessageStatus_l & 0x80) {
            break;
        }
        switch (g_MenuActionResult_l) {
        case 2:
            p7 = &g_MenuFlags;
            *p7 |= 0x40;
            return;
        case 3:
            p8 = &g_MenuFlags;
            *p8 |= 2;
            break;
        case 1:
            g_ItemActionMenuState_l = 3;
            break;
        case 4:
            menu_item_apply_combine_l();
            break;
        case 5:
            g_ItemActionMenuState_l = 2;
            break;
        }
        g_MenuActionResult_l = 0;
        break;
    }
    if (g_MenuActionResult_l == 0) {
        if (g_ItemActionMenuState_l != 3) {
            if (n < g_TotalHeldItems) {
                draw_item_name_p(g_ItemSlotsPointer_l[n * 2], 0, 0x30 - g_ScreenOffsetX_l, 0xBA - g_ScreenOffsetY);
            }
        } else {
            draw_item_name_p(g_ItemMenuSelectedItemId, 0, 0x30 - g_ScreenOffsetX_l, 0xBA - g_ScreenOffsetY);
        }
    }
    if (g_ItemActionMenuState_l != 4 || g_ItemMoveAnim_l[1] != 0) {
        LITP(GsSPRITE, sp_sprite)->attribute = 0x1000040;
        LITP(GsSPRITE, sp_sprite)->tpage = 0x1C;
        LITP(GsSPRITE, sp_sprite)->cy = 0x1E4;
        LITP(GsSPRITE, sp_sprite)->u = 0x6C;
        LITP(GsSPRITE, sp_sprite)->cx = 0;
        LITP(GsSPRITE, sp_sprite)->v = 0x60;
        if (!(g_MenuSubTimer_l & 0x20)) {
            LITP(GsSPRITE, sp_sprite)->u = 0x74;
        }
        LITP(GsSPRITE, sp_sprite)->w = 4;
        LITP(GsSPRITE, sp_sprite)->h = 4;
        LITP(GsSPRITE, sp_sprite)->x = g_InvSlotPosX_l[g_ItemMoveSrcCursor] + 0x12;
        LITP(GsSPRITE, sp_sprite)->y = y = g_InvSlotPosY_l[g_ItemMoveSrcCursor] - ({ g_ItemMoveAnim_l[2] - 0xD; });
        if (!(g_MenuCharacterFlags & 2)) {
            LITP(GsSPRITE, sp_sprite)->y = y + 0x1E;
        }
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
        LITP(GsSPRITE, sp_sprite)->v += 8;
        LITP(GsSPRITE, sp_sprite)->y += LIT(u8, g_ItemMoveAnim_OffsetY) * 2;
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
        LITP(GsSPRITE, sp_sprite)->v += 8;
        LITP(GsSPRITE, sp_sprite)->x -= g_ItemMoveAnim_l[1];
        y = g_InvSlotPosY_l[g_ItemMoveSrcCursor];
        LITP(GsSPRITE, sp_sprite)->y = y + 0xD;
        if (!(g_MenuCharacterFlags & 2)) {
            LITP(GsSPRITE, sp_sprite)->y = y + 0x2B;
        }
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
        LITP(GsSPRITE, sp_sprite)->v += 8;
        LITP(GsSPRITE, sp_sprite)->x += LIT(u8, g_ItemMoveAnim_OffsetX) * 2;
        GsSortFastSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), 4);
    }
}

s32 menu_item_check_combine(void) {
    extern void (*menu_item_combine_tbl_h10[])(u8, u8) __asm__("menu_item_combine_tbl");
    extern u8 *g_ItemSlotsPointer_h10 __asm__("g_ItemSlotsPointer");
    extern u8 g_RoomId_h10 __asm__("g_RoomId");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    void menu_item_combine_refresh(u8, u8, u8, u8);
    u8 *p;
    u8 *q;
    u8 x;
    u8 z;
    u8 a;
    u8 b;
    u8 n;
    u8 k;
    register s32 ret __asm__("$2"); /* pins the result to v0; a plain local gets v1 (dist 12, see 99d8_e.c) */

    x = (g_MenuWork.cursor >> 1) - 4;
    z = (g_ItemMoveSrcCursor >> 1) - 4;
    p = g_ItemCombinePtrs[g_ItemCombineIdx[({ (g_MenuWork.item_id - 1) * 4; })]];
    a = g_ItemSlotsPointer_h10[x * 2];
    b = g_ItemSlotsPointer_h10[z * 2];
    for (n = *p++; n != 0; n--, p += 4) {
        if (b != p[0]) {
            continue;
        }
        k = p[3];
        if (k >= 6) {
            if (k == 6) {
                q = &g_ItemSlotsPointer_h10[x * 2];
            } else {
                q = &g_ItemSlotsPointer_h10[z * 2];
            }
            if (INV_SLOT(q)->count != 0) {
                ret = 0;
                goto end;
            }
        }
        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
        if ((u8)(a - 0x13) < 8) {
            if (g_RoomId_h10 != 9) {
                ret = 1;
                goto end;
            }
            if (g_StageId != 3) {
                ret = 1;
                goto end;
            }
        } else if ((u8)(a - 0x43) < 9) {
            ret = 2;
            goto end;
        }
        p++;
        g_ItemSlotsPointer_h10[x * 2] = *p++;
        g_ItemSlotsPointer_h10[z * 2] = *p;
        if (p[1] != 0) {
            menu_item_combine_tbl_h10[p[1]](x, z);
        }
        menu_item_combine_refresh(x, z, a, b);
        rearrange_item_slots();
        ret = 4;
        goto end;
    }
    ret = 0;
    if ((u32)(a - 0x43) < 9) {
        ret = -((u32)(b - 0x43) < 9) & 3;
    }
end:
    return ret;
}

void menu_item_apply_combine(void) {
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    extern u8 g_MessageStatus_l __asm__("g_MessageStatus");
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    void menu_item_combine_refresh(u8, u8, u8, u8);
    u8 a;
    u8 b;
    u8 ca;
    u8 cb;
    u8 *p;
    u8 *tbl;

    if (g_MessageStatus_l != 0) {
        play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CANCEL, 0);
        LIT(u8, g_ItemActionMenuState) = 2;
        return;
    }
    play_sfx_l(SFX_BANK_SYSTEM, SE_MENU_CONFIRM, 0);
    LIT(u8, g_ItemActionMenuState) = 3;
    a = (LIT(u8, g_MenuCursor) >> 1) - 4;
    b = (LIT(u8, g_ItemMoveSrcCursor) >> 1) - 4;
    g_MenuFlags |= 8;
    tbl = g_ItemCombinePtrs[g_ItemCombineIdx[({ LIT(u8, g_ItemMenuSelectedItemId) - 1; }) * 4]];
    ca = g_ItemSlotsPointer_l[a * 2];
    cb = g_ItemSlotsPointer_l[b * 2];
    p = tbl + 1;
    while (cb != *p) {
        p += 4;
    }
    p++;
    g_ItemSlotsPointer_l[a * 2] = p[0];
    g_ItemSlotsPointer_l[b * 2] = p[1];
    menu_item_combine_refresh(a, b, ca, cb);
    rearrange_item_slots();
    menu_update_selected_item();
}

void menu_use_ammo_combine_a(s32 dst_slot, s32 src_slot) {
    extern s32 g_ItemMaxQty_s32[] __asm__("g_ItemMaxQty");
    s32 dst_ofs;
    s32 src_ofs;
    u8 max;
    u32 total;
    u32 dst_count;
    void *dst;

    dst_ofs = (dst_slot & 0xFF) << 1;
    src_ofs = (src_slot & 0xFF) << 1;
    dst = (void *)(dst_ofs + g_ItemSlotsPointer);
    dst_count = INV_SLOT(dst)->count & 0x7F;
    total = INV_SLOT((src_ofs + g_ItemSlotsPointer))->count + dst_count;
    max = g_ItemMaxQty_s32[({ INV_SLOT(dst)->item_id - 1; })];
    if (max < total) {
        INV_SLOT(dst)->count = max;
        INV_SLOT((src_ofs + g_ItemSlotsPointer))->count = total - max;
        return;
    }
    INV_SLOT(dst)->count = total;
    *(s8 *)(src_ofs + g_ItemSlotsPointer) = 0;
}

void menu_use_ammo_combine_b(s32 src_slot, s32 dst_slot) {
    extern s32 g_ItemMaxQty_s32[] __asm__("g_ItemMaxQty");
    s32 dst_ofs;
    s32 src_ofs;
    u8 max;
    u32 total;
    u32 dst_count;
    void *dst;

    dst_ofs = (dst_slot & 0xFF) << 1;
    src_ofs = (src_slot & 0xFF) << 1;
    dst = (void *)(dst_ofs + g_ItemSlotsPointer);
    dst_count = INV_SLOT(dst)->count & 0x7F;
    total = dst_count + INV_SLOT((src_ofs + g_ItemSlotsPointer))->count;
    max = g_ItemMaxQty_s32[({ INV_SLOT(dst)->item_id - 1; })];
    if (max < total) {
        INV_SLOT(dst)->count = max;
        INV_SLOT((src_ofs + g_ItemSlotsPointer))->count = total - max;
        return;
    }
    INV_SLOT(dst)->count = total;
    *(s8 *)(src_ofs + g_ItemSlotsPointer) = 0;
}

void menu_use_qty_merge(s32 dst_slot, s32 src_slot) {
    s32 src_ofs;
    u32 total;
    void *dst;

    dst = (void *)(((dst_slot & 0xFF) * 2) + g_ItemSlotsPointer);
    src_ofs = (src_slot & 0xFF) << 1;
    total = INV_SLOT((src_ofs + g_ItemSlotsPointer))->count + INV_SLOT(dst)->count;
    if (total >= 0xFBU) {
        INV_SLOT(dst)->count = 0xFAU;
        INV_SLOT((src_ofs + g_ItemSlotsPointer))->count = (s8) (total + 6);
        return;
    }
    INV_SLOT(dst)->count = total;
    *(s8 *)(src_ofs + g_ItemSlotsPointer) = 0;
}

void menu_use_set_flag(void) {
    g_MenuWork.action_result = 2;
    Flg_on(ADDR_g_ScenarioFlags, 0x16);
}

void menu_use_set_mode(void) {
    g_MenuWork.action_result = 3;
}

void menu_use_ammo_transfer_a(s32 dst_slot, s32 src_slot) {
  s32 src_ofs;
  u8 max;
  u8 src_count;
  int one;
  void *dst;
  void *src;
  src_ofs = (src_slot & 0xFF) * 2;
  dst = (void *)(((dst_slot & 0xFF) * 2) + g_ItemSlotsPointer);
  one = 1;
  src_count = *((u8 *) (((s8 *) (src_ofs + g_ItemSlotsPointer)) + one));
  max = ((s32 *) (&g_ItemMaxQty))[(*((u8 *) (((s8 *) dst) + 0))) - one];
  if (max < src_count)
  {
    *((u8 *) (((s8 *) dst) + one)) = max;
    src = (void *)(src_ofs + g_ItemSlotsPointer);
    if (one)
    {
      *((u8 *) (((s8 *) src) + one)) = (u8) ((*((u8 *) (((s8 *) src) + 1))) - max);
      return;
    }
  }
  *((u8 *) (((s8 *) dst) + 1)) = src_count;
  *((s8 *) (src_ofs + g_ItemSlotsPointer)) = 0;
}

void menu_use_ammo_transfer_b(s32 src_slot, s32 dst_slot) {
    extern s32 g_ItemMaxQty_s32[] __asm__("g_ItemMaxQty");
    s32 src_ofs;
    u8 max;
    u8 src_count;
    void *dst;
    void *src;

    src_ofs = (src_slot & 0xFF) << 1;
    dst = (void *)(((dst_slot & 0xFF) * 2) + g_ItemSlotsPointer);
    src_count = INV_SLOT((src_ofs + g_ItemSlotsPointer))->count;
    max = g_ItemMaxQty_s32[({ INV_SLOT(dst)->item_id - 1; })];
    if (max < src_count) {
        INV_SLOT(dst)->count = max;
        src = (void *)(src_ofs + g_ItemSlotsPointer);
        INV_SLOT(src)->count = (u8) (INV_SLOT(src)->count - max);
        return;
    }
    INV_SLOT(dst)->count = src_count;
    *(s8 *)(src_ofs + g_ItemSlotsPointer) = 0;
}

void menu_item_combine_refresh(u8 slot_a, u8 slot_b, u8 old_a, u8 old_b) {
    extern s8 g_MenuModelLoaded_s8[] __asm__("g_MenuModelLoaded");
    void load_file_blocking_l(s32, void *) __asm__("load_file_blocking");
    u8 *p;
    u8 *q;
    u8 a;
    u8 b;
    u8 t;

    a = 0;
    p = (u8 *)(g_ItemSlotsPointer + (slot_a) * 2);
    b = 0;
    t = *p;
    if (t != 0 && t != (old_a)) {
        a = g_ItemImageTypeTable[g_ItemCombineIdx[(*p - 1) << 2]];
    }
    q = (u8 *)(g_ItemSlotsPointer + (slot_b) * 2);
    t = *q;
    if (t != 0 && t != (old_b)) {
        b = g_ItemImageTypeTable[g_ItemCombineIdx[(*q - 1) << 2]];
    }
    if (((g_MenuModelLoaded_s8[0] + 1) & ~0x80) && (a | b)) {
        g_MenuModelLoaded_s8[0] |= 0x7F;
        load_file_blocking_l(0xF, LITP(void, g_DoorFile));
    }
    if (a) {
        item_icon_upload(a - 1, g_ItemSlotIconIndex[slot_a & 0xFF], LITP(void, g_DoorFile));
    }
    if (b) {
        item_icon_upload(b - 1, g_ItemSlotIconIndex[slot_b & 0xFF], LITP(void, g_DoorFile));
    }
}

void load_texture_from_table(void) {
    s32 load_file_c4(u16, s32) __asm__("load_file");
    load_file_c4(((s16 *)&g_RoomTexFiles)[g_DoorTexIndex], ADDR_g_DoorFile);
    g_VramTpageCursor = 0x1F15;
    tim_load_to_vram(g_DoorFileTim + ADDR_g_DoorFile);
}

void mem_copy(u8 *dst, u8 *src, s32 n) {
    while (n-- > 0) {
        *dst++ = *src++;
    }
}

void vram_clear_rect(s16 x, s16 y, s16 w, s16 h) {
    void ClearImage(RECT *rect, u_char r, u_char g, u_char b);
    s16 area[4];

    area[0] = x;
    area[1] = y;
    area[2] = w;
    area[3] = h;
    ClearImage((RECT *) area, 0U, 0U, 0U);
}
