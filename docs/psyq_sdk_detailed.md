# Sony SDK in RE1 Dual Shock (detailed)

Target: `extracted/RE1-Dualshock/SLUS_007.47` (text 0x80010000,
size 0xA2800, entry 0x80061888) and the 12 `PSXUSA/PROG2/*.EXE` overlays.

## Method: `tools/psyq_match.py`

```sh
tools/external/venv/Scripts/python.exe tools/psyq_match.py \
    extracted/RE1-Dualshock/SLUS_007.47 \
    --report dualshock/config/psyq_report_SLUS_007.47.md \
    --symbols dualshock/config/symbol_addrs_psyq.txt
```

(Runtime is about 20 s; all 16 SDK versions are scanned.)

- **Signatures:** ghidra_psx_ldr's `data/psyq/<ver>/<LIB>.json` (installed in the project's
  Ghidra settings dir). Each object has a hex signature, where `??` marks relocated bytes,
  plus labels at offsets. Versions available: 2.6, 3.0, 3.3, 3.4, 3.5, 3.6.10, 3.6.11, 3.7,
  4.0–4.7. Data-only objects (just an `xbss` list) are skipped.
- **A candidate is accepted only if all of these hold:**
  1. It matches exactly **once**, 4-byte aligned.
  2. It has at least **12 fixed bytes**.
  3. It starts at a **function boundary**: the previous word is 0, or the word two before
     is `jr <reg>` / `j`.
- **Rejected in this run:** 225 candidates matched more than once (tiny generic functions,
  e.g. `LIBDS`/`LIBCOMB`/`_OP_VDEL` patterns found 10–23 times). Another 41 weren't at a
  function boundary: they had matched the tail end of Capcom functions, e.g. "SpuVmCheck"
  at 0x8003BA30 and "GsSetOrign" at 0x8003BAE0, inside Capcom code.
- **Overlaps:** assignment goes longest-signature-first, and overlapping candidates are dropped.
- Identical matches across versions are merged. Each object lists every version whose
  signature fits. A library's "consistent versions" is the intersection over its objects.

## Results: main executable

| Library | Objects | Bytes | Consistent versions | Conclusion |
|---|---|---|---|---|
| LIBGTE | 87 | 46,804 | 3.7 | **3.7** |
| LIBSND | 36 | 24,636 | (mixed) | **3.3**: 34/36 objects. `VM_PROG.OBJ` matches 3.6.10–4.0 and `UT_PB.OBJ` matches 3.6.x, so Capcom updated two objects |
| LIBGS | 36 | 21,912 | 3.7, 4.0 | 3.7 (identical in 4.0) |
| LIBGPU | 4 | 18,140 | 3.7 | **3.7** |
| LIBCD | 15 | 16,188 | 3.7 | **3.7** |
| LIBSPU | 24 | 13,468 | 3.3 | **3.3** |
| LIBPAD | 6 | 6,848 | (mixed) | **4.2**: `PADSEQD` only matches 4.2; the others match 4.2 and later (unchanged code) |
| LIBETC | 6 | 3,460 | 3.7, 4.0 | 3.7 |
| LIBAPI | 61 | 2,992 | (mixed) | System-call stubs are identical in every version (the "2.6" hits are an artifact: 2.6 stores each stub as its own object). A few objects are 4.2+ only, so LIBAPI comes from **4.2** (with LIBPAD) |
| LIBPRESS | 2 | 2,808 | 3.7, 4.0 | 3.7 (MDEC decoding for the FMVs) |
| LIBC | 1 | 2,140 | 3.7, 4.0 | 3.7 |
| LIBSN | 5 | 300 | 3.3–4.6 | (identical in all) |
| LIBC2 | 1 | 108 | 3.3–4.0 | (identical in all) |
| 2MBYTE.OBJ | 1 | 196 | all | Startup. `__SN_ENTRY_POINT` (`stup2`) = **0x80061888 = the EXE entry point**, which confirms the match independently |

**Total: 160,788 bytes of identified Sony code (24.2% of text), 687 unique function names.**

Evidence that fits: the Sony `$Id` strings `sys.c 1.129 1996/12/25`, `bios.c 1.81
1996/12/16` and `intr.c 1.74 1996/12/04` are from around PsyQ 3.7. The build stamp
`CAPCOM-Jul 27 1998` is the Dual Shock re-release, which added LIBPAD 4.2.

## Memory map (main executable)

| Range | Size | Content |
|---|---|---|
| 0x80010000–0x80061880 | 0x51880 (≈326 KB) | **Capcom code**: 962 `jr ra` (about 1,000 functions) |
| 0x80061880–0x8008F89C | 0x2E01C (≈188 KB) | **Sony libraries**, linked after the game code. About 160.8 KB identified; **≈27 KB unidentified** in gaps (biggest: 0x80084A1C–0x80088060, 13.6 KB, between LIBSND objects, so probably LIBSND objects whose version isn't in the signature set) |
| 0x8008F89C–0x800B2800 | 0x22F64 (≈140 KB) | **Data** (rodata/data). Only 2 coincidental `jr ra` words; the last at 0x800A9824 |

Smaller gaps inside the library range (0.6–3.7 KB each) are around LIBGTE (`DVF3`/`DVG3`/`DVGT3`
objects matched twice, so ambiguous), LIBSND and LIBPAD.

## Overlays (`PROG2/*.EXE`)

All 12 (LOGO, PROLOGUE, TITLE, SELECT, ENDING at 0x800E0000; STAGE1–7 at 0x80105400)
only contain **`NONE2.OBJ`** (24 bytes: `__main`, `stup2`, `stup0`). That's Sony's minimal
startup stub, there because every overlay was linked as its own PS-X EXE. The EXE entry
point of each overlay is that `stup2` (e.g. LOGO 0x800E03AC). Everything else in the
overlays is Capcom code and data.

## Symbol file conventions (`dualshock/config/symbol_addrs_psyq.txt`)

- splat format: `Name = 0xADDR; // type:func psyq LIB:OBJ`.
- Only global labels; local `loc_`/`text_` labels are dropped.
- **Duplicate names get `_<OBJ>`.** They're genuine in Sony's libraries: BIOS stub vs.
  wrapper (`InitPAD_PAD` vs `InitPAD_A18`), LIBCARD vs. LIBAPI (`InitCARD_INIT` /
  `InitCARD_A74`), file-local helpers (`memclr_INTR`, `memclr_INTR_VB`, `memclr_INTR_DMA`),
  and `memcpy`/`memset` in both LIBAPI and LIBCD/LIBGPU.

## What this means for the decomp

- splat segments: Capcom code 0x80010000–0x80061880, Sony libs 0x80061880–0x8008F89C (kept as
  assembly, never decompiled), then data. The ≈27 KB of unidentified library-range code stays as
  assembly too.
- The libraries don't decide Capcom's compiler (Sony shipped them pre-built). A PsyQ 3.7-era build
  pointed to GCC 2.7.2, but matching functions showed that Capcom's code was compiled with **GCC 2.6.3**
  (see [matching_guide.md](matching_guide.md)).
- The Sony functions the game calls (687 names) are the game's whole interface to the PS1
  hardware: the GPU/GS/GTE, SND/SPU, CD, PAD/CARD and ETC families.

## Correction

The function at **0x8008C37C** was matched as `SpuRead`, but it is **`SpuWrite`** (it calls
`_spu_write`; the two are near-identical code). Fixed in `dualshock/config/symbol_addrs_psyq.txt`
(`SpuWrite = 0x8008C37C`).
