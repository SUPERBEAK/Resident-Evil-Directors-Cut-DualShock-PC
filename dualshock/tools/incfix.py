"""incfix.py <in.c> <out.c>: m2c writes a statement `x++;` / `x--;` on a signed field as
`x = (s16) ((u16) x + 1);`. Capcom's `x++` on an address-first field (M2C_PFIELD) computes the
address once and matches only in the ++ form (stage6 func_80113F14). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"^(\s*)(.+) = \((?:s16|s8|s32)\) \(\((?:u16|u8|u32)\) \2 ([+-]) 1\);$",
           lambda m: f"{m.group(1)}{m.group(2)}{m.group(3) * 2};", s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
