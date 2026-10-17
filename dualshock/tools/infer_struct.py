"""infer_struct.py - collect how a pointer is used, to reconstruct its struct layout.

Run from dualshock/ with the project venv python:
    python tools/infer_struct.py D_800CAB14 [--max-offset 0x400]

Scans every C draft (build/auto/*/c/*.c) and every source file (src/*/*.c) for m2c field
accesses `M2C_[S|P]FIELD(BASE, TYPE *, OFFSET)` where BASE is the given symbol, and prints
each offset with the access types seen (and how often). Accesses that agree across many
functions are strong evidence for a struct member of that type at that offset.
"""

import argparse
import collections
import glob
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base")
    ap.add_argument("--max-offset", type=lambda s: int(s, 0), default=0x10000)
    args = ap.parse_args()
    pat = re.compile(r"M2C_[SP]?FIELD\(\s*\(?" + re.escape(args.base) +
                     r"\)?\s*,\s*([\w ]+?)\s*\*\s*,\s*(-?0x[0-9A-Fa-f]+|-?\d+)\s*\)")
    seen = collections.defaultdict(collections.Counter)
    funcs = collections.defaultdict(set)
    files = glob.glob("build/auto/*/c/*.c") + glob.glob("src/*/*.c")
    for f in files:
        text = open(f, errors="replace").read()
        for m in pat.finditer(text):
            off = int(m.group(2), 0)
            if 0 <= off <= args.max_offset:
                seen[off][m.group(1).strip()] += 1
                funcs[off].add(f)
    print(f"{args.base}: {len(seen)} distinct offsets in {len(files)} files")
    print(f"{'offset':>8s}  {'files':>5s}  types (count)")
    for off in sorted(seen):
        types = ", ".join(f"{t}({c})" for t, c in seen[off].most_common())
        print(f"  0x{off:04X}  {len(funcs[off]):5d}  {types}")


if __name__ == "__main__":
    main()
