#!/bin/sh
# buildenv.sh - run a command with the MIPS toolchain on PATH and MIPS_TOOLCHAIN set.
# On Windows the command runs inside the project's MSYS2 (MINGW64); on Linux in the normal shell.
#
# Usage (Git Bash on Windows, any shell on Linux; any working directory):
#   sh tools/buildenv.sh "make -C src"
#   sh tools/buildenv.sh            # interactive shell
#
# The command runs in the directory you call this script from.

case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) ;;    # Windows: see below
*)
    ROOT="$(cd "$(dirname "$0")/.." && pwd)"
    export MIPS_TOOLCHAIN="$ROOT/tools/external/mips-toolchain"
    export PATH="$MIPS_TOOLCHAIN/bin:$PATH"
    if [ $# -eq 0 ]; then
        exec "${SHELL:-sh}" -i
    fi
    exec sh -c "$*"
    ;;
esac

# Windows. The caller's PATH is appended so Windows tools (python, ...) are still found.
# Note: environment variables do NOT survive the jump from Git Bash's msys runtime into
# MSYS2's bash.exe (different runtime copies), so everything is passed as positional
# arguments and MSYSTEM / profile are set up by hand.

ROOT="$(cd "$(dirname "$0")/.." && pwd -W)"
TOOLS_W="$ROOT/tools/external"
CWD_W="$(pwd -W)"
PATH_W="$(cygpath -w -p "$PATH")"

SETUP='export MSYSTEM=MINGW64
. /etc/profile
T="$(cygpath -u "$1")"
export MIPS_TOOLCHAIN="$T/mips-toolchain"
export PATH="$MIPS_TOOLCHAIN/bin:$PATH:$(cygpath -u -p "$3")"
cd "$(cygpath -u "$2")" || exit 1'

if [ $# -eq 0 ]; then
    exec "$TOOLS_W/msys64/usr/bin/bash.exe" -c "$SETUP
exec bash --norc -i" buildenv "$TOOLS_W" "$CWD_W" "$PATH_W"
else
    exec "$TOOLS_W/msys64/usr/bin/bash.exe" -c "$SETUP
$*" buildenv "$TOOLS_W" "$CWD_W" "$PATH_W"
fi
