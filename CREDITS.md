# Credits

This project builds on the work of many others. None of the tools below are included in this repository:
`tools/setup_tools.sh` downloads them (or builds them from source) into `tools/external/`.

## Generated files in this repository

### splat

[splat](https://github.com/ethteck/splat) by Ethan Roseman and contributors splits the original
executables into assembly and data (`dualshock/*.yaml`) and generated the assembly helper files in
`dualshock/include/`: `include_asm.h`, `macro.inc`, `labels.inc` and `gte_macros.inc`.

splat is released under the MIT License, Copyright (c) 2021 Ethan Roseman.

## Decompilation tools

| Tool | Used for | License |
|---|---|---|
| [splat](https://github.com/ethteck/splat) (Ethan Roseman and contributors) | Splitting the executables, generating the linker scripts | MIT |
| [spimdisasm](https://github.com/Decompollaborate/spimdisasm) and [rabbitizer](https://github.com/Decompollaborate/rabbitizer) (Decompollaborate) | The MIPS disassembly behind splat | MIT |
| [m2c](https://github.com/matt-kempster/m2c) | First C drafts from assembly; its `m2c_macros.h` is downloaded, not committed | GPL-3.0 |
| [maspsx](https://github.com/mkst/maspsx) (Mark Street) | Reproducing the output of Sony's ASPSX assembler | MIT |
| [old-gcc](https://github.com/decompals/old-gcc) (decompals) | The build recipes and patches for GCC 2.6.3 / 2.7.2 / 2.8.1 for PlayStation | |
| [GCC](https://gcc.gnu.org/) (Free Software Foundation) | The old compilers themselves, built from the original 2.x sources | GPL |
| [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) (Simon Lindholm and contributors) | Random C rewrites for almost-matching functions | MIT |
| [objdiff](https://github.com/encounter/objdiff) (encounter) | Comparing compiled functions with the originals | |

## Build and disc tools

| Tool | Used for | License |
|---|---|---|
| Prebuilt MIPS toolchain (GNU Binutils + GCC, `mips64-elf-*`) from the [libdragon](https://github.com/DragonMinded/libdragon) project | Assembling and linking the decomp | GNU tools: GPL |
| [MSYS2](https://www.msys2.org/) (Windows only) | `make`, a shell and the host compiler that builds the old GCC | |
| [mkpsxiso / dumpsxiso](https://github.com/Lameguy64/mkpsxiso) (Lameguy64) | Extracting the disc and building the disc image | GPL-2.0 |
| [jPSXdec](https://github.com/m35/jpsxdec) (m35) | Decoding room backgrounds (`tools/bss_view.py`) | |

## Optional tools

- [Ghidra](https://github.com/NationalSecurityAgency/ghidra) with
  [ghidra_psx_ldr](https://github.com/lab313ru/ghidra_psx_ldr) (lab313ru): disassembling the PS1 executable;
  ghidra_psx_ldr's PsyQ signatures are what `tools/psyq_match.py` uses to identify the Sony library functions.
- [ImHex](https://github.com/WerWolv/ImHex) (WerWolv): a hex editor for studying file formats (Windows).
- [DuckStation](https://github.com/stenzek/duckstation): comparing the rebuilt disc image with the original.
- The Python packages listed in `tools/requirements.txt`.

## References

- [resident-evil-pc-decomp](https://github.com/ecruells/resident-evil-pc-decomp): the decompilation of the
  PC version of Resident Evil. Where it has the same function, its name (and the names of its struct fields)
  is used here, so the two can be read side by side. No code was taken from it.
- The "RDT (Resident Evil 1997)" page of the [Just Solve the File Format Problem](http://fileformat.archiveteam.org/)
  wiki: the room file format, used for room-related names and by the `tools/rdt_*.py` scripts.
- Published cheat-code lists for the Dual Shock version (GameHacking.org, CodeTwink): RAM addresses that
  helped name globals.
