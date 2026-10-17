"""padsweep.py <target> [...]: give already-decompiled overlay functions whose original jump table
ends in Capcom's 8-byte pad word (integrate.needs_rodata_pad) the RODATA_PAD line after them.

Why: a C function without its pad still matches while the *next* rodata comes from an
INCLUDE_ASM .s file, because that file's own `.align 3` (the Makefile only rewrites the
compiler's output to `.align 2`) re-aligns the next table and fills the gap. As soon as that next
function is integrated too, the gap disappears and everything after it shifts by 4 bytes
(stage6 func_80121878 / func_80121D00).

Some trailing zero words are really the alignment of whatever follows (an INCLUDE_RODATA block,
another C function's data), so each candidate is tried on its own and kept only if the build
still matches. Run from dualshock/ with the project venv python."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402

DEF = re.compile(r"^[A-Za-z_][\w \*]*?\b(func_[0-9A-F]{8})\s*\([^;]*\)\s*\{\s*$")
PAD_CODE = '__asm__(".section .rodata\\n\\t.word 0\\n\\t.text");'


def candidates(t, text):
    lines = text.split("\n")
    out = []
    for k, l in enumerate(lines):
        m = DEF.match(l)
        if not m:
            continue
        j = k + 1
        while j < len(lines) and lines[j] != "}":
            j += 1
        follow = "\n".join(lines[j + 1:j + 5])
        if ig.needs_rodata_pad(os.path.join(t.folder, m.group(1) + ".s")) and PAD_CODE not in follow:
            out.append(m.group(1))
    return out


def with_pad(text, fn):
    lines = text.split("\n")
    k = next(n for n, l in enumerate(lines) if DEF.match(l) and DEF.match(l).group(1) == fn)
    while lines[k] != "}":
        k += 1
    return "\n".join(lines[:k + 1] + ig.RODATA_PAD.rstrip("\n").split("\n") + lines[k + 1:])


for name in sys.argv[1:]:
    if name == "main":
        continue
    t = ig.Target(name)
    cands = candidates(t, open(t.cfile).read())
    if not cands:
        print(f"{name}: nothing to add")
        continue
    print(f"{name}: candidates: {' '.join(cands)}")
    kept = []
    for fn in cands:
        cur = open(t.cfile).read()
        ig.write_file(t.cfile, with_pad(cur, fn))
        ok, _ = ig.build(t)
        if ok:
            kept.append(fn)
            print(f"  {fn}: pad kept (build matches)")
        else:
            ig.write_file(t.cfile, cur)
            print(f"  {fn}: pad breaks the build - not added")
    ig.build(t)
    print(f"{name}: added {len(kept)} pads")
