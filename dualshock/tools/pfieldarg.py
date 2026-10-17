"""pfieldarg.py <in.c> <out.c> [--all]: m2c's address-first `M2C_PFIELD(p, T, o)` passed as a call
argument (`f(M2C_PFIELD(D_800CAB14, s16 *, 0xC2), ...)`) is a plain field read in Capcom's code
(`lh a0, 0xC2(v1)` without the separate address add) -> `M2C_FIELD`. --all: every PFIELD that is
read (not assigned). Stage7 func_801198C4, stage6 func_8011E670. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
P = r"M2C_PFIELD(\((?:[^()]|\([^()]*\))*\))"
if "--all" in sys.argv[3:]:
    s = re.sub(P + r"(?! =[^=])", r"M2C_FIELD\1", s)
else:
    s = re.sub(r"(?<=[(,] )" + P + r"|(?<=\)\()" + P, lambda m: "M2C_FIELD" + (m.group(1) or m.group(2)), s)
    s = re.sub(r"(?<=\(\(s32\) )" + P, r"M2C_FIELD\1", s)
open(sys.argv[2], "w", newline="\n").write(s)
