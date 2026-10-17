"""narrowparam.py <in.c> <out.c>: a narrow (u8/s8/u16/s16) parameter. GCC -O0 copies it from its
incoming int into its own narrow home slot (`lw v1,32(s8); sb v1,2(s8)`), which m2c shows as a
local plus a cast: `u8 sp2; ... sp2 = (u8) arg4;`. Make the parameter itself narrow and use it
directly (stage5 func_8012AE80, stage3 func_801310B0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
while True:
    m = re.search(r"\n[ \t]*(?P<t>[su](?:8|16)) (?P<v>sp[0-9A-F]+);\n", s)
    if not m:
        break
    t, v = m.group("t"), m.group("v")
    a = re.search(rf"\n[ \t]*{v} = (?:\({t}\) )?(?P<arg>arg\d+);\n", s)
    if not a:
        break
    arg = a.group("arg")
    if len(re.findall(rf"\b{arg}\b", s)) != 2:          # declaration + this copy only
        break
    s = s.replace(a.group(0), "\n", 1).replace(m.group(0), "\n", 1)
    s = re.sub(rf"\b(?:[su](?:8|16|32)|M2C_UNK|void \*|\?) {arg}\b(?=[,)])", f"{t} {arg}", s, count=1)
    s = re.sub(rf"\b{v}\b", arg, s)
open(sys.argv[2], "w", newline="\n").write(s)
