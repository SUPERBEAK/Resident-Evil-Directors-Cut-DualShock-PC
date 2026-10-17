#!/bin/bash
# build_old_gcc.sh - build a PSX GCC (cpp + cc1) natively in MSYS2. (Windows / MSYS2 only)
#
# Usage (from Git Bash, any working directory):
#   sh tools/build_old_gcc.sh [VERSION]    # VERSION: 2.7.2 (default), 2.6.3, 2.8.1
#
# It re-launches itself inside the project's MSYS2 (MSYS environment, x86_64,
# LP64 like macOS) and follows decompals/old-gcc's per-version recipes
# (gcc-<ver>-psx.Dockerfile / -macos.sh):
#   * same source tarball and patches (tools/external/old-gcc/patches),
#   * modern config.guess/config.sub (pinned to the same gcc-mirror commits as the macOS build),
#   * the macOS host header (forces 32-bit HOST_WIDE_INT like the Linux i386
#     reference build) installed as config/xm-msys.h for *-pc-msys hosts,
#   * MSYS2 fix: GCC's bundled gstab.h copied to stab.h (no system <stab.h>).
# Only cpp and cc1 are built: decomp builds call them directly (no gcc driver).
# Output: tools/external/old-gcc-build/gcc-<ver>-psx/{cpp,cc1}.exe (+ msys-2.0.dll dependency).

set -e

if [ -z "$OLDGCC_IN_MSYS" ]; then
    ROOT_W="$(cd "$(dirname "$0")/.." && pwd -W)"
    exec "$ROOT_W/tools/external/msys64/usr/bin/bash.exe" -c \
        'export MSYSTEM=MSYS OLDGCC_IN_MSYS=1; . /etc/profile; exec bash "$(cygpath -u "$1")" "$2" "$3"' \
        _ "$ROOT_W/tools/build_old_gcc_diag.sh" "$ROOT_W" "${1:-2.7.2}"
fi

ROOT="$(cygpath -u "$1")"
VERSION="$2"
TOOLS="$ROOT/tools/external"
PATCHES="$TOOLS/old-gcc/patches"
TESTS="$TOOLS/old-gcc/tests"
DL="$TOOLS/_downloads"
OUTDIR="$ROOT/temp/old-gcc-diag-out-$VERSION"
WORKDIR="$ROOT/temp/old-gcc-diag-$VERSION"

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
       print "\t*-pc-msys*)"; print "\t\txm_file=xm-msys.h"
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

log "make cpp cc1"
make --jobs "$(nproc)" cpp cc1 \
    CFLAGS="-std=gnu89 -g -Wimplicit-function-declaration -Wint-to-pointer-cast -Wpointer-to-int-cast -Wno-int-conversion -Wno-return-mismatch -Wno-incompatible-pointer-types $DIAG_EXTRA $EXTRA_CFLAGS" \
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
cp cpp.exe cc1.exe "$OUTDIR/"
log "done: $OUTDIR"
