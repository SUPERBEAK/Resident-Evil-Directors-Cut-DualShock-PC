"""deadlabel.py <in.c> <out.c>: remove labels that no `goto` uses. m2c sometimes leaves one
behind (stage4 func_80129BE8 had a stray `sp18:`); GCC 2.6.3 emits an extra `nop` for a user
label next to its own, so a dead label costs a byte-match. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for lab in re.findall(r"^(\w+):\s*$", s, re.M):
    if lab in ("default",) or re.search(rf"\bgoto {lab};", s):
        continue
    s = re.sub(rf"^{lab}:\s*\n", "", s, count=1, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
