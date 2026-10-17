"""emptycase.py <in.c> <out.c>: the function's jump table (from its .s) has one more entry than
the draft's highest `case`, and that entry is the switch's exit: Capcom's switch ended with an
empty `case N:;` (an idiom seen before). Replace m2c's trailing
`/* fallthrough */ default: return;` with `break; case N:;` (stage4 func_80124690; the plain
`case N:;` variant comes from refine's dropdefault). Derives the .s from the draft path
build/auto/<target>/c/<func>.c. For trybatch."""
import glob
import os
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
p = os.path.abspath(src).replace("\\", "/")
m = re.search(r"build/auto/(\w+)/c/(func_[0-9A-F]{8})\.c$", p)
if m:
    asm = glob.glob(f"asm/{m.group(1)}/nonmatchings/**/{m.group(2)}.s", recursive=True)
    if asm:
        a = open(asm[0]).read()
        tables = re.findall(r"dlabel jtbl_\w+\n(.*?)enddlabel", a, re.S)
        cases = [int(c, 0) for c in re.findall(r"\n[ \t]*case (0x[0-9A-F]+|\d+):", s)]
        if len(tables) == 1 and cases:
            n = len(re.findall(r"\.word \.L", tables[0]))
            top = max(cases)
            if n == top + 2:
                s = re.sub(r"\n([ \t]*)/\* fallthrough \*/\n([ \t]*)default:[^\n]*\n[ \t]*return;\n",
                           lambda mm: f"\n{mm.group(1)}break;\n{mm.group(2)}case {n - 1}:;\n", s, count=1)
open(dst, "w", newline="\n").write(s)
