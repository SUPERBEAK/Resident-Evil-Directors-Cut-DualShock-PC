"""subfix.py <in.c> <out.c> [all]: entity fields at offsets >= 0x16C written as direct fields
(`M2C_FIELD(D_800CAB14, T, 0x16E)`) -> the sub-structure form Capcom used
(`M2C_FIELD((D_800CAB14 + 0x16C), T, 2)`). With `all`, M2C_PFIELD accesses too."""
import re, sys

s = open(sys.argv[1]).read()
kinds = "M2C_(?:S?FIELD|PFIELD)" if len(sys.argv) > 3 else "M2C_S?FIELD"


def rep(m):
    off = int(m.group(3), 16)
    if not 0x16C <= off < 0x1A0:
        return m.group(0)
    return f"M2C_FIELD((D_800CAB14 + 0x16C), {m.group(2)}, 0x{off - 0x16C:X})"


s = re.sub(rf"({kinds})\(D_800CAB14, ([\w ]+ \*), 0x([0-9A-F]+)\)", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
