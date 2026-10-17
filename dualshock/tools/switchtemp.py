"""switchtemp.py <in.c> <out.c>: m2c sometimes loads the switch value into a temp first:
`temp = EXPR; switch (temp) {`. At -O0 the temp costs a stack store/reload the original doesn't
have; switch on EXPR directly (stage3 func_8012B1EC). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for m in list(re.finditer(r"\n([ \t]*)(temp_\w+|var_\w+) = ([^;\n]+);\n\1switch \(\2\) \{", s)):
    t = m.group(2)
    if len(re.findall(rf"\b{t}\b", s)) != 3:          # declaration, assignment, switch
        continue
    s = s.replace(m.group(0), f"\n{m.group(1)}switch ({m.group(3)}) {{", 1)
    s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{t};(?=\n)", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
