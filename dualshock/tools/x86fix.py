"""x86fix.py <in.c> <out.c>: the entity's `routine` bytes at 0x86/0x87 are cleared together with
one halfword store (`sh zero, 0x86`); m2c sometimes types the address-first form as a byte:
`M2C_PFIELD(D_800CAB14, s8 *, 0x86) = 0;` -> `M2C_PFIELD(D_800CAB14, s16 *, 0x86) = 0;`
(stage7 func_8011B890, stage6 func_8012063C). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"M2C_PFIELD\(D_800CAB14, [su]8 \*, 0x86\) = (0|\w+);", r"M2C_PFIELD(D_800CAB14, s16 *, 0x86) = \1;", s)
open(sys.argv[2], "w", newline="\n").write(s)
