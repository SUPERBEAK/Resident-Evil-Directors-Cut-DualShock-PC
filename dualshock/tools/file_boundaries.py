"""file_boundaries.py - estimate Capcom's original source-file boundaries in SLUS_007.47.

Run from dualshock/ with the project venv python:  python tools/file_boundaries.py

Idea: the linker concatenates each object's .rodata (and .text) in the same object order.
splat's jump-table alignment check gives rodata file splits (HINTS below, rodata file
offsets). For every rodata chunk between two splits, the functions referencing it must
belong to the same source file, so their address range brackets that file's .text.
Data (.data, 0x8008F93C+) references are reported per function too, as a second signal.
"""

import bisect
import os
import re

RODATA_START, RODATA_END = 0x80010000, 0x8001098C        # Capcom rodata
# splat "jumptables not aligned file-wise" suggestions (file offsets) for the capcom rodata
HINTS = [0x86C, 0x938, 0x974, 0xAD8, 0xBDC, 0xBF8, 0xCF4, 0xD20, 0xEDC, 0x1070, 0x10EC,
         0x1100, 0x1114, 0x1140, 0x1174]
FUNC_DIR = os.path.join("asm", "main", "nonmatchings", "capcom")


def vram(off):
    return off - 0x800 + 0x80010000


def main():
    starts = [RODATA_START] + [vram(h) for h in HINTS]
    chunk_names = [f"R{i:02d}@{s:08X}" for i, s in enumerate(starts)]

    # rodata symbols referenced by each function (by address in the symbol name)
    refs = {}
    for f in sorted(os.listdir(FUNC_DIR)):
        if not f.startswith("func_"):
            continue
        text = open(os.path.join(FUNC_DIR, f)).read()
        addrs = {int(a, 16) for a in re.findall(r"\b(?:D|jtbl)_(8001[0-9A-F]{4})\b", text)}
        addrs = {a for a in addrs if RODATA_START <= a < RODATA_END}
        if addrs:
            refs[f[:-2]] = addrs

    by_chunk = {}
    for fn, addrs in refs.items():
        for a in addrs:
            c = bisect.bisect_right(starts, a) - 1
            by_chunk.setdefault(c, set()).add(int(fn[5:], 16))

    print(f"{len(refs)} functions reference Capcom rodata; {len(starts)} rodata chunks\n")
    print(f"{'chunk':16s} {'#funcs':>6s}  first func   last func    (text range of referencing functions)")
    prev_last = 0
    for c in range(len(starts)):
        fs = sorted(by_chunk.get(c, []))
        if not fs:
            print(f"{chunk_names[c]:16s} {0:6d}  (unreferenced)")
            continue
        order = "" if fs[0] > prev_last else "   <-- overlaps previous chunk's functions"
        print(f"{chunk_names[c]:16s} {len(fs):6d}  0x{fs[0]:08X}  0x{fs[-1]:08X}{order}")
        prev_last = fs[-1]


if __name__ == "__main__":
    main()
