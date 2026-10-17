#!/bin/sh
# ghidra.sh - Linux counterpart of ghidra.bat: start an installed Ghidra 12.1.3 with a
# project-local settings/cache dir. The PS1 loader (ghidra_psx_ldr, installed by setup_tools.sh) lives in
# that settings dir, so your normal Ghidra profile (~/.ghidra) is never touched.
#
# Usage: GHIDRA_DIR=/path/to/ghidra_12.1.3_PUBLIC sh tools/ghidra.sh
if [ -z "$GHIDRA_DIR" ]; then
    echo "GHIDRA_DIR is not set: set it to your Ghidra 12.1.3 folder, the one containing ghidraRun." >&2
    exit 1
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools/external"
mkdir -p "$ROOT/temp"
export GHIDRA_JAVA_OPTIONS="$GHIDRA_JAVA_OPTIONS -Dapplication.settingsdir=$TOOLS/ghidra_settings -Dapplication.cachedir=$TOOLS/ghidra_cache -Dapplication.tempdir=$ROOT/temp"
exec "$GHIDRA_DIR/ghidraRun" "$@"
