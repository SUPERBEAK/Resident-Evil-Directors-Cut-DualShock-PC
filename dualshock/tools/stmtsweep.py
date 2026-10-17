"""stmtsweep.py <func> <template.c> <out.c> [--target main]: try every order of a block of
statements, and alternatives for each statement, until the function matches.

In the template, the block to permute sits between a `//STMTS` line and an `//END` line, one
statement per line (a line may hold several statements that stay together). Alternatives for a
line are separated by ` || `. Every order x every choice is compiled with sidediff.py; the first
exact match is written to <out.c>, otherwise the closest version is printed.

Example (main func_800470DC):
    //STMTS
    *(s32 *)0x800CF644 -= 4;
    (*(u8 *)0x800CF63A)--; || *(u8 *)0x800CF63A -= 1;
    p = *(u8 **)0x800CF640; *(u8 **)0x800CF640 = (u8 *)(p[1] + (s32)p);
    //END
Run from dualshock/ with the project venv python."""
import argparse
import itertools
import os
import re
import subprocess
import sys

ap = argparse.ArgumentParser()
ap.add_argument("func")
ap.add_argument("template")
ap.add_argument("out")
ap.add_argument("--target", default="main")
a = ap.parse_args()

t = open(a.template).read()
m = re.search(r"//STMTS\n(.*?)//END\n", t, re.S)
alts = [l.strip().split(" || ") for l in m.group(1).splitlines() if l.strip()]
tmp = os.path.join(os.path.dirname(os.path.abspath(a.out)), "_stmtsweep.c")
best = None
for perm in itertools.permutations(range(len(alts))):
    for choice in itertools.product(*[alts[i] for i in perm]):
        src = t.replace(m.group(0), "".join("    " + c.strip() + "\n" for c in choice))
        open(tmp, "w", newline="\n").write(src)
        r = subprocess.run([sys.executable, os.path.join("tools", "sidediff.py"), "--target", a.target,
                            a.func, "--file", tmp], capture_output=True, text=True).stdout.splitlines()
        d = r[0].split()[-1] if r else "?"
        d = int(d) if d.isdigit() else 9999
        if best is None or d < best[0]:
            best = (d, src)
        if d == 0:
            open(a.out, "w", newline="\n").write(src)
            print(f"MATCH -> {a.out}")
            sys.exit(0)
print(f"no match; closest distance {best[0]}:")
print(best[1])
