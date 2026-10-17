#!/bin/sh
# setup_tools.sh - download, unpack and build all project tools into tools/external/.
#
# Usage (Git Bash on Windows, a shell on Linux x86_64; any working directory):
#   sh tools/setup_tools.sh
#
# Re-runnable: archives are cached in tools/external/_downloads and a tool whose folder
# already exists is skipped. Delete a tool's folder to reinstall it.
# Versions are pinned below; bump them here and re-run to upgrade.
#
# Linux needs these system packages (Debian/Ubuntu names):
#   curl git unzip tar make gcc patch diffutils binutils python3 python3-venv
# On Windows everything except Git for Windows and Python is downloaded (MSYS2 provides make/gcc/patch).

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools/external"
DL="$TOOLS/_downloads"
mkdir -p "$DL"

case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) WINDOWS=1; EXE=.exe ;;
Linux)
    WINDOWS=0; EXE=
    [ "$(uname -m)" = x86_64 ] || { echo "only x86_64 Linux is supported (the prebuilt tools are x86_64)" >&2; exit 1; }
    case "$ROOT" in *" "*)
        echo "the project path contains spaces ($ROOT); the old GCC build can't handle that on Linux:" >&2
        echo "move the project to a folder path without spaces" >&2; exit 1 ;;
    esac ;;
*) echo "unsupported system: $(uname -s) (Windows with Git Bash, or Linux x86_64)" >&2; exit 1 ;;
esac

# --- pinned versions ---------------------------------------------------------
MSYS2_URL="https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20260927.tar.xz"
# Prebuilt MIPS toolchain (binutils + gcc): assembles and links the decomp. Same release for both systems.
TOOLCHAIN_BASE="https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease"
TOOLCHAIN_WIN="$TOOLCHAIN_BASE/gcc-toolchain-mips64-win64.zip"
TOOLCHAIN_LINUX="$TOOLCHAIN_BASE/gcc-toolchain-mips64-x86_64.deb"
MKPSXISO_WIN="https://github.com/Lameguy64/mkpsxiso/releases/download/v2.30/mkpsxiso-2.30-win64.zip"
MKPSXISO_LINUX="https://github.com/Lameguy64/mkpsxiso/releases/download/v2.30/mkpsxiso-2.30-Linux.zip"
JPSXDEC_URL="https://github.com/m35/jpsxdec/releases/download/v2.1/jpsxdec_v2.1-beta.zip"
IMHEX_URL="https://github.com/WerWolv/ImHex/releases/download/v1.38.1/imhex-1.38.1-Windows-Portable-x86_64.zip"
GHIDRA_VER="12.1.3"
PSXLDR_URL="https://github.com/lab313ru/ghidra_psx_ldr/releases/download/2026.10.03/ghidra_${GHIDRA_VER}_PUBLIC_20261004_ghidra_psx_ldr.zip"
# PS1 decomp toolchain (commits pinned: maspsx and old-gcc decide the byte match)
MASPSX_GIT="https://github.com/mkst/maspsx.git"
MASPSX_COMMIT="e85ecb373828aabea96d7e6400974d4f91ce8c74"
OLDGCC_GIT="https://github.com/decompals/old-gcc.git"
OLDGCC_COMMIT="b74211c9d959e9724802f3177c8229cd67202c87"
PERMUTER_GIT="https://github.com/simonlindholm/decomp-permuter.git"
PERMUTER_COMMIT="8556c81d80d1c1af98a858c8f4dc951357f29139"
OBJDIFF_VER="v3.8.2"
# ------------------------------------------------------------------------------

log() { echo "==> $*"; }

# fetch <url> [<filename>] : download into the cache once, print the local path
fetch() {
    name="${2:-$(basename "$1")}"
    if [ ! -s "$DL/$name" ]; then
        log "downloading $name" >&2
        curl -fL --retry 3 -A "Mozilla/5.0" -o "$DL/$name.part" "$1"
        mv "$DL/$name.part" "$DL/$name"
    fi
    echo "$DL/$name"
}

# mv_retry <src> <dst> : antivirus briefly locks freshly extracted .exe files,
# which makes a directory rename fail with "Permission denied"; retry a few times.
mv_retry() {
    for i in 1 2 3 4 5 6 7 8 9 10; do
        mv "$1" "$2" 2>/dev/null && return 0
        sleep 3
    done
    mv "$1" "$2"
}

# unzip_to <zip> <dir> : extract; if the zip has one top folder, flatten it
unzip_to() {
    tmp="$2.tmp"
    rm -rf "$tmp" && mkdir -p "$tmp"
    unzip -q "$1" -d "$tmp"
    if [ "$(ls -A "$tmp" | wc -l)" -eq 1 ] && [ -d "$tmp/$(ls -A "$tmp")" ]; then
        mv_retry "$tmp/$(ls -A "$tmp")" "$2" && rmdir "$tmp"
    else
        mv_retry "$tmp" "$2"
    fi
}

install_zip() { # <dir-name> <url> [<filename>]
    if [ -d "$TOOLS/$1" ]; then log "$1: already installed"; return; fi
    f="$(fetch "$2" "$3")"
    log "$1: extracting"
    unzip_to "$f" "$TOOLS/$1"
}

# --- system tools -------------------------------------------------------------
if [ "$WINDOWS" = 1 ]; then
    # MSYS2 (make, bash, and the host gcc that builds the old PSX GCC)
    if [ ! -d "$TOOLS/msys64" ]; then
        f="$(fetch "$MSYS2_URL")"
        log "msys64: extracting"
        tar -xJf "$f" -C "$TOOLS"
        log "msys64: first-run init"
        "$TOOLS/msys64/usr/bin/bash.exe" -lc "true"
    else
        log "msys64: already installed"
    fi
    # Mirrors rate-limit (HTTP 429) parallel downloads: fetch one file at a time, retry.
    sed -i 's/^ParallelDownloads *=.*/ParallelDownloads = 1/' "$TOOLS/msys64/etc/pacman.conf"
    # pacman's own downloader gives up at the first 429 (Too Many Requests); curl --retry waits (growing
    # delays, honours Retry-After) and retries just that file. -C - resumes partial downloads.
    sed -i 's|^#\?XferCommand = /usr/bin/curl .*|XferCommand = /usr/bin/curl -L -C - -f --retry 10 -o %o %u|' "$TOOLS/msys64/etc/pacman.conf"
    # make: the decomp build. gcc/patch/diffutils (MSYS env): building the old PSX GCC.
    log "msys64: installing packages (make, msys gcc, patch, diffutils)"
    n=0
    until "$TOOLS/msys64/usr/bin/bash.exe" -lc \
        "pacman -S --needed --noconfirm make gcc patch diffutils" >/dev/null; do
        n=$((n + 1))
        [ "$n" -ge 5 ] && { echo "pacman failed 5 times (downloaded packages are kept: just run the script again)" >&2; exit 1; }
        log "msys64: pacman failed, retrying in 60s ($n/5)"
        sleep 60
    done
    PYTHON=python
else
    missing=""
    for c in curl git unzip tar make gcc patch diff ar python3; do
        command -v "$c" >/dev/null 2>&1 || missing="$missing $c"
    done
    python3 -c "import venv, ensurepip" 2>/dev/null || missing="$missing python3-venv"
    if [ -n "$missing" ]; then
        echo "missing system tools:$missing" >&2
        echo "(Debian/Ubuntu: sudo apt install curl git unzip tar make gcc patch diffutils binutils python3 python3-venv)" >&2
        exit 1
    fi
    log "system tools: found"
    PYTHON=python3
fi

# --- MIPS toolchain (mips64-elf-as/ld/objcopy assemble and link the decomp; mips64-elf-gcc: syncheck.py)
if [ "$WINDOWS" = 1 ]; then
    install_zip mips-toolchain "$TOOLCHAIN_WIN"
elif [ -d "$TOOLS/mips-toolchain" ]; then
    log "mips-toolchain: already installed"
else
    # The Linux build is a .deb: unpack it (ar + tar) instead of installing it system-wide.
    f="$(fetch "$TOOLCHAIN_LINUX")"
    log "mips-toolchain: extracting"
    tmp="$TOOLS/mips-toolchain.tmp"
    rm -rf "$tmp" && mkdir -p "$tmp"
    (cd "$tmp" && ar x "$f" && mkdir root && tar -xf data.tar.* -C root)
    as="$(find "$tmp/root" -path '*/bin/mips64-elf-as' | head -1)"
    [ -n "$as" ] || { echo "mips64-elf-as not found in $f" >&2; exit 1; }
    mv "$(dirname "$(dirname "$as")")" "$TOOLS/mips-toolchain"
    rm -rf "$tmp"
fi

# --- PS1 extraction (dumpsxiso/mkpsxiso) + jPSXdec (tools/bss_view.py: room backgrounds) ---
if [ "$WINDOWS" = 1 ]; then
    install_zip mkpsxiso "$MKPSXISO_WIN"
elif [ ! -d "$TOOLS/mkpsxiso" ]; then
    install_zip mkpsxiso "$MKPSXISO_LINUX"
    mv "$TOOLS/mkpsxiso/bin/"* "$TOOLS/mkpsxiso/"    # same place as on Windows: mkpsxiso/dumpsxiso
    rmdir "$TOOLS/mkpsxiso/bin"
else
    log "mkpsxiso: already installed"
fi
install_zip jpsxdec "$JPSXDEC_URL"

# --- misc ---------------------------------------------------------------------
if [ "$WINDOWS" = 1 ]; then
    install_zip imhex "$IMHEX_URL"
else
    log "imhex: skipped on Linux (optional hex editor; install it from your distribution if you want it)"
fi

# --- Ghidra extensions (installed into a project-local Ghidra settings dir) ---
# ghidra.bat / ghidra.sh start Ghidra with -Dapplication.settingsdir pointing here.
EXT="$TOOLS/ghidra_settings/ghidra/ghidra_${GHIDRA_VER}_PUBLIC/Extensions"
mkdir -p "$EXT"
for u in "$PSXLDR_URL"; do
    f="$(fetch "$u")"
    top="$(unzip -Z1 "$f" | head -1 | cut -d/ -f1)"
    if [ -d "$EXT/$top" ]; then log "ghidra ext $top: already installed"; continue; fi
    log "ghidra ext $top: extracting"
    unzip -q "$f" -d "$EXT"
done

# --- PS1 decomp toolchain -----------------------------------------------------
# Clones use LF line endings (core.autocrlf=false): old-gcc's patches break with CRLF.
clone_pinned() { # <dir-name> <git-url> [<commit>]
    if [ -d "$TOOLS/$1" ]; then log "$1: already cloned"; return; fi
    log "$1: cloning"
    git clone -q -c core.autocrlf=false "$2" "$TOOLS/$1"
    if [ -n "$3" ]; then git -C "$TOOLS/$1" -c advice.detachedHead=false checkout -q "$3"; fi
}
clone_pinned maspsx "$MASPSX_GIT" "$MASPSX_COMMIT"
clone_pinned old-gcc "$OLDGCC_GIT" "$OLDGCC_COMMIT"
# random C rewrites for almost-matching drafts (dualshock/tools/permbatch.py, permsetup.py)
clone_pinned decomp-permuter "$PERMUTER_GIT" "$PERMUTER_COMMIT"

if [ ! -f "$TOOLS/objdiff/objdiff-cli$EXE" ]; then
    log "objdiff: downloading $OBJDIFF_VER"
    mkdir -p "$TOOLS/objdiff"
    if [ "$WINDOWS" = 1 ]; then
        cp "$(fetch "https://github.com/encounter/objdiff/releases/download/$OBJDIFF_VER/objdiff-cli-windows-x86_64.exe" "objdiff-cli-$OBJDIFF_VER.exe")" "$TOOLS/objdiff/objdiff-cli.exe"
        cp "$(fetch "https://github.com/encounter/objdiff/releases/download/$OBJDIFF_VER/objdiff-windows-x86_64.exe" "objdiff-$OBJDIFF_VER.exe")" "$TOOLS/objdiff/objdiff.exe"
    else
        cp "$(fetch "https://github.com/encounter/objdiff/releases/download/$OBJDIFF_VER/objdiff-cli-linux-x86_64" "objdiff-cli-$OBJDIFF_VER-linux")" "$TOOLS/objdiff/objdiff-cli"
        cp "$(fetch "https://github.com/encounter/objdiff/releases/download/$OBJDIFF_VER/objdiff-linux-x86_64" "objdiff-$OBJDIFF_VER-linux")" "$TOOLS/objdiff/objdiff"
        chmod +x "$TOOLS/objdiff/objdiff-cli" "$TOOLS/objdiff/objdiff"
    fi
else
    log "objdiff: already installed"
fi

# --- Python venv (before the old GCC: building 2.6.3 runs tools/gcc263_lp64_decls.py with it) ---
if [ "$WINDOWS" = 1 ]; then VENV_PY="$TOOLS/venv/Scripts/python.exe"; else VENV_PY="$TOOLS/venv/bin/python"; fi
if [ ! -d "$TOOLS/venv" ]; then
    log "venv: creating"
    "$PYTHON" -m venv "$TOOLS/venv"
fi
log "venv: installing packages"
"$VENV_PY" -m pip install -q --upgrade pip
"$VENV_PY" -m pip install -q -r "$ROOT/tools/requirements.txt"
log "m2c: downloading m2c_macros.h (GPL-3.0, not in git) into dualshock/include"
(cd "$ROOT/dualshock" && "$VENV_PY" tools/m2c_macros.py)

# 2.6.3 = RE1's compiler; 2.7.2 / 2.8.1 kept for per-file comparisons.
for v in 2.6.3 2.7.2 2.8.1; do
    if [ ! -f "$TOOLS/old-gcc-build/gcc-$v-psx/cc1$EXE" ]; then
        log "old-gcc: building gcc-$v-psx (cpp + cc1)"
        sh "$ROOT/tools/build_old_gcc.sh" "$v"
    else
        log "old-gcc: gcc-$v-psx already built"
    fi
done

log "done"
