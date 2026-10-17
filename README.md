# Resident Evil Director's Cut: Dual Shock Ver. Decompiled

## Status
100% matched!

Every function of the game's 13 program files is C source code that compiles, with the same compiler
generation Capcom used (GCC 2.6.3 for PlayStation), to **exactly the original bytes**. No assembly is
included anywhere, every function and global has a name, and the build has no compiler warnings.

| Program | What it is | Functions | Bytes of code | Matched |
|---|---|---:|---:|---:|
| `SLUS_007.47` | Main program (engine, player, items, menus, sound, ...) | 962 / 962 | 327,076 | 100% |
| `LOGO.EXE` | Capcom logo | 1 / 1 | 932 | 100% |
| `PROLOGUE.EXE` | Prologue | 1 / 1 | 684 | 100% |
| `TITLE.EXE` | Title screen | 7 / 7 | 6,996 | 100% |
| `SELECT.EXE` | Character select | 5 / 5 | 19,956 | 100% |
| `ENDING.EXE` | Ending and results | 20 / 20 | 23,068 | 100% |
| `STAGE1.EXE` | Mansion 1F | 208 / 208 | 117,148 | 100% |
| `STAGE2.EXE` | Mansion 2F (Yawn) | 114 / 114 | 114,916 | 100% |
| `STAGE3.EXE` | Courtyard and caves (Black Tiger, Cerberus, hunters) | 245 / 245 | 181,436 | 100% |
| `STAGE4.EXE` | Guardhouse (Plant 42, Neptune, wasps) | 249 / 249 | 187,040 | 100% |
| `STAGE5.EXE` | Laboratory (Tyrant, chimeras) | 203 / 203 | 163,404 | 100% |
| `STAGE6.EXE` | Mansion 1F revisited (hunters) | 228 / 228 | 144,836 | 100% |
| `STAGE7.EXE` | Mansion 2F revisited (hunters, Yawn) | 187 / 187 | 156,904 | 100% |
| **Total** | | **2,430 / 2,430** | **1,444,396** | **100%** |

Sony's PsyQ library functions inside the main program are identified and named, not decompiled
(see [docs/psyq_sdk.md](docs/psyq_sdk.md)). The numbers above come from `dualshock/tools/progress.py`.

## Why?

### Newest version
The dualshock version is by definition (aside from the DS version), the most modern Resident Evil version.

### What about the music?
Luckily, music is not bound to code, so it can be swapped out.

### Modding
Not only does this allow us to understand Resident Evil better, it allows for a broader amount of mods on native hardware.

## How to build

The game itself is **not** included: you need your own copy of *Resident Evil Director's Cut Dual Shock
Ver.* (USA, SLUS-00747) as a disc dump. The build runs on **Windows** and on **Linux** (x86_64); both produce
the same, byte-identical result.

### Requirements

- A disc dump of the game: a `.cue` with `(Track 1).bin` and `(Track 2).bin`
- **Windows:** [Git for Windows](https://gitforwindows.org/) (the scripts run in **Git Bash**) and
  [Python 3.14](https://www.python.org/) on PATH
- **Linux:** Python 3.12 or newer and a few standard packages, e.g. on Debian / Ubuntu:

  ```sh
  sudo apt install curl git unzip tar make gcc patch diffutils binutils python3 python3-venv
  ```

Everything else (the MIPS assembler/linker, GCC 2.6.3 for PlayStation built from source, splat, m2c, maspsx,
mkpsxiso, ...; on Windows also MSYS2) is downloaded and built into `tools/external/` by the setup script.
Put the project in a folder path **without spaces** (required on Linux, safest on Windows).

### Steps

Run these from the project folder, in **Git Bash** on Windows or a terminal on Linux:

1. **Install the tools** (the first run downloads several GB and builds the old compilers; it takes a
   while, and it can be run again safely - it skips what is already installed):

   ```sh
   sh tools/setup_tools.sh
   ```

2. **Add your game:** put the disc dump into `game_files/RE1-Dualshock/`, then extract it:

   ```sh
   sh tools/dump_discs.sh
   ```

3. **Split and build:**

   ```sh
   cd dualshock
   sh ../tools/buildenv.sh "make split"   # cut the original programs into assembly and data
   sh ../tools/buildenv.sh "make -j8"     # compile the C and compare with the originals
   ```

   When everything is right, it prints `OK` for each of the 13 files and
   `All 13 executables match the originals.`

4. **Optional - build a playable disc image:**

   ```sh
   sh ../tools/buildenv.sh "make disc"    # still in dualshock/ (on Windows you can also double-click build_disc.bat)
   ```

   It puts the 13 programs built from the C code into a copy of your disc and writes `dualshock/build/disc/`
   (`(Track 1).bin`, `(Track 2).bin` and the `.cue`). Built from unchanged code, the image is CRC-identical
   to the original dump; open the `.cue` in an emulator.

If the game files don't match, check your dump: the Track 1 `.bin` of the expected disc has the SHA1
`08fbc01ae9c3d47aa93d2a99d69fa67a54e4982b`.

## Documentation

All documentation is in [docs/](docs/README.md):

- [decomp_guide.md](docs/decomp_guide.md) - working on the code: the tools, naming conventions, folder layout
- [matching_guide.md](docs/matching_guide.md) - how to write C that GCC 2.6.3 turns into the original code
- [code_notes.md](docs/code_notes.md) - how the source is organized, and why some odd-looking forms must stay
- [game_notes.md](docs/game_notes.md) - how the game works, as found while decompiling it
- [tools.md](docs/tools.md) - the installed tools and scripts

Credits for the tools and references this project builds on: [CREDITS.md](CREDITS.md).

## Legal

This is an unofficial fan project. It is not affiliated with or endorsed by Capcom or Sony Interactive
Entertainment. *Resident Evil* is a trademark of Capcom Co., Ltd.; *PlayStation* is a trademark of Sony
Interactive Entertainment Inc. This repository contains no game files, assets or executables: building it
requires your own copy of the game.

# AI disclaimer
This decompilation was done with the assistance of AI.
