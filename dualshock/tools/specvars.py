"""specvars.py - follow local variable / parameter renames in the struct specs.

Run from dualshock/ with the project venv python:
    python tools/specvars.py ../temp/locals_log_B1.txt [more logs]

The local-pointer specs (config/locals/*.txt, also config/structs/*.txt) name variables per function:
`param <function> <variable or expression>`. After locals were renamed (log lines `<file> <function> <old>
<new>`), those lines must use the new names, or a later `apply` of the spec would not find them. Each `param`
line of a renamed function gets every old name replaced (whole words, also inside expressions).
"""
import glob
import os
import re
import sys
from collections import defaultdict


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    ren = defaultdict(list)                  # function -> [(old, new)]
    for log in sys.argv[1:]:
        for line in open(log):
            cols = line.split()
            if len(cols) == 4:
                ren[cols[1]].append((cols[2], cols[3]))
    changed = 0
    for p in glob.glob(os.path.join("config", "locals", "*.txt")) + glob.glob(os.path.join("config", "structs", "*.txt")):
        lines = open(p).read().split("\n")
        hit = False
        for i, l in enumerate(lines):
            m = re.match(r"^(param\s+)(\w+)(\s+)(.*)$", l)
            if not m or m.group(2) not in ren:
                continue
            expr = m.group(4)
            # simultaneous replacement (old names of one function may swap: a -> b, b -> c)
            mapping = dict(ren[m.group(2)])
            new = re.sub(r"\b\w+\b", lambda w: mapping.get(w.group(0), w.group(0)), expr) if mapping else expr
            if new != expr:
                lines[i] = m.group(1) + m.group(2) + m.group(3) + new
                hit = True
                changed += 1
        if hit:
            open(p, "w", newline="\n").write("\n".join(lines))
    print(f"{changed} param lines updated")


if __name__ == "__main__":
    main()
