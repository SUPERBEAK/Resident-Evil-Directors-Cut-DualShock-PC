"""pfieldadd.py <in.c> <out.c>: m2c writes Capcom's `p->x38 += a + b;` as
`M2C_FIELD(p, u16 *, 0x38) = (s32) (M2C_PFIELD(p, s32 *, 0x38) + a + b);` (wrong store type,
address-first load). Rewrite as `M2C_FIELD(p, s32 *, 0x38) += a + b;` using the load's type
(stage5 func_80116C20). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
B = r"(?P<b>(?:[^(),]|\([^()]*\))+?)"
pat = re.compile(r"M2C_FIELD\(" + B + r", [su](?:8|16|32) \*, (?P<o>0x[0-9A-F]+|\d+)\) = (?:\([\w ]+\) )?\(M2C_PFIELD\((?P=b), (?P<t>[su](?:8|16|32)) \*, (?P=o)\) \+ (?P<rest>.+)\);$", re.M)
s = pat.sub(lambda m: f"M2C_FIELD({m.group('b')}, {m.group('t')} *, {m.group('o')}) += {m.group('rest')};", s)
open(sys.argv[2], "w", newline="\n").write(s)
