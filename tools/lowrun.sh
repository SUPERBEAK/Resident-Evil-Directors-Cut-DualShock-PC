#!/bin/sh
# lowrun.sh "<command>": run a long batch from dualshock/ through buildenv.sh at BelowNormal process priority,
# (Windows) / nice 10 (Linux), so it doesn't slow down your other programs (child processes inherit it):
#
#   sh tools/lowrun.sh "make -j8"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/dualshock" || exit 1
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
    wpid=$(cat /proc/$$/winpid)
    powershell -NoProfile -c "(Get-Process -Id $wpid).PriorityClass='BelowNormal'"
    sh ../tools/buildenv.sh "$1" ;;
*)  exec nice -n 10 sh ../tools/buildenv.sh "$1" ;;
esac
