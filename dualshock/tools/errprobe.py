"""errprobe.py - why does a function's m2c draft not compile (after all automatic repairs)?

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/errprobe.py [--target main] func_X [func_Y ...]

Runs m2c (with the Sony prototypes as context), the K&R fix-ups and the error-driven repair
of tools/autodecomp.py, compiles the result alone and prints its status, then each remaining
compiler error with the draft line it points at. Nothing is written to results.json.
"""

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import autodecomp as ad  # noqa: E402
import compiler_matrix as cm  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("names", nargs="+")
    args = ap.parse_args()
    exe, base, func_dir, _ = cm.target_info(args.target)
    flags = ad.TARGET_FLAGS.get(args.target, ad.DEFAULT_OVERLAY_FLAGS)
    odir = ad.out_dir(args.target)
    shared = sorted(os.path.join(func_dir, f) for f in os.listdir(func_dir)
                    if f.endswith(".s") and not f.startswith("func_") and f != "main.s")
    ctx = ad.make_psyq_context()
    a = (func_dir, shared, ctx, open(exe, "rb").read(), base, flags, "2.6.3", odir)
    for name in args.names:
        cmd = [ad.M2C, "-t", "mips-gcc-c", "--valid-syntax", "--context", ctx,
               os.path.join(func_dir, name + ".s"), *shared]
        code = subprocess.run(cmd, capture_output=True, text=True, env=cm.ENV).stdout
        v = ad.kr_decls(name, code) or code
        v = ad.errfix(name, v, a) or v
        st, err = ad.check(name, v, a, "probe")
        print(f"=== {name}: {st}")
        pre = cm.preamble_for(v).count("\n")
        lines = v.split("\n")
        errs = re.findall(r"\.c:(\d+): (?!warning)(.*)", err)
        for ln, msg in errs[:8]:
            k = int(ln) - 1 - pre
            print(f"  {msg}\n      {lines[k].strip() if 0 <= k < len(lines) else '?'}")
        if st == "cc-fail" and not errs:          # not a C error: assembler/maspsx/crash
            tail = [l for l in err.strip().splitlines() if l.strip() and "warning" not in l][-4:]
            print("  (no C error) " + "\n  ".join(tail)[:600])


if __name__ == "__main__":
    main()
