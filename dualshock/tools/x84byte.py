"""x84byte.py <in.c> <out.c>: the entity's routine bytes 0x84..0x87 are written either all at once
(`*(s32 *)&p->x84 = 0x20001`) or one byte at a time. m2c types a small constant store to 0x84 as a
word; when the value fits in a byte, Capcom stored only routine0 (`sb`): `M2C_FIELD(p, u8 *, 0x84) = 8;`
(stage4 func_80120DF0, func_80119150). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()


def rep(m):
    v = int(m.group(3), 0)
    if v > 0xFF or v < 2:               # 0/1 are word stores (routine = 0x00000001 / 0) in the files seen
        return m.group(0)
    return f"M2C_{m.group(1)}FIELD({m.group(2)}, u8 *, 0x84) = {m.group(3)};"   # keep address-first


s = re.sub(r"M2C_(P?)FIELD\(((?:[^(),]|\([^()]*\))+?), [su]32 \*, 0x84\) = (0x[0-9A-F]+|\d+);", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
