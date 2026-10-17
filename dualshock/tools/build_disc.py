"""build_disc.py - build a Resident Evil Director's Cut Dual Shock (USA) disc image from the decomp.

Run from dualshock/ after `make` (build_disc.bat does both). Steps:
  1. Unpack the original disc once (dumpsxiso) into build/disc_work/: every file, the license
     sectors, the CD audio track and an XML describing the exact layout.
  2. Replace the 13 executables in that tree with the ones compiled from src/ (build/).
  3. Rebuild the image with mkpsxiso and split it into "(Track 1).bin" (data) and "(Track 2).bin"
     (CD audio, from its 2-second pregap on) plus a .cue - the same files as the original dump.
  4. Print the CRC32 of each output file (and of the original's, if found) for comparison.

The game's assets (movies, sounds, rooms, ...) and the audio track come from the original disc;
only the 13 program files are built from this project.
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DECOMP = os.path.dirname(HERE)
ROOT = os.path.dirname(DECOMP)
TOOLS = os.path.join(ROOT, "tools", "external", "mkpsxiso")
EXE = ".exe" if os.name == "nt" else ""
ORIG_DIR = os.path.join(ROOT, "game_files", "RE1-Dualshock")
# The dump's own name (the one .cue in game_files/RE1-Dualshock, like tools/dump_discs.sh); the usual
# Redump name if there isn't exactly one.
_CUES = glob.glob(os.path.join(ORIG_DIR, "*.cue"))
NAME = os.path.splitext(os.path.basename(_CUES[0]))[0] if len(_CUES) == 1 else \
    "Resident Evil - Director's Cut - Dual Shock Ver. (USA)"
ORIG_CUE = os.path.join(ORIG_DIR, NAME + ".cue")
WORK = os.path.join(DECOMP, "build", "disc_work")
OUT = os.path.join(DECOMP, "build", "disc")
EXES = ["SLUS_007.47"] + ["PSXUSA/PROG2/%s.EXE" % n for n in (
    "LOGO", "PROLOGUE", "TITLE", "SELECT", "ENDING",
    "STAGE1", "STAGE2", "STAGE3", "STAGE4", "STAGE5", "STAGE6", "STAGE7")]
SECTOR = 2352


def run(args):
    r = subprocess.run(args, cwd=WORK)
    if r.returncode != 0:
        sys.exit("FAILED: %s" % " ".join(args))


def crc32(path):
    c = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(1 << 24)
            if not b:
                return c & 0xFFFFFFFF
            c = zlib.crc32(b, c)


def msf_to_sectors(msf):
    m, s, f = (int(x) for x in msf.split(":"))
    return (m * 60 + s) * 75 + f


def main():
    for e in EXES:
        if not os.path.isfile(os.path.join(DECOMP, "build", os.path.basename(e))):
            sys.exit("build/%s is missing - run make first (build_disc.bat does)" % os.path.basename(e))

    # 1. unpack the original disc (once)
    os.makedirs(WORK, exist_ok=True)
    xml = os.path.join(WORK, "layout.xml")
    if not os.path.isfile(xml):
        if not os.path.isfile(ORIG_CUE):
            sys.exit("original disc not found: %s" % ORIG_CUE)
        print("Unpacking the original disc into build/disc_work (first run only)...")
        run([os.path.join(TOOLS, "dumpsxiso" + EXE), "-q", "-e", "pcm", "-x", "extracted",
             "-s", "layout.xml", ORIG_CUE])

    # 2. our executables replace the original ones
    for e in EXES:
        shutil.copyfile(os.path.join(DECOMP, "build", os.path.basename(e)),
                        os.path.join(WORK, "extracted", *e.split("/")))
    print("Copied the 13 executables built from src/ into the disc tree.")

    # 3. rebuild the image, then split it like the original dump
    print("Building the disc image with mkpsxiso...")
    run([os.path.join(TOOLS, "mkpsxiso" + EXE), "-q", "-y", "-o", "image.bin", "-c", "image.cue",
         "layout.xml"])
    cue = open(os.path.join(WORK, "image.cue")).read()
    m = re.search(r"TRACK 02 AUDIO\s+INDEX 00 (\d+:\d+:\d+)", cue)
    if not m:
        sys.exit("unexpected cue sheet from mkpsxiso:\n" + cue)
    split = msf_to_sectors(m.group(1)) * SECTOR

    os.makedirs(OUT, exist_ok=True)
    t1 = os.path.join(OUT, NAME + " (Track 1).bin")
    t2 = os.path.join(OUT, NAME + " (Track 2).bin")
    image = os.path.join(WORK, "image.bin")
    with open(image, "rb") as src:
        with open(t1, "wb") as dst:
            left = split
            while left:
                b = src.read(min(left, 1 << 24))
                dst.write(b)
                left -= len(b)
        with open(t2, "wb") as dst:
            shutil.copyfileobj(src, dst, 1 << 24)
    os.remove(image)
    with open(os.path.join(OUT, NAME + ".cue"), "w", newline="\r\n") as f:
        f.write('FILE "%s (Track 1).bin" BINARY\n' % NAME)
        f.write("  TRACK 01 MODE2/2352\n")
        f.write("    INDEX 01 00:00:00\n")
        f.write('FILE "%s (Track 2).bin" BINARY\n' % NAME)
        f.write("  TRACK 02 AUDIO\n")
        f.write("    INDEX 00 00:00:00\n")
        f.write("    INDEX 01 00:02:00\n")

    # 4. report
    print()
    print("Output folder: %s" % OUT)
    print()
    print("%-12s %-10s %-10s %s" % ("File", "Built", "Original", "Result"))
    for label, suffix in (("Track 1", " (Track 1).bin"), ("Track 2", " (Track 2).bin"), ("cue", ".cue")):
        built = crc32(os.path.join(OUT, NAME + suffix))
        orig_path = os.path.join(ORIG_DIR, NAME + suffix)
        orig = crc32(orig_path) if os.path.isfile(orig_path) else None
        print("%-12s %08X   %-10s %s" % (
            label, built, "%08X" % orig if orig is not None else "-",
            "-" if orig is None else ("MATCH" if built == orig else "DIFFERENT")))


if __name__ == "__main__":
    main()
