# Matching guide

*Writing matching C for GCC 2.6.3*

This project is a byte-matching decompilation: the C sources in `dualshock/src`, built with the
same compiler Capcom used (GCC 2.6.3 for the PlayStation) and an assembler step that behaves like
Sony's ASPSX, produce all 13 executables of the Dual Shock disc SHA1-identical to the originals.
Every function is C (962 in the main program, 1,468 in the overlays) and no `INCLUDE_ASM` is
left, so this guide is mainly for changing code (renaming, retyping, restructuring, splitting
files) without breaking the match. Many C forms that look equivalent compile to different bytes
with this compiler, and the rules below explain which forms must stay and why. They also serve
anyone matching another PS1 game built with GCC 2.6.3, since most of them are properties of the
compiler and not of Resident Evil.

For a beginner introduction to the build, see [decomp_guide.md](decomp_guide.md). Naming and
code conventions are in [code_notes.md](code_notes.md), game knowledge is in
[game_notes.md](game_notes.md), and the full tool reference is in
[tools_detailed.md](tools_detailed.md).

## Contents

1. [The toolchain and build pipeline](#the-toolchain-and-build-pipeline)
2. [Verifying a change](#verifying-a-change)
3. [How GCC 2.6.3 decides the output](#how-gcc-263-decides-the-output)
4. [Capcom's coding idioms and the C that reproduces them](#capcoms-coding-idioms-and-the-c-that-reproduces-them)
5. [Field access: member form vs pointer form](#field-access-member-form-vs-pointer-form)
6. [Rules for the overlays (-O0)](#rules-for-the-overlays--o0)
7. [Rules for the main program (-O2)](#rules-for-the-main-program--o2)
8. [Data, rodata and alignment](#data-rodata-and-alignment)
9. [Declarations, types and casts](#declarations-types-and-casts)
10. [Common m2c draft errors](#common-m2c-draft-errors)
11. [The draft and search tools](#the-draft-and-search-tools)
12. [Compiler bugs and crashes](#compiler-bugs-and-crashes)
13. [FAKEMATCH](#fakematch)

**Paths in this guide.** Commands are run from `dualshock/`. A bare tool name such as
`refine.py`, and a path such as `tools/hunkcheck.py` in a command run from `dualshock/`, refer to
`dualshock/tools/`. Project-level scripts are written with their full path from the project root
(for example the top-level `tools/build_old_gcc.sh`), and installed third-party tools are in the
top-level `tools/external/`.

**Function names.** Many examples use the address names (`func_8010F7FC`, `D_800CAB14`) under
which the rule was found. Many functions and globals have readable names in `src/`; the
address stays the reliable key (stage overlays reuse the same address range, so an overlay
example names its overlay where the source did).

---

## The toolchain and build pipeline

### Build targets

Running `make` in `dualshock/` rebuilds all 13 executables of the Dual Shock disc, each
SHA1-identical to the original.

| File | VRAM | Layout (file offsets) | gp |
|---|---|---|---|
| SLUS_007.47 | 0x80010000 | header 0x0, rodata 0x800, Capcom asm 0x2278 (0x80011A78), Sony asm 0x52080 (0x80061880), data 0x8013C (0x8008F93C), end 0xA3000 | 0x800B235C |
| LOGO | 0x800E0000 | asm 0x800, end 0x1000 | 0x800E03BC |
| PROLOGUE | 0x800E0000 | asm 0x800, end 0x1000 | 0x800E02C4 |
| TITLE | 0x800E0000 | asm 0x800, end 0x2800 | 0x800E1BD0 |
| SELECT | 0x800E0000 | asm 0x800, data 0x5644, end 0x5800 | 0x800E4E68 |
| ENDING | 0x800E0000 | rodata 0x800, asm 0x818, data 0x62CC, end 0x6800 | 0x800E5D9C |
| STAGE1 | 0x80105400 | asm 0x800, data 0x1D30C, end 0x1E000 | 0x80122778 |
| STAGE2 | 0x80105400 | asm 0x800, data 0x1CB0C, end 0x1D800 | 0x80121DE8 |
| STAGE3 | 0x80105400 | asm 0x800, data 0x2CF18, end 0x2D800 | 0x80132400 |
| STAGE4 | 0x80105400 | rodata 0x800, asm 0xAF0, data 0x2E69C, end 0x2F800 | 0x80133C9C |
| STAGE5 | 0x80105400 | asm 0x800, data 0x288AC, end 0x29800 | 0x8012DE60 |
| STAGE6 | 0x80105400 | asm 0x800, data 0x2408C, end 0x25000 | 0x801296F4 |
| STAGE7 | 0x80105400 | asm 0x800, data 0x26F94, end 0x28000 | 0x8012C4F0 |

The rodata/asm/data starts in this table are `splat create_config` estimates. They are good
enough for byte-identical assembly, but they are not the exact section boundaries. The main
executable's measured layout (where Capcom code really starts at 0x80011ADC, not the estimated
0x80011A78) is in [Main executable layout](#main-executable-layout).

### Tools

- **splat 0.50.0** (spimdisasm 1.42.4) splits the original executables into `asm/<target>/` and
  writes `build/<target>.ld` plus `build/<target>_undefined_{funcs,syms}_auto.txt`.
- **MIPS binutils 2.45** (`mips64-elf-as/ld/objcopy`) handle the PS1 objects:
  - `as -EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -O1 -G0 -Iinclude`
  - `ld -EL -m elf32ebmip -nostdlib --no-check-sections`. `-EL` turns the big-endian-named
    emulation into `elf32-littlemips`, so no separate mipsel binutils are needed.
  - `objcopy -O binary` writes the linked ELF straight out as the PS-X EXE. The EXE header is
    linked in as the `header` segment.
- **Compiler:** `cc1` from **gcc-2.6.3-psx** (see [Which compiler](#which-compiler-gcc-263)). The
  project's `cc1` is a 64-bit MSYS2 build made by the top-level `tools/build_old_gcc.sh 2.6.3`,
  with a crash fix described in [Compiler bugs and crashes](#compiler-bugs-and-crashes).
- **maspsx** (`--aspsx-version=2.56 --expand-div`) rewrites GCC's assembly the way Sony's ASPSX
  assembler would (see [maspsx behaviour](#maspsx-behaviour)).
- Everything runs through the environment wrapper, from inside `dualshock/`:

  ```sh
  sh ../tools/buildenv.sh "make ..."
  ```

  It provides MSYS2 `make` and the right `PATH`.

### The C pipeline (`dualshock/Makefile`)

```
cpp -> cc1 (gcc-2.6.3-psx) -> [jtalign.awk, main program] / [alignfix, overlays] -> maspsx -> mips64-elf-as
```

The intermediate files `.i`, `.s` and `.ms.s` are kept in `build/src/...`.

```make
CPPFLAGS = -undef -nostdinc -Iinclude -D__GNUC__=2 -Dmips -D__mips__ -D_LANGUAGE_C -DLANGUAGE_C
CFLAGS   = -quiet -O2 -G0 -mcpu=3000 -mips1 -fno-builtin -msoft-float
```

- **Optimization level per target:** the Makefile picks it with `opt_for`: `src/main/*` gets
  `-O2`, everything else (all overlays) gets `-O0`. See
  [-O0 and -O2](#-o0-and--o2).
- **`-DM2C_FIELD_STRUCT`** is set by the Makefile and the tools for the `-O0` targets (see
  [The m2c field macros](#the-m2c-field-macros)).
- **`-G0`:** Capcom code never uses `$gp` (0 uses in `capcom`, `stage1`). Only Sony's startup code
  sets it.
- **Jump-table alignment steps:** for every non-main target the Makefile's `alignfix_for` step
  rewrites GCC's `.align 3` before jump tables to `.align 2`; for the main program
  `dualshock/tools/jtalign.awk` runs on cc1's output, before maspsx (which drops comments), and
  converts only the tables marked `#JTALIGN2`. See
  [Jump-table alignment](#jump-table-alignment).
- **Header dependencies:** C objects depend on `include/*.h`. Without that dependency a broken
  header change did not trigger a rebuild, and the build falsely looked like it "still matches".
- **Makefile edits do not rebuild objects:** run `touch src/main/*.c` (or `touch src/*/*.c`) after
  changing the Makefile.
- **Line endings:** `dualshock/.gitattributes` forces **LF**. The `cpp` of gcc 2.6/2.7 fails with
  "stray '\'" on CRLF line endings in `include_asm.h`, which Git for Windows would otherwise
  produce on checkout.

### Which compiler: GCC 2.6.3

All Capcom code is built by **one compiler, GCC 2.6.3** (gcc-2.6.3-psx).
`dualshock/tools/compiler_matrix.py dualshock/tools/compiler_matrix_sample.c` compiles 15
hand-decompiled functions with every candidate compiler and compares them with the original
(the address fields of relocated words are ignored; `-v` prints the differing instructions):

| Compiler | Exact matches |
|---|---|
| **gcc 2.6.3-psx** | **15 / 15** |
| gcc 2.7.2-psx | 13 / 15 (wrong register choice for `mult`/`mflo` code: `$2/$8` instead of `$3/$2`) |
| gcc 2.8.1-psx | 4 / 15 (sizes differ on most functions) |

No `-O`/`-f` flag variation changes these results (tested: `-O1`, `-O3` and several `-fno-*`
flags). Code in the 0x80031xxx area can look like the work of a different compiler, but it is
not: the difference comes from the
[pointer adjusted in a separate statement](#pointer-adjusted-in-a-separate-statement) idiom.

Other compiler builds live in `tools/external/old-gcc-build/`. `refine.py` and `sidediff.py`
take an `RE_GCC` environment variable (default `2.6.3`) to try one of them; 2.7.2 and 2.8.1 were
tested this way and did not help with any known case. `try_c.sh` takes `GCCVER=2.6.3/2.7.2/2.8.1`.

### maspsx behaviour

- **`--aspsx-version=2.56`:** its `mult`/`mflo` `nop`-gap behaviour matches the original. For
  simple multiply arithmetic maspsx inserts the 2 `nop`s after `mflo` that Sony's assembler adds.
  The exact ASPSX version has not been pinned down further than that.
- **`la` expansion:** the original expands `la` as **`lui` + `ori`** (ASPSX behaviour), and maspsx
  reproduces this. A fixed address that the original reached through a symbol compiles to
  `lui/addiu`; the numeric form compiles to `lui/ori`.
- **Expanded divisions (`--expand-div`):** every division in the original program (main and
  overlays) is the assembler-expanded form:

  ```
  div   $zero,a,b
  bnez  b,1f
  break 7
  1: li  at,-1
  bne   b,at,2f
  lui   at,0x8000
  bne   a,at,2f
  nop
  break 6
  2: mflo
  ```

  (`divu` has only the zero check.) Without `--expand-div`, maspsx emits a bare `div`, and no C
  for a function with a division can match (such functions stay a few instructions off whatever
  the C). The flag is set in `dualshock/Makefile` (`MASPSXFLAGS`) and in
  `dualshock/tools/compiler_matrix.py` (both the subprocess and the in-process path).
- **Indexed accesses: symbol vs number.** maspsx expands a load/store with a *symbol* address
  (`lhu $3,D_X($5)`) as `lui at; addu $at,$at,$5`, but a *numeric* address greater than 32767
  (`lhu $3,-2146684878($5)`) as `lui at; addu $at,$5,$at` (`maspsx/__init__.py`, around lines
  1028/1060). The operand order of that `addu` therefore shows how Capcom wrote the access; see
  [Fixed RAM addresses](#fixed-ram-addresses-number-vs-symbol).

### splat configuration and gotchas

- **`ld_align_section_vram_end: False` and `ld_align_segment_vram_end: False` are required.**
  splat's default `ALIGN(., 16)` between sections moved `.text` from 0x80011A78 to 0x80011A80.
  That shifted all code by 8 bytes and broke every jump-table entry (the build differed from the
  original in 516,087 bytes).
- **`subalign 4`** in all 13 yamls: splat's linker script packs sections, so section alignment
  directives are ignored. Our `as` runs with `-no-pad-sections`, so an object whose `.rodata` ends
  in a 10-byte table would leave the next object 2 bytes off. With `SUBALIGN(4)` every object's
  section starts 4-aligned, as in Capcom's link.
- **`gp_value`:** the EXE headers say gp = 0, but each executable's startup code loads its own
  `_gp` (found by `create_config`). The main program uses `-G` small data relative to 0x800B235C.
  Each overlay has its own gp, and its linker script sets `_gp` to match.
- **Overlays and the global range:** `global_vram_start/end: 0x80010000-0x800B2800` declares the
  main program as the always-resident "global" range. Without it, splat prints "Unable to
  determine a segment" errors for the main-program symbols that overlay configs load.
- **Symbol file comments:** notes in `symbol_addrs_psyq.txt` must not contain `:`. splat parses
  `key:value` there as attributes, so the files write `LIB/OBJ` instead of `LIB:OBJ`.
- **Symbol file loading order** in overlay configs is psyq, then main Capcom, then the overlay's
  own file, so calls from an overlay into the main program get names.
- **One `.rodata` and one `c` subsegment per source file**, at the original offsets (taken from
  splat's per-function `.s` offset comments).
- **`disassemble_all: True` and `matchings_path: nonmatchings`** in all 13 yamls, so `make split`
  regenerates every function's `.s` file (the diff tools need them even though nothing is
  `INCLUDE_ASM`).
- **Main program subsegments** named `rodata`, `capcom`, `psyq` and `data` give files like
  `asm/main/capcom.s` (about 91k lines) and `asm/main/psyq.s` (about 54k lines). (This describes
  the single-file layout; see the note in
  [Original source file boundaries](#original-source-file-boundaries).)
- **Rodata split suggestions:** splat prints 19 rodata split suggestions for the main program
  (jump-table alignment): 0x86C, 0x938, 0x974, 0xAD8, 0xBDC, 0xBF8, 0xCF4, 0xD20, 0xEDC, 0x1070,
  0x10EC, 0x1100, 0x1114, 0x1140, 0x1174, 0x1730, 0x17DC, 0x1FA8, 0x221C. They hint at where
  Capcom's original `.c` files begin and end (see
  [Original source file boundaries](#original-source-file-boundaries)).
- **Known harmless warning:** `asm/main/psyq.s: Warning: missing .end`, caused by a Sony function
  without a `.end` marker.
- **splat never overwrites an existing `.c` file.** To regenerate one, delete it first, and only
  if it holds no decompiled work.
- **splat never removes an old `asm/.../NAME.s` after a rename.** `tools/rename.py` deletes it;
  stale files make `tools/twins.py` see renamed functions twice. After a big rename run:
  `rm -rf asm && make split`.
- **splat does not write rodata that it attributes to a function that is already C**, and
  declaring such data as `type:data` in `symbol_addrs.txt` does not help. Rodata that was only
  ever cut out of a function's `.s` file therefore disappears in a fresh checkout unless it is
  generated another way.
- **Jump tables in function files:** in 65 function `.s` files the migrated jump table
  (`.section .rodata`) comes *before* the `nonmatching name, 0xSIZE` line, so tools must search
  the whole file for it.
- **Data label quirk:** in stage3 the hunter state table's slot 8 is split across two labels
  (`D_80132020` / `D_80132023`); the byte table `hunter_behavior_seed_tbl` (0x80132023, `u8[8]`)
  is unaligned and its `[0]` overlaps the last byte of the preceding pointer.

### Main executable layout

| Range (VRAM) | File offset | splat subsegment | Content |
|---|---|---|---|
| 0x80010000-0x8001098C | 0x800 | `.rodata, capcom` | Capcom rodata: jump tables, debug strings like `"%d Atari!\n"`. Owned by the C code; jump tables migrate into their functions' `.s` files, and shared items became `INCLUDE_RODATA` during matching |
| 0x8001098C-0x80011ADC | 0x118C | `rodata, psyq_rodata` | Sony rodata. It starts with printf's `"0123456789ABCDEF"` (referenced from `psyq.s`); its jump tables point into Sony code |
| 0x80011ADC-0x80061880 | 0x22DC | `c, capcom` | **Capcom code: 962 functions, 327,076 bytes**. The first function is `func_80011ADC` (splat's 0x80011A78 estimate was still Sony rodata) |
| 0x80061880- | 0x52080 | `asm, psyq` | Sony code |

### Overlay layout

Measured by `dualshock/tools/gen_overlay_configs.py`, each overlay is laid out as:

1. a leading `.rodata` (jump tables, owned by the C file) up to the first function prologue;
2. the `c` segment (Capcom code);
3. Sony's `NONE2.OBJ` startup stub (at entry point - 8), as `asm`;
4. data.

### Symbols and linking

- `config/symbol_addrs.txt` (main program) is listed in every overlay yaml too, so a main-program
  name reaches the overlays.
- Data symbols in `symbol_addrs.txt` have **no** `type:` (splat only accepts
  `func`/`jtbl`/`jtbl_label`/`label`/`alabel`).
- splat only defines symbols a program references. A second linker name for an address that a
  program's original code never used fails to link there. Examples: `D_800C5124` (the player
  entity) is defined in `config/symbol_addrs.txt` for every program because the ending overlay
  never referenced it; `g_PlayerWork` is a macro over `g_PlayerEntity` (+0x15C) instead of a
  symbol at 0x800C5280. A C name for an existing linker symbol is written as
  `extern Entity g_PlayerEntity __asm__("D_800C5124");` (no splat or linker change).

### Original source file boundaries

Capcom built the main program and each overlay from several source files. The compiler does not
care how the C is split (one compiler builds everything), but file boundaries matter for
`static` functions, for which prototypes each function sees, for rodata alignment, and for
readability. The known signals for where Capcom's files began:

- splat finds **no padding** between functions, so the original objects are packed.
- The rodata split hints map to increasing code ranges (`dualshock/tools/file_boundaries.py`).
  The 15 hints give about 12 file groups that have rodata. Three hints fall inside a single file
  (overlapping ranges R00/R01, R04/R05, R06/R07).
- 4-aligned jump tables in the main program: 0x800106DC..0x80010804 are packed 4-aligned (see
  [Jump-table alignment](#jump-table-alignment)).
- The zero pad words of overlay jump tables are visible markers of Capcom's original source-file
  boundaries.

> **Note:** the main program was first decompiled as one file (`src/main/capcom.c`) and later split
> into 45 topic files (`player.c`, `status_menu.c`, `door.c`, `effect.c`, ...) without changing a byte;
> each file is one yaml subsegment. Some matching-era tools still assume the single file (e.g.
> `compiler_matrix.target_info` names `capcom.c`); see [The draft and search tools](#the-draft-and-search-tools).

### Splitting files

Splitting a source file at a function boundary keeps the link order and, with subalign 4, the
byte match. `dualshock/tools/splitfile.py` does it (see [code_notes.md](code_notes.md) for usage).
Each part must keep seeing exactly what it saw before: functions defined later stay unseen, so
implicit declarations behave as before (see
[Declarations, types and casts](#declarations-types-and-casts)).

- Each C file's `.rodata` is a separate input section. `.align 3` jump tables are padded relative
  to the **file** start, so a file that contains an 8-aligned table must start its rodata at an
  8-aligned address. Files without 8-aligned rodata can start anywhere; 4-aligned tables marked
  `#JTALIGN2` don't count.
- Overlay jump tables are 4-aligned (the `alignfix` step), so any file boundary works in an
  overlay.
- The original alignment gaps between files are reproduced with an explicit `.space N` at the end
  of the earlier file's rodata (`tools/splitc.py --pad` computes N from the build map; one round is
  enough).

---

## Verifying a change

Every change must keep all 13 executables matching: the build ends with "All 13 executables
match". After any edit, and after any tool that integrated code, run a plain `make` once and
confirm that line.

### Stale objects

`make` decides by timestamps, and a C file rewritten within the file system's timestamp
resolution (the same second as the last object) can look unchanged. A stale object then gets
linked and a broken file passes as "matches" (the OLD build matches).

- Any tool that builds after editing a C file must delete `build/src/<file>.{i,s,ms.s,o}` first
  (`integrate.py`'s `build()` and `jtinsert.py`'s `fresh_make` do; per-function trial tools should
  delete the touched objects).
- After any doubt: `touch src/*/*.c` and `make`.
- After a failed integrate, delete the target's `build/src` objects before a plain `make`.
- After Makefile edits: `touch src/main/*.c`.

### Clean builds and fresh checkouts

- **Clean build.** All readability tools end with a full split + clean rebuild that must match all
  13 executables; otherwise they restore everything. Because the main program is `-O2` and the
  overlays `-O0`, a spelling that matches in one is not automatically valid in the other, so every
  readability tool verifies each program that compiles a file.
- **Fresh checkout test.** Copy `dualshock/` without `build/` and `asm/`, run `make split` and
  `make`. `tools/workcopy.py` automates private copies (see [code_notes.md](code_notes.md)).

### Finding what changed

- **`funcdiff.py`** without arguments prints the first *shifted* function (the one before it
  changed size) plus the differing functions; with names it shows side-by-side disassembly,
  original vs built.
- **Size changes.** When a function's size changes, everything after it moves and a byte compare
  flags all of it. `declclean.bad_functions` therefore reports only the functions whose **size**
  changed when any did (sizes from splat's `nonmatching NAME, 0xSIZE`).
- **Shift without a changed function.** If integrating a function shifts later code, check
  `build/slus_007.47.map` section sizes (`.text` of the object): a +4 shift with an unchanged
  function means a caller changed (prototype/return type), not the function itself.
- **`intdiag.py <overlay> <func>`** inserts one draft, builds, and lists the differing address
  ranges of the whole executable (always restores the C file).
- **Parallel build errors.** Compiler messages of parallel jobs interleave, so parsing `file.c:N:`
  lines misattributes failures. Tools read make's own `*** [build/src/stageN/x.o] Error` lines
  instead.
- **Compile-error fixups.** When a rename hits a field name that belongs to another struct,
  compile and revert exactly the lines with "structure has no member" errors.
- **Byte checks can't catch wrong names.** A readable rewrite can match and still be semantically
  wrong (two fields at the same offset of different structs, e.g. `ENT(..)->id` vs
  `JOINT(..)->index`). Check the meaning, not only the bytes.
- **The diff tools do not compare everything.** `sidediff.py` does NOT compare jump-table contents
  (rodata), and the refine distance ignores `#JTALIGN2`; see
  [Diagnosis tools](#diagnosis-tools). Only the full build is authoritative.

### hunkcheck.py: keep only edits that leave the assembly unchanged

```
python tools/hunkcheck.py [--base DIR] [--snapshot] [--dry] [files...]
```

Compares the edited sources with the last accepted copy, rebuilds the affected objects and
compares their assembly per function with a baseline copy of `build/**/*.ms.s` (make it with
`--snapshot` after a clean build of the accepted sources). Every diff hunk inside a function whose
assembly changed is put back; when a change cannot be attributed to a function, that file's
file-scope hunks are put back. It repeats until the assembly matches, then also puts back the
edits of any function that now has more warnings than before. It keeps its accepted copy, baseline
assembly and per-function warning counts under `temp/`.

### formtry.py: rule-driven per-site trials

```
python tools/formtry.py RULES.py src/main/effect.c [more files]
python tools/formtry.py --whole RULES.py src/main/effect.c    # per function only, no per-site retries
```

`RULES.py` defines `RULES = [(regex, replacement), ...]` (applied inside function bodies only; a
callable replacement returns the new text or None), optionally `SKIP_LINE` and `FUNCS`. Round 1
rewrites all sites of every function; a function that fails or changes bytes then tries its sites
one at a time, keeping each that compiles to the same bytes.

### aliastry.py: remove per-function aliases where possible

```
python tools/aliastry.py src/main/door.c [more files]
python tools/aliastry.py --data-only src/main/door.c
```

Same scheme as formtry: remove every candidate alias per function, build; a function that fails
tries its aliases one at a time (all functions advance in parallel, one build per round); a final
check gives the old text back to functions that still differ (removals can interact across
functions). Candidates are data aliases of globals with a file-scope declaration and aliases of
functions defined in OTHER files and not prototyped at file scope. The older `funcaliasclean.py` /
`declclean.py` only replace exact-type aliases and assume one C file per program (they still work
per file with `--target main/door`).

### Pitfalls of per-function trials

- `make` can miss an edit saved in the same second as the last build: per-function trials should
  delete the touched objects.
- `formtry.py` can drop good results when another function's size changes: run it with one
  function at a time.
- A data piece can fail alone only because the next one relies on its padding; see
  [Rules that must hold](#rules-that-must-hold-for-data).

### Regression tests

- **Compiler choice:** `compiler_matrix_sample.c` holds the 15 test functions; all match on 2.6.3.
- **integrate.py:** `tools/test_integrate.py <target> <funcs...>` puts finished functions back to
  `INCLUDE_ASM`, re-integrates them, checks the build, and always restores. Avoid test functions
  preceded by `#JTALIGN2` or `INCLUDE_RODATA` lines. See [integrate.py](#integratepy).

---

## How GCC 2.6.3 decides the output

Most mismatches have a cause that can be read in the GCC 2.6.3 sources. This section collects how
the individual passes behave; the per-level rules that follow from it are in
[Rules for the overlays (-O0)](#rules-for-the-overlays--o0) and
[Rules for the main program (-O2)](#rules-for-the-main-program--o2).

### -O0 and -O2

The main executable is compiled at `-O2`, the overlays at `-O0`. Many rules differ between the two
levels, so a readable spelling that matches in an overlay is not automatically valid in the main
program (and vice versa).

- **Evidence for -O0:** every overlay function keeps a frame pointer (`move $fp,$sp`). That is GCC
  2.6.3 at **`-O0`**: it matches where `-O2`, `-O1` and `-O2 -fno-omit-frame-pointer` do not.
- **-O0 register allocation:** at -O0 GCC 2.6.3 does NOT run local-alloc: `toplev.c` runs
  `stupid_life_analysis` (`stupid.c`) then reload. `stupid.c` sorts all pseudos function-wide by
  life length, longest first (ties: fewer refs, then lower regno); each takes the lowest free hard
  register. Life is counted in suids and NOTES get suids too, so a `({ })` (DELETED + BLOCK_BEG +
  BLOCK_END notes) lengthens every pseudo live across it: adding or moving one changes allocation
  order without changing code. Every pseudo lives >= 2 suids (stage4 func_80125EAC).

  > **Note:** in GCC 2.6.3 `local_alloc` does not run at -O0: `toplev.c` sets
  > `obey_regdecls = (optimize == 0)` and calls `local_alloc ()` only `if (!obey_regdecls)`, so the
  > `stupid.c` allocation above is the one that applies to the overlays.

- -O0 code normally never uses s0-s7; when it does, see
  [Callee-saved registers in -O0 code](#callee-saved-registers-in--o0-code).
- At -O0 every spelling of a player field (`g_PlayerEntity.f`, a cast, the separate symbol) gives
  the same code; at -O2 they differ (see
  [Struct fields vs separate globals at -O2](#struct-fields-vs-separate-globals-at--o2)).

### Reading the compiler

- **GCC sources:** GCC 2.6.3 is in `tools/external/_downloads/gcc-2.6.3.tar.gz`. Reading
  `fold-const.c`, `expr.c`, `c-typeck.c`, `mips.c`, `mips.md`, `stmt.c`, `cse.c`, `loop.c`,
  `jump.c`, `reorg.c`, `sched.c`, `local-alloc.c`, `global-alloc.c` and `stupid.c` explains most
  mismatches.
- **Fast experiments:** run a tiny C file through `cpp.exe` + `cc1.exe` directly instead of the
  whole build, or use `try_c.sh <file.c> [flags]`, which compiles a snippet the way the Makefile
  does and prints the assembly.
- **RTL dumps:** run `cc1.exe ... -dd -dj -dJ -df` on the function's preprocessed `.i` file.
- **Allocation dumps:** `-dl` dumps print "Register N used X times across Y insns", which is what
  you need to compute allocation priorities (see [Register allocation](#register-allocation)).
  The `.lreg` / `.greg` dumps show local/global allocation; the `;; Register N in R` lines in
  `.lreg` show local-alloc's choices.
- **Scheduler:** `cc1 -dS` shows the scheduler's ready lists (useful for sched1 tie-breaks).
- **Delay slots:** for delay-slot mismatches, an instrumented copy of `reorg.c` (stderr prints
  added, linked into a separate cc1) that prints, per branch, the predicted direction, tried
  threads, live registers, candidates and rejection reasons beats guessing. Build and run it under
  MSYS2 bash with `/usr/bin` on `PATH`.
- **Crashes:** the pass dumps `-drjcsLftlgRdS` locate a crashing pass (the last, empty dump); see
  [Compiler bugs and crashes](#compiler-bugs-and-crashes).

### The parser: `emit_nop()` at labels and loops

The C parser calls `emit_nop()` before a user label (`name:`) and at every `while`/`for`/`do`. At
-O0 it emits a real `nop` when the previous insn is a CODE_LABEL. This is the source of "lone
`nop`s" in overlay code; see [Branch layout and labels](#branch-layout-and-labels).

### Front end and `fold()`

GCC 2.6.3's `fold` rewrites some expressions so that m2c shows a different shape. Writing m2c's
shape back never matches; write the shape Capcom wrote. Seen in -O0 overlay code:

| m2c shows | Capcom wrote | Why | Tool |
|---|---|---|---|
| `t_s0 = X; if (c) v = t_s0 + 4; else v = t_s0; X = v;` (temps in s0/s1/s2 in -O0 code) | `X += (c) ? 4 : 0;` | fold turns `A + (c ? K1 : K2)` into `c ? A + K1 : A + K2`; A becomes a SAVE_EXPR, which lives in a callee-saved register across the call in `c` | `condadd.py` |
| same with a computed value instead of 4 | `X += (r() & 0xFFF) * !(flag & 1);` | `A * !c` is folded the same way | `condadd.py` |
| `if (c) v = (0x400 - t) & M; else v = (0xC00 - t) & M;` | `X = ((c ? 0x400 : 0xC00) - A) & M;` | `(c ? K1 : K2) - A` folded into both arms | `foldarms.py` |
| `A + (B + K)` | `A + K + B` | fold moves the constant inward | `constmove.py` |
| `A - (B - K)` | `A + K - B` | same | `constmove.py` |
| `(a - b) + (((s - 1) << 8) + 0x600)` | `a - b + 0x600 + (s - 1) * 0x100` | same, plus `* 0x100` (a `move`) | `constmove.py --mul` |

Seen in -O2 main-program code:

- fold's single-bit test turns `(x & 0x20) != 0` into `(x >> 5) & 1` (`srl; andi 1`). The original
  keeps `andi; sltu`: write `(x & ({ 0x20; })) != 0`, or split it over two statements:
  `t = f & 8; func(t != 0);` (main func_80044FC0). Passing it to a `u8` parameter also gives the
  shift form; call through an `s32` alias when the callee was defined with `u8`.
- `-(b < 1) & 0x7FFF` is folded back into a conditional (branches); `-({ b < 1; }) & 0x7FFF`
  keeps `slti; negu; andi` (func_80048FC0).
- GCC's folder turns `(cmp) * x` into a select; `({ (f & ({ 2; })) != 0; }) * x` keeps
  `andi; sltu; mult`.
- `X * (c ? K1 : K2)` folds to a SAVE_EXPR of X evaluated before the test, with two mult arms (the
  flag once, but gtu temp + copy = two registers). A narrower flag variable
  (`s16 b = ({...}) != 0`) makes it a SUBREG copy so CSE keeps ONE pseudo for both arms
  (func_80043F48).
- `t = (b != 0)` expands as `tmp = gtu b; t = tmp` (flag in two registers across arms).
  `t = ({ (x & ({2;})) != 0; });` makes temp + copy (two registers across arms);
  `s16 v = ({ c ? A : B; })` keeps the arms SImode with a copy at the join.
- `x1A = -({ x2A + x48; })`: without the statement expression GCC narrows the loads to `lhu`
  (func_8001DFE0, func_8001D450).
- A ternary abs `(x < 0) ? -x : x` is one schedulable `abssi2` insn (different allocation than
  `if (x < 0) x = -x;`) (func_80052FAC). `__builtin_abs(x)` gives the `bgez x; nop; negu x,x`
  sequence in the register the original uses, where an explicit `if (d < 0) d = -d;` picked
  another register (func_8003A6F0).
- `&D_800B9D38[12]` compiles to `D_800B9D38+0x600` where the symbol at that address would be used
  otherwise (func_8001B7DC). With `s16 D[2]`, `p = D; p[1]` folds to `%lo(D+2)`.
- `arr[0]`, `*arr` and `*(T *)arr` fold back to the element form; `*(T *)(void *)arr = x` is not
  treated as an array-member access and keeps its order against other global stores.
- `pointer_int_sum()` and `expand_expr(EXPAND_SUM)` fold a `- 1` in an index into the symbol
  offset; see `idxwrap.py` in [Rules for the main program](#rules-for-the-main-program--o2).
- **Narrowing of read-modify-write:** `convert_to_integer()` narrows a whole subtraction to the
  destination's char type; see [u8 read-modify-write narrowing](#negation-multiplication-shifts-division).

### Statement expressions stop `fold()`

When GCC re-associates or folds something the original kept apart and no cast works, wrap part of
the expression in a GNU statement expression `({ ...; })`. `fold()` cannot look into it and it
costs no code. Capcom most likely used macros built on `({ ... })`.

- **Wrapping an inner sub-expression** (-O0): the func_8010F7FC family (stage1 func_8010F6DC,
  stage2 func_8010F768, stage4 func_8010F9AC, stage5 func_8010F7E4, stage6 func_8010F824, stage7
  func_8010F7FC) computes `v - v * (bit * 2)` as `andi; move; sll 1; mult`. The `move` is GCC's
  `synth_mult` copying the operand for `* 2`, so the source had a real multiply by 2, but `fold()`
  normally re-associates `v * (bit * 2)` into `(v * 2) * bit`. Same-mode casts are stripped by
  `split_tree()`, narrower casts add an `andi`, unary `+` is stripped, and a `register` temporary
  lands in `s1`. What matches:

  ```c
  sp20.vx - sp20.vx * ({ (u8) ((u8) (M2C_FIELD(arg0, u8 *, 3) & 0x40) >> 6) * 2; })
  ```

  `stmtexpr.py` rewrites m2c's form. The same holds at -O2: fold re-associates `a * (b * 2)` to
  `(a * 2) * b`, and `({ b * 2; })` keeps the operand order.
- **Wrapping just a constant** (`x + (y - CONST)`, `x * (y * CONST)`; stage4 func_8011A570,
  func_80119E00, stage3 func_80112394): `fold()`'s `split_tree` only splits when an operand is
  TREE_CONSTANT, and a mode-changing cast would add a sign extension. Wrap the constant:
  `({ -0x400; })`, `({ 0x64; })`. fold no longer sees a constant, but at expand time it is still a
  CONST_INT, so you keep `addiu -1024` and the shift/add multiply. Unlike wrapping the whole
  sub-expression, this keeps `rand()` calls visible to `preexpand_calls` (call order unchanged).
  Operand order matters for register ties: `tbl + (({ -0x400; }) + (rand() & 0x7FF))` matched,
  `- ({ 0x400; })` did not.
- **What `({ })` does to memory:** `({ field; })` returns the MEM unloaded (address computed
  inside, load outside).
- **Calls:** calls in an assignment to a non-variable lhs are pre-expanded (`preexpand_calls`),
  except inside `({ })`. `(t = expr, t * t)` in a call argument isn't reordered after nested calls
  by `preexpand_calls`; assign the inner call result to its own variable.
- **Byte post-decrement tests:** see [Increments and decrements](#increments-and-decrements).
- **At -O0 a `({ })` changes register allocation** through its notes (see
  [-O0 and -O2](#-o0-and--o2)).

### Operand order of additions

- GCC canonicalises `ptr + n` with the pointer first. For `addu a1,v0(int),a1(ptr)` (int operand
  first) write the sum in integers: `(void *)(n + (s32)ptr)`, or `idx * 12 + (s32)base`
  (func_8005ACF4, func_8002CF44).
- Commutative PLUS operand order is decided at expand time: `data = x + data` and `data += x`
  always give `addu data,data,x` (`expand_binop` swaps when the target is the second operand).
  Copy the base first: `t = data; data = (u8 *)((*(u16 *)t & 0xFFFC) + (u32)t);` gives
  `addu data,x,data` (func_800313E8).
- In an address context `expand_expr(EXPAND_SUM)` puts the MULT term first
  (`addu v0,scaled,base`); `r = base; r += i;` gives base first. Base-pointer-first:
  `*(u16 *)((u8 *)base + ({ idx * 2; }))` (func_800360A8). Base-first address sums also come from a
  real array MEMBER `p->arr[i].f` (func_80037CEC).
- **Split a pointer sum to choose its register:** `q = tbl[x].p; q += y * 8;` makes the sum reuse
  the loaded pointer's register (`lw a0; addu a0,a0,v1`); `q = tbl[x].p + y * 8` puts the sum in
  the index register (main func_8001B180, a `lw` from a 128-byte-stride pointer table).
- An address `row * 128 + &table + col * 2` that the original keeps unfolded:
  `q = (u16 *)(({ (p[1] << 7) + table; }) + p[2] * 2);` (func_8001AEE0, func_8001B0AC).
- Literal base + byte offset `(u8 *)0x801FEC2C + off` gave the original `addu` operand order where
  the extern symbol did not (func_800375B8); `s32 base = 0x801FEC24; arg0 = (arg0 << 7) + base;`
  (func_8003767C).
- **Operand order** (`A op B` vs `B op A`) in general changes register choice at `-O2` (refine
  mutations `commute` and `commute2`).

### Memory references: which loads may pass which stores

- **MEM_IN_STRUCT_P:** GCC 2.6.3 `expr.c` marks an indirect reference "in struct" when its address
  is a `PLUS_EXPR` (`p[1]`, `*(p + 1)`, `M2C_FIELD`), when it is an aggregate, or a struct member.
  The scheduler then lets an in-struct load with a varying address pass a store to a fixed scalar
  (a global), and vice versa.
  - If the original keeps `lhu 2(a0)` *after* `sb D_X`, read through a plain `*q`
    (`q = p + 1; b = *q;`); CSE still folds it to `2(a0)` (main func_80048FC0). Conversely `*p++`
    reads give `lhu 0(v1); addiu v1,2` chains (func_800493CC).
  - `*(T *)(ptr + off)` is a NOP_EXPR around a PLUS, not in-struct; `M2C_SFIELD` is, and its load
    can pass fixed-address scratchpad stores (func_80052CE4, func_8005F790 with
    `M2C_SFIELD(arg0, s32 *, 0x5C)`).
  - Choosing which store a reload may pass (main func_80050D34): after `p[0] = 0` (a byte store,
    which may alias anything) the global pointer is reloaded. The original hoists that reload
    above the next store `p->x16 = 2`, which GCC only allows when that store is MEM_IN_STRUCT:
    `M2C_SFIELD(p, s16 *, 0x16) = 2` matched, `*(s16 *)(p + 0x16)` and `*q` did not.
  - **Array loads vs scalar stores:** `D_800B255A[i]` (ARRAY_REF, "in struct") lets GCC move a
    later scalar store past it; m2c's `*(u16 *)(&D_800B255A + t)` (u8 symbol + byte offset) keeps
    the original order (func_8004ED90).
  - The main program's draft `M2C_FIELD` is a plain cast (not a struct member), so its accesses
    never pass a fixed-address scalar access; see `sfield.py` in
    [Rules for the main program](#rules-for-the-main-program--o2).
- **Symbols vs literal addresses** (`memrefs_conflict_p`): GCC 2.6.3 treats two different SYMBOL
  addresses as possibly overlapping but compares two LITERAL addresses exactly: a global load can
  only move above a store to another global when both use literal addresses (func_80059C30).
  SYMBOL_REF vs CONST_INT addresses never conflict: a symbol load always passes literal
  scratchpad stores, and no alias trick adds the dependence.
- A BLKmode struct copy conflicts with every CONST_INT address but never with a SYMBOL_REF: a
  `*(T *)0x8...` literal load before a struct copy to a literal address can't sink below it
  (func_80044938: the fix was the literal load instead of the D_ symbol).
- Two `__asm__` aliases of the SAME global with different types are treated as non-aliasing: a
  load can move above a store to the same address. Use casts of one declaration when the order
  matters (func_800404B8).
- A load through a register address never passes a literal-address store in sched1: the
  original's load-vs-fixed-store order fixes the source statement order (stores among themselves
  move freely). Non-struct loads stay below non-struct scratchpad stores, while loads from a
  register holding a known constant address still move above them (func_8001E4D4).
- sched alias analysis resolves an address register only with a REG_EQUAL note and ONE flow set
  (or REG_EQUIV): a pointer variable assigned more than once stays unknown and conflicts with every
  symbol store. Its loads stay after an earlier-in-source store (true dependence) and before a
  later one (anti dependence) (func_8003BAF8). A load through such a multi-set pointer must come
  BEFORE (in source) a store through a constant-address register to stay above it. A separate
  single-assignment pointer per branch makes the address a known constant so its load can pass an
  unrelated global store (func_80058E4C). sched1 alias analysis runs before the lui/ori split (a
  scratchpad pointer is still a single set with REG_EQUAL).
- sched won't move a QImode load past a store to a fixed address (explicit QImode exception). To
  overlap a QImode load with a later fixed-address store, read it into a block-local temp before
  the store.
- Global pointer reload after a store through it: a `sb` through `D_800C512C` forces a reload of
  the pointer, an `sh` at a PLUS address did not (in-struct). Reading it through
  `void **pp = &D_800C512C;` with `M2C_FIELD(*pp, u16 *, 2) = ...` reloads the pointer like the
  original (func_8003C99C, found by the permuter).

### CSE

- GCC 2.6.3 CSE drops a cached global after any BYTE store through a pointer, or a store not
  addressed as reg+offset; halfword/word stores at reg+offset keep it (`note_mem_written` in
  `cse.c`).
- On a store CSE drops only plain MEM entries: `u16 c = <u8 global>` records `(zero_extend (mem))`,
  which survives, so later reads reuse c; use a u8/s32 c to get a reload.
- Reading a byte into an `s16 c` local for the compare stops CSE merging it with a later `% 18`
  read, with no volatile needed (func_80035EB4). `arg % 2` instead of `arg & 1` keeps CSE from
  reusing the load for a signed `/2`.
- CSE's canonical register for a copy `y = base` is the one living beyond the block with the later
  last use (`make_regs_eqv`), so `y = base; if (c) y = base + X;` becomes `y + X`; write
  `y = (s16)base;` to break it. A copy `Q = cur` survives CSE only if Q becomes the canonical
  register (copy inside the arm, Q's last use later; reuse a later variable).
- CSE skip-blocks follows a branch around a block only if the target label has exactly 1 use, no
  label is inside the skipped block, and there are < 9 branches; equivalences survive. It merges a
  copy `q = t` placed before such a branch; writing `q = t;` in BOTH arms ends the CSE path at the
  join (post-reload cross-jumping + reorg leave one copy in the delay slot) (func_8003DB1C). A new
  CSE path at a join redoes every global load CSE'd across it; hold such globals in a local
  pointer.
- An unused C label is deleted by jump1 before CSE (it can't split CSE blocks).
- cse2 won't replace a constant SET_SRC with an equal register when the destination is a user
  variable (`la sN` directly). `r = t;` with an s32 `r` is merged by CSE; a u8 copy target keeps
  the move. CSE drops a u16 variable's first set if it is overwritten before a non-substitutable
  use.
- CSE folds `(u16)ang & 0xFFF` back to `ang` when it is already masked; an s16 copy (sign_extend)
  stays separate from zero_extend.
- A store followed by an immediate reload of an address-taken local (`sb; lbu` same slot) means
  CSE didn't record the store: `*(volatile u8 *)&cnt = ...` reproduces it (last resort, see
  [FAKEMATCH](#fakematch)).

### combine

- combine drops `andi 0xff` before `srl` on a u8 only if every set has clean upper bits: an `andi`
  in a loop means the variable is reused elsewhere for arithmetic (Capcom reused variables across
  cases).
- combine drops a u16 `andi` feeding a compare when the HImode register's nonzero bits over ALL
  sets fit: a surviving `andi` means a set with unknown bits, or the value is live at entry.
- `record_value_for_reg`: re-setting a pseudo after a recorded value mentioned it invalidates that
  value (`y = (subreg:HI ang)` becomes unknown), so a u16 `andi` before a compare survives; reuse
  the variable earlier in the block.
- u16/s16 locals stay HImode pseudos (not promoted): each relational use gets its own
  zero_extend/`andi 0xffff` unless combine knows the bits. A u16 local from a load is combined into
  the use's zero_extend (the load moves down); `s32 s = ((u16 *)X)[i];` keeps it at the statement.
- `y = expr; f(y)` lets combine drop a u16 argument's zero-extend; `f(.., y = expr, ..)` keeps the
  `andi`.

### Scheduling (sched1 before reload, sched2 after)

- **Statement order decides -O2 scheduling.** Statement order of two adjacent statements changes
  scheduling (refine mutation `swap`). Examples: func_8004ED90 (the three byte stores had to follow
  both u16 copies), func_8005ED40 (`v.vy = 0` after `v.vz`). Two stores of the same constant: the
  constant is loaded where its FIRST use is in the source, then the stores are scheduled
  (func_80050B84/80050DD4: `0x20 = 0x28` had to come early, `0x26 = 0x28` late).
- **Birthing boost** (`adjust_priority`/`birthing_insn_p`): before reload, an insn whose destination
  is a REG that flow counts as set exactly once (`reg_n_sets == 1`) gets priority 0x7f000001 and
  lands just before its first consumer; only multi-set pseudos keep their source position.
  Assigning the variable a second time (`o = i; o <<= 2; ... o += base;`) or a narrow type (SUBREG
  destination) removes the boost (func_80022238, func_8004A034). `(u16)(x - y)` becomes a subreg
  set (no boost); use `u32 t = a - b; (t & 0xFFFF)`. `c &= 1` on a u8 `c` is not a second set (CSE
  makes a new temp). An insn made ready by scheduling its consumer gets priority 0x7f000001 and
  goes next.
- `schedule_select` picks, among equal-priority ready insns, the one with the largest potential
  hazard (a load). Scheduling runs backward, so a global load lands AFTER unrelated stores. Ties:
  the larger luid is picked first backward (= placed later). sched1 never places a load directly
  before its consumer while a 1-cycle producer is ready.
- With equal sched1 priorities, RTL creation order of constant-address registers decides their
  set-up order: reading a literal-address global earlier moves its lui/ori ahead without moving
  the load itself. A literal created in RTL before `p = K` gets its lui/ori first and the lower
  register. Source order of two independent loads via CSE'd literal-address registers decides a0
  vs v1.
- sched1 may move a set of a call-crossing pseudo (counter init, hoisted constant) across other
  calls: write inits where they naturally belong; pinning them with `do {} while (0)` makes
  call-argument set-up land late. A statement independent of a call: its position before/after
  the call decides sched1 order around argument set-up. Moving inits after a call moves them
  relative to an address load. `D[0] -= (k = expr);` moves the address set-up earlier.
- The temp's type changes sched1 placement (u16 temp for `lhu` vs u32 + zero_extend).
- sched2 has no boost: priority = longest path from block start (loads +1, anti/output
  dependences on hard registers +0); priority-1 loads float to the top unless their register is
  reused. Ties: stores first, then loads, then ALU. A load is never scheduled immediately before a
  store (memory-unit blockage).
- A load the original keeps after a compare (right before the branch) can come from REUSING a
  variable; the anti-dependence pins the load late:
  `c = (u32)w < (u32)dx; dx = *(s16 *)(arg0 + 0x8E); if (c) return 0;` (func_8003A890).
- Direct field copies (`dst = src;`, no temps) plus statement-order hill-climbing beat
  temp-variable drafts (func_800514E8).
- Struct-form field access changes -O2 scheduling in the main program (e.g. func_80038F7C); see
  [The m2c field macros](#the-m2c-field-macros).

### Register allocation

These rules are for -O2; -O0 uses `stupid.c` (see [-O0 and -O2](#-o0-and--o2)).

- **Variable reuse decides register sharing:** when one register holds first a loaded value and
  later a computed one (`lhu a3,0(v0)` ... `or a3,v0,a1`), the source reused the same local
  (`a = p[0]; ... a = (q->x16 & 0xFFC0) | (b & 0xFF);`); separate locals get separate registers
  (main func_8004A208, func_8001B358). Two locals sharing a register over disjoint ranges are
  usually ONE C variable; one variable reused across two separate regions becomes one long-lived
  register (func_8005E710, func_8002E58C).
- A parameter that ends up in another argument register (`move a1,a2`) can mean the source reused
  a dead parameter variable (`arg1 = arg2;`, func_80030B10).
- **Priorities:** local- and global-alloc priority is `floor_log2(refs) * refs / live_length`
  (local-alloc: `floor_log2(refs) * refs * size / (death - birth)`), with refs weighted by loop
  depth. global-alloc breaks ties by the lower pseudo number (declaration order of locals: a
  pointer local declared first flips a register, func_80053D98); local-alloc by quantity number
  (creation order in the block).
- flow weights refs: 1 outside loops, 2 inside a real loop (a goto loop counts once), +1 per
  `do {} while (0)` level. So:
  - a `do {} while (0)` around a single use deepens its loop nesting and raises that register's
    weight (func_8003A330); nested `do { do { x = ..; } while (0); } while (0)` raises one pseudo's
    priority with no code change; nested `do {} while (0)` around a constant set makes it win
    local-alloc (v0) even when set before the other compare operand;
  - moving neighbouring uses into an existing barrier raises allocation priority;
  - to raise a group's priority without moving code, wrap its last uses (stores) in nested
    `do {} while (0)` levels (+1 weight per level per ref); the schedule is unchanged if all moved
    insns share one wrap (func_8002EE08);
  - converting a goto loop to a real loop (or back) can flip a global-alloc priority.
- A global-alloc tie can be won by ADDING refs: a statement duplicated in both branches counts
  twice at allocation time and is cross-jumped afterwards (func_80035048). Arm-duplicate trick:
  `if (c) az = v; else { ...; do { az = v; } while (0); }` (short arm first) adds refs; the copies
  merge later (func_8002DD14). A duplicated `b++` (continue path + end of body) raises a pointer's
  allocation weight the same way.
- A barely lost priority can be won by moving one assignment one insn later (live length -1).
  Swapping two independent sets in one if-arm changes live length by 1 and can flip a near-tie
  (func_800560E8 went from distance 55 to 0). Copying a parameter early (`len = size;`, used in the
  expression) lowers the parameter pseudo's ref count and flips local-alloc order.
- `local_alloc` treats a pseudo as local only if `reg_basic_block >= 0 && reg_n_deaths == 1`: a
  variable reused for two short values dies twice and goes to global-alloc, whose other life adds
  hard-register conflicts (moves it off v0; MIPS has no REG_ALLOC_ORDER, the next free is v1).
  Loop-body temps that stay inside one basic block are LOCAL-alloc'd even when they cross calls
  (they can get s-registers).
- local-alloc ties the dying input of each insn to its output (srl -> sll -> add in one register):
  the group's refs are summed and its life runs from first birth to last death; free registers are
  handed out lowest number first (s0 first) in priority order. Break the chain by making one end a
  variable set in more than one block.
- A hard-register input dying in an insn (call result in v0) becomes `qty_phys_sugg` for the
  output's local quantity: the output gets v0 whenever free. Delaying the copy of a call result
  (`v = f(); ...; r = v;`) keeps v0 live longer and reshuffles the block's registers.
- local-alloc `optimize_reg_copy_1` rewrites later uses of src into dest after a REG-REG copy; a
  narrower dest type (s16/s8) is a SUBREG copy and stops it.
- movstrsi (struct copy) scratches are local-alloc quantities with priority 0: a LOCAL pseudo live
  across a struct copy gets v0 ahead of them, a GLOBAL one is allocated after; load into a local,
  then copy into the global variable.
- Global-alloc treats hard registers that local-alloc gave to block-locals as conflicts and avoids
  registers that conflicting pseudos prefer. A function-scope variable shared by two blocks is
  allocated with both blocks' conflicts. A function-scope temp shared by several switch cases
  becomes one long-lived pseudo (a0-a3); per-case block-local temps get v0/v1. One pointer reused
  across several loops is a global pseudo and loses hard registers to block-local constants; use
  one local per loop.
- A chained `g = D_x = t;` with a block-local temp lets local-alloc give `t` a low register; the
  global `g` then takes it via copy preference (the move disappears) (func_8003BAF8).
- Re-reading through a literal-address global instead of a local pointer lets hoisted address
  constants win low registers.
- Reload deletes the init copy of a spilled pseudo with a constant REG_EQUIV, but deletes the
  feeding insn only if it is the immediately preceding real insn: once sched separates them, a
  dead `addiu vX,...` stays in a delay slot (func_8002EE08).
- `s = tbl; pp = &s;` (store first) vs `pp = &s; *pp = tbl;` decides whether the address goes
  through v1 or straight into the saved register.

### Stack frame layout (-O2)

- A scalar whose address is taken gets its stack slot lazily (when `&x` is first expanded), after
  arrays; put the padding array in a later block to move it above (func_80014DF8).
- -O2 drops unused scalars, not arrays. An unused local array (`s32 unused[1];`) gives the 8-byte
  frame of a leaf function that has no saved registers (func_8004AB78, func_8004748C); larger
  unused arrays (`s32 unused[2]`, `[4]`, `[8]`, `[12]`) make the frame 8/16/32-48 bytes bigger
  (refine mutation `padlocal`; a leaf function with an `addiu sp,sp,-8` frame it never uses is the
  signature).
- A single-set `s16 x = <s16 mem>;` used sign-extended gives `lh T; move x,T` and reserves an
  unused 8-byte frame slot (`vars += 8` with no sp accesses hints at such locals); reusing one s16
  for two loads gives `lh` + `lhu`.
- A 1-D table symbol made a stale pseudo get an 8-byte stack slot (`vars=8`); declaring it 2-D
  `u16 tbl[][4]` fixed the frame (func_80055184).
- `p += 12; continue;` right after a call result test in a real loop added an unused 8-byte frame
  slot; use `goto next`.
- Header struct sizes can be shorter than the real strides (GsDOBJ5/GsCOORDINATE2 in
  func_8002FB30): use a local struct of the real size.

### Loop optimizer (`loop.c`)

- Only `while`/`for`/`do` loops carry loop notes; a `goto` back-edge does not. Loop-invariant
  constants (`lui; ori` hoisted into s-registers) therefore depend on the loop shape. `goto`-built
  loops keep `loop.c` from hoisting constants into s-registers (func_8002B8A4); a constant hoisted
  into an s-register across two loops means a real enclosing `do { } while` retry loop, not goto
  (func_80019BD0). Real do/while loops indexing `tbl[arg0 & 0xFF]` produced the original register
  layout where goto loops did not (func_8005362C; the opposite of func_80053858, so try both).
- An m2c `loop: { ... } goto loop;` has no loop notes, so every weighted ref count inside is
  halved and local-alloc ties come out differently. Try `goto start; for (;;) { ... }` first, even
  when a goto enters the middle of the body (func_8002A5C4 went from distance 30 to 7).
- **Hoist threshold:** `loop.c` hoists a movable when `threshold * savings * lifetime >= insn_count`.
  The threshold is `1 + non-fixed regs` when the loop has a call, and drops by 3 per moved value
  (it was exactly 26 in func_8002E650), so the order in which constants first appear in a loop
  decides which get hoisted. insn_count is counted BEFORE combine: a literal `*(u8 *)0x800Cxxxx`
  instead of a symbol alias (same asm) adds an insn and can stop a hoist. To keep a constant
  INSIDE a loop, add RTL insns that combine removes later: a u8 local copy
  (`k = D_x; f(k * 20 + C)`) adds a QImode set + zero_extend and pushes insn_count past the
  threshold (~26 with a call).
- A user variable set just before its call (lifetime ~2) is "not desirable" to hoist. A
  block-local user-variable constant (`s32 k = 4;`) set in a conditional block and used in another
  is not movable (`scan_loop` rule 2). A pointer variable assigned after a branch inside the loop
  is never hoisted. A pointer assigned at the loop top becomes a hoisted movable (threshold -3),
  which can stop later constants from being hoisted. A call argument `p + 6` is not movable but
  `q = p + 6` is.
- `combine_movables` merges equal constants across the loop and sums their savings/lifetimes; the
  first movable at least as wide absorbs later equal-valued ones (an SImode `1` at the loop top
  absorbs QImode `1` stores; a QImode movable absorbed by an SImode one leaves its absorbed stores
  as `li v0,1` in the loop). QImode constant movables move at the position of the SImode movable
  they merge into. Hoisted insns are emitted just before loop_start in movable-list order
  (func_8003A9AC).
- Loop invariants written as explicit locals before the loop, in the original's set-up order
  (`i = 4; tbl = D_800952B0; c = 0x30; p = &D_800BC873;`) and used instead of the literals
  (func_8003B504).
- Loop-invariant table addresses: a plain array `D[n]` in a loop gets its address hoisted into an
  s-register; a struct-member access `D[n].x0` (32-byte records) stays `lui; addu; lw %lo`
  (func_8005DE08, func_8004E00C). Indexing an array instead of walking a pointer lets `loop.c`
  hoist the array base instead of a constant (func_80039F0C).
- Induction variables:
  - `for (i = 0; i < *(u8 *)X; i++)` re-reads the bound (`loop.c` hoists it into a register copy,
    `move a3,a0`) and `D[i * 2]` gets strength-reduced after the guard (func_80047FA4).
  - `*(u16 *)(base + 2) += ...; base += 8;` gives `loop.c`'s `base + 2` giv (func_8005FB04).
  - `for (; n != 0; n--) { p[0] = p[2]; p[1] = p[3]; p += 2; }` gives two pointers (func_8002D56C).
  - Backward scan `n--; e = tbl[n];` (index form) lets `loop.c` build the pointer with the
    original's `&tbl[n] - 4` set-up; a hand-written pointer folds the -4 (func_8003A6F0).
  - A loop cursor into the middle of a struct comes from a pointer incremented separately
    (`i++, p += 7`) that `loop.c` reduces (func_8005C83C).
  - A pointer-compared loop `do { ...p[-1]...; p--; } while (base - 4 < p)` lets `loop.c` drop the
    counter (func_800514E8).
  - Giv reduction is suppressed when the biv is incremented in two places (`p++; continue;` plus
    the normal `p++`, func_800520E0). A `loop.c`-optimised loop strength-reduces `p+0x34` even with
    two increments; `if` + `goto` avoids it.
  - For-loop increment order matters for strength-reduced pointers: `p++, i++` vs `i++, p++` moves
    the counter `addiu` (main func_800156EC / func_80015C08). Init order `p = X, i = 0` likewise.

### Jump optimization and cross-jumping

- if/else whose two branches end in the same statement (a call, a store) with only one differing
  operand: GCC cross-jumps the common tail, so write the duplicate in both branches
  (`if (c) f(1, A); else f(1, B);`, main func_8002B31C, func_800208B8). Each branch storing to the
  same field (`obj->x64 = A` / `= NULL` / ...) cross-jumps into one store; a store written in both
  arms and cross-jumped explains a shared store at the join label ahead of the rest of the block.
- `jump.c` rewrites `if (c) x = a; else x = b;` as `x = b; if (c) x = a;`; choose the polarity for
  the delay slot. It also turns `ret = 1; if (a && b) ret = 0;` into `sltu`; write
  `if (!a || !b) ret = 1; else ret = 0;` for the branchy original (func_8005D160).
- Only the post-reload jump pass (jump2) cross-jumps, and one equal insn suffices
  (`x += k; goto L;` vs `x += k; L:`). jump2 sees only patterns: identical increments before a goto
  and before its label always merge unless a USE/CLOBBER/asm sits next to the duplicate (notes,
  labels, no-op moves and swapped operands don't block it). Put a blocker on the LABEL side so
  reorg can still fill the jump-side `j` delay slot. jump2 = `jump_optimize(insns, 1, 1, 0)` at
  every -O level; no `-f` flag disables it. jump2's jump-around inversion always turns
  `bnez L; j X; L:` into `beqz X`. An empty `__asm__("")` used as such a blocker is one of the
  two [FAKEMATCH](#fakematch) cases (func_8003E228).
- **Line layout can change the code** (main func_8005905C): the permuter's match has an
  always-true `if (D_800BC5BA)` whose two arms are the same call. Reformatted as `if (...) {` /
  `} else {` (calls on lines +1/+3) the build MISMATCHED (`move a1,zero` scheduled differently);
  the permuter's own layout (`if` / `{` / call / `}` / `else` / `{` / call / `}`, calls on lines
  +2/+6) matches, with any indentation. GCC 2.6.3 keeps line-number notes in the RTL even without
  `-g`, and cross-jumping of the identical calls sees them. When tidying permuter output, rebuild
  after every layout change of duplicated code.

### Delay slots (reorg)

- A forward branch whose target follows a loop's LOOP_END note is predicted NOT taken by reorg
  (delay slot filled from the fallthrough); a goto-built loop loses the note and fills from the
  target. This is a way to tell real loops from goto loops.
- reorg's forward fill of a conditional branch can take insns from the merge point after its
  target label (`i++` at the end of the if-block, func_800262AC). A `(use (insn))` marker from a
  backward fill re-adds a register as live in later liveness checks.
- A branch delay slot equal to the target's first insn = reorg filling from the target: source
  `if (c) y = ~y; a = y & 0x7FF;`.
- A statement in one branch can be hoisted by reorg into a delay slot so it looks duplicated /
  unconditional (func_80048378: `coord = obj + 0x20` belongs inside the branch).
- An `s32` return type with no return value keeps v0 live and reproduces an unfilled delay slot
  (func_80039240, func_80039BDC).

---

## Capcom's coding idioms and the C that reproduces them

### Fixed RAM addresses: number vs symbol

Capcom often reached big global structs and tables through numbers, with macros like

```c
#define WORK ((Work *)0x800C3000)
```

instead of declared variables (`*(u8 *)0x800C8660`, `((Effect *)0x800C3024)[i]`). The number form
gives different machine code than the symbol:

- maspsx emits `addu at,REG,at` for the number but `addu at,at,REG` for the symbol (index register
  order; see [maspsx behaviour](#maspsx-behaviour));
- a fixed address that the original reached through a symbol compiles to `lui/addiu`; the numeric
  form compiles to `lui/ori` (an `la` of a raw number, which splat does not symbolize);
- at -O2 the compiler CSEs the `lui` of a number differently, and the alias rules differ (see
  [Memory references](#memory-references-which-loads-may-pass-which-stores)).

Reading the original assembly:

- `lui at; addu at, <idx>, at; lw %lo(D)(at)` (index first) = a literal-address table
  `((T *)0x800C52A6)[i]` or `((u8 *)0x800C8660)[i]`; `addu at, at, <idx>` = a symbol such as
  `D_800C52A6[i]`. In the main program the literal form is typically a record array, e.g.
  `((Rec84 *)0x800C3000)[*(u8 *)0x800CF86E].x32 |= ...` (main func_8001F0AC). Plain byte
  arithmetic on the literal (`(u8 *)0x800C3032 + i * 0x84`) does not work at -O2: CSE then merges
  the address.
- A byte-offset base `(u8 *)&D_X` that Capcom wrote as a number shows the same `addu at,reg,at`
  order (refine mutation `sym2num`; mutation `symaddr` covers the `lui/addiu` vs `lui/ori` case).
- Both orders are common in Capcom code (loads: 462 symbol-style / 483 numeric; stores: 91 / 450),
  so each access must use the form the original used. To reproduce the numeric form, write the
  access as a cast literal: `D_X` becomes `(*(T *)0xX)`, or `((T *)0xX)` for arrays (refine
  mutation `fixaddr`; `fixaddr-fp` for function-pointer tables at a fixed address; `litbase.py`
  rewrites m2c's table-index shapes into literal-address or symbol tables).
- One function can use both: `lui t1,0x800c; ori t1,..` (CSE'd literal `*(u16 *)0x800CF84C`) for
  the early reads and `%hi/%lo(D_800CF84C)` for the last one (func_800175B4).
- Struct-field access on a record table gives index-first `lui at; addu at,idx,at`; pointer
  arithmetic on a literal keeps the constant in a register. Global struct-field / array accesses
  put the address in a register (CSE decides later); plain scalar globals stay direct `%hi/%lo`.

So the matching C keeps the numbers. They are named through the `LIT(...)` / `LITP(...)` /
`ADDR_` macros of `include/re1_addrs.h`, which the preprocessor turns back into the same tokens
(plus parentheses), so the naming cannot change the build (see "Fixed RAM addresses" in
[code_notes.md](code_notes.md)).

Not every 0x80xxxxxx number is an address: 0x80000000 / 0x80008000 are sign masks, and 0x800C0000
is an upper address half kept in a dead local for the match.

### A symbol vs base + offset

- `__asm__("D_80093BB0+2")` as a block-scope alias name gives a direct scalar access to an address
  with no symbol of its own (cc1 emits it verbatim, no extra directives).
- Where -O2 code addressed an array element as its own symbol, a function-local alias whose
  assembler name is `base+offset` reproduces that:

  ```c
  extern s32 g_PacketArea_1 __asm__("g_PacketArea+4");
  ```

  Examples: the map sprites (15 aliases in `map_display_animate`, 3 per sprite in
  `map_display_draw`), `g_FilePageArchive_0_Size`, `g_RebeccaItemSlots_0_Qty`. Everywhere else the
  code indexes the real array (`g_TitleSpriteTpage[option]`, `g_PlayerAnimFunctions[40]`,
  `g_SelectPanels[1].pos_x`, `g_EndingEpilogueSlots[n]`, `g_RoomItemsFlags[8]`,
  `g_FileBookTable[b][i]`, `LITP(TaskSlot, g_Tasks)[slot].gp`, `g_OtMain[1].org`). The element
  aliases are listed in `dualshock/config/elemfield_syms.txt`; the cast/warning tools never touch
  lines that use them.
- The computer-lab slides read `g_LabSlidesScrollX` (0x800B9D24) as `g_LabSlidesAnimState + 3`.
  Written through its own symbol it compiles differently, so `computer_lab.c` keeps the original
  expression behind a macro:

  ```c
  #define LAB_SCROLL_X(T) (*(T *)(g_LabSlidesAnimState + 3))
  ```

- `g_MenuFlags` (at `g_MenuWork` + 3) keeps its own symbol: changing the symbol a view starts from
  could change the code.

### Pointer adjusted in a separate statement

```c
p = base;
p = (T *)((u8 *)p + 0x744);
```

This makes GCC 2.6.3 keep a separate `addiu p,p,0x744` and use small store offsets. Written as one
expression, the offset is folded into the stores instead (`sh $0,0x7B8(base)`). Seen in
`func_80031A58`, `func_80031EDC`, `func_80031A74` and `func_80031BFC` (objects at
`D_800C51BC + 0x744` / `+ 0x6C8`). This idiom is why code in the 0x80031xxx area can look like the
work of another compiler.

### The scratchpad (0x1F800000)

The scratchpad (1 KB at 0x1F800000) is addressed by number.

- Many of Capcom's accesses read/write it as struct members: `lui v0,0x1f80; lw 0x3D4(v0)` per
  access, no CSE'd `lui; ori` address register. In a draft this is
  `M2C_SFIELD(0x1F800000, s32 *, 0x3D4)` (main func_80013B14). A scratchpad address stored twice
  across calls needs the struct form (`M2C_SFIELD(0x1F800000, s32 *, 0x3D8)`), otherwise its
  address is kept in an s-register (an extra saved register).
- Mixed forms in one function: an address used for a call argument / several stores stays a plain
  literal `*(s32 *)0x1F8003D0` (address CSE'd with the argument, kept in a register), the rest are
  `M2C_SFIELD` (func_800123C0, func_8001D234). func_80012A9C matched with only 0x3D4 as struct;
  the loop counter 0x1C and 0x4/0x8/0xC/0x14 literal. `scratchmix.py` searches these combinations,
  and `scratch.py` rewrites scratchpad accesses into the struct form.
- A readable struct form (`SCRATCH->vec_x`) is NOT interchangeable with the number: struct members
  get their own `lui` per access and follow in-struct alias rules. The numbers stay and get `sp_`
  names (see [code_notes.md](code_notes.md)).

  > **Note:** both forms occur in `src/` (about 735 `SCRATCH->field` accesses and 3,700
  > `LIT(T, sp_...)` / `LITP(T, sp_...)` uses): each site keeps the form the original code used, because
  > the struct form and the number form are not interchangeable.

- **Scratchpad loop counters `while (--*X != -1)`** (main func_80012ED0): m2c shows

  ```c
  *(s8 *)0x1F80001C = (s32) n; t = n - 1; *(s32 *)0x1F80001C = t;
  if (t != -1) do { ... t = *(s32 *)0x1F80001C - 1; *(s32 *)0x1F80001C = t; } while (t != -1);
  ```

  The original is

  ```c
  *(s32 *)0x1F80001C = n;
  while (--*(s32 *)0x1F80001C != -1) { ... }
  ```

  Written as two plain stores, GCC's flow pass deletes the first one (`sw n` missing).

### The 0x84-byte record table at 0x800C3024

The table of 0x84-byte records at 0x800C3024 (indexed by the byte at 0x800CF86E; the `Effect`
type in `include/re1_types.h`, `EFFECT_SIZE 0x84`):

- m2c's `*(X + &(*(s32 *)0x800C30YY))` is a scaled pointer sum: X is a byte offset in the original
  but as C it is multiplied by 4 (`sll 4` where the original has `sll 2`). Write
  `M2C_SFIELD(&((Effect *)0x800C3024)[*(u8 *)0x800CF86E], T *, off)` (main func_8001FD00,
  func_8001C018; `ply84.py` rewrites every type, with an outer cast, into struct-member form).
- Fields read twice around a call: the struct form keeps the address from being CSE'd into an
  s-register (func_8001F38C).
- Chained stores `x8 = xA = xC = 0` appear on these records (func_8001D450).
- An `Effect` slot's `transform` is a `MATRIX`; block copies of it are plain assignments.

### Struct fields vs separate globals at -O2

- At -O2 a struct field is reached through `la reg, sym+off` and **reloaded on every access**, while
  a separate symbol was loaded once and the value reused; the struct form also keeps
  `&g_PlayerEntity` in a register. So many main-program functions keep separate symbols for player
  fields (`g_PlayerHitState`, `g_PlayerHealth`, `g_PlayerPoisonTimer`, ...; the original source
  evidently used separate globals or something equivalent there). The player's per-field globals
  were replaced by `g_PlayerEntity` / `g_PlayerWork` fields only where the bytes stayed the same.
- The same holds for `g_MenuWork`: its fields are used where they match; elsewhere per-function
  aliases remain.
- Struct members can also change scheduling at -O2, so every conversion in the main program needs
  its own verified pass.

### Struct and string copies

- `lwl/lwr ... swl/swr` (+ `lb/sb` for odd sizes) on the stack = a by-value copy of a small struct
  from rodata: `typedef struct { char s[6]; } Str6; Str6 p = D_80010130;` (the "bu00:" memory-card
  path, main func_8001A0D4). `char path[20] = "bu00:"` is the 6-byte copy plus
  `memset_C43(path + 6, 0, 14)` (func_80019EA4, func_80019D90). A local `s16 t[2][3]` table copied
  from D_80010000 (func_800123C0).
- 32-byte struct copies (`lw` x4 / `sw` x4 groups) = GCC's block move of a 4-aligned struct
  (func_80031BFC, MATRIX at +0x44).
- -O0 struct copies use `lw t2,t3,t4,t1`/`sw` groups; m2c's word-by-word copy of 16/32 bytes is
  `*(VECTOR *)dst = *(VECTOR *)src;` or `MATRIX`; see
  [Pointers, records and tables](#pointers-records-and-tables).
- Joints, entities, vectors and matrices are copied and stepped with their own types (`Joint`,
  `Entity`, `SVECTOR`, `VECTOR`, `MATRIX`, `GsRVIEW2`, `RoomLight`, ...) wherever that matches.
  Some copies only match as raw bytes, written with one macro from `re1_types.h`:

  ```c
  #define BYTES(n) struct { u8 b[n]; }
  ```

  It is needed when:

  - a record is copied as raw bytes: the assignment compiles to unaligned word copies (`lwl`/`lwr`
    + `swl`/`swr`), while the record's own (aligned) type would compile to `lw`/`sw` (e.g.
    `effect.c` `EffectPhaseBytes`);
  - a pointer is stepped n bytes at a time where a plain pointer of the element type changes the
    -O2 code (GCC 2.6.3 schedules struct-typed memory accesses differently): `cd.c`
    `BYTES(2) *ent` (a `u16 *` schedules differently at -O2), the TMD primitive steps in
    `model.c`, the ending's 6-byte text records, `ZombieHitThresholds`.

### Engine API calls in the overlays

The stage overlays call the engine through a table of function pointers at 0x801FE9xx with the
form `(D_801FEC18 = D_801FExxx)()`: store the "current function" pointer, then call it. For
example `(D_801FEC18 = D_801FEA20)(M2C_PFIELD(p, s16 *, 0x170))`.

- m2c writes `D_801FEC18 = D_801FEA20; D_801FEA20(..)` or `(D_801FEC18 = X, (X(args) == 0))`; the
  original loads the function pointer before the argument (`addiu; move; lh 0(..)`), and the split
  form costs a reload (refine mutation `callassign`; `callmerge.py`, which also handles a
  call-table function passed as the argument of another).
- Typing `D_801FEC18` as `u16 (*)()` adds `andi 0xffff` before tests (mutation `fpret`).
- In `src/`, `include/overlay_api.h` declares the `api_*` pointers and `g_ApiCallPtr` once. A
  call's return type is `g_ApiCallPtr`'s (an assignment has the left side's type). `API_AS(T, f)`
  is only needed where the result must be another type, in practice `u32` (unsigned `%` and
  compares):

  ```c
  #define API_AS(T, f) ((T (*)())(g_ApiCallPtr = (f)))
  ```

- At -O0 a prototype vs a `()` declaration of an API pointer compiles identically.

### Unprototyped calls

Capcom often called functions without prototypes, and its callers often saw a different prototype
than the function's definition. The C reproduces this with K&R definitions and per-function
aliases; see [Declarations, types and casts](#declarations-types-and-casts).

---

## Field access: member form vs pointer form

### The two forms

At `-O0` GCC 2.6.3 compiles a struct member access and pointer arithmetic differently:

| spelling | code at -O0 |
|---|---|
| `OFFSET_MEMBER(p, T *, off)`: a real (anonymous) struct member (m2c's `M2C_SFIELD`) | `sb $v1,0x85($v0)` |
| `OFFSET_DEREF(p, T *, off)`: pointer arithmetic (m2c's `M2C_PFIELD`) | `addiu; move; sb ..,0(..)` (array-element style) |

At -O2 the struct form changes scheduling and alias decisions (MEM_IN_STRUCT_P; see
[Memory references](#memory-references-which-loads-may-pass-which-stores)), e.g. func_80038F7C.

Consequences:

- A plain member access (`e->field`) compiles exactly like `M2C_SFIELD`/`OFFSET_MEMBER`.
- `*(s32 *)&e->field` is **not** the same code as a member access at `-O0`: it computes the address
  separately (`addu` + `move` + `sw 0(r)`).
- Pointer-arithmetic accesses (`M2C_PFIELD`) cannot simply become `->field`; they keep a
  pointer-form macro.
- Array indexing through a typed pointer (`JOINT(p)[1].f`) does **not** equal the original at
  `-O0`, so later array elements always use the exact form.
- Typed pointer arithmetic scales: `n * 0x7C + v + 0x44` must not become
  `n * 0x7C + &JOINT(v)->world`, and the enemy work base address `(u8 *)e + 0x16C` stays a byte
  pointer (as a `W *` it would scale `+ n`).
- In one case (`yawn_update`) a plain member did not equal `M2C_SFIELD`: an extra delay-slot `nop`
  appeared after a call. The exact level of the readability macros therefore always uses
  `MEMBER_AT` (the `OFFSET_MEMBER` form).

### The m2c field macros

`include/m2c_fields.h` defines two forms of m2c's field-access macro for decompiler drafts:

- `M2C_SFIELD`: anonymous struct member (`sb $v1,0x85($v0)` at `-O0`).
- `M2C_PFIELD`: pointer arithmetic (at `-O0`: `addiu; move; sb ..,0(..)`, i.e. an array-element
  access).
- `M2C_FIELD` = `SFIELD` for `-O0` targets (`-DM2C_FIELD_STRUCT`, set by the Makefile and the
  tools) and `PFIELD` for the main program, because the struct form changes GCC 2.6.3's `-O2`
  scheduling (e.g. `func_80038F7C`).

  > **Note:** `M2C_FIELD` (decompiler drafts only, `include/m2c_fields.h`) is the struct-member form
  > (`M2C_SFIELD`, the same form as `OFFSET_MEMBER`) for the -O0 targets (`-DM2C_FIELD_STRUCT`) and the
  > pointer form (`M2C_PFIELD`, like `OFFSET_DEREF`) for the -O2 main program.

`include/m2c_fields.h` includes m2c's own `include/m2c_macros.h` and redefines `M2C_FIELD`;
drafts include `m2c_fields.h`. m2c's header is **not committed** (it is GPL-3.0 licensed, and the
project itself has no license). `dualshock/tools/m2c_macros.py` downloads it from the m2c
repository at the commit the Python venv installs (`M2C_COMMIT`, the same pin as
`tools/requirements.txt`; keep the two in step). The top-level `tools/setup_tools.sh` runs it, and
`compiler_matrix.preamble_for()` calls `ensure()` when the file is missing.

The m2c macros themselves (`M2C_FIELD`, `M2C_SFIELD`, `M2C_PFIELD`) no longer appear in `src/`;
they only exist in `include/m2c_fields.h` for drafts. Many examples in this guide are written in
draft form; in `src/` the same code uses the exact forms of `re1_types.h`, which are built on
`OFFSET_MEMBER` / `OFFSET_DEREF`:

```c
#define MEMBER_AT(p, W, path, T)    OFFSET_MEMBER(p, T *, PATH_OFF(W, path))
#define FIELD_AT(p, W, path, T)     (*(T *)((s8 *)(p) + PATH_OFF(W, path)))
#define MEMBER_AS(p, W, f, T)       OFFSET_MEMBER(p, T *, FIELD_OFF(W, f))
#define FIELD_OF(p, W, f)           (*(__typeof__(((W *)0)->f) *)((s8 *)(p) + FIELD_OFF(W, f)))
#define FIELD_OF_AS(p, W, f, T)     (*(T *)((s8 *)(p) + FIELD_OFF(W, f)))
#define ENTITY_AS(e, f, T)          OFFSET_MEMBER(e, T *, ENTITY_OFF(f))
#define ENTITY_FIELD(e, f)          (*(__typeof__(((Entity *)0)->f) *)((s8 *)(e) + ENTITY_OFF(f)))
#define ENTITY_FIELD_AS(e, f, T)    (*(T *)((s8 *)(e) + ENTITY_OFF(f)))
```

The exact macros equal the m2c forms at both `-O0` and `-O2`. The readable cast form (`(T)e->f` on
read) equals at `-O2` but not always at `-O0`, so tools fall back per function.

### Address-first fields

Capcom reaches some fields through a computed address (`addiu base,0x17x; move; lh 0(..)`),
written `M2C_PFIELD(p, s16 *, 0x170)` in a draft. Entity fields at 0x170+ with that signature are
`M2C_PFIELD`. The sub-structure at 0x16C is reached the same way:
`M2C_FIELD(((u8 *)D_800CAB14 + 0x16C), u8 *, k)` (`entsub.py`; func_8011AE08 went from distance
46 to 18). Never write `D_800CAB14 + 0x16C` on an `Entity *`: it scales by sizeof(Entity) = 0x18C
(stage4 func_8011B1E4). `x = (s16) ((u16) x + 1);` is `x++;` for such fields (`incfix.py`).

### Entity state words

The original code reads/writes Entity's state bytes as one value, in pointer-arithmetic form (a
struct member would compile differently at `-O0`):

```c
#define ENTITY_STATE_WORD(e)  OFFSET_DEREF(e, s32 *, 0x84)  /* state, sub_state, action_behavior, action_state set together */
#define ENTITY_ACTION_S16(e)  OFFSET_DEREF(e, s16 *, 0x86)  /* action_behavior + action_state */
#define ENTITY_ACTION_U16(e)  OFFSET_DEREF(e, u16 *, 0x86)
```

### The four ways Capcom's code reaches the enemy work area

Entity bytes 0x16C..0x18B are each enemy's private data. Compile tests at `-O0` showed which
readable spelling gives identical code for each original form:

| original | readable | code |
|---|---|---|
| A `M2C_FIELD(((u8 *)e + 0x16C), s16 *, 6)` | `ZOMBIE(e)->f` = `ENEMY_WORK`: `((W *)((u8 *)e + 0x16C))->f` | `addu 364; lh 6(..)` |
| B `M2C_PFIELD(e, s16 *, 0x172)` | `ZOMBIE_FIELD(e, f)` = `*(typeof f *)((s8 *)e + (0x16C + (int)&((W *)0)->f))` | `addu 370; lh 0(..)` |
| C `M2C_FIELD(e, s16 *, 0x172)` | `ZOMBIE_MEMBER(e, f)` = member form with the same computed offset | `lh 370(..)` |
| D `(u8 *)e + 0x179` (an address) | `&ZOMBIE(e)->f` | folded to one `addu` |

A plain struct member (`e->work.zombie.f`) only matches form C, and `*&ZOMBIE(e)->f` does not give
form B. The generic macros are:

```c
#define ENEMY_WORK(e, W)            ((W *)((u8 *)(e) + 0x16C))
#define ENEMY_WORK_AS(e, W, f, T)   OFFSET_MEMBER((u8 *)(e) + 0x16C, T *, (int)&((W *)0)->f)
#define ENEMY_FIELD(e, W, f)        (*(__typeof__(((W *)0)->f) *)((s8 *)(e) + (0x16C + (int)&((W *)0)->f)))
#define ENEMY_FIELD_AS(e, W, f, T)  (*(T *)((s8 *)(e) + (0x16C + (int)&((W *)0)->f)))
#define ENEMY_MEMBER(e, W, f)       OFFSET_MEMBER(e, __typeof__(((W *)0)->f) *, 0x16C + (int)&((W *)0)->f)
#define ENEMY_MEMBER_AS(e, W, f, T) OFFSET_MEMBER(e, T *, 0x16C + (int)&((W *)0)->f)
```

### Type mismatches (signedness)

The original code very often reads a field with another type than the one it stores (an `s16`
field read as `u16`, and so on; signedness differs per site). The rule:

- the struct field gets the most common type;
- another type is written as a **cast when read**, and **plainly when written** (a same-size store
  is the same instruction);
- tested contexts where the cast gives identical code: store, load, compare, `++`, `+=`, function
  argument, array index;
- contexts that differ with a cast: `x-- == 0` tests and form-B `== 0` compares. There the exact
  `_AS(e, f, T)` forms are used (per enemy, e.g. `HUNTER_AS`, `CERBERUS_FIELD_AS`; generic
  `ENEMY_WORK_AS`, `ENEMY_FIELD_AS`, `ENTITY_AS`, ...).

### Typed structs do not help m2c

Regenerating m2c drafts with the `Entity` struct as context made drafts *worse*: on 30 stage-4
functions, the existing struct gave 1 better / 12 worse, and with types corrected from the original
loads (all byte fields `u8`, halfwords by majority load) 0 better / 13 worse. Capcom reaches the
sub-structure at 0x16C address-first (`addiu v0,v1,0x16C; move; lbu 18(v1)`), while plain struct
fields compile to direct offsets. The untyped `M2C_FIELD`/`M2C_PFIELD` forms plus refine reproduce
such details. Structs are a readability pass for *matched* code (rewrite and verify byte identity),
not a matching tool.

---

## Rules for the overlays (-O0)

All overlays (LOGO, PROLOGUE, TITLE, SELECT, ENDING, STAGE1-7) are compiled at -O0. Addresses
0x800E.... are LOGO/PROLOGUE/TITLE/SELECT/ENDING functions; 0x801..... are stage functions. The
refine mutation and rewrite-tool names in parentheses automate each rule (see
[The draft and search tools](#the-draft-and-search-tools)).

### Locals and the stack frame

- GCC 2.6.3 at -O0 gives every local, even an unused one, a stack slot, in declaration order.
  m2c's `spXX` names are the original offsets, so missing slots need padding locals, and unused
  locals change the frame and must be removed (or kept) to match.
- An unused local still takes a stack slot at -O0 (frame 8 bytes bigger, no `fp` references;
  mutation `padlocal`). Use separate `s32 padNN;` variables, not arrays (an `s32 x[2]` is
  8-aligned and shifts everything). Padding locals may need to be declared *after* the others
  (func_8011E810). An unused `temp_x` declaration grows the -O0 frame (`dropunused.py`).
- Four adjacent shorts passed as `&sp20` are one struct: `struct { s16 x, y, w, h; } sp20;`.
- A `VECTOR` local after one unused `s32` gives the layout `sp10 @0x10, gap, vx @0x18, vz @0x20`
  (func_80118B90).
- m2c silently drops dead stores to locals (`sp10 = x98;` never read). When a frame slot is
  written but unused in the original, add the store back (stage3 func_8010DEB0). An unused local
  initialised at the start (`p = D_800CAB14->ptr98;`) shows as `lw; sw N($fp)` in the first
  instructions (mutation `deadinit`).
- A stack struct/array (`SVECTOR v; RotMatrix(&v, ..)`) that m2c split into `sp10`/`sp12`/`sp14`:
  ours drops the stores to the "unused" members (mutation `stackarr`).
- Locals initialised from rodata are described in
  [Local initialised data](#local-initialised-data).
- **Doubled `addiu v0,fp,N` before a block copy** (the func_80106F44 and func_8010574C families in
  every stage, 12 functions, which copy the 32-byte table `D_80105400` into a local at `fp+0x18`):
  the first `addiu` is a dead `register` variable in its own block:

  ```c
  Bytes32 sp18;
  { register Bytes32 *p = &sp18; }   /* never used - probably a macro in Capcom's source */
  sp18 = D_80105400;
  ```

  At -O0 a `register` variable lives in a pseudo; its unused initialiser is never deleted, and the
  pseudo gets `v0` because it dies immediately. A function-scope `register` variable lands in
  s0/s1 instead (extra prologue save), so the inner `{ }` block matters. `mips.c`
  `expand_block_move` has only one `copy_addr_to_reg`, so the extra address has to come from the
  C source. (Struct init, brace init, string literal, memcpy and statement-expression copies all
  give a single `addiu`.)

### Callee-saved registers in -O0 code

-O0 code normally never uses s0-s7. When an -O0 overlay function saves `s0/s1/s2` in its prologue
(m2c names the temps `temp_s0/temp_s1/temp_s2`), it almost always hides a `fold()` SAVE_EXPR (see
[Front end and fold()](#front-end-and-fold)), not `register` variables. A common case is one
`x += (call() == 0) ? 1 : 0`. Also: `x -= cond ? 4 : 1` and `(c ? 0x800 : 0x1000) - f()` fold the
constant ternary into the outer operation and keep the shared operand (or the call result) in
`s0` (func_8011FCAC, func_8011EF90).

### Increments and decrements

- **`x++` on a memory lvalue** loads the base pointer once more, and that load is unused. `x++`
  emits load + add + `move` + store, so `X += 1` and `X++` are not interchangeable (mutation
  `X += 1` -> `X++`; `X++` vs `X += 1` on a global short differ, `plusone.py`).
- A cast increment such as `F = (u8) (F + 1);` must be written `F++;` to reproduce the "unused
  first load" signature (refine's cast-aware `++`/`--`). `x = (s16) ((u16) x + 1);` is `x++;`
  (`incfix.py`; matters for address-first fields).
- `X = (u8) (X + 0xFF)` / `x += 0xFF` is `X--` (the -O0 `move; sb` decrement idiom that reloads the
  base pointer; mutation `decr`). Extended `decr`: `p->x--` where m2c gave the left side another
  field type (`u16` at odd offset 0x87); signature: `sh` at the wrong offset + missing base reload.
- **`if ((u32) p->x-- == 0)`** (func_8010C994): a test of a post-decremented *signed* halfword
  field, written with a sign-changing cast. Signature (-O0):
  `lhu v1,X(v0); addiu a0,v1,-1; sh a0,X(v0); sll v1,v1,16; sra v0,v1,16; bnez` (one base load).
  Without the cast GCC 2.6.3 folds `x-- == 0` into `--x == -1` (`li -1; bne`); with a `u16` field
  it decrements with `+0xFFFF` and tests with `andi`. m2c writes it as
  `t = x; x = (u16) (t - 1); if ((s16) t == 0)` (signature: an extra stack copy of the old value).
  Mutation `postdec` generates the cast form. The pattern is very common in the stages. It also
  combines: `(u32) x-- == 0 || y == 0` in one condition (func_8011C6F4; an explicit `j` to the
  body).
- **Byte post-decrement tests** `if (x-- == 0)` on a `u8` field: GCC folds it to `--x == 255`. What
  matches is `if (!({ (s32) x--; }))`, and for an `s8` field compared to a constant
  `if (({ (s32) x--; }) == 1)` (`postdecse.py`).
- **`if (--p->x == 0)`**: m2c loses the value (its test uses a never-assigned `temp_`, or it
  writes `x--;` + a test, or `(x = x - 1) == 0`). Ours then tests a stack slot (`lb`/`lh`) with a
  frame 8 bytes bigger; the original reloads the base and tests the new value with `sll/sra`. -O0
  loads the base three times for `--p->x` (mutation `predec`; func_8010B314).
- **Pre-increment through a pointer chain** (`--p->q->x`, `--D->xC4 > 0`): GCC -O0 expands the
  target address once, throws it away and re-expands `x = x - 1`, leaving a *dead*
  `lw v0,0xB8(v1)` (or a doubled address) right before the read. m2c drops the dead load; write
  `--field` (`nestdec.py`).
- `p = B; v = p->f; p->f = v + 0xFF; if (!(v & 7))` is `if (!(B->f-- & 7))` (`tempdec.py`).
- `x--; if (x == 0)` reloads after the store; `if (--x == 0)` does not. Pick by the asm.
- `*p++`: m2c moves the increment out of the delay slot (mutation `postinc`).

### Negation, multiplication, shifts, division

- `x = -x` on an `s16` field gets narrowed (`lhu; negu`). `x *= -1;` gives `lh; move; negu; sh`
  (func_8010BCF0). `= -(E)` -> `= (E) * -1` (`move; negu` at -O0; `negmul.py`). The same keeps a
  truncation: `x = (s16)(x / 3) * -1` gives `sll/sra 16; move; negu` (stage4 func_8011D70C,
  func_801220E4).
- `x /= -2;` for `x = -(x / 2)` (stage3 func_8013119C, stage5 func_80128818).
- `x * 0x10` vs `x << 4`: the multiply leaves a `move` (`andi; move; sll`). Pick by the asm
  (mutations `shl2mul`, `mul2shl`; e.g. `(x & 1) << 4` swaps register roles a0/v1 after the `andi`
  in func_8011A9E0). `(r & 1) * 0x800` and `(r & 1) << 0xB` give the same instructions with
  different register ties; try the multiply when the shift version has swapped registers around a
  call result.
- `a + a` vs `a * 2`: two loads.
- `(x * x++) << 3` instead of `x * 8 * x++` (stage6 func_801137D4, stage3 func_8010FCCC).
- m2c's `((s32) (x + ((u32) (x >> 0x1F) >> 0x1F)) >> 1)` is `x / 2` (`divfix.py`; `--s16`:
  `(s16) (x / 2)`). m2c's `t - (t / K) * K` is `t % K` (`modfix.py`, also for divisors with the
  add-back step such as `% 7`).
- **`abs`**: `abs()` at -O0 becomes a real call (link fails); use `__builtin_abs()`.
  `__builtin_abs(a - b)` gives `subu; bgez; move (delay); negu` with no stack slot (func_801092D8).
  `x < 0 ? -x : x` is not folded when x reads memory. m2c's
  `v = E; if (temp_x < 0) { v = -v; }` (temp never set) is also `__builtin_abs(E)` (mutation
  `absb`). `X ? -A : A` is expanded by GCC's COND_EXPR "singleton" path: copy A, then
  conditionally negate; it looks like a temp loaded before the test.
- u16 sum with a big constant: `(x & 7) - (u16)((y & 0xF) - 30)` keeps the constant `0xFFE2`
  (`li a1,0xffe2; addu` instead of `addiu -30`; func_801158B8).
- `s16` vs `u16` for `x -= K`: u16 loads `0x10000-K` into a register (`u16sub.py`).
- `A -= (u16) (B + K)` keeps GCC from re-associating into `(A - K) - B`.
- `x += (s16) ((r & 7) + 0x1E)`: the cast decides the evaluation order (func_80117CBC).
- **u8 read-modify-write narrowing** (stage4 func_80109CD4 family, 6 functions):
  `field = field - (f() + 1)` on a byte. `convert_to_integer()` narrows the whole subtraction to
  the destination's char type, which lets fold re-associate (`(field - 1) - f()`) or adds a subreg
  `move`. With a *different* signedness between the callee's `u8` return and an `s8` field
  (`s8 x = s8 x - (u8 f() + 1)`) the original code comes out (found by brute-forcing return type x
  field type x constant form).
- `{ s32 t = EXPR; *(s16 *)P = t; }`: computing in full width, then storing narrower; a direct
  narrow store lets GCC narrow the reads (`lhu` instead of `lh`/`lw`; mutation `widetmp`).
- `((s16) x < 0)` instead of m2c's `(x & 0x8000)`: `sll 16; bgez` vs `andi 0x8000; beqz` (mutation
  `signtest`).

### Evaluation order and operand order

- Comparison operand order decides the -O0 load order: the left operand's base pointer is loaded
  first, its value read last.
- With a call on one side, Capcom wrote the field on the LEFT: the base pointer is loaded before
  the call and kept in `s0` (`cmpswap.py`). m2c's `a < (x = e)` is Capcom's `(x = e) > a` (stage5
  func_8012CB94).
- `(5 + (r() & 7) + (r() & 7) + (r() & 7)) * 2`: the order of the three random calls follows the
  order of the additions (stage3 func_8011D874; `randsum.py`).
- A pointer local (`void *sp1C`), not `s32`, keeps `a + b + sp1C` in Capcom's addition order
  (stage4 func_8011AA34).
- `X = X op E;` vs `X op= E;` changes the -O0 evaluation order of the lvalue (mutations
  `opexpand`/`opassign`; `selfplus.py` turns `X = X + a` into `X += a`). `t = X = (u8) (t + f());`
  (t never set) is `X += f();` (`selfadd.py`).
- Swapped operands of a parenthesised `(A + B)` with complex operands change `addu` operand order
  (mutation `commute2`).
- `if ((p->x = f()) == 0)`: an assignment inside the condition. m2c writes
  `t = f(); p->x = t; if (t == 0)`, which adds a temp store + reload (`sb; lb`) instead of the
  original's `sll/sra` of the result (mutation `assigncond`).
- `b = c = v; a = v;` or `t = X; c = t; b = t; a = t;` is `a = b = c = v;` (`chainfix.py`).
- `x = c ? a : b`: m2c writes an if/else into a temp, then `x = temp` (temp stack slot; mutation
  `ternary`). `v = (C) ? A : B;` into a local still used later gives `li; j; li; sw` into one stack
  slot (func_8011F11C; mutation `ternary2`).
- `flag = (a < 0x401) || (b >= 0xC00);` gives the boolean value pattern `v0 = 0; tests; v0 = 1`
  (func_80120254).
- A conditional array index `T[(a != 0) ? ((a > 0) ? 2 : 0) : 1]` folds into the address
  (func_801187DC).

### Branch layout and labels

- **An explicit `return;` emits a `j <epilogue>`.** This also applies to a `return;` inside the
  last `case` before the end of the function (mutation `dropjump` drops one `break`/`return`).
- **Return placement:** a trailing `return;` belongs inside the innermost `if` at the end, so
  failing tests jump past its `j <epilogue>` (branch targets 8 bytes later, same size;
  func_8011F618; mutation `movereturn`, `retinto.py`). The other if/else layout:
  `if (c) return R; rest` <-> `if (!c) { rest } return R;`, and
  `if (c) { A; return x; } B; return y;` -> `if (!c) { B; return y; } A; return x;` (inverted
  branch such as `bgez` vs `bltz`, blocks in swapped order; mutation `invertret`). Every return in
  an if/else chain may need to become one shared exit (func_800E3874). A then-branch that ends with
  `break;`/`return;` and *no* `else` (mutation `earlyexit`); a negated condition with the short
  branch first (mutation `swapif`).
- **Lost returns:** a `return 1;`/`return 0;` m2c lost at the end of a non-void function shows as
  a missing `li v0,N` (mutation `retconst`).
- **goto pairs:** m2c's `if (A) { if (B) goto L; } else { L: BODY }` and
  `if (A) { if (B) { } } else { lbl: BODY }` are one condition `if (!(A) || (B)) { BODY }`: GCC's
  jump code for a trailing `&&` inside `||` ends with an explicit `j <body>` to the next
  instruction (func_80115274, func_80112928; mutation `gotoor`). `(A && B) || C` appears in m2c as
  `if (A) { if (B) {} goto block; } if (C) {} else { block: }` (stage5 func_80111A34, stage3
  func_8011E740).
- `if (a != x && b != y) { } else { body }` is `if (a == x || b == y) { body }` (`emptythen.py`).
  Function-final `if (!A && !B) {body} S;` is `if (A || B) {S;} else {body}` (`orthen.py`).
- **Loops:** `while (c) { .. }`: m2c writes `loop_N: if (c) { ..; goto loop_N; }` (`bnez body;
  j end` vs our `beqz end`; mutation `gotowhile`). `for (;;) { ...; if (c) break; }` compiles to
  `bne skip; j exit; skip: j top`; m2c writes `loop_N: ... if (!c) goto loop_N;` (`gotoloop.py`).
  An endless loop whose tail is `j cont; j exit; cont: j top` is `do { ... } while (1);`.
- `for (;;)` and `while (1)` compile differently at -O0 (ending func_800E590C: only `for (;;)`
  matches); `for (i = 0; ...)` vs `i = 0; while (...)` too (func_800E203C).
- **Lone `nop`s = a user label after another label.** The parser calls `emit_nop()` before a user
  label, and at -O0 that is a real `nop` when the previous insn is a CODE_LABEL. A single `nop`
  that is neither a delay slot nor a load delay means a `goto` label written right after another
  label: a `case N:` label, or the end of an `if`/`else`. Raw cc1 output shows it as
  `.set noreorder / nop / .set reorder`. Branches that skip the `nop` target the user label;
  branches that land on it target the earlier label.
  - stage4 func_8011FEA0: `... break; case 6: block: ...` with `goto block;` from case 5 (the
    jump-table entry for case 6 points at the `nop`, case 5's `goto` lands after it).
  - stage2 func_8011E868: `if (len < 50) goto block; <statements ending in an if>; block: ...`.
    Wrapping the statements in `{ }` breaks it (the block-end NOTE becomes the previous insn).
  - `if (cond) { setup; goto L; } fallback; break; case 3: L: ...`: the setup arm jumps past the
    `nop` that `case 3:` + `L:` produce.
  - The reverse: an extra `nop` at a label in our build means the original's label followed a
    real instruction, not an `if` end. A label no `goto` uses must go (`deadlabel.py`), and m2c's
    `goto block_N` before a label is often a plain if/else.
- **Loops after an if/else:** `emit_nop()` also runs at every `while`/`for`/`do`; if the previous
  insn is a label you get a `nop`. No `nop` in the original means the if/else was inside its own
  `{ }` block (`loopblock.py`).
- **Dead code after a jump = a constant-false condition.** m2c writes
  `call(); goto end; x = x >> 2;` when the original evaluates a call, jumps, and has unreachable
  code after the jump. That is -O0's output for an `if` whose condition folds to a constant but
  keeps the call's side effect, e.g. `if ((u32) call() < 0U) { x >>= 2; }` (stage1 func_80114E4C;
  `call() && 0` and `>= 0U ... else` give the same code).

### Switches

- No `break;` before the switch's closing brace (ours would have an extra `j <after switch>; nop`;
  mutation `dropbreak`). A `case 1: f(); }` that ends the switch without a `j` has no `break`.
  No trailing `default: return;` in a switch that ends the function (mutation `dropdefault`;
  `defaultret.py` turns `/* fallthrough */ default: return;` into `break;`).
- A `break;` may be missing before a case label the draft falls into (mutation `addbreak`).
- The jump table covers 0..4 although case 4 has no code: an empty `case 4:;` as the last case;
  the code after the switch is the out-of-range target (func_8010DCF0; `emptycase.py`).
- Binary-tree switches (`< 6`, `< 8`, `== 0x80`) are a `switch` with `case 6: case 7: case 0x80:`
  (func_80110D20).
- `if (c) { } else { case 2: ... }` is `if (c) break; case 2: ...` (`elsecase.py`). m2c's "case
  label inside an if" usually means `if (cond) break;` / `if (cond) goto L;` at the top of the case
  (`beqz over; j exit`); check where the jumps go before keeping its shape.
- `if (e != 0) { } else { body }` with a `j` after both arms is
  `switch (e) { case 0: body; break; }` (`switch1.py`). `t = E; switch (t)` is `switch (E)`
  (`switchtemp.py`).
- Separate `if (a) {...; break;} if (b) {...; break;}` instead of else-if when every arm jumps
  straight to the switch exit.
- A small `switch` m2c cannot parse ("unable to parse jump table") is usually
  `case a: case b: ... f(); break; default: g();`. Read the jtbl words: entries equal to the
  default target are cases that fall into `default`.
- Jump-table alignment and pad words are in [Jump-table alignment](#jump-table-alignment).

### Pointers, records and tables

- **Address-first fields and the 0x16C sub-structure:** see
  [Address-first fields](#address-first-fields).
- **Record tables** with 2/4/6-byte records: `D_X[i].field` on an anonymous struct array, e.g.
  `extern struct { s16 a; s16 b; s16 c; } D_X[];` or `extern struct { u8 a; u8 b; } D_X[];` or
  `extern struct { s16 x, y; } D_X[];` with `D_X[i].x * 2`. -O0 indexes with symbol-relative
  addressing (`lui at; addu at,at,idx; lh %lo(D_X+off)(at)`) and the `i * size` multiply leaves a
  `move` (func_8010B9A0, func_801114B4, func_801214F4, func_8011A8B4, func_801182F4,
  func_8011C2E0, stage6 func_8011FD08, stage3 func_80118BDC; `rectable.py`). `u16 D[i*2]` or a
  cast pointer gives `la; addu; lhu 0` instead. Use an anonymous struct, not a typedef
  (`integrate.py` moves the draft's file-scope declarations into the function).
- **Byte tables:** `*(s32 *)((u8 *)&D_X + (i))` on a byte table is `D_X[i]` with `extern u8 D_X[];`
  (`bytearr.py`, also `--s16/--u16`). 2-D byte tables: `extern u8 D_X[][8];` and
  `D_X[a & 1][b & 7]` (stage6 func_801123C4). A u16 table that m2c turns into an invented parameter
  `(arg2) = &D_X;` is `x74 += D_X[x2];` with `extern u16 D_X[];` and a `(void)` parameter list
  (stage6 func_80120FDC).
- **Indexing:** `*(&D_X + (E * 4))` -> `((s32 *)&D_X)[E]` (also 2- and 1-byte variants; mutation
  `index`).
- Records of `size` bytes at a symbol or Capcom's fixed address: `*(T *)((u8 *)&D_X + i * size)` /
  `(u8 *)0xADDR + ...`. m2c's `*((i * N) + &D_X)` scales the byte offset again (wrong `sll`, wrong
  load/store width; mutations `byteoff`, `byteoff2`, `byteoff-asm`; the last reads each access
  type from the original's `lbu/lh/lw/sb/sh/sw ..., %lo(D_X)` all at once, because one-at-a-time
  is a local minimum when the accesses share one index computation, e.g. record arrays at
  0x800C3000+). Pointer additions that m2c scaled twice: mutation `ptradd`; pointer element type
  (stride errors like `addiu 16` vs `4`) is another refine mutation.
- A global accessed as an array element `D_X[0]` (`extern T D_X[];`): -O2 loads the address once
  per block (`lui/addiu`); symbol form = `%lo` per access, constant form = `lui/ori` (mutation
  `symarr`; `symsplit` mixes `D_X[0]` and a scalar alias per switch case, as -O2 code does).
- A global reached through a local pointer `T *p = &D_X;` gives one `lui/addiu` then `0($reg)` per
  access (mutation `symptr`). `p = base; p += i * size;` adds into the base's register; with
  `void **` the base is a word load (mutation `splitadd`).
- **Stepping record pointers:** `ptr++` on a pointer to an N-byte struct leaves a `move`
  (`addiu; move`) that `ptr += N` on a `void *` does not (`ptrinc.py`, mutation `structinc`). Write
  `((struct { u8 b[0x7C]; } *)p)++;` (cast as lvalue; `ptrstep.py`) for
  `addiu v0,v1,124; move v1,v0; sw v1`. A plain `p += 0x3E0` has no `move`.
- `D_800CAB10->arr[i].m = ...` (a pointer to a struct holding an array of 0x2C-byte records at
  0x9C) gives the original's "base first, index second" order; `base + i * 0x2C + 0x9C` does not
  (ending func_800E4280).
- **Struct copies:** m2c's word-by-word copy of 16/32 bytes is `*(VECTOR *)dst = *(VECTOR *)src;`
  or `MATRIX` (-O0 struct copies use `lw t2,t3,t4,t1`/`sw` groups), e.g.
  `*(MATRIX *)0x1F8003E0 = *D_801FEC10` (func_80108F04), `MATRIX.t` into a local `VECTOR`
  (func_8011825C), `M2C_FIELD(base, MATRIX *, off) = *(MATRIX *)0x1F8003E0;` (mutation
  `structcopy`, `matcopy.py`, `blockcopy.py`).
- A stray `*(s16 *)&D_X = v;` next to `D_X = v;` byte stores of the same global: Capcom wrote one
  variable with different widths (stage4 func_801173DC; mutation `partstore`).
- Wrong store width from m2c: `*(s32 *)`, `M2C_FIELD(p, s16 *, ..)` (`sw`/`sh` vs `sb`; refine
  mutation "access width").
- A byte field one off (m2c misplaced a delay-slot load): `lbu a0,1(v1)` vs `0(v1)` (mutation
  `fieldoff`).
- **PsyQ macros:** `setUV4(p,...)` / `setRGB0(p,...)` (comma expressions of `(p)->field = ...`) give
  the "address once per field, then `sb N(v0)`" pattern; direct `arr[i][j].field` gives
  `lui at; addu at; sb %lo(sym)(at)` (select overlay).

### Calls, parameters and return types

- **Narrow parameters:** `lw v1,32(s8); sb v1,2(s8)` (or `move v0,a3; sh v0,0(s8)`) in the
  prologue is a `u8/u16/s16` *parameter*, not a local copy (`narrowparam.py`, `argcopy.py`). A
  function whose prologue never saves `a0..a3` has no parameters, whatever m2c says
  (`fakearg.py`). m2c also uses invented parameters as temps (`argtemp.py`).
- m2c drops and invents call arguments, especially on table calls: drop the last, or append 0 /
  a constant the original loads into that `$aN` (`move a0,zero` / `li a0,5`; mutations `argfix`,
  `argval`). Example: `&sp10` lost from two calls in ending func_800E4F3C. `t = E; f(a, (void *) t, t)`
  is `f(a, E)` (`dupargfix.py`, `--drop` also removes t's declaration).
- **Table calls without arguments:** m2c writes `D_X[i](D_X)`, where the "argument" is only the
  table address left in `$a0` by the address computation. Dropping it gives exact -O0 code (the
  call target then goes through `$s0`, which looks like a `register` variable but is not).
- **Function-pointer table dispatch:** m2c's `*((idx * 4) + &D_X)(..)` is written `D_X[idx](..)`
  with `extern s32 (*D_X[])();`.
- m2c's `sp10 = arg0` copy: use the argument directly (extra `lhu/sh` stack copy otherwise;
  mutation `copyprop`).
- `x = D_Y;` then using the local `x`: m2c sometimes keeps writing the global `D_Y` (stack loads
  `lw v0,16(s8)` vs `lui/lw` of the global; mutation `localuse`).
- A temp read the line before its assignment: inline the expression (mutation `uninit`;
  func_80045564).
- **Callee return types:** a callee whose result is added straight from `v0` (no `move` after the
  `jal`) returns `u8`: the add is narrowed to QImode. `p->x87 += f(...)` only matches when f returns
  `u8` (stage2 func_8011F47C, used by func_8011C6E4 and func_8011AE08; `u8 func_80126B70()`). If
  the callee is already defined in the file with another type, declare an alias in the caller:
  `u8 f_u8() __asm__("func_X");`.
- **In-file-only mismatches:** a draft can match alone but not inside the file. Stage1
  func_80118B90 calls func_801188C0, which the file defines as `(u16, u16, u16)`, but Capcom's
  caller passed ints (`lh`, not `lhu`). Declaring the callee in the draft as
  `void *func_801188C0(s32, s32, s32); /* extern */` makes `integrate.py` call it through a private
  alias.
- The return type of a function-pointer extern matters (mutation `fpret`), and so do the width and
  sign of a plain `extern T D_X;` (mutations `exttype`, `asmtype`).
- **Calling Sony functions through a cast does not work:** `((T (*)(..))f)(..)` makes GCC 2.6.3
  optimize the call differently (`func_800479A4`), so prototypes are copied into function bodies
  instead (see [Sony prototypes](#sony-prototypes-and-block-scope-declarations)).

### Forms that compile identically at -O0

Comparing the per-stage copies of the same enemy code shows differences that make no difference at
-O0: a prototype vs a `()` declaration of an API pointer, `(void *)` casts, extra parentheses, and
`!(x-- != 0)` vs `x-- == 0`.

---

## Rules for the main program (-O2)

The pass-level explanations are in [How GCC 2.6.3 decides the output](#how-gcc-263-decides-the-output);
this section lists the source-level rules found in the main program.

### Memory and alias rules in practice

- **Literal-address tables:** see [Fixed RAM addresses](#fixed-ram-addresses-number-vs-symbol).
- **MEM_IN_STRUCT_P decides load hoisting.** The main program's draft `M2C_FIELD` is a plain cast
  (not a struct member), so its accesses never pass a fixed-address scalar access. When the
  original hoists a load of `D_X` / `*(T *)0x800C....` above stores through a pointer, write those
  pointer accesses as `M2C_SFIELD(...)` (or a real struct). `sfield.py [all|stores|loads|base:<expr>]`
  does it (trybatch-able; main func_8005B8C0).
- `tbl[x - 1]` whose asm keeps `addiu -1` before the `sll`: write `tbl[({ x - 1; })]` (wrap the
  whole index). `pointer_int_sum()` and `expand_expr(EXPAND_SUM)` otherwise fold the -1 into the
  symbol offset (`idxwrap.py`, trybatch-able).
- An intermediate store our build drops as dead (`x &= A; x = (x & B) | C` keeps both `sw` in the
  original): make the first store `*(volatile T *)addr = tmp;`.
- Stores reordered in the original (`C8` written after `CA`/`CC` but loaded first): try statement
  permutations; the matching order is often not the m2c order (func_8005FD58).

### Assignments and arithmetic

- Chained assignment `a = b = c = x;` stores c, b, a from one register (`move v0,v1` then three
  stores); three separate statements were scheduled differently (main func_800297C4, a fade colour
  triple).
- `u8 *flag = &D_800BC5B5; if (*flag != arg1) *flag = arg1;` gives the original `lui; addiu` +
  `lbu 0(v1)` / `sb 0(v1)` (func_8002960C).
- A 24-bit address split over a u16 + u8 pair is
  `u32 t = lo + (hi << 16) + add; lo = t; hi = t >> 16;`.
- u8 global decrement: `*(u8 *)X -= 1` compiles to `addiu v0,v0,255`, `(*(u8 *)X)--` to
  `addiu v0,v0,-1` (main func_8004B020). `(*X)--; if (*X != 0)` = `addiu -1` + reload; `if (--*X)`
  reuses the value. After `(*X)--` a u8 temp reuses the stored byte, an s32 reloads.
- `f = e; if (f < 0) f = K; else f += K;` reuses the stored register for `+= K` but recomputes `e`
  for the test.
- `x*x+y*y+z*z` built step by step: `t = x*x; t += y*y; t += z*z;` (func_8002FC88).
  `((h << 5) + t) / 4` with `t = h << 4` gives the original's `(h<<5)+(h<<4)` (func_8005DCF0).
- In-place `lhu; sll 16; sra 24` = `s32 c; c = *(u16 *)p; c <<= 16; c >>= 24;`.
- `(u32) *(s16 *)p / 256` gives `lh; srl 8` (func_8004AA00).
- Bit test `arg0[arg1 >> 5] & (0x80000000U >> (arg1 & 0x1F))`: the operand order matters
  (func_8004AFFC).
- `if (x >> 15)` as an s16 sign test keeps the `sll` test apart from the `sra` value
  (func_8005E99C).
- Range tests: `if (b[2] < (u32)(a[0] - b[0]) || b[3] < (u32)(a[2] - b[1])) return 0; return 1;`
  gave the original's two branches with `move v0,zero` in both delay slots; nested ifs / a result
  variable did not (func_80038704).
- A value computed into a temp (`t = a + b; f1 = t; f2 = t >> 7;`) vs re-reading the field: the
  register flow shows which (`srl` from the sum register = temp).
- Commutative operand order (`addu v1,v1,a0` vs `addu v1,a0,v1`): split the operand into a temp
  (`m = pa & 0x7F; v = pb + m;`) or swap the source order; also try s32/u32 types for temps.
- A constant needed in a temp inside a `do {} while (0)` barrier gets its own variable.

### Variable types

- Globals read once into locals at the top (`p = D_X; id = D_Y;`) and reused after a store: the
  original keeps them in a0/a1 for the call (main func_80035320). The local for a u8 global is
  often `s32` (no extra `andi`).
- Signed `slt` for compares of two u16 values: hold them in `s32` locals; `u16` locals give `sltu`
  plus `andi` masks (func_8004748C).
- An `s16` local keeps `addiu` where nonzero-bits analysis would make it `ori` (func_800356B4); a
  `u16` result variable stops coalescing with a temp (func_800175B4).
- `u8 w; if (p[1]) w = p[1];` reloads the byte while an s32 reuses the tested value. A u8 copy
  `c = on;` gives `move s1,s0; andi v0,s1,0xff`.
- An m2c `do {} while (0)` barrier can just be a workaround for wrong variable types: fix the types
  and drop it.

### Statement placement

- m2c hoists reads above stores: when the original loads a field *after* a store to another object
  (`sw 0x34(s0)` then `lhu 6(a1)`), move the read below that store in the C source (main
  func_80048BF4).
- Index re-loaded between two stores in ours but not in the original: read the index into a local
  first (func_80049EF4).
- A call whose argument is loaded before the function pointer: compute the argument into a temp
  first (`a = &tbl[p[1]]; fn[p[2]](a);`, func_800496A4).
- A pointer variable set right before a call fixes OT-argument register order
  (`GsOT *ot = (GsOT *)0x800C88A0; f(..., ot + D_800C3010, ...)`, func_80058060).
- The entry jump of a rotated loop lands after a load only if the load is written at the end of the
  body (`p++; q = tbl[1]; test:`).

### Loops

Loop shape decides `loop.c`'s behaviour (see [Loop optimizer](#loop-optimizer-loopc)). Matched
shapes:

- The script interpreter func_8004B020 is
  `loop: while (fn[*PC]() != 0) {} if (depth) { pop; goto loop; }` inside a `while`; the nested
  `while (1)` forms hoisted one constant too many.
- `while (n--)` loops whose test is not copied to the top (`j test` / `move v0,n; bnez v0;
  addiu n,-1`) with the stored value computed before the loop: write the loop with gotos
  (`goto test; loop: ...; test: if (n-- != 0) goto loop;`, func_8004A208).
- The `for (n = K; n != 0; n--)` form matched a TestEvent polling loop (func_8001A1C8). Wait loops:
  `do { i--; func_80037508(1); } while (i != 0);` (func_80054FB8). `do { i--; ... } while (...)`
  loops are also Capcom's shape in func_8001A248 and func_80055F24 (see
  [Compiler bugs and crashes](#compiler-bugs-and-crashes)).
- CD retry loops:
  `do { inner: if (CdSync() == 5) { wait; goto inner; } } while (!a || !b); } while (!CdRead2(...));`;
  the nested do-whiles keep the constant 5 hoisted while the goto keeps the original jump targets
  (func_8005D4FC).
- Index loops `while (1) { if (i == 7) break; i++; off = i * 0x34; if (tbl[off] == 0) break; }`
  (func_80026710).
- A do-while starting with `if (c) break;` gets the exit rotated to the end and duplicated in
  front; `goto label_after_loop;` keeps the test at the top.

### Switch statements

- Switch decision tree without `slti`: the switch operand must be unsigned
  (`u32 x; switch (x >> 8)`); GCC then tests `== 0` with `beqz` instead of a signed range split
  (func_80047298).
- A `switch` with the cases in the original's body order (0xFF before 0xFE) gave the original's
  compare chain (func_80048BF4). Case bodies are laid out in source order. `switch (rand() % 4)`
  with an explicit `case 0: break;` reproduced the original decision tree (func_8001BABC).
- **Empty cases:** `case 1: ... case 12: break;` must be listed when the original jump table has
  entries pointing to the end label; without them GCC drops below the table threshold and emits a
  compare tree (func_8003E030).
- A switch with two cases whose asm has an extra `slti x, root+1; bnez default` test has a third,
  empty case (`case 0: break;`) completing the balanced tree.
- `balance_case_nodes` splits a 3-node case list at the middle node; a lopsided original tree means
  an extra left range node whose label equals default (`jump.c` deletes its `bge`); add an explicit
  `case a..b: break;` group (func_8003626C). A case placed between the last fall-through block and
  the loop-test label stops tail cross-jumping.
- An m2c `case 1: case 3: ... case 2:` merge compiles to distance 0 and still fails integration,
  because the diff tools do not compare jump-table contents (func_8003E030).

### Prototypes, calls and parameters

- **Keep m2c's prototypes for callees** (`M2C_UNK f(u16, s32, u32, u16)`): they change argument
  conversions and register allocation (func_80049148).
- **Callee return type matters:** a prototype returning a value (`s32`/`M2C_UNK`) instead of `void`
  changes register allocation after the call (v0 is "used"); call through an `__asm__` alias if the
  file defines the callee as void (main func_800490E4, func_80048B84). func_80016688 had to be
  declared `u8` (func_80015C6C).
- Callers that call a function without any declaration (implicit `int`) change register allocation
  when the new definition returns `void`: define it `s32 f(...)` without a return value
  (func_8003C518). An `s32` return type with no return value also reproduces an unfilled delay
  slot (see [Delay slots](#delay-slots-reorg)).
- A caller that passes its own a0/a1 through (`f()` in a wrapper) needs a private alias
  `M2C_UNK f_passthru() __asm__("f");` before the callee gets a prototype; the other callers'
  block-scope declarations become aliases too (func_8002B8A4).
- A call passing this function's own parameters straight through
  (`func_80030B10(arg0, arg1, arg2, 0)`: only `a3` is set before the `jal`; main func_8005F1EC).
- Narrow parameter types change the order of the prologue's argument copies: `u8 com` matched where
  `s32 com` did not (func_80015634); `s32 arg3` where m2c said `s16` (func_8003C518). A `u8`
  parameter gives `move t0,a2` + `andi t0,0xff` at use (func_800175B4). A u16 parameter in a callee
  alias removes a final truncation mask (func_80058B08).
- Arguments used without truncation in the prologue (`move s1,a0` then `beqz s1`) are int-typed
  parameters, even if they are stored with `sh`/`sb`.
- A missing parameter shifts every `aN` (5-parameter function whose arg0 is unused,
  func_8005DF64); a missing 4th argument shows as `li a3` absent.
- m2c merges two calls of one function into a shared variable + `goto`: write the two calls (main
  func_8005A6D0).
- `if (f() != 0) { ...; return 1; } return 0;` keeps the `return 0` block right after the test
  (`bnez -> L; j end; move v0,0`); `if (!f()) return 0;` moves it to the end.
- Inline asm `move $8, %0; sw $31, 0($8)` saves $ra into a global (func_80037508).

---

## Data, rodata and alignment

All data in `src/` is C (no `INCLUDE_ASM` / `INCLUDE_RODATA` is left).

> **Note:** the `test_integrate.py` instructions say to avoid test functions preceded by `#JTALIGN2` or
> `INCLUDE_RODATA` lines. No `INCLUDE_RODATA` line is left in `src/`; `#JTALIGN2` markers still exist
> (10 files), so that part of the advice still applies.

### Jump-table alignment

Our GCC puts switch tables in `.rodata` with `.align 3`. Capcom built each executable from several
source files, linked 4-aligned, with jump tables 8-aligned *within* each file.

**Overlays.** Example: in STAGE1 the tables of func_8011CF24 (0x80105504) and func_8011D4BC
(0x8010551C) are 4 mod 8, and CF24's table is followed by a zero word. Because each overlay is one
`stageN.c`, this is modelled by:

- `.align 2` for overlay jump tables (`Makefile` `alignfix_for`, `compiler_matrix.fix_align`); the
  main program keeps `.align 3`, because most of its tables need the 8-byte padding;
- an explicit pad word wherever Capcom's file had one:

  ```c
  __asm__(".section .rodata\n\t.word 0\n\t.text");
  ```

Where the pad goes:

- **After** the function's last jump table (`RODATA_PAD`): added automatically by `integrate.py`
  (`integrate.needs_rodata_pad` looks only at the function's LAST table). `intdiag.py` adds the pad
  the same way.
- **Before** a table, after the function's string literals (ending func_800E2F18: splat's output
  shows a `.asciz ""` just before the table), or **between** two tables of one function (title
  func_800E07BC). Emit the pad from inside the body, right before the `switch` that owns the next
  table. GCC writes string literals while building the function, but inline asm and jump tables
  only at final output, in code order. In `src/` this form appears where Capcom's table was
  8-aligned (`explode_leg_and_drop`, `ending.c`, `crow.c`).
- **Before the function:** a jump table whose asm starts with `.align 3` may need the pad before
  the function (padding after the previous file-rodata item), not only after it.

**Pads after finished functions (`padsweep.py`).** A C function whose original jump table ends in
Capcom's pad word can still match without the pad while the next function is an `INCLUDE_ASM` `.s`
(its own `.align 3` fills the gap). Converting that next function to C then shifts the whole
program by 4 bytes (stage6 func_80121878 / func_80121D00). `padsweep.py` tries the pad after every
such function one at a time and keeps it only when the build still matches (some zero words belong
to what follows instead). Run it after large integration rounds.

Pads are visible markers of Capcom's original source-file boundaries; splitting the overlay C files
at those boundaries is the proper model.

**Main program (`#JTALIGN2`).** Most matched main-program tables need `.align 3`, but some original
tables are only 4-aligned: Capcom's main program was built from several source files, and
0x800106DC..0x80010804 are packed 4-aligned.

- Put `__asm__("#JTALIGN2");` before such a function. `jtalign.awk` runs on cc1's output, before
  maspsx (which drops comments), and turns the next `.align 3` into `.align 2` (func_8004748C,
  jtbl_8001074C).
- The awk script counts markers: use exactly one `__asm__("#JTALIGN2");` per 4-aligned table.
  `#JTALIGN2PAD` does the same and also appends a `.word 0` after that table (before GCC's
  `.text`). func_80049478 = `#JTALIGN2PAD` + `#JTALIGN2` (jtbl_80010764 + pad + jtbl_8001078C).
  Because of the counter, a DUPLICATED marker shifts the conversion to the next function's table.
- A jump table that ends in `.word 0` can also be written as
  `__asm__(".section .rodata\n\t.word 0\n\t.text");` after the function (func_800251A4).
- The refine distance ignores `#JTALIGN2`; see [Diagnosis tools](#diagnosis-tools).
- `jtinsert.py` integrates such functions (see [jtinsert.py](#jtinsertpy)).

### Rodata order and `INCLUDE_RODATA` (matching-era drafts)

- The `.rodata` order in an object follows the order of `INCLUDE_RODATA` lines and C functions in
  the file. Example (func_80017D3C): the original order was D_80010044 (in the function's `.s`),
  D_80010048, D_80010058 (INCLUDE_RODATA lines after the function) and then the function's own
  jump table. As C, the three string includes go in front of the function (plus `#JTALIGN2`) and
  the jump table's INCLUDE_RODATA line is removed.
- Some rodata (strings/tables) was placed by splat in a function's `.s` before `.section .text`.
  When the C references it as `extern` symbols, `rodatasplit.py <func>` writes it to its own
  `D_xxxxxxxx.s` files and prints the `INCLUDE_RODATA(...)` lines to put right before the function
  (func_800195E8; otherwise "undefined reference to D_80010128").
- When the C uses a string LITERAL instead, no INCLUDE_RODATA is needed: the compiler emits the
  string itself (func_80056A54 `"%03d"`). A string literal can replace a function-own rodata string
  (D_800102C8) as long as the rodata order stays the same; the full build check confirms it.
- printf strings: reference the INCLUDE_RODATA symbol (`extern char D_80010030[];`).
- For a function with its own rodata string and a 4-aligned jump table, put the INCLUDE_RODATA line
  at the top of the jtinsert draft (before the marker).
- A shared file-level constant copied into a local: see
  [Local initialised data](#local-initialised-data).

### How original rodata maps to C

#### Local initialised data

GCC 2.6 emits a local initializer to `.rodata` while it compiles the
function, i.e. exactly where the decompiler saw a named rodata block being copied into a local
(`t = g_EffectTintScales;`).

- Examples: `effect_behavior_init_scale` `u8 t[10]`, `weapon_damage` `s16 t[2][3]` (two users share
  one constant: GCC reuses identical constants in a file), `player` `u8 tbl[2][4]`, `player_weapon`
  `u8 tbl[6][2]` and `u16 t[4]`, `room` `u8 tbl[7][9]`, `entity_common` two
  `VECTOR v = {2000/5000, 0, 0}`, the start items in `enemy_path` `s8 a[5]` / `s8 b[7]`, `zombie`
  `SVECTOR probe_neg/pos = {-+800, 0, 0}` (overlays, -O0).
- A local `SVECTOR v = { -600, 0, 0, 0 };` puts its data in `.rodata` and copies it with
  `lwl/lwr/swl/swr` (func_80106048). A local byte array with an initialiser (`u8 sp18[8] = {...}`)
  likewise (`arrinit.py`). A 6-byte local initialised from rodata right before the function's jump
  table is a brace-init local (`struct { u8 a, b; s8 c; } sp10[2] = {...}`); the compiler re-emits
  the rodata in place.
- **Shared file-level constants copied into locals:** `SVECTOR sp10 = D_801054AC;` where the data
  is an `INCLUDE_RODATA` block that another (assembly) function also references (stage1
  func_8010EA7C, func_8010DCF0). During matching that `INCLUDE_RODATA` line was kept and
  `extern SVECTOR D_801054AC;` declared in the function.

#### String literals

A string literal used in the function right after where the piece sat becomes
a plain literal: the six memory card title strings of `memcard_build_file_title` (Shift-JIS, written
with octal escapes, e.g. `#define MC_TITLE_JILL "\202\161..."`), "_Nothing_" (`itembox_draw`),
"Yes  No" (`update_message_display`).

#### File-scope const data

Used where something else needs the name: `s_SumError` (printed by
`cd.c`, lives in `memcard.o`), `s_Empty`, `s_SaveFilePattern`, `s_SaveFileName`, an unreferenced ""
(`s_UnusedEmpty`: 4 zero bytes at 0x80010068 that no alignment explains), `s_bu00` (`door.c`, used
by `memcard.c`), `zombie_hit_threshold_hard_tbl`.

### Rules that must hold for data

- **subalign 4** in all 13 yamls (see [splat configuration](#splat-configuration-and-gotchas)).
- **Statement order:** an initializer runs before every statement. `room_get_rdt_stage_index`
  clears `i` before the copy, so `u8 i = 0;` is declared before the table. `zombie_init` /
  `zombie_damaged` (-O0) compute `&thresholds` once before the copy (a dead `addu`), which only a
  statement before the copy gives, so that table stays a named const table copied by value
  (`ZombieHitThresholds`).
- **Jump table alignment:** overlay jump tables are made 4-aligned (Makefile `alignfix_for`); where
  Capcom's was 8-aligned, a zero word is emitted (see [Jump-table alignment](#jump-table-alignment)).
- **Test data pieces together:** a piece can fail alone only because the next one relies on its
  padding (the start items failed alone because `s_Nothing`, which has no `.align` of its own,
  relied on their padding; converted together they match).
- **File boundaries:** see [Splitting files](#splitting-files) for the 8-aligned rodata rule and
  `.space N` gaps.

---

## Declarations, types and casts

### GCC 2.6.3 rules for declarations

- **Block-scope `extern` types are per function.** Each function is compiled with *its own*
  block-scope `extern` type for a global; a different type for the same global in another function
  is only a warning. Only next to a *file-scope* declaration does a different block-scope type
  become an error ("conflicting types for ..."). This is why `declclean.py` picks the canonical
  type as: the file-scope type, else the most common plain type, else the most common alias type.
  GCC 2.6.3 only warns about conflicting block-scope declarations, which is also why drafts can
  carry their own declarations inside function bodies.
- **Conflicting declarations:** GCC 2.6.3 keeps the first type of a global when two declarations
  disagree (only a warning), which changes the code of later functions.
- **Implicit declarations persist:** GCC keeps an implicit function declaration for the rest of the
  file, so removing an alias in one function can change another function further down.
- **Unprototyped redeclarations:** if a function is defined with *narrow* prototyped parameters
  (`void f(u8 a)`), an older block-scope unprototyped declaration of `f` is treated as a different,
  local function and called as `f.NNN` (link error).
- **A function must keep seeing exactly the prototypes it saw in the original single file:**
  functions defined later in the file must stay unseen (implicit declarations behave differently
  from prototypes). See [Splitting files](#splitting-files).
- **K&R definitions:** Capcom often called functions without prototypes. An older `T f(void)`
  definition becomes K&R `T f()` when a caller passes arguments; matching sometimes needs a
  declaration that differs from the definition, such as `void f();` where calls pass extra
  arguments.

### Asm labels and per-function aliases

GCC 2.6.3 supports asm labels: `extern s32 D_X_s32 __asm__("D_X");` gives a second C name for the
same symbol (verified byte-identical). The project uses this for block-scope aliases such as

```c
extern u8 *g_RoomData_l __asm__("g_RoomData");
s32 f_l() __asm__("f");
```

which give a function a private declaration of a global or function, usually with another type
than the shared declaration.

- **Typed data aliases.** A function that needs a global with another type gets a private typed
  alias for the same linker symbol; its code is unchanged:

  ```c
  extern s8 D_X_s8 __asm__("D_X");
  ```

  The suffix is the type words, plus `p` per `*`, plus `a` for arrays (for example `_s8`, `_u8`,
  `_Entityp`; `g_DoorCmdCur_voidp` is the `void *` view of `g_DoorCmdCur`). These aliases are
  needed for the match: they reproduce the type each original function saw. Removing one can give
  "conflicting types" (for example where a stage `globals.h` declares the symbol with another
  type).
- **Function aliases.** Capcom's callers often saw a different prototype than the function's
  definition (for example `s32` for a function that is `void`). Such call sites use a per-function
  alias like `void pad_update_l(void) __asm__("pad_update");`. `funcaliasclean.py` replaces an
  alias by the real name only when the alias prototype has *exactly* the definition's types
  (definition earlier in the file: the alias line is dropped; definition later: an ordinary
  block-scope prototype with the real name). The remaining function aliases have a different
  prototype from the definition and are needed for the match; 23 of them are Sony functions
  (`VSync_l`, `open_l`, ...).
- Aliases of functions defined later in the same file cannot simply be removed: the implicit
  declaration would then conflict with the definition, far from the function at fault.
- Two aliases of the same global with different types do not alias each other for the scheduler
  (func_800404B8; see [Memory references](#memory-references-which-loads-may-pass-which-stores)).
- An alias name can also be an address expression (`__asm__("D_80093BB0+2")`,
  `__asm__("g_PacketArea+4")`); see [A symbol vs base + offset](#a-symbol-vs-base--offset).
- `aliastry.py` removes aliases where possible (see
  [Verifying a change](#aliastrypy-remove-per-function-aliases-where-possible)).

### Sony prototypes and block-scope declarations

- `include/psyq_types.h` (types only) is included by every `src/*/*.c`.
- `include/psyq_protos.h` holds the 187 Sony prototypes. `autodecomp.py --psyq` flattens it (cpp
  `-P`, because m2c can't parse preprocessor directives) into `build/m2c_context.h` for m2c, and
  compiles drafts with it.
- `integrate.py` copies the prototypes a new function calls **into its body**. They can't go at file
  scope: older integrated functions declare Sony functions with m2c's guessed types in block scope,
  and that is an error next to a file-scope prototype.
- Calling a Sony function through a cast does not work (func_800479A4; see
  [Calls, parameters and return types](#calls-parameters-and-return-types)).

### Declaration types and warnings

A cast between 32-bit types does not change the code, so missing casts (pointer/integer,
incompatible pointer types) can be added freely. Matching sometimes needs a declaration that
differs from the definition: `void f();` where calls pass extra arguments, or a per-function alias
with another type.

### Array and element types

- `g_KeyConfig` only matches as `u16[16]`.
- A byte index read `x[k]` uses the variable's DECLARED element type: through an `s8` / `char`
  pointer it reads a signed byte. Rewriting it with a `u8` pointer mismatches.
- `MenuWork` fields that were `s8` in one old view and `u8` in another (`char_flags`,
  `slot_count`, `has_radio`, `cursor`, `viewer_state`) compile identically as `u8`.
- Struct typedefs used in a function *signature* must be at file scope before the function
  (during matching: before the `INCLUDE_ASM` line; `integrate.py` keeps a draft's top-level
  `typedef` blocks there).

### GCC 2.6.3 language limits

- **No anonymous unions or structs** in C ("unnamed struct/union that defines no instances").
- **No `offsetof` builtin.** `(int)&((W *)0)->f` is a constant expression and is used as the field
  offset (also as an array size inside the member-access macro). The project macros are
  `FIELD_OFF(W, f)` and `PATH_OFF(W, path)` (`((int)&(((W *)0)path))`).
- **`sizeof` is unsigned** and can compile differently, so array strides are written as numbers:
  `ENTITY_SIZE 0x18C`, `JOINT_SIZE 0x7C`, `EFFECT_SIZE 0x84` (`include/re1_types.h`).
- **`abs()` at -O0** becomes a real call; see [Other compiler traps](#other-compiler-traps).

---

## Common m2c draft errors

m2c (run with `--valid-syntax`) gives a useful first draft, but its output has systematic errors.

- **Byte offsets on unknown arrays.** m2c's `M2C_UNK` is `s32` in valid-syntax mode, but m2c emits
  **byte offsets** on unknown arrays (`&D_X + off`), which gives wrong code. Example:
  `func_80047038` stores a word with a 48-byte stride instead of a byte every 12 bytes, and indexing
  came out scaled x4 (`sll 4` instead of `sll 2`). The tools therefore treat `M2C_UNK` and
  `M2C_UNK8` (a byte type for unknown arrays) as bytes, and `autodecomp.py` tries an `M2C_UNK8`
  variant. The same scaling error hits the 0x84-byte record table
  ([details](#the-0x84-byte-record-table-at-0x800c3024)).
- **Constants lost** (`x & x`, `x >> x`): rewrite from the asm's meaning; it usually matches quickly
  (func_8004AB78).
- **Two stack buffers passed to one call merged into one variable** (`CdControlB(com, res)`):
  declare separate arrays (main func_8005D9E4).
- **Stale registers passed as extra call arguments** (`f(x, y, temp_a2)` for a 2-parameter callee):
  drop them (func_800258F0; `argtrim.py`). A call to a callee taking no arguments is `f()`, not
  `f(r)` (func_8002B8A4). Invented arguments from leftover registers:
  `func_80027D08(0x800C3000, 0x3FFFFFFF)` was `func_80027D08()` (func_80032C40).
- **Dropped arguments:** in if/else pairs (`var = X; ... f();`) write the call in both branches
  (func_8001930C). Register arguments that pass straight through are dropped: in
  func_800519B8/func_80051A14 a0 went unchanged to
  `func_80051ABC(a0, r[0]+o, r[1]+o, r[2]-o, r[3]-o)` (5th argument on the stack) and m2c showed no
  call arguments at all; func_80026710 had a parameter passed straight through in a1. Check the
  `aN` set-up before every `jal`.
- **Wrong access widths:** byte stores `*(s8 *)X = n` are often wrong: check the original opcode for
  every narrow store/load m2c prints (sb/sh/sw, lb/lh/lw).
- **Miscounted loop iterations** in wait loops (func_80054FB8).
- **`fold()` shapes written back:** m2c shows the folded form of an expression; write Capcom's (see
  [Front end and fold()](#front-end-and-fold)).
- **Other m2c habits** covered in the per-level rules: dropped dead stores to locals, split stack
  structs, invented and dropped parameters, `t = x; x = t - 1` post-decrement tests, lost `--x`
  values, goto-built loops and `goto` pairs, table-call arguments that are only the table address,
  split engine calls, hoisted reads ([-O0 rules](#rules-for-the-overlays--o0),
  [-O2 rules](#rules-for-the-main-program--o2)).

---

## The draft and search tools

All tools are in `dualshock/tools/` unless a path says otherwise, and run with the project venv
python from `dualshock/`. The matching-era tools that assume a single `src/main/capcom.c`
(`autodecomp`, `refine`, `permbatch`, ...) do not work on the split tree. `integrate.Target` takes
`main/<file>` for the per-file tools (`declclean`, `entityfields`, `playerfields`, `funcaliasclean`,
`test_integrate`).

### The bulk pipeline

1. **`autodecomp.py --target T`** makes m2c drafts and repairs them.
2. **`integrate.py --target T`** puts all `match` drafts into the C file.
3. **`refine.py --target T`** hill-climbs over source mutations.
4. **`propagate.py`** reuses matched C for identical copies of a function in other STAGE
   overlays. Across STAGE1-7 there are only about 577 distinct function shapes.

Results go to `build/auto/<T>/results.json` and `build/auto/<T>/c/`. A re-run only replaces a
function's stored draft if the new one is at least as good (ranking: match > diff (smaller is
better) > size > cc-fail).

### autodecomp.py: m2c drafts and automatic repairs

Runs m2c (`--valid-syntax`, with the target's shared rodata files for jump tables), compiles the
draft *alone* with the real toolchain, and compares it with the original. Variants tried:

- the base output;
- **K&R fix-ups** if it doesn't compile: unprototyped `s32` declarations, unprototyped function
  pointers, `extern M2C_UNK func_X` -> function, `*(void *)` -> `*(s32 *)`;
- function-pointer table dispatch `*((idx * 4) + &D_X)(..)` -> `D_X[idx](..)` with
  `extern s32 (*D_X[])();` (this cut STAGE1 compile failures from 33 to 4). It also accepts tables
  splat mislabeled as `func_X` (e.g. `&func_8008F89C`);
- an `M2C_UNK8` variant (byte type for unknown arrays);
- an explicit `return;` (an `-O0` idiom).

Options and repairs:

- `--psyq` flattens `include/psyq_protos.h` into `build/m2c_context.h` (see
  [Sony prototypes](#sony-prototypes-and-block-scope-declarations)).
- `--m2c "<flags>"` passes extra flags to m2c; a stored draft is only replaced by a better one.
  (`--reg-vars s0,...,s7,fp` was tried on the main program's worst drafts with no improvement.)
- **`errfix`** (error-driven repair) handles drafts that still don't compile: it reads the
  compiler's messages, declares m2c's placeholders (`sp18`, `saved_reg_s4`, ...) as locals, and adds
  `(s32 *)` casts to dereferences of plain integer expressions, for up to 10 rounds. It runs on the
  base output even when the K&R fix-up changed nothing. Repairs:
  - casts `*var` on integer variables;
  - turns `*(void *)ADDR` values into 32-bit accesses; `*0xADDR` -> `*(s32 *)0xADDR`;
  - a stack struct used as a number -> `(*(s32 *)&spXX)` on that line;
  - pointer subtraction between unrelated types -> `(s32)` operands;
  - table calls through a pointer variable `*((i * 4) + D_X)()` -> `((s32 (**)())D_X)[i]()`.
- **`align_fix`** turns misaligned constant-address accesses into the widest access the alignment
  allows (see [Compiler bugs and crashes](#compiler-bugs-and-crashes)).
- Writes retry on transient Windows file locks.

`errprobe.py [--target T] func ...` lists the compile errors left in a draft after all automatic
repairs, with the draft line each one points at (or the raw tail when cc1 crashed).

### refine.py: mutation search

Hill-climbing over source mutations, scored by an exact word diff against the original. Each
candidate is **linked at its original address** (symbols from the built ELF + splat's auto files,
rodata at its original address).

- **Score with masked branch targets:** `refine.distance` compares branch and jump instructions
  without their target field (BEQ/BNE/BLEZ/BGTZ/REGIMM offsets, J targets; JAL is kept). Otherwise
  a fix that changes the code size moves every later branch target, and real improvements score
  *worse* (a restored call argument went from 9 to 12). Only byte-identical code scores 0 (masked
  0 but bytes different = 1). `nearmiss.py`, `mutprobe.py` and `distprobe.py` use the same score.
- **Sideways moves:** when no mutation improves a draft, refine moves to an equally good candidate
  it hasn't seen (up to 4 times per function) and continues from there; the best version is saved.
  Some fixes need two mutations where the first alone doesn't change the score.
- Every candidate drops unused locals (at `-O0` they get stack slots) and unprototypes function
  pointers.
- `RE_GCC` selects another compiler build (see [Which compiler](#which-compiler-gcc-263)).

Base mutations:

| Mutation | Effect |
|---|---|
| field form | per-access struct/pointer field form |
| `fixaddr` | fixed-address access: `D_X` -> `(*(T *)0xX)` or `((T *)0xX)` |
| `index` | `*(&D_X + (E * 4))` -> `((s32 *)&D_X)[E]` (and 2/1-byte variants) |
| return / casts / signedness / access width | `return;`, casts, signedness, load/store width |
| pointer element type | stride errors like `addiu 16` vs `4`; `extern M2C_UNK` type |
| inlining | inline one/all single-use temps, forward substitution |
| call arguments | drop call arguments, call-through-assignment |
| increments | `X += 1` -> `X++`, and cast-aware `++`/`--` (`F = (u8) (F + 1);` -> `F++;`) |
| compound / chained assignment | `X op= E`; `Y = X = E` |
| `dropjump` | drop one `break`/`return` (including a `return;` in the last `case`) |
| `commute` / `swap` | `A op B` -> `B op A`; two adjacent statements |
| declarations | declaration order, never-assigned variable -> another local, redundant masks |
| compound step | drop table-call args + substitute + inline + K&R, to escape local minima |

Further mutations (signature = how to recognise the need in the original):

| Mutation | Original idiom | Machine-code signature |
|---|---|---|
| `signall` | flip every `(t *)` cast of one type at once | |
| `fixaddr-fp` | function-pointer tables at a fixed address | `addu at,v0,at` order |
| `ptradd` | pointer additions m2c scaled twice | |
| `padlocal` | an unused local (-O0 slot); at -O2 an unused local *array* (`s32 unused[2]`, `[4]`, `[8]`, `[12]`): -O2 drops unused scalars, not arrays | frame 8/16/32-48 bytes bigger; a leaf function with an `addiu sp,sp,-8` frame it never uses |
| `postdec` | `if (x-- == 0)`-style tests from `t = x; x = t - 1; if ((s16) t == 0)` | extra stack copy of the old value |
| `ternary` / `ternary2` | `x = c ? a : b` (m2c: if/else into a temp, then `x = temp`); `v = (C) ? A : B;` into a local still used later | temp stack slot; `li; j; li; sw` into one stack slot (func_8011F11C) |
| `fieldoff` | a byte field one off (m2c misplaced a delay-slot load) | `lbu a0,1(v1)` vs `0(v1)` |
| `signtest` | `((s16) x < 0)` instead of m2c's `(x & 0x8000)` | `sll 16; bgez` vs `andi 0x8000; beqz` |
| `decr` (extended) | `p->x--` where m2c gave the left side another field type (`u16` at odd offset 0x87) | `sh` at the wrong offset + missing base reload |

Mutations described with their rules elsewhere in this guide: `absb`, `addbreak`, `argfix`,
`argval`, `assigncond`, `asmtype`, `byteoff`, `byteoff2`, `byteoff-asm`, `callassign`, `commute2`,
`copyprop`, `deadinit`, `decr`, `dropbreak`, `dropdefault`, `earlyexit`, `exttype`, `fpret`,
`gotoor`, `gotowhile`, `invertret`, `localuse`, `movereturn`, `mul2shl`, `opassign`, `opexpand`,
`partstore`, `postinc`, `predec`, `retconst`, `shl2mul`, `splitadd`, `stackarr`, `structcopy`,
`structinc`, `swapif`, `sym2num`, `symaddr`, `symarr`, `symptr`, `symsplit`, `uninit`, `widetmp`.

### integrate.py

Puts finished C into a source file, builds, and reverts culprits on compile errors or byte
mismatches until the target matches again.

- **Check build:** deletes the target's `.i/.s/.ms.s/.o` first (see
  [Stale objects](#stale-objects)). Builds retry on transient Windows file locks, and so do writes.
- **Culprit search:** tries every error line and prints the reason for each revert; the status file
  is saved on success too. Integrating several drafts at once can revert innocent functions; retry
  those one by one.
- **Function search:** `DEF_RE` finds functions with any name, and `integrate.function_spans` also
  finds a function whose signature continues on the next line (`str_open`, 0x8005CE44, is the only
  one).
- **Declarations it moves and copies:**
  - moves declarations into the function body;
  - copies the typed globals and Sony prototypes the new function uses into its body;
    `compiler_matrix.preamble_for(code)` adds only the declarations a draft doesn't already make
    itself (old drafts declare Sony functions with m2c's guessed types);
  - top-level `typedef` blocks and `#define`s of a draft are kept at file scope in front of the
    function (needed when the struct type is used in the signature). Other draft file-scope
    declarations are moved into the function;
  - the draft's file-scope prototype of a function defined in the same file is dropped (e.g.
    `u8 func_80126B70();`), so the call defaults to `int`; add a block-scope declaration in the
    draft when the return type matters.
- **Declaration repairs:**
  - **retypes older block-scope declarations** that conflict with a newly integrated definition
    (`M2C_UNK f();` -> `void f();`, unprototyped); the full build verifies the result;
  - an older function's `extern M2C_UNK func_X;` (address use) is retyped to `void func_X();` when
    func_X gets defined; a declaration of an already-defined function used as an address is
    dropped;
  - an older `T f(void)` definition becomes K&R `T f()` when a new caller passes arguments;
  - an older block-scope declaration of a function that becomes defined with narrow prototyped
    parameters is deleted, not retyped (see the `f.NNN` quirk in
    [GCC 2.6.3 rules for declarations](#gcc-263-rules-for-declarations)); the repair also applies
    to functions defined earlier;
  - every older caller's own block-scope declaration of a newly defined function becomes a private
    alias (not only for narrow parameters): a caller's `M2C_UNK f(M2C_UNK, u16);` must keep its code
    (func_8003726C); this is `integrate.alias_old_decls()`, together with type-conflict aliases and
    prototype aliases (see [Asm labels and per-function aliases](#asm-labels-and-per-function-aliases)).
- **Asm aliases in hand-made code:** hand-made code declares globals as block-scope asm aliases
  (`extern void *D_X_l __asm__("D_X");`). integrate.py treats these correctly:
  - name searches and renames skip string literals (`STR_RE`, `code_only()`, `sub_code()`), so the
    `"D_X"` inside `__asm__("...")` is neither a use nor renamed;
  - `declared_names()` counts the function's own block-scope declarations and asm-alias targets as
    declared.

  Without this, integrate.py added `extern Entity *D_X;` from `include/re1_externs.h`, which
  conflicted with the file's `void *D_X`, triggered the rename-to-alias step, and rewrote the
  string to `__asm__("D_X_Entityp")`, an undefined symbol at link time.
- **Rodata pads:** adds `RODATA_PAD` after a function's last jump table where needed
  (`integrate.needs_rodata_pad`). It does not handle the `#JTALIGN2` marker; use `jtinsert.py`.
- **Drafts must not carry file-scope prototypes of other functions.** A file-scope
  `void f(void *, u8);` in a draft changes every later caller in the same C file; drafts that
  measured 0 alone mismatched in the full build. Use block-scope `__asm__` aliases inside the
  function instead. integrate.py reverts such functions as "mismatch"; integrating one at a time
  shows which ones are affected. File-scope `extern` DATA declarations in a verbatim draft can
  conflict too (`conflicting types for D_800CAB14`, or a file-scope `extern MATRIX D_800BC4C8;`
  clashing with a later block-scope declaration); use block-scope `__asm__` aliases there as well.
- **If integrate.py reverts a distance-0 draft as "mismatch"**, try a verbatim splice: replace the
  INCLUDE_ASM line with the draft text and run `make`. func_80035EB4 (block-scope externs incl.
  `extern u16 D_800BAE3E;`) matched that way while integrate.py's declaration rewriting broke it.
  Integrate one at a time; a batch can fail through interactions. A verbatim splice also needs
  INCLUDE_RODATA lines for rodata symbols the draft references (func_8005F2D4 needed D_80010964).
- **Overlays:** `integrate.py --target stage4` broke the build (undefined `D_800CAB14_Entityp`:
  existing block-scope aliases were mangled); a verbatim splice of the draft in place of INCLUDE_ASM
  plus a rebuild without `stage4.o` worked. Use verbatim splices on overlays.
- Drafts that use a file-level typedef of the target C file (e.g. `Str6`) need that typedef only for
  sidediff.
- After integrating, always run a plain `make` once and confirm "All 13 executables match".
- Regression test: `test_integrate.py` (see [Regression tests](#regression-tests)).

### jtinsert.py

Integrates functions with 4-aligned jump tables (`#JTALIGN2` / `#JTALIGN2PAD`, plus a `.word 0`
pad when the table ends in one). It inserts the draft verbatim, runs `integrate.alias_old_decls()`
and requires "All 13 executables match"; it does not add a marker when the draft already has one.
(A later caller's `M2C_UNK func_800544E0();` conflicting with the new `void` definition once broke
the C file while the build still "matched".) Its `fresh_make` deletes the touched objects first.

### Twins: propagate.py, port.py, twinhunt.py

- `propagate.py` reuses matched C for identical copies of a function in other STAGE overlays
  (symbol references are paired up in order, then the copy is verified at its own address). It only
  stores the copies as `match` drafts: run `integrate.py` per stage afterwards.
- `port.py src_t src_f dst_t dst_f out.c` copies a matched function to a near twin (symbols paired
  in order); `twinhunt.py` searches for near twins automatically.

### Diagnosis tools

| Tool | Use |
|---|---|
| `progress.py` | Decompiled functions/bytes per target |
| `funcdiff.py [func ...]` | Without arguments: the first *shifted* function (the one before it changed size) plus the differing functions. With names: side-by-side disassembly, original vs built |
| `try_c.sh <file.c> [flags]` | Compile a snippet the way the Makefile does and print the assembly. `GCCVER=2.6.3/2.7.2/2.8.1` picks the compiler (run via `buildenv.sh`) |
| `compiler_matrix.py <file.c> [-v]` | Compare functions against the original for several compilers/flags; `-v` shows the differing instructions |
| `compiler_matrix_sample.c` | The 15 test functions: a regression test for the compiler choice (all match on 2.6.3) |
| `file_boundaries.py` | Maps splat's rodata split hints to the code ranges that use each rodata chunk (estimating original source files) |
| `nearmiss.py --target T --max-dist 5` / `[--only f --file x.c]` | Lists near-miss drafts and the exact differing instruction runs of the closest drafts; `--file` diffs a hand-edited version instead of the stored draft |
| `sidediff.py --target T func [--file my.c] [--context N]` | Original and built instructions side by side with addresses around each difference (shows where an extra instruction sits, which nearmiss hides) |
| `mutprobe.py <target> <func> [kinds]` | Distance of every mutation of a stored draft |
| `distprobe.py <target> <func> <file.c>` | Distance of a hand-edited draft, linked like refine does; its assembly stays in `build/refine/<target>/work/<func>/hand/m.s` |
| `intdiag.py <overlay> <func>` | Inserts one draft, builds, and lists the differing address ranges of the whole executable (always restores the C file) |
| `errprobe.py` | Compile errors left after automatic repairs |

`funcdiff.py` and `compiler_matrix.py` cover diffing; objdiff (`objdiff.json`) is not set up.

Caveats of the distance:

- The refine distance ignores `#JTALIGN2`, so drafts of functions with a 4-aligned jump table
  measure 1 too high; a "distance 1" jump-table function may already be 0 once the markers are
  applied the way `jtalign.awk` does (func_80060078). Count every `#JTALIGN2` / `#JTALIGN2PAD`
  marker when measuring drafts with several 4-aligned tables.
- sidediff does NOT compare jump-table contents (rodata): an m2c `case 1: case 3: ... case 2:`
  merge compiles to distance 0 and still fails integration (func_8003E030).
- A permuter score is not our distance (func_8003FC6C: score 5 = distance 1).

### Near-miss and hand-matching workflow

- **Near-miss loop:** run `nearmiss.py --target T --max-dist 5` to see the exact differing
  instructions of the closest drafts, find the missing mutation or tool bug, fix it, and re-run.
  Each fix typically solves dozens of functions. The `fixaddr`, `index`, `X++`, `dropjump`,
  `commute` and `swap` mutations were found this way.
- **Hand matching:** `nearmiss.py` -> read the draft and the original `.s` -> hand-edit and run
  `distprobe.py` until 0 -> write the fix as a mutation or rewrite -> `mutprobe.py` -> refine run
  over all drafts.

### Rewrite tools and batch runs

Most rules found by hand exist as source-to-source rewrites in `dualshock/tools/<name>.py`
(`<in.c> <out.c> [flag]`).

- `trybatch.py "<python> tools/<transform>.py" <targets>` applies any `in.c out.c` transform to
  every unfinished draft, measures, and stores only drafts that get closer.
- `chain.py in out a.py b.py:--flag` runs several rewrites in a row (some functions only match with
  two or three rewrites together).

Rewrites described with their rules in this guide: `argcopy`, `argtemp`, `argtrim`, `arrinit`,
`blockcopy`, `bytearr`, `callmerge`, `chainfix`, `cmpswap`, `condadd`, `constmove`, `deadlabel`,
`defaultret`, `divfix`, `dropunused`, `dupargfix`, `elsecase`, `emptycase`, `emptythen`, `entsub`,
`fakearg`, `foldarms`, `gotoloop`, `idxwrap`, `incfix`, `litbase`, `loopblock`, `matcopy`, `modfix`,
`narrowparam`, `negmul`, `nestdec`, `orthen`, `plusone`, `ply84`, `postdecse`, `ptrinc`, `ptrstep`,
`randsum`, `rectable`, `retinto`, `scratch`, `selfadd`, `selfplus`, `sfield`, `stmtexpr`, `switch1`,
`switchtemp`, `tempdec`, `u16sub`.

Other rewrite tools: `inlinetemps` (`--pfield`, `--abs`), `x86fix`, `preinc`, `vcopy`, `pfieldadd`,
`pfieldarg`, `dowhile`, `whiledec`, `condassign`, `misalign`, `ptrlocal`, `elseret`, `x84byte`,
`decsign`, `subfix`, `absfix`.

### Sweep and search tools

| Tool | What it does |
|---|---|
| `stmtsweep.py` | Mark a block with `//STMTS` ... `//END`, alternatives with ` \|\| `; tries every order. Most of the script-handler family was matched with it. |
| `movesweep.py one <func> <in> <out>` | Hill-climbs by moving single statements inside runs of simple statements. |
| `movesweep.py batch --jobs N` | The same for every pending draft. Also tries other widths for narrow loads/stores (`--no-widths` turns this off). |
| `scratch.py` | Rewrites scratchpad accesses into the `M2C_SFIELD(0x1F800000, ...)` struct form (for trybatch). |
| `scratchmix.py <func> <draft> <out>` | Tries literal/struct form per scratchpad address (2^n combinations); `scratchmixbatch.py` runs it over drafts. |
| `litbase.py` | Rewrites m2c's table-index shapes into literal-address or symbol tables. |
| `ply84.py` | Rewrites m2c's accesses to the 0x84-byte record table at 0x800C3024 (the `Effect` type) into struct-member form, for every type and with an outer cast. |
| `sfield.py [all\|stores\|loads\|base:<expr>]` | Rewrites pointer accesses into `M2C_SFIELD` form (trybatch-able). |
| `idxwrap.py` | Wraps an index in `({ })` (trybatch-able). |
| `argtrim.py` | Trims stale extra call arguments for callees already defined in C. |
| `padsweep.py` | Tries the rodata pad after every function with a pad-ending jump table, keeping it only when the build still matches. |
| `permharvest.py --target main` | Measures the 3 lowest-score outputs in every `build/permuter/<target>/<func>/` and stores one that beats the draft. |
| `rodatasplit.py`, `jtinsert.py`, `integrate.py`, `jtalign.awk` | See [Data, rodata and alignment](#data-rodata-and-alignment) and the sections above. |

**movesweep only measures bytes.** A non-zero result can move a statement above the one that sets
its inputs (func_8005E99C: `d = ... cur ...` ended up before `cur = ...`; func_800560E8 read `n`
before assigning it), and it can reorder dependent statements across `do {} while (0)` wraps. Its
width variants can change semantics (u16 -> s8). Only a distance-0 result is guaranteed equivalent;
check stored non-zero drafts for use-before-set before building on them.

### decomp-permuter (`permbatch.py`)

For what rule-based refine can't find (mostly -O2 register allocation in the main program).
`permbatch.py` runs the permuter per function with a time limit and exports exact matches, checked
byte for byte (the permuter itself ignores branch targets). Results look like macro-expanded C with
permuter tricks (`new_var = 4;`) and need tidying; rebuild after every layout change of duplicated
code (see [line layout](#jump-optimization-and-cross-jumping)). `permsetup.py export` marks
function prototypes `/* extern */` like m2c, so `integrate.py` treats them the same way. Permuter
exports drop typedefs of the hand draft (func_8005E0E4 lost `Ent32`): re-add them before
integrating. On the main program, long permuter batches rarely find anything; the rule-based
approach is far more productive.

### Speed

`compiler_matrix.build` runs maspsx in-process (`MaspsxProcessor`, with the settings of
`--aspsx-version=2.56`), and `text_and_syms` / `refine.distance` read the ELF directly instead of
calling objcopy/nm/objdump. The objects are byte-identical, and each check is 3-6x faster.

### Running jobs safely

- Run integrations/builds of one target strictly one after another (one chain script); two at once
  race on the build objects and give a false MISMATCH.
- A permuter stopped at its time limit must be killed as a process tree (`taskkill /T /F`), or its
  worker processes keep running. `permbatch.py` does this.
- Transient Windows file locks (antivirus) can break builds and writes, so `integrate.py`'s build
  and `autodecomp.py`'s writes retry.
- `compiler_matrix._context_decls` publishes its dict only when it is complete; a race there once
  crashed a refine run.
- Don't write scripts, headers or C containing backslash escapes through shell heredocs or `sed`;
  use an editor. Writing `\` + newline through heredocs mangles macros. A literal backspace (0x08)
  once ended up in `integrate.py` instead of `\b`, so `extern ... func_X;` declarations of defined
  functions were never dropped (drafts using `&func_X` failed with "redeclared as different kind of
  symbol"). Likewise, raw newlines inside the `__asm__(".section .rodata\n\t.word 0\n\t.text")`
  string silently produced a 4-byte-shorter build.

---

## Compiler bugs and crashes

### The LP64 crash in cc1

The project's GCC 2.6.3 `cc1` is a 64-bit MSYS2 build. The 1994 GCC sources call some
pointer-returning functions without a prototype (`negate_rtx` in reorg.c, `replace_regs` in
loop.c/jump.c, `remove_death`, `label_rtx`, `expand_expr`, `emit`, ...). Implicit declarations
return `int`, so the pointer was truncated to 32 bits; with a heap address above 2 GB, cc1
**segfaulted silently** (empty `.s`, exit code 139, and sidediff reports "does not compile"). It
showed up on `do { i--; ... } while (...)` loops in the main program, which turned out to be
exactly Capcom's source (func_8001A248, func_80055F24).

- The crash was located with GCC's pass dumps (`-drjcsLftlgRdS`: the last, empty dump is the
  crashing pass, here `.dbr`) plus a function-entry trace.
- The fix is the top-level `tools/gcc263_lp64_decls.py`, run by `tools/build_old_gcc.sh 2.6.3`. It
  adds declarations returning `struct rtx_def *` / `union tree_node *` after `#include "config.h"`
  (insn-emit.c is generated first). After a full rebuild with the fixed cc1 all 13 executables
  still match: the fix only turns crashes into normal output.
- If a variant "does not compile" but cc1 prints no error, check for a crash
  (`cc1.exe ... ; echo $?` = 139) instead of assuming the C is wrong.

### Misaligned constant-address accesses crash the optimizer

An access wider than the address' alignment (`*(s32 *)0x1F80038E`) makes cc1 segfault without a
message at -O1/-O2. `autodecomp.align_fix` turns such accesses into the widest access the
alignment allows.

### Other compiler traps

- **`abs()` at -O0** becomes a real call (link fails); use `__builtin_abs()`.
- **`f.NNN` link errors:** an older block-scope unprototyped declaration of a function later
  defined with narrow prototyped parameters is treated as a different, local function (see
  [GCC 2.6.3 rules for declarations](#gcc-263-rules-for-declarations)).
- **Conflicting declarations only warn** and keep the first type, silently changing later code.
- **CRLF line endings** make the gcc 2.6/2.7 `cpp` fail with "stray '\'" (see
  [The C pipeline](#the-c-pipeline-dualshockmakefile)).

---

## FAKEMATCH

A match that needs a compiler-only trick with no natural C equivalent carries a
`/* FAKEMATCH: ... */` comment explaining why; `grep FAKEMATCH` lists them. There are exactly two:

- **`player_ctrl_behavior_walk_back`** (func_8003E228, `src/main/player.c`): an empty
  `__asm__("");` before `next:`. It is an insn that emits no code, and it keeps jump2's
  cross-jumping from merging the two `p += ENTITY_SIZE;` (`p += 0x18C`) statements; the original
  keeps both. No natural C does this in GCC 2.6.3 and the shape occurs exactly once in the whole
  binary, so the original likely had an invisible construct there (see
  [Jump optimization and cross-jumping](#jump-optimization-and-cross-jumping)).
- **`menu_draw_health_bar`** (func_80056FC0, `src/main/status_menu.c`): a volatile store
  `*(volatile u8 *)&cnt = ...;`. CSE must not record it, so the next read of `cnt` is a real reload
  (original: `sb v0,16(sp); lbu v1,16(sp)`; see [CSE](#cse)).

Replace them if natural C is found.
