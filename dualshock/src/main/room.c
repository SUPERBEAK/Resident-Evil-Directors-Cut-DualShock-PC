/* room.c - Room loading: the RDT file (load and relocate), room init and the room action table. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void clut_fade_reset(void);
void clut_fade_capture(s32 row_end);
void check_camera_switch(s32 refresh);
void display_room_camera_bg(void);
void stage_init_eff_sprite(void);
void load_entity_emd(u8 *ent, u8 id);
void room_events_check(void);
void load_file(s32 a, s32 b);
void load_held_items_images(s32 img_buf);
void init_anim_structure(s32 tmd);
u32 *setup_joint_structures(u32 *packet);
void entity_set_joints(void *ent, s32 joint_size);
void setup_entity_joint_animation(void);
void room_set_visited_flag(u32 stage, s32 room);
void setup_character_data(void);


u8 room_get_rdt_stage_index(void) {
    u8 i = 0;
    /* per stage: the rooms (0xFF = none) that load the stage+7 RDT in Advanced mode */
    u8 tbl[7][9] = {
        { 0x01, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C, 0xFF },
        { 0x01, 0x02, 0x03, 0x04, 0x12, 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x00, 0x04, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x04, 0x05, 0x07, 0x11, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x01, 0x04, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C },
        { 0x01, 0x02, 0x03, 0x04, 0x12, 0x18, 0x1A, 0xFF, 0xFF }
    };

    if (!(g_StatusFlags & STATUS_ADVANCED)) {
        return g_StageId;
    }
    do {
        if (tbl[LIT(u8, g_StageId)][i] != 0xFF && LIT(u8, g_RoomId) == tbl[LIT(u8, g_StageId)][i]) {
            return LIT(u8, g_StageId) + 7;
        }
        i++;
    } while (i < 9);
    return g_StageId;
}

void room_init(void) {
    extern u8 *g_ItemModelTable_l[] __asm__("g_ItemModelTable");
    extern u8 *g_OmodelTable_l[] __asm__("g_OmodelTable");
    extern u8 g_RoomId_l __asm__("g_RoomId");
    extern u8 *g_RoomData_l __asm__("g_RoomData");
    void clut_fade_reset_l() __asm__("clut_fade_reset");
    void clut_fade_capture_l(u8) __asm__("clut_fade_capture");
    void load_entity_emd_l(u8 *, s32) __asm__("load_entity_emd");
    void load_held_items_images_l(u8 *) __asm__("load_held_items_images");
    u8 *setup_joint_structures_l(u8 *) __asm__("setup_joint_structures");
    void entity_set_joints_l(u8 *, s32) __asm__("entity_set_joints");
    void room_set_visited_flag_l() __asm__("room_set_visited_flag");
    void rdt_load_and_relocate(void);
    void room_action_table_clear(void);
    s32 Flg_ck(s32 *, u32);
    void interactive_screen_reset(void);
    void room_setup_collision_callbacks(void);
    void sound_load_room_bank(void);
    void update_room_bgm(void);
    s32 sound_preload_bank1(void);
    void room_state_reset(void);
    u32 i;
    u8 *e;
    u8 *prev;
    u32 cur;
    u8 nb;
    u8 t1;
    u8 t2;
    u32 old;
    u16 tmp;
    s32 pad[6];

    g_LoadPointer = (s32)g_RoomLoadBase;
    LIT(s32, g_GameStateFlags) &= ~0xF;
    LIT(s32, g_StatusFlags) &= ~0xF;
    clut_fade_reset_l();
    room_action_table_clear();
    g_SysFlags[1] = 0;
    g_SysFlags[0] = 0;
    i = 0;
    do {
        LITP(ScdEvent, g_ScdEventTable)[i].active = 0;
        i++;
    } while (i < 8);
    g_PlayerEntity.scd_entity_flags = 0;
    LIT(s32, g_GameStateFlags) &= ~GAME_CUT_LOCKED;
    g_PlayerEntity.collision_flags &= 0xBF;
    room_set_visited_flag_l(g_StageId, g_RoomId_l);
    g_Fade2Level = -1;
    room_state_reset();
    *(u8 *)&g_NextCamera = 0;
    interactive_screen_reset();
    old = g_PlayerEntity.id;
    cur = LIT(u8, g_CharacterModelId);
    if (old != cur) {
        nb = LIT(volatile u8, g_CharacterModelId);
        if (nb & 8) {
            if (LIT(s32, g_StatusFlags) & STATUS_ADVANCED) {
                if (Flg_ck(LITP(s32, g_ScenarioFlags), 0x6F) != 0 || Flg_ck(LITP(s32, g_ScenarioFlags), 0x70) != 0) {
                    LIT(s32, g_StatusFlags) |= STATUS_ALT_COSTUME;
                } else {
                    LIT(s32, g_StatusFlags) &= ~STATUS_ALT_COSTUME;
                }
            } else {
                LIT(s32, g_StatusFlags) |= STATUS_ALT_COSTUME;
            }
            LIT(u8, g_CharacterModelId) &= 7;
        } else {
            if (!(cur < 4 && old < 4)) {
                goto skip;
            }
            g_PlayerEntity.id = nb;
            if (LIT(u8, g_CharacterModelId) == 3) {
                tmp = g_PlayerEntity.health;
                g_PlayerEntity.health = 0x58;
                g_PlayerWork.max_health = 0x58;
                g_PartnerSwapCollisionFlags = g_PlayerEntity.collision_flags;
                g_PartnerSwapHealth = tmp;
                if (g_StatusFlags & STATUS_TRAINING) {
                    g_PlayerEntity.health = 0x8C;
                    g_PlayerWork.max_health = 0x8C;
                }
                if (g_StatusFlags & STATUS_ADVANCED) {
                    g_PlayerEntity.health = 0x40;
                    g_PlayerWork.max_health = 0x40;
                }
                g_PlayerEntity.collision_flags = 0;
                g_ItemSlotsPointer = (s32)LITP(u8, g_RebeccaItemSlots);
                t1 = LIT(u8, g_EquippedSlot);
                LIT(u8, g_EquippedSlot) = 0;
                g_PartnerEquippedSlot = t1;
            } else {
                g_PlayerWork.max_health = 0x8C;
                if (g_StatusFlags & STATUS_TRAINING) {
                    g_PlayerWork.max_health = 0xB4;
                }
                if (g_StatusFlags & STATUS_ADVANCED) {
                    g_PlayerWork.max_health = 0x64;
                }
                g_ItemSlotsPointer = (s32)LITP(u8, g_InventorySlots);
                g_PlayerEntity.collision_flags = g_PartnerSwapCollisionFlags;
                g_PlayerEntity.health = g_PartnerSwapHealth;
                t2 = LIT(u8, g_PartnerEquippedSlot);
                LIT(u8, g_PartnerEquippedSlot) = 0;
                g_EquippedSlot = t2;
            }
        }
        load_held_items_images_l(LIT(u8 *, g_LoadPointer));
        ENTITY_STATE_WORD(&g_PlayerEntity) = 0;
        setup_character_data();
        sound_load_bank1(g_PlayerEntity.behavior_flags, LIT(u8 *, g_LoadPointer));
    skip:
        g_PlayerEntity.id = LIT(u8, g_CharacterModelId);
        sound_load_bank3(LIT(u8, g_CharacterModelId));
    }
    i = 0;
    rdt_load_and_relocate();
    room_setup_collision_callbacks();
    sound_load_room_bank();
    stage_init_eff_sprite();
    g_LoadPointer = ROOM_L_FIELD_AS(vab_body, s32);
    if (ROOM_L->object_count != 0) {
        do {
            g_OmodelTable_l[i] = LIT(u8 *, g_LoadPointer);
            LIT(u8 *, g_LoadPointer) = g_OmodelTable_l[i] + 0xA4;
            *g_OmodelTable_l[i] = 0;
            i++;
        } while (i < LIT(u8 *, g_RoomData)[2]);
    }
    i = 0;
    if (ROOM_L->item_model_count != 0) {
        do {
            g_ItemModelTable_l[i] = LIT(u8 *, g_LoadPointer);
            LIT(u8 *, g_LoadPointer) = g_ItemModelTable_l[i] + 0xA4;
            *g_ItemModelTable_l[i] = 0;
            i++;
        } while (i < LIT(u8 *, g_RoomData)[3]);
    }
    g_VramTpageCursor = 0xA06;
    LIT(u8, g_EnemyCount) = 0;
    LIT(u8, g_LastEnemyModelId) = 0xFF;
    g_OmodelCount = 0;
    g_ItemModelCount = 0;
    scd_run_command_functions(g_RoomInitScd);
    LIT(u16, g_UpdateFlags) |= 0x80;
    room_events_check();
    LIT(u16, g_UpdateFlags) &= 0xFF7F;
    update_room_bgm();
    g_PlayerWork.action_anim_header = ROOM_L_FIELD(skeleton_anim);
    g_PlayerWork.action_anim_base = ROOM_L_FIELD(skeleton_steps);
    g_CurrentEntity = LITP(Entity, g_EnemiesList);
    LIT(u8, g_LastEnemyModelId) = 0xFF;
    i = 0;
    if (LIT(u8, g_EnemyCount) != 0) {
        do {
        e = (u8 *)g_CurrentEntity;
        if (ENT(e)->status_flags & 1) {
            if (ENT(e)->id != LIT(u8, g_LastEnemyModelId)) {
                LIT(u8, g_LastEnemyModelId) = ENT(e)->id;
                load_entity_emd_l(e, ENT(e)->id + 4);
            } else {
                ENT(e)->anim_header = ENT(prev)->anim_header;
                ENT(e)->anim_base = ENT(prev)->anim_base;
                ENT(e)->model_load_buffer = ENT(prev)->model_load_buffer - 0xC;
            }
            entity_set_joints_l(LIT(u8 *, g_CurrentEntity), 0x7C);
            init_anim_structure(CURRENT_ENTITY_LIT->model_load_buffer);
            LIT(u8 *, g_LoadPointer) = setup_joint_structures_l(LIT(u8 *, g_LoadPointer));
            prev = LIT(u8 *, g_CurrentEntity);
            if (g_GameStateFlags & GAME_MIRROR_ON) {
                setup_entity_joint_animation();
            }
            i++;
        }
        LIT(u8 *, g_CurrentEntity) += ENTITY_SIZE;
        } while (i < g_EnemyCount);
    }
    clut_fade_capture_l(g_VramClutRowCursor);
    g_CameraSwitchZones = (void *)ROOM_L_FIELD(camera_switches);
    g_NextCamera = g_VramTpageCursor;
    if (!(g_GameStateFlags & GAME_CUT_LOCKED)) {
        g_CutId = 0;
        check_camera_switch(1);
    } else {
        display_room_camera_bg();
    }
    if (g_StageId == 2 && g_RoomId_l == 3) {
        g_SndBank1PreloadPtr = LIT(u8 *, g_LoadPointer);
        LIT(u8 *, g_LoadPointer) += sound_preload_bank1();
    }
}

void rdt_load_and_relocate(void) {
    extern u32 g_GameStateFlags_u32 __asm__("g_GameStateFlags");
    extern u8 *g_RoomData_u8p __asm__("g_RoomData");
    void GsSetAmbient(long r, long g, long b);
    void GsSetLightMode(int mode);
    void load_file_l(s32, u8 *) __asm__("load_file");
    s32 *q;
    s32 *p;
    s32 *tb;
    s32 *tc;
    u8 r;
    u8 room;
    s32 n;
    s32 t;

    LIT(u8 *, g_RoomData) = (u8 *)g_LoadPointer;
    q = (s32 *)(g_LoadPointer + 0x94);
    r = room_get_rdt_stage_index();
    room = g_RoomId;
    if (g_RoomRdtSharedTable[r][room] != 0) {
        if ((u8)(r - 5) < 2 || (u8)(r - 12) < 2) {
            load_file_l(g_RoomFileTable[r][room], LIT(u8 *, g_RoomData));
        } else {
            load_file_l(g_RoomFileTable[r][room] + 1, LIT(u8 *, g_RoomData));
        }
    } else {
        if ((u8)(r - 5) < 2 || (u8)(r - 12) < 2) {
            load_file_l(((g_GameStateFlags_u32 >> 23) & 1) + g_RoomFileTable[r][room], LIT(u8 *, g_RoomData));
        } else {
            load_file_l(((g_GameStateFlags_u32 >> 23) & 1) + ({ g_RoomFileTable[r][room] + 1; }), LIT(u8 *, g_RoomData));
        }
    }
    for (n = LIT(u8 *, g_RoomData)[1]; n != 0; n--) {
        t = LIT(s32, g_LoadPointer);
        q[0] += t;
        q[1] += t;
        q = (s32 *)((u8 *)q + 0x2C);
    }
    for (p = (s32 *)(LIT(u8 *, g_RoomData) + 0x48); p < (s32 *)(LIT(u8 *, g_RoomData) + 0x94); p++) {
        *p = LIT(s32, g_LoadPointer) + *p;
    }
    tb = ((s32 *) ROOM_P->object_models);
    for (n = ROOM_P->object_count; n != 0; n--, tb += 2) {
        g_OmodelTable_m1[n] = 0;
        if (tb[0] != 0) {
            tb[0] = LIT(s32, g_LoadPointer) + tb[0];
        }
        if (tb[1] != 0) {
            tb[1] = LIT(s32, g_LoadPointer) + tb[1];
        }
    }
    tc = ((s32 *) ROOM_P->item_models);
    for (n = ROOM_P->item_model_count; n != 0; n--, tc += 2) {
        g_ItemModelTable_m1[n] = 0;
        if (tc[0] != 0) {
            tc[0] = LIT(s32, g_LoadPointer) + tc[0];
        }
        if (tc[1] != 0) {
            tc[1] = LIT(s32, g_LoadPointer) + tc[1];
        }
    }
    g_RoomInitScd = ROOM_P->init_script;
    g_RoomMainScd = ROOM_P->main_script;
    g_RoomEventScripts = ((s32 *) ROOM_P->event_scripts);
    p = g_RoomEventScripts;
    while (*p != 0) {
        *p += (s32)g_RoomEventScripts;
        p++;
    }
    GsSetAmbient(ROOM_P->ambient_r, ROOM_P->ambient_g, ROOM_P->ambient_b);
    GsSetLightMode(0);
}

void room_action_table_clear(void) {
    s32 off;
    u32 i;

    i = 0;
    off = 0;
    do {
        *(&g_RoomActionTable + off) = 0;
        i += 1;
        off += 0xC;
    } while (i < 0x18U);
    g_RoomActionTail = (u32)&g_RoomActionTable;
}
