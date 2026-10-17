/* sound.c - Sound: banks, BGM, sound effects (play_sfx, sound_play_se_3d), footsteps, fades.
   (Stage start and overlay loading: stage_load.c.) */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void spiderweb_update(void);
void load_file(s32 a, s32 b);
s32 calc_angle_xz(s32 x0, s32 z0, s32 x1, s32 z1);
u8 lookup_footstep_zone(s16 x, s16 y);
void load_stage_overlay(s32 slot, s32 file);
void task_sleep(s16 frames);
void room_init(void);


void sound_init() {
    void sound_start(void);
    void SsInit(void);
    void SsSetTableSize(char *table, short s_max, short t_max);
    void SsSetTickMode(long tick_mode);
    void SsUtReverbOn(void);
    short SsUtSetReverbType(short type);
    void SsUtSetReverbDepth(short ldepth, short rdepth);
    long SpuClearReverbWorkArea(long rev_mode);
    g_SndKeyOnMask = 0;
    g_SndVabId0 = -1;
    g_BgmState = 0xFF;
    g_SndVabId1 = -1;
    g_SndVabIdRoom = -1;
    g_SndVabId3 = -1;
    g_SndVabIdBgm = -1;
    g_SepAccessNum = -1;
    g_SndPanVol[0][0] = 0x5F;
    g_SndPanVol[0][1] = 0x5F;
    g_SndPanVol[1][0] = 0x5F;
    g_SndPanVol[1][1] = 0x5F;
    g_SndPanVol[2][0] = 0x5F;
    g_SndPanVol[2][1] = 0x5F;
    SsInit();
    SsSetTableSize(&g_SsSeqTable, 1, 3);
    SsSetTickMode(1);
    SsUtSetReverbType(1);
    do {

    } while (SpuClearReverbWorkArea(1) == -1);
    SsUtReverbOn();
    SsUtSetReverbDepth(0x1E, 0x1E);
    sound_start();
}

void sound_start(void) {
    s32 SsSetMVol(s32, s32);
    s32 SsSetRVol(s32, s32);
    s32 SsSetSerialAttr(s32, s32, s32);
    s32 SsSetSerialVol(s32, s32, s32);
    s32 SsStart();
    SsStart();
    SsSetMVol(0x7F, 0x7F);
    SsSetRVol(0x50, 0x50);
    SsSetSerialAttr(0, 0, 0);
    SsSetSerialVol(0, 0x7F, 0x7F);
}

void sound_shutdown(void) {
    s32 SsEnd();
    s32 SsQuit();
    void SsSepClose(short sep_access_num);
    s32 SsUtReverbOff();
    void SsUtSetReverbDepth(short ldepth, short rdepth);
    short SsUtSetReverbType(short type);
    void SsVabClose(short vab_id);
    s8 sep;
    s8 vab0;
    s8 vab1;
    s8 vab_room;
    s8 vab3;
    s8 vab_bgm;

    sep = LIT(s8, g_SepAccessNum);
    if (sep >= 0) {
        SsSepClose(sep);
        LIT(s8, g_SepAccessNum) = -1;
    }
    vab0 = LIT(s8, g_SndVabId0);
    if (vab0 >= 0) {
        SsVabClose(vab0);
        LIT(s8, g_SndVabId0) = -1;
    }
    vab1 = LIT(s8, g_SndVabId1);
    if (vab1 >= 0) {
        SsVabClose(vab1);
        LIT(s8, g_SndVabId1) = -1;
    }
    vab_room = LIT(s8, g_SndVabIdRoom);
    if (vab_room >= 0) {
        SsVabClose(vab_room);
        LIT(s8, g_SndVabIdRoom) = -1;
    }
    vab3 = LIT(s8, g_SndVabId3);
    if (vab3 >= 0) {
        SsVabClose(vab3);
        LIT(s8, g_SndVabId3) = -1;
    }
    vab_bgm = LIT(s8, g_SndVabIdBgm);
    if (vab_bgm >= 0) {
        SsVabClose(vab_bgm);
        LIT(s8, g_SndVabIdBgm) = -1;
    }
    SsUtReverbOff();
    SsUtSetReverbType(0);
    SsUtSetReverbDepth(0, 0);
    SsEnd();
    SsQuit();
}

void sound_reinit(void) {
    s32 sound_init_a2() __asm__("sound_init");
    sound_init_a2();
}

void sound_close_bank3(void) {
    void SsVabClose(short vab_id);
    s8 vab;

    vab = LIT(s8, g_SndVabId3);
    if (vab >= 0) {
        SsVabClose(vab);
        LIT(s8, g_SndVabId3) = -1;
    }
}

void sound_end(void) {
    s32 SsEnd();
    s32 SsQuit();
    SsEnd();
    SsQuit();
}

void sound_set_stereo(u32 stereo) {
    int CdMix(u_char *vol);
    void SsSetMono(void);
    void SsSetStereo(void);
    s8 mix[4];

    if (stereo & 0xFF) {
        SsSetStereo();
        mix[0] = 0x7F;
        mix[1] = 0;
        mix[2] = 0x7F;
        mix[3] = 0;
    } else {
        SsSetMono();
        mix[0] = 0x3F;
        mix[1] = 0x3F;
        mix[2] = 0x3F;
        mix[3] = 0x3F;
    }
    CdMix(mix);
}

void sound_load_bank0(u8 bank) {
    extern u16 g_CdFileTable_u16[] __asm__("g_CdFileTable");
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    s32 SsVabClose_s8(s8) __asm__("SsVabClose");
    void load_file_l() __asm__("load_file");
    void task_sleep_l() __asm__("task_sleep");
    s8 *vab;
    u8 *base;
    u16 *p;
    u16 *q;
    u16 *tbl;
    s32 idx;

    vab = LITP(s8, g_SndVabId0);
    if (*vab >= 0) {
        SsVabClose_s8(*vab);
        *vab = -1;
    }
    base = LITP(u8, g_SndBank0HedBuf);
    idx = bank;
    do {
        tbl = g_SndBank0HedFiles;
    } while (0);
    p = tbl + idx;
loop1:
    load_file_l(*p, base);
    g_SndVabHead0 = base + ((s32 *)(g_CdFileTable_u16[*p * 4] + (s32)base))[-2];
    if ((g_SndVabId0 = SsVabOpenHeadSticky(g_SndVabHead0, 0, 0x1020)) == -1) {
        goto loop1;
    }
    tbl = g_SndBank0VbFiles;
    q = tbl + bank;
loop2:
    load_file_l(*q, ADDR_g_SndBank0VbBuf);
    if (SsVabTransBody(LITP(u8, g_SndBank0VbBuf), g_SndVabId0) == -1) {
        goto loop2;
    }
loop3:
    task_sleep_l(1);
    if (SsVabTransCompleted(0) == 0) {
        goto loop3;
    }
}

void sound_load_bank1(s32 bank, u8 *vb_buf) {
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    void SsVabClose(short vab_id);
    extern u16 g_CdFileTable_h6[] __asm__("g_CdFileTable");
    void load_file_h6(u16, u8 *) __asm__("load_file");
    void task_sleep_h6(s32) __asm__("task_sleep");
    u8 *buf;
    u8 *p;

    if (LIT(s8, g_SndVabId1) >= 0) {
        SsVabClose(LIT(s8, g_SndVabId1));
        LIT(s8, g_SndVabId1) = -1;
    }
    do {
        buf = LITP(u8, g_SndBank1HedBuf);
    } while (0);
    do {
        load_file_h6(g_SndBank1HedFiles[bank & 0xFF], buf);
        p = buf + *(s32 *)(buf + *(u16 *)((u8 *)g_CdFileTable_h6 + (g_SndBank1HedFiles[bank & 0xFF] << 3)) - 8);
        g_SndVabHead1 = p;
    } while ((g_SndVabId1 = SsVabOpenHeadSticky(p, 1, 0x7300)) == -1);
    do {
        load_file_h6(g_SndBank1VbFiles[bank & 0xFF], vb_buf);
    } while (SsVabTransBody(vb_buf, g_SndVabId1) == -1);
    do {
        task_sleep_h6(1);
    } while (SsVabTransCompleted(0) == 0);
}

void sound_load_room_bank(void) {
    s32 task_sleep_a9(s32) __asm__("task_sleep");
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    void SsVabClose(short vab_id);
    s8 vab;
    s8 new_id;

    g_SndVabHeadRoom = (s32 *)((u8 *) ROOM->vab_header);
    vab = LIT(s8, g_SndVabIdRoom);
    if (vab >= 0) {
        SsVabClose((s16) vab);
        LIT(s8, g_SndVabIdRoom) = -1;
    }
    if (*g_SndVabHeadRoom != 0) {
        do {
            new_id = SsVabOpenHeadSticky((u_char *)g_SndVabHeadRoom, 2, 0xE800U);
            g_SndVabIdRoom = new_id;
        } while (new_id == -1);
        do {

        } while (SsVabTransBody(((u8 *) ROOM->vab_body), (s16) g_SndVabIdRoom) == -1);
        do {
            task_sleep_a9(1);
        } while ((SsVabTransCompleted(0) << 0x10) == 0);
    }
}

void sound_load_bank3(s32 bank) {
    extern u16 g_CdFileTable_u16[] __asm__("g_CdFileTable");
    s32 load_file_p(u16, u8 *) __asm__("load_file");
    s32 task_sleep_p(s32) __asm__("task_sleep");
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    void SsVabClose(short vab_id);
    u8 *buf;
    u16 *ent;
    u8 *p;
    u8 **pp;
    u16 *tbl;
    s32 idx;

    if (LIT(s8, g_SndVabId3) >= 0) {
        SsVabClose(LIT(s8, g_SndVabId3));
        LIT(s8, g_SndVabId3) = -1;
    }
    buf = LITP(u8, g_SndBank3HedBuf);
    idx = bank & 0xFF;
    do {
        tbl = g_SndBank3HedFiles;
    } while (0);
    ent = tbl + idx;
loop1:
    load_file_p(*ent, buf);
    p = buf + *(s32 *)(buf + *(u16 *)((u8 *)g_CdFileTable_u16 + (*ent << 3)) - 8);
    g_SndVabHead3 = p;
    if ((g_SndVabId3 = SsVabOpenHeadSticky(p, 3, 0x37800)) == -1) {
        goto loop1;
    }
    tbl = g_SndBank3VbFiles;
    ent = tbl + (bank & 0xFF);
    pp = LITP(u8 *, g_LoadPointer);
loop2:
    load_file_p(*ent, *pp);
    if (SsVabTransBody(*pp, g_SndVabId3) == -1) {
        goto loop2;
    }
    do {
        task_sleep_p(1);
    } while (SsVabTransCompleted(0) == 0);
}

void bgm_load_bank(s32 bgm_cfg) {
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    void SsVabClose(short vab_id);
    void load_file_l() __asm__("load_file");
    void task_sleep_l(s32) __asm__("task_sleep");
    void bgm_sep_open_l(u8, s32) __asm__("bgm_sep_open");
    extern u16 g_BgmBankFiles_l[] __asm__("g_BgmBankFiles");
    extern s32 g_CdFileTable_l[] __asm__("g_CdFileTable");
    u8 *buf;
    u8 *p;
    s32 n;
    s32 i;
    s8 id;

loop:
    if (LIT(s8, g_SndVabIdBgm) >= 0) {
        SsVabClose(LIT(s8, g_SndVabIdBgm));
        LIT(s8, g_SndVabIdBgm) = -1;
    }
    g_SndVabHeadBgm = (u8 *)(g_BgmSepData = g_BgmVabHeadDst = ADDR_g_BgmHeadBuf);
    buf = LIT(u8 *, g_LoadPointer);
    i = bgm_cfg & 7;
    g_BgmVabBody = buf;
    g_BgmFileTail = buf;
    load_file_l(g_BgmBankFiles_l[(LIT(u8 *, g_StageRoomBgmTable) + LIT(u8, g_RoomId) * 4)[i]], buf);
    n = g_CdFileTable_l[g_BgmBankFiles_l[(LIT(u8 *, g_StageRoomBgmTable) + LIT(u8, g_RoomId) * 4)[i]] * 2];
    g_BgmFileSize = n;
    p = g_BgmFileTail + g_BgmFileSize;
    g_BgmFileTail = p - 8;
    g_BgmSepData += *(s32 *)(p - 8);
    g_BgmFileTail = p - 0xC;
    g_BgmVabBody += *(s32 *)(p - 0xC);
    mem_copy(g_BgmVabHeadDst, LIT(u8 *, g_LoadPointer), *(s32 *)(p - 0xC));
    id = SsVabOpenHeadSticky(LITP(u8, g_BgmHeadBuf), 4, 0x408B0);
    LIT(s8, g_SndVabIdBgm) = id;
    if (id == -1) {
        goto loop;
    }
    if (SsVabTransBody(g_BgmVabBody, id) == -1) {
        goto loop;
    }
    do {
        task_sleep_l(1);
    } while (SsVabTransCompleted(0) == 0);
    bgm_sep_open_l(bgm_cfg, g_BgmSepData);
}

void bgm_sep_open(s32 bgm_cfg, u32 *sep_data) {
    short SsSepOpen(u_long *addr, short vab_id, short seq_cnt);
    void SsSepClose(short sep_access_num);
    void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    s32 i;

    i = 3;
    if (LIT(s8, g_SepAccessNum) >= 0) {
        SsSepClose(LIT(s8, g_SepAccessNum));
        LIT(s8, g_SepAccessNum) = -1;
    }
    while ((g_SepAccessNum = SsSepOpen((u_long *)sep_data, g_SndVabIdBgm, i)) == -1) {
    }
    do {
        i--;
        SsSepSetVol(LIT(s8, g_SepAccessNum), (s16)i, g_SndPanVol[i][0], g_SndPanVol[i][1]);
        SsSepPlay(LIT(s8, g_SepAccessNum), (s16)i, 0, 1);
    } while (i != 0);
}

void bgm_sep_replay(void) {
    s32 SsSepReplay(s8, s32);
    if (LIT(u8, g_TargetBgmState) != 0xFF) {
        if (LIT(u8, g_TargetBgmState) & 8) {
            SsSepReplay(g_SepAccessNum, 0);
            g_BgmState |= 8;
        }
        if (LIT(u8, g_TargetBgmState) & 0x10) {
            SsSepReplay(g_SepAccessNum, 1);
            g_BgmState |= 0x10;
        }
        if (LIT(u8, g_TargetBgmState) & 0x20) {
            SsSepReplay(g_SepAccessNum, 2);
            g_BgmState |= 0x20;
        }
    }
}

void update_room_bgm(void) {
    extern u8 g_PrevRoomId_u8 __asm__("g_PrevRoomId");
    void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
    void SsSepStop(short sep_access_num, short seq_num);
    short SsIsEos(short access_num, short seq_num);
    void bgm_load_bank_l(u8) __asm__("bgm_load_bank");
    void bgm_fade_out_all(void);
    s32 unused[2];
    u8 cur;
    u8 prev;
    u8 *p630;

    p630 = LITP(u8, g_TargetBgmState);
    cur = g_RoomBgmConfig[LIT(u8, g_RoomId)];
    prev = g_BgmState;
    LIT(u8, g_PrevBgmState) = prev;
    *p630 = cur;
    if (LIT(u8, g_TargetBgmState) == 0xFF) {
        if (LIT(u8, g_PrevBgmState) != 0xFF) {
            bgm_fade_out_all();
            g_BgmState = 0xFF;
        }
        LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
        return;
    }
    if (*(g_StageRoomBgmTable + LIT(u8, g_RoomId) * 4 + (cur & 7)) != *(g_StageRoomBgmTable + g_PrevRoomId_u8 * 4 + (prev & 7))
        && LIT(u8, g_PrevBgmState) != 0xFF && (prev & 0x38)) {
        bgm_fade_out_all();
        LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
    }
    switch (LIT(u8, g_TargetBgmState) >> 6) {
    case 0: {
        u8 *p61;
        u8 *p63;
        u8 a;
        u16 b;
        s32 t;

        if (LIT(u8, g_PrevBgmState) == 0xFF) {
            LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
            bgm_load_bank_l(LIT(u8, g_TargetBgmState));
            bgm_sep_replay();
            break;
        }
        a = LIT(u8, g_TargetBgmState);
        b = LIT(u8, g_PrevBgmState);
        p61 = LITP(u8, g_RoomId);
        p63 = LITP(u8, g_PrevRoomId);
        if (*(g_StageRoomBgmTable + *p61 * 4 + (a & 7)) != *(g_StageRoomBgmTable + *p63 * 4 + (b & 7))) {
            bgm_fade_out_all();
            LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
            bgm_load_bank_l(LIT(u8, g_TargetBgmState));
            bgm_sep_replay();
            break;
        }
        LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
        if (*(g_StageRoomBgmTable + *p61 * 4 + (a & 7)) != *(g_StageRoomBgmTable + *p63 * 4 + (b & 7))) {
            break;
        }
        t = a & 8;
        if (t != (b & 8)) {
            if (t) {
                if (!SsIsEos(LIT(s8, g_SepAccessNum), 0)) {
                    SsSepPlay(LIT(s8, g_SepAccessNum), 0, 1, 1);
                }
            } else if (SsIsEos(LIT(s8, g_SepAccessNum), 0)) {
                SsSepStop(LIT(s8, g_SepAccessNum), 0);
            }
        }
        if ((g_TargetBgmState & 0x10) != (g_PrevBgmState & 0x10)) {
            if (g_TargetBgmState & 0x10) {
                if (!SsIsEos(LIT(s8, g_SepAccessNum), 1)) {
                    SsSepPlay(LIT(s8, g_SepAccessNum), 1, 1, 1);
                }
            } else if (SsIsEos(LIT(s8, g_SepAccessNum), 1)) {
                SsSepStop(LIT(s8, g_SepAccessNum), 1);
            }
        }
        if ((g_TargetBgmState & 0x20) != (g_PrevBgmState & 0x20)) {
            if (g_TargetBgmState & 0x20) {
                if (!SsIsEos(LIT(s8, g_SepAccessNum), 2)) {
                    SsSepPlay(LIT(s8, g_SepAccessNum), 2, 1, 1);
                }
            } else if (SsIsEos(LIT(s8, g_SepAccessNum), 2)) {
                SsSepStop(LIT(s8, g_SepAccessNum), 2);
            }
        }
        break;
    }
    case 1:
        bgm_fade_out_all();
        LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
        bgm_load_bank_l(LIT(u8, g_TargetBgmState));
        break;
    case 2:
        LIT(u8, g_TargetBgmState) &= 0x7F;
        bgm_fade_out_all();
        LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
        bgm_load_bank_l(LIT(u8, g_TargetBgmState));
        if ((LIT(u8, g_TargetBgmState) >> 6) == 0) {
            bgm_sep_replay();
        }
        break;
    }
    LIT(s32, g_StatusFlags) &= ~STATUS_DOOR_WAIT_BGM;
    g_BgmState = g_TargetBgmState;
}

void bgm_track_play(s16 seq) {
    void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
    SsSepPlay(g_SepAccessNum, seq, 1, 1);
}

void bgm_track_stop(s16 seq) {
    void SsSepStop(short sep_access_num, short seq_num);
    if (g_TargetBgmState != 0xFF) {
        SsSepStop(g_SepAccessNum, seq);
    }
}

void bgm_track_set_volume(s16 seq, s16 vol_l, s16 vol_r) {
    void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
    SsSepSetVol(g_SepAccessNum, seq, vol_l, vol_r);
}

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
void sound_play_se_3d(u8 type, u8 idx, s32 unused, s32 pos) {
    s32 sound_calc_pan_volume_l() __asm__("sound_calc_pan_volume");
    s8 *ent;
    u8 *data;
    s16 vol;
    s32 ok;

    switch (type) {
    case 0:
        if (idx >= 2) {
            return;
        }
        ent = LITP(s8, g_SndBank0HedBuf);
        vol = (s8)g_SndVabId0;
        data = g_SndVabHead0;
        break;
    case 1:
        if (g_StatusFlags & STATUS_SFX_BANK1_SMALL) {
            ok = idx < 0x10;
        } else {
            ok = idx < 0x30;
        }
        if (!ok) {
            return;
        }
        ent = LITP(s8, g_SndBank1HedBuf);
        vol = (s8)g_SndVabId1;
        data = g_SndVabHead1;
        break;
    case 2:
        if (idx >= 0x30) {
            return;
        }
        vol = (s8)g_SndVabIdRoom;
        data = (u8 *)g_SndVabHeadRoom;
        ent = ((s8 *) ROOM->sound_attributes);
        break;
    case 3:
        if (idx >= 0x10) {
            return;
        }
        ent = LITP(s8, g_SndBank3HedBuf);
        vol = (s8)g_SndVabId3;
        data = g_SndVabHead3;
        break;
    case 4:
        if (idx >= 0x30) {
            return;
        }
        vol = (s8)g_SndVabIdBgm;
        data = g_SndVabHeadBgm;
        ent = ((s8 *) ROOM->sound_attributes);
        break;
    default:
        return;
    }
    ent += idx * 4;
    data += (ent[1] << 9) + ({ (ent[2] << 5) + 0x820; });
    sound_calc_pan_volume_l(pos);
    g_SndKeyOnMask |= 1 << SsUtKeyOn(vol, ent[1], ent[2], data[6], data[5], g_SndPanVolL, g_SndPanVolR);
}
__asm__(".section .rodata\n\t.word 0\n\t.text");

/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
void play_sfx(u8 type, u8 idx) {
    s16 vab;
    u8 *data;
    s8 *tbl;
    s8 prog;
    s8 tone;

    switch (type) {
    case 0:
        if (idx >= 2) {
            return;
        }
        vab = (s8)g_SndVabId0;
        data = g_SndVabHead0;
        tbl = LITP(s8, g_SndBank0HedBuf);
        break;
    case 1:
        if (!((g_StatusFlags & STATUS_SFX_BANK1_SMALL) ? idx < 0x10 : idx < 0x30)) {
            return;
        }
        vab = (s8)g_SndVabId1;
        data = g_SndVabHead1;
        tbl = LITP(s8, g_SndBank1HedBuf);
        break;
    case 2:
        if (idx >= 0x30) {
            return;
        }
        vab = (s8)g_SndVabIdRoom;
        data = (u8 *)g_SndVabHeadRoom;
        tbl = ((s8 *) ROOM_HDR(g_RoomData)->sound_attributes);
        break;
    case 3:
        if (idx >= 0x10) {
            return;
        }
        tbl = LITP(s8, g_SndBank3HedBuf);
        vab = (s8)g_SndVabId3;
        data = g_SndVabHead3;
        break;
    case 4:
        if (idx >= 0x30) {
            return;
        }
        vab = (s8)g_SndVabIdBgm;
        data = g_SndVabHeadBgm;
        tbl = ((s8 *) ROOM_HDR(g_RoomData)->sound_attributes);
        break;
    default:
        return;
    }
    tbl += idx * 4;
    prog = tbl[1];
    tone = tbl[2];
    data += (prog << 9) + ({ (tone << 5) + 0x820; });
    g_SndKeyOnMask |= 1 << SsUtKeyOn(vab, prog, tone, data[6], data[5], data[2], data[2]);
}

void sound_play_footstep(u8 se) {
    s32 lookup_footstep_zone_l() __asm__("lookup_footstep_zone");
    s32 sound_calc_pan_volume_l() __asm__("sound_calc_pan_volume");
    s32 t;
    s32 v;
    s32 x;
    s8 *ent;
    u8 *data;

    if (se < 3) {
        ent = ((s8 *) ROOM->sound_attributes);
        if ((g_GameStateFlags & GAME_ON_BOX) && g_PlayerEntity.floor_y == -0x708) {
            se += 0x23;
        } else {
            t = lookup_footstep_zone_l(g_CurrentEntity->pos_x, g_CurrentEntity->pos_z);
            x = se;
            if (g_StatusFlags & STATUS_IN_WATER) {
                v = ({ x - 3; }) + t;
            } else {
                v = x + t;
            }
            se = v;
        }
        if (se < 0x30) {
            ent += se * 4;
            data = (u8 *)g_SndVabHeadRoom;
            data += (ent[1] << 9) + ({ (ent[2] << 5) + 0x820; });
            sound_calc_pan_volume_l(&g_CurrentEntity->pos_x);
            g_SndKeyOnMask |= 1 << SsUtKeyOn(g_SndVabIdRoom, ent[1], ent[2], data[6], data[5], g_SndPanVolL, g_SndPanVolR);
        }
    }
}

void sound_play_entity_se(u8 se) {
    s32 sound_calc_pan_volume_l() __asm__("sound_calc_pan_volume");
    u8 *obj;
    s8 *ent;
    u8 *data;
    u8 x;

    if (se < 10) {
        data = (u8 *)g_SndVabHeadRoom;
        obj = (u8 *)g_CurrentEntity;
        ent = ((s8 *) ROOM->sound_attributes);
        /* one u8 variable for the bit-field steps (keeps it in v0 like the original) */
        x = ENT(obj)->slot_info;
        x = (x & 0x70) >> 4;
        se += x * 10;
        if (se < 0x30) {
            ent += se * 4;
            data += (ent[1] << 9) + ({ (ent[2] << 5) + 0x820; });
            sound_calc_pan_volume_l(&ENT(obj)->pos_x, obj);
            g_SndKeyOnMask |= 1 << SsUtKeyOn(g_SndVabIdRoom, ent[1], ent[2], data[6], data[5],
                                                 g_SndPanVolL, g_SndPanVolR);
        }
    }
}

void sound_poll_voice_status(void) {
    void SpuGetAllKeysStatus(char *status);
    s32 voice;

    SpuGetAllKeysStatus(&g_SpuKeyStatus);
    voice = 0;
    do {
        if ((&g_SpuKeyStatus)[voice] == 0) {
            if ((1 << voice) == 0) {
                g_SndKeyOnMask &= 1;
                voice += 1;
            } else {
                g_SndKeyOnMask = 0;
                goto block_5;
            }
        } else {
block_5:
            voice += 1;
        }
    } while (voice >= 0x18);
}

void sound_keys_off(void) {
    s32 SpuSetKey(s32, s32);
    SpuSetKey(0, g_SndKeyOnMask);
    g_SndKeyOnMask = 0;
}

void sound_calc_pan_volume(VECTOR *pos) {
    extern s16 g_SndPanVolL_l[2] __asm__("g_SndPanVolL");
    s32 d;
    s32 dist;
    s32 ang;
    u32 a;
    typedef struct {
        GsRVIEW2 view;
        u8 pad[0xC];
    } CamEnt;
    typedef struct {
        u8 pad[0x9C];
        CamEnt cam[1];
    } CamSet;
    s16 x;
    u16 y;
    s16 t0;
    s16 t1;
    s32 q;
    s32 k;
    s16 r;
    s16 l;
    s32 t;
    s32 e;

    LIT(s16, sp_svec_vx) = LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpx - pos->vx;
    SCRATCH->sv_z = LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpz - pos->vz;
    d = SquareRoot0(LIT(s16, sp_svec_vx) * LIT(s16, sp_svec_vx) +
                    SCRATCH->sv_z * SCRATCH->sv_z);
    ang = LIT(s16, sp_svec_vx) * LIT(s16, sp_svec_vx);
    e = SquareRoot0(ang + SCRATCH->sv_z * SCRATCH->sv_z);
    t = g_View_Vpy - SCRATCH->sv_y;
    dist = SquareRoot0(t * t + d * e);
    a = calc_angle_xz(LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpx,
                          LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpz,
                          pos->vx, pos->vz);
    t = a - calc_angle_xz(LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpx,
                                 LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vpz,
                                 LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vrx,
                                 LIT(CamSet *, g_RoomData)->cam[LIT(u8, g_CutId)].view.vrz);
    ang = t & 0xFFF;
    x = ang;
    y = ang;
    if (x != 0 && x != 0x1000 && x != 0x800) {
        t1 = 0;
        if (y > 0x800) {
            y = ~ang;
            y &= 0x7FF;
            t1 = 1;
            t0 = 0;
        } else {
            t0 = 1;
        }
        if (y > 0x400) {
            y = ~y;
        }
        a = y & 0x7FF;
        if (a > 0x40) {
            q = (s32)a / (dist / 2000 + 24);
            r = q + 0x7F;
            l = 0x7F - q;
            g_SndPanVolL_l[1] = t1 * ((l < 30) * 30 + (l >= 30) * l) + t0 * ((r > 0x7F) * 0x7F + (r < 0x80) * r);
            g_SndPanVolL_l[0] = t1 * ((r >= 0x80) * 0x7F + (r < 0x80) * r) + t0 * ((l < 30) * 30 + (l >= 30) * l);
            goto done;
        }
    }
    g_SndPanVolL_l[1] = 0x7F;
    g_SndPanVolL_l[0] = 0x7F;
done:
    g_SndPanVolL_l[0] -= (k = dist / 500);
    g_SndPanVolL_l[1] -= k;
    if (g_SndPanVolL_l[0] < 30) {
        g_SndPanVolL_l[0] = 30;
    }
    if (g_SndPanVolL_l[1] < 30) {
        g_SndPanVolL_l[1] = 30;
    }
    g_SndPanVolL_l[0] &= 0x7F;
    g_SndPanVolL_l[1] &= 0x7F;
}

void sound_fade_start(s8 step, s8 mode) {
    s32 SsGetMVol(s32 *);
    SsGetMVol(&g_SndMasterVol);
    g_SndFadeStep = step;
    g_SndFadeActive = mode;
}

void sound_fade_update(void) {
    void SsUtAllKeyOff(s32);
    extern s16 g_SndMasterVol_s16[] __asm__("g_SndMasterVol");
    s32 SsSetMVol(s32, s32);
    void SsVabClose(short vab_id);
    void SsSepClose(short sep_access_num);
    void SsSepStop(short sep_access_num, short seq_num);
    short SsIsEos(short access_num, short seq_num);
    s8 v;
    s8 w;

    v = LIT(s8, g_SndFadeActive);
    if (v < 0) {
        w = v + 1;
        LIT(s8, g_SndFadeActive) = w;
        if (w == -1) {
            LIT(s8, g_SndFadeActive) = 0;
            SsSetMVol(0x7F, 0x7F);
        } else if (w == -2) {
            if (LIT(s8, g_SepAccessNum) >= 0) {
                SsSepClose(LIT(s8, g_SepAccessNum));
                LIT(s8, g_SepAccessNum) = -1;
            }
            if (LIT(s8, g_SndVabIdBgm) >= 0) {
                SsVabClose(LIT(s8, g_SndVabIdBgm));
                LIT(s8, g_SndVabIdBgm) = -1;
            }
        } else if (w == -4) {
            SsUtAllKeyOff(0);
        } else if (w == -29) {
            if (LIT(s8, g_SepAccessNum) >= 0) {
                if (SsIsEos(LIT(s8, g_SepAccessNum), 0)) {
                    SsSepStop(LIT(s8, g_SepAccessNum), 0);
                }
                if (SsIsEos(LIT(s8, g_SepAccessNum), 1)) {
                    SsSepStop(LIT(s8, g_SepAccessNum), 1);
                }
                if (SsIsEos(LIT(s8, g_SepAccessNum), 2)) {
                    SsSepStop(LIT(s8, g_SepAccessNum), 2);
                }
            }
        }
    } else {
        g_SndMasterVol_s16[0] += (s8)g_SndFadeStep;
        g_SndMasterVol_s16[1] += (s8)g_SndFadeStep;
        if (((s8)g_SndMasterVol_s16[0] < 1) & ((s8)g_SndMasterVol_s16[1] < 1)) {
            g_SndMasterVol_s16[1] = 0;
            g_SndMasterVol_s16[0] = 0;
            LIT(s8, g_SndFadeActive) = -30;
            g_SndFadeEndDelay = 30;
        }
        SsSetMVol(g_SndMasterVol_s16[0], g_SndMasterVol_s16[1]);
    }
}

void bgm_fade_out_all(void) {
    void SsUtAllKeyOff(s32);
    void SsVabClose(short vab_id);
    void SsSepClose(short sep_access_num);
    void SsSepStop(short sep_access_num, short seq_num);
    void SsSepSetCrescendo(short sep_access_num, short seq_num, short v, long t);
    short SsIsEos(short access_num, short seq_num);
    s32 i;

    if (LIT(s8, g_SepAccessNum) != -1) {
        i = 2;
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 0) << 16) != 0) {
            SsSepSetCrescendo(LIT(s8, g_SepAccessNum), 0, -0x5F, 2);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 1) << 16) != 0) {
            SsSepSetCrescendo(LIT(s8, g_SepAccessNum), 1, -0x5F, 2);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 2) << 16) != 0) {
            SsSepSetCrescendo(LIT(s8, g_SepAccessNum), 2, -0x5F, 2);
        }
        do {
            i--;
            task_sleep(1);
        } while (i != 0);
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 0) << 16) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 0);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 1) << 16) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 1);
        }
        if ((SsIsEos(LIT(s8, g_SepAccessNum), 2) << 16) != 0) {
            SsSepStop(LIT(s8, g_SepAccessNum), 2);
        }
        i = 8;
        do {
            i--;
            task_sleep(1);
        } while (i != 0);
        SsUtAllKeyOff(0);
        i = 2;
        do {
            i--;
            task_sleep(1);
        } while (i != 0);
        if (LIT(s8, g_SepAccessNum) >= 0) {
            SsSepClose(LIT(s8, g_SepAccessNum));
            LIT(s8, g_SepAccessNum) = -1;
        }
        if (LIT(s8, g_SndVabIdBgm) >= 0) {
            SsVabClose(LIT(s8, g_SndVabIdBgm));
            LIT(s8, g_SndVabIdBgm) = -1;
        }
    }
}

s32 sound_preload_bank1(void) {
    extern u16 g_CdFileTable_u16a[][4] __asm__("g_CdFileTable");
    void load_file_l() __asm__("load_file");
    s32 *base = (s32 *)g_SndBank1PreloadPtr;
    u16 *id1 = &g_SndBank1HedFiles[10];
    u16 *id2;
    s32 size;
    s32 off;
    u32 o;
    u8 *q;

    load_file_l(*id1, base + 1);
    size = g_CdFileTable_u16a[*id1][0];
    id2 = &g_SndBank1VbFiles[10];
    off = size + 4;
    o = off & 0xFFFF;
    do {
        *base = size;
    } while (0);
    load_file_l(*id2, (u8 *)base + o);
    return (g_CdFileTable_u16a[*id2][0] + off) & 0xFFFF;
}

void sound_load_bank1_from_memory(void) {
    s32 mem_copy(s32, u8 *, s32);
    short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
    short SsVabTransBody(u_char *addr, short vabid);
    short SsVabTransCompleted(short immediateFlag);
    void SsVabClose(short vab_id);
    s32 hed_size;
    u8 *buf;
    s8 vab;
    s8 new_id;
    u16 hed_size16;
    u8 *vab_head;
    u8 *hed_src;
    u8 *vb_body;

    vab = LIT(s8, g_SndVabId1);
    if (vab >= 0) {
        SsVabClose((s16) vab);
        LIT(s8, g_SndVabId1) = -1;
    }
    hed_size16 = *(s16 *)g_SndBank1PreloadPtr;
    hed_src = g_SndBank1PreloadPtr + 4;
    g_SndBank1PreloadPtr = hed_src;
    hed_size = hed_size16 & 0xFFFF;
    buf = LITP(u8, g_SndBank1HedBuf);
    mem_copy((s32)buf, hed_src, hed_size);
    vab_head = (u8 *)(*(s32 *)(LITP(u8, g_SndBank1HedBuf_Minus8) + (hed_size)) + (s32)buf);
    g_SndVabHead1 = vab_head;
    new_id = SsVabOpenHeadSticky(vab_head, 1, 0x7300U);
    LIT(s8, g_SndVabId1) = new_id;
    vb_body = &g_SndBank1PreloadPtr[hed_size];
    g_SndBank1PreloadPtr = vb_body;
    SsVabTransBody(vb_body, (s16) new_id);
    SsVabTransCompleted(1);
}
