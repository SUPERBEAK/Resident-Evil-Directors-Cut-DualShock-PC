"""sub16c.py <in.c> <out.c> [--pfield|--field|--both] [--base 0x16C] [--size 0x40]:
the entity work struct has a sub-struct at +0x16C that Capcom reaches through its own address
(`addiu v0,v1,0x16C; move v1,v0; sh v0,6(v1)`), which m2c sometimes writes as a flat field
(`M2C_PFIELD(D_800CAB14, s16 *, 0x172)` or `M2C_FIELD(D_800CAB14, s8 *, 0x175)`).
Rewrite such fields to `M2C_FIELD((D_800CAB14 + 0x16C), T, off - 0x16C)` (stage4 func_80120550).
--pfield (default) only touches M2C_PFIELD, --field only M2C_FIELD, --both both. For trybatch."""
import re
import sys

args = sys.argv[3:]
mode = "both" if "--both" in args else "field" if "--field" in args else "pfield"
base = int(args[args.index("--base") + 1], 16) if "--base" in args else 0x16C
size = int(args[args.index("--size") + 1], 16) if "--size" in args else 0x40
macros = {"pfield": "M2C_PFIELD", "field": "M2C_FIELD", "both": "M2C_P?FIELD"}[mode]

s = open(sys.argv[1]).read()


def rep(m):
    off = int(m.group("o"), 0)
    if not base <= off < base + size:
        return m.group(0)
    return f"M2C_FIELD(((u8 *)D_800CAB14 + 0x{base:X}), {m.group('t')}, 0x{off - base:X})"


s = re.sub(r"(?:" + macros + r")\(D_800CAB14, (?P<t>[\w ]+\*), (?P<o>0x[0-9A-Fa-f]+|\d+)\)", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
