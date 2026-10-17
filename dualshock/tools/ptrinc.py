"""ptrinc.py <in.c> <out.c>: a global record pointer advanced by its record size, which m2c types
as `void *` and writes `D_X += 0xC;`. Capcom's `D_X++` on a pointer to a 12-byte struct leaves
an extra `move` that the void-pointer addition doesn't (ending func_800E2510, func_800E2860).
Declare `extern struct { u8 b[K]; } *D_X;` and use `D_X++;` (or `D_X--;` for `-= K`).
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for m in list(re.finditer(r"^([ \t]*)(D_[0-9A-F]{8}) ([+-])= (0x[0-9A-F]+|\d+);$", s, re.M)):
    name, k = m.group(2), int(m.group(4), 0)
    if not re.search(rf"^extern void \*{name};$", s, re.M) or k < 2:
        continue
    incs = re.findall(rf"^[ \t]*{name} [+-]= (0x[0-9A-F]+|\d+);$", s, re.M)
    if any(int(x, 0) != k for x in incs):
        continue
    s = re.sub(rf"^([ \t]*){name} ([+-])= (0x[0-9A-F]+|\d+);$", lambda mm: f"{mm.group(1)}{name}{mm.group(2) * 2};", s, flags=re.M)
    s = re.sub(rf"^extern void \*{name};$", f"extern struct {{ u8 b[{k}]; }} *{name};", s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
