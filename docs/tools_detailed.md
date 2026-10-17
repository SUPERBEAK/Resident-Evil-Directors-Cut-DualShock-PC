# Project tools (detailed)

Everything is installed by `tools/setup_tools.sh` into `tools/external/` (git-ignored). All versions and
URLs are pinned at the top of that script and in `tools/requirements.txt`. Simple overview: [tools.md](tools.md).

## Requirements on your PC

The same scripts run on both systems; `setup_tools.sh` picks the Windows or Linux downloads itself.

- **Windows:** **Git for Windows** (the scripts run in Git Bash; `curl`, `unzip`, `tar` and `git` come with
  it) and **Python 3.14** on PATH (the venv is created with it). MSYS2 is downloaded by the setup script.
- **Linux (x86_64):** Python 3.12 or newer (tested with 3.12.3 on Ubuntu 24.04) and the system packages
  `curl git unzip tar make gcc patch diffutils binutils python3 python3-venv` (Debian/Ubuntu names; the setup
  script checks for them and prints the `apt` line if one is missing). The project path must not contain
  spaces.
- **Java** (only for jPSXdec, used by `tools/bss_view.py`).
- Optional: **Ghidra 12.1.3** (for `tools/ghidra.bat` / `ghidra.sh`), DuckStation (see below).

Both systems produce the same build: the same pinned tools, and the MIPS toolchain from the same release
(GNU Binutils 2.45). Checked on Windows and in a fresh Ubuntu 24.04 container: all 13 executables match and
the disc image is CRC-identical.

## Versions and sources

| Folder (`tools/external/...`) | Version | Source |
|---|---|---|
| `msys64/` (Windows only) | MSYS2 base 2026-09-27, + `make` 4.4.1, MSYS `gcc` 15.3, `patch`, `diffutils` | repo.msys2.org |
| `mips-toolchain/` | `mips64-elf-gcc` 16.2.0 (+ binutils, gdb, newlib); the decomp uses its assembler/linker | prebuilt `gcc-toolchain-mips64-win64.zip` (Windows) / `gcc-toolchain-mips64-x86_64.deb` (Linux: unpacked with `ar` + `tar`, not installed); continuous prerelease, URLs in `tools/setup_tools.sh` |
| `mkpsxiso/` | v2.30: `dumpsxiso` and `mkpsxiso` (`win64.zip` on Windows; `Linux.zip` on Linux, its `bin/` moved up so both systems use the same paths) | github.com/Lameguy64/mkpsxiso |
| `jpsxdec/` | v2.1 beta (needs Java); used by `tools/bss_view.py` | github.com/m35/jpsxdec |
| `imhex/` (Windows only, optional) | v1.38.1 Windows portable | github.com/WerWolv/ImHex |
| `ghidra_settings/.../Extensions/ghidra_psx_ldr` | release 2026.10.03, built for Ghidra 12.1.3 | github.com/lab313ru/ghidra_psx_ldr |
| `venv/` | Python venv (`Scripts/` on Windows, `bin/` on Linux): numpy 2.5.3, Pillow 12.3.0, toml 0.10.2 + Levenshtein 0.27.5 (for decomp-permuter); every version pinned in `tools/requirements.txt` | PyPI |
| venv, decomp packages | splat64 0.50.0 with `[mips]` (spimdisasm 1.42.4, rabbitizer 1.16.2 and splat's helper packages), m2c 0.1.0 (git, pinned to commit 708d2d2c; its `m2c_macros.h` is downloaded into `dualshock/include/` by `dualshock/tools/m2c_macros.py`, not in git: GPL-3.0) | PyPI / GitHub |
| `_downloads/` | Cache of every archive, so a re-run doesn't download again | — |
| `maspsx/` | git `e85ecb3` (pinned, LF line endings) | github.com/mkst/maspsx |
| `old-gcc/` | git `b74211c` (pinned, LF line endings): build recipes + patches | github.com/decompals/old-gcc |
| `decomp-permuter/` | git `8556c81` (pinned) | github.com/simonlindholm/decomp-permuter |
| `old-gcc-build/gcc-<ver>-psx/` | `cpp.exe` + `cc1.exe` for 2.6.3, 2.7.2 and 2.8.1, built from source (Windows: in MSYS2's MSYS env; Linux: with the system gcc) | Slackware 2.2.0 mirror (2.6.3), ftp.gnu.org (2.7.2, 2.8.1) |
| `objdiff/` | v3.8.2: `objdiff` (GUI) + `objdiff-cli` (`.exe` on Windows) | github.com/encounter/objdiff |

**Why pinned:** maspsx, old-gcc, splat, spimdisasm and m2c decide the byte match; a newer upstream version
could split or assemble differently. Not pinnable: the MIPS toolchain zip, a continuously updated prerelease (the
decomp only uses its `mips64-elf-as`/`ld`/`objcopy`, and `mips64-elf-gcc` for `dualshock/tools/syncheck.py`).

## DuckStation (optional, installed by hand)

- From the official GitHub release (`duckstation-windows-x64-release.zip`) into `tools/external/duckstation/`.
- A `portable.txt` next to the exe makes it keep settings, memory cards, save states and caches in that
  folder, so a DuckStation profile elsewhere on the PC is never touched.
- The PS1 BIOS goes into `tools/external/duckstation/bios/` (never committed: `tools/external/` is git-ignored).
- Use: compare the original Dual Shock disc with the disc image built from the decomp
  (`dualshock/build_disc.bat`).

## PS1 matching-decomp toolchain

Pipeline, checked by `tools/test_psx_toolchain.sh` (prints `PASS`):

```
C --cpp--> .i --cc1 (GCC 2.6.3-psx)--> .s --maspsx--> .s --mips64-elf-as--> .o (ELF32 LE, R3000, o32, mips1)
```

- **Compiler:** RE1 was built with GCC 2.6.3 for PlayStation: `-O2 -G0` for the main program, `-O0` for
  the overlays, plus `-mcpu=3000 -mips1 -fno-builtin -msoft-float` (see `dualshock/Makefile`). 2.7.2 and
  2.8.1 are built too, for comparisons (`RE_GCC=2.7.2` with the draft tools).
- **cpp flags**: `-undef -nostdinc -D__GNUC__=2 -Dmips -D__mips__ -D_LANGUAGE_C -DLANGUAGE_C`.
- **maspsx** (emulates Sony's ASPSX assembler): `--aspsx-version=2.56 --expand-div`. Every division in
  the original code is the assembler-expanded form (`div $zero` + `bnez; break 7` + overflow check
  `break 6`), hence `--expand-div`. maspsx reads its input from **stdin** whenever stdin isn't a terminal
  (scripts, make): always use `maspsx.py [opts] < in.s > out.s`; passing a filename makes it hang.
- **Assembler:** the toolchain's `mips64-elf-as` works fine for PS1 objects:
  `-EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -O1 -G0`. No separate
  `mipsel-linux-gnu` binutils are needed.
- `cpp.exe` and `cc1.exe` are MSYS binaries (they link `msys-2.0.dll`). Run them inside
  the project's MSYS2 (through `tools/buildenv.sh`), not directly from Git Bash, which ships its own,
  different `msys-2.0.dll`.

### How the old GCC is built (`tools/build_old_gcc.sh`)

This follows decompals/old-gcc's `gcc-2.7.2-psx-macos.sh` (the recipe for a 64-bit host), adapted for
MSYS2 and for Linux x86_64:
- On Windows it builds in MSYS2's **MSYS** environment (x86_64, LP64, POSIX), using MSYS `gcc` 15.3 and
  `patch`; on Linux directly with the system `gcc` (tested with GCC 13.3).
- **Linux:** `configure` gets the same host case as MSYS2 (`*-pc-msys*|x86_64-*-linux*`, with the macOS
  host header), and the C files are compiled with `-include time.h`: `cccp.c` takes `struct tm` (for
  `__DATE__` / `__TIME__`) from `<sys/time.h>`, as on BSD / macOS / MSYS2, but glibc declares it only in
  `<time.h>` ("invalid use of undefined type 'struct tm'" without it).
- It applies the same patches: `obstack-2.7.2.h`, `configure`, `mipsel-2.7`, `psx-2.5.7`,
  plus the varargs→stdarg replacement.
- It uses the same pinned modern `config.guess`/`config.sub`, with `psx*` added to the OS list.
- **Host header:** `xm-darwin.h` is copied as `config/xm-msys.h`, and `configure` gets a
  `*-pc-msys*` case. The header forces a 32-bit `HOST_WIDE_INT` on the 64-bit host, which
  matches the Linux i386 reference build.
- **MSYS2 fix:** there's no system `<stab.h>`, so `gstab.h` is copied to `stab.h`.
- Only `cpp` and `cc1` are built (no gcc driver needed).
- The upstream tests pass: little-endian `lbu`, 32-bit `HOST_WIDE_INT` (`0x80000000`),
  section attribute, `-msoft-float`. The real proof: all 13 RE1 executables rebuild byte for byte.
- **Versions:** `build_old_gcc.sh [2.6.3|2.7.2|2.8.1]` gives `tools/external/old-gcc-build/gcc-<ver>-psx/`.
  - 2.6.3 (RE1's compiler): source from the Slackware 2.2.0 mirror; patches `obstack-2.7.2.h`,
    `sdbout-2.6.3.c`, `psx-2.5.7`; extra `-Dbsd4_4 -DHAVE_STRERROR`; its `configure` has no `--disable-*`
    options.
  - 2.8.1: patches `obstack-2.8.1.h`, `mips.patch`, `psx.patch`.
- **LP64 fix (2.6.3):** the script runs `tools/gcc263_lp64_decls.py` before `make`. It declares the
  pointer-returning functions that 2.6.3 calls without a prototype (negate_rtx, replace_regs, remove_death,
  label_rtx, expand_expr, emit, emit_insns_after, reorder_blocks, alter_subreg, truthvalue_conversion).
  Without it the 64-bit cc1 truncates those pointers and crashes on some inputs (in the delayed-branch
  pass; "does not compile" with no error message = exit code 139). The script needs the project venv, so
  `setup_tools.sh` creates the venv before building the old GCCs. Diagnostic build (warnings on, debug):
  `tools/build_old_gcc_diag.sh` -> `temp/old-gcc-diag-<ver>/`.
- **Generated headers first:** the old Makefiles don't list the generated `insn-*.h` headers as
  dependencies of every object, so with `make --jobs` an object can compile before its header exists
  ("insn-codes.h: No such file or directory"). The script makes `insn-config.h insn-flags.h insn-codes.h
  insn-attr.h` with one job first (`make-headers.log`), then `cpp` and `cc1` in parallel.
- **Spaces in the project path:** GCC 2.x's configure and Makefiles don't quote paths (configure's
  `cd $STARTDIR` fails with "too many arguments" and continues in the wrong folder). When the path has a
  space, the script works through the Windows 8.3 short name of the project folder (`cygpath -m -s`); if
  the drive has no short names it stops and asks for a path without spaces. Harmless configure output,
  also in working builds: `-D__psx__: command not found`, `CC: fatal error: no input files`.
- **CRLF gotcha:** Git for Windows checks out with CRLF line endings by default, which breaks the old-gcc
  patches (and `config.sub` edits). Every tool clone uses `core.autocrlf=false`.
- **GCC 15 gotcha:** 1990s autoconf test programs (implicit `int main()`) fail under GCC
  15's default C23 ("C compiler cannot create executables"). Configure therefore runs with
  `CC="gcc -std=gnu89 -w"`.

## Scripts (`tools/`)

### `setup_tools.sh`
- Can be re-run safely. A tool is skipped if its folder exists; delete a tool's folder to reinstall it.
- Zip files with a single top-level folder are flattened, so each program sits directly in
  `tools/external/<name>/` (not in an extra versioned subfolder).
- Its last steps: the Python venv (+ `dualshock/tools/m2c_macros.py`, which downloads m2c's GPL-3.0 header),
  then the three old GCCs.

### `dump_discs.sh`
- Unpacks `game_files/RE1-Dualshock` with dumpsxiso into `extracted/RE1-Dualshock/` (+ `.xml` layout,
  `.sha1` of every file).
- **Gotcha:** `dumpsxiso.exe` exits with 127 and writes nothing when Git Bash hands it `/c/...` paths;
  with `C:/...` paths it works. The script therefore takes `ROOT` from `pwd -W`.

### `buildenv.sh`
Runs a command (the decomp build: `make`) with the MIPS toolchain
(`MIPS_TOOLCHAIN=<tools>/mips-toolchain`) and its `bin/` on PATH; on Windows inside MSYS2 MINGW64, on
Linux in a normal `sh`:

```sh
sh tools/buildenv.sh "make"    # run in the current directory
sh tools/buildenv.sh           # interactive shell
```

- **Windows gotcha:** environment variables do *not* make it from Git Bash's msys runtime into
  MSYS2's `bash.exe`. Even `FOO=1 bash.exe -c 'echo $FOO'` prints nothing. So the script
  passes the tools dir, the working dir and the caller's PATH as positional arguments, sets
  `MSYSTEM=MINGW64` itself, and sources `/etc/profile`.
- The caller's PATH is appended, so programs from the normal Windows PATH are still found.
- Inside MSYS2, `python` resolves to the Windows Store stub. Always call
  `tools/external/venv/Scripts/python.exe` explicitly.
- For the same reason the Makefile doesn't look at `$(OS)`: it picks the venv layout by what exists
  (`venv/Scripts/python.exe` on Windows, `venv/bin/python` on Linux); the Python tools use `os.name`
  (`compiler_matrix.WINDOWS`, `EXE`, `VENV_BIN`, `PY`).

### `lowrun.sh`
`sh tools/lowrun.sh "<command>"` runs a long batch from `dualshock/` through `buildenv.sh` at BelowNormal
process priority (Windows) or `nice 10` (Linux), so it doesn't slow down other programs (child processes
inherit the priority).
Stopping such a batch does not always stop Python multiprocessing workers (decomp-permuter,
`movesweep.py`): find the process tree (e.g. `Get-CimInstance Win32_Process`, CommandLine containing the
script name) and end it with `taskkill /T /F /PID <root>` (Linux: `pkill -f <script name>`).

### `ghidra.bat` / `ghidra.sh`
Start an installed Ghidra 12.1.3 (set `GHIDRA_DIR` to its folder first; `.sh` on Linux) with these Java options set
through `GHIDRA_JAVA_OPTIONS`:

```
-Dapplication.settingsdir=<tools>\ghidra_settings   (preferences, extensions, logs)
-Dapplication.cachedir=<tools>\ghidra_cache
-Dapplication.tempdir=<project>\temp                (Ghidra uses temp\ghidra)
```

- Extensions are loaded from `ghidra_settings/ghidra/ghidra_12.1.3_PUBLIC/Extensions/`.
- Your normal Ghidra profile (`%APPDATA%\ghidra`, on Linux `~/.ghidra`) and the Ghidra install folder are
  never touched.
- The log line `ClassSearcher Ignoring class 'MipsPreAnalyzer' from ghidra_psx_ldr.jar` is
  harmless: the PSX loader bundles a copy of an analyzer that Ghidra already has.
- To use another Ghidra version, change `GHIDRA_VER` in `setup_tools.sh` (the extension zips are built for
  one exact Ghidra version).

### Research scripts
- `psyq_match.py`: matches the Sony library functions against the PsyQ signatures of ghidra_psx_ldr
  (see [psyq_sdk.md](psyq_sdk.md)).
- `rdt_em_set.py`, `rdt_msg.py`, `rdt_zones.py`: scan the room files (RDT) for entity spawns, text
  messages and room-action zones. They parse the opcode table of the "RDT (Resident Evil 1997)" page of the
  Just Solve the File Format Problem wiki, saved as HTML at `reference/rdt_format.htm` (or the path in
  `RDT_FORMAT_PAGE`).
- `emd_tim.py`: renders an EMD model's texture to PNG (to identify a model).
- `bss_view.py`: decodes a room's camera backgrounds (BSS) to PNGs with jPSXdec.

## decomp-permuter

- Cloned by `setup_tools.sh` into `tools/external/decomp-permuter` (commit 8556c81); its Python packages
  `toml` and `Levenshtein` are in `tools/requirements.txt`.
- Windows notes: it calls `bash` (Git Bash) for `compile.sh` and `cpp` for its own preprocessing:
  the GCC 2.6.3 `cpp.exe` (`tools/external/old-gcc-build/gcc-2.6.3-psx`) must be on PATH (permbatch.py
  does that); the `objdump_command` in settings.toml needs a `C:/...` path (the permuter is a Windows
  python), compile.sh needs `/c/...` paths (bash).
- The wrappers (`dualshock/tools/`): `permsetup.py setup|check|export <target> <func>` builds the
  permuter folder (`build/permuter/<target>/<func>/`: base.c, compile.sh = the cc1 -> maspsx -> as
  pipeline LINKED at the function's original address, target.o = the original function built the
  same way, settings.toml), checks a result byte for byte and exports it as a `match` draft;
  `permbatch.py --target T [--max-dist N] [--seconds S] [--jobs J]` does that for many functions.

## Problems handled by the setup script

- **MSYS2 mirrors return HTTP 429 (rate limit).** pacman's own downloader aborts the whole transaction at
  the first 429, so the script sets `ParallelDownloads = 1` and
  `XferCommand = /usr/bin/curl -L -C - -f --retry 10 -o %o %u` in `msys64/etc/pacman.conf` (curl retries
  one file with growing delays and honours Retry-After) and retries pacman 5 times, 60 s apart. Downloaded
  packages stay in `msys64/var/cache/pacman/pkg`, so a re-run continues where it stopped.
- **`mv` of a freshly extracted folder fails with "Permission denied"**, most likely Windows Defender
  scanning the new `.exe` files. The `mv_retry` helper retries for up to about 30 seconds.
- **Windows Defender and scratch files:** tens of thousands of small scratch files keep Defender busy; the
  draft tools delete each function's work folder after use. An antivirus exclusion for `dualshock/build`
  and `temp` (your decision, needs admin rights) speeds up long batches.

## What a fresh clone needs

Everything git-ignored is either rebuilt by a script or your own input:

| Ignored | Comes back with |
|---|---|
| `tools/external/` | `tools/setup_tools.sh` (DuckStation is optional and installed by hand, see above) |
| `game_files/RE1-Dualshock/` | your own disc dump (never in git) |
| `extracted/` | `tools/dump_discs.sh` |
| `dualshock/asm/`, `dualshock/build/`, `dualshock/*.ld`, `undefined_*_auto.txt` | `make split` + `make` |
| `dualshock/include/m2c_macros.h` | `dualshock/tools/m2c_macros.py` (run by `setup_tools.sh` and by the draft tools) |
| `dualshock/compile_commands.json` | `make compile_commands` |
| `reference/` | optional reference material you add yourself (the RE1 PC decompilation, the RDT format page) |
| `temp/`, `__pycache__/`, `work_*/` | scratch only |

A copy containing only the files git keeps (plus the installed tools and a disc dump) runs the whole
pipeline: `dump_discs.sh`, `make split`, `make` (all 13 match) and `make disc` / `build_disc.bat` (disc
image CRC-identical to the original). Checked on Windows and on Linux (a fresh Ubuntu 24.04 container
with only the README's packages).
