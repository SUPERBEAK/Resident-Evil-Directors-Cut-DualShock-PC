"""elsecase.py <in.c> <out.c>: m2c turns `case 1: ...; if (c) break; case 2: ...` (a case that
falls through into the next one unless a condition holds) into an empty then-branch with the next
case label inside the else: `if (c) {} else { case 2: ... }`. Rewrite it back to the break form;
the else's closing brace now closes a plain block after the label (stage7 func_8011A70C,
stage6 func_8011F4B8). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()


def rep(m):
    ind = m.group(1)
    case_ind = ind[:-4] if len(ind) >= 4 else ind
    return (f"\n{ind}if ({m.group(2)}) {{\n{ind}    break;\n{ind}}}\n"
            f"{case_ind}case {m.group(3)}:\n{ind}{{\n")


s = re.sub(r"\n([ \t]*)if \((.*)\) \{\n[ \t]*\n[ \t]*\} else \{\n[ \t]*case ([^:\n]+):\n", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
