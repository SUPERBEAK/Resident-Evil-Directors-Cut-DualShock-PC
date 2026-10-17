# Project history

How the decompilation got to its current state, phase by phase. The current status is in the
[README](../README.md); how to work on the code is in [decomp_guide.md](decomp_guide.md).

## 1. Setup

- Tools installed inside the project (`tools/setup_tools.sh`, see [tools.md](tools.md)).
- The Dual Shock disc dumped and inventoried: [disc_contents.md](disc_contents.md).
- GCC 2.x for PlayStation built from source in MSYS2; the full pipeline (cpp → cc1 → maspsx → as)
  passes `tools/test_psx_toolchain.sh`.

## 2. Identifying the Sony SDK and library functions

Sony's PsyQ libraries (libgpu, libgte, libcd, libspu/libsnd, libetc, libapi, libc, libpad/libgun, ...) make
up a big part of `SLUS_007.47`. They are Sony's code, not Capcom's, so they are named and skipped, not
decompiled. Result ([psyq_sdk_detailed.md](psyq_sdk_detailed.md)):

- Base SDK is **PsyQ 3.7** (LIBGTE/GPU/CD/CARD/GS/ETC/C/PRESS). LIBSND/LIBSPU are **3.3**
  (two LIBSND objects are newer). LIBPAD (+ part of LIBAPI) is **4.2**.
- Main program: Capcom code 0x80010000–0x80061880 (≈326 KB); Sony libs
  0x80061880–0x8008F89C (≈188 KB); data 0x8008F89C–0x800B2800.
- The overlays are pure Capcom code (+ a 24-byte Sony startup stub each).
- `dualshock/config/symbol_addrs_psyq.txt` (687 unique names) and
  `dualshock/config/psyq_report_SLUS_007.47.md`, made by `tools/psyq_match.py` with the PsyQ signatures of
  ghidra_psx_ldr.

## 3. A byte-identical rebuild from assembly

`make` in `dualshock/` rebuilds `SLUS_007.47` (text at 0x80010000, size 0xA2800, entry 0x80061888) and all
12 overlays SHA1-identical. `LOGO`, `PROLOGUE`, `TITLE`, `SELECT` and `ENDING` share the load address
0x800E0000, `STAGE1–7` share 0x80105400; each is split as its own splat segment. The key fixes were turning
off splat's 16-byte section alignment, using each executable's own `gp`, and declaring the main program as
the global VRAM range for the overlays. Details: [matching_guide.md](matching_guide.md).

## 4. Matching decompilation of Capcom's code

- **The compiler:** GCC 2.6.3 for PlayStation, `-O2 -G0` for the main program and `-O0` for the overlays.
  On the first 15 test functions 2.6.3 matched 15, 2.7.2 matched 13 and 2.8.1 matched 4. Capcom code
  never uses `$gp`.
- **The method:** m2c drafts for every function, automatic repairs and a mutation search
  (`autodecomp.py`, `refine.py`), decomp-permuter for the hardest -O2 cases, and hand-matching; every rule
  found on the way is in [matching_guide.md](matching_guide.md).
- **The result:** all 2,430 functions (1,444,396 bytes) in all 13 executables are C that rebuilds
  byte-identically, with no `INCLUDE_ASM` left. Two functions carry a `FAKEMATCH` comment
  (func_8003E228: an empty `__asm__("")` blocking a cross-jump that no natural C avoids;
  func_80056FC0: a volatile store forcing a reload).

## 5. Readability pass (byte match kept after every step)

- **Names:** every function (962 in the main program, 1,468 in the overlays) and every global has a name;
  the evidence for each name is in `dualshock/config/*names*` / `*globals*`. Where the RE1 PC decompilation
  has the same function, its name is used.
- **Types:** `Entity` fields named after the RE1 PC decompilation; every field access in the C code is
  named (no raw field macros or offsets left, apart from a few stream reads on purpose); all placeholder
  fields of the shared and local structs named; `psyq_types.h` `GsCOORDINATE2` / `GsDOBJ5` corrected.
- **Files:** the main program split from one file into 45 topic files; each stage overlay split into
  `entry.c` + one file per enemy; every enemy used by several stages is compiled from one `src/shared/`
  file (stage code halved).
- **Enemy data:** each enemy's own part of the entity (bytes 0x16C..) is a named struct per enemy
  (e.g. `ZOMBIE(g_CurrentEntity)->stagger_timer`). The player's own data is `PlayerWork`
  (`poison_timer`, `max_health`, `aim_flags`, ...).
- **Addresses:** every RAM address the code has to write as a number is named (`LIT(u8, g_StageId)`,
  `LITP(Effect, g_EffectPool)[i]`, `tools/litaddr.py`), and so is every scratchpad slot (`sp_` names); the
  PS1 RAM map: [re1_ram_map.md](re1_ram_map.md).
- **Locals:** all local variables and parameters named; per-function `__asm__` aliases reduced to the ones
  the byte match needs (`tools/aliastry.py`).
- **Data:** all data is C: strings and tables are local arrays, string literals or `const` definitions;
  `src/` includes no assembly at all.
- **Review:** a whole-codebase review fixed misleading names, added
  constants for flags / pad bits / sizes / enemy states / sounds, one declaration of the overlay API, shared
  stage headers and a header comment on every file. The build has 0 compiler warnings.

## 6. Preparing the public release

- No license file; credits in `CREDITS.md`. m2c's GPL-3.0 `m2c_macros.h` is downloaded by
  `dualshock/tools/m2c_macros.py` instead of being committed.
- Tool versions pinned; a fresh setup and a fresh clone checked end to end (setup, disc dump,
  `make split`, `make`: 13/13 match; disc image CRC-identical). Details: [tools_detailed.md](tools_detailed.md).
