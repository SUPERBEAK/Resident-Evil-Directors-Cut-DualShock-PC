#!/bin/bash
# test_psx_toolchain.sh - smoke test of the PS1 matching-decomp compile pipeline:
#   C -> cpp -> cc1 (GCC 2.7.2-psx) -> maspsx -> mips64-elf-as (R3000, o32, EL) -> .o
#
# Usage (Git Bash on Windows, a shell on Linux; any working directory):
#   sh tools/test_psx_toolchain.sh
#
# Compiles tools/tests/psx_toolchain/sample.c into temp/psx_toolchain_test/
# and checks the object is ELF32 / little endian / MIPS R3000 / mips1 with a jump table.
# On Windows cpp/cc1 are MSYS binaries (need msys-2.0.dll), so this runs inside the project's MSYS2.

set -e

case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
*) WINDOWS=0 ;;
esac

if [ "$WINDOWS" = 1 ] && [ -z "$PSXTEST_IN_MSYS" ]; then
    ROOT_W="$(cd "$(dirname "$0")/.." && pwd -W)"
    exec "$ROOT_W/tools/external/msys64/usr/bin/bash.exe" -c \
        'export MSYSTEM=MSYS PSXTEST_IN_MSYS=1; . /etc/profile; exec bash "$(cygpath -u "$1")" "$2"' \
        _ "$ROOT_W/tools/test_psx_toolchain.sh" "$ROOT_W"
fi

if [ "$WINDOWS" = 1 ]; then
    ROOT="$(cygpath -u "$1")"
    PY="$ROOT/tools/external/venv/Scripts/python.exe"
else
    ROOT="$(cd "$(dirname "$0")/.." && pwd)"
    PY="$ROOT/tools/external/venv/bin/python"
fi
T="$ROOT/tools/external"
GCC="$T/old-gcc-build/gcc-2.7.2-psx"
BIN="$T/mips-toolchain/bin"
SRC="$ROOT/tools/tests/psx_toolchain/sample.c"
OUT="$ROOT/temp/psx_toolchain_test"
mkdir -p "$OUT" && cd "$OUT"

fail() { echo "FAIL: $*" >&2; exit 1; }

"$GCC/cpp" -undef -D__GNUC__=2 -Dmips -D__mips__ -D_LANGUAGE_C -DLANGUAGE_C "$SRC" sample.i \
    || fail "cpp"
"$GCC/cc1" -quiet -G0 -O2 -mcpu=3000 -mips1 -fno-builtin -msoft-float sample.i -o sample.s \
    || fail "cc1"
"$PY" "$T/maspsx/maspsx.py" --aspsx-version=2.56 < sample.s > sample.ms.s \
    || fail "maspsx"
"$BIN/mips64-elf-as" -EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -O1 -G0 \
    sample.ms.s -o sample.o || fail "as"

hdr="$("$BIN/mips64-elf-readelf" -h sample.o)"
echo "$hdr" | grep -q "ELF32"            || fail "not ELF32"
echo "$hdr" | grep -q "little endian"    || fail "not little endian"
echo "$hdr" | grep -q "MIPS R3000"       || fail "not MIPS R3000"
echo "$hdr" | grep -q "mips1"            || fail "not mips1"
"$BIN/mips64-elf-nm" sample.o | grep -q " T enemy_update" || fail "enemy_update missing"
grep -q '^\$L16:' sample.s               || fail "no jump table emitted"

echo "PASS: PS1 toolchain OK ($(wc -c < sample.o) byte object in temp/psx_toolchain_test)"
