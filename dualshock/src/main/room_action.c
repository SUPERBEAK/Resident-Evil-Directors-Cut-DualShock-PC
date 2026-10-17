/* room_action.c - Room sprites and room actions: the RDT's action zones (doors, items, typewriters, item boxes,
   stairs, water, events) and their per-frame checks; room objects. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void display_room_camera_bg(void);
s32 is_point_in_quad(s32 *pos, u16 *quad);
s32 memcard_screen(s32 is_load, s32 save_buf, s32 use_ribbon, u8 sfx_bank);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
void scd_event_entry_create(u32 slot, s32 script);
void fade_update(void);
void update_entity_lighting(s32 *pos);
u32 get_item_slot(s32 item_id);
void map_set_area_known(s32 map_id);
void use_room_action_item(void);
s32 set_message_display(u8 msg, s16 msg_id);



void room_load_camera_sprites(void) {
    typedef struct {
        s32 attr;
        s16 x, y;
        u16 w, h;
        u16 tpage;
        u8 u, v;
        u16 cx, cy;
        u8 pad14[0xC];
        u8 active, group;
        u16 depth;
    } RoomSpriteView;
    typedef struct { u32 *hdr; void *tim_mask; u8 pad[0x24]; } RdtCameraView;
    typedef struct { u8 pad[0x94]; RdtCameraView rm[1]; } Ctx;
    extern Ctx *g_RoomData_ctx __asm__("g_RoomData");
    extern RoomSpriteView g_RoomSprites_l[] __asm__("g_RoomSprites");
    extern u8 g_VramTpageCursor_l __asm__("g_VramTpageCursor");
    extern u8 g_VramClutRowCursor_l __asm__("g_VramClutRowCursor");
    void tim_load_to_vram_l() __asm__("tim_load_to_vram");
    u8 *hdr;
    u16 *grp;
    u8 *q;
    u16 count;
    u32 i;
    u32 j;
    u16 f;
    u16 t;

    count = 0;
    hdr = (u8 *)g_RoomData_ctx->rm[g_CutId].hdr;
    grp = (u16 *)(hdr + 4);
    q = (u8 *)grp + *(u32 *)hdr * 8;
    for (i = 0; i < *g_RoomData_ctx->rm[g_CutId].hdr; grp += 4, i++) {
        for (j = 0; j < grp[0]; j++) {
            g_RoomSprites_l[count].active = 1;
            g_RoomSprites_l[count].group = i + 1;
            g_RoomSprites_l[count].cx = (grp[1] & 0x3F) << 4;
            g_RoomSprites_l[count].cy = g_VramClutRowCursor_l + 0x1E0;
            g_RoomSprites_l[count].u = ROOM_SPR(q)->u;
            g_RoomSprites_l[count].v = ROOM_SPR(q)->v;
            g_RoomSprites_l[count].x = ROOM_SPR(q)->x + grp[2] - 0xA0;
            g_RoomSprites_l[count].y = ROOM_SPR(q)->y + grp[3] - 0x78;
            g_RoomSprites_l[count].depth = ROOM_SPR(q)->depth;
            f = ROOM_SPR(q)->attr;
            g_RoomSprites_l[count].tpage = g_VramTpageCursor_l + (f & 0x1F);
            if (!(f & 0xF000)) {
                g_RoomSprites_l[count].w = ROOM_SPR(q)->w;
                g_RoomSprites_l[count].h = ROOM_SPR(q)->h;
                q += 12;
            } else {
                q += 8;
                t = (f >> 12) * 8;
                g_RoomSprites_l[count].w = t;
                g_RoomSprites_l[count].h = t;
            }
            g_RoomSprites_l[count].attr |= 0x40;
            if (!(f & 0xC00)) {
                g_RoomSprites_l[count].attr |= 0x08000000;
            }
            g_RoomSprites_l[count].attr |= (((f >> 7) & 3) << 24);
            g_RoomSprites_l[count].attr |= (((f >> 5) & 3) << 28);
            count++;
        }
    }
    *LIT(u8 *, g_RoomData) = count;
    if (count) {
        tim_load_to_vram_l(LIT(Ctx *, g_RoomData)->rm[g_CutId].tim_mask);
    }
}

void draw_room_spr(void) {
    extern u8 g_RoomSpriteActive_u8 __asm__("g_RoomSpriteActive");
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 unused[2];
    s32 off;
    u32 z;
    u32 i;

    if (!(g_StatusFlags & STATUS_ROOM_SPRITES_OFF)) {
        i = 0;
        if (*g_RoomData_u8p != 0) {
            off = 0;
            do {
                if (*((u8 *)&g_RoomSpriteActive_u8 + off) != 0) {
                    z = *(u16 *)((u8 *)&g_RoomSpriteDepth + off) >> 2;
                    GsSortFastSprite((GsSPRITE *)((u8 *)&g_RoomSprites + off), &LITP(GsOT, g_OtMain)[g_ActiveBuffer], z >= 0x400 ? 0x3FF : z);
                }
                off += 0x24;
                i++;
            } while (i < *g_RoomData_u8p);
        }
    }
}

typedef struct { u8 active; u8 sprite_id; u8 pad[0x22]; } RoomSpriteState;   /* g_RoomSpriteActive[] */

void room_spr_set_inactive(u8 sprite_id) {
    extern RoomSpriteState g_RoomSpriteActive[];
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    s32 unused[2];
    u32 i;

    i = 0;
    if (*g_RoomData_u8p != 0) {
        do {
            if (g_RoomSpriteActive[i].sprite_id == sprite_id) {
                g_RoomSpriteActive[i].active = 0;
            }
            i++;
        } while (i < *LIT(u8 *, g_RoomData));
    }
}

void room_spr_set_active(u8 sprite_id) {
    extern RoomSpriteState g_RoomSpriteActive_r[] __asm__("g_RoomSpriteActive");
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    s32 unused[2];
    u32 i;

    i = 0;
    if (*g_RoomData_u8p != 0) {
        do {
            if (g_RoomSpriteActive_r[i].sprite_id == sprite_id) {
                g_RoomSpriteActive_r[i].active = 1;
            }
            i++;
        } while (i < *LIT(u8 *, g_RoomData));
    }
}

s32 check_entity_collision(u8 *a, u8 *b) {
    s16 *ap;
    s16 *bp;
    u8 *ar;
    u8 *br;
    s32 dx;
    s32 dz;
    s32 d;
    s32 r;
    s32 ay;
    s32 dy;
    s32 h;
    s32 mx;
    s32 mz;
    s32 hit;
    s16 by;

    hit = 0;
    if (ENT(b)->state == 4 || ((ENT(a)->status_flags | ENT(b)->status_flags) & 2)) {
        return 0;
    }
    ar = ((u8 *) ENT(a)->sca_info);
    ap = ((s16 *) ENT(a)->sca_hit_data);
    for (;;) {
        bp = ((s16 *) ENT(b)->sca_hit_data);
        br = ((u8 *) ENT(b)->sca_info);
        for (;;) {
            dx = (bp[0] + ENT(b)->pos_x) - (ap[0] + ENT(a)->pos_x);
            dz = (bp[2] + ENT(b)->pos_z) - (ap[2] + ENT(a)->pos_z);
            d = SCA(ar)->radius + SCA(br)->radius;
            r = SquareRoot0(dx * dx + dz * dz) + 1;
            d -= r;
            if (d > 0) {
                by = bp[1];
                dy = (by + ENT(b)->pos_y) - (ay = ap[1] + ENT(a)->pos_y);
                h = SCA(br)->half_height + SCA(ar)->half_height;
                if (-h < dy && dy < h) {
                    mx = dx * d / r;
                    mz = dz * d / r;
                    dy = (((s16) ENT(b)->position_vy) + by) - ay;
                    if (!(-h < dy && dy < h)) {
                        if ((((s16) ENT(b)->position_vx) < ENT(a)->pos_x && ENT(a)->pos_x < ENT(b)->pos_x) ||
                            (ENT(a)->pos_x < ((s16) ENT(b)->position_vx) && ENT(b)->pos_x < ENT(a)->pos_x)) {
                            mx = -(dx * d) / r;
                            if (mx > 0) {
                                mx = SCA(ar)->radius * 2 - mx;
                            } else {
                                mx = -(SCA(ar)->radius * 2) - mx;
                            }
                        }
                        if ((((s16) ENT(b)->position_vz) < ENT(a)->pos_z && ENT(a)->pos_z < ENT(b)->pos_z) ||
                            (ENT(a)->pos_z < ((s16) ENT(b)->position_vz) && ENT(b)->pos_z < ENT(a)->pos_z)) {
                            mz = -(dz * d) / r;
                            if (mz > 0) {
                                mz = SCA(ar)->radius * 2 - mz;
                            } else {
                                mz = -(SCA(ar)->radius * 2) - mz;
                            }
                        }
                    }
                    hit = 1;
                    ENT(b)->pos_x += mx;
                    ENT(b)->pos_z += mz;
                }
            }
            bp += 3;
            if (*(s16 *)br < 0) {
                break;
            }
            br += 12;
        }
        ap += 3;
        if (*(s16 *)ar < 0) {
            break;
        }
        ar += 12;
    }
    return hit;
}

void entity_rotate_collision_points(u8 *obj) {
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    s16 *src;
    s16 *dst;

    src = ((s16 *) ENT(obj)->sca_info);
    dst = ((s16 *) ENT(obj)->sca_hit_data);

    SCRATCH->sv_y = ((s16) ENT(obj)->angle);
    for (;;) {
        SCRATCH->sv_z = 0;
        LIT(s16, sp_svec_vx) = 0;
        RotMatrix(LITP(SVECTOR, sp_svec), LITP(MATRIX, sp_matrix));
        LIT(s16, sp_svec_vx) = src[1];
        SCRATCH->sv_z = src[3];
        ApplyMatrix(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(VECTOR, sp_vec));
        dst[0] = SCRATCH_AS(vec_x, s16);
        dst[1] = src[2];
        dst[2] = SCRATCH_AS(vec_z, s16);
        if (*src < 0) {
            break;
        }
        dst += 3;
        src += 6;
    }
}

void rotate_vector_y(s16 angle, SVECTOR *src, void *dst) {
    MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    s16 rot[3];

    rot[1] = angle;
    rot[2] = 0;
    rot[0] = 0;
    RotMatrix((SVECTOR *) rot, LITP(MATRIX, sp_matrix));
    ApplyMatrix(LITP(MATRIX, sp_matrix), src, LITP(VECTOR, sp_vec));
    FIELD_AT(dst, SVECTOR, ->vx, u16) = (u16) LIT(u16, sp_vec_vx);
    FIELD_AT(dst, SVECTOR, ->vy, u16) = (u16) LIT(u16, sp_vec_vy);
    FIELD_AT(dst, SVECTOR, ->vz, u16) = (u16) LIT(u16, sp_vec_vz);
}

s32 is_point_in_action_zone(s32 *pos, u16 *zone) {
    if (zone[2] < (u32)(pos[0] - zone[0]) || zone[3] < (u32)(pos[2] - zone[1])) {
        return 0;
    }
    return 1;
}

s32 door_try_enter(void *act) {
    extern u8 g_RoomId_l __asm__("g_RoomId");
    extern s16 g_UpdateFlags_l __asm__("g_UpdateFlags");
    s32 get_item_slot_l() __asm__("get_item_slot");
    void set_message_display_l() __asm__("set_message_display");
    void Flg_on_l() __asm__("Flg_on");
    s32 Flg_ck_l() __asm__("Flg_ck");
    void play_sfx_l() __asm__("play_sfx");
    u32 id;
    u8 flags;

    if (g_GameStateFlags & GAME_ON_BOX) {
        return 0;
    }
    if ((SCD_DOOR(ROOM_ACTION(act)->rec)->lock & 0x40) && (g_PlayerEntity.id & 3) == 3) {
        set_message_display_l(0xD6, 0xFF);
        return 0;
    }
    flags = SCD_DOOR(ROOM_ACTION(act)->rec)->lock;
    if ((flags & 0x80) && Flg_ck_l(ADDR_g_LocksFlags, flags & 0x3F) == 0) {
        id = SCD_DOOR(ROOM_ACTION(act)->rec)->key_item;
        if (id == 0x33 && (g_PlayerEntity.id & 3) == 1) {
            if (Flg_ck_l(ADDR_g_ScenarioFlags, 0x7C) != 0) {
                g_SelectedItemId = 0xD;
                goto play;
            }
            id = 0xD;
            goto sound;
        }
        if (id == 0xFE) {
            set_message_display_l(0xD4, 0xFF);
            play_sfx_l(2, 0x21, 0);
            Flg_on_l(ADDR_g_LocksFlags, SCD_DOOR(ROOM_ACTION(act)->rec)->lock & 0x3F);
            goto ret0;
        }
        if (id == 0xFF) {
            id = 0xB;
            goto sound;
        }
        if (get_item_slot_l(id) >= 0) {
            g_EventItemUsedFlag = 1;
            g_SelectedItemId = id;
play:
            set_message_display_l(0xC3, 0xFF);
            if (g_StageId == 4 && g_RoomId_l == 5) {
                play_sfx_l(4, 0x17, 0);
                play_sfx_l(4, 0x18, 0);
                play_sfx_l(4, 0x19, 0);
            } else {
                play_sfx_l(2, 0x22, 0);
            }
            Flg_on_l(ADDR_g_LocksFlags, SCD_DOOR(ROOM_ACTION(act)->rec)->lock & 0x3F);
            goto ret0;
        }
        id -= 0x33;
        if (id >= 10) {
            id = 10;
        }
sound:
        play_sfx_l(2, 0x14, 0);
        set_message_display_l(id + 0xC8, 0xFF);
ret0: /* shared "return 0": reorg steals v0=0 from this jump target (no USE marker) */
        return 0;
    }
    {
        void *p = ROOM_ACTION(act)->rec;
        g_OpenMenuFlag = 1;
        g_UpdateFlags_l = 0;
        g_DoorRecord = p;
    }
    LIT(s32, g_GameStateFlags) |= GAME_ROOM_CHANGE;
    return 0;
}

s32 display_msg_room_action(void *act) {
    s32 set_message_display_a5(u16, u16) __asm__("set_message_display");
    set_message_display_a5(ROOM_ACTION(act)->arg0, ROOM_ACTION(act)->arg1);
    return 0;
}

s32 include_key(void *act) {
    s32 set_message_display_a6(s32, s32) __asm__("set_message_display");
    if ((g_EquippedSlot == 0) || (((InventorySlot *)((g_EquippedSlot * 2) + g_ItemSlotsPointer))[-1].item_id != SCD_ZONE(ROOM_ACTION(act)->rec)->item_id)) {
        g_pRoomActionEntry = act;
        set_message_display_a6(0xC1, 0xFF);
    }
    return 0;
}

s32 set_key_flag(void *act) {
    g_pRoomActionEntry = act;
    if (ROOM_ACTION(act)->arg0 == 0) {
        LIT(s32, g_GameStateFlags) |= MENU_REQUEST(MENU_MODE_PICKUP);
        return 0;
    }
    return 1;
}

s32 room_action_stairs_zone_x(void *act) {
    void *e;
    void *e2;
    void *zone;

    zone = ROOM_ACTION(act)->rec;
    if ((g_CurrentEntity->pos_x - SCD_ZONE(zone)->x) < (s32) ((u16) SCD_ZONE(zone)->w >> 1)) {
        if ((ENTITY_FIELD_AS(g_CurrentEntity, angle, s16) + 0x400) & 0x800) {
            goto end;
        }
        ENTITY_FIELD_AS(g_CurrentEntity, action_ticks_counter, s16) = 1;
    } else {
        if ((ENTITY_FIELD_AS(g_CurrentEntity, angle, s16) - 0x400) & 0x800) {
            goto end;
        }
        ENTITY_FIELD_AS(g_CurrentEntity, action_ticks_counter, s16) = -1;
    }
    CURRENT_ENTITY_LIT->sub_state = 0;
    e = LIT(void *, g_CurrentEntity);
    LIT(u16, g_UpdateFlags) &= 0xFFBF;
    ENT(e)->has_enter_switch_zone = (u8) (ENT(e)->has_enter_switch_zone & 0xEF);
    e2 = LIT(void *, g_CurrentEntity);
    if ((((s32) (ENT(e2)->action_ticks_counter << 0x10) >> 0x11) ^ ROOM_ACTION(act)->arg0) & 1) {
        ENT(e2)->has_enter_switch_zone = (u8) (ENT(e2)->has_enter_switch_zone | 0x10);
    }
end:
    g_CurrentEntity->has_enter_switch_zone = (u8) (g_CurrentEntity->has_enter_switch_zone | 0x20);
    LIT(s32, g_StatusFlags) |= STATUS_STAIRS_ZONE;
    return 0;
}

s32 room_action_stairs_zone_z(void *act) {
    void *e;
    void *e2;
    void *zone;

    zone = ROOM_ACTION(act)->rec;
    if ((g_CurrentEntity->pos_z - SCD_ZONE(zone)->z) < (s32) ((u16) SCD_ZONE(zone)->d >> 1)) {
        if ((ENTITY_FIELD_AS(g_CurrentEntity, angle, s16) + 0x800) & 0x800) {
            goto end;
        }
        ENTITY_FIELD_AS(g_CurrentEntity, action_ticks_counter, s16) = 1;
    } else {
        if ((u16) ENTITY_FIELD_AS(g_CurrentEntity, angle, s16) & 0x800) {
            goto end;
        }
        ENTITY_FIELD_AS(g_CurrentEntity, action_ticks_counter, s16) = -1;
    }
    CURRENT_ENTITY_LIT->sub_state = 0;
    e = LIT(void *, g_CurrentEntity);
    LIT(u16, g_UpdateFlags) &= 0xFFBF;
    ENT(e)->has_enter_switch_zone = (u8) (ENT(e)->has_enter_switch_zone & 0xEF);
    e2 = LIT(void *, g_CurrentEntity);
    if ((((s32) (ENT(e2)->action_ticks_counter << 0x10) >> 0x11) ^ ROOM_ACTION(act)->arg0) & 1) {
        ENT(e2)->has_enter_switch_zone = (u8) (ENT(e2)->has_enter_switch_zone | 0x10);
    }
end:
    g_CurrentEntity->has_enter_switch_zone = (u8) (g_CurrentEntity->has_enter_switch_zone | 0x60);
    LIT(s32, g_StatusFlags) |= STATUS_STAIRS_ZONE;
    return 0;
}

s32 flag_bank_set(u16 *cmd, u32 *p) {
    extern u32 g_ItemUseFlags_u32[] __asm__("g_ItemUseFlags");
    u32 v;

    switch (cmd[1]) {
    case 0:
        p = LITP(u32, g_ScenarioFlags) + (cmd[2] >> 5);
        break;
    case 1:
        p = LITP(u32, g_ScenarioFlags2) + (cmd[2] >> 5);
        break;
    case 2:
        p = LITP(u32, g_LocksFlags) + (cmd[2] >> 5);
        break;
    case 3:
        p = LITP(u32, g_EnemiesFlags) + (cmd[2] >> 5);
        break;
    case 4:
        p = LITP(u32, g_SysFlags) + (cmd[2] >> 5);
        break;
    case 5:
        p = LITP(u32, g_GameStateFlags);
        break;
    case 6:
        p = LITP(u32, g_UpdateFlags);
        break;
    case 7:
        p = LITP(u32, g_RoomItemsFlags) + (cmd[2] >> 5);
        break;
    case 8:
        p = LITP(u32, g_RoomFlags) + (cmd[2] >> 5);
        break;
    case 9:
        p = g_ItemUseFlags_u32 + (cmd[2] >> 5);
        break;
    }
    if (cmd[3] != 0) {
        v = (0x80000000U >> (cmd[2] & 0x1F)) | *p;
    } else {
        v = ~(0x80000000U >> (cmd[2] & 0x1F)) & *p;
    }
    *p = v;
    return 0;
}

s32 open_itembox(s32 act) {
    u16 msg_flags;

    if (LIT(u8, g_ItemboxState) == 0) {
        if ((((u8) g_PlayerEntity.hit_state) == 0) && !(g_GameStateFlags & GAME_MENU_REQUESTS)) {
            msg_flags = LIT(u16, g_UpdateFlags);
            if (msg_flags & 0x40) {
                LIT(u8, g_ItemboxState) = 1;
                LIT(u16, g_UpdateFlags) = msg_flags & 0xFFBA;
                play_sfx(2, 0x20, 0);
                g_pRoomActionEntry = (void *)act;
            }
        }
    }
    return 0;
}

s32 create_room_event(void *act) {
    s32 scd_event_entry_create_a2(u8, u8) __asm__("scd_event_entry_create");
    scd_event_entry_create_a2(FIELD_AT(act, RoomAction, ->arg0, u8), FIELD_AT(act, RoomAction, ->arg1, u8));
    return 0;
}

s32 room_action_0a_noop(void) {
    return 0;
}

s32 room_action_water_zone(void) {
    s32 effect_create_billboard_a13(s32, s32, s32, s32, s32, s32) __asm__("effect_create_billboard");
    s32 next;
    s32 ofs;

    LIT(s32, g_StatusFlags) |= STATUS_IN_WATER;
    if ((g_PlayerEntity.move_speed_current > 0) && (g_ActiveBuffer != 0)) {
        ofs = g_RoomActionFxIndex * 4;
        LIT(s32, sp_vec_vy) = ENT(g_OmodelTable)->pos_y - g_PlayerEntity.pos_y;
        LIT(s32, sp_vec_vx) = *(s32 *)((u8 *)&g_RoomActionFxX + (ofs));
        LIT(s32, sp_vec_vz) = *(s32 *)((u8 *)&g_RoomActionFxZ + (ofs));
        effect_create_billboard_a13(0x17, 8, 0, ADDR_g_PlayerCoordM00, ADDR_sp_vec, 0);
        next = g_RoomActionFxIndex + 1;
        g_RoomActionFxIndex = next;
        if (next >= 6) {
            g_RoomActionFxIndex = 0;
        }
    }
    return 0;
}

s32 set_ladder_zone(void *act) {
    u8 zone_flags;

    zone_flags = g_PlayerEntity.has_enter_switch_zone;
    g_PlayerHasEnterSwitchZone = zone_flags | 0x20;
    if (FIELD_AT(act, RoomAction, ->arg0, u16) != 0) {
        g_PlayerHasEnterSwitchZone = zone_flags | 0x30;
    }
    g_PlayerEntity.anim_base_x = FIELD_AT(act, RoomAction, ->arg1, u16);
    g_PlayerEntity.anim_base_z = FIELD_AT(act, RoomAction, ->arg2, u16);
    FIELD_AT(act, RoomAction, ->arg0, u16) = (u16) (FIELD_AT(act, RoomAction, ->arg0, u16) ^ 1);
    LIT(s32, g_GameStateFlags) |= GAME_LADDER;
    return 0;
}

s32 set_room_event_flag(u16 *act) {
    g_pRoomActionEntry = act;
    if (act[1] == 0) {
        LIT(s32, g_GameStateFlags) |= GAME_FILE_PICKUP;
        return 0;
    }
    return 1;
}

s32 check_desk(void *act) {
    extern RoomAction g_RoomActionTable_l[] __asm__("g_RoomActionTable");
    s32 get_item_slot_l() __asm__("get_item_slot");
    void set_message_display_l() __asm__("set_message_display");
  s32 msg;
  u8 *item_obj;
  int not_first;
  u8 cut;
  void *zones;
  void *zone;
  if ((LIT(u8, g_DeskCheckState)) == 0)
  {
    if (((((u8) g_PlayerEntity.hit_state) == 0) && (!(g_GameStateFlags & GAME_MENU_REQUESTS))) && (g_UpdateFlags & 0x40))
    {
      if (Flg_ck(ADDR_g_RoomItemsFlags, g_RoomActionTable_l[ROOM_ACTION(act)->arg1].arg2) != 0)
      {
        if ((g_PlayerEntity.id & 3) == 3)
        {
          msg = 0xD7;
          goto block_12;
        }
        if (Flg_ck(ADDR_g_LocksFlags, *((u16 *) (((s8 *) act) + 2))) == 0)
        {
          if ((get_item_slot_l(0x3D) >= 0) || (Flg_ck(ADDR_g_ScenarioFlags, 0x7CU) != 0))
          {
            g_pRoomActionEntry = act;
            LIT(u8, g_DeskCheckState) = 1;
            goto block_17;
          }
          msg = 0xD8;
          block_12:
          set_message_display_l(msg, 0xFF);

          return 0;
        }
        item_obj = (u8 *)((s32 *) (&g_ItemModelTable))[g_RoomActionTable_l[ROOM_ACTION(act)->arg1].arg1];
        g_pRoomActionEntry = act;
        *item_obj |= 1;
        play_sfx(2, 0x24, 0);
        g_SavedCutId = LIT(u8, g_CutId);
        LIT(u8, g_CutId) = *((u8 *) (((s8 *) act) + 6));
        LIT(s8, g_DeskCheckState) = 0x23;
        zones = *((void **) (((s8 *) g_RoomData) + 0x48));
        cut = LIT(volatile u8, g_CutId);
        not_first = (*((u16 *) (((s8 *) zones) + 2))) != cut;
        g_CameraSwitchZones = zones;
        if (not_first)
        {
          do
          {
            zone = LIT(void *, g_CameraSwitchZones);
            LIT(void *, g_CameraSwitchZones) = zone;
            LIT(void *, g_CameraSwitchZones) = (LIT(void *, g_CameraSwitchZones)) + 0x14;
          }
          while ((*((u16 *) (((s8 *) zone) + 0x16))) != cut);
        }
        LIT(u16, g_UpdateFlags) &= 0xFFBA;
        goto block_17;
      }
      goto block_17;
    }
    return 0;
  }
  block_17:
  return 0;

}

s32 check_desk_state(void) {
    extern u8 * g_ItemModelTable_u8p[] __asm__("g_ItemModelTable");
    extern u8 * g_pRoomActionEntry_u8p __asm__("g_pRoomActionEntry");
    extern u8 g_RoomActionTable_u8[] __asm__("g_RoomActionTable");
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");
    void display_room_camera_bg_l() __asm__("display_room_camera_bg");
    s32 get_item_slot_l() __asm__("get_item_slot");
    void set_message_display_l() __asm__("set_message_display");
    void Flg_on_l() __asm__("Flg_on");
    s32 Flg_ck_l() __asm__("Flg_ck");
    void play_sfx_l() __asm__("play_sfx");
    u8 *p;
    s32 v;

    switch (g_DeskCheckState) {
    case 0:
        break;
    case 1:
    case 2:
        g_DeskItemSlot = get_item_slot_l(0x3D);
        v = Flg_ck_l(ADDR_g_ScenarioFlags, 0x7C);
        if (v == 0) {
            v = 0x3D;
        } else {
            v = 0xD;
        }
        do { /* scheduling barrier */
            g_SelectedItemId = v;
        } while (0);
        set_message_display_l(0xD9, 0xFF);
        g_DeskCheckState = 3;
        break;
    case 3:
        if (!(g_MessageStatus_u8 & 0x80)) {
            if (!(g_MessageStatus_u8 & 1)) {
                Flg_on_l(ADDR_g_LocksFlags, ROOM_ACTION(g_pRoomActionEntry_u8p)->arg0);
                play_sfx_l(2, 0x26, 0);
                v = Flg_ck_l(ADDR_g_ScenarioFlags, 0x7C);
                if (v == 0) {
                    v = 0x3D;
                } else {
                    v = 0xD;
                }
                g_SelectedItemId = v;
                set_message_display_l(0xC3, 0xFF);
            }
            g_DeskCheckState = 0;
        }
        break;
    case 4:
        display_room_camera_bg_l();
        p = g_ItemModelTable_u8p[ROOM_ACTION(g_pRoomActionEntry_u8p)->arg1];
        g_DeskCheckState = 0;
        *p &= 0xFE;
        break;
    case 5:
        g_DeskCheckState = 4;
        g_pRoomActionEntry_u8p = g_RoomActionTable_u8 + ROOM_ACTION(g_pRoomActionEntry_u8p)->arg1 * 12;
        LIT(s32, g_GameStateFlags) |= MENU_REQUEST(MENU_MODE_PICKUP);
        g_CutId = g_SavedCutId;
        LIT(u16, g_UpdateFlags) |= 0x45;
        break;
    case 35:
        display_room_camera_bg_l();
    default:
        LIT(u8, g_DeskCheckState)--;
        break;
    }
}

s32 pickup_key_event(void *act) {
    s32 Flg_off();
    extern s8 g_ItemModelTable_s8 __asm__("g_ItemModelTable");
    ROOM_ACTION(act)->type = 0;
    **(s8 **)(&g_ItemModelTable_s8 + (ROOM_ACTION(act)->arg1 * 4)) = 0;
    Flg_off(ADDR_g_RoomItemsFlags, SCD_ZONE(ROOM_ACTION(act)->rec)->picked_flag);
    map_set_area_known(SCD_ZONE(ROOM_ACTION(act)->rec)->item_id - 0x4E);
    g_PickedItemId = SCD_ZONE(ROOM_ACTION(act)->rec)->item_id;
    return 0;
}

s32 check_typewriter(void *act) {
    u32 get_item_slot(s32);
    s32 set_message_display_a7(s32, s32) __asm__("set_message_display");
    s32 slot;

    if (LIT(u8, g_TypewriterState) == 0) {
        if (!(g_GameStateFlags & GAME_MENU_REQUESTS) && (LIT(u16, g_UpdateFlags) & 0x40)) {
            slot = get_item_slot(0x2F);
            if (slot >= 0) {
                g_pRoomActionEntry = act;
                ROOM_ACTION(act)->arg0 = slot;
                LIT(u8, g_TypewriterState) = 1;
                LIT(u16, g_UpdateFlags) &= 0xFFBA;
            } else {
                set_message_display_a7(0xDE, 0xFF);
                LIT(u8, g_TypewriterState) = 0;
            }
        }
    }
    return 0;
}

/* Room action type 17: a slope (ramp) zone, applied while the player walks in it (zone flag 0x40). The floor
   height rises in small steps: arg2 units every arg1 units walked from the zone edge arg0 (0/1 = -x/+x side,
   2/3 = -z/+z). Only in stage 4 room 0x0D (the corridor below the guardhouse: 36 every 175). */
s32 slope_height_update(void *act, s32 dist_in) {
    s32 unused;
    s32 dist;
    s32 unused2;
    u16 dir;
    void *zone;
    void *zone2;

    dist = dist_in;
    dir = ROOM_ACTION(act)->arg0;
    switch (dir) {
    case 0:
        dist = g_PlayerEntity.pos_x - SCD_ZONE(ROOM_ACTION(act)->rec)->x;
        break;
    case 1:
        zone = ROOM_ACTION(act)->rec;
        dist = (SCD_ZONE(zone)->x + SCD_ZONE(zone)->w) - g_PlayerEntity.pos_x;
        break;
    case 2:
        dist = g_PlayerEntity.pos_z - SCD_ZONE(ROOM_ACTION(act)->rec)->z;
        break;
    case 3:
        zone2 = ROOM_ACTION(act)->rec;
        dist = (SCD_ZONE(zone2)->z + SCD_ZONE(zone2)->d) - g_PlayerEntity.pos_z;
        break;
    }
    g_PlayerEntity.pos_y = ((s16) (dist / (s32) ROOM_ACTION(act)->arg1) + 1) * ((s16) ROOM_ACTION(act)->arg2);
    g_PlayerEntity.floor_y = ((s16) (dist / (s32) ROOM_ACTION(act)->arg1) + 1) * ((s16) ROOM_ACTION(act)->arg2);
    return 0;
}

/* one entry of the room action table (g_RoomActionTable): handler type, flags, data */
typedef struct {
    u8 type;
    u8 flags;
    u8 pad[6];
    s32 data;
} RoomActionEntry;

s32 check_room_actions(s32 obj, s32 mask) {
    s32 rotate_vector_y_p() __asm__("rotate_vector_y");
    extern s32 (*g_RoomCheckActions[])();
    extern RoomActionEntry * g_RoomActionTail_ep __asm__("g_RoomActionTail");
    extern RoomActionEntry g_RoomActionTable_e[] __asm__("g_RoomActionTable");
    extern s8 g_FwdPosActionId_s8 __asm__("g_FwdPosActionId");
    s32 ret;
    s32 (**tbl)();
    s8 n;
    RoomActionEntry *p;
    u32 t;
    s16 *s;

    s = LITP(s16, sp_svec);
    *s = 0x258;
    SCRATCH->sv_z = 0;
    rotate_vector_y_p(((s16) g_PlayerEntity.angle), LITP(s16, sp_svec), LITP(s16, sp_svec));
    SCRATCH->vec_x = *s + g_PlayerEntity.pos_x;
    SCRATCH->vec_z = SCRATCH->sv_z + g_PlayerEntity.pos_z;
    p = g_RoomActionTable_e;
    g_CurrentEntity->has_enter_switch_zone &= 0xDF;
    ret = 0;
    if (g_RoomActionTail_ep >= p) {
        tbl = g_RoomCheckActions;
        n = 1;
        do {
            if (p->type != 0) {
                t = p->flags;
                if ((t & mask) && (t >> 7) != 1) {
                    if (p->flags & 0x40) {
                        if (is_point_in_action_zone((s32 *)(obj + 0x34), (u16 *)p->data) & 0xFFFF) {
                            g_EntityPosActionId = n;
                            ret = tbl[p->type](p);
                        }
                    } else if (is_point_in_action_zone(LITP(s32, sp_vec), (u16 *)p->data) & 0xFFFF) {
                        g_FwdPosActionId_s8 = n;
                        ret = tbl[p->type](p);
                    }
                }
            }
            p++;
            n++;
        } while (g_RoomActionTail_ep >= p);
    }
    return ret;
}

s32 check_action_object(void) {
    s32 rotate_vector_y_p() __asm__("rotate_vector_y");
    extern s32 (*g_RoomCheckActions[])();
    extern RoomActionEntry *g_RoomActionTail_p __asm__("g_RoomActionTail");
    extern RoomActionEntry g_RoomActionTable_e[] __asm__("g_RoomActionTable");
    extern s8 g_FwdPosActionId_s8 __asm__("g_FwdPosActionId");
    s32 (**tbl)();
    s8 n;
    RoomActionEntry *p;
    s16 *s;

    s = LITP(s16, sp_svec);
    *s = 0x258;
    SCRATCH->sv_z = 0;
    rotate_vector_y_p(((s16) g_PlayerEntity.angle), LITP(s16, sp_svec), LITP(s16, sp_svec));
    SCRATCH->vec_x = *s + g_PlayerEntity.pos_x;
    SCRATCH->vec_z = SCRATCH->sv_z + g_PlayerEntity.pos_z;
    p = g_RoomActionTable_e;
    if (g_RoomActionTail_p >= p) {
        tbl = g_RoomCheckActions;
        n = 1;
        do {
            if (p->type != 0) {
                if ((p->flags & 1) && (p->flags >> 7) != 0) {
                    if (p->flags & 0x40) {
                        if (is_point_in_action_zone(LITP(s32, g_PlayerPosX), (u16 *)p->data) & 0xFFFF) {
                            g_EntityPosActionId = n;
                            return tbl[p->type](p);
                        }
                    } else if (is_point_in_action_zone(LITP(s32, sp_vec), (u16 *)p->data) & 0xFFFF) {
                        g_FwdPosActionId_s8 = n;
                        return tbl[p->type](p);
                    }
                }
            }
            p++;
            n++;
        } while (g_RoomActionTail_p >= p);
    }
    return 0;
}

void check_itembox_state(void) {
    extern u8 * g_OmodelTable_u8p[] __asm__("g_OmodelTable");

    switch (g_ItemboxState) {
    case 0:
        break;
    case 1:
        g_ItemboxLidStep = 1;
        g_ItemboxLidAccel = 1;
        g_ItemboxState = 2;
        g_ItemboxObject = g_OmodelTable_u8p[ROOM_ACTION(g_pRoomActionEntry)->arg1];
        /* fallthrough */
    case 2:
        ENT(g_ItemboxObject)->angle_z -= g_ItemboxLidStep;
        g_ItemboxLidStep += g_ItemboxLidAccel;
        if ((s16) ENT(g_ItemboxObject)->angle_z < -0xC7) {
            g_ItemboxState = 3;
            g_ItemboxLidAccel = (s16)g_ItemboxLidAccel * -1;
        }
        break;
    case 3:
        ENT(g_ItemboxObject)->angle_z -= g_ItemboxLidStep;
        g_ItemboxLidStep += g_ItemboxLidAccel;
        if ((s16)g_ItemboxLidStep <= 0) {
            g_UpdateFlags = 0xFFFF;
            g_ItemboxState = 4;
            LIT(s32, g_GameStateFlags) |= MENU_REQUEST(MENU_MODE_ITEMBOX);
        }
        break;
    case 4:
        g_ItemboxState = 0;
        ENT(g_ItemboxObject)->angle_z = 0;
        break;
    }
}

s32 check_typewriter_state(void) {
    int VSync(int mode);
    void memcard_screen_l(s32, s32, s32, s32) __asm__("memcard_screen");
    void set_message_display_l(s32, s32) __asm__("set_message_display");
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");

    /* declared s32 without a return value: v0 stays live at the exit, which keeps
       the original's unfilled delay slot in case 2 */
    switch (g_TypewriterState) {
    case 0:
        break;
    case 1:
        g_TypewriterSaveArg = ROOM_ACTION(g_pRoomActionEntry)->arg0;
        set_message_display_l(0xDF, 0xFF);
        g_TypewriterState = 2;
        break;
    case 2:
        if (g_MessageStatus_u8 & 0x80) {
            break;
        }
        if (!(g_MessageStatus_u8 & 1)) {
            g_FadeTypeId = 2;
            g_FadeStep = 0x1000;
            fade_update();
            g_TypewriterState = 3;
        } else {
            g_TypewriterState = 0;
            LIT(u16, g_UpdateFlags) |= 0x45;
        }
        break;
    case 3:
        if (g_FadeLevel < 0) {
            LIT(s32, g_PlayTime) += VSync(-1) - g_PlayTimeVSync;
            g_PlayTimeVSync = VSync(-1);
            memcard_screen_l(0, g_LoadPointer, g_TypewriterSaveArg + 1, 2);
            g_FadeTypeId = 2;
            g_FadeStep = -0x1000;
            fade_update();
            g_TypewriterState = 4;
        }
        break;
    case 4:
        if (g_FadeLevel < 0) {
            g_TypewriterState = 0;
            LIT(u16, g_UpdateFlags) |= 0x45;
        }
        break;
    }
}

volatile unsigned int check_event_item_usage(void) {
    s32 use_room_action_item_a1() __asm__("use_room_action_item");
    extern u8 g_MessageStatus_u8 __asm__("g_MessageStatus");
  u8 used_flag;
  used_flag = LIT(u8, g_EventItemUsedFlag);
  if (((used_flag != 0) && (used_flag == 1)) && (!(g_MessageStatus_u8 & 0x80)))
  {
    use_room_action_item_a1();
    LIT(u8, g_EventItemUsedFlag) = 0;
  }
}

void render_room_objects(void) {
    MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
    extern u8 *g_ItemModelTable_g[] __asm__("g_ItemModelTable");
    extern u8 *g_OmodelTable_g[] __asm__("g_OmodelTable");
    void room_object_render_l() __asm__("room_object_render");
    u8 *obj;
    u8 i;
    u8 j;
    u8 n;

    for (i = 0; i < 2; i++) {
        if (i == 0) {
            n = ROOM->object_count;
        } else {
            n = ROOM->item_model_count;
        }
        for (j = 0; j < n;) {
            if (i == 0) {
                obj = g_OmodelTable_g[j];
            } else {
                obj = g_ItemModelTable_g[j];
            }
            j++;
            if (obj != NULL && (MODEL_OBJ(obj)->flags & 1) && MODEL_OBJ(obj)->tmd != 0) {
                RotMatrix((SVECTOR *)(&MODEL_OBJ(obj)->rot_x), (MATRIX *)(&MODEL_OBJ(obj)->coord));
                MODEL_OBJ(obj)->coord_flg = 0;
                room_object_render_l(obj);
            }
        }
    }
}

s32 update_room_objects(void) {
    extern u8 *g_OmodelTable_l[] __asm__("g_OmodelTable");
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    extern u8 *g_CurrentEntity_l __asm__("g_CurrentEntity");
    extern u8 *g_PushObject_l __asm__("g_PushObject");
    extern u8 g_PlayerActionBehavior_l __asm__("g_PlayerActionBehavior");
    s32 chk_entity_slide(u8 *, u8 *, s32);
    s32 chk_pl_reach_entity(u8 *);
    s32 check_room_collision_two_point_l(void *, void *) __asm__("check_room_collision_two_point");
    s32 chk_obj_slide(u8 *, u8 *);
    void check_room_actions_l(u8 *, s32) __asm__("check_room_actions");
    u8 *e;
    u8 *o;
    u8 *q;
    s32 i;
    s32 k;
    s32 n;
    s32 found;
    s32 sx;
    s32 sz;

    i = 0;
    found = 0;
    for (; i < ROOM_L->object_count; i++) {
        e = g_OmodelTable_l[i];
        if (e == NULL || !(MODEL_OBJ(e)->flags & 1)) {
            continue;
        }
        n = g_EnemyCount;
        for (q = LITP(u8, g_EnemiesList); n > 0; q += ENTITY_SIZE) {
            if (ENT(q)->status_flags & 1) {
                n--;
                chk_entity_slide(q, e, 0);
            }
        }
        sx = FIELD_AT(e, ModelObject, ->coord.t[0], s32);
        sz = FIELD_AT(e, ModelObject, ->coord.t[2], s32);
        if (chk_entity_slide(LITP(u8, g_PlayerEntity), e, 1) != 0 && (g_PadHeld & PAD_FORWARD)
            && chk_pl_reach_entity(e) != 0) {
            (FIELD_AT(e, ModelObject, ->billboard, u16))++;
        } else {
            FIELD_AT(e, ModelObject, ->billboard, u16) = 0;
        }
        if (!(MODEL_OBJ(e)->flags & 0x20) && FIELD_AT(e, ModelObject, ->billboard, u16) == 9) {
            g_CurrentEntity_l = e;
            if (!(MODEL_OBJ(e)->flags & 4) && check_room_collision_two_point_l(e + 0x94, e + 0x9C) != 0) {
            fail:
                FIELD_AT(e, ModelObject, ->billboard, u16) = 10;
                g_PushObject_l = NULL;
                goto restore;
            }
            found = 1;
            n = g_EnemyCount;
            q = LITP(u8, g_EnemiesList);
            FIELD_AT(e, ModelObject, ->billboard, u16) = 8;
            g_PushObject_l = e;
            g_PlayerAngle = (g_PlayerAngle + 0x200) & 0xC00;
            for (; n > 0; q += ENTITY_SIZE) {
                if (ENT(q)->status_flags & 1) {
                    n--;
                    if (chk_entity_slide(q, e, 1) != 0) {
                        FIELD_AT(e, ModelObject, ->billboard, u16) = 10;
                    }
                }
            }
            for (k = 0; k < ROOM_L->object_count; k++) {
                o = g_OmodelTable_l[k];
                if ((o[0] & 1) && e != o && chk_obj_slide(o, e) != 0) {
                    goto fail;
                }
            }
            if (g_PlayerHitState != 0 || g_PlayerActionBehavior_l == 0x10) {
                goto skip;
            }
            LIT(u16, g_UpdateFlags) &= 0xFFBF;
        }
    restore:
        FIELD_AT(e, ModelObject, ->coord.t[0], s32) = sx;
        FIELD_AT(e, ModelObject, ->coord.t[2], s32) = sz;
    skip:
        chk_entity_slide(LITP(u8, g_PlayerEntity), e, 0);
        if (FIELD_AT(e, ModelObject, ->coord.t[0], s32) != FIELD_AT(e, ModelObject, ->position_vx, s16) || FIELD_AT(e, ModelObject, ->coord.t[2], s32) != FIELD_AT(e, ModelObject, ->position_vz, s16)) {
            for (k = 0; k < ROOM_L->object_count; k++) {
                o = g_OmodelTable_l[k];
                if ((o[0] & 1) && e != o) {
                    chk_obj_slide(e, o);
                }
            }
            FIELD_AT(e, ModelObject, ->position_vx, s16) = FIELD_AT(e, ModelObject, ->coord.t[0], s32);
            FIELD_AT(e, ModelObject, ->position_vz, s16) = FIELD_AT(e, ModelObject, ->coord.t[2], s32);
        }
        check_room_actions_l(e, 4);
    }
    if (found != 0) {
        LIT(s32, g_GameStateFlags) |= GAME_PUSHING;
        return;
    }
    if (LIT(s32, g_GameStateFlags) & GAME_PUSHING) {
        LIT(s32, g_GameStateFlags) &= ~GAME_PUSHING;
        LIT(u16, g_UpdateFlags) |= 0x40;
    }
}

s32 chk_entity_slide(u8 *ent, u8 *obj, s32 push_obj) {
    s16 *ofs;
    s16 *box;
    u8 *m;
    s16 *s;
    s32 x1;
    s32 z1;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 wx;
    s32 wy;
    s32 wz;
    s32 ax;
    s32 az;
    s32 v;
    u16 h;
    s32 n;

    n = 0;
    if ((ENT(ent)->status_flags & 8) || (ENT(obj)->status_flags & 2)) {
        return 0;
    }
    ofs = ((s16 *) ENT(ent)->sca_hit_data);
    box = ((s16 *) ENT(ent)->sca_info);
    for (;;) {
        x1 = ENT(obj)->pos_x;
        z1 = ENT(obj)->pos_z;
        dx = x1 - (ofs[0] + ENT(ent)->pos_x);
        dy = ENT(obj)->pos_y - (ofs[1] + ENT(ent)->pos_y);
        dz = z1 - (ofs[2] + ENT(ent)->pos_z);
        m = ((u8 *) ENT(ent)->sca_info);
        s = ((s16 *) ENT(obj)->sca_info);
        h = SCA(m)->radius;
        wx = h + s[1];
        wy = SCA(m)->half_height + s[2];
        wz = h + s[3];
        if ((u32)(dx + wx) <= (u32)(wx * 2) && (u32)(dy + wy) <= (u32)(wy * 2)
            && (u32)(dz + wz) <= (u32)(wz * 2)) {
            ax = __builtin_abs(dx * wz);
            az = __builtin_abs(dz * wx);
            if (az < ax) {
                if (push_obj != 0) {
                    v = SCA(m)->ofs_x + ENT(ent)->pos_x;
                    if (dx < 0) {
                        ENT(obj)->pos_x = v - wx;
                    } else {
                        ENT(obj)->pos_x = v + wx;
                    }
                    n++;
                } else if (dx < 0) {
                    ENT(ent)->pos_x = x1 + wx;
                } else {
                    ENT(ent)->pos_x = x1 - wx;
                }
            } else if (push_obj != 0) {
                v = SCA(m)->ofs_z;
                /* do/while(0): raises the z0 register's allocation priority */
                do {
                    v += ENT(ent)->pos_z;
                } while (0);
                if (dz < 0) {
                    v -= wz;
                } else {
                    v += wz;
                }
                ENT(obj)->pos_z = v;
                n++;
            } else {
                v = z1 - wz;
                if (dz < 0) {
                    v = z1 + wz;
                }
                ENT(ent)->pos_z = v;
            }
        }
        if (*box < 0) {
            break;
        }
        ofs += 3;
        box += 6;
    }
    return n;
}

s32 chk_obj_slide(u8 *fixed, u8 *moved) {
    s32 x0;
    s32 z0;
    s32 dx;
    s32 dz;
    s32 w;
    s32 d;
    s32 m1;
    s32 m2;
    u8 *pa;
    u8 *pb;

    if ((MODEL_OBJ(fixed)->flags | MODEL_OBJ(moved)->flags) & 8) {
        return 0;
    }
    dx = MODEL_OBJ(moved)->coord.t[0] - MODEL_OBJ(fixed)->coord.t[0];
    dz = MODEL_OBJ(moved)->coord.t[2] - MODEL_OBJ(fixed)->coord.t[2];
    x0 = MODEL_OBJ(fixed)->coord.t[0];
    z0 = MODEL_OBJ(fixed)->coord.t[2];
    pa = ((u8 *) MODEL_OBJ(fixed)->sca_info);
    pb = ((u8 *) MODEL_OBJ(moved)->sca_info);
    w = SCA(pa)->ofs_x + SCA(pb)->ofs_x;
    d = SCA(pa)->ofs_z + SCA(pb)->ofs_z;
    if ((u32)(dx + w) > (u32)(w * 2)) {
        return 0;
    }
    if ((u32)(dz + d) > (u32)(d * 2)) {
        return 0;
    }
    m1 = __builtin_abs(dx * d);
    m2 = __builtin_abs(dz * w);
    if (m2 < m1) {
        if (dx < 0) {
            MODEL_OBJ(moved)->coord.t[0] = ({ x0 - 1; }) - w;
        } else {
            MODEL_OBJ(moved)->coord.t[0] = ({ x0 + 1; }) + w;
        }
    } else {
        if (dz < 0) {
            MODEL_OBJ(moved)->coord.t[2] = ({ z0 - 1; }) - d;
        } else {
            MODEL_OBJ(moved)->coord.t[2] = ({ z0 + 1; }) + d;
        }
    }
    return 1;
}

void room_object_render(u8 *obj) {
    MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
    void SetLightMatrix(MATRIX *m);
    void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m);
    void GsSetLsMatrix(MATRIX *mp);
    void GsSortObject5(GsDOBJ5 *objp, GsOT *otp, int shift, u_long *scratch);
    s32 is_point_in_quad_l(void *, s32) __asm__("is_point_in_quad");
    void update_entity_lighting_l(void *) __asm__("update_entity_lighting");
    MATRIX ls;

    update_entity_lighting_l(obj + 0x34);
    GsGetLs(MODEL_OBJ(obj)->coord2, &ls);
    MulMatrix0((MATRIX *)&g_LightMatrix, (MATRIX *)(&MODEL_OBJ(obj)->coord), LITP(MATRIX, sp_matrix));
    SetLightMatrix(LITP(MATRIX, sp_matrix));
    GsSetLsMatrix(&ls);
    if (is_point_in_quad_l(obj + 0x54, (s32)g_CameraSwitchZones) != 0) {
        if (MODEL_OBJ(obj)->flags & 0x80) {
            GsSortObject5((GsDOBJ5 *)(&MODEL_OBJ(obj)->attribute), LITP(GsOT, g_OtBg) + g_ActiveBuffer, 0xA, LITP(u_long, sp_tmp00));
        } else {
            GsSortObject5((GsDOBJ5 *)(&MODEL_OBJ(obj)->attribute), LITP(GsOT, g_OtMain) + g_ActiveBuffer, 4, LITP(u_long, sp_tmp00));
        }
    }
}

s32 check_climb_object(void) {
    s32 chk_pl_reach_entity_a1() __asm__("chk_pl_reach_entity");
    extern u8 * g_OmodelTable_u8p[] __asm__("g_OmodelTable");
    extern s16 g_PlayerAngle_s16 __asm__("g_PlayerAngle");
    s32 t;
    s32 n;
    u8 *e;
    s32 d;
    s32 r;

    t = LIT(s32, g_GameStateFlags);
    if (t & 0x80) {
        r = 0;
        if ((u16)(__builtin_abs(g_PlayerAngle_s16 - FIELD_AT(g_ClimbObject, ModelObject, ->angle, s16)) - 300) >= 0xDA9) {
            LIT(s32, g_GameStateFlags) = t & ~GAME_ON_BOX;
            g_PlayerHasEnterSwitchZone |= 0x10;
            goto dir;
        }
        return r;
    }
    n = g_OmodelCount;
    if (n != 0) {
        do {
            n--;
            e = g_OmodelTable_u8p[n];
            if ((MODEL_OBJ(e)->flags & 0x40) && chk_pl_reach_entity_a1(e) != 0) {
                d = ((g_PlayerAngle_s16 + 0x800) & 0xFFF) - ((s16) MODEL_OBJ(e)->angle);
                if (d < 0) {
                    d = -d;
                }
                if ((u16)(d - 300) >= 0xDA9) {
                    g_ClimbObject = e;
                    LIT(s32, g_GameStateFlags) |= GAME_ON_BOX;
                    g_PlayerHasEnterSwitchZone &= 0xEF;
                    goto dir;
                }
            }
        } while (n != 0);
    }
    return 0;
dir:
    if ((g_PlayerAngle_s16 + 0x200) & 0x800) {
        g_PlayerActionTicksCounter = 1;
    } else {
        g_PlayerActionTicksCounter = -1;
    }
    return 1;
}

s32 chk_pl_reach_entity(u8 *obj) {
    void rotate_vector_y_l() __asm__("rotate_vector_y");
    s16 *p;
    s16 x;
    s16 z;
    s32 ax;
    s32 az;
    s32 dx;
    s32 dz;
    s32 w;
    s32 c;

    p = LITP(s16, sp_svec);
    *p = 0x1D6;
    SCRATCH->sv_z = 0;
    rotate_vector_y_l(((s16) g_PlayerEntity.angle), LITP(s16, sp_svec), LITP(s16, sp_svec));
    x = *p + (*(u16 *)&g_PlayerEntity.pos_x);
    *p = x;
    z = SCRATCH->sv_z + (*(u16 *)&g_PlayerEntity.pos_z);
    SCRATCH->sv_z = z;
    dx = x - ENT(obj)->pos_x;
    dz = z - ENT(obj)->pos_z;
    w = FIELD_AT(obj, Entity, ->hit_state, s16);
    dx += w;
    w *= 2;
    c = (u32)w < (u32)dx;
    dx = ENT(obj)->floor_y;
    if (c) {
        return 0;
    }
    dz += dx;
    if ((u32)(dx * 2) < (u32)dz) {
        return 0;
    }
    ax = __builtin_abs(x);
    az = __builtin_abs(z);
    if (az < ax) {
        SCRATCH->sv_z = 0;
    } else {
        *p = 0;
    }
    return 1;
}
