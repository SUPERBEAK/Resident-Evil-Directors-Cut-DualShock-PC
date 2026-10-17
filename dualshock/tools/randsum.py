"""randsum.py <in.c> <out.c>: three random numbers plus a constant. m2c writes
`((R & 7) + ((R & 7) + K) + (R & 7))` (R = the random call); Capcom's order of the calls follows
`K + (R & 7) + (R & 7) + (R & 7)` (stage3 func_8011D874, stage5 func_80110A94,
stage4 func_801159E0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
R = r"\(+(?P<c{n}>\(D_801FEC18 = D_801FE\w+\)\(\))\)* & (?P<m{n}>0x[0-9A-F]+|\d+)\)"
pat = re.compile(r"\(" + R.format(n=1) + r" \+ \(" + R.format(n=2) + r" \+ (?P<k>0x[0-9A-F]+|\d+)\) \+ " + R.format(n=3))


def rep(m):
    parts = [f"({m.group('c%d' % i)} & {m.group('m%d' % i)})" for i in (1, 2, 3)]
    out = f"({m.group('k')} + {' + '.join(parts)}"
    bal = lambda t: t.count("(") - t.count(")")
    d = bal(m.group(0)) - bal(out)          # keep the surrounding expression balanced
    return "(" * d + out if d > 0 else out + ")" * -d


s = pat.sub(rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
