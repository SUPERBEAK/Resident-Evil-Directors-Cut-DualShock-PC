#!/bin/sh
# build_disc_make.sh <tools dir> <decomp dir> - called by build_disc.bat through the project's
# MSYS2 bash.exe: sets up the toolchain like tools/buildenv.sh, then rebuilds all 13 executables
# from scratch (make clean + make). make itself checks each one against the original (SHA1).
# The full compiler output (many harmless old-GCC warnings) goes to build/make.log; only the
# per-executable result lines are shown.
export MSYSTEM=MINGW64
. /etc/profile
T="$(cygpath -u "$1")"
export MIPS_TOOLCHAIN="$T/mips-toolchain"
export PATH="$MIPS_TOOLCHAIN/bin:$PATH"
cd "$(cygpath -u "$2")" || exit 1
make clean > build/make.log 2>&1 && make -j8 >> build/make.log 2>&1
rc=$?
grep -E "^(OK|MISMATCH)|All 13 executables|Error|error:" build/make.log
[ $rc -ne 0 ] && echo "make failed - full output in dualshock/build/make.log"
exit $rc
