"""selfadd.py <in.c> <out.c>: m2c sometimes writes `X += call(...)` as
`temp = X = (u8) (temp + call(...));` with `temp` never initialised (it lost track of the load
of X across the call). Rewrite as `X += call(...);` and drop the temp (stage6 func_8011E2CC,
stage3 func_801171A0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"^([ \t]*)(temp_\w+) = ([^=\n]+?) = \((?:u8|s8|u16|s16|s32|u32)\) \(\2 \+ (.+)\);$", re.M)
while True:
    m = pat.search(s)
    if not m:
        break
    t = m.group(2)
    s = s[:m.start()] + f"{m.group(1)}{m.group(3)} += {m.group(4)};" + s[m.end():]
    if len(re.findall(rf"\b{t}\b", s)) == 1:
        s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{t};(?=\n)", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
