"""dupargfix.py <in.c> <out.c> [--drop]: when m2c can't tell how many arguments a call through a
function pointer takes, it sometimes passes one temp twice: `temp = EXPR; ... f(x, (void *) temp, temp)`.
The original passed EXPR once (stage7 func_80115CB4: the value was loaded into a2 and moved to a1).
Drop the duplicate, inline EXPR and delete the assignment. The temp's declaration is kept (now
unused) because the original frame often still had room for it; with --drop it is removed too
(stage4 func_801110A0 had no room). For trybatch: run once plain and once with --drop."""
import re
import sys

s = open(sys.argv[1]).read()
for m in list(re.finditer(r"\((?:void \*|s32|u32)\) (\w+), \1\)", s)):
    t = m.group(1)
    a = re.search(rf"\n[ \t]*{t} = ([^;\n]+);\n", s)
    if not a or len(re.findall(rf"\b{t}\b", s)) != 4:  # declaration, assignment, two uses
        continue
    s = s.replace(a.group(0), "\n", 1)
    s = re.sub(rf"\((?:void \*|s32|u32)\) {t}, {t}\)", lambda _: a.group(1) + ")", s, count=1)
    if "--drop" in sys.argv[3:]:
        s = re.sub(rf"\n[ \t]*[\w ]+\b{t};\n", "\n", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
