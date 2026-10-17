# Working on the decompilation (`dualshock/`)

The `dualshock/` folder turns the PS1 program of **Resident Evil Director's Cut Dual Shock (USA)** back
into C source code that compiles to exactly the original bytes. It rebuilds all 13 program files
**byte for byte**:

- `SLUS_007.47`, the main program
- the 12 overlays in `PROG2/`: `LOGO`, `PROLOGUE`, `TITLE`, `SELECT`, `ENDING`, `STAGE1`–`STAGE7`

Every function and all data is C (no assembly is included anywhere), compiled with the compiler generation
Capcom used (GCC 2.6.3 for PlayStation). How to set up and build is in the [README](../README.md); this guide
covers the tools, the code conventions and the folder layout. **Paths in this guide are relative to
`dualshock/`**, and the commands run from there.

The progress report: `../tools/external/venv/Scripts/python.exe tools/progress.py`.

## Changing a function

1. Edit the C. The original assembly of every function is in `asm/<program>/nonmatchings/<file>/<name>.s`
   (written by `make split`), for comparison.
2. Run `make`. If it prints `OK` for all files, the C still compiles to exactly the original bytes.
   Otherwise `python tools/funcdiff.py <name>` shows original vs. built side by side.
3. How to write C that GCC 2.6.3 turns into the original code: [matching_guide.md](matching_guide.md).

## The decomp tools (in `dualshock/tools/`, run from `dualshock/` through `../tools/buildenv.sh`)

| Tool | What it does |
|---|---|
| `autodecomp.py` | Makes a first C draft for every unmatched function (m2c + automatic repairs) |
| `refine.py` | Tries small rewrites of a draft until it compiles to the original bytes |
| `integrate.py` | Moves matching drafts into the C files and checks the build |
| `rename.py` | Gives a function/global a real name everywhere (code, aliases, symbol files, header specs, assembly), re-splits and checks the build; undoes everything on failure |
| `litaddr.py` | Names the RAM addresses the code has to write as numbers (`config/addrs.txt` -> `include/re1_addrs.h`): `*(u8 *)0x800C8660` becomes `LIT(u8, g_StageId)`, `((Effect *)0x800C3024)` becomes `LITP(Effect, g_EffectPool)`, a bare number `ADDR_name`. The macros expand to the same code, so the match can't break; `unnamed` lists what is left |
| `rawfield.py` | Hands the raw byte-offset accesses (`*(s32 *)((u8 *)e + 0x34)`) to the struct tools: `to` turns them into the equivalent `OFFSET_DEREF`, `localview.py`/`structview.py apply` name the ones their specs know (`ENT(e)->pos_x`), `back` returns the rest to raw form |
| `fieldrename.py` | Renames struct fields everywhere, guided by the compiler: renames the definition (spec / typedef), then fixes each line the compiler reports as "no member named old"; a clean build must match, else everything is restored |
| `hunkcheck.py` | Verifies source edits per function: rebuilds the changed files, compares each function's assembly with a saved baseline and rolls back the edits of every function whose code changed or that gained warnings (keeps its baseline and state in `../temp/warn/`) |
| `mmfix.py` / `castfix.py` / `aliasren.py` | The warning fixers: declaration conflicts (block-scope prototypes get the definition's type), casts to the declared type at assignments and calls, renaming one function's declaration to an `__asm__` alias |
| `formtry.py` | Rule-driven readability rewrites (regex -> replacement) verified per function: rewrites every site, builds, and where a function's bytes change retries one site at a time, keeping every rewrite that matches (used for `EFFECT->`, `EFFECT_AT`, the player / MenuWork struct forms) |
| `splitfile.py` | Splits a main-program topic file in two at a function boundary (optionally renaming the first part): prototypes / declarations for the new file, yaml subsegments, re-split + clean build, restore on failure |
| `ovlrename.py` | `rename.py` for overlay symbols, including the shared enemy code in `src/shared/` (all stage symbol files at once; evidence appended to `config/names_overlays.txt`) |
| `aliastry.py` | Removes per-function `__asm__` aliases wherever the real name compiles to the same bytes: per function all at once, then one at a time, every step verified by a build (`funcaliasclean.py` / `declclean.py` only handle identical types) |
| `declclean.py` | Moves the per-function `extern` declarations of globals to one block at the top of a C file (keeps aliases whose type differs); reverts any function whose bytes change |
| `workcopy.py` | `create`/`diff`/`apply` a private copy of dualshock/ (`../work_<name>/`) for parallel work, and merge its changes back with a full build check |
| `splitplan.py` / `splitc.py` | Split one program's big C file into topic files without changing a byte: `splitplan.py` checks/moves the file boundaries of the main program (8-aligned jump tables), `splitc.py [--target stageN] plan.txt` writes the files, `globals.h`, the yaml and the rodata piece folders; `--pad` adds the original padding between files |
| `sharetwins.py` | Enemy code that several stages contain (zombie is in six) is compiled from ONE file in `src/shared/`; each stage's file becomes a small wrapper. Tries every stage's text, keeps what matches; safe to run again |
| `enemywork.py` | `survey` shows how an enemy file uses the entity bytes 0x16C.. (its own "work area"); `apply` turns them into a named struct from a spec in `config/enemy_work/` (e.g. `ZOMBIE(g_CurrentEntity)->hit_count`), checks every function's bytes in every stage that builds the file and falls back per function |
| `playersyms.py` | Turns the leftover separate symbols inside the player object (`D_800C51AE` = `hit_state`, any type) into `g_PlayerEntity` fields, with casts where the old code used another type; checks every function in every program that builds the file and keeps the old text where the bytes change (some -O2 main-program cases) |
| `entityas.py` | The remaining `g_CurrentEntity` field macros (fields read with another type, pointer-arithmetic form) become named fields: `g_CurrentEntity->f` with a cast where it compiles the same, otherwise `ENTITY_AS(e, f, T)` / `ENTITY_FIELD(_AS)`; checked per function in every program |
| `structview.py` | Gives a struct to data reached through one pointer or global, from a spec in `config/structs/` (e.g. `ISCREEN->cursor_row` for the passcode panel); writes `include/re1_views.h` and rewrites the code, checked per function |
| `localview.py` | Names fields reached through local pointer variables, parameters and expressions, from a spec in `config/locals/` (`JOINT(sp18)->world.t[0]`, `ENT(sp14)->health`). Spec keys: `source` (every assignment must match; also per use, for variables m2c reuses), `param <function> <variable or expression>`, `expr` (base expressions), `stride` (arrays), `noaddr` (leave `p + N` alone), `file` / `exclude` (path regexes). Checked per function |
| `test_integrate.py` | Regression test for `integrate.py`: puts already-finished functions back to assembly, integrates them again and checks the build (always restores everything) |
| `integrate_retry.py` | Retries, one at a time, matches that a batch integration reverted |
| `propagate.py` | Copies a matched stage function to its identical twins in the other stages (then run `integrate.py` per stage) |
| `port.py` / `twinhunt.py` | Copy a matched function to a *near* twin in another stage (symbols paired in order) / search all stages for near twins of matched functions and try them |
| `permbatch.py` / `permsetup.py` | Runs decomp-permuter (random rewrites) on drafts that are almost right |
| `nearmiss.py`, `sidediff.py`, `mutprobe.py`, `distprobe.py`, `errprobe.py` | Show why a draft doesn't match yet (for finding new rewrite rules); `sidediff.py` puts original and built code side by side |
| `trybatch.py` | Applies one rewrite to every unfinished draft of a program, keeps only the drafts it improves |
| rewrites for `trybatch.py` | `callmerge`, `chainfix`, `selfadd`, `tempdec`, `divfix`, `incfix`, `dupargfix`, `dropunused`, `bytearr`, `elsecase`, `emptythen`, `switch1`, `switchtemp`, `retinto`, `deadlabel`, `subfix`, `absfix`, `postdecfix`, `argfix_feb38`, `condadd`, `foldarms`, `constmove`, `rectable`, `dowhile`, `whiledec`, `condassign`, `misalign`, `pfieldarg`, `ptrinc`, `ptrlocal`, `plusone`, `elseret`, `u16sub`, `randsum`, `defaultret`, `x86fix`, `preinc`, `vcopy`, `argcopy`, `argtemp`, `selfplus`, `pfieldadd`, `orthen`, `inlinetemps`, `predectest`, `blockcopy`, `arrinit`, `modfix`, `emptycase`, `decsign`, `negmul`, `x84byte`, `stmtexpr` (GNU `({ ... })` to stop GCC re-associating `v * (bit * 2)`), `pfieldrmw`, `cmpswap`, `sub16c`, `rettype`, `minusk`, `postdecse`, `ptrstep`, `loopblock`, `narrowparam`, `tempcompound`, `fakearg`, `gotoloop`, `nestdec`, `abssum`, `matcopy`, `entsub`, `idxwrap`, `sfield`, `temptype`, `swapadj`, `ply84`, `litbase`, `scratch` (scratchpad as a struct), `argtrim` (drops stale extra call arguments), `predecloop` (m2c's counter loops back to `while (--*X != -1)`); `chain.py` runs several in a row |
| `intdiag.py` | For an overlay function that matches alone but not inside its file: shows which bytes of the program change |
| `stmtsweep.py` | Tries every order of a marked block of statements (with alternatives per line) until a function matches |
| `rodatasplit.py` | (Historical - no INCLUDE_RODATA is left.) Splits strings/tables from a function's .s into D_xxxx.s files and prints the INCLUDE_RODATA lines its C code needs |
| `permharvest.py` | Stores the best decomp-permuter near-misses (not only exact matches) as drafts when they beat the stored draft |
| `movesweep.py` | Hill-climbs statement order by moving single statements within blocks (`one` for a draft, `batch --jobs N` for all pending drafts, stores improvements) |
| `jtinsert.py` | Integrates a function marked for a 4-aligned jump table (`jtalign.awk`), which `integrate.py` can't do |
| `jtalign.awk` | Build step for the main program: a function preceded by `__asm__("#JTALIGN2");` gets a 4-aligned jump table (some of Capcom's are) |
| `padsweep.py` | Adds missing jump-table pad words after already-finished overlay functions (needed before their neighbours can be integrated) |
| `syncheck.py` | Parses every C file with a modern GCC (`mips64-elf-gcc -fsyntax-only -std=gnu89`, after `make compile_commands`); must print `total errors 0` |
| `audit_scan.py` / `audit_unused.py` | Readability checks: counts leftover decompiler patterns (`D_`/`func_` names, `unk` fields, m2c locals, ...) per folder / lists file-scope declarations and `#define`s that nothing uses |
| `argcheck.py` | Lists calls whose argument count differs from the callee's definition (fine on the PS1 with its register calling convention, a problem for any other compiler or platform) |

`mmfix.py` and `castfix.py` never touch lines with a symbol listed in `config/elemfield_syms.txt` (element
aliases such as `g_PacketArea_1`: -O2 code that addressed one array element as its own symbol).

Rules: run only one job per program at a time, and don't use the probe tools on a program while
`refine.py` runs on it. Details and the list of rewrite rules: [matching_guide.md](matching_guide.md).

## Build

Setup and the first build are described in the [README](../README.md). Day to day, from `dualshock/`:

```sh
sh ../tools/buildenv.sh "make split"   # cut the originals into assembly (once, or after changing a .yaml / symbol file)
sh ../tools/buildenv.sh "make -j8"     # rebuild + compare
```

When everything is right, it prints `OK` for each file and
`All 13 executables match the originals.`

## Editor support (IntelliSense)

`make compile_commands` writes `compile_commands.json` (per-file flags; a `src/shared/` enemy file gets the
headers and stage `globals.h` its wrapper compiles it with), which `.vscode/c_cpp_properties.json` points the
VS Code C/C++ extension at (32-bit MIPS, GNU C89). Re-run it after adding or splitting files. The sources are
plain GNU C89: a modern GCC (`mips64-elf-gcc -fsyntax-only -std=gnu89 -mabi=32`) parses every file without
errors (old GCC-only forms such as cast increments `((Joint *)p)++` were rewritten, byte-identical).

## Building a playable disc image

Run `sh ../tools/buildenv.sh "make disc"` (Windows and Linux), or on Windows double-click **`build_disc.bat`**
(in `dualshock/`). It:

1. rebuilds the 13 executables from the C code (`build_disc.bat`: from scratch, `make clean` + `make`;
   `make disc`: only what changed; the
   full log goes to `build/make.log`),
2. puts them into a copy of the original disc (`game_files/RE1-Dualshock` is unpacked once into
   `build/disc_work/`; the movies, sounds, rooms and the CD-audio track come from there),
3. writes `build/disc/` with the same three files as the original dump:
   `... (Track 1).bin` (data), `... (Track 2).bin` (CD audio) and the `.cue`,
4. prints the CRC32 of each file next to the original's (`MATCH` / `DIFFERENT`).

The image can be played in an emulator (open the `.cue`). Scripts: `tools/build_disc_make.sh`
(the make step), `tools/build_disc.py` (the disc step, uses mkpsxiso/dumpsxiso).

## Reading the code: names and conventions

- **Functions** are `snake_case` (`pad_update`, `zombie_chase_player`, `itembox_draw`). Where the RE1 PC
  decompilation has the same function, its name is used, so the two can be read side by side.
  Every function the C code uses has a name (no `func_XXXXXXXX` is left in src/).
- **Globals** are `g_Name` (`g_CurrentEntity`, `g_PlayerEntity`, `g_GameStateFlags`), strings `s_Name`
  (`s_SaveFileName`, `s_YesNo`). No unnamed `D_XXXXXXXX` global is left in src/.
  Every name and the evidence for it is listed in `config/names_main.txt`, `config/names_overlays.txt`,
  `config/globals_main.txt`, `globals_main_2.txt`, `globals_main_3.txt` and `globals_overlays.txt`.
- **Fixed addresses**: Capcom's code often reached globals through their address instead of the
  symbol, and that changes the machine code, so the matching C has to do the same. Those places say
  `LIT(u8, g_StageId)` ("the byte at g_StageId's address"), `LITP(Effect, g_EffectPool)[i]` (a pointer
  to that address) or `ADDR_g_PlayerPosX` (the address itself); see `include/re1_addrs.h`.
  The PS1's 1 KB scratchpad (fast RAM at 0x1F800000) works the same way with `sp_` names:
  `LITP(GsSPRITE, sp_sprite)->x`, `LIT(s32, sp_vec_vy)`, `LITP(MATRIX, sp_matrix)`, `LIT(s32, sp_tmp1C)`
  (per-function temporaries); `SCRATCH->field` is the struct form where the original used one.
- **Game objects** use `Entity` (`include/re1_types.h`): `g_CurrentEntity->action_state`,
  `g_PlayerEntity.pos_x`. Overlays call the main program through a table:
  `(g_ApiCallPtr = api_RotMatrix)(...)` calls `RotMatrix`; every `api_*` pointer is declared once in
  `include/overlay_api.h`, and `API_AS(u32, api_rand)(...)` marks the few calls whose result Capcom's code
  read as another type. The stage overlays' shared declarations are in `include/stage_common.h`.
- **Named constants** (`include/re1_flags.h`, `include/re1_types.h`): status flags (`STATUS_ADVANCED`,
  `STATUS_IN_WATER`, ...), game state flags (`GAME_MIRROR_ON`, `MENU_REQUEST(MENU_MODE_x)`, ...), pad bits
  (`PAD_RUN` = game action bits, `PSX_*` = the raw controller bits), menu sounds (`SE_MENU_CONFIRM`), sizes
  (`ENTITY_SIZE`, `JOINT_SIZE`, `EFFECT_SIZE`); in the overlays `STATE4(state, sub, behavior, action)` for
  the 4 state bytes written at once, `JOINT_N(joints, n)`, `STAGE_ROOM(stage, room)`, per-enemy sound ids
  (`ZOMBIE_SE_GROAN`) and scenario flags (`SCN_FLAG_PLANT42_DEAD`). Bits whose meaning isn't clear yet stay
  numbers.
- **The player** is `g_PlayerEntity` (an `Entity`); its own data from byte 0x15C on is `g_PlayerWork`
  (`poison_timer`, `max_health`, ...). In the main program you'll also see `g_PlayerHitState`,
  `g_PlayerHealth`, ...: the same bytes as `g_PlayerEntity.hit_state`, `.health`, but separate
  symbols, because at -O2 the original code compiled like separate globals there.
- **Typed pointers**: `ENT(p)->health` (p points to an entity), `JOINT(p)->world.t[0]` (a skeleton joint;
  `world.t` = its position in the room), `BILLBOARD(p)->corners[0]`; and views of single structures such
  as `ISCREEN->cursor_row` (passcode panel) or `END_SLOT->fade_level` (ending). The variables keep their
  original types, these macros only name the fields. `MEMBER_AT(p, Joint, [1].flags, u8)`-style forms
  are the exact spelling where the readable one compiled differently.
- **Enemy code** is in the stage folders (`src/stage3/hunter.c`); when a stage file only contains
  `#include "../shared/hunter.c"`, the code is in `src/shared/` and used by several stages (every
  enemy that appears in more than one stage is shared: zombie in 6 stages, crow in 4, ...).
- **Enemy-specific data**: the entity bytes from 0x16C on mean something different per enemy. Each enemy
  file has its own struct at the top (`ZombieWork`: `hit_count`, `stagger_timer`, ...), written as
  `ZOMBIE(g_CurrentEntity)->hit_count`. `ZOMBIE_FIELD(e, f)` and `ZOMBIE_MEMBER(e, f)` reach the same
  field; they only exist because the original code compiled differently (see `include/re1_types.h`).
- **Things that look odd but are needed for the byte match** (do not "clean them up" without
  rebuilding and checking): field macros such as `FIELD_AT(p, Joint, [1].flags, u8)`, `ENTITY_AS(e, f, T)`
  and the `OFFSET_MEMBER` / `OFFSET_DEREF` forms they are built on (`include/re1_types.h`: the same field
  access compiles differently in struct-member and pointer-arithmetic spelling), per-function aliases like
  `extern u8 g_StageId_u8 __asm__("g_StageId");` (Capcom's source files declared some globals with other
  types), `do { } while (0)` blocks and `({ x; })` expressions (they change the compiler's register
  choices), the `LIT(...)` fixed addresses (a test converting them to symbols broke the match almost
  everywhere), `BYTES(n)` byte blocks (a copy or pointer step that compiles differently with the real type),
  element aliases like `extern s32 g_PacketArea_1 __asm__("g_PacketArea+4");` (-O2 code that addressed one
  array element as its own symbol), per-function declarations with another type under a suffixed alias name
  (`Flg_on_v`, `joint_move_v`, `g_EndingSlots_s32`: Capcom's files declared the same function / global
  differently), `void f();` without parameters where calls pass extra arguments, and the two `FAKEMATCH`
  comments.
- The build prints **no compiler warnings**; a new warning usually means a real mistake.
- After any change, `make` must still print `All 13 executables match the originals.`

## What's in here

| Path | What |
|---|---|
| `slus_007.47.yaml` | How the main program is cut up (splat config) |
| `<overlay>.yaml` | Same for each overlay (generated by `tools/gen_overlay_configs.py`) |
| `config/symbol_addrs_psyq.txt` | Names of the 687 Sony library functions (see [psyq_sdk.md](psyq_sdk.md)) |
| `config/symbol_addrs*.txt` | Names and addresses of Capcom's functions and globals |
| `src/main/*.c` | Capcom's game code in 45 topic files (door.c, player.c, sound.c, inventory.c, camera.c, ...; split from one capcom.c by `tools/splitc.py`, later refined with `tools/splitfile.py`); each file starts with a comment saying what it holds |
| `src/main/globals.h` | Declarations shared by all main-program files |
| `src/<overlay>/*.c` | Each overlay's code; the stage overlays are split per enemy (`entry.c` = stage setup + the API table, `zombie.c`, `hunter.c`, ...) with their own `globals.h` |
| `config/locals/*.txt` | Structs reached through local pointers (joints, entities, billboards) for `tools/localview.py`; generates `include/re1_locals.h` |
| `config/structs/*.txt` | Structs reached through one pointer/global (passcode panel, ending slots, door commands, ...) for `tools/structview.py`; generates `include/re1_views.h` |
| `config/enemy_work/*.txt` | Field names of each enemy's work area (entity 0x16C..), with the evidence, for `tools/enemywork.py` |
| `src/shared/*.c` | Enemy code used by several stages, written once (`tools/sharetwins.py`); a stage's `<enemy>.c` that only says `#include "../shared/<enemy>.c"` compiles it with that stage's declarations |
| `include/` | `common.h` (basic types), `re1_flags.h` (flag / pad / sound constants), `overlay_api.h` + `stage_common.h` (overlay declarations), `re1_types.h` (Capcom's structs: Entity, Effect, MenuWork, ... and the field macros), `psyq_types.h` (Sony library types), generated `re1_views.h` / `re1_locals.h` / `re1_addrs.h` (from `config/structs`, `config/locals`, `config/addrs.txt` - edit the specs, not the headers), assembly helper macros; `m2c_fields.h` is only for decompiler drafts (tools/autodecomp.py), no source file includes it (it adds this project's two field-access forms to m2c's own `m2c_macros.h`, which is GPL-3.0 and therefore not in git: `tools/m2c_macros.py` downloads it) |
| `tools/` | Progress report, function diff, compiler experiments |
| `Makefile` | Build + checksum check |
| `asm/`, `build/` | Generated; **never in git** (derived from the game) |

The original game files are never copied in here; they're read from `../extracted/`.

More detail: [matching_guide.md](matching_guide.md) (writing matching C), [code_notes.md](code_notes.md)
(code structure and conventions), [game_notes.md](game_notes.md) (how the game works).
