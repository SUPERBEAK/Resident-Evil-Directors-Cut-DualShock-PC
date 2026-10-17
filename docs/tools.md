# Project tools (simple guide)

Every tool lives in `tools/external/`, inside the project. That folder is git-ignored
because it's several GB of downloaded programs. Nothing is installed system-wide.

## Install / reinstall

From the project root, in Git Bash on Windows or a terminal on Linux (x86_64):

```sh
sh tools/setup_tools.sh
```

The script downloads anything that's missing (the Windows or Linux versions) and builds the old PS1
compilers. On Linux it first checks for the needed system packages. Running it again is safe:
it skips whatever is already there. To reinstall one tool, delete its folder and run the
script again.

## What's installed

| Tool | What it's for | Start it with |
|---|---|---|
| **MIPS toolchain** | The assembler and linker the decomp build uses | `sh tools/buildenv.sh "make"` (the build environment wrapper) |
| **MSYS2** (Windows only) | Provides `make`, a bash shell and the C compiler that builds the old PS1 compilers | used by `buildenv.sh` |
| **dumpsxiso / mkpsxiso** | Extract the files from the PS1 disc, and build a disc image from the decomp | `tools/external/mkpsxiso/dumpsxiso.exe` |
| **jPSXdec** | Decodes PS1 pictures and videos; `tools/bss_view.py` uses it to show room backgrounds (needs Java) | `python tools/bss_view.py STAGE2/ROOM207` |
| **ImHex** (Windows only, optional) | Hex editor for studying unknown file formats | `tools/external/imhex/imhex-gui.exe` |
| **Ghidra + PS1 loader** | Disassemble the RE1 PS1 executable | `tools/ghidra.bat` (Linux: `ghidra.sh`) |
| **Python venv** | The decomp tools (splat, m2c, our scripts) | `tools/external/venv/Scripts/python.exe` |
| **DuckStation (portable)**, optional, installed by hand | PS1 emulator to compare our build with the original game. All its data stays in its own folder (`portable.txt`); the BIOS is in `bios/` | `tools/external/duckstation/duckstation-qt-x64-ReleaseLTCG.exe` |

### PS1 decompilation tools

| Tool | What it's for | Start it with |
|---|---|---|
| **GCC 2.6.3-PSX** (built from source by us, plus 2.7.2/2.8.1) | The compiler Capcom used. Used to check that our C code compiles to exactly the original bytes | used by the decomp build |
| **maspsx** | Makes the output match Sony's own assembler | used by the decomp build |
| **splat** | Cuts the PS1 program into pieces (code / data) | `tools/external/venv/Scripts/splat.exe` |
| **m2c** | Turns assembly into a first C draft | `tools/external/venv/Scripts/m2c.exe` |
| **decomp-permuter** | Randomly rewrites a function's C in small ways and keeps what compiles closer to the original - finds the register-allocation details our rule-based search can't | `python tools/permbatch.py --target main` in `dualshock/` (see the detailed doc) |
| **objdiff** | Shows side by side whether our compiled function matches the original | `tools/external/objdiff/objdiff.exe` |

Quick check that the decomp compiler chain works:

```sh
sh tools/test_psx_toolchain.sh   # prints PASS
```

Ghidra itself stays in your normal install folder. `ghidra.bat` just starts it with a
separate, project-only profile that already has the PS1 loader, so your normal Ghidra
profile is left alone.

More detail is in [tools_detailed.md](tools_detailed.md).
