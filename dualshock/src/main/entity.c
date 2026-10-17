/* entity.c - entity models (EMD / weapon animations), NPC room-script behaviours (npc_scd_*), the per-frame
   entity update and the enemy state kept across rooms */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 clut_fade_add(u32 row, s8 id);

s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);

void load_entity_emd(u8 *ent, u8 id) {
    void load_file(u16, s32);
    extern s32 g_CdFileTable_l[][2] __asm__("g_CdFileTable");
    extern u8 g_VramTpageCursor_l __asm__("g_VramTpageCursor");
    extern u8 g_VramClutRowCursor_l __asm__("g_VramClutRowCursor");
    void clut_fade_add_l(u8, u8) __asm__("clut_fade_add");
    s32 unused[9];
    u8 cx;
    u8 cy;
    u8 lang;
    u8 t;
    u32 size;
    u8 **pool;
    u8 *base;
    u32 *end;
    Entity *obj;

    cx = g_VramTpageCursor_l;
    cy = g_VramClutRowCursor_l;
    if ((g_StatusFlags & STATUS_ALT_COSTUME) && id < 2) {
        id = 0x33;
    }
    if (g_StatusFlags & STATUS_ADVANCED) {
        if (id < 2) {
            id = 0x34;
        }
        if (id == 3) {
            id = 0x35;
        }
        if ((u8)(id - 0x24) < 2 || id == 0x27 || id == 0x30) {
            id += 0x14;
        }
        if (id == 0x33 && Flg_ck(ADDR_g_ScenarioFlags, 0x6F) != 0) {
            id = g_PlayerEntity.id & 1;
        }
    }
    load_file(*(u16 *)(id * 2 + ({ (g_PlayerEntity.id & 1) * 0x8A + g_EmdFileTable; })), g_LoadPointer);
    lang = g_PlayerId & 1;
    size = g_CdFileTable_l[*(u16 *)(id * 2 + ({ lang * 0x8A + g_EmdFileTable; }))][0];
    if ((g_StatusFlags & STATUS_ALT_COSTUME) && id == 0x33) {
        id = lang;
    }
    if (g_StatusFlags & STATUS_ADVANCED) {
        s32 k;

        /* loop-depth weight: k (set first) wins local-alloc priority -> v0 */
        do { do { k = 0x34; } while (0); } while (0);
        if (id == k) {
            id = g_PlayerEntity.id & 1;
        }
        if (id == 0x35) {
            id = 3;
        }
        if (id == 0x1A) {
            g_CurrentEntity->id = 0;
        }
    }
    pool = LITP(u8 *, g_LoadPointer);
    base = *pool;
    end = (u32 *)(base + (size >> 2) * 4);
    *(s32 *)pool = (*(end - 1) >> 2) * 4 + (s32)base;
    obj = g_CurrentEntity;
    t = obj->id;
    if (t >= 0x20) {
        NPC_LOAD_TEX(obj)->clut_row = g_VramClutRowCursor_l;
        NPC_LOAD_TEX(obj)->tpage = g_VramTpageCursor_l;
    }
    tim_load_to_vram(*pool, obj, size);
    if ((g_EmdClutFadeMask >> t) & 1) {
        clut_fade_add_l(cy, t);
    }
    ENT(ent)->model_load_buffer = (s32)(base + (*(end - 2) >> 2) * 4);
    tmd_relocate_texture(2, ((u8 *) ENT(ent)->model_load_buffer), cx, cy);
    ENT(ent)->anim_base = (s32)(base + (*(end - 3) >> 2) * 4);
    ENT(ent)->anim_header = (s32)(base + (*(end - 4) >> 2) * 4);
    if (*(end - 5) != 0) {
        g_PlayerWork.grab_anim_header = (s32)base;
        g_PlayerWork.grab_anim_base = (s32)(base + (*(end - 5) >> 2) * 4);
    }
    task_sleep(1);
}

void load_behavior_model(void) {
    void load_file_blocking(u16, s32);
    void load_file(u16, s32);
    extern u32 g_CdFileTable_u32a[][2] __asm__("g_CdFileTable");
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    u8 *p;
    u8 *obj;
    u8 k;
    u8 t;
    u32 cur;
    u8 save5;
    u8 save4;
    u8 *q;
    u32 size;

    q = (u8 *)g_CurrentEntity;
    k = ENT(q)->behavior_flags;
    if (k != 0) {
        t = ENT(q)->id - 0x20;
        if (t >= 5) {
            t = 2;
        }
        if (t >= 12) {
            t = 3;
        }
        obj = ((u8 *) ENT(q)->joints);
        obj += 0x6C8;
        if (g_GameStateFlags & GAME_DOOR_ANIM) {
            load_file(g_BehaviorModelFiles[t][k], g_LoadPointer);
        } else {
            load_file_blocking(g_BehaviorModelFiles[t][k], g_LoadPointer);
        }
        p = (u8 *)g_CurrentEntity;
        size = g_CdFileTable_u32a[g_BehaviorModelFiles[t][p[2]]][0];
        cur = LIT(u32, g_LoadPointer);
        LIT(u32, sp_tmp20) = cur;
        LIT(u32, g_LoadPointer) = ((size >> 2) << 2) + cur;
        JOINT(obj)->anim_slot = cur;
        save5 = LIT(u8, g_VramClutRowCursor);
        LIT(u8, g_VramClutRowCursor) = p[0x16C];
        save4 = LIT(u8, g_VramTpageCursor);
        LIT(u8, g_VramTpageCursor) = p[0x16E];
        tmd_relocate_texture(2, ((u32) JOINT(obj)->anim_slot), LIT(u8, g_VramTpageCursor), LIT(u8, g_VramClutRowCursor));
        LIT(u8, g_VramClutRowCursor) = save5;
        LIT(u8, g_VramTpageCursor) = save4;
        JOINT(obj)->anim_slot += 0xC;
        JOINT(obj)->anim_object = LIT(void *, g_LoadPointer);
        LIT(u32, g_LoadPointer) = (u32)(GsPresetObject((GsDOBJ5 *)(&JOINT(obj)->anim_field), (u_long *)((u32) JOINT(obj)->anim_object)));
    }
}

void load_equipped_weapon_animation(s32 weapon, s32 joint_index, s32 buf, u32 *anim_object) {
    void load_file(u16, s32);
    void load_file_blocking(u16, s32);
    extern u32 g_CdFileTable_u32[] __asm__("g_CdFileTable");
    u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
    u8 *obj;
    u32 *hdr;
    u32 t;
    u8 saved;
    s32 n = weapon;

    obj = g_PlayerJoints;
    obj += (joint_index & 0xFF) * 0x7C;
    if (g_StatusFlags & STATUS_ADVANCED) {
        if ((g_PlayerEntity.id & 3) < 2 && ((n & 0xFF) == 2 || (n & 0xFF) == 4)) {
            n = 0xC;
            g_PlayerEntity.behavior_flags = 2;
        }
    }
    if (g_GameStateFlags & GAME_DOOR_ANIM) {
        load_file(g_WeaponAnimFiles[(g_PlayerEntity.id & 3) * 13 + (n & 0xFF)], buf);
    } else {
        load_file_blocking(g_WeaponAnimFiles[(g_PlayerEntity.id & 3) * 13 + (n & 0xFF)], buf);
    }
    do {
        t = g_CdFileTable_u32[g_WeaponAnimFiles[(g_PlayerEntity.id & 3) * 13 + (n & 0xFF)] * 2];
    } while (0);
    g_PlayerWeaponAnimHeader = buf;
    hdr = (u32 *)(buf + (t >> 2) * 4);
    g_PlayerWork.weapon_anim_base = buf + (hdr[-2] >> 2) * 4;
    if ((n & 0xFF) != 0) {
        FIELD_AT(obj, Joint, ->anim_slot, s32) = buf + (hdr[-1] >> 2) * 4;
        t = LIT(u8, g_VramClutRowCursor);
        LIT(u8, g_VramClutRowCursor) = 7;
        saved = LIT(u8, g_VramTpageCursor);
        LIT(u8, g_VramTpageCursor) = 0x16;
        tmd_relocate_texture(2, FIELD_AT(obj, Joint, ->anim_slot, s32), 0x16, 7);
        LIT(u8, g_VramClutRowCursor) = t;
        LIT(u8, g_VramTpageCursor) = saved;
        FIELD_AT(obj, Joint, ->anim_object, u32 *) = anim_object;
        FIELD_AT(obj, Joint, ->anim_slot, s32) += 0xC;
    } else {
        FIELD_AT(obj, Joint, ->anim_slot, s32) = g_PlayerWeaponPartAnimSlot;
        FIELD_AT(obj, Joint, ->anim_object, u32 *) = g_PlayerWeaponPartAnimObject;
    }
    GsPresetObject((GsDOBJ5 *)&JOINT(obj)->anim_field, (u_long *)FIELD_AT(obj, Joint, ->anim_object, u32 *));
}

void npc_state8_action_update(void) {
    extern s32 (*g_NpcScdBehaviors[])();
  s8 *new_var;
  int new_var2;
  u16 flags;
  void *e;
  g_NpcScdBehaviors[*((u8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x86))]();
  e = LIT(void *, g_CurrentEntity);
  new_var = (s8 *) e;
  if ((*((u16 *) (new_var + 0xE0))) & 2)
  {
    g_NpcScdBehaviors[*((u8 *) (new_var + 0x86))]();
  }
  flags = *((u16 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xE0));
  if (flags & 4)
  {
    new_var2 = 0;
    entity_update_weapon_joint((flags & 8) != new_var2);
  }
}

void entity_apply_walk_speed(u16 speed) {
    void *e;
    void *e2;
    void *e3;

    e = LIT(void *, g_CurrentEntity);
    ENT(e)->move_speed_current = speed;
    if ((u32) (ENT(e)->animation_frame_id - 0x15) < 7U) {
        ENT(e)->move_speed_current = (u16) (speed - 0xD);
    }
    e2 = LIT(void *, g_CurrentEntity);
    if ((u32) (ENT(e2)->animation_frame_id - 7) < 7U) {
        ENT(e2)->move_speed_current = (u16) (((u16) ENT(e2)->move_speed_current) - 0xD);
    }
    if ((u32) (CURRENT_ENTITY_LIT->animation_frame_id - 0x17) < 3U) {
        CURRENT_ENTITY_LIT->move_speed_current = (u16) (((u16) CURRENT_ENTITY_LIT->move_speed_current) - 0xE);
    }
    e3 = LIT(void *, g_CurrentEntity);
    if ((u32) (ENT(e3)->animation_frame_id - 9) < 3U) {
        ENT(e3)->move_speed_current = (u16) (((u16) ENT(e3)->move_speed_current) - 0xE);
    }
}

void npc_scd_00(void) {
    u8 step;
    void *e;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        /* fallthrough */
    case 1:
        joint_move(g_CurrentEntity->scd_entity_flags & 1, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x200);
        return;
    }
}

void npc_scd_01(void) {
    s32 Flg_on();
    u16 ticks;
    u8 step;
    void *e;
    void *e3;
    void *e2;
    void *e4;
    void *e5;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        e2 = LIT(void *, g_CurrentEntity);
        ENT(e2)->move_speed_current = 0;
        if (ENT(e2)->scd_entity_flags & 0x20) {
            ENT(e2)->blend_counter = 0;
        }
        CURRENT_ENTITY_LIT->action_ticks_counter = 1;
        /* fallthrough */
    case 1:
        if (g_CurrentEntity->scd_entity_flags & 0x80) {
            ticks = g_CurrentEntity->action_ticks_counter;
            g_CurrentEntity->action_ticks_counter = (u16) (ticks - 1);
            if (ticks == 0) {
                g_CurrentEntity->action_ticks_counter = 1U;
                return;
            }
        }
        e3 = LIT(void *, g_CurrentEntity);
        if (joint_move(ENT(e3)->scd_entity_flags & 1, ENT(e3)->anim_header, ENT(e3)->anim_base, 0x200) != 0) {
            CURRENT_ENTITY_LIT->action_state = 2U;
            e4 = LIT(void *, g_CurrentEntity);
            ENT(e4)->angle = (u16) (ENT(e4)->angle + ENT(e4)->scd_timer);
            return;
        }
        return;
    case 2:
        Flg_on(ADDR_g_SysFlags, ENT(e)->scd_anim_param);
        e5 = LIT(void *, g_CurrentEntity);
        ENT(e5)->scd_timer = 0U;
        if (ENT(e5)->scd_entity_flags & 0x10) {
            ENT(e5)->action_state = 0U;
        }
        break;
    }
}

void npc_scd_02(void) {
    long SquareRoot0(long a);
    void entity_apply_walk_speed_l(s32) __asm__("entity_apply_walk_speed");
    s32 dx;
    s32 dz;
    s32 r;
    u8 *e0;
    u8 *e1;
    u8 *e2;
    u8 *e3;
    u8 *e4;
    u8 *e5;
    u8 *e6;

    e0 = LIT(u8 *, g_CurrentEntity);
    switch (ENT(e0)->action_state) {
    case 0:
        ENT(e0)->animation_frame_id = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 7;
        LIT(u8 *, g_CurrentEntity)[0x87] = 1;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 7;
        /* fallthrough */
    case 1:
        e1 = LIT(u8 *, g_CurrentEntity);
        SCRATCH->vec_x = *(u16 *)(&ENT(e1)->anim_base_x);
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = ENT(e1)->anim_base_z;
        r = turn_toward_target(LITP(void, sp_vec), *(s16 *)(&ENT(e1)->scd_timer));
        e2 = LIT(u8 *, g_CurrentEntity);
        ENT(e2)->angle += r;
        joint_move(ENT(e2)->scd_entity_flags & 1, ENT(e2)->anim_header, ENT(e2)->anim_base, 0x200);
        if (turn_toward_target(LITP(void, sp_vec), 0x16A) == 0) {
            LIT(u8 *, g_CurrentEntity)[0x87] = 2;
        }
        break;
    case 2:
        ENT(e0)->animation_frame_id = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 7;
        LIT(u8 *, g_CurrentEntity)[0x87] = 3;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 7;
        /* fallthrough */
    case 3:
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 8) {
            sound_play_footstep(0, 0);
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 0x16) {
            sound_play_footstep(0, -4);
        }
        entity_apply_walk_speed_l(0x5D);
        e3 = LIT(u8 *, g_CurrentEntity);
        SCRATCH->vec_x = *(u16 *)(&ENT(e3)->anim_base_x);
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = ENT(e3)->anim_base_z;
        entity_rotate_toward_target(LITP(void, sp_vec), *(s16 *)(&ENT(e3)->scd_timer));
        e4 = LIT(u8 *, g_CurrentEntity);
        joint_move(ENT(e4)->scd_entity_flags & 1, ENT(e4)->anim_header, ENT(e4)->anim_base, 0x200);
        entity_add_speed_xz(0);
        e5 = LIT(u8 *, g_CurrentEntity);
        dx = ENT(e5)->pos_x - ENT(e5)->anim_base_x;
        dz = ENT(e5)->pos_z - ENT(e5)->anim_base_z;
        if (SquareRoot0(dx * dx + dz * dz) < 0x96) {
            Flg_on(LITP(void, g_SysFlags), LIT(u8 *, g_CurrentEntity)[0xDB]);
            e6 = LIT(u8 *, g_CurrentEntity);
            if (!(ENT(e6)->collision_flags & 0x80)) {
                FIELD_AT(e6, Entity, ->action_behavior, s16) = 0;
            }
        }
        break;
    }
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
void npc_scd_03(void) {
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    s16 ticks;
    u8 step;
    u8 flag_id;
    void *e2;
    void *e5;
    void *e8;
    void *e;
    void *e3;
    void *e4;
    void *e6;
    void *e7;
    void *e9;
    void *e10;

    step = g_CurrentEntity->action_state;
    switch (step) {
    case 0:
        CURRENT_ENTITY_LIT->animation_frame_id = 0U;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 8;
        CURRENT_ENTITY_LIT->action_state = 1;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        /* fallthrough */
    case 1:
        do {
            e = LIT(void *, g_CurrentEntity);
        } while (0);
        SCRATCH->vec_x = (s32) FIELD_AT(e, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = (s32) ENT(e)->anim_base_z;
        dx = turn_toward_target(ADDR_sp_vec, FIELD_AT(e, Entity, ->scd_timer, s16));
        e2 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e2, Entity, ->angle, u16) += dx;
        joint_move(FIELD_AT(e2, Entity, ->scd_entity_flags, u16) & 1, FIELD_AT(e2, Entity, ->anim_header, s32), FIELD_AT(e2, Entity, ->anim_base, s32), 0x200);
        if (turn_toward_target(ADDR_sp_vec, 0x16A) == 0) {
            CURRENT_ENTITY_LIT->action_state = 2;
            return;
        }
    default:
        return;
    case 2:
        e3 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e3, Entity, ->move_speed_current, u16) = 0xD2U;
        FIELD_AT(e3, Entity, ->animation_frame_id, u8) = 0U;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 8;
        CURRENT_ENTITY_LIT->action_state = 3;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        /* fallthrough */
    case 3:
        if (CURRENT_ENTITY_LIT->animation_frame_id == 0) {
            sound_play_footstep(1, 0);
        }
        if (CURRENT_ENTITY_LIT->animation_frame_id == 0xA) {
            sound_play_footstep(1, -4);
        }
        e4 = LIT(void *, g_CurrentEntity);
        SCRATCH->vec_x = (s32) FIELD_AT(e4, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = (s32) ENT(e4)->anim_base_z;
        entity_rotate_toward_target(ADDR_sp_vec, FIELD_AT(e4, Entity, ->scd_timer, s16));
        e5 = LIT(void *, g_CurrentEntity);
        joint_move(FIELD_AT(e5, Entity, ->scd_entity_flags, u16) & 1, FIELD_AT(e5, Entity, ->anim_header, s32), FIELD_AT(e5, Entity, ->anim_base, s32), 0x200);
        entity_add_speed_xz(0);
        e6 = LIT(void *, g_CurrentEntity);
        dx = FIELD_AT(e6, Entity, ->pos_x, s32) - FIELD_AT(e6, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e6, Entity, ->pos_z, s32) - FIELD_AT(e6, Entity, ->anim_base_z, u16);
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0xFA) {
            CURRENT_ENTITY_LIT->action_state = 4;
            e7 = LIT(void *, g_CurrentEntity);
            if (FIELD_AT(e7, Entity, ->collision_flags, u8) & 0x80) {
                FIELD_AT(e7, Entity, ->action_state, s8) = 3;
                Flg_on(ADDR_g_SysFlags, CURRENT_ENTITY_LIT->scd_anim_param);
            }
        }
        break;
    case 4:
        CURRENT_ENTITY_LIT->animation_frame_id = 0U;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 0;
        CURRENT_ENTITY_LIT->action_state = 5;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        CURRENT_ENTITY_LIT->action_ticks_counter = 0U;
        /* fallthrough */
    case 5:
        e8 = LIT(void *, g_CurrentEntity);
        joint_move(FIELD_AT(e8, Entity, ->scd_entity_flags, u16) & 1, FIELD_AT(e8, Entity, ->anim_header, s32), FIELD_AT(e8, Entity, ->anim_base, s32), 0x200);
        e9 = LIT(void *, g_CurrentEntity);
        ticks = FIELD_AT(e9, Entity, ->action_ticks_counter, u16) + 1;
        FIELD_AT(e9, Entity, ->action_ticks_counter, u16) = ticks;
        if (ticks >= 4) {
            FIELD_AT(e9, Entity, ->action_state, s8) = 6;
        }
        e10 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e10, Entity, ->move_speed_current, u16) = (u16) (FIELD_AT(e10, Entity, ->move_speed_current, u16) - 0x1E);
        entity_add_speed_xz(0);
        return;
    case 6:
        flag_id = g_CurrentEntity->scd_anim_param;
        ENTITY_FIELD_AS(g_CurrentEntity, action_behavior, s16) = 0;
        Flg_on(ADDR_g_SysFlags, flag_id);
        break;
    }
}
__asm__(".section .rodata\n\t.word 0\n\t.text");

void npc_scd_04(void) {
    s32 Flg_on();
    long SquareRoot0(long a);
  s32 dx;
  s32 dz;
  int frame2;
  u8 step;
  u8 frame;
  void *e;
  void *e2;
  void *e3;
  void *e4;
  void *e5;
  e = LIT(void *, g_CurrentEntity);
  step = *((u8 *) (((s8 *) e) + 0x87));
  switch (step)
  {
    case 0:
      *((s8 *) (((s8 *) e) + 0xBE)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
      *((u8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x87)) = 1U;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBD)) = 3;

    case 1:
      frame = *((u8 *) (((s8 *) g_CurrentEntity) + 0xBE));
      if ((frame == 8) || (frame == 0x16))
    {
      sound_play_footstep(0, 0);
    }
      frame2 = *((u8 *) (((s8 *) g_CurrentEntity) + 0xBE));
      *((s16 *) (((s8 *) g_CurrentEntity) + 0xC2)) = 0x3C;
      if ((frame2 >= 5U) || (frame2 < 8U))
    {
      *((s16 *) (((s8 *) g_CurrentEntity) + 0xC2)) = 0x40;
    }
      e2 = LIT(void *, g_CurrentEntity);
      LIT(s32, sp_vec) = (s32) (*((u16 *) (((s8 *) e2) + 0xC6)));
      LIT(s32, sp_vec_vz) = (s32) (*((u16 *) (((s8 *) e2) + 0xC8)));
      LIT(s32, sp_vec_vy) = 0;
      *((u16 *) (((s8 *) e2) + 0x74)) = (u16) (((*((u16 *) (((s8 *) e2) + 0x74))) + 0x800) & 0xFFF);
      entity_rotate_toward_target(ADDR_sp_vec, *((s16 *) (((s8 *) e2) + 0xDE)));
      e3 = LIT(void *, g_CurrentEntity);
      *((u16 *) (((s8 *) e3) + 0x74)) = (u16) ((*((u16 *) (((s8 *) e3) + 0x74))) - 0x800);
      joint_move((*((u16 *) (((s8 *) e3) + 0xE0))) & 1, *((s32 *) (((s8 *) e3) + 0x90)), *((s32 *) (((s8 *) e3) + 0x94)), 0x200);
      entity_add_speed_xz(0x800);
      e4 = LIT(void *, g_CurrentEntity);
      dx = (*((s32 *) (((s8 *) e4) + 0x34))) - (*((u16 *) (((s8 *) e4) + 0xC6)));
      dz = (*((s32 *) (((s8 *) e4) + 0x3C))) - (*((u16 *) (((s8 *) e4) + 0xC8)));
      if (SquareRoot0((dx * dx) + (dz * dz)) < 0x64)
    {
      Flg_on(ADDR_g_SysFlags, *((u8 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0xDB)));
      e5 = LIT(void *, g_CurrentEntity);
      if (!((*((u8 *) (((s8 *) e5) + 0xDC))) & 0x80))
      {
        *((s16 *) (((s8 *) e5) + 0x86)) = 0;
      }
    }
      return;

  }

}

void npc_scd_05(void) {
    s32 Flg_on();
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    u8 step;
    u8 frame;
    void *e;
    void *e2;
    void *e3;
    void *e4;
    void *e5;

    e = LIT(void *, g_CurrentEntity);
    step = FIELD_AT(e, Entity, ->action_state, u8);
    switch (step) {
    case 0:
        FIELD_AT(e, Entity, ->animation_frame_id, s8) = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        CURRENT_ENTITY_LIT->animation_id = 2;
        CURRENT_ENTITY_LIT->move_speed_current = 0x1D;
        /* fallthrough */
    case 1:
        frame = g_CurrentEntity->animation_frame_id;
        if (((frame == 7) || (frame == 0x1B)) && (((u8) g_CurrentEntity->timing_control) == 2)) {
            sound_play_footstep(0, -4);
        }
        e2 = LIT(void *, g_CurrentEntity);
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(e2, Entity, ->anim_base_x, u16);
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(e2, Entity, ->anim_base_z, u16);
        LIT(s32, sp_vec_vy) = 0;
        FIELD_AT(e2, Entity, ->angle, u16) = (u16) ((FIELD_AT(e2, Entity, ->angle, u16) + 0x800) & 0xFFF);
        entity_rotate_toward_target(ADDR_sp_vec, FIELD_AT(e2, Entity, ->scd_timer, s16));
        e3 = LIT(void *, g_CurrentEntity);
        FIELD_AT(e3, Entity, ->angle, u16) = (u16) (FIELD_AT(e3, Entity, ->angle, u16) - 0x800);
        joint_move(FIELD_AT(e3, Entity, ->scd_entity_flags, u16) & 1, FIELD_AT(e3, Entity, ->anim_header, s32), FIELD_AT(e3, Entity, ->anim_base, s32), 0x200);
        entity_add_speed_xz(0x800);
        e4 = LIT(void *, g_CurrentEntity);
        dx = FIELD_AT(e4, Entity, ->pos_x, s32) - FIELD_AT(e4, Entity, ->anim_base_x, u16);
        dz = FIELD_AT(e4, Entity, ->pos_z, s32) - FIELD_AT(e4, Entity, ->anim_base_z, u16);
        if (SquareRoot0((dx * dx) + (dz * dz)) < 0x64) {
            Flg_on(ADDR_g_SysFlags, CURRENT_ENTITY_LIT->scd_anim_param);
            e5 = LIT(void *, g_CurrentEntity);
            if (!(FIELD_AT(e5, Entity, ->collision_flags, u8) & 0x80)) {
                FIELD_AT(e5, Entity, ->action_behavior, s16) = 0;
            }
        }
        return;
    }
}

void npc_scd_06(void) {
    s32 Flg_on();
    s32 turn_toward_target();
    u8 step;
    void *e;
    void *e3;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 7;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        /* fallthrough */
    case 1:
        e2 = LIT(void *, g_CurrentEntity);
    SCRATCH->vec_x = (s32) ENT(e2)->anim_base_x;
    SCRATCH->vec_z = (s32) ENT(e2)->anim_base_z;
    SCRATCH->vec_y = 0;
        entity_rotate_toward_target(ADDR_sp_vec, ((s16) ENT(e2)->scd_timer));
        e3 = LIT(void *, g_CurrentEntity);
        joint_move(ENT(e3)->scd_entity_flags & 1, ENT(e3)->anim_header, ENT(e3)->anim_base, 0x200);
        if (turn_toward_target(ADDR_sp_vec, 0x28) == 0) {
            CURRENT_ENTITY_LIT->action_state = 2U;
            return;
        }
        return;
    case 2:
        FIELD_AT(e, Entity, ->action_behavior, s16) = 0;
        Flg_on(ADDR_g_SysFlags, ENT(e)->scd_anim_param);
        break;
    }
}


void npc_scd_07(void) {
    s32 Flg_on();
    u8 step;
    void *e;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        /* fallthrough */
    case 1:
        CURRENT_ENTITY_LIT->action_state = CURRENT_ENTITY_LIT->action_state + joint_move(CURRENT_ENTITY_LIT->scd_entity_flags & 1, CURRENT_ENTITY_LIT->anim_header, CURRENT_ENTITY_LIT->anim_base, 0x200);
        break;
    case 2:
        Flg_on(ADDR_g_SysFlags, ENT(e)->scd_anim_param);
        break;
    }
    g_CurrentEntity->angle = (u16) (g_CurrentEntity->angle + g_CurrentEntity->scd_timer);
}

__asm__("#JTALIGN2");
void npc_scd_08(void) {
    typedef struct {
        u8 frame;
        u8 fx_type;
        u8 fx_variant;
        u8 pad3;
        s16 off_x;
        s16 off_y;
        s16 off_z;
    } ShotEffect;
    extern ShotEffect g_WeaponMuzzleFlashFx_l[] __asm__("g_WeaponMuzzleFlashFx");
    extern ShotEffect g_WeaponShellFx_l[] __asm__("g_WeaponShellFx");
    extern ShotEffect g_WeaponMuzzleFlash2Fx_l[] __asm__("g_WeaponMuzzleFlash2Fx");
    s32 effect_create_billboard_l() __asm__("effect_create_billboard");
    u8 *base;
    u8 k;
    u16 c;
    s32 r;

    k = ENT(g_CurrentEntity)->behavior_flags - 2;
    switch (ENT(g_CurrentEntity)->action_state) {
    case 0:
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0x87] = 1;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        if (k == 3) {
            LIT(u8 *, g_CurrentEntity)[0x87] = 3;
            LIT(u8 *, g_CurrentEntity)[0xBD] = 0x17;
            goto anim;
        }
    case 1:
        base = ((u8 *) ENT(g_CurrentEntity)->joints);
        base += 0x6C8;
        if (ENT(g_CurrentEntity)->animation_frame_id == g_WeaponMuzzleFlashFx_l[k].frame) {
            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlashFx_l[k].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlashFx_l[k].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlashFx_l[k].off_z;
            effect_create_billboard_l(g_WeaponMuzzleFlashFx_l[k].fx_type, g_WeaponMuzzleFlashFx_l[k].fx_variant, 0, base + 0x44, ADDR_sp_vec, 0);
            if (k == 2) {
                LIT(s32, sp_vec_vx) = 0x96;
                SCRATCH->vec_y = 0x17C;
                SCRATCH->vec_z = 0;
                effect_create_billboard_l(0x11, 3, 0, base + 0x44, ADDR_sp_vec, 0);
            }
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == g_WeaponShellFx_l[k].frame) {
            u8 *obj = LIT(u8 *, g_CurrentEntity);
            s32 r1;

            LIT(s32, sp_vec_vx) = g_WeaponShellFx_l[k].off_x;
            SCRATCH->vec_y = g_WeaponShellFx_l[k].off_y + (ENT(obj)->id & 1) * (1 - k) * 300;
            SCRATCH->vec_z = g_WeaponShellFx_l[k].off_z;
            r1 = effect_create_billboard_l(g_WeaponShellFx_l[k].fx_type, g_WeaponShellFx_l[k].fx_variant, (k != 8) ? 0x555 : 0, obj + 0x20,
                                ADDR_sp_vec, 0);
            SCRATCH->tmp00 = r1;
            LITP(Effect, g_EffectPool)[r1].hdr07 = k;
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == g_WeaponMuzzleFlash2Fx_l[k].frame) {
            s32 r2;

            LIT(s32, sp_vec_vx) = g_WeaponMuzzleFlash2Fx_l[k].off_x;
            SCRATCH->vec_y = g_WeaponMuzzleFlash2Fx_l[k].off_y;
            SCRATCH->vec_z = g_WeaponMuzzleFlash2Fx_l[k].off_z;
            r2 = effect_create_billboard_l(g_WeaponMuzzleFlash2Fx_l[k].fx_type, g_WeaponMuzzleFlash2Fx_l[k].fx_variant, 0, base + 0x44, ADDR_sp_vec, 0);
            SCRATCH->tmp00 = r2;
            LITP(Effect, g_EffectPool)[r2].hdr04 = k;
        }
        r = joint_move(0, CURRENT_ENTITY_LIT->anim_header,
                            CURRENT_ENTITY_LIT->anim_base, 0x400);
        LIT(u8 *, g_CurrentEntity)[0x87] += r;
        break;
    case 2:
        Flg_on(ADDR_g_SysFlags, ENT(g_CurrentEntity)->scd_anim_param);
        break;
    case 3:
    anim:
        r = joint_move(0, CURRENT_ENTITY_LIT->anim_header,
                            CURRENT_ENTITY_LIT->anim_base, 0x400);
        LIT(u8 *, g_CurrentEntity)[0x87] += r;
        break;
    case 4:
        LIT(u8 *, g_CurrentEntity)[0x87] = 5;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 0x14;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        CURRENT_ENTITY_LIT->action_ticks_counter = 15;
    case 5:
        if ((u8)(LIT(u8 *, g_CurrentEntity)[0xBE] % 6) == 0) {
            u8 *obj = LIT(u8 *, g_CurrentEntity);

            LIT(s32, sp_vec_vx) = 0x21C;
            SCRATCH->vec_y = 0x4EC;
            SCRATCH->vec_z = 0;
            effect_create_billboard_l(12, 0, 0, ((u8 *) ENT(obj)->joints) + 0x70C, ADDR_sp_vec, 0);
        }
        c = CURRENT_ENTITY_LIT->action_ticks_counter;
        CURRENT_ENTITY_LIT->action_ticks_counter = c - 1;
        if (c == 0) {
            CURRENT_ENTITY_LIT->action_ticks_counter = 15;
            sound_play_se_3d(2, 0x1E, 0, LIT(u8 *, g_CurrentEntity) + 0x34);
            sound_play_se_3d(2, 0x1F, 0, LIT(u8 *, g_CurrentEntity) + 0x34);
        }
        joint_move(0, CURRENT_ENTITY_LIT->anim_header,
                        CURRENT_ENTITY_LIT->anim_base, 0x400);
        CURRENT_ENTITY_LIT->angle += CURRENT_ENTITY_LIT->scd_timer;
        break;
    }
}

void npc_scd_09(void) {
    s32 Flg_on();
    u8 step;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 3;
        /* fallthrough */
    case 1:
        e2 = LIT(void *, g_CurrentEntity);
        CURRENT_ENTITY_LIT->action_state = (u8) (CURRENT_ENTITY_LIT->action_state + joint_move(1, ENT(e2)->anim_header, ENT(e2)->anim_base, 0x400));
        return;
    case 2:
        FIELD_AT(e, Entity, ->action_behavior, s16) = 0;
        Flg_on(ADDR_g_SysFlags, ENT(e)->scd_anim_param, e);
        return;
    }
}

void npc_scd_10(void) {
    s32 Flg_on();
    u8 step;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    step = ENT(e)->action_state;
    switch (step) {
    case 0:
        ENT(e)->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->action_state = 1U;
        CURRENT_ENTITY_LIT->blend_counter = 7;
        CURRENT_ENTITY_LIT->move_speed_current = 0;
        /* fallthrough */
    case 1:
        CURRENT_ENTITY_LIT->action_state = CURRENT_ENTITY_LIT->action_state + joint_move(CURRENT_ENTITY_LIT->scd_entity_flags & 1, CURRENT_ENTITY_LIT->anim_header, CURRENT_ENTITY_LIT->anim_base, 0x200);
        return;
    case 2:
        Flg_on(ADDR_g_SysFlags, ENT(e)->scd_anim_param);
        e2 = LIT(void *, g_CurrentEntity);
        if (ENT(e2)->scd_entity_flags & 0x10) {
            ENT(e2)->action_state = 0U;
        }
        return;
    }
}

void tmd_object_set_tpage(void *obj, s32 tpage) {
    s32 unused[2];
    s32 next;
    s32 count;
    s32 left;
    u16 *prim_tpage;

    count = TMD_OBJ(obj)->n_primitive;
    LIT(s32, sp_tmp00) = count;
    left = count - 1;
    prim_tpage = (u16 *)(((s32) TMD_OBJ(obj)->primitive_top) + 6);
    LIT(s32, sp_tmp00) = left;
    if (left != -1) {
        do {
            *prim_tpage = (*prim_tpage & 0xFFC0) | (tpage & 0x3F);
            prim_tpage += 14;
            next = LIT(s32, sp_tmp00) - 1;
            LIT(s32, sp_tmp00) = next;
        } while (next != -1);
    }
}

void update_entities(void) {
    extern void (**g_EnemyUpdateTable)(void);
    void entity_render_mirror(void);
    s32 unused[8];
    s32 i;
    u8 *obj;

    g_CurrentEntity = LITP(Entity, g_EnemiesList);
    for (i = 0; i < g_EnemyCount; LIT(u8 *, g_CurrentEntity) += ENTITY_SIZE) {
        if (i >= 0x1E) {
            break;
        }
        obj = LIT(u8 *, g_CurrentEntity);
        if (ENT(obj)->status_flags & 1) {
            g_EnemyUpdateTable[ENT(obj)->id]();
            if ((g_GameStateFlags & GAME_MIRROR_ON) &&
                mirror_point_visible(ROOM_CAMERA_VIEW(g_CutId), (u32)(g_GameStateFlags & GAME_MIRROR_AXIS) >> 1,
                                LIT(u8 *, g_CurrentEntity) + 0x34) != 0) {
                entity_render_mirror();
            }
            i++;
        }
    }
}

void build_enemy_snap(void) {
    extern u8 g_RoomId_u8 __asm__("g_RoomId");
    extern u8 g_PrevRoomId_u8 __asm__("g_PrevRoomId");
    extern s32 g_CurrentEntity_s32 __asm__("g_CurrentEntity");
    s32 i;
    s32 unused;
    s8 *slot;
    u8 *rooms_left;
    s32 rooms;
    void *e2;
    void *snap;
    void *cur_snap;
    void *e;
    void *e3;

    i = 0;
    if (SCD_DOOR(g_DoorRecord)->next_stage_room != g_PrevRoomId_u8) {
        rooms_left = &LITP(EnemySnapshot, g_EnemySnapshots)[0].rooms_left;
        do {
            rooms = *rooms_left;
            i += 1;
            if (rooms != 0) {
                *rooms_left = rooms - 1;
            }
            rooms_left += 0x1C;
        } while (i < 0x10);
    }
    g_CurrentEntity_s32 = ADDR_g_EnemiesList;
    g_EnemySnapCur = ADDR_g_EnemySnapshots;
    if (g_EnemyCount != 0) {
        do {
            if (SNAP(LIT(void *, g_EnemySnapCur))->rooms_left != 0) {
                do {
                    cur_snap = LIT(void *, g_EnemySnapCur);
                    LIT(void *, g_EnemySnapCur) = cur_snap + 0x1C;
                } while (SNAP(cur_snap)[1].rooms_left != 0);
            }
            e = LIT(void *, g_CurrentEntity);
            if (ENT(e)->health < 0) {
                SNAP(LIT(void *, g_EnemySnapCur))->status_flags = 0;
                if (CURRENT_ENTITY_LIT->death_event_id != 0xFF) {

                } else {
                    goto block_13;
                }
            } else {
                SNAP(LIT(void *, g_EnemySnapCur))->status_flags = (s8) (ENT(e)->status_flags & 0xF);
block_13:
                e2 = LIT(void *, g_CurrentEntity);
                if (((s8) ENT(e2)->slot_info) >= 0) {
                    SNAP(LIT(void *, g_EnemySnapCur))->behavior_flags = (u8) ENT(e2)->behavior_flags;
                    SNAP(LIT(void *, g_EnemySnapCur))->room_id = (u8) g_RoomId_u8;
                    SNAP(LIT(void *, g_EnemySnapCur))->slot_info = (u8) ((s8) CURRENT_ENTITY_LIT->slot_info);
                    snap = LIT(void *, g_EnemySnapCur);
                    e3 = LIT(void *, g_CurrentEntity);
                    SNAP(snap)->state = (s32) ENT(e3)->state;
                    SNAP(snap)->pos_x = (u16) FIELD_AT(e3, Entity, ->pos_x, u16);
                    SNAP(snap)->pos_y = (u16) FIELD_AT(e3, Entity, ->pos_y, u16);
                    SNAP(snap)->pos_z = (u16) FIELD_AT(e3, Entity, ->pos_z, u16);
                    SNAP(snap)->angle = (u16) ENT(e3)->angle;
                    SNAP(snap)->rooms_left = 5U;
                }
            }
            LIT(s8, g_EnemyCount) -= 1;
            LIT(void *, g_CurrentEntity) += ENTITY_SIZE;
        } while (LIT(u8, g_EnemyCount) != 0);
    }
    i = 0x1D;
    slot = LITP(s8, g_EnemySlot29);
    do {
        *slot = 0;
        i -= 1;
        slot -= ENTITY_SIZE;
    } while (i >= 0);
}

s32 restore_saved_enemy_state(s32 slot) {
    s32 i;

    g_EnemySnapCur = (s32)LITP(u8, g_EnemySnapshots);
    i = 0;
    do {
        if (LIT(u8 *, g_EnemySnapCur)[0x14] != 0 && LIT(u8 *, g_EnemySnapCur)[2] == LIT(u8, g_RoomId) &&
            LIT(u8 *, g_EnemySnapCur)[3] == slot) {
            LIT(u8 *, g_CurrentEntity)[0] = LIT(u8 *, g_EnemySnapCur)[0];
            LIT(u8 *, g_CurrentEntity)[2] = LIT(u8 *, g_EnemySnapCur)[1];
            *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34) = *(s16 *)(LIT(u8 *, g_EnemySnapCur) + 8);
            if (LIT(u8 *, g_CurrentEntity)[2] & 0x70) {
                *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x38) = *(s16 *)(LIT(u8 *, g_EnemySnapCur) + 0xA);
            }
            *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C) = *(s16 *)(LIT(u8 *, g_EnemySnapCur) + 0xC);
            *(u16 *)(LIT(u8 *, g_CurrentEntity) + 0x74) = *(u16 *)(LIT(u8 *, g_EnemySnapCur) + 0x10);
            LIT(u8 *, g_EnemySnapCur)[0x14] = 0;
            return 1;
        }
        i++;
        LIT(u8 *, g_EnemySnapCur) += 0x1C;
    } while (i < 16);
    return 0;
}
