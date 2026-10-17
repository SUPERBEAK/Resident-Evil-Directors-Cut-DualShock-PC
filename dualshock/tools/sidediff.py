"""sidediff.py - original vs. built instructions side by side, around each difference.

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/sidediff.py --target stage1 func_X [--file my_version.c] [--context 3]

Like nearmiss.py, but prints whole aligned windows (with addresses) instead of only the
changed runs, so an inserted/removed instruction is easy to place in the C.
"""

import argparse
import difflib
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refine  # noqa: E402
import nearmiss  # noqa: E402
import autodecomp as ad  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="stage1")
    ap.add_argument("--file")
    ap.add_argument("--context", type=int, default=3)
    ap.add_argument("name")
    args = ap.parse_args()
    ctx = refine.Ctx(args.target)
    n = args.name
    src = args.file or os.path.join("build", "auto", args.target, "c", f"{n}.c")
    code = open(src).read().split("\n/* failed:")[0]
    d = refine.distance(ctx, n, code, "side")
    work = os.path.join(ctx.dir, "work", n)
    if d is None:
        print("does not compile")
        return
    got = refine.cm.read_elf(os.path.join(work, "side", "m.elf"))[0][".text"][1]
    shutil.rmtree(work, ignore_errors=True)
    addr, osz = ad.func_addr(ctx.func_dir, n), ad.orig_size(ctx.func_dir, n)
    o0 = addr - ctx.base + 0x800
    scratch = os.path.join(ctx.dir, "side.bin")
    a = nearmiss.disasm(ctx.orig[o0:o0 + osz], addr, scratch)
    b = nearmiss.disasm(got, addr, scratch)
    print(f"=== {n}: distance {d}")
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for grp in sm.get_grouped_opcodes(args.context):
        print("----")
        for op, i1, i2, j1, j2 in grp:
            for k in range(max(i2 - i1, j2 - j1)):
                l = a[i1 + k] if i1 + k < i2 else ""
                r = b[j1 + k] if j1 + k < j2 else ""
                mark = " " if op == "equal" else "*"
                ad_ = f"{addr + 4 * (i1 + k):08X}" if i1 + k < i2 else "        "
                print(f"{mark} {ad_}  {l:34s} {r}")


if __name__ == "__main__":
    main()
