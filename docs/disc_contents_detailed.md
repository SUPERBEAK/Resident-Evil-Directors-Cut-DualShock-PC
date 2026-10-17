# RE1 disc contents (detailed)

Dumped 2026-10-06 with dumpsxiso 2.30 (`-l`). Script: `tools/dump_discs.sh`.

## Source image

| Image | Track 1 (MODE2/2352) SHA1 | Track 2 (CD audio) SHA1 |
|---|---|---|
| RE1 DC Dual Shock (USA), SLUS-00747 | `08fbc01ae9c3d47aa93d2a99d69fa67a54e4982b` (691,205,760 B) | `d9f92af296360772e62caa4cb276de3fa74f5538` |

- Track 2 is a 3:32 CD audio track; dumpsxiso names it `ZNULL.WAV`. Its content hasn't been
  identified yet.
- `SYSTEM.CNF`: `BOOT = cdrom:\SLUS_007.47;1`, `STACK = 0x801FFF00`.

## Output layout (`extracted/`, git-ignored)

| Path | Content |
|---|---|
| `RE1-Dualshock/` | All disc files |
| `RE1-Dualshock.xml` | mkpsxiso rebuild script with the original LBAs |
| `RE1-Dualshock.sha1` | SHA1 of every extracted file |

## File types

| Folder | Files | Size | Notes |
|---|---|---|---|
| `PSXUSA/DATA` | 13 TIM, 11 PIX, 4 DAT, STF, ETM, ESP | 2.3 MB | UI, title, fonts. `.PIX` is a raw 320×240 image (153,600 B) |
| `PSXUSA/ENEMY` | 74 EMD | 9.8 MB | Enemy models + animations |
| `PSXUSA/ITEM_M1` | 34 DOR | 2.2 MB | Door-opening animations |
| `PSXUSA/ITEM_M2` | 79 IVM, 48 PIX, 34 TIM | 12 MB | Item models (inventory view) |
| `PSXUSA/PLAYERS` | 32 EMW, 7 TMD | 0.8 MB | Player weapon models / player models |
| `PSXUSA/PROG2` | 12 EXE | 1.2 MB | Code overlays |
| `PSXUSA/SOUND` | 57 HSB, 35 HED+VB | 13 MB | Music + sound effect banks (see below) |
| `PSXUSA/STAGE1-E` | 323 RDT, 160 BSS | ~150 MB | Rooms; BSS files are 229,376 B (backgrounds, one MDEC picture per camera cut: `tools/bss_view.py`) |
| `PSXUSA/VOICE` | 5 XAS | 104 MB | XA: 16 channels per file, mono, 37.8 kHz, 4-bit ADPCM |
| `PSXUSA/ZMOVIE` | 24 STR | 339 MB | FMV (MDEC video + XA audio) |

## Audio format

- **`SEPxx.HSB` = one music track.** Each file holds a VAB header (`pBAV`), a SEQ (`pQES`)
  and the VAB sample body, in that order. There are 57 slots (`SEP00`–`SEP38`). It's sequenced
  music, not streamed audio.
- `*.HED` + `*.VB` = VAB header + body for sound-effect banks (characters, doors, weapons
  `WP00x`, `BIO`, `EVIL`, `SELECT`, `ENDING`).
- RDT room files contain 334 embedded VABs in total (room-specific sounds).
- `VOICE/*.XAS`: 80 XA streams in total (voices).
- FMV audio: stereo XA, 37.8 kHz, 4-bit, interleaved in the STR.

## FMVs

Measured from the STR video sector headers (magic `0x0160` / `0x8001`, frame number, width, height) of the
extracted files, which dumpsxiso writes as 2336-byte sectors (8-byte subheader + data):

- 2× disc speed (150 sectors/s). 10 sectors per frame (video + interleaved XA audio) means 15 fps;
  `CAPCOM.STR` has about 7.5 sectors per frame, so 20 fps.
- Size: `CAPCOM`, `ED*`, `OJ`, `PJ` (intro) and `STFR` (credits) are 320×240. The `DM*`
  files (short in-game clips) are 256×240.
- Total: 24 files, 15,215 frames, **1,012 s = 16.9 min**. The longest are `STFR` (4,045 frames, 270 s)
  and `PJ` (3,158 frames, 211 s).
- The movie player (`fmv_play`, 0x800376DC) skips frames 0x97E..0xA15 of movie 1 (`PJ.STR`) when playing
  as Jill: the mansion doors / main hall shots that Chris narrates.
