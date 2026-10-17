"""pfieldrmw.py <in.c> <out.c> [--all]: a read-modify-write of a field through a pointer variable,
`M2C_FIELD(p, u8 *, 0x2E8) = M2C_FIELD(p, u8 *, 0x2E8) & 0xFE;`, where the original computes the
field address first (`addiu v1,v0,0x2E8; move v0,v1; ...`) like its neighbours that m2c already
wrote as M2C_PFIELD: switch both sides to M2C_PFIELD (stage4 func_8012A4EC).
Without --all only the first such statement is changed (trybatch keeps the better result); with
--all every one. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
B = r"(?P<b>(?:[^(),]|\([^()]*\))+?)"
T = r"(?P<t>[su](?:8|16|32) \*)"
# Either side may already be M2C_PFIELD (`M2C_PFIELD(p, u8 *, 0x5D0) = (u8) (M2C_FIELD(p, u8 *, 0x5D0) | 2)`,
# stage4 func_80120550), but not both.
pat = re.compile(r"(?!M2C_PFIELD\([^;]*= (?:\([\w ]+\) )?\(?M2C_PFIELD)"
                 r"M2C_P?FIELD\(" + B + r", " + T + r", (?P<o>0x[0-9A-F]+|\d+)\) = (?P<c>(?:\([\w ]+\) )?\(?)"
                 r"M2C_P?FIELD\((?P=b), [su](?:8|16|32) \*, (?P=o)\)")
count = 0 if "--all" in sys.argv[3:] else 1
s = pat.sub(lambda m: f"M2C_PFIELD({m.group('b')}, {m.group('t')}, {m.group('o')}) = {m.group('c')}"
            f"M2C_PFIELD({m.group('b')}, {m.group('t')}, {m.group('o')})", s, count=count)
open(sys.argv[2], "w", newline="\n").write(s)
