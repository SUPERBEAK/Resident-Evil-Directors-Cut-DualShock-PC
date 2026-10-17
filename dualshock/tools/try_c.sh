#!/bin/sh
# try_c.sh <file.c> [extra cc1 flags...] - compile a C snippet exactly like the Makefile
# (cpp -> cc1 gcc-2.7.2-psx -> maspsx) and print the final assembly. For quick matching
# experiments. Run from dualshock/ through the wrapper:
#   sh ../tools/buildenv.sh "sh tools/try_c.sh /path/to/test.c"
set -e
T=../tools/external
G=$T/old-gcc-build/gcc-${GCCVER:-2.6.3}-psx   # GCCVER=2.6.3 / 2.7.2 / 2.8.1
src="$1"; shift
out=build/try_c
mkdir -p "$out"
"$G/cpp" -undef -nostdinc -Iinclude -D__GNUC__=2 -Dmips -D__mips__ -D_LANGUAGE_C -DLANGUAGE_C "$src" "$out/t.i"
"$G/cc1" -quiet -O2 -G0 -mcpu=3000 -mips1 -fno-builtin -msoft-float "$@" "$out/t.i" -o "$out/t.s"
PY=$T/venv/bin/python; [ -x "$T/venv/Scripts/python.exe" ] && PY=$T/venv/Scripts/python.exe
"$PY" $T/maspsx/maspsx.py --aspsx-version=2.56 --expand-div < "$out/t.s" > "$out/t.ms.s"
grep -vE '^\s*\.(file|gnu|section|align|globl|ent|end|frame|mask|fmask|set|type|size|sdata|text|data|rdata)\b|^\s*#|^\s*$' "$out/t.ms.s"
