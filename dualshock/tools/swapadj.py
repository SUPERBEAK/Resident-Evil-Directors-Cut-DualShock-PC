"""swapadj.py <in.c> <out.c> <k>: swap the k-th pair of adjacent simple statements (two
consecutive lines at the same indentation that are plain `...;` statements - no declarations,
no control flow, no labels) of the function body.

At -O2 the order of independent statements decides scheduling and register allocation; m2c's
order often differs from Capcom's (main func_8005FD58, func_80050B84, func_8005123C). Run with
trybatch for k = 0, 1, 2, ... (an out-of-range k writes the input unchanged)."""
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[2 + 1])
lines = s.split("\n")
start = next((i for i, l in enumerate(lines) if re.match(r"^\w.*\)\s*\{\s*$", l)), None)
CTRL = re.compile(r"^\s*(if|else|for|while|do|switch|case|default|return|goto|break|continue|\}|\{)\b|^\s*\}|:\s*$")
DECL = re.compile(r"^\s*(?:extern\s+|register\s+|static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?"
                  r"(?:[us](?:8|16|32)|int|char|short|long|void|f32|M2C_UNK\w*|[A-Z]\w*)\b[\w\s\*]*"
                  r"(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*$")


def simple(l):
    return l.strip().endswith(";") and not CTRL.match(l) and not DECL.match(l)


pairs = []
if start is not None:
    for i in range(start + 1, len(lines) - 1):
        a, b = lines[i], lines[i + 1]
        ia = len(a) - len(a.lstrip())
        ib = len(b) - len(b.lstrip())
        if ia == ib and ia > 0 and simple(a) and simple(b) and not CTRL.match(a) and not CTRL.match(b) \
                and not DECL.match(a) and not DECL.match(b):
            pairs.append(i)
if k < len(pairs):
    i = pairs[k]
    lines[i], lines[i + 1] = lines[i + 1], lines[i]
open(sys.argv[2], "w", newline="\n").write("\n".join(lines))
