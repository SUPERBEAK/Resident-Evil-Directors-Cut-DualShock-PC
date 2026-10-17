"""nearmiss.py - show the closest non-matching drafts and exactly how they differ.

Run from dualshock/ with the project venv python:
    python tools/nearmiss.py --target stage1 [--max-dist 5] [--count 10] [--only f ...]
    python tools/nearmiss.py --target stage1 --only f --file my_version.c   (a hand-edited draft)

For drafts in build/auto/<target>/ whose status is diff:N (N <= --max-dist), the draft is
compiled and linked at its original address (refine.distance) and the differing instruction
runs are printed (original | built). Meant for spotting missing refine.py mutations.
"""

import argparse
import difflib
import json
import os
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import autodecomp as ad  # noqa: E402
import refine  # noqa: E402


def disasm(blob, addr, scratch):
    open(scratch, "wb").write(blob)
    out = subprocess.run([os.path.join(refine.BIN, "mips64-elf-objdump"), "-D", "-b", "binary",
                          "-m", "mips:3000", "-EL", f"--adjust-vma=0x{addr:X}", scratch],
                         capture_output=True, text=True).stdout
    return [l.split("\t", 2)[2].replace("\t", " ") for l in out.splitlines()
            if ":\t" in l and len(l.split("\t", 2)) > 2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="stage1")
    ap.add_argument("--max-dist", type=int, default=5)
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--file", help="diff this C file instead of the stored draft (one --only function)")
    args = ap.parse_args()
    ctx = refine.Ctx(args.target)
    res = json.load(open(os.path.join("build", "auto", args.target, "results.json")))
    if args.only:
        names = args.only
    else:
        cands = [(int(s.split(":")[1].split(",")[0]), n) for n, s in res.items()
                 if s.startswith("diff:") and int(s.split(":")[1].split(",")[0]) <= args.max_dist]
        names = [n for _, n in sorted(cands)[:args.count]]
    scratch = os.path.join(ctx.dir, "nearmiss.bin")
    for n in names:
        src = args.file or os.path.join("build", "auto", args.target, "c", f"{n}.c")
        code = open(src).read().split("\n/* failed:")[0]
        d = refine.distance(ctx, n, code, "near")
        work = os.path.join(ctx.dir, "work", n)
        if d is None:
            print(f"=== {n}: does not compile")
            shutil.rmtree(work, ignore_errors=True)
            continue
        elf = os.path.join(work, "near", "m.elf")
        got = refine.cm.read_elf(elf)[0][".text"][1]
        addr, osz = ad.func_addr(ctx.func_dir, n), ad.orig_size(ctx.func_dir, n)
        o0 = addr - ctx.base + 0x800
        a, b = disasm(ctx.orig[o0:o0 + osz], addr, scratch), disasm(got, addr, scratch)
        print(f"=== {n}: distance {d}, {osz} bytes")
        for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if op != "equal":
                print(f"  {op:7s} orig {a[i1:i2][:6]}\n          built {b[j1:j2][:6]}")
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
