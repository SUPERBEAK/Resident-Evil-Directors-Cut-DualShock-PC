#!/bin/sh
# dump_discs.sh - extract the RE1 Dual Shock (USA) PS1 disc into extracted/ (git-ignored).
#
# Usage (Git Bash on Windows, a shell on Linux; any working directory):
#   sh tools/dump_discs.sh
#
# Reads game_files/RE1-Dualshock/*.cue (your own disc dump) and produces:
#   extracted/RE1-Dualshock/        all files from the disc (dumpsxiso), CD audio as WAV
#   extracted/RE1-Dualshock.xml     mkpsxiso rebuild script incl. original LBAs (-l)
#   extracted/RE1-Dualshock.sha1    SHA-1 of every extracted file
# Skipped if extracted/RE1-Dualshock exists; delete it to re-dump.

set -e
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
    # pwd -W: Windows form (C:/...). dumpsxiso.exe fails (exit 127, no output) when given /c/... paths.
    ROOT="$(cd "$(dirname "$0")/.." && pwd -W)"; EXE=.exe ;;
*)  ROOT="$(cd "$(dirname "$0")/.." && pwd)"; EXE= ;;
esac
T="$ROOT/tools/external"
OUT="$ROOT/extracted"
mkdir -p "$OUT"

v=RE1-Dualshock
cue="$(ls "$ROOT/game_files/$v/"*.cue)"
if [ -d "$OUT/$v" ]; then
    echo "==> $v: already dumped"
else
    echo "==> $v: dumpsxiso"
    "$T/mkpsxiso/dumpsxiso$EXE" -q -l -x "$OUT/$v" -s "$OUT/$v.xml" "$cue"
    echo "==> $v: file hashes"
    (cd "$OUT/$v" && find . -type f -exec sha1sum {} + | sort -k2) > "$OUT/$v.sha1"
fi
echo "==> done"
