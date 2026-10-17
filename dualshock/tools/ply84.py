"""ply84.py <in.c> <out.c>: rewrite m2c's accesses to the 0x84-byte record table at 0x800C3024
(indexed by the byte at 0x800CF86E, see Ply84 in include/re1_types.h) as struct-member accesses:

    temp = *(u8 *)0x800CF86E * 0x84;            (inlined - Capcom re-reads the index each time)
    *(T *)((u8 *)&(*(s8 *)0x800C30XX) + temp)    \
    *(T *)(0x800C30XX + temp)                     >  M2C_SFIELD(&((Ply84 *)0x800C3024)[IDX], T *, off)
    *(temp + &(*(s8 *)0x800C30XX))               /
    *(temp + &D_800C30XX)   (type from the draft's extern declaration)

The literal base keeps 0x800C3024 in a register for field 0, and struct members (MEM_IN_STRUCT_P)
schedule like the original (main func_8001D5B0, func_8001E85C, func_800208B8). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
BASE = 0x800C3024
IDX = "*(u8 *)0x800CF86E"
IDXE = r"(?:\*\(u8 \*\)0x800CF86E|D_800CF86E)"

# 1. inline the index temporaries
temps = {}
for m in re.finditer(rf"^[ \t]*(\w+) = {IDXE} \* 0x84;[ \t]*\n", s, re.M):
    temps.setdefault(m.group(1), []).append(m)
for t, ms in temps.items():
    if len(ms) != 1 or len(re.findall(rf"\b{t}\s*=[^=]", s)) != 1:
        continue
    s = s.replace(ms[0].group(0), "", 1)
    s = re.sub(rf"\n[ \t]*\w+ {t};", "", s, count=1)
    s = re.sub(rf"\b{t}\b", f"({IDX} * 0x84)", s)
X = rf"\(?\(?{IDXE} \* 0x84\)?\)?"


def field(typ, addr):
    off = int(addr, 16) - BASE
    if not 0 <= off < 0x84:
        return None
    return f"M2C_SFIELD(&((Ply84 *)0x800C3024)[{IDX}], {typ} *, 0x{off:X})"


def sub(pat, fn):
    global s
    s = re.sub(pat, lambda m: fn(m) or m.group(0), s)


sub(rf"\*\((\w+) \*\)\(\(u8 \*\)&\(\*\(s8 \*\)(0x800C30[0-9A-F]{{2}})\) \+ {X}\)", lambda m: field(m.group(1), m.group(2)))
sub(rf"\*\((\w+) \*\)\(\(u8 \*\)(0x800C30[0-9A-F]{{2}}) \+ {X}\)", lambda m: field(m.group(1), m.group(2)))
sub(rf"\*\((\w+) \*\)\((0x800C30[0-9A-F]{{2}}) \+ {X}\)", lambda m: field(m.group(1), m.group(2)))
sub(rf"\*\({X} \+ &\(\*\(s8 \*\)(0x800C30[0-9A-F]{{2}})\)\)", lambda m: field("s8", m.group(1)))
# m2c also writes `X + &(*(s32 *)A)` (and with an outer cast) although X is a BYTE offset: as C
# that scales X by the type size (main func_8001FD00, func_8001C018 - `sll 4` instead of `sll 2`)
sub(rf"\*\((\w+ \**)\*\)\({X} \+ &\(\*\(\w+ \*\)(0x800C30[0-9A-F]{{2}})\)\)", lambda m: field(m.group(1).strip(), m.group(2)))
sub(rf"\*\({X} \+ &\(\*\((\w+) \*\)(0x800C30[0-9A-F]{{2}})\)\)", lambda m: field(m.group(1), m.group(2)))
externs = dict((n, t) for t, n in re.findall(r"^extern (\w+) (D_800C30[0-9A-F]{2});", s, re.M))
sub(rf"\*\({X} \+ &(D_800C30[0-9A-F]{{2}})\)", lambda m: field(externs.get(m.group(1), "s8"), "0x" + m.group(1)[2:]))
open(sys.argv[2], "w", newline="\n").write(s)
