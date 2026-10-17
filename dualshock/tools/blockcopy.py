"""blockcopy.py <in.c> <out.c>: m2c's word-by-word copy of a 16- or 32-byte block between any two
address expressions
    M2C_FIELD(DST, s32 *, 0) = (s32) M2C_FIELD(SRC, s32 *, 0);  ...  0xC  (or ... 0x1C)
is one struct assignment in Capcom's source: `*(VECTOR *)(DST) = *(VECTOR *)(SRC);` or `MATRIX`
for 32 bytes (the original loads four words into t0-t3, stores them, repeats). Generalises
vcopy.py to non-constant addresses (stage7 func_8010F7FC: the camera matrix into the scratch pad).
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
E = r"((?:[^(),]|\((?:[^()]|\([^()]*\))*\))+?)"
first = re.compile(r"\n([ \t]*)M2C_FIELD\(" + E + r", s32 \*, 0\) = \(s32\) M2C_FIELD\(" + E + r", s32 \*, 0\);\n")
pos = 0
while True:
    m = first.search(s, pos)
    if not m:
        break
    ind, dst, src = m.group(1), m.group(2), m.group(3)
    end, n = m.end(), 1
    while n < 8:
        o = rf"(?:0x{n * 4:X}|{n * 4})"          # m2c prints 4 and 8 in decimal, 0xC in hex
        nxt = re.compile(r"[ \t]*M2C_FIELD\(" + re.escape(dst) + rf", s32 \*, {o}\) = \(s32\) M2C_FIELD\("
                         + re.escape(src) + rf", s32 \*, {o}\);\n").match(s, end)
        if not nxt:
            break
        end, n = nxt.end(), n + 1
    if n in (4, 8):
        t = "VECTOR" if n == 4 else "MATRIX"
        new = f"\n{ind}*({t} *)({dst}) = *({t} *)({src});\n"
        s = s[:m.start()] + new + s[end:]
        pos = m.start() + len(new)
    else:
        pos = m.end() - 1
open(sys.argv[2], "w", newline="\n").write(s)
