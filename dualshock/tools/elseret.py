"""elseret.py <in.c> <out.c>: inside a switch case, m2c writes `if (c) { ... } else { return; } break;`
where Capcom had `if (c) { ... } break;` (the false branch jumps straight past the switch, which
m2c reads as a return) - stage5 func_80117128. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"\n([ \t]*)\} else \{\n[ \t]*return;\n\1\}\n(\1break;\n)", r"\n\1}\n\2", s)
open(sys.argv[2], "w", newline="\n").write(s)
