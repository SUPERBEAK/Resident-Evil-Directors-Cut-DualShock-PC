"""retinto.py <in.c> <out.c>: a function ending `if (c) { ... } return; }` where the original's
if-body ends with its own `return` (a `j` to the epilogue that the false branch skips): move the
trailing `return;` into the last (innermost) if-block (stage6 func_8011EC7C, stage3 func_80117B50).
Works at any depth and through several closing braces, e.g. the last case of a switch (stage3
func_8010EB30) or nested ifs (stage6 func_8011BAC0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
m = re.search(r"\n(?P<b>[ \t]*[^\n]*;)\n(?P<braces>(?:[ \t]*\}\n)+)(?P<i>[ \t]*)return;\n(?P<rest>(?:[ \t]*\}\n)*\}\s*)$", s)
if m:
    braces = m.group("braces").splitlines(True)
    inner_ind = braces[0][:len(braces[0]) - len(braces[0].lstrip())]
    s = (s[:m.start()] + f"\n{m.group('b')}\n{inner_ind}    return;\n" + "".join(braces) + m.group("rest"))
open(sys.argv[2], "w", newline="\n").write(s)
