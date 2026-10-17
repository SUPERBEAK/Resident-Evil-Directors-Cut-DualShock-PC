# RE1 disc contents (simple overview)

The Resident Evil Director's Cut **Dual Shock** (USA) disc is dumped into `extracted/` (git-ignored). Put
your own disc dump into `game_files/RE1-Dualshock/` and run:

```sh
sh tools/dump_discs.sh
```

## What the decomp uses

| What | Files on disc |
|---|---|
| The program the decomp rebuilds byte for byte | `SLUS_007.47` (main program) + `PSXUSA/PROG2/*.EXE` (12 overlays) |
| Everything else (rooms, models, sounds, movies) | taken from your disc when `dualshock/build_disc.bat` builds a disc image |

## What's on the disc

| Folder | What it is | Size |
|---|---|---|
| `ZMOVIE` | 24 FMVs (about 17 minutes) | 339 MB |
| `VOICE` | Character voices (XA audio) | 104 MB |
| `STAGE1` … `STAGEE` | Rooms (`.RDT`) and room backgrounds (`.BSS`) | ~150 MB |
| `SOUND` | Music (57 tracks, `SEP*.HSB`) + sound effect banks | 13 MB |
| `ITEM_M1`, `ITEM_M2` | Door animations, 3D item models | 14 MB |
| `ENEMY` | Enemy models | 10 MB |
| `PLAYERS` | Player models | 0.8 MB |
| `DATA` | Menus, title screen, fonts | 2.3 MB |

**The music is "sheet music + instruments" (like MIDI), not recorded audio.** Each music file is
small (about 100–260 KB).

More detail is in [disc_contents_detailed.md](disc_contents_detailed.md).
