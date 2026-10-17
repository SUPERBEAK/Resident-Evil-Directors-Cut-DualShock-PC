"""port.py <src_target> <src_func> <dst_target> <dst_func> <out.c>: copy a matched function's C to a
near-identical copy in another overlay. Symbol references (func_X, D_X, jtbl_X) are paired up in
order of first appearance in the two asm listings and renamed, like propagate.py does for exact
twins - but this also works when the copies differ slightly, so the result can then be refined
by hand (stage5 func_8011D6BC from stage3 func_80126040). Prints the symbol mapping."""
import os
import re
import sys

SYM = re.compile(r"\b(?:func|D|jtbl)_[0-9A-F]{8}\b")


def syms(target, func):
    sub = "capcom" if target == "main" else target
    p = os.path.join("asm", target, "nonmatchings", sub, f"{func}.s")
    out = []
    for line in open(p):
        if line.lstrip().startswith(("glabel", "dlabel", "nonmatching", "enddlabel", ".size", "endlabel")):
            continue
        for s in SYM.findall(line):
            if s not in out:
                out.append(s)
    return out


st, sf, dt, df, out = sys.argv[1:6]
a, b = syms(st, sf), syms(dt, df)
if len(a) != len(b):
    print(f"warning: {len(a)} vs {len(b)} symbols - pairing the common prefix only")
mapping = {x: y for x, y in zip(a, b) if x != y}
mapping[sf] = df
src = open(os.path.join("build", "auto", st, "c", f"{sf}.c")).read()
res = SYM.sub(lambda m: mapping.get(m.group(0), m.group(0)), src)
open(out, "w", newline="\n").write(res)
for k, v in mapping.items():
    print(f"  {k} -> {v}")
