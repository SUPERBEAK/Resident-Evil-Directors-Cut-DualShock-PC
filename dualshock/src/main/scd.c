/* scd.c - The room script (SCD) interpreter: one function per opcode (scd_op_NN_*). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void voice_command(s32 cmd, s32 clip);
s32 clut_fade_add(u32 row, s8 id);
void clut_fade_add_delta(s32 dr, s32 dg, s32 db, s16 start, u16 count, u8 id);
void clut_fade_set(s8 dr, s8 dg, s8 db, s16 start, u16 count, u8 id);
void clut_fade_clear(u8 id, s16 start, s16 count);
void cut_set(void);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
void effect_free_matching(u8 mask, u8 a, u8 b, u8 c, s32 d);
s32 restore_saved_enemy_state(s32 slot);
void scd_event_entry_create(u32 slot, s32 script);
void rearrange_item_slots(void);
u32 get_item_slot(s32 item_id);
void setup_entity_joint_animation(void);
void entity_alloc_weapon_buffers(void);
void entity_setup_weapon_part(s32 part);
s32 set_message_display(u8 msg, s16 msg_id);
void room_spr_set_inactive(u8 sprite_id);
void room_spr_set_active(u8 sprite_id);
void pad_vibrate_small(s32 duration, s32 delay);
void pad_vibrate_large(s32 duration, s32 vol, s32 delay);
s32 pad_vibrate_large_ramp(s32 duration, s32 vol_start, s32 vol_end, s32 delay);
void pad_vibration_reset(void);


s32 scd_op_00_block_end(void) {
    g_ScdContinueCount = 0;
    return 0;
}

s32 scd_op_01_if(void) {
    u8 *p;
    u8 **sp;
    u16 op;

    p = LIT(u8 *, g_ScdIp);
    sp = LIT(u8 **, g_ScdBranchSp);
    op = *(u16 *)p;
    p += 2;
    LIT(u8 *, g_ScdIp) = p;
    LIT(u8 **, g_ScdBranchSp) = sp + 1;
    *sp = (u8 *)((op >> 8) + (s32)p);
    LIT(u8, g_ScdContinueCount)++;
    return 1;
}

s32 scd_op_02_else(void) {
    u8 *p;

    LIT(s32, g_ScdBranchSp) -= 4;
    LIT(u8, g_ScdContinueCount)--;
    p = LIT(u8 *, g_ScdIp); LIT(u8 *, g_ScdIp) = (u8 *)(p[1] + (s32)p);
    return 1;
}

s32 scd_op_03_end_if(void) {
    LIT(s32, g_ScdBranchSp) -= 4;
    LIT(u8, g_ScdContinueCount)--;
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
extern u8 g_PlayerHitState;

/* Room script flag banks (ops 04 bit_test / 05 bit_op): the high byte of the first halfword picks the bank
   (0 scenario, 1 scenario 2, 2 locks, 3 enemies, 4 g_SysFlags, 5 g_GameStateFlags, 6 g_UpdateFlags,
   7 room items, 8 room, 9 item use; 10 = "is the player busy", no bank). In the second halfword, bits 5-7
   pick the word of the bank and bits 0-4 the bit, counted from the top (bit n = 0x80000000 >> n). */
u32 scd_op_04_bit_test(void) {
    extern u32 g_ItemUseFlags_u32[] __asm__("g_ItemUseFlags");
    u16 *p;
    u32 *w;
    u32 x;
    u16 *q;

    p = LIT(u16 *, g_ScdIp);
    x = *p;
    LIT(u16 *, g_ScdIp) = p + 1;
    switch (x >> 8) {
    case 0:
        w = LITP(u32, g_ScenarioFlags);
        break;
    case 1:
        w = LITP(u32, g_ScenarioFlags2);
        break;
    case 2:
        w = LITP(u32, g_LocksFlags);
        break;
    case 3:
        w = LITP(u32, g_EnemiesFlags);
        break;
    case 4:
        w = LITP(u32, g_SysFlags);
        break;
    case 5:
        w = LITP(u32, g_GameStateFlags);
        break;
    case 6:
        w = LITP(u32, g_UpdateFlags);
        break;
    case 7:
        w = LITP(u32, g_RoomItemsFlags);
        break;
    case 8:
        w = LITP(u32, g_RoomFlags);
        break;
    case 9:
        w = g_ItemUseFlags_u32;
        break;
    case 10:
        LIT(s32, g_ScdIp) += 2;
        return ((u8) g_PlayerEntity.hit_state) >> 7;
    }
    q = LIT(u16 *, g_ScdIp);
    x = *q;
    LIT(u16 *, g_ScdIp) = q + 1;
    return ((w[(x >> 5) & 7] << (x & 0x1F)) >> 31) ^ (x >> 8);
}

__asm__(".section .rodata\n\t.word 0\n\t.text");

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_05_bit_op(void) {
    extern u32 g_ItemUseFlags_u32[] __asm__("g_ItemUseFlags");
    u16 *p;
    u32 *w;
    u32 x;
    s32 bit;

    p = LIT(u16 *, g_ScdIp);
    LIT(u16 *, g_ScdIp) = p + 2;
    switch (p[0] >> 8) {
    case 0:
        w = LITP(u32, g_ScenarioFlags);
        break;
    case 1:
        w = LITP(u32, g_ScenarioFlags2);
        break;
    case 2:
        w = LITP(u32, g_LocksFlags);
        break;
    case 3:
        w = LITP(u32, g_EnemiesFlags);
        break;
    case 4:
        w = LITP(u32, g_SysFlags);
        break;
    case 5:
        w = LITP(u32, g_GameStateFlags);
        break;
    case 6:
        w = LITP(u32, g_UpdateFlags);
        break;
    case 7:
        w = LITP(u32, g_RoomItemsFlags);
        break;
    case 8:
        w = LITP(u32, g_RoomFlags);
        break;
    case 9:
        w = g_ItemUseFlags_u32;
        break;
    }
    x = p[1];
    w += (x & 0xE0) >> 5;
    bit = x & 0x1F;
    switch (x >> 8) {
    case 0:
        *w |= 0x80000000 >> bit;
        return 1;
    case 1:
        *w &= ~(0x80000000 >> bit);
        return 1;
    case 2:
        *w ^= 0x80000000 >> bit;
        return 1;
    }
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_06_state_byte_test(void) {
    u16 *p;
    u32 cur;
    u32 val;
    u8 op;

    p = LIT(u16 *, g_ScdIp);
    LIT(u16 *, g_ScdIp) = p + 2;
    cur = LITP(u8, g_ScdByteVars)[*p++ >> 8];
    op = *(u8 *)p;
    val = *p >> 8;
    switch (op) {
    case 0:
        return cur == val;
    case 1:
        return val < cur;
    case 2:
        return cur >= val;
    case 3:
        return cur < val;
    case 4:
        return val >= cur;
    case 5:
        return cur != val;
    }
}

/* jtbl_8001074C is 4-aligned in the original (see tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_07_state_word_test(void) {
    s32 unused[2];
    u8 *p;
    s32 cur;
    s32 val;
    u8 op;

    p = LIT(u8 *, g_ScdIp);
    LIT(u8 *, g_ScdIp) = p + 6;
    op = SCD_WORD_TEST(p)->compare;
    cur = LITP(u16, g_ScdWordVars)[SCD_WORD_TEST(p)->index];
    val = SCD_WORD_TEST(p)->value;
    switch (op) {
    case 0:
        return cur == val;
    case 1:
        return val < cur;
    case 2:
        return cur >= val;
    case 3:
        return cur < val;
    case 4:
        return val >= cur;
    case 5:
        return cur != val;
    }
}

s32 scd_op_08_state_byte_set(void) {
    u16 *p;
    u32 i;

    p = LIT(u16 *, g_ScdIp);
    LIT(u16 *, g_ScdIp) = p + 2;
    i = *p++ >> 8;
    LITP(u8, g_ScdByteVars)[i] = *(u8 *)p;
    return 1;
}

s32 scd_op_31_state_word_set(void) {
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 4;
    *(s16 *)(LITP(u8, g_ScdWordVars) + ((((u16) SCD_STATE_WORD_SET(p)->head >> 8) * 2))) = SCD_STATE_WORD_SET(p)->value;
    return 1;
}

s32 scd_op_0e_nop(void) {
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_09_cut_lock_set(void) {
    s32 cut_set_s() __asm__("cut_set");
    s32 cut;
    void **zones_addr;
    void *zones;
    void *zone;
    void *p;

    g_SavedCutId = LIT(u8, g_CutId);
    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 1;
    LIT(u8, g_CutId) = SCD_CUT_SET(p)->cut;
    zones = ((void *) ROOM->camera_switches);
    cut = LIT(u8, g_CutId);
    g_CameraSwitchZones = zones;
    zones_addr = (void **)0x800C0000;
    if (CAM_ZONE(zones)->cam_from != cut) {
        zones_addr = LITP(void *, g_CameraSwitchZones);
        do {
            zone = LIT(void *, g_CameraSwitchZones);
            LIT(void *, g_CameraSwitchZones) = zone + 0x14;
        } while (MEMBER_AT(zone, CamSwitchZone, [1].cam_from, u16) != cut);
    }
    cut_set_s();
    LIT(s32, g_GameStateFlags) |= GAME_CUT_LOCKED;
    LIT(s32, g_ScdIp) = (s32)(LIT(s32, g_ScdIp) + 1);
    return 1;
}

s32 scd_op_0a_cut_restore(void) {
    s32 cut_set_s() __asm__("cut_set");
    s32 cut;
    void **zones_addr;
    void *zones;
    void *zone;

    LIT(u8, g_CutId) = g_SavedCutId;
    zones = ((void *) ROOM->camera_switches);
    cut = LIT(u8, g_CutId);
    g_CameraSwitchZones = zones;
    zones_addr = (void **)0x800C0000;
    if (CAM_ZONE(zones)->cam_from != cut) {
        zones_addr = LITP(void *, g_CameraSwitchZones);
        do {
            zone = LIT(void *, g_CameraSwitchZones);
            LIT(void *, g_CameraSwitchZones) = zone + 0x14;
        } while (MEMBER_AT(zone, CamSwitchZone, [1].cam_from, u16) != cut);
    }
    cut_set_s();
    LIT(s32, g_GameStateFlags) &= ~GAME_CUT_LOCKED;
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_0b_message_set(void) {
    s32 set_message_display_a8(u32, u16) __asm__("set_message_display");
  int ip_addr;
  void *p;
  void **ip_ptr;
  u16 head;
  p = LIT(void *, g_ScdIp);
  head = (u16) (*((u16 *) (((s8 *) p) + 0)));
  *((void **) (ip_addr = ADDR_g_ScdIp)) = p + 2;
  if (1)
  {
  }
  set_message_display_a8(head >> 8, *((u16 *) (((s8 *) p) + 2)));
  ip_ptr = (void **) ip_addr;
  *ip_ptr += 2;
  return 1;
}

s32 scd_op_37_room_bgm_state_set(void) {
    u16 *p;
    u16 a;
    u32 b;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    ((u8 (*)[0x20])ADDR_g_RoomBgmState)[a >> 8][b & 0xFF] = b >> 8;
    return 1;
}

void scd_op_2e_nop(void) {
}

s32 scd_op_15_bgm_play(void) {
    void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
  u16 *p;
  u32 seq;
  p = LIT(u16 *, g_ScdIp);
  seq = ((u16) (*p)) >> 8;
  LIT(u16 *, g_ScdIp) = p + 1;
  SsSepPlay((s16) g_SepAccessNum, (s16) seq, 1, 1);
 do { g_BgmState |= 1 << (seq + 3); } while (0);
  return 1;
}

s32 scd_op_4a_bgm_restore(void) {
    void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    u32 bgm_state;

    LIT(s32, g_ScdIp) += 2;
    if (g_TargetBgmState != 0xFF) {
        bgm_state = (u32) g_BgmState >> 8;
        g_BgmState = bgm_state;
        if (bgm_state & 8) {
            SsSepSetVol((s16) LIT(s8, g_SepAccessNum), 0, g_SndPanVol[0][0], g_SndPanVol[0][1]);
            SsSepPlay((s16) LIT(s8, g_SepAccessNum), 0, 1, 1);
        }
        if (g_BgmState & 0x10) {
            SsSepSetVol((s16) LIT(s8, g_SepAccessNum), 1, g_SndPanVol[1][0], g_SndPanVol[1][1]);
            SsSepPlay((s16) LIT(s8, g_SepAccessNum), 1, 1, 1);
        }
        if (g_BgmState & 0x20) {
            SsSepSetVol((s16) LIT(s8, g_SepAccessNum), 2, g_SndPanVol[2][0], g_SndPanVol[2][1]);
            SsSepPlay((s16) LIT(s8, g_SepAccessNum), 2, 1, 1);
        }
    }
    return 1;
}

s32 scd_op_4b_bgm_stop_all(void) {
    s32 SsIsEos(s8, s32);
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    s32 SsSepStop(s8, s32);
    s32 SsUtAllKeyOff(s32);
    LIT(s32, g_ScdIp) += 2;
    if (g_TargetBgmState != 0xFF) {
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 0) << 0x10) != 0) {
            SsSepSetVol(LIT(s8, g_SepAccessNum), 0, 0, 0);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 1) << 0x10) != 0) {
            SsSepSetVol(LIT(s8, g_SepAccessNum), 1, 0, 0);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 2) << 0x10) != 0) {
            SsSepSetVol(LIT(s8, g_SepAccessNum), 2, 0, 0);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 0) << 0x10) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 0);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 1) << 0x10) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 1);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 2) << 0x10) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 2);
        }
        SsUtAllKeyOff(0);
        g_BgmState <<= 8;
    }
    return 1;
}

s32 scd_op_2f_snd_pan_vol_set(void) {
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    u16 *p;
    u16 a;
    u16 b;
    s32 i;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    if (g_SepAccessNum >= 0) {
        SsSepSetVol(g_SepAccessNum, a >> 8, b & 0xFF, b >> 8);
    }
    i = a >> 8;
    g_SndPanVol[i][0] = b & 0xFF;
    g_SndPanVol[i][1] = b >> 8;
    return 1;
}

s32 scd_op_16_bgm_stop(void) {
    s32 SsSepStop(s8, s32);
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    void SpuSetKey(long on_off, u_long voice_bit);
    s32 bit;
    u16 *p;
    u32 seq;

    p = LIT(u16 *, g_ScdIp);
    seq = (u16) *p >> 8;
    LIT(u16 *, g_ScdIp) = p + 1;
    bit = 1 << (seq + 3);
    if (bit & g_BgmState) {
        SsSepSetVol((s16) LIT(s8, g_SepAccessNum), (s16) seq, 0, 0);
        SsSepStop((s16) LIT(s8, g_SepAccessNum), (s16) seq);
        SpuSetKey(0, g_SndKeyOnMask == 0);
        g_BgmState &= ~bit;
        SsSepSetVol((s16) LIT(s8, g_SepAccessNum), (s16) seq, 0x5F, 0x5F);
    }
    return 1;
}

void scd_op_26_nop(void) {
}

s32 scd_op_27_snd_fade_set(void) {
    s32 sound_fade_start(u32, s32);
    sound_fade_start((u16) *LIT(u16 *, g_ScdIp) >> 8, 0x7F);
    LIT(s8 *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_43_bgm_volume_ramp(void) {
    void SsSepSetCrescendo(short sep_access_num, short seq_num, short v, long t);
    u16 ramp;
    u32 seq;
    void *p;

    p = LIT(void *, g_ScdIp);
    seq = (u16) SCD_BGM_RAMP(p)->head >> 8;
    LIT(void *, g_ScdIp) = p + 2;
    ramp = SCD_BGM_RAMP(p)->ramp;
    LIT(void *, g_ScdIp) = p + 4;
    if ((1 << (seq + 3)) & g_BgmState) {
        SsSepSetCrescendo((s16) g_SepAccessNum, (s16) seq, (s16) (s8) ramp, (s32) (ramp >> 8));
    }
    return 1;
}

s32 scd_op_17_sfx_3d_play(void) {
    s32 unused[4];
    s32 type;
    u16 w0;
    u16 w1;
    u16 w2;
    void *p;

    p = LIT(void *, g_ScdIp);
    w0 = FIELD_AT(p, ScdSfx3dPlay, ->head, u16);
    LIT(void *, g_ScdIp) = p + 2;
    w1 = FIELD_AT(p, ScdSfx3dPlay, ->sound, u16);
    LIT(void *, g_ScdIp) = p + 4;
    w2 = FIELD_AT(p, ScdSfx3dPlay, ->where, u16);
    LIT(void *, g_ScdIp) = p + 6;
    type = w2 & 0xFF;
    switch (type) {
    case 0:
        LIT(s32, sp_vec_vx) = (s32) FIELD_AT(p, ScdSfx3dPlay, ->x, s16);
        LIT(void *, g_ScdIp) = p + 8;
        LIT(s32, sp_vec_vy) = 0;
        LIT(s32, sp_vec_vz) = (s32) FIELD_AT(p, ScdSfx3dPlay, ->z, s16);
        LIT(void *, g_ScdIp) = p + 0xA;
        sound_play_se_3d(w0 >> 8, w1 & 0xFF, (s32) (w1 << 0x10) >> 0x18, LITP(void, sp_vec));
        break;
    case 1:
        LIT(void *, g_ScdIp) = p + 0xA;
        sound_play_se_3d(w0 >> 8, w1 & 0xFF, (s32) (w1 << 0x10) >> 0x18, LITP(void, g_PlayerPosX));
        break;
    case 2:
        LIT(void *, g_ScdIp) = p + 0xA;
        sound_play_se_3d(w0 >> 8, w1 & 0xFF, (s32) (w1 << 0x10) >> 0x18, (void *)(((w2 >> 8) * ENTITY_SIZE) + ADDR_g_EnemySlot0_PosX));
        break;
    case 3:
        LIT(void *, g_ScdIp) = p + 0xA;
        play_sfx(w0 >> 8, w0 >> 8, (s32) (w1 << 0x10) >> 0x18);
        break;
    }
    return 1;
}

s32 scd_op_1d_equipped_item_test(void) {
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    return INV_SLOT((g_EquippedSlot * 2) + g_ItemSlotsPointer)[-1].item_id == SCD_ITEM_OP(p)->item_id;
}

s32 scd_op_11_picked_item_test(void) {
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    return g_PickedItemId == SCD_ITEM_OP(p)->item_id;
}

s32 scd_op_10_used_item_test(void) {
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    return g_UsedItemId == SCD_ITEM_OP(p)->item_id;
}

s32 scd_op_19_model_flag_set(void) {
    extern s8 g_ItemModelTable_s8 __asm__("g_ItemModelTable");
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 4;
    **(s8 **)(&g_ItemModelTable_s8 + (SCD_MODEL_FLAG_SET(p)->index * 4)) = SCD_MODEL_FLAG_SET(p)->value;
    return 1;
}

s32 scd_op_1a_item_search(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    s32 unused[1];
    u8 *p;
    u32 i;

    i = 0;
    p = LIT(u8 *, g_ScdIp);
    LIT(u8 *, g_ScdIp) = p + 2;
    for (; i < LIT(u8, g_TotalHeldItems); i++) {
        if (p[1] == g_ItemSlotsPointer_u8p[i * 2]) {
            return 1;
        }
    }
    return 0;
}

s32 scd_op_12_room_action_reset(void) {
    void *p;
    void *action;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 0xA;
    action = (SCD_ROOM_ACTION_MOD(p)->slot * 0xC) + &g_RoomActionTable;
    ROOM_ACTION(action)->type = (u8) SCD_ROOM_ACTION_MOD(p)->type;
    ROOM_ACTION(action)->flags = (u8) SCD_ROOM_ACTION_MOD(p)->flags;
    ROOM_ACTION(action)->arg0 = (u16) SCD_ROOM_ACTION_MOD(p)->arg0;
    ROOM_ACTION(action)->arg1 = (u16) SCD_ROOM_ACTION_MOD(p)->arg1;
    ROOM_ACTION(action)->arg2 = (u16) SCD_ROOM_ACTION_MOD(p)->arg2;
    return 1;
}

s32 scd_op_13_room_action_arm(void) {
    void *p;
    void *action;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 4;
    action = (SCD_ROOM_ACTION_MOD(p)->slot * 0xC) + &g_RoomActionTable;
    ROOM_ACTION(action)->type = (u8) SCD_ROOM_ACTION_MOD(p)->type;
    ROOM_ACTION(action)->flags = (u8) SCD_ROOM_ACTION_MOD(p)->flags;
    return 1;
}

s32 scd_op_14_event_create(void) {
    s32 scd_event_entry_create_a3(s32, u32) __asm__("scd_event_entry_create");
    u16 event;
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    event = SCD_EVENT_CREATE(p)->event;
    scd_event_entry_create_a3(event & 0xFF, event >> 8);
    LIT(void *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_44_event_kill(void) {
    LITP(ScdEvent, g_ScdEventTable)[(u16) *LIT(u16 *, g_ScdIp) >> 8].active = 0;
    LIT(s8 *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_0f_mirror_set(void) {
    s32 setup_entity_joint_animation_a1() __asm__("setup_entity_joint_animation");
    s32 entity_alloc_weapon_buffers_p() __asm__("entity_alloc_weapon_buffers");
    s32 entity_setup_weapon_part_a1() __asm__("entity_setup_weapon_part");
    u8 *p;
    u16 a;
    u16 b;
    u16 c;
    s32 save;
    s32 flags;

    flags = LIT(s32, g_GameStateFlags) & ~(GAME_MIRROR_ON | GAME_MIRROR_AXIS);
    p = LIT(u8 *, g_ScdIp);
    LIT(s32, g_GameStateFlags) = flags;
    LIT(s32, g_GameStateFlags) = SCD_MIRROR(p)->mode | flags;
    a = SCD_MIRROR(p)->range_min;
    b = SCD_MIRROR(p)->range_max;
    c = SCD_MIRROR(p)->plane;
    save = LIT(s32, g_CurrentEntity);
    LIT(s32, g_CurrentEntity) = ADDR_g_PlayerEntity;
    g_MirrorRangeMin = a;
    g_MirrorRangeMax = b;
    g_MirrorPlane = c;
    setup_entity_joint_animation_a1();
    entity_alloc_weapon_buffers_p();
    entity_setup_weapon_part_a1(0xE);
    LIT(s32, g_CurrentEntity) = save;
    LIT(s32, g_ScdIp) += 8;
    return 1;
}

s32 scd_op_0d_room_action_set(void) {
    void *action;
    void *p;
    void *q;

    p = LIT(void *, g_ScdIp);
    action = (SCD_ROOM_ACTION_SET(p)->slot * 0xC) + &g_RoomActionTable;
    if ((u32) g_RoomActionTail < (u32) action) {
        g_RoomActionTail = (u32) action;
    }
    ROOM_ACTION(action)->type = (u8) SCD_ROOM_ACTION_SET(p)->type;
    ROOM_ACTION(action)->flags = (u8) SCD_ROOM_ACTION_SET(LIT(void *, g_ScdIp))->flags;
    q = LIT(void *, g_ScdIp);
    ROOM_ACTION(action)->arg0 = (u16) SCD_ROOM_ACTION_SET(q)->arg0;
    ROOM_ACTION(action)->arg1 = (u16) SCD_ROOM_ACTION_SET(q)->arg1;
    ROOM_ACTION(action)->arg2 = (u16) SCD_ROOM_ACTION_SET(q)->arg2;
    ROOM_ACTION(action)->rec = (void *) (q + 2);
    LIT(void *, g_ScdIp) = q + 0x12;
    return 1;
}

s32 scd_op_0c_door_set(void) {
  void **ip_ptr;
  int slot;
  void *action;
  void *p;
  slot = *((u8 *) (((s8 *) (LIT(void *, g_ScdIp))) + 1));
  action = (slot * 0xC) + (&g_RoomActionTable);
  if (((u32) g_RoomActionTail) < ((u32) action))
  {
    g_RoomActionTail = (u32) action;
  }
  *((s8 *) (((s8 *) action) + 0)) = 1;
  ip_ptr = LITP(void *, g_ScdIp);
  *((u8 *) (((s8 *) action) + 1)) = (u8) (*((u8 *) (((s8 *) (*ip_ptr)) + 0x19)));
  *((s16 *) (((s8 *) action) + 2)) = (s16) slot;
  p = LIT(void *, g_ScdIp);
  *((void **) (((s8 *) action) + 8)) = (void *) (p + 2);
  LIT(void *, g_ScdIp) = p + 0x1A;
  return 1;
}

s32 scd_op_18_item_model_set(void) {
    extern u8 * g_ItemModelTable_u8p[] __asm__("g_ItemModelTable");
    extern u8 * g_OmodelTable_u8p[] __asm__("g_OmodelTable");
    extern u8 g_RoomActionTable_u8[] __asm__("g_RoomActionTable");
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    extern u8 * g_ScdIp_u8p __asm__("g_ScdIp");
    void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
    s16 effect_create_billboard_l() __asm__("effect_create_billboard");
    s32 Flg_ck_l() __asm__("Flg_ck");
    u8 *ent;
    u8 *p;
    u8 *p2;
    u8 *p3;
    u8 *p4;
    u8 *p5;
    u8 *p6;
    u8 *rec;
    u8 *obj;
    u8 *coord;
    u8 kind;
    u16 flags;
    u32 f;
    s32 anim;

    kind = g_ScdIp_u8p[0xA];
    ent = &g_RoomActionTable_u8[(g_ScdIp_u8p[1] & 0x7F) * 12];
    if (kind >= 0x54) {
        ROOM_ACTION(ent)->type = Flg_ck_l(ADDR_g_RoomItemsFlags, g_ScdIp_u8p[0x16]) ? 0xD : 0;
    } else if (kind >= 0x4E) {
        ROOM_ACTION(ent)->type = Flg_ck_l(ADDR_g_RoomItemsFlags, g_ScdIp_u8p[0x16]) ? 0xF : 0;
    } else {
        ROOM_ACTION(ent)->type = Flg_ck_l(ADDR_g_RoomItemsFlags, g_ScdIp_u8p[0x16]) ? 4 : 0;
    }
    p = LIT(u8 *, g_ScdIp);
    flags = SCD_ITEM_MODEL_SET(p)->flags;
    SCD_ITEM_MODEL_SET(p)->flags = flags & 1;
    ROOM_ACTION(ent)->flags = SCD_ITEM_MODEL_SET(p)->action_flags;
    p2 = LIT(u8 *, g_ScdIp);
    ROOM_ACTION(ent)->arg0 = SCD_ITEM_MODEL_SET(p2)->flags;
    ROOM_ACTION(ent)->arg1 = SCD_ITEM_MODEL_SET(p2)->model;
    ROOM_ACTION(ent)->arg2 = SCD_ITEM_MODEL_SET(p2)->flag;
    ROOM_ACTION(ent)->rec = p2 + 2;
    f = flags & 0xFFFE;
    if (g_RoomActionTail < (u32)ent) {
        g_RoomActionTail = (u32)ent;
    }
    rec = ((u8 *) ROOM_P->item_models) + SCD_ITEM_MODEL_SET(p2)->model * 8;
    obj = g_ItemModelTable_u8p[SCD_ITEM_MODEL_SET(p2)->model];
    if (RDT_MODEL(rec)->tmd == 0) {
        MODEL_OBJ(obj)->tmd = 0;
        coord = (u8 *)&MODEL_OBJ(obj)->coord;
    } else {
        if ((g_ItemModelCount == 0 || RDT_MODEL(g_ItemModelLastRec)->tim != RDT_MODEL(rec)->tim)
            && RDT_MODEL(rec)->tim != 0) {
            g_ItemModelTpage = g_VramTpageCursor;
            g_ItemModelClutRow = g_VramClutRowCursor;
            tim_load_to_vram(RDT_MODEL(rec)->tim);
        }
        if (g_ItemModelCount == 0 || RDT_MODEL(g_ItemModelLastRec)->tmd != RDT_MODEL(rec)->tmd) {
            tmd_relocate_texture(2, RDT_MODEL(rec)->tmd, g_ItemModelTpage, g_ItemModelClutRow);
        }
        tmd_link_object(RDT_MODEL(rec)->tmd, &MODEL_OBJ(obj)->attribute, &MODEL_OBJ(obj)->coord_flg);
        p3 = g_ScdIp_u8p;
        if (p3[0xD] == 0xFF) {
            MODEL_OBJ(obj)->super = NULL;
            coord = (u8 *)&MODEL_OBJ(obj)->coord;
        } else if (p3[0xD] == 0xFE) {
            MODEL_OBJ(obj)->super = LITP(void, g_PlayerCoord2);
            coord = LITP(u8, g_PlayerCoordM00);
        } else {
            MODEL_OBJ(obj)->super = (void *)(g_OmodelTable_u8p[p3[0xD]] + 0x1C);
            coord = g_OmodelTable_u8p[p3[0xD]] + 0x20;
        }
        GsInitCoordinate2(((void *) MODEL_OBJ(obj)->super), (GsCOORDINATE2 *)&MODEL_OBJ(obj)->coord_flg);
    }
    MODEL_OBJ(obj)->billboard = 0;
    if (Flg_ck_l(ADDR_g_RoomItemsFlags, g_ScdIp_u8p[0x16]) != 0) {
        MODEL_OBJ(obj)->billboard = 0;
        if (f & 0x8000) {
            switch (f & 0xF00) {
            case 0x000:
                anim = 3;
                break;
            case 0x100:
                anim = 0xB;
                break;
            case 0x200:
                anim = 0x13;
                break;
            case 0x300:
                anim = 0x1B;
                break;
            case 0x400:
                anim = 4;
                break;
            case 0x500:
                anim = 0xC;
                break;
            case 0x600:
                anim = 0x14;
                break;
            case 0x700:
                anim = 0x1C;
                break;
            default:
                anim = 0x14;
                break;
            }
            p4 = g_ScdIp_u8p;
            if (SCD_ITEM_MODEL_SET(p4)->parent == 0xFF) {
                SCRATCH->vec_x = 0;
                SCRATCH->vec_y = -((f & 0xF0) * 2);
                SCRATCH->vec_z = 0;
            } else {
                SCRATCH->vec_x = SCD_ITEM_MODEL_SET(p4)->pos_x;
                SCRATCH->vec_y = SCD_ITEM_MODEL_SET(p4)->pos_y - (f & 0xF0) * 2;
                SCRATCH->vec_z = SCD_ITEM_MODEL_SET(p4)->pos_z;
            }
            MODEL_OBJ(obj)->billboard = effect_create_billboard_l(0xB, anim, 0, coord, ADDR_sp_vec, 0);
        }
    }
    MODEL_OBJ(obj)->flags = Flg_ck_l(ADDR_g_RoomItemsFlags, LIT(u8 *, g_ScdIp)[0x16]) != 0;
    MODEL_OBJ(obj)->rot_x = 0;
    MODEL_OBJ(obj)->index = g_ItemModelCount;
    p5 = LIT(u8 *, g_ScdIp);
    MODEL_OBJ(obj)->angle = SCD_ITEM_MODEL_SET(p5)->angle;
    MODEL_OBJ(obj)->angle_z = 0;
    MODEL_OBJ(obj)->attribute = 0x40000000;
    if (SCD_ITEM_MODEL_SET(p5)->slot & 0x80) {
        MODEL_OBJ(obj)->attribute = 0x40000040;
    }
    p6 = LIT(u8 *, g_ScdIp);
    g_ItemModelLastRec = rec;
    MODEL_OBJ(obj)->coord.t[0] = SCD_ITEM_MODEL_SET(p6)->pos_x;
    MODEL_OBJ(obj)->coord.t[1] = SCD_ITEM_MODEL_SET(p6)->pos_y;
    g_ItemModelCount++;
    MODEL_OBJ(obj)->coord.t[2] = SCD_ITEM_MODEL_SET(p6)->pos_z;
    LIT(u8 *, g_ScdIp) = p6 + 0x1A;
    return 1;
}

s32 scd_op_1b_enemy_set(void) {
    s32 restore_saved_enemy_state_a1() __asm__("restore_saved_enemy_state");
    s32 Flg_ck_a9() __asm__("Flg_ck");
    s32 slot_ofs;
    u8 killed;
    void *ent;
    void *p2;
    void *p3;
    void *p;
    void *ent3;
    void *ent6;
    void *ent2;
    void *ent4;
    void *ent5;
    void *ent7;

    killed = SCD_EM_SET(LIT(void *, g_ScdIp))->killed;
    if ((killed == 0xFF) || (Flg_ck_a9(ADDR_g_EnemiesFlags, killed) == 0)) {
        p = LIT(void *, g_ScdIp);
        slot_ofs = (SCD_EM_SET(p)->slot & 0xF) * ENTITY_SIZE;
        LIT(void *, g_CurrentEntity) = (void *)(slot_ofs + ADDR_g_EnemiesList);
        *(s32 *)(LITP(u8, g_EnemySlot0_PosY) + (slot_ofs)) = (s32) SCD_EM_SET(p)->y;
        *(slot_ofs + &LIT(s8, g_EnemySlot0_SlotInfo)) = SCD_EM_SET(p)->slot & 0xF;
        ent = LIT(void *, g_CurrentEntity);
        ENT(ent)->slot_info = (u8) (ENT(ent)->slot_info | (SCD_EM_SET(LIT(void *, g_ScdIp))->sound_set * 0x10));
        if ((SCD_EM_SET(LIT(void *, g_ScdIp))->no_restore == 0) || (ent2 = LIT(void *, g_CurrentEntity), ENT(ent2)->slot_info = (u8) (ENT(ent2)->slot_info | 0x80), (SCD_EM_SET(LIT(void *, g_ScdIp))->no_restore == 0))) {
            if (restore_saved_enemy_state_a1(SCD_EM_SET(LIT(void *, g_ScdIp))->slot, p) == 0) {
                goto block_6;
            }
        } else {
block_6:
            CURRENT_ENTITY_LIT->status_flags = 1U;
            CURRENT_ENTITY_LIT->behavior_flags = (u8) SCD_EM_SET(LIT(s32, g_ScdIp))->behavior;
            p2 = LIT(void *, g_ScdIp);
            ent3 = LIT(void *, g_CurrentEntity);
            ENT(ent3)->angle = (u16) SCD_EM_SET(p2)->angle;
            ENT(ent3)->pos_x = (s32) SCD_EM_SET(p2)->x;
            ENT(ent3)->pos_z = (s32) SCD_EM_SET(p2)->z;
            ENT(ent3)->position_vx = (u16) ENT(ent3)->pos_x;
            ENT(ent3)->position_vy = (u16) FIELD_AT(ent3, Entity, ->pos_y, u16);
            ENT(ent3)->position_vz = (u16) ENT(ent3)->pos_z;
            ENT(ent3)->animation_id = (u8) SCD_EM_SET(p2)->anim_id;
            CURRENT_ENTITY_LIT->animation_frame_id = (u8) SCD_EM_SET(LIT(s32, g_ScdIp))->anim_frame;
            CURRENT_ENTITY_LIT->timing_control = 1;
        }
        ent4 = LIT(void *, g_CurrentEntity);
        if (ENT(ent4)->status_flags & 1) {
            FIELD_AT(ent4, Entity, ->state, s32) = 0;
            ENT(ent4)->id = (u8) SCD_EM_SET(LIT(s32, g_ScdIp))->id;
            CURRENT_ENTITY_LIT->death_event_id = (u8) SCD_EM_SET(LIT(s32, g_ScdIp))->killed;
            p3 = LIT(void *, g_ScdIp);
            ent5 = LIT(void *, g_CurrentEntity);
            ENT(ent5)->rot_x = (u16) SCD_EM_SET(p3)->rot_x;
            ENT(ent5)->angle_z = (u16) SCD_EM_SET(p3)->angle_z;
            ENT(ent5)->hit_state = 0;
            ent6 = LIT(void *, g_CurrentEntity);
            ENT(ent6)->scale = 0;
            ENT(ent6)->collision_flags = 0;
            CURRENT_ENTITY_LIT->look_at_flags = 0;
            ent7 = LIT(void *, g_CurrentEntity);
            ENT(ent7)->sca_info = (s32) g_DefaultScaInfo;
            ENT(ent7)->sca_hit_data = (s32) g_ScaHitPoolPtr;
            g_ScaHitPoolPtr += SCD_EM_SET(LIT(s32, g_ScdIp))->sca_count * 6;
            LIT(u8, g_EnemyCount) += 1;
        }
    }
    LIT(s32, g_ScdIp) = (s32)(LIT(s32, g_ScdIp) + 0x16);
    return 1;
}

s32 scd_op_29_fmv_set(void) {
    u16 *p;

    LIT(u32, g_GameStateFlags) |= GAME_FMV_REQUEST;
    p = LIT(u16 *, g_ScdIp);
    g_SelectedFmvId = *p >> 8;
    g_FmvBuffer = g_LoadPointer;
    LIT(u16 *, g_ScdIp) = p + 1;
    return 1;
}

s32 scd_op_1e_voice_play(void) {
    void voice_command(s32 cmd, s32 clip);
    u16 *p;
    u16 a;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    voice_command(a >> 8, p[1]);
    LIT(s32, g_ScdIp) += 2;
    LIT(u32, g_GameStateFlags) |= GAME_VOICE_PLAYING;
    return 1;
}

s32 scd_op_1f_omodel_set(void) {
    s32 clut_fade_add_p(u8, s32) __asm__("clut_fade_add");
    s32 tmd_link_object(s32, void *, void *);
    void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
    s32 id;
    u16 z;
    u16 x;
    u16 angle;
    s16 y;
    u8 id_flags;
    s32 parent;
    u8 *q;
    void *p;
    void *obj;
    void *rec;
    void *attr;

    id = SCD_OM_SET(g_ScdIp)->id & 0x3F;
    rec = (void *)((id * 8) + ROOM->object_models);
    obj = (void *)(((s32 *)&g_OmodelTable)[id]);
    if (RDT_MODEL(rec)->tmd == 0) {
        ENT(obj)->model_load_buffer = 0;
    } else {
        if (((g_OmodelCount == 0) || (RDT_MODEL(g_OmodelLastRec)->tim != RDT_MODEL(rec)->tim)) && (RDT_MODEL(rec)->tim != 0)) {
            g_OmodelTpage = g_VramTpageCursor;
            g_OmodelClutRow = g_VramClutRowCursor;
            tim_load_to_vram(RDT_MODEL(rec)->tim);
        }
        if ((g_OmodelCount == 0) || (attr = &MODEL_OBJ(obj)->attribute, (RDT_MODEL(g_OmodelLastRec)->tmd != RDT_MODEL(rec)->tmd))) {
            tmd_relocate_texture(2, RDT_MODEL(rec)->tmd, g_OmodelTpage, g_OmodelClutRow);
            id_flags = SCD_OM_SET(g_ScdIp)->id;
            if (id_flags & 0x80) {
                clut_fade_add_p(g_OmodelClutRow, id_flags & 0xBF);
            }
            attr = &MODEL_OBJ(obj)->attribute;
        }
        tmd_link_object(RDT_MODEL(rec)->tmd, attr, &ENT(obj)->sca_matrix_head);
        q = g_ScdIp;
        parent = q[3];
        switch (parent) {
        case 0xFF:
            MODEL_OBJ(obj)->super = NULL;
            break;
        case 0xFE:
            MODEL_OBJ(obj)->super = LITP(GsCOORDINATE2, g_PlayerCoord2);
            break;
        default:
            if (parent < 0x80U) {
                MODEL_OBJ(obj)->super = (void *)(*(s32 *)((u8 *)&g_OmodelTable + ((parent * 4))) + 0x1C);
            } else {
                MODEL_OBJ(obj)->super = (void *)(((q[3] & 0x7F) * ENTITY_SIZE) + ADDR_g_EnemySlot0_Coord2);
            }
            break;
        }
        GsInitCoordinate2(MODEL_OBJ(obj)->super, (GsCOORDINATE2 *)&ENT(obj)->sca_matrix_head);
    }
    MODEL_OBJ(obj)->attribute = 0x40000000;
    if (SCD_OM_SET(LIT(void *, g_ScdIp))->flags & 0x10) {
        MODEL_OBJ(obj)->attribute = 0x40000040;
    }
    ENT(obj)->status_flags = (u8) SCD_OM_SET(LIT(void *, g_ScdIp))->flags;
    ENT(obj)->id = (s8) (SCD_OM_SET(LIT(void *, g_ScdIp))->id & 0x7F);
    ENT(obj)->rot_x = 0;
    p = LIT(void *, g_ScdIp);
    angle = SCD_OM_SET(p)->angle;
    ENT(obj)->sca_info = (s32)(&ENT(obj)->health);
    ENT(obj)->angle_z = 0;
    ENT(obj)->health = -0x8000;
    ENT(obj)->prev_angle = angle;
    ENT(obj)->angle = angle;
    FIELD_AT(obj, Entity, ->hit_state, u16) = (u16) SCD_OM_SET(p)->half_x;
    FIELD_AT(obj, Entity, ->blend_counter, u16) = (u16) SCD_OM_SET(p)->half_height;
    ENT(obj)->floor_y = (u16) SCD_OM_SET(p)->half_z;
    FIELD_AT(obj, Entity, ->anim_header, u16) = (u16) SCD_OM_SET(p)->half_height;
    MODEL_OBJ(obj)->sca_radius = (u16) SCD_OM_SET(p)->radius;
    FIELD_AT(obj, Entity, ->anim_base, u16) = (u16) SCD_OM_SET(p)->probe1_x;
    FIELD_AT(obj, Entity, ->joints, u16) = (u16) SCD_OM_SET(p)->probe1_z;
    FIELD_AT(obj, Entity, ->weapon_part_anim_slot, u16) = (u16) SCD_OM_SET(p)->probe2_x;
    FIELD_AT(obj, Entity, ->weapon_part_anim_object, u16) = (u16) SCD_OM_SET(p)->probe2_z;
    x = SCD_OM_SET(p)->x;
    ENT(obj)->position_vx = x;
    ENT(obj)->pos_x = (s32) (s16) x;
    y = SCD_OM_SET(p)->y;
    g_OmodelLastRec = rec;
    ENT(obj)->position_vy = y;
    ENT(obj)->pos_y = (s32) y;
    z = SCD_OM_SET(p)->z;
    g_OmodelCount += 1;
    ENT(obj)->position_vz = z;
    ENT(obj)->pos_z = (s32) (s16) z;
    LIT(void *, g_ScdIp) = p + 0x1C;
    return 1;
}

s32 scd_op_25_room_sprite_set(void) {
    s32 room_spr_set_inactive_a1() __asm__("room_spr_set_inactive");
    s32 room_spr_set_active_a1() __asm__("room_spr_set_active");
    u16 sprite;
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    sprite = SCD_ROOM_SPRITE_SET(p)->sprite;
    if ((sprite >> 8) != 0) {
        room_spr_set_inactive_a1(sprite & 0xFF);
    } else {
        room_spr_set_active_a1(sprite & 0xFF);
    }
    LIT(s32, g_ScdIp) = (s32)(LIT(s32, g_ScdIp) + 2);
    return 1;
}

s32 scd_op_1c_room_light_fade_set(void) {
    u16 *p;
    u16 *q;
    u16 v;
    s16 b;

    p = LIT(u16 *, g_ScdIp);
    v = *p;
    LIT(u16 *, g_ScdIp) = p + 1;
    g_Fade2BlendMode = v >> 8;
    q = p + 1;
    b = *q;
    LIT(u16 *, g_ScdIp) = p + 2;
    g_SpecialRoomLightDelta = b;
    q = p + 2;
    v = *q;
    LIT(u8, g_Fade2MaskR) = LIT(u8, g_Fade2MaskG) = LIT(u8, g_Fade2MaskB) = 0;
    if (v & 1) {
        LIT(u8, g_Fade2MaskB) = 0xFF;
    }
    if (v & 2) {
        LIT(u8, g_Fade2MaskG) = 0xFF;
    }
    if (v & 4) {
        LIT(u8, g_Fade2MaskR) = 0xFF;
    }
    LIT(u16 *, g_ScdIp) = p + 3;
    if (b != 0) {
        g_Fade2Level = -({ b < 1; }) & 0x7FFF;
    }
    return 1;
}


s32 scd_op_4f_vibrate_small(void) {
    s32 pad_vibrate_small_a2(u16, u16) __asm__("pad_vibrate_small");
    void *p;
    u16 a;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    a = SCD_VIBRATE(p)->duration;
    LIT(void *, g_ScdIp) = p + 4;
    pad_vibrate_small_a2(a, SCD_VIBRATE(p)->delay);
    LIT(void *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_50_vibrate_large(void) {
    s32 pad_vibrate_large_s(s32, s32, s32) __asm__("pad_vibrate_large");
    void *p;
    u16 c;
    u16 a;

    p = LIT(void *, g_ScdIp);
    c = SCD_VIBRATE(p)->head;
    LIT(void *, g_ScdIp) = p + 2;
    a = SCD_VIBRATE(p)->duration;
    LIT(void *, g_ScdIp) = p + 4;
    pad_vibrate_large_s(a, c >> 8, SCD_VIBRATE(p)->delay);
    LIT(void *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_51_vibrate_ramp(void) {
    u16 strength;
    u16 duration;
    void *p;

    if (((u16) SCD_VIBRATE_RAMP(LIT(void *, g_ScdIp))->head >> 8) != 0) {
        pad_vibration_reset();
    }
    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    strength = SCD_VIBRATE_RAMP(p)->strength;
    LIT(void *, g_ScdIp) = p + 4;
    duration = SCD_VIBRATE_RAMP(p)->duration;
    LIT(void *, g_ScdIp) = p + 6;
    pad_vibrate_large_ramp(duration, strength & 0xFF, strength >> 8, SCD_VIBRATE_RAMP(p)->delay);
    LIT(void *, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_52_vibrate_stop(void) {
    void pad_vibration_reset();
    LIT(s32, g_ScdIp) += 2;
    pad_vibration_reset();
    return 1;
}

s32 scd_op_20_player_pos_set(void) {
    u16 x;
    u16 y;
    u16 z;
    void *p;

    p = LIT(void *, g_ScdIp);
    g_PlayerScdEntityFlags &= 0xFFF3;
    g_PlayerRotX = FIELD_AT(p, ScdPlayerPosSet, ->rot_x, u16);
    g_PlayerAngle = FIELD_AT(p, ScdPlayerPosSet, ->angle, u16);
    g_PlayerAngleZ = FIELD_AT(p, ScdPlayerPosSet, ->angle_z, u16);
    x = FIELD_AT(p, ScdPlayerPosSet, ->x, u16);
    g_PlayerPositionVx = x;
    g_PlayerPosX = (s32) (s16) x;
    y = FIELD_AT(p, ScdPlayerPosSet, ->y, u16);
    g_PlayerPositionVy = y;
    g_PlayerPosY = (s32) (s16) y;
    z = FIELD_AT(p, ScdPlayerPosSet, ->z, u16);
    LIT(void *, g_ScdIp) = p + 0xE;
    g_PlayerPositionVz = z;
    g_PlayerPosZ = (s32) (s16) z;
    return 1;
}

s32 scd_op_21_enemy_pos_set(void) {
    u16 *p;
    s32 off;
    u16 v;

    p = LIT(u16 *, g_ScdIp);
    LIT(u16 *, g_ScdIp) = p + 7;
    off = ((s16)*p++ >> 8) * ENTITY_SIZE;
    *(u16 *)(ADDR_g_EnemySlot0_ScdEntityFlags + off) &= 0xFFF3;
    /* do {} while (0): loop-depth weight gives p priority over the offset (original registers) */
    do {
        *(u16 *)(ADDR_g_EnemySlot0_RotX + off) = *p++;
        *(u16 *)(ADDR_g_EnemySlot0_Angle + off) = *p++;
        *(u16 *)(ADDR_g_EnemySlot0_AngleZ + off) = *p++;
        v = *p++;
    } while (0);
    *(u16 *)(ADDR_g_EnemySlot0_PositionVx + off) = v;
    *(s32 *)(ADDR_g_EnemySlot0_PosX + off) = (s16)v;
    v = p[0];
    *(u16 *)(ADDR_g_EnemySlot0_PositionVy + off) = v;
    *(s32 *)(ADDR_g_EnemySlot0_PosY + off) = (s16)v;
    v = p[1];
    *(u16 *)(ADDR_g_EnemySlot0_PositionVz + off) = v;
    *(s32 *)(ADDR_g_EnemySlot0_PosZ + off) = (s16)v;
    return 1;
}

s32 scd_op_47_obj_transform_set(void) {
    extern u8 *g_OmodelTable_p[] __asm__("g_OmodelTable");
    s16 *p;
    u8 *obj;
    s16 v;

    p = LIT(s16 *, g_ScdIp);
    LIT(s16 *, g_ScdIp) = p + 7;
    obj = g_OmodelTable_p[*p++ >> 8];
    MODEL_OBJ(obj)->rot_x = *p++;
    MODEL_OBJ(obj)->angle = *p++;
    MODEL_OBJ(obj)->angle_z = *p++;
    MODEL_OBJ(obj)->position_vx = v = *p++;
    MODEL_OBJ(obj)->coord.t[0] = v;
    MODEL_OBJ(obj)->position_vy = v = *p++;
    MODEL_OBJ(obj)->coord.t[1] = v;
    MODEL_OBJ(obj)->position_vz = v = *p++;
    MODEL_OBJ(obj)->coord.t[2] = v;
    return 1;
}

/* two 4-aligned jump tables, the first followed by a pad word (tools/jtalign.awk) */
__asm__("#JTALIGN2PAD");
__asm__("#JTALIGN2");
s32 scd_op_22_item_count_test(void) {
    extern u8 *g_ItemSlotsPointer_l __asm__("g_ItemSlotsPointer");
    u8 *cmd;
    u32 i;
    u32 id;
    u32 val;
    s32 cnt;

    cmd = LIT(u8 *, g_ScdIp);
    cnt = 0;
    LIT(u8 *, g_ScdIp) = cmd + 4;
    id = *(u16 *)cmd >> 8;
    for (i = 0; i < LIT(u8, g_TotalHeldItems); i++) {
        if (id == g_ItemSlotsPointer_l[i * 2] || id == g_ItemSlotsPointer_l[i * 2] + 11) {
            val = g_ItemSlotsPointer_l[i * 2 + 1];
        }
        switch (id) {
        case 11:
            if (g_ItemSlotsPointer_l[i * 2] == 2) {
                cnt++;
                val += g_ItemSlotsPointer_l[i * 2 + 1];
            }
            break;
        case 12:
            if (g_ItemSlotsPointer_l[i * 2] == 3) {
                cnt++;
                val += g_ItemSlotsPointer_l[i * 2 + 1];
            }
            break;
        case 13:
            if (g_ItemSlotsPointer_l[i * 2] == 4 || g_ItemSlotsPointer_l[i * 2] == 5) {
                cnt++;
                val += g_ItemSlotsPointer_l[i * 2 + 1];
            }
            break;
        case 15:
            if (g_ItemSlotsPointer_l[i * 2] == 6) {
                cnt++;
                val += g_ItemSlotsPointer_l[i * 2 + 1];
            }
            break;
        case 16:
        case 17:
        case 18:
            if (g_ItemSlotsPointer_l[i * 2] == 7 || g_ItemSlotsPointer_l[i * 2] == 8 || g_ItemSlotsPointer_l[i * 2] == 9) {
                cnt++;
                val += g_ItemSlotsPointer_l[i * 2 + 1];
            }
            break;
        case 10:
            val = g_ItemSlotsPointer_l[i * 2 + 1];
            cnt++;
            break;
        }
    }
    cmd += 2;
    id = *(u16 *)cmd >> 8;
    if (cnt != 0) {
        switch (cmd[0]) {
        case 0:
            return val == id;
        case 1:
            return id < val;
        case 2:
            return val >= id;
        case 3:
            return val < id;
        case 4:
            return id >= val;
        case 5:
            return val != id;
        }
    }
    return 0;
}

s32 scd_op_23_cut_lock_write(void) {
    if (SCD_CUT_LOCK_WRITE(g_ScdIp)->lock == 0) {
        LIT(s32, g_GameStateFlags) &= ~GAME_CUT_LOCKED;
    } else {
        LIT(s32, g_GameStateFlags) = (s32) (LIT(s32, g_GameStateFlags) | GAME_CUT_LOCKED);
    }
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_24_room_action(void) {
    extern s32 (*g_RoomCheckActions[])();
    extern u8 g_RoomActionTable_u8[] __asm__("g_RoomActionTable");
    u8 *p;
    void *a;

    p = LIT(u8 *, g_ScdIp);
    LIT(u8 *, g_ScdIp) = p + 4;
    a = &g_RoomActionTable_u8[p[1] * 12];
    g_RoomCheckActions[p[2]](a);
    return 1;
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_28_enemy_prop_set(void) {
    u8 *pc;
    u8 *e;
    u8 *p;
    u16 w;
    u16 n;

    pc = LIT(u8 *, g_ScdIp);
    pc += 2;
    w = *(u16 *)pc;
    e = LITP(u8, g_EnemiesList) + (w & 0xFF) * ENTITY_SIZE;
    pc += 2;
    switch ((s8)(w >> 8)) {
    case 0:
        ENT(e)->behavior_flags = pc[0];
        LIT(u8 *, g_ScdIp) += 6;
        break;
    case 1:
        ENT(e)->state = 2;
        ENT(e)->sub_state = 0;
        ENT(e)->action_behavior = 0;
        ENT(e)->action_state = 0;
        ENT(e)->health = *(u16 *)pc;
        ENT(e)->hit_state = pc[2];
        LIT(u8 *, g_ScdIp) += 8;
        break;
    case 2:
        ENT(e)->action_behavior = pc[0];
        ENT(e)->action_state = 0;
        LIT(u8 *, g_ScdIp) += 6;
        break;
    case 3:
        switch ((s8)(*(u16 *)pc >> 8)) {
        case 0:
            ENT(e)->status_flags = pc[0];
            break;
        case 1:
            ENT(e)->status_flags |= pc[0];
            break;
        case 2:
            ENT(e)->status_flags ^= pc[0];
            break;
        }
        LIT(u8 *, g_ScdIp) += 6;
        break;
    case 5:
        ENT(e)->angle = *(u16 *)pc;
        LIT(u8 *, g_ScdIp) += 6;
        break;
    case 6:
        ENT(e)->blend_counter = 0;
        LIT(u8 *, g_ScdIp) += 4;
        break;
    case 8:
        ENT(e)->state = 9;
        ENT(e)->sub_state = 0;
        ENT(e)->action_behavior = 0;
        ENT(e)->action_state = 0;
        LIT(u8 *, g_ScdIp) += 4;
        break;
    case 9:
        n = *(u16 *)pc;
        p = ((u8 *) ENT(e)->joints);
        while (n != 0) {
            *p ^= n & 1;
            n >>= 1;
            p += 0x7C;
        }
        LIT(u8 *, g_ScdIp) += 6;
        break;
    case 10:
        ENT(e)->action_state = pc[0];
        LIT(u8 *, g_ScdIp) += 6;
        break;
    }
    return 1;
}
__asm__(".section .rodata\n\t.word 0\n\t.text");

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_33_player_prop_set(void) {
    u8 *w = LITP(u8, g_PlayerEntity);
    u16 *p = g_ScdIp;
    u16 n;
    u8 *e;
    u8 v;
    s32 *pp;

    switch ((s16)*p++ >> 8) {
    case 1:
        pp = LITP(s32, g_ScdIp);
        v = *p;
        ENTITY_STATE_WORD(&g_PlayerEntity) = 0x01000001;
        g_PlayerEntity.animation_frame_id = 0;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.animation_id = 0;
        g_PlayerEntity.blend_counter = 3;
        g_PlayerEntity.action_ticks_counter = 100;
        g_PlayerEntity.move_speed_current = 0;
        ENT(w)->hit_state = v;
        *pp += 4;
        break;
    case 0:
        g_EquippedSlot = 0;
        ENT(w)->behavior_flags = 0;
        LIT(s32, g_ScdIp) += 2;
        break;
    case 3:
        switch ((s16)*p >> 8) {
        case 0:
            ENT(w)->status_flags = *p;
            break;
        case 1:
            ENT(w)->status_flags |= *p;
            break;
        case 2:
            ENT(w)->status_flags ^= *p;
            break;
        }
        LIT(s32, g_ScdIp) += 4;
        break;
    case 4:
        ENT(w)->action_behavior = 1;
        ENT(w)->action_state = 6;
        LIT(s32, g_ScdIp) += 2;
        break;
    case 5:
        ENT(w)->angle = *p;
        LIT(s32, g_ScdIp) += 4;
        break;
    case 6:
        ENT(w)->blend_counter = 0;
        LIT(s32, g_ScdIp) += 2;
        break;
    case 7:
        FIELD_AT(w, Entity, ->state, u16) = 1;
        FIELD_AT(w, Entity, ->action_behavior, u16) = 0x200;
        ENT(w)->hit_state = 0;
        ENT(w)->animation_frame_id = 0;
        ENT(w)->timing_control = 0;
        ENT(w)->animation_id = 0;
        ENT(w)->blend_counter = 3;
        LIT(s32, g_ScdIp) += 2;
        break;
    case 8:
        switch ((s16)*p >> 8) {
        case 0:
            ENT(w)->collision_flags = *p;
            break;
        case 1:
            ENT(w)->collision_flags |= *p;
            break;
        case 2:
            ENT(w)->collision_flags ^= *p;
            break;
        }
        LIT(s32, g_ScdIp) += 4;
        break;
    case 9:
        n = *p;
        e = ((u8 *) ENT(w)->joints);
        while (n != 0) {
            *e ^= n & 1;
            n >>= 1;
            e += 0x7C;
        }
        LIT(s32, g_ScdIp) += 4;
        break;
    case 10:
        if (*p >> 8) {
            ENT(w)->scd_entity_flags |= 0x40;
        } else {
            ENT(w)->scd_entity_flags &= ~0x40;
        }
        LIT(s32, g_ScdIp) += 4;
        break;
    }
    return 1;
}
/* jtbl_800107D4 ends in a .word 0 pad */
__asm__(".section .rodata\n\t.word 0\n\t.text");

s32 scd_op_2a_effect_spawn(void) {
    extern u8 * g_OmodelTable_u8p[] __asm__("g_OmodelTable");
    extern u8 g_IdentityMatrix_u8[] __asm__("g_IdentityMatrix");
    s32 effect_create_billboard_t(u8, u8, s16, void *, s32 *, s32) __asm__("effect_create_billboard");
    s32 v[3];
    u8 *dst;
    s16 *p;
    s16 op;
    u16 b;
    s32 k;
    u16 x;

    p = g_ScdIp;
    x = *p++;
    b = *p++;
    v[0] = *p++;
    op = (s16)x >> 8;
    v[1] = *p++;
    v[2] = *p++;
    k = b >> 8;
    switch ((b >> 8)) {
    case 0:
        dst = g_IdentityMatrix_u8;
        break;
    case 1:
        dst = LITP(u8, g_PlayerCoordM00);
        break;
    default:
        if ((b >> 8) & 0x80) {
            dst = g_OmodelTable_u8p[(b >> 8) & 0x7F] + 0x20;
        } else {
            dst = LITP(u8, g_EnemySlotBase2_CoordMatrix) + (b >> 8) * ENTITY_SIZE;
        }
        break;
    }
    effect_create_billboard_t(op, b, *p, dst, v, 0);
    LIT(s32, g_ScdIp) += 0xC;
    return 1;
}

s32 scd_op_3d_bullet_effect_spawn(void) {
    extern void * g_OmodelTable_voidp[] __asm__("g_OmodelTable");
    s32 effect_create_billboard_t(u8, u8, s16, void *, s32 *, s32) __asm__("effect_create_billboard");
    s32 v[3];
    s16 *p;
    u16 a;
    u16 b;
    s16 id;
    void *obj;

    p = g_ScdIp;
    a = *p++;
    b = *p++;
    v[0] = *p++;
    id = (s16)a >> 8;
    v[1] = *p++;
    v[2] = *p++;
    switch (b >> 8) {
    case 0:
        obj = &g_IdentityMatrix;
        break;
    case 1:
        obj = LITP(void, g_PlayerCoordM00);
        break;
    default:
        if ((b >> 8) & 0x80) {
            obj = (u8 *)g_OmodelTable_voidp[(u16)id >> 8] + 0x20;
        } else {
            obj = LITP(u8, g_EnemySlotBase2_CoordMatrix) + ((u16)id >> 8) * ENTITY_SIZE;
        }
        break;
    }
    effect_create_billboard_t(id, b, *p, obj, v, 0);
    g_BulletEffectId = id;
    g_BulletEffectParent = (s32)obj;
    LIT(s32, g_ScdIp) += 0xC;
    return 1;
}

s32 scd_op_3e_bullet_effect_clear(void) {
    s32 effect_free_matching_a1(s32, u8, s32, s32, s32) __asm__("effect_free_matching");
    effect_free_matching_a1(9, g_BulletEffectId, 0, 0, g_BulletEffectParent);
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

s32 scd_op_42_effect_clear_typed(void) {
    s32 effect_free_matching_a2(s32, u32, s32, s32, s32) __asm__("effect_free_matching");
    u16 *p;
    u16 b;
    u32 a;

    p = LIT(u16 *, g_ScdIp);
    /* p++; p--; was found by decomp-permuter: it keeps the original load order */
    p++;
    p--;
    a = p[0];
    b = p[1];
    LIT(s32, g_ScdIp) += 4;
    effect_free_matching_a2(3, a >> 8, b & 0xFF, 0, 0);
    return 1;
}

s32 scd_op_48_effect_pool_clear(void) {
    s32 i;

    g_FreeEffectSlots = 0;
    LIT(s32, g_ScdIp) += 2;
    do {
        i = LIT(u8, g_FreeEffectSlots);
        LITP(Effect, g_EffectPool)[i].post_behavior = 0;
        LITP(Effect, g_EffectPool)[i].pre_behavior = 0;
        LIT(u8, g_FreeEffectSlots) += 1;
    } while (LIT(u8, g_FreeEffectSlots) < 0x40);
    return 1;
}

typedef struct {
    u16 anim_ids;
    u8 pad2[0xC];
    u16 draw_flags;
    u8 pad10[0x74];
} ScdEffectFlagsView;

s32 scd_op_4e_effect_flags_modify(void) {
    u16 **pp;
    u32 op;
    u32 val;
    u32 i;
    u16 *p;

    pp = LITP(u16 *, g_ScdIp);
    p = *pp;
    op = p[0] >> 8;
    *pp = p + 1;
    val = p[1];
    *pp = p + 2;
    for (i = 0; i < 0x40; i++) {
        if (LITP(ScdEffectFlagsView, g_EffectPool)[i].anim_ids != 0) {
            switch (op) {
            case 0:
                LITP(ScdEffectFlagsView, g_EffectPool)[i].draw_flags = LITP(ScdEffectFlagsView, g_EffectPool)[i].draw_flags | val;
                break;
            case 1:
                LITP(ScdEffectFlagsView, g_EffectPool)[i].draw_flags &= ~val;
                break;
            case 2:
                LITP(ScdEffectFlagsView, g_EffectPool)[i].draw_flags = LITP(ScdEffectFlagsView, g_EffectPool)[i].draw_flags ^ val;
                break;
            }
        }
    }
    return 1;
}

s32 scd_op_2b_attack_anim_set(void) {
    s32 *pc = LITP(s32, g_ScdIp);
    u16 a;
    u16 b;
    u16 *p;
    s32 t;
    s32 x;

    do {
        /* volatile: the original loads the bytecode pointer twice */
        t = *(volatile s32 *)pc + 4;
        p = *(u16 **)pc;
        a = p[0];
        x = a;
        b = p[1];
        *pc = t;
        g_PlayerEntity.state = 8;
        g_PlayerEntity.timing_control = 0;
        g_PlayerEntity.blend_counter = 0;
    } while (0);
    x += 0x200;
    x &= 0xFF00;
    g_PlayerActionBehavior = x;
    g_PlayerAnimationId = b;
    b >>= 8;
    g_PlayerAnimationFrameId = b;
    return 1;
}

s32 scd_op_2c_item_remove(void) {
    s32 rearrange_item_slots_a3() __asm__("rearrange_item_slots");
    s32 slot;
    void *p;

    p = LIT(void *, g_ScdIp);
    LIT(void *, g_ScdIp) = p + 2;
    slot = get_item_slot(SCD_ITEM_OP(p)->item_id);
    if (slot >= 0) {
        *(s8 *)((slot * 2) + g_ItemSlotsPointer) = 0;
        rearrange_item_slots_a3();
        return 1;
    }
    return 0;
}

s32 scd_op_2d_got_item(void) {
    s32 scd_op_24_room_action();
    scd_op_24_room_action();
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) | MENU_REQUEST(MENU_MODE_GOT_ITEM)) ^ MENU_REQUEST(MENU_MODE_PICKUP);
    return 0;
}

s32 scd_op_30_boundary_set(void) {
    u16 v;
    u16 *p;
    u8 *obj;

    p = LIT(u16 *, g_ScdIp);
    v = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    obj = ((u8 *) ROOM_HDR(g_RoomData)->collision) + (v >> 8) * 4;
    v = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    obj = (v & 0xFF) * 0xC + FIELD_AT(obj, RoomBoundary, ->x_min, u8 *);
    if (v >> 8) {
        BOUNDARY(obj)->flags = (BOUNDARY(obj)->flags & 0xF0FF) | (v & 0xF00);
    }
    v = *LIT(u16 *, g_ScdIp)++;
    BOUNDARY(obj)->x_min = v;
    v = *LIT(u16 *, g_ScdIp)++;
    BOUNDARY(obj)->z_min = v;
    v = *LIT(u16 *, g_ScdIp)++;
    BOUNDARY(obj)->x_max = v;
    v = *LIT(u16 *, g_ScdIp)++;
    BOUNDARY(obj)->z_max = v;
    return 1;
}

s32 scd_op_32_obj_clut_set(void) {
    extern u8 *g_OmodelTable_p[] __asm__("g_OmodelTable");
    u16 *p;
    u16 a;
    u16 b;
    u8 *obj;
    u8 *q;
    u8 *r;
    s32 n;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    obj = g_OmodelTable_p[a >> 8];
    q = ((u8 *) MODEL_OBJ(obj)->packet);
    n = *(s32 *)(((u8 *) MODEL_OBJ(obj)->tmd) + 0x14) * 2;
    r = q + 8;
    a = (POLY_FT4_P(q)->tpage & 0xFFC0) | (b & 0xFF);
    goto test;
loop:
    FIELD_AT(r - 8, POLY_FT4, ->tpage, s16) = a;
    r += 0x28;
test:
    if (n-- != 0) {
        goto loop;
    }
    return 1;
}

s32 scd_op_34_model_tint_set(void) {
    void clut_fade_add_delta_l(u8, u8, u8, u8, u8, u8) __asm__("clut_fade_add_delta");
    void clut_fade_set_l(u8, u8, u8, u8, u8, u8) __asm__("clut_fade_set");
    void clut_fade_clear_l(u8, u8, u8) __asm__("clut_fade_clear");
    u8 **pp;
    s8 op;
    u8 b;
    u8 c;
    u8 d;
    u8 e;
    u8 f;
    u8 g;

    pp = LITP(u8 *, g_ScdIp);
    (*pp)++;
    op = *(*pp)++;
    b = *(*pp)++ - 0x80;
    c = *(*pp)++;
    d = *(*pp)++;
    e = *(*pp)++;
    f = *(*pp)++;
    g = *(*pp)++;
    switch (op) {
    case 0:
        clut_fade_add_delta_l(e, f, g, c, d, b);
        break;
    case 1:
        clut_fade_set_l(e, f, g, c, d, b);
        break;
    case 2:
        clut_fade_clear_l(b, c, d);
        break;
    }
    return 1;
}

s32 scd_op_35_obj_flag_set(void) {
    extern u8 * g_ItemModelTable_u8p[] __asm__("g_ItemModelTable");
    extern u8 * g_OmodelTable_u8p[] __asm__("g_OmodelTable");
    u16 *p;
    u16 a;
    u16 b;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    switch (a >> 8) {
    case 0:
        *g_OmodelTable_u8p[b & 0xFF] = b >> 8;
        break;
    case 1:
        *g_ItemModelTable_u8p[b & 0xFF] = b >> 8;
        break;
    }
    return 1;
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
s32 scd_op_36_obj_field_test(void) {
    extern s32 g_OmodelTable_s[] __asm__("g_OmodelTable");
    u16 *p;
    u16 a;
    u16 b;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    switch (b & 0xFF) {
    case 0:
        return MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard == (b >> 8);
    case 1:
        return (u32)(b >> 8) < MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard;
    case 2:
        return MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard >= (u32)(b >> 8);
    case 3:
        return MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard < (u32)(b >> 8);
    case 4:
        return (u32)(b >> 8) >= MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard;
    case 5:
        return MODEL_OBJ(g_OmodelTable_s[a >> 8])->billboard != (b >> 8);
    default:
        return 0;
    }
}

s32 scd_op_38_dpad_test(void) {
    u16 *p;
    u16 a;
    u16 b;

    p = LIT(u16 *, g_ScdIp);
    a = p[0];
    LIT(u16 *, g_ScdIp) = p + 1;
    b = p[1];
    LIT(u16 *, g_ScdIp) = p + 2;
    if ((a >> 8) != 0) {
        return (b & g_PadHeld) == 0;
    }
    return b & g_PadHeld;
}

s32 scd_op_39_enemy_flags_get(void) {
    u16 *p;
    u8 v;

    p = LIT(u16 *, g_ScdIp);
    g_ScdLastEnemyFlags = LITP(u8, g_EnemySlot0_BehaviorFlags)[(*p >> 8) * ENTITY_SIZE];
    LIT(u16 *, g_ScdIp) = p + 1;
    return 1;
}

s32 scd_op_3a_cut_zone_set(void) {
    void *p;

    p = LIT(void *, g_ScdIp);
    CAM_ZONE((SCD_CUT_ZONE_SET(p)->zone * 0x14) + ROOM->camera_switches)->cam_from = (s16) SCD_CUT_ZONE_SET(p)->cam_from;
    CAM_ZONE((SCD_CUT_ZONE_SET(p)->zone * 0x14) + ROOM->camera_switches)->cam_to = (s16) SCD_CUT_ZONE_SET(p)->cam_to;
    LIT(void *, g_ScdIp) = p + 4;
    return 1;
}

s32 scd_op_3b_obj_rotation_set(void) {
    s32 unused[2];
    u32 head;
    u32 head16;
    void *p;
    void *q;
    void *obj;

    p = LIT(void *, g_ScdIp);
    head = ((u16 *)p)[0];
    LIT(void *, g_ScdIp) = p + 2;
    head16 = head & 0xFFFF;
    if ((head >> 0xF) == 0) {
        obj = (void *)(((s32 *)&g_ItemModelTable)[(head16 >> 8)]);
    } else {
        obj = (void *)(*(s32 *)((u8 *)&g_OmodelTable + (((head16 >> 6) & 0x1FC))));
    }
    if (MODEL_OBJ(obj)->flags != 0) {
        q = LIT(void *, g_ScdIp);
        MODEL_OBJ(obj)->rot_x = ((u16 *)q)[0];
        LIT(s32, g_ScdIp) = (s32)(q + 2);
        MODEL_OBJ(obj)->angle_z = ((u16 *)q)[1];
        LIT(s32, g_ScdIp) = (s32)(q + 4);
    } else {
        LIT(s32, g_ScdIp) = (s32)(LIT(s32, g_ScdIp) + 4);
    }
    return 1;
}

s32 scd_op_3c_player_dist_test(void *pos_in) {
    long SquareRoot0(long a);
    s32 dx;
    s32 dz;
    void *args;
    void *pos;

    pos = pos_in;
    args = g_ScdIp + 2;
    switch ((FIELD_AT(g_ScdIp, ScdPlayerDistTest, ->target, u8))) {
    case 0:
        pos = (void *)(((((u16) SCD_PLAYER_DIST_TEST(g_ScdIp)->target) >> 8) * ENTITY_SIZE) + ADDR_g_EnemySlot0_PosX);
        break;
    case 1:
        pos = (void *)(((s32 *)&g_OmodelTable)[((u16) SCD_PLAYER_DIST_TEST(g_ScdIp)->target >> 8)] + 0x34);
        break;
    case 2:
        pos = (void *)(((s32 *)&g_ItemModelTable)[((u16) SCD_PLAYER_DIST_TEST(g_ScdIp)->target >> 8)] + 0x34);
        break;
    }
    dx = g_PlayerEntity.pos_x - VEC(pos)->vx;
    dz = g_PlayerEntity.pos_z - VEC(pos)->vz;
    LIT(s32, g_ScdIp) += 6;
    return (s32) SCD_PLAYER_DIST_TEST(args - 2)->distance >= SquareRoot0((dx * dx) + (dz * dz));
}

s32 scd_op_3f_player_dir_test(void) {
    u8 *p;

    p = LIT(u8 *, g_ScdIp);
    LIT(u8 *, g_ScdIp) = p + 6;
    /* volatile: the original reads p[2] twice (GCC would merge two plain reads) */
    return ((g_PlayerEntity.angle - FIELD_AT(p, ScdOpPlayerDirTest, ->angle_from, volatile u16)) & 0xFFFF) <= SCD_DIR_TEST(p)->angle_to - SCD_DIR_TEST(p)->angle_from;
}

s32 scd_op_40_light_param_set(void) {
    s16 *p;
    u8 *e;
    s32 idx;

    p = LIT(s16 *, g_ScdIp);
    LIT(s16 *, g_ScdIp) = p + 8;
    idx = (s8)(*(u16 *)p++ >> 8);
    e = g_RoomData + ({ idx * 0x2C + 0x94; });
    RDT_CAM(e)->from_x = *p++;
    RDT_CAM(e)->from_y = *p++;
    RDT_CAM(e)->from_z = *p++;
    RDT_CAM(e)->to_x = *p++;
    RDT_CAM(e)->to_y = *p++;
    RDT_CAM(e)->to_z = *p++;
    RDT_CAM(e)->fov = *p;
    return 1;
}

s32 scd_op_41_entity_posy_set(void) {
    u32 target;
    void *p;

    p = LIT(void *, g_ScdIp);
    target = (u16) SCD_ENTITY_POSY_SET(p)->head >> 8;
    LIT(void *, g_ScdIp) = p + 2;
    if (target == 0) {
        g_PlayerEntity.floor_y = SCD_ENTITY_POSY_SET(p)->y;
    } else {
        *(s16 *)(LITP(u8, g_EnemySlotBase1_FloorY) + ((target * ENTITY_SIZE))) = SCD_ENTITY_POSY_SET(p)->y;
    }
    LIT(s32, g_ScdIp) = (s32)(LIT(s32, g_ScdIp) + 2);
    return 1;
}

s32 scd_op_45_player_posy_add(void) {
    u16 *p;

    p = LIT(s16 *, g_ScdIp);
    g_PlayerFloorY += (u32) *(s16 *)p / 256;
    LIT(s16 *, g_ScdIp) = p + 1;
    return 1;
}

s32 scd_op_46_room_lights_set(void) {
    extern u8 *g_RoomData_u8p __asm__("g_RoomData");
    void GsSetAmbient(long r, long g, long b);
    u32 i;
    s32 off;
    u8 *p;
    u8 *d;
    u8 *d2;
    u8 *p2;

    LIT(u8 *, g_ScdIp) += 2;
    for (i = 0; i < 3; i++) {
        off = i * 20;
        d = LIT(u8 *, g_RoomData);
        d += off;
        p = LIT(u8 *, g_ScdIp);
        ROOM_HDR(d)->light_x = SCD_LIGHT(p)->x;
        ROOM_HDR(d)->light_y = SCD_LIGHT(p)->y;
        ROOM_HDR(d)->light_z = SCD_LIGHT(p)->z;
        ROOM_HDR(d)->light_r = SCD_LIGHT(p)->r;
        *(u8 *)(LIT(u8 *, g_RoomData) + off + 0x19) = *(u8 *)(LIT(u8 *, g_ScdIp) + 7);
        *(u8 *)(LIT(u8 *, g_RoomData) + off + 0x1A) = *(u8 *)(LIT(u8 *, g_ScdIp) + 8);
        d2 = LIT(u8 *, g_RoomData);
        d2 += off;
        p2 = LIT(u8 *, g_ScdIp);
        ROOM_HDR(d2)->light_type = SCD_LIGHT(p2)->type;
        ROOM_HDR(d2)->light_radius = SCD_LIGHT(p2)->radius;
        LIT(u8 *, g_ScdIp) = p2 + 0xC;
    }
    for (i = 0; i < 3; i++) {
        *(u16 *)(LIT(u8 *, g_RoomData) + i * 2 + 6) = *LIT(u16 *, g_ScdIp);
        LIT(u8 *, g_ScdIp) += 2;
    }
    GsSetAmbient(ROOM_P->ambient_r, ROOM_P->ambient_g, ROOM_P->ambient_b);
    return 1;
}

s32 room_sprite_apply_hide_mask(void) {
    extern u8 g_RoomSpriteActive[][0x24];
    s32 unused[1];
    u32 bits;
    u32 i;
    u32 j;

    bits = g_RoomSpriteHideMask;
    for (i = 0; i < 16; i++) {
        if (bits & 1) {
            for (j = 0; j < *LIT(u8 *, g_RoomData); j++) {
                if (g_RoomSpriteActive[j][1] == i) {
                    g_RoomSpriteActive[j][0] = 0;
                }
            }
        }
        bits >>= 1;
    }
    g_RoomSpriteHideMask = 0;
    return 1;
}

s32 scd_op_49_room_sprite_hide(void) {
    u16 *p;
    u32 sprite;

    p = LIT(u16 *, g_ScdIp);
    sprite = (u16) *p >> 8;
    LIT(u16 *, g_ScdIp) = p + 1;
    if (sprite == 0xFF) {
        g_RoomSpriteHideMask = 0;
    } else {
        g_RoomSpriteHideMask |= 1 << sprite;
    }
    return 1;
}

s32 scd_op_4c_item_record_transfer(void) {
    extern RoomAction g_RoomActionTable_l[] __asm__("g_RoomActionTable");
    s32 slot;
    u16 target;
    u8 v;
    s32 rec;

    target = SCD_ITEM_RECORD_TRANSFER(g_ScdIp)->target;
    switch (((u16) SCD_ITEM_RECORD_TRANSFER(g_ScdIp)->head >> 8)) {
    case 1:
        SCD_ZONE(g_RoomActionTable_l[target & 0xFF].rec)->item_count = (u8) *(u8 *)((u8 *)&LIT(s8, g_StageId) + ((target >> 8)));
        break;
    case 0:
        *(u8 *)((u8 *)&LIT(s8, g_StageId) + ((target >> 8))) = SCD_ZONE(g_RoomActionTable_l[target & 0xFF].rec)->item_count;
        break;
    case 2:
        slot = get_item_slot(SCD_ZONE(g_RoomActionTable_l[target & 0xFF].rec)->item_id);
        if (slot >= 0) {
            rec = (s32)g_RoomActionTable_l[target & 0xFF].rec;
            v = INV_SLOT(((slot * 2) + g_ItemSlotsPointer))->count;
            *(u8 *)((u8 *)&LIT(s8, g_StageId) + ((target >> 8))) = v;
            SCD_ZONE(rec)->item_count = v;
        }
        break;
    }
    LIT(s32, g_ScdIp) += 4;
    return 1;
}

s32 scd_op_4d_player_joint_tint(void) {
    s32 joints;

    joints = g_PlayerEntity.joints;
    joint_apply_color_tint(joints, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x7C, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0xF8, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x45C, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x5D0, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x174, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x1F0, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x26C, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x2E8, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x364, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x3E0, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x4D8, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x554, 0x30, 0x80820, 0x606060);
    joint_apply_color_tint(joints + 0x64C, 0x30, 0x80820, 0x606060);
    LIT(s32, g_ScdIp) += 2;
    return 1;
}

void Flg_on(s32 flags, u32 bit) {
    s32 *word;

    word = (s32 *)(((bit >> 5) * 4) + flags);
    *word |= 0x80000000U >> (bit & 0x1F);
}

void Flg_off(s32 flags, u32 bit) {
    s32 *word;

    word = (s32 *)(((bit >> 5) * 4) + flags);
    *word &= ~(0x80000000U >> (bit & 0x1F));
}

s32 Flg_ck(s32 *flags, u32 bit) {
    return flags[bit >> 5] & (0x80000000U >> (bit & 0x1F));
}

void scd_run_command_functions(u8 *script) {
    extern u8 * g_ScdIp_u8p __asm__("g_ScdIp");
    extern s32 (*g_ScdOpcodeTable[])(void);
    u8 *p;
    u16 len;

    p = script;
    g_ScdBranchSp = ADDR_g_ScdBranchStack;
    g_ScdContinueCount = 0;
    g_ScdIp_u8p = p;
    while ((len = *(u16 *)p) != 0) {
        LIT(s32, g_ScdIp) += 2;
loop:
 while (g_ScdOpcodeTable[*g_ScdIp_u8p]() != 0) {}
 if (LIT(u8, g_ScdContinueCount) != 0) { LIT(s32, g_ScdBranchSp) -= 4; g_ScdIp_u8p = *LIT(u8 **, g_ScdBranchSp); LIT(u8, g_ScdContinueCount)--; goto loop; }
        p += len;
        g_ScdIp_u8p = p;
    }
}
