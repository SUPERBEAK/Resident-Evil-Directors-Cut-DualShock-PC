"""mutprobe.py - which refine mutations bring a draft closer to the original?

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/mutprobe.py <target> <func> [kind ...]

Prints the distance (differing words, see refine.py) of the stored draft
build/auto/<target>/c/<func>.c and of every mutation refine.py generates for it (only the
given kinds, e.g. `symptr argfix`). None = doesn't compile/link. Use it to test a new mutation
or to see why refine got stuck on a function.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refine  # noqa: E402

t, name, kinds = sys.argv[1], sys.argv[2], set(sys.argv[3:])
ctx = refine.Ctx(t)
refine.ARG_CONSTS[name] = refine.asm_arg_consts(ctx.func_dir, name)
refine.ASM_TYPES[name] = refine.asm_sym_types(ctx.func_dir, name)
refine.DEAD_INITS[name] = refine.asm_dead_inits(ctx.func_dir, name)
code = open(os.path.join("build", "auto", t, "c", f"{name}.c")).read().split("\n/* failed:")[0]
print("start", refine.distance(ctx, name, code, "p0"))
for i, (k, c) in enumerate(refine.mutations(name, code, "-O0" in ctx.flags)):
    if kinds and k not in kinds:
        continue
    print(k, refine.distance(ctx, name, c, f"p{i % 4}"))
