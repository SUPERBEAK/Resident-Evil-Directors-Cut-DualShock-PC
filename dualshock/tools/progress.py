"""progress.py - how much Capcom code is decompiled (milestone M3).

Run from dualshock/ with the project venv python:  python tools/progress.py

A function counts as decompiled when its INCLUDE_ASM line is gone from src/ (it is then C
that compiles to the original bytes - `make` would fail otherwise). Sizes come from the
splat function files in asm/*/nonmatchings/ (first line: `nonmatching name, 0xSIZE`).
"""

import os
import re


def sizes(asm_root):
    out = {}
    for root, _, files in os.walk(asm_root):
        if "nonmatchings" not in root:
            continue
        for f in files:
            # any name: since 2026-10-16 every function has a real name (no func_XXXXXXXX left)
            if f.endswith(".s"):
                # Not necessarily the first line: migrated jump tables (.rodata) can come first.
                m = re.search(rf"^nonmatching {f[:-2]},\s*(0x[0-9A-Fa-f]+)",
                              open(os.path.join(root, f)).read(), re.M)
                if m:
                    out[f[:-2]] = int(m.group(1), 16)
    return out


def main():
    total_f = done_f = total_b = done_b = 0
    for target in sorted(os.listdir("src")):
        src_dir = os.path.join("src", target)
        text = "".join(open(os.path.join(dp, f)).read()
                       for dp, _, fs in os.walk(src_dir) for f in fs if f.endswith(".c"))
        pending = set(re.findall(r"INCLUDE_ASM\([^,]+,\s*(\w+)\)", text))
        fsz = sizes(os.path.join("asm", target))
        if not fsz:                  # src/shared: compiled into the stages, counted there
            continue
        d = {k: v for k, v in fsz.items() if k not in pending}
        tf, df = len(fsz), len(d)
        tb, db = sum(fsz.values()), sum(d.values())
        total_f, done_f, total_b, done_b = total_f + tf, done_f + df, total_b + tb, done_b + db
        print(f"{target:10s} {df:5d}/{tf:<5d} functions  {db:8,d}/{tb:<9,d} bytes  "
              f"({100 * db / tb if tb else 0:5.2f}%)")
    print(f"{'total':10s} {done_f:5d}/{total_f:<5d} functions  {done_b:8,d}/{total_b:<9,d} bytes  "
          f"({100 * done_b / total_b if total_b else 0:5.2f}%)")


if __name__ == "__main__":
    main()
