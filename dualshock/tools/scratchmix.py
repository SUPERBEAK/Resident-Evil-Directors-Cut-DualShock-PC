"""scratchmix.py <func> <draft.c> <out.c> [--target main]: try every literal/struct choice per
distinct scratchpad address (`*(T *)0x1F80XXXX` vs `M2C_SFIELD(0x1F800000, T *, 0xXXXX)`, see
scratch.py) and keep the best. Capcom mixed both: an address whose value is also passed as a
pointer, or a loop counter kept in a register, stays literal (main func_800123C0, func_80012A9C).
Writes the best version to <out.c> and prints its distance."""
import argparse
import itertools
import os
import re
import subprocess
import sys

ap = argparse.ArgumentParser()
ap.add_argument("func")
ap.add_argument("draft")
ap.add_argument("out")
ap.add_argument("--target", default="main")
a = ap.parse_args()
s0 = open(a.draft).read()
pat = re.compile(r"\*\(([A-Za-z_]\w*(?:\s*\*)*?)\s*\*\)0x1F80([0-9A-F]{4})\b")
addrs = sorted(set(m.group(2) for m in pat.finditer(s0.split("{", 1)[-1])))
tmp = os.path.join(os.path.dirname(os.path.abspath(a.out)), "_scratchmix.c")
best = None
for combo in itertools.product([0, 1], repeat=len(addrs)):
    chosen = {ad for ad, c in zip(addrs, combo) if c}
    head, sep, body = s0.partition("{")
    body = pat.sub(lambda m: f"M2C_SFIELD(0x1F800000, {m.group(1)} *, 0x{m.group(2)})"
                   if m.group(2) in chosen else m.group(0), body)
    src = head + sep + body
    open(tmp, "w", newline="\n").write(src)
    r = subprocess.run([sys.executable, os.path.join("tools", "sidediff.py"), "--target", a.target,
                        a.func, "--file", tmp], capture_output=True, text=True).stdout.splitlines()
    d = r[0].split()[-1] if r else "?"
    d = int(d) if d.isdigit() else 9999
    if best is None or d < best[0]:
        best = (d, src, sorted(chosen))
    if d == 0:
        break
open(a.out, "w", newline="\n").write(best[1])
print(f"best distance {best[0]} with struct form for {best[2]} (of {addrs})")
