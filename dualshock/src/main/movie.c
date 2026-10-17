/* movie.c - FMV playback: STR streaming (open, MDEC decode, frames, close) and the CD audio mix for movies. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
s32 cd_get_file_lba(u32 file, s32 *size_out);


s32 str_open_by_name(s8 *fname, u32 first_frame, s32 last_frame, u32 *ring_buf, u32 ring_len, u32 *vlc_buf0, u32 *vlc_buf1, s32 slice_buf, u32 flags) {
    int VSync(int mode);
    int CdControlB();
    CdlFILE *CdSearchFile(CdlFILE *fp, char *name);
    void StSetRing(u_long *ring_addr, u_long ring_size);
    void StSetStream(u_long mode, u_long start_frame, u_long end_frame, void (*func1)(), void (*func2)());
    u_long StGetNext(u_long **addr, u_long **header);
    u_long StFreeRing(u_long *base);
    void DecDCTReset(int mode);
    int DecDCTvlc(u_long *bs, u_long *buf);
    int DecDCToutCallback(void (*func)());
    typedef struct { s32 w[4]; u16 x, y; s32 v[3]; } Hdr32;
    extern Hdr32 g_StrFrameHeader;
    s32 str_cd_start_read_l() __asm__("str_cd_start_read");
    void str_dct_out_callback_l() __asm__("str_dct_out_callback");
    s32 unused[2];
    u32 *addr;
    u32 *header;
    u32 i;

    g_StrRingBuf = ring_buf;
    g_StrEndFrame = last_frame;
    g_StrVlcBufIndex = 0;
    g_StrDecodeBusy = 0;
    g_StrCurrentFrame = 0;
    g_StrPrevFrame = 0;
    g_StrIs24Bit = flags;
    g_StrVlcBuf[0] = (u_long *)vlc_buf0;
    g_StrVlcBuf[1] = (u_long *)vlc_buf1;
    g_StrSliceBuf = slice_buf;
    g_StrSkipDecode = (flags >> 1) & 1;
    DecDCTReset(0);
    DecDCToutCallback(str_dct_out_callback_l);
    while (CdControlB(8, NULL, NULL) == 0) {
    }
    if (CdSearchFile((CdlFILE *)&g_StrCdFile, fname) == NULL) {
        return 1;
    }
    StSetRing((u_long *)ring_buf, ring_len);
    StSetStream(1, first_frame, 0x0FFFFFFF, NULL, NULL);
    if (str_cd_start_read_l(fname) != 0) {
        return 1;
    }
    i = 0;
    while (StGetNext((u_long **)&addr, (u_long **)&header) != 0) {
        VSync(0);
        i++;
        if (i > 600) {
            return 1;
        }
    }
    g_StrFrameHeader = *(Hdr32 *)header;
    g_StrFrameHeader.x = (g_StrFrameHeader.x + 8) / 16 * 16;
    g_StrFrameHeader.y = (g_StrFrameHeader.y + 8) / 16 * 16;
    DecDCTvlc((u_long *)addr, g_StrVlcBuf[g_StrVlcBufIndex]);
    StFreeRing((u_long *)addr);
    return 0;
}

typedef struct {
    s32 w[4];
    u16 width;
    u16 height;
    s32 x[3];
} StrFrameHeader;

s32 str_open(s32 file_id, u32 first_frame, s32 last_frame, u_long *ring_buf, u32 ring_len, u_long *vlc_buf0, s32 vlc_buf1, s32 slice_buf,
                  u32 flags) {
    u_long StFreeRing(u_long *base);
    u_long StGetNext(u_long **addr, u_long **header);
    void StSetStream(u_long mode, u_long start_frame, u_long end_frame, void (*func1)(), void (*func2)());
    void StSetRing(u_long *ring_addr, u_long ring_size);
    void DecDCTReset(int mode);
    extern StrFrameHeader g_StrFrameHeader_v __asm__("g_StrFrameHeader");
    s32 cd_get_file_lba_l(s32, s32 *) __asm__("cd_get_file_lba");
    void str_dct_out_callback_l() __asm__("str_dct_out_callback");
    void cd_mix_fade_in(void);
    void cd_mix_fade_out(void);
    s32 cd_recover_setmode_a8(void);
    CdlLOC loc;
    u8 param[4];
    u8 result[8];
    s32 size;
    u_long *addr;
    StrFrameHeader *hdr;
    s32 v;
    u32 pos;
    u32 i;

    g_StrEndFrame = last_frame;
    g_StrVlcBufIndex = 0;
    g_StrDecodeBusy = 0;
    g_StrCurrentFrame = 0;
    g_StrPrevFrame = 0;
    g_StrIs24Bit = flags;
    g_StrVlcBuf[0] = vlc_buf0;
    g_StrVlcBuf[1] = (u_long *)vlc_buf1;
    g_StrSliceBuf = slice_buf;
    g_StrSkipDecode = (flags >> 1) & 1;
    DecDCTReset(0);
    DecDCToutCallback(str_dct_out_callback_l);
    StSetRing(ring_buf, ring_len);
    StSetStream(1, first_frame, 0x0FFFFFFF, NULL, NULL);
    g_StrStartLba = cd_get_file_lba_l(file_id, &size);
    CdIntToPos(g_StrStartLba, &loc);
    cd_mix_fade_out();
    param[0] = 1;
    param[1] = 1;
    do {
        do {
        retry:
            if (CdSync(0, NULL) != 2) {
                do {
                } while (cd_recover_setmode_a8() == 0);
                goto retry;
            }
        } while (CdControlB(0xD, param, NULL) == 0 || CdControlB(0x15, (u_char *)&loc, NULL) == 0);
    } while (CdRead2(0x1C8) == 0);
    do {
    } while (CdControlB(0x11, NULL, &g_StrGetlocResult) == 0);
    pos = CdPosToInt(&g_StrGetlocPos);
    v = g_StrStartLba;
    if (pos < v - 100 || v + 5 < pos) {
        return 1;
    }
    i = 0;
    do {
        if (StGetNext(&addr, (u_long **)&hdr) == 0) {
            goto found;
        }
        VSync(0);
        i++;
    } while (i < 301);
    return 1;
found:
    cd_mix_fade_in();
    g_StrFrameHeader_v = *hdr;
    g_StrFrameHeader_v.width = (g_StrFrameHeader_v.width + 8) / 16 * 16;
    g_StrFrameHeader_v.height = (g_StrFrameHeader_v.height + 8) / 16 * 16;
    g_StrWidth = g_StrFrameHeader_v.width;
    g_StrHeight = g_StrFrameHeader_v.height;
    DecDCTvlc(addr, g_StrVlcBuf[g_StrVlcBufIndex]);
    StFreeRing(addr);
    return 0;
}

s32 str_decode_next_frame(s32 dst_x_flags, s16 dst_y) {
    int VSync(int mode);
    u_long StGetNext(u_long **addr, u_long **header);
    u_long StFreeRing(u_long *base);
    void DecDCTReset(int mode);
    int DecDCTvlc(u_long *bs, u_long *buf);
    void DecDCTin(u_long *buf, int mode);
    void DecDCTout(u_long *buf, int size);
    typedef struct {
        s32 w[4];
        u16 width;
        u16 height;
        s32 x[3];
    } StrHdr_l;
    extern StrHdr_l g_StrFrameHeader_l __asm__("g_StrFrameHeader");
    extern s16 g_StrSliceRect_l __asm__("g_StrSliceRect");
    extern u32 g_StrWidth_l __asm__("g_StrWidth");
    int VSync(int mode);
    u_long StGetNext(u_long **addr, u_long **header);
    u_long StFreeRing(u_long *base);
    void DecDCTReset(int mode);
    void DecDCTin(u_long *buf, int mode);
    void DecDCTout(u_long *buf, int size);
    int DecDCTvlc(u_long *bs, u_long *buf);
    s32 unused[2];
    u_long *addr;
    StrHdr_l *hdr;
    u32 i;
    s32 ret;
    s32 h;
    s32 t;
    s32 u;
    u16 x;

    i = 0;
    while (StGetNext(&addr, (u_long **)&hdr) != 0) {
        VSync(0);
        i++;
        if (i >= 180) {
            return 1;
        }
    }
    g_StrWidth_l = g_StrFrameHeader_l.width;
    g_StrHeight = g_StrFrameHeader_l.height;
    g_StrCurrentFrame = g_StrFrameHeaderFrameCount;
    if (g_StrFrameHeaderFrameCount >= g_StrEndFrame || g_StrFrameHeaderFrameCount < g_StrPrevFrame) {
        ret = 1;
    } else {
        ret = 0;
    }
    g_StrPrevFrame = g_StrCurrentFrame;
    if (dst_x_flags & 0x400) {
        g_StrSkipDecode = 1;
    }
    if (dst_x_flags & 0x800) {
        g_StrSkipDecode = 0;
    }
    g_StrSliceRect_l = dst_x_flags & 0x3FF;
    g_StrSliceRectY = dst_y;
    t = 0x10;
    if (g_StrIs24Bit) {
        t = 0x18;
    }
    x = g_StrHeight;
    g_StrSliceRectW = t;
    /* do/while(0): scheduling barrier (loop notes) that pins the height load
       after the g_StrSliceRectW store and the g_StrWidth load after these stores */
    do {
        h = g_StrFrameHeader_l.height;
        g_StrSliceIndex = 0;
        g_StrSliceRectH = x;
    } while (0);
    g_StrSliceCount = g_StrWidth_l >> 4;
    u = h << 4;
    if (g_StrIs24Bit) {
        g_StrSliceSize = ((h << 5) + u) / 4;
    } else {
        g_StrSliceSize = h << 3;
    }
    DecDCTReset(1);
    if (g_StrSkipDecode == 0) {
        DecDCTin(g_StrVlcBuf[g_StrVlcBufIndex], g_StrIs24Bit);
        DecDCTout((u_long *)g_StrSliceBuf, g_StrSliceSize);
    }
    g_StrVlcBufIndex = g_StrVlcBufIndex == 0;
    g_StrFrameHeader_l = *hdr;
    g_StrFrameHeader_l.width = (g_StrFrameHeader_l.width + 8) / 16 * 16;
    g_StrFrameHeader_l.height = (g_StrFrameHeader_l.height + 8) / 16 * 16;
    DecDCTvlc(addr, g_StrVlcBuf[g_StrVlcBufIndex]);
    StFreeRing(addr);
    return ret;
}

void str_close(void) {
    s32 CdDataCallback(s32);
    s32 CdReadyCallback(s32);
    s32 CdSync(s32, s32 *);
    int DecDCToutCallback(void (*func)());
    s32 StUnSetRing();
    void cd_mix_fade_out(void);
    s32 cd_recover_setmode_90();
    s32 sync;

    cd_mix_fade_out();
    DecDCToutCallback(0);
    CdDataCallback(0);
    CdReadyCallback(0);
    StUnSetRing();
    do {
        sync = CdSync(1, &g_StrCdSyncResult);
    } while (sync == 0);
    if (sync == 5) {
        do {

        } while (cd_recover_setmode_90() == 0);
    }
}

void str_wait_decode(void) {
    extern volatile s32 g_StrDecodeBusy_l __asm__("g_StrDecodeBusy");

    while (g_StrDecodeBusy_l != 0) {
    }
}

s32 str_is_finished(void) {
    return (u32) g_StrCurrentFrame >= (u32) g_StrEndFrame;
}

s32 str_get_frame(void) {
    return g_StrCurrentFrame;
}

s32 str_cd_start_read(void) {
    void cd_mix_fade_in(void);
    void cd_mix_fade_out(void);
    s32 cd_recover_setmode_a8();
    int CdControlB();
    int CdSync_u8(int mode, u_char *result) __asm__("CdSync");
    int CdRead2(long mode);
    int CdPosToInt(CdlLOC *p);
    u8 param[8];

    cd_mix_fade_out();
    param[0] = 1;
    param[1] = 1;
    g_StrStartLba = CdPosToInt(&g_StrCdFile);
    do {
        do {
inner:
            if (CdSync_u8(0, NULL) == 5) {
                while (cd_recover_setmode_a8() == 0) {
                }
                goto inner;
            }
        } while (CdControlB(0xD, param, NULL) == 0 || CdControlB(0x15, (u8 *)&g_StrCdFile, NULL) == 0);
    } while (CdRead2(0x1C8) == 0);
    cd_mix_fade_in();
    return 0;
}

void str_dct_out_callback(void) {
    int LoadImage(RECT *rect, u_long *p);
    void StCdInterrupt(void);
    void DecDCTout(u_long *buf, int size);
    g_StrSliceIndex += 1;
    if (StCdIntrFlag != 0) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    LoadImage(&g_StrSliceRect, (u_long *)g_StrSliceBuf);
    if (g_StrSliceIndex < g_StrSliceCount) {
        g_StrSliceRect.x += g_StrIs24Bit ? 24 : 16;
        DecDCTout((u_long *)g_StrSliceBuf, g_StrSliceSize);
        g_StrDecodeBusy = 1;
    } else {
        g_StrDecodeBusy = 0;
    }
}

void str_seek_frame(s32 frame) {
    s32 CdControl(s32, s32 *, s32);
    int CdControlB();
    s32 CdIntToPos(s32, s32 *);
    s32 CdRead2_s32(s32) __asm__("CdRead2");
    s32 CdSync_i(s32, s32) __asm__("CdSync");
    s32 StClearRing();
    s32 loc;

    StClearRing();
    CdIntToPos((frame * 0xA) + g_StrStartLba, &loc);
    do {
loop_1:
        if (CdSync_i(1, 0) != 2) {
            goto loop_1;
        }
        if (CdControlB(9, 0, 0) == 0) {
            goto loop_1;
        }
        if (CdControl(0x15, &loc, 0) == 0) {
            goto loop_1;
        }
    } while (CdRead2_s32(0x1C8) == 0);
}

void cd_mix_fade_in(void) {
    int CdMix();
  s8 *atv3;
  s32 half;
  s32 mono_level;
  unsigned int level;
  if (g_CdStereo != 0)
  {
    level = 0;
    do
    {
      *((u8 *) (((s8 *) (&g_CdMixAtv)) + 0)) = level;
      *((s8 *) (((s8 *) (&g_CdMixAtv)) + 1)) = 0;
      *((u8 *) (((s8 *) (&g_CdMixAtv)) + 2)) = level;
      *((s8 *) (((s8 *) (&g_CdMixAtv)) + 3)) = 0;
      CdMix(&g_CdMixAtv);
      level += 1;
    }
    while (((u32) g_CdAudioVolume) >= level);
    return;
  }
  mono_level = 0;
  do
  {
    half = mono_level / 2;
    *((u8 *) (((s8 *) (&g_CdMixAtv)) + 0)) = (u8) half;
    *((s8 *) (((s8 *) (&g_CdMixAtv)) + 1)) = (s8) half;
    *((u8 *) (((s8 *) (&g_CdMixAtv)) + 2)) = (u8) half;
    atv3 = ((s8 *) (&g_CdMixAtv)) + 3;
    *((s8 *) atv3) = (s8) half;
    CdMix(&g_CdMixAtv);
    mono_level += 1;
  }
  while (((u32) g_CdAudioVolume) >= ((u32) mono_level));
}

void cd_mix_fade_out(void) {
    s32 CdMix(s32 *);
    s32 half;
    s32 level;
    s32 mono_level;

    if (g_CdStereo != 0) {
        level = g_CdAudioVolume;
        if (level >= 0) {
            do {
                CD_MIX_FIELD_AS(val0, s8) = (s8) level;
                CD_MIX_FIELD_AS(val1, s8) = 0;
                CD_MIX_FIELD_AS(val2, s8) = (s8) level;
                CD_MIX_FIELD_AS(val3, s8) = 0;
                CdMix(&g_CdMixAtv);
                level -= 2;
            } while (level >= 0);
        }
    } else {
        mono_level = g_CdAudioVolume;
        if (mono_level >= 0) {
            do {
                half = mono_level / 2;
                CD_MIX_FIELD_AS(val0, s8) = (s8) half;
                CD_MIX_FIELD_AS(val1, s8) = (s8) half;
                CD_MIX_FIELD_AS(val2, s8) = (s8) half;
                CD_MIX_FIELD_AS(val3, s8) = (s8) half;
                CdMix(&g_CdMixAtv);
                mono_level -= 2;
            } while (mono_level >= 0);
        }
    }
}

void cd_set_mix_volume(u32 level) {
    extern u8 g_CdMixAtv_u8 __asm__("g_CdMixAtv");
    s32 CdMix();
  u32 half;
  g_CdAudioVolume = level;
  if (g_CdStereo != 0)
  {
    g_CdMixAtv_u8 = (u8) level;
    g_CdMixAtvLR = 0;
    g_CdMixAtvRR = (s8) level;
    g_CdMixAtvRL = 0;
  }
  else
  {
    half = level >> 1;
    g_CdMixAtv_u8 = (u8) half;
    g_CdMixAtvLR = (s8) half;
    g_CdMixAtvRL = (g_CdMixAtvRR = (s8) half);
  }
  CdMix(&g_CdMixAtv_u8);
}

void cd_set_stereo(s32 stereo) {
    extern u8 g_CdMixAtv_u8 __asm__("g_CdMixAtv");
    int CdMix_u8(u_char *vol) __asm__("CdMix");
    u32 half;

    g_CdStereo = stereo;
    if (stereo != 0) {
        g_CdMixAtvLR = 0;
        g_CdMixAtvRL = 0;
        g_CdMixAtv_u8 = g_CdAudioVolume;
        g_CdMixAtvRR = g_CdAudioVolume;
    } else {
        half = (u32) g_CdAudioVolume >> 1;
        g_CdMixAtv_u8 = (u8) half;
        g_CdMixAtvLR = (s8) half;
        g_CdMixAtvRR = (u8) half;
        g_CdMixAtvRL = (s8) half;
    }
    CdMix_u8(&g_CdMixAtv_u8);
}

s32 cd_recover_setmode_90(void) {
    void cd_seek_file0_pause();
    int VSync(int mode);
    int CdControlB();
    u8 res[8];
    u8 com[8];

    com[0] = 0x90;
    CdControlB(0xE, com, res);
    if (res[0] & 0x15) {
        return 0;
    }
    VSync(3);
    cd_seek_file0_pause();
    return 1;
}

s32 cd_recover_setmode_a8(void) {
    void cd_seek_file0_pause();
    int VSync(int mode);
    s32 CdControlB();
    u8 res[8];
    u8 com[8];

    com[0] = 0xA8;
    CdControlB(0xE, com, res);
    if (res[0] & 0x15) {
        return 0;
    }
    VSync(3);
    cd_seek_file0_pause();
    return 1;
}

void cd_seek_file0_pause(void) {
    s32 CdControlB(s32, s32 *, s32);
    s32 CdIntToPos(s32, s32 *);
    s32 CdSync_i2(s32, s32) __asm__("CdSync");
    s32 size;

    CdIntToPos(cd_get_file_lba(0, &size), &g_CdSeekLoc);
    do {
loop_1:
        if (CdSync_i2(1, 0) != 2) {
            goto loop_1;
        }
        if (CdControlB(0x15, &g_CdSeekLoc, 0) == 0) {
            goto loop_1;
        }
    } while (CdControlB(9, NULL, 0) == 0);
}

void mdec_decode_image(s32 bitstream, s32 vlc_buf, s32 out_buf, s32 w, s32 h, s32 is24) {
    void DecDCTReset(int mode);
    void DecDCTin();
    void DecDCTout();
    s32 DecDCToutSync(s32);
    int DecDCTvlc();
    u32 pixels;
    u32 out_size;

    DecDCTReset(0);
    DecDCTvlc(bitstream, vlc_buf);
    DecDCTin(vlc_buf, is24);
    pixels = w * h;
    if (is24 != 0) {
        out_size = (u32) (pixels * 3) >> 2;
    } else {
        out_size = pixels >> 1;
    }
    DecDCTout(out_buf, out_size);
    DecDCToutSync(0);
}

void mdec_decode_image_to_vram(u16 *dst_rect, u32 *bitstream, u32 *vlc_buf, u32 *slice_buf, s32 is24) {
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    void DecDCTReset(int mode);
    int DecDCTvlc(u_long *bs, u_long *buf);
    void DecDCTin(u_long *buf, int mode);
    void DecDCTout(u_long *buf, int size);
    int DecDCToutSync(int mode);
    int DecDCToutCallback(void (*func)());
    RECT r;
    s32 step;
    u32 total;
    u32 i;

    DecDCTReset(0);
    DecDCToutCallback(NULL);
    DecDCTvlc((u_long *)bitstream, (u_long *)vlc_buf);
    DecDCTin((u_long *)vlc_buf, is24);
    step = 0x10;
    if (is24 != 0) {
        step = 0x18;
    }
    r.x = dst_rect[0];
    r.y = dst_rect[1];
    r.w = step;
    r.h = dst_rect[3];
    total = (u32)(FIELD_AT(dst_rect, RECT, ->w, s16) * step) >> 4;
    for (i = 0; i < total; i += step) {
        DecDCTout((u_long *)slice_buf, (u32)(step * ((s16)dst_rect[3] << 1)) >> 2);
        DecDCToutSync(0);
        LoadImage(&r, (u_long *)slice_buf);
        DrawSync(0);
        r.x += step;
    }
}

void mdec_upload_decoded_image(RECT *src, u32 *buf, s32 is24) {
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    RECT r;
    u32 w;
    u32 n;
    u32 i;
    s32 h;
    s32 t;
    s32 stride;
    u32 *p;

    w = 0x10;
    if (is24 != 0) {
        w = 0x18;
    }
    r.x = src->x;
    r.y = src->y;
    r.w = w;
    r.h = src->h;
    h = src->h;
    n = (u32)(src->w * w) >> 4;
    t = h << 4;
    if (is24 != 0) {
        stride = ((h << 5) + t) / 4;
        p = buf;
    } else {
        stride = h * 8;
        p = buf;
    }
    for (i = 0; i < n; i += w) {
        LoadImage(&r, (u_long *)p);
        DrawSync(0);
        p += stride;
        r.x += w;
    }
}

void str_pc_set_buffers(s32 buf, u32 n_sectors) {
    g_StrPcFrameBuf[0] = buf;
    g_StrPcFrameBuf[1] = buf + ((n_sectors >> 1) << 0xB);
}

/* g_StrPcSectorHeaders: one 0x20-byte record per frame buffer (the first 0x14 bytes are the sector's StHEADER) */
typedef struct { u8 b[4]; u16 secCount; u16 nSectors; u32 frameCount; u8 pad[20]; } StrSectorHeader;

s32 str_pc_open(char *name) {
    s32 str_pc_read_frame_a1() __asm__("str_pc_read_frame");
    extern s32 g_StrPcFrameBuf_s32[] __asm__("g_StrPcFrameBuf");
    extern void (*g_StrPcOpenCallback)();
    extern StrSectorHeader g_StrPcSectorHeaders_l[] __asm__("g_StrPcSectorHeaders");
    int PCopen(char *name, int flags, int perms);
    int PCread(int fd, char *buff, int len);
    int PClseek(int fd, int offset, int mode);
    s32 fd;
    s32 a;
    s32 b;

    fd = PCopen(name, 0, 0);
    g_StrPcFd = fd;
    if (fd < 0) {
        return -1;
    }
    g_StrPcHasSubHeaders = 0;
    PCread(fd, (char *)g_StrPcSubHeader, 8);
    a = g_StrPcSubHeader[0] & 0xFF0000;
    b = g_StrPcSubHeader[1] & 0xFF0000;
    g_StrPcSubHeader[0] = a;
    g_StrPcSubHeader[1] = b;
    if (a == 0x420000 && b == a) {
        g_StrPcHasSubHeaders = 1;
    } else if (a == 0x640000 && b == a) {
        g_StrPcHasSubHeaders = 1;
    }
    PClseek(g_StrPcFd, 0, 0);
    g_StrPcSectorHeaders_l[g_StrPcBufIndex].frameCount = 0;
    do {
        str_pc_read_frame_a1(g_StrPcFrameBuf_s32[g_StrPcBufIndex]);
    } while (g_StrPcSectorHeaders_l[g_StrPcBufIndex].frameCount < g_StrPcStartFrame);
    if (g_StrPcOpenCallback != NULL) {
        g_StrPcOpenCallback();
    }
    return 0;
}

void str_pc_init(s32 unused, s32 first_frame, s32 last_frame, s32 open_cb, s32 end_cb) {
    extern s32 g_StrPcOpenCallback_s32 __asm__("g_StrPcOpenCallback");
    extern s32 g_StrPcFrameCallback_s32 __asm__("g_StrPcFrameCallback");
    extern StrSectorHeader g_StrPcSectorHeaders_l[] __asm__("g_StrPcSectorHeaders");
    g_StrPcBufIndex = 0;
    g_StrPcEndFrame = last_frame;
    g_StrPcOpenCallback_s32 = open_cb;
    g_StrPcEof = 0;
    g_StrPcSectorHeaders_l[0].frameCount = 0;
    g_StrPcSectorHeaders_l[1].frameCount = 0;
    g_StrPcStartFrame = first_frame;
    g_StrPcFrameCallback_s32 = end_cb;
}

void str_pc_close(void) {
    s32 PCclose(s32);
    PCclose(g_StrPcFd);
}

s32 str_pc_next_frame(void) {
    s32 str_pc_read_frame_a2(s32) __asm__("str_pc_read_frame");
    extern void (*g_StrPcOpenCallback)();
    extern s32 (*g_StrPcFrameCallback)();
    extern StrSectorHeader g_StrPcSectorHeaders_l[] __asm__("g_StrPcSectorHeaders");
    s32 idx;

    idx = g_StrPcBufIndex == 0;
    g_StrPcBufIndex = idx;
    str_pc_read_frame_a2(g_StrPcFrameBuf[idx]);
    if (g_StrPcOpenCallback != NULL) {
        g_StrPcOpenCallback();
    }
    if ((g_StrPcSectorHeaders_l[g_StrPcBufIndex].frameCount >= (u32) g_StrPcEndFrame) || (g_StrPcEof != 0)) {
        if (g_StrPcFrameCallback != NULL) {
            g_StrPcFrameCallback();
        }
    }
    return 0;
}

s32 str_pc_get_frame(s32 *frame_buf, void **header) {
    *frame_buf = g_StrPcFrameBuf[g_StrPcBufIndex];
    *header = (g_StrPcBufIndex << 5) + &g_StrPcSectorHeaders;
    return 0;
}

void str_pc_read_frame(u8 *buf) {
    extern StrSectorHeader g_StrPcSectorHeaders_l[] __asm__("g_StrPcSectorHeaders");
    int PCread(int fd, char *buff, int len);
    int PClseek(int fd, int offset, int mode);
    s32 magic;
    s32 n;
    s32 a;
    s32 b;

    n = 0;
    if (g_StrPcEof != 0) {
        return;
    }
    do {
        if (g_StrPcHasSubHeaders != 0) {
            while (1) {
                if (PCread(g_StrPcFd, (char *)g_StrPcSubHeader, 8) == 0) {
                    g_StrPcEof = 1;
                    return;
                }
                a = g_StrPcSubHeader[0] & 0xFF0000;
                b = g_StrPcSubHeader[1] & 0xFF0000;
                g_StrPcSubHeader[0] = a;
                do { g_StrPcSubHeader[1] = b; } while (0); /* needed for the original scheduling */
                magic = 0x420000;
                if (a == magic) {
                    break;
                }
                PClseek(g_StrPcFd, 0x918, 1);
                n++;
            }
        }
        if (PCread(g_StrPcFd, (char *)&g_StrPcSectorHeaders_l[g_StrPcBufIndex], 0x14) == 0) {
            g_StrPcEof = 1;
            return;
        }
        PClseek(g_StrPcFd, 0xC, 1);
        PCread(g_StrPcFd, (char *)buf, 0x7E0);
        buf += 0x7E0;
        if (g_StrPcHasSubHeaders != 0) {
            PClseek(g_StrPcFd, 0x118, 1);
        }
        n++;
    } while (g_StrPcSectorHeaders_l[g_StrPcBufIndex].secCount != g_StrPcSectorHeaders_l[g_StrPcBufIndex].nSectors - 1);
    if (g_StrPcSectorHeaders_l[g_StrPcBufIndex].frameCount >= g_StrPcEndFrame) {
        g_StrPcEof = 1;
    }
    g_StrPcFrameSectors = n;
}
