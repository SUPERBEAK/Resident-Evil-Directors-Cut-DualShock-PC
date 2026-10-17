"""negmul.py <in.c> <out.c>: Capcom negates fields with `x *= -1;`, which GCC compiles with an extra
`move` before the `negu`; m2c prints `x = (s8) -x;` (stage4 func_8011F8C0). Rewrite the statement
form `X = [(T)] -X;` as `X *= -1;`. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"^([ \t]*)([^;\n=]+?) = (?:\([\w ]+\) )?-\(?(?:\([\w ]+\) )?\2\)?;$", r"\1\2 *= -1;", s, flags=re.M)
# m2c also prints a wrong-width store with a signed load:
# `M2C_FIELD(B, u32 *, 0x7C) = -(s16) M2C_FIELD(B, u16 *, 0x7C);` -> `M2C_FIELD(B, s16 *, 0x7C) *= -1;`
# (stage4 func_80125498, func_8012682C).
s = re.sub(r"^([ \t]*)(M2C_P?FIELD)\((?P<b>(?:[^(),]|\([^()]*\))+?), [su]32 \*, (?P<o>0x[0-9A-Fa-f]+|\d+)\) = "
           r"-\((?P<t>s8|s16)\) M2C_P?FIELD\((?P=b), [su](?:8|16) \*, (?P=o)\);$",
           lambda m: f"{m.group(1)}{m.group(2)}({m.group('b')}, {m.group('t')} *, {m.group('o')}) *= -1;", s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
