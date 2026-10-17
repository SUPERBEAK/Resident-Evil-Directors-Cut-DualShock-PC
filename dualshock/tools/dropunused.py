"""dropunused.py <in.c> <out.c>: remove block-scope temp_* declarations that are never used
(left behind by other transforms; at -O0 every local takes stack space, so an unused one makes
the frame too big - stage4 func_801110A0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for m in re.finditer(r"\n[ \t]*[\w ]+\*?\s*\b(temp_\w+);(?=\n)", s):
    if len(re.findall(rf"\b{m.group(1)}\b", s)) == 1:
        s = s.replace(m.group(0), "", 1)
open(sys.argv[2], "w", newline="\n").write(s)
