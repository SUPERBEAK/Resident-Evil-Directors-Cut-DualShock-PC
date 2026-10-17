"""preinc.py <in.c> <out.c>: m2c writes a pre-increment used as a value as an assignment
expression `(F(b, s8 *, o) = F(b, u8 *, o) + 1)`; Capcom's `++F(b, s8 *, o)` loads the field
through a second address computation and only that form matches (stage7 func_8011A31C,
stage6 func_8011F0C8: `if (++p->x1B == 0x5A)`). `- 1` -> `--`. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
F = r"M2C_FIELD\((?P<b>(?:[^()]|\([^()]*\))+?), (?P<t>[su](?:8|16|32)) \*, (?P<o>0x[0-9A-F]+|\d+)\)"
pat = re.compile(r"\(" + F + r" = M2C_FIELD\((?P=b), [su](?:8|16|32) \*, (?P=o)\) (?P<op>[+-]) 1\)")
s = pat.sub(lambda m: f"{'++' if m.group('op') == '+' else '--'}M2C_FIELD({m.group('b')}, {m.group('t')} *, {m.group('o')})", s)
open(sys.argv[2], "w", newline="\n").write(s)
