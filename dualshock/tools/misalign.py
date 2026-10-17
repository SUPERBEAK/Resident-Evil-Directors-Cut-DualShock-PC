"""misalign.py <in.c> <out.c> [--s16]: m2c sometimes types a field at an offset that is not
4-aligned as s32 (`M2C_FIELD(p, s32 *, 0xBE)`) - impossible on the R3000. The original reads a
byte there (`lbu`); rewrite as u8 (--s16: s16 when the offset is 2-aligned) - stage7
func_801198C4, stage6 func_8011E670. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
half = "--s16" in sys.argv[3:]


def rep(m):
    off = int(m.group(3), 0)
    if off % 4 == 0:
        return m.group(0)
    t = "s16" if half and off % 2 == 0 else "u8"
    return f"{m.group(1)}({m.group(2)}, {t} *, {m.group(3)})"


s = re.sub(r"(M2C_P?FIELD)\(((?:[^(),]|\([^()]*\))+?), [su]32 \*, (0x[0-9A-F]+|\d+)\)", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
