"""jtinsert.py <func> <draft.c> [--target main]: integrate a function whose switch jump table is
4-aligned in the original (integrate.py cannot carry the marker): replaces its INCLUDE_ASM in
src/<target>/*.c with `__asm__("#JTALIGN2");` + the draft (see tools/jtalign.awk), builds, and
restores the C file if the build does not match. Run from dualshock/ through buildenv.sh.
The draft must be self-contained (its extern declarations are inserted as they are)."""
import argparse
import glob
import re
import subprocess

def fresh_make():
    """make with capcom.o removed first: a C file rewritten within the same second as the last
    object keeps make from recompiling it, and the OLD build then 'matches' (2026-10-15 incidents)"""
    import os, subprocess
    for o in ('build/src/main/capcom.o', 'build/src/main/capcom.i', 'build/src/main/capcom.s', 'build/src/main/capcom.ms.s'):
        try:
            os.remove(o)
        except OSError:
            pass
    return subprocess.run(["make", "-j12"], capture_output=True, text=True)

ap = argparse.ArgumentParser()
ap.add_argument("func")
ap.add_argument("draft")
ap.add_argument("--target", default="main")
a = ap.parse_args()

pat = re.compile(r'INCLUDE_ASM\("[^"]+", ' + a.func + r'\);')
files = [p for p in glob.glob(f"src/{a.target}/*.c") if pat.search(open(p).read())]
assert len(files) == 1, files
path = files[0]
old = open(path).read()
code = open(a.draft).read().strip()
marker = '' if '#JTALIGN2' in code else '/* 4-aligned jump table (tools/jtalign.awk) */\n__asm__("#JTALIGN2");\n'
new = pat.sub(lambda m: marker + code, old, count=1)
# like integrate.py: older callers' own declarations of this function become private aliases,
# else a later `M2C_UNK f();` conflicts with the definition (main func_800544E0, 2026-10-15)
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate  # noqa: E402
new, _ = integrate.alias_old_decls(new, a.func)
open(path, "w", newline="\n").write(new)
r = fresh_make()
log = r.stdout + r.stderr
if r.returncode == 0 and "All 13 executables match" in log:
    print(f"OK: {a.func} integrated in {path}, build matches")
else:
    open(path, "w", newline="\n").write(old)
    fresh_make()
    print(f"reverted {a.func}:")
    print("\n".join(l for l in log.splitlines() if "MISMATCH" in l or "error" in l.lower())[-2000:])
