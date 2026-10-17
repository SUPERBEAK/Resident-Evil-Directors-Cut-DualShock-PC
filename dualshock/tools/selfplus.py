"""selfplus.py <in.c> <out.c> [--minus]: `X = X + rest;` -> `X += rest;` (with --minus also
`X = X - rest;` -> `X -= rest;`). At -O0 the two forms evaluate in a different order: `X += a + b`
computes `a + b` first (stage5 func_8011620C `D_800CAB14->pos_y += 0x64 + ...`,
func_80116C20). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
ops = "[+-]" if "--minus" in sys.argv[3:] else r"\+"
plain = re.compile(r"^([ \t]*)([^=\n;]+?) = \2 (" + ops + r") (.+);$", re.M)
wrapped = re.compile(r"^([ \t]*)([^=\n;]+?) = \([\w ]+\) \(\2 (" + ops + r") (.+)\);$", re.M)


def rep(m):
    rest = m.group(4)
    if rest.count("(") != rest.count(")"):
        return m.group(0)
    return f"{m.group(1)}{m.group(2)} {m.group(3)}= {rest};"


s = wrapped.sub(rep, s)
s = plain.sub(rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
