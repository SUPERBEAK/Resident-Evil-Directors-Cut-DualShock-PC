"""orthen.py <in.c> <out.c>: m2c turns a function-final `if (A || B) { S; } else { body }` into
`if (!A && !B) { body } S;` (the then-branch's jump to the end hides that S belongs to it).
Rewrite a function's last `if` followed by exactly one statement back into the if/else
(stage5 func_80129448, func_8012B3A0). For trybatch."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys_argv = sys.argv
sys.argv = [sys_argv[0], os.devnull, os.devnull]          # emptythen runs on import
import emptythen  # noqa: E402
sys.argv = sys_argv

s = open(sys.argv[1]).read()
m = re.search(r"\n    if \((?P<c>.*)\) \{\n(?P<body>(?:        .*\n|\n)*?)    \}\n    (?P<st>[^\n{}]+;)\n\}\n?$", s)
if m and "return" not in m.group("st"):
    new = (f"\n    if ({emptythen.neg_cond(m.group('c'))}) {{\n        {m.group('st')}\n    }} else {{\n"
           f"{m.group('body')}    }}\n}}\n")
    s = s[:m.start()] + new
open(sys.argv[2], "w", newline="\n").write(s)
