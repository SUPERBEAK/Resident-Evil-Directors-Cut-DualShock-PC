"""ptrlocal.py <in.c> <out.c>: an `s32 spXX;` local that holds an entity/table address and is used
in additions (`(i * 0x7C) + 0x7C + sp18`). As an int, GCC re-associates the constants
(`sp18 + 0x7C` first); Capcom's local was a pointer, which keeps the source order
(stage4 func_8011AA34, func_801181D4). Changes every such local to `void *`. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for m in list(re.finditer(r"\n([ \t]*)s32 (sp[0-9A-F]+);", s)):
    v = m.group(2)
    if re.search(rf"\+ {v}\b|\b{v} \+", s):
        s = s.replace(m.group(0), f"\n{m.group(1)}void *{v};", 1)
open(sys.argv[2], "w", newline="\n").write(s)
