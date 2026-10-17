"""argcopy.py <in.c> <out.c>: GCC 2.6.3 keeps a `char`/`short` parameter in its own stack slot;
m2c shows that as a local copy `sp18 = arg2;` and then uses the local. Capcom's source used the
parameter itself - replace the copy by the parameter when the parameter is not used anywhere else
(stage5 func_8012AC4C, func_8012D0B8). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for m in list(re.finditer(r"\n[ \t]*((?:sp|var_|temp_)\w+) = (arg\d+);\n", s)):
    loc, arg = m.group(1), m.group(2)
    if len(re.findall(rf"\b{arg}\b", s)) != 2:         # the parameter list and this copy
        continue
    if len(re.findall(rf"\b{loc}\s*=[^=]", s)) < 1:
        continue
    s = s.replace(m.group(0), "\n", 1)
    s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{loc};(?=\n)", "", s, count=1)
    s = re.sub(rf"\b{loc}\b", arg, s)
open(sys.argv[2], "w", newline="\n").write(s)
