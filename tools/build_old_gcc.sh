#!/bin/bash
# build_old_gcc.sh - build a PSX GCC (cpp + cc1) natively: in MSYS2 on Windows, directly on Linux.
#
# Usage (Git Bash on Windows, a shell on Linux; any working directory):
#   sh tools/build_old_gcc.sh [VERSION]    # VERSION: 2.7.2 (default), 2.6.3, 2.8.1
#
# On Windows it re-launches itself inside the project's MSYS2 (MSYS environment, x86_64,
# LP64 like macOS); on Linux x86_64 (also LP64) it builds with the system gcc. Both follow
# decompals/old-gcc's per-version recipes (gcc-<ver>-psx.Dockerfile / -macos.sh):
#   * same source tarball and patches (tools/external/old-gcc/patches),
#   * modern config.guess/config.sub (pinned to the same gcc-mirror commits as the macOS build),
#   * the macOS host header (forces 32-bit HOST_WIDE_INT like the Linux i386
#     reference build) installed as config/xm-msys.h for *-pc-msys and x86_64-*-linux* hosts,
#   * MSYS2 fix: GCC's bundled gstab.h copied to stab.h (no system <stab.h>; harmless on Linux).
# Only cpp and cc1 are built: decomp builds call them directly (no gcc driver).
# Output: tools/external/old-gcc-build/gcc-<ver>-psx/{cpp,cc1} (Windows: .exe + msys-2.0.dll dependency).

set -e

case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
*) WINDOWS=0 ;;
esac

if [ "$WINDOWS" = 1 ] && [ -z "$OLDGCC_IN_MSYS" ]; then
    ROOT_W="$(cd "$(dirname "$0")/.." && pwd -W)"
    exec "$ROOT_W/tools/external/msys64/usr/bin/bash.exe" -c \
        'export MSYSTEM=MSYS OLDGCC_IN_MSYS=1; . /etc/profile; exec bash "$(cygpath -u "$1")" "$2" "$3"' \
        _ "$ROOT_W/tools/build_old_gcc.sh" "$ROOT_W" "${1:-2.7.2}"
fi

if [ "$WINDOWS" = 0 ]; then
    ROOT="$(cd "$(dirname "$0")/.." && pwd)"
    set -- "$ROOT" "${1:-2.7.2}"
    case "$ROOT" in *" "*)
        # GCC 2.x's configure and Makefiles don't quote paths (`cd $STARTDIR`: "too many arguments").
        echo "error: the project path contains spaces; move the project to a folder path without spaces." >&2
        exit 1 ;;
    esac
    EXE=
    PYEXE="$ROOT/tools/external/venv/bin/python"
    native() { echo "$1"; }
else
    EXE=.exe
    PYEXE=""                                  # set below, once ROOT is known
    native() { cygpath -w "$1"; }
    ROOT="$(cygpath -u "$1")"
    case "$ROOT" in *" "*)
        # GCC 2.x's configure and Makefiles don't quote paths (`cd $STARTDIR`: "too many arguments"), so a
        # project path with spaces breaks the build. Work through its Windows 8.3 short name instead.
        ROOT="$(cygpath -u "$(cygpath -m -s "$1")")"
        case "$ROOT" in *" "*)
            echo "error: the project path contains spaces and Windows has no short (8.3) name for it;" >&2
            echo "       move the project to a folder path without spaces." >&2
            exit 1 ;;
        esac
        echo "==> project path has spaces: building through its short name $ROOT" ;;
    esac
    PYEXE="$ROOT/tools/external/venv/Scripts/python.exe"
fi
VERSION="$2"
TOOLS="$ROOT/tools/external"
PATCHES="$TOOLS/old-gcc/patches"
TESTS="$TOOLS/old-gcc/tests"
DL="$TOOLS/_downloads"
OUTDIR="$TOOLS/old-gcc-build/gcc-$VERSION-psx"
WORKDIR="$ROOT/temp/old-gcc-work-$VERSION"

log() { echo "==> $*"; }

# --- per-version recipe (from decompals/old-gcc) ----------------------------
EXTRA_CFLAGS=""
WIDE_INT_TEST=yes
CONFIGURE_EXTRA="--disable-gprof --disable-gdb --disable-werror"
case "$VERSION" in
    2.6.3)
        URL="https://mirrors.slackware.com/slackware/slackware-2.2.0/source/d/gcc/gcc-2.6.3.tar.gz"
        apply_patches() {
            patch -u -p1 obstack.h -i "$PATCHES/obstack-2.7.2.h.patch"
            patch -u -p1 sdbout.c  -i "$PATCHES/sdbout-2.6.3.c.patch"
            patch -su -p1          <  "$PATCHES/psx-2.5.7.patch"
            touch insn-config.h
        }
        EXTRA_CFLAGS="-Dbsd4_4 -DHAVE_STRERROR"
        WIDE_INT_TEST=no     # not in upstream's 2.6.3 recipe either
        CONFIGURE_EXTRA=""   # 2.6.3's configure has no --disable-* options
        ;;
    2.7.2)
        URL="https://ftp.gnu.org/old-gnu/gcc/gcc-2.7.2.tar.gz"
        apply_patches() {
            patch -u -p1 obstack.h          -i "$PATCHES/obstack-2.7.2.h.patch"
            patch -u -p1 configure          -i "$PATCHES/configure.patch"
            patch -u -p1 config/mips/mips.h -i "$PATCHES/mipsel-2.7.patch"
            patch -su -p1                   <  "$PATCHES/psx-2.5.7.patch"
        }
        ;;
    2.8.1)
        URL="https://ftp.gnu.org/gnu/gcc/gcc-2.8.1.tar.gz"
        apply_patches() {
            patch -u -p1 obstack.h          -i "$PATCHES/obstack-2.8.1.h.patch"
            patch -u -p1 config/mips/mips.h -i "$PATCHES/mips.patch"
            patch -su -p1                   <  "$PATCHES/psx.patch"
        }
        ;;
    *) echo "unsupported version: $VERSION" >&2; exit 1 ;;
esac
# ------------------------------------------------------------------------------

mkdir -p "$DL"
TARBALL="$DL/gcc-$VERSION.tar.gz"
[ -s "$TARBALL" ] || curl -fL -o "$TARBALL" "$URL"
[ -s "$DL/config.guess" ] || curl -fsL -o "$DL/config.guess" "https://raw.githubusercontent.com/gcc-mirror/gcc/74af13c174714dd3b9f1ded4b39955f003c16361/config.guess"
[ -s "$DL/config.sub" ]   || curl -fsL -o "$DL/config.sub"   "https://raw.githubusercontent.com/gcc-mirror/gcc/6fad101f3063d722e3348d07dc93cf737f8709e4/config.sub"

rm -rf "$WORKDIR" && mkdir -p "$WORKDIR" && cd "$WORKDIR"
log "extracting gcc-$VERSION"
tar xzf "$TARBALL"
cd "gcc-$VERSION"
chmod -R u+w .

log "patching"
sed -i 's/include <varargs\.h>/include <stdarg\.h>/g' *.c
apply_patches

cp "$DL/config.guess" "$DL/config.sub" .
chmod +x config.guess config.sub
sed -i 's/| hiux\* | abug | nacl\*/| psx* \\\
'"$(printf '\t')"'     | hiux* | abug | nacl*/' config.sub

# MSYS2 has no system <stab.h> (Linux/macOS do); use GCC's bundled copy.
cp gstab.h stab.h

# Host header: identical to xm-darwin.h (LP64, 32-bit HOST_WIDE_INT).
cp "$PATCHES/xm-darwin.h" config/xm-msys.h
awk '/^\t\*\)$/ { buf=$0; next }
     buf != "" { if (!done && /echo.*Configuration.*not supported/) {
       print "\t*-pc-msys*|x86_64-*-linux*)"; print "\t\txm_file=xm-msys.h"
       print "\t\tfixincludes=Makefile.in"; print "\t\t;;"; done=1 }
       print buf; buf="" } { print }' configure > configure.tmp
grep -q 'xm-msys.h' configure.tmp || { echo "could not add the msys host case to configure" >&2; exit 1; }
mv configure.tmp configure
chmod +x configure

log "configure"
# CC with -std=gnu89: 1990s autoconf test programs (implicit int) fail under GCC 15's default C23.
CC="gcc -std=gnu89 -w" ./configure \
    --target=mips-sony-psx \
    --prefix=/opt/cross \
    --with-endian-little \
    --with-gnu-as \
    $CONFIGURE_EXTRA > "$WORKDIR/configure.log" 2>&1 \
    || { tail -20 "$WORKDIR/configure.log"; exit 1; }
touch insn-config.h

if [ "$VERSION" = 2.6.3 ]; then
    # LP64 host fix (2026-10-14): declare pointer-returning functions GCC calls without a
    # prototype - see gcc263_lp64_decls.py. insn-emit.c is generated, so make it first.
    LP64FIX="$ROOT/tools/gcc263_lp64_decls.py"
    make insn-emit.c CFLAGS="-std=gnu89 -w $EXTRA_CFLAGS" > "$WORKDIR/make-gen.log" 2>&1 \
        || { tail -20 "$WORKDIR/make-gen.log"; exit 1; }
    "$PYEXE" "$(native "$LP64FIX")" "$(native "$PWD")"
fi

MAKE_CFLAGS="-std=gnu89 -w -Wno-int-conversion -Wno-implicit-function-declaration -Wno-return-mismatch -Wno-incompatible-pointer-types $EXTRA_CFLAGS"
if [ "$WINDOWS" = 0 ]; then
    # cccp.c takes `struct tm` (for __DATE__ / __TIME__) from <sys/time.h>, as on BSD / macOS / MSYS2;
    # glibc declares it only in <time.h>.
    MAKE_CFLAGS="$MAKE_CFLAGS -include time.h"
fi
# The generated insn-*.h headers first, one job: the old Makefiles don't list them as dependencies of
# every object, so with --jobs an object can compile before its header exists (2026-10-17, 24 cores:
# "insn-codes.h: No such file or directory" in caller-save.o).
log "make generated headers"
make insn-config.h insn-flags.h insn-codes.h insn-attr.h CFLAGS="$MAKE_CFLAGS" \
    > "$WORKDIR/make-headers.log" 2>&1 \
    || { grep -E "error|Error" "$WORKDIR/make-headers.log" | head -30; exit 1; }
log "make cpp cc1"
make --jobs "$(nproc)" cpp cc1 CFLAGS="$MAKE_CFLAGS" \
    > "$WORKDIR/make.log" 2>&1 \
    || { grep -E "error|Error" "$WORKDIR/make.log" | head -30; exit 1; }

log "tests"
./cc1 -quiet -O2 "$TESTS/little_endian.c" -o little_endian.s
grep -E 'lbu\s\$2,0\(\$4\)' little_endian.s
./cc1 -quiet -O2 "$TESTS/section_attribute.c" -o /dev/null
if [ "$WIDE_INT_TEST" = yes ]; then
    ./cc1 -quiet -O2 "$TESTS/host_wide_int.c" -o host_wide_int.s
    grep -E '# 0x80000000$' host_wide_int.s
fi
./cc1 -quiet -help </dev/null 2>&1 | grep -q -- -msoft-float && echo "msoft-float ok"

mkdir -p "$OUTDIR"
cp "cpp$EXE" "cc1$EXE" "$OUTDIR/"
log "done: $OUTDIR"
