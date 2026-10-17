/* cd.c - CD access: file reads by sector with checksum checks, XA voice lines (voice_command) and the XA
   stream / mixer state machine. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"


void cd_init(void) {
    while (CdInit() == 0) {
    }
    g_SkipCdChecksum = 0;
    xa_set_volume(0x80);
    g_XaState = 0;
}

void voice_command(s32 cmd, s32 clip) {
    void voice_play_clip(u32 clip);
    s32 xa_apply_mix_p(u8) __asm__("xa_apply_mix");
    s32 cd_control_retry_a1(s32, s32, s32) __asm__("cd_control_retry");
    extern s32 g_XaVolume_s32 __asm__("g_XaVolume");
    s32 unused[4];
    s32 op;
    s32 vol;

    op = cmd & 0xFFFF;
    switch (op) {
    case 0:
        break;
    case 1:
        voice_play_clip(clip & 0xFFFF);
        return;
    case 2:
        vol = g_XaVolume_s32;
        if (vol != 0) {
            do {
                xa_apply_mix_p(vol & 0xFF);
                vol -= 1;
            } while (vol != 0);
        }
        xa_apply_mix_p(0);
        cd_control_retry_a1(9, 0, 0);
        g_XaState = 0;
        LIT(s32, g_GameStateFlags) &= ~GAME_VOICE_PLAYING;
        return;
    }
}

void cd_unused_stub(void) {
}

s32 cd_read_file(s32 file, u32 *dst) {
    s32 cd_control_retry(u8 com, u8 *param, u8 *res);
    s32 cd_get_file_lba(u32 file, s32 *size_out);
    extern u8 g_CdStatusResult_u8 __asm__("g_CdStatusResult");
    int CdReset(int mode);
    int CdControlB(u_char com, u_char *param, u_char *result);
    int CdRead(int sectors, u_long *buf, int mode);
    CdlLOC *CdIntToPos(int i, CdlLOC *p);
    s32 unused[2];
    u8 mode;

    mode = 0x80;
    g_XaState = 0;
    g_CdReadBuffer = (u8 *)dst;
    g_CdReadFileIndex = file;
    g_CdReadBusy = 1;
    g_CdCurrentSector = cd_get_file_lba(file, &g_CdReadSize);
    CdIntToPos(g_CdCurrentSector, &g_CdReadLoc);
loop:
    cd_control_retry(1, 0, 0);
    if (!(g_CdStatusResult_u8 & 2)) {
        CdReset(0);
        return 1;
    }
    if (CdControlB(0xE, &mode, NULL) == 0) {
        goto loop;
    }
    if (CdControlB(2, (u8 *)&g_CdReadLoc, NULL) == 0) {
        goto loop;
    }
    if (CdRead((u32)(g_CdReadSize + 0x7FF) >> 0xB, (u_long *)dst, mode) == 0) {
        goto loop;
    }
    return 0;
}

s32 cd_get_file_lba(u32 file, s32 *size_out) {
  s32 unused;
  if (size_out != 0)
  {
    *size_out = g_CdFileTable[file].size;
  }
  ;
  if (file >= 0x3D8U)
  {
    return 0;
  }
  return (g_CdFileTable[file].lba_hi << 0x10) | g_CdFileTable[file].lba;
}

s32 cd_verify_file_checksum(void) {
    int printf();
    int printf();
    s32 unused[2];
    u8 sum;
    s32 t;
    u32 i;
    u32 n;
    u8 *p;
    u8 want;

    if (g_SkipCdChecksum != 0) {
        return 1;
    }
    sum = 0;
    p = g_CdReadBuffer;
    i = 0;
    n = g_CdReadSize;
    if (n != 0) {
        do {
            u8 c = *p;
            do {
                i += 0x200;
            } while (0);
            sum ^= c;
            p += 0x200;
        } while (i < n);
    }
    want = g_CdFileTable[g_CdReadFileIndex].sum;
    t = sum;
    if (t != want) {
        printf(s_SumError, t, want);
        return 0;
    }
    return 1;
}

s32 room_bg_verify_checksum(void) {
    extern u8 g_RoomId_u8 __asm__("g_RoomId");
    int printf();
    s32 t;
    u8 sum;
    s32 unused;
    u32 i;
    u8 *p;
    u8 want;

    sum = 0;
    p = g_CdReadBuffer;
    i = 0;
    do {
        sum ^= *p;
        p += 0x800;
        i += 0x800;
    } while (i <= 0x7FFFU);
    want = *(s32 *)((g_RoomId_u8 * 8) + g_StageBgChecksumTable + g_CutId);
    t = sum;
    if (t != want) {
        printf(s_SumError, t, want);
        return 0;
    }
    return 1;
}

void voice_play_clip(u32 clip) {
    s32 cd_get_file_lba_p(u16, u32 *) __asm__("cd_get_file_lba");
    void xa_stream_start(void);
    CdlLOC *CdIntToPos(int i, CdlLOC *p);
    u32 n;
    s32 start;
    s32 base_lba;
    s32 chan;
    BYTES(2) *ent;   /* steps through the u16 voice offset table; a u16 * compiles differently here */
    u32 n_next;

    ent = (void *)g_StageVoiceOffsetTable;
    chan = 0;
    n = 0;
    if (clip != 0) {
        do {
            if (*(u16 *)ent & 0x8000) {
                chan += 1;
                ent++;
            }
            n_next = n + 1;
            n = n_next;
            ent++;
        } while (n_next < clip);
    }
    g_XaFilter = 1;
    g_XaFilterChan = chan;
    base_lba = cd_get_file_lba_p(*(u16 *)((u8 *)&g_StageVoiceFileIndex + ((g_StageId * 2))), &n);
    g_XaStartSector = base_lba;
    g_XaEndSector = base_lba;
    /* ent[0] / ent[1]: the clip's start / end entries of the u16 voice offset table (bit 15 = next channel) */
    start = ((*(u16 *)ent[0].b & 0x7FFF) * 0x10) + chan + base_lba;
    g_XaStartSector = start;
    g_XaEndSector = ((*(u16 *)ent[1].b & 0x7FFF) * 0x10) + chan + base_lba;
    CdIntToPos(start, (CdlLOC *)&g_XaStartLoc);
    {
        s32 unused[2];
    }
    g_XaStartRetries = 0;
    xa_stream_start();
}

void xa_stream_start(void) {
    s32 cd_control_retry_a2(s32, s32, s32) __asm__("cd_control_retry");
    int VSync(int mode);
    int CdControl(u_char com, u_char *param, u_char *result);
    int CdControlB(u_char com, u_char *param, u_char *result);
    void SsSetSerialAttr(char s_num, char attr, char mode);
    u8 cd_mode;
    s32 can_retry;

    SsSetSerialAttr(0, 0, 1);
    cd_mode = 0xC8;
    cd_control_retry_a2(1, 0, 0);
loop_1:
    can_retry = (u32) g_XaStartRetries < 5U;
    g_XaStartRetries += 1;
    if (can_retry == 0) {
        g_XaState = 0;
        return;
    }
    if ((CdControlB(0xEU, &cd_mode, NULL) == 0) || (CdControlB(0xDU, &g_XaFilter, NULL) == 0) || (CdControlB(0x15U, &g_XaStartLoc, NULL) == 0) || (CdControl(0x1BU, &g_XaStartLoc, NULL) == 0)) {
        VSync(2);
        goto loop_1;
    }
    g_XaFrameCounter = 0;
    g_XaState = 1;
}

void xa_set_volume(s32 vol) {
    extern s32 g_XaVolume_s32 __asm__("g_XaVolume");
    g_XaVolume_s32 = vol & 0xFF;
}

void xa_apply_mix(u8 level) {
    int CdMix(u_char *vol);
    u32 half;

    if (g_SoundStereo == 1) {
        g_CdMixVolume = level;
        g_CdMixVolumeLR = 0;
        g_CdMixVolumeRR = level;
        g_CdMixVolumeRL = 0;
    } else {
        half = (u32) (level & 0xFF) >> 1;
        g_CdMixVolume = (u8) half;
        g_CdMixVolumeLR = (s8) half;
        g_CdMixVolumeRR = (u8) half;
        g_CdMixVolumeRL = (s8) half;
    }
    CdMix(&g_CdMixVolume);
}

void sound_set_mono(void) {
    g_SoundStereo = 0;
    xa_apply_mix(g_XaVolume);
}

void sound_enable_stereo_mix(void) {
    g_SoundStereo = 1;
    xa_apply_mix(g_XaVolume);
}

void xa_update(void) {
    extern s32 (*g_XaStateFuncs[])();
    if ((u32) g_XaState >= 3U) {
        g_XaState = 0;
    }
    g_XaStateFuncs[g_XaState]();
}

void xa_state_idle(void) {
    STATE_BLOCK->flags &= ~GAME_VOICE_PLAYING;
}

void xa_state_wait_start(void) {
    s32 xa_apply_mix_p(u8) __asm__("xa_apply_mix");
    s32 cd_control_retry_a3(s32, s32, s32) __asm__("cd_control_retry");
    extern u32 g_XaFrameCounter_u32 __asm__("g_XaFrameCounter");
    extern s32 g_XaVolume_s32 __asm__("g_XaVolume");
    int CdControlB(u_char com, u_char *param, u_char *result);
    int CdSync(int mode, u_char *result);
    void SsSetSerialAttr(char s_num, char attr, char mode);
    u8 *res;
    u8 st;

    g_XaFadeVolIn = CdSync(1, NULL);
    if (g_XaFadeVolIn != 0) {
        if (g_XaFadeVolIn == 5) {
            for (g_XaFadeVolIn = g_XaVolume_s32; g_XaFadeVolIn > 0; g_XaFadeVolIn--) {
                xa_apply_mix_p(g_XaFadeVolIn);
            }
            xa_apply_mix_p(0);
            cd_control_retry_a3(9, 0, 0);
            g_XaState = 0;
            SsSetSerialAttr(0, 0, 0);
        }
        res = g_XaSyncResult;
        if (CdControlB(1, NULL, res) != 0) {
            st = *res;
            if ((st & 6) != 2 || g_XaFrameCounter_u32++ >= 0xB5) {
                for (g_XaFadeVolIn = g_XaVolume_s32; g_XaFadeVolIn > 0; g_XaFadeVolIn--) {
                    xa_apply_mix_p(g_XaFadeVolIn);
                }
                xa_apply_mix_p(0);
                cd_control_retry_a3(9, 0, 0);
                g_XaState = 0;
                SsSetSerialAttr(0, 0, 0);
            } else if (!(st & 0x40) && (st & 0x20)) {
                for (g_XaFadeVolIn = 0; (u32)g_XaFadeVolIn < (u32)g_XaVolume_s32; g_XaFadeVolIn++) {
                    xa_apply_mix_p(g_XaFadeVolIn);
                }
                xa_apply_mix_p(g_XaVolume_s32);
                g_XaFrameCounter_u32 = 0;
                g_XaState = 2;
                LIT(u32, g_GameStateFlags) |= GAME_VOICE_PLAYING;
            }
        }
    }
}

void xa_state_playing(void) {
    s32 xa_apply_mix_p(u8) __asm__("xa_apply_mix");
    s32 cd_control_retry_a4(s32, s32, s32) __asm__("cd_control_retry");
    extern u32 g_XaFrameCounter_u32 __asm__("g_XaFrameCounter");
    extern s32 g_XaVolume_s32 __asm__("g_XaVolume");
    int CdControlB(u_char com, u_char *param, u_char *result);
    int CdPosToInt(CdlLOC *p);
    void SsSetSerialAttr(char s_num, char attr, char mode);
    s32 sector;
    s32 vol;
    s32 vol2;
    s32 vol3;
    s32 vol4;
    u32 frames;

    if ((g_XaFrameCounter_u32 & 0x1FF) != 0x100) {
        if (CdControlB(0x11U, NULL, g_XaGetlocResult) != 0) {
            g_XaGetlocSpare = 0;
            sector = CdPosToInt((CdlLOC *) (g_XaGetlocResult + 5));
            g_CdCurrentSector = sector;
            if ((u32) (g_XaEndSector + 0x1194) < (u32) sector) {
                g_XaFadeVolOut = g_XaVolume_s32;
                if (g_XaVolume_s32 != 0) {
                    do {
                        xa_apply_mix_p((u8) g_XaFadeVolOut);
                        vol = g_XaFadeVolOut - 1;
                        g_XaFadeVolOut = vol;
                    } while (vol != 0);
                }
                xa_stream_start();
                return;
            }
            if ((u32) g_XaEndSector < (u32) sector) {
                g_XaFadeVolOut = g_XaVolume_s32;
                if (g_XaVolume_s32 != 0) {
loop_8:
                    xa_apply_mix_p((u8) g_XaFadeVolOut);
                    vol2 = g_XaFadeVolOut - 1;
                    g_XaFadeVolOut = vol2;
                    if (vol2 != 0) {
                        goto loop_8;
                    }
                }
                goto block_14;
            }
            goto block_15;
        }
        goto block_15;
    }
    if ((CdControlB(1U, NULL, g_XaGetlocResult) != 0) && ((g_XaGetlocResult[0] & 0x34) != 0x20)) {
        g_XaFadeVolOut = g_XaVolume_s32;
        if (g_XaVolume_s32 != 0) {
            do {
                xa_apply_mix_p((u8) g_XaFadeVolOut);
                vol3 = g_XaFadeVolOut - 1;
                g_XaFadeVolOut = vol3;
            } while (vol3 != 0);
        }
block_14:
        xa_apply_mix_p(0U);
        cd_control_retry_a4(9, 0, 0);
        g_XaState = 0;
        SsSetSerialAttr(0, 0, 0);
    }
block_15:
    frames = g_XaFrameCounter_u32 + 1;
    g_XaFrameCounter_u32 = frames;
    if (frames >= 0x8CBU) {
        g_XaFadeVolOut = g_XaVolume_s32;
        if (g_XaVolume_s32 != 0) {
            do {
                xa_apply_mix_p((u8) g_XaFadeVolOut);
                vol4 = g_XaFadeVolOut - 1;
                g_XaFadeVolOut = vol4;
            } while (vol4 != 0);
        }
        xa_apply_mix_p(0U);
        cd_control_retry_a4(9, 0, 0);
        g_XaState = 0;
        SsSetSerialAttr(0, 0, 0);
    }
}

s32 cd_control_retry(u8 com, u8 *param, u8 *res) {
    int CdControl(u_char com, u_char *param, u_char *result);
    int CdControlB(u_char com, u_char *param, u_char *result);
    int CdSync(int mode, u_char *result);
    u8 i;

    do {
        while (CdSync(1, NULL) == 0) {
        }
        while (CdControl(1, NULL, g_CdStatusResult) == 0) {
        }
    } while (g_CdStatusResult[0] & 0x50);
    for (i = 0; i < 0x50; i++) {
        if (CdControlB(com, param, res) != 0) {
            return 1;
        }
    }
    return 0;
}
