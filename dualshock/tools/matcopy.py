"""matcopy.py <in.c> <out.c> [--vector]: m2c splits a MATRIX (or VECTOR) struct copy into eight (four)
s32 field copies, often through base temps:
    temp_v0 = D_80132284 + 0x7C;
    temp_v0_2 = temp_v0 + 0x44;
    M2C_FIELD(temp_v0, s32 *, 0x44) = (s32) M2C_FIELD((void *)0x1F8003E0, s32 *, 0);
    M2C_FIELD(temp_v0_2, s32 *, 4) = (s32) M2C_FIELD((void *)0x1F8003E0, s32 *, 4);  ... 0x1C
Rewrite as one copy written as a field of the first base, so the address is built the same way
(`addiu v0,v1,0x7C; move; addiu 0x44` then the lw/sw block):
    M2C_FIELD((D_80132284 + 0x7C), MATRIX *, 0x44) = *(MATRIX *)0x1F8003E0;
(stage3 func_80120E14). Without --vector, runs of 8 become MATRIX; with it, runs of 4 VECTOR.
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
n, typ = (4, "VECTOR") if "--vector" in sys.argv[3:] else (8, "MATRIX")
B = r"(?:[^(),]|\((?:[^()]|\([^()]*\))*\))+?"
LINE = re.compile(r"^(?P<i>[ \t]*)M2C_\w*FIELD\((?P<d>" + B + r"), s32 \*, (?P<o>0x[0-9A-Fa-f]+|\d+)\) = (?:\(s32\) )?"
                  r"\(?M2C_\w*FIELD\((?P<s>" + B + r"), s32 \*, (?P<so>0x[0-9A-Fa-f]+|\d+)\)\)?;$")
lines = s.split("\n")


def temp_def(name, upto):
    for j in range(upto - 1, -1, -1):
        m = re.fullmatch(rf"[ \t]*{re.escape(name)} = (.+);", lines[j])
        if m:
            return j, m.group(1)
    return None, None


i = 0
while i + n <= len(lines):
    ms = [LINE.match(lines[i + k]) for k in range(n)]
    if all(ms) and all(int(ms[k].group("so"), 0) - int(ms[0].group("so"), 0) == 4 * k for k in range(n)) \
            and len({m.group("s") for m in ms}) == 1:
        m0 = ms[0]
        base, off = m0.group("d"), m0.group("o")
        drop = []
        if base.startswith("temp_"):
            j, e = temp_def(base, i)
            if j is not None:
                drop.append(j)
                base = f"({e})"
        for m in ms[1:]:                      # second base temp (temp_v0_2 = temp_v0 + 0x44)
            if m.group("d").startswith("temp_") and m.group("d") != m0.group("d"):
                j, e = temp_def(m.group("d"), i)
                if j is not None and j not in drop:
                    drop.append(j)
        src, so = m0.group("s"), m0.group("so")
        rhs = f"*({typ} *){src.replace('(void *)', '')}" if int(so, 0) == 0 and re.fullmatch(r"\(void \*\)0x[0-9A-Fa-f]+", src) \
            else f"M2C_FIELD({src}, {typ} *, {so})"
        new = f"{m0.group('i')}M2C_FIELD({base}, {typ} *, {off}) = {rhs};"
        lines[i:i + n] = [new]
        for j in sorted(drop, reverse=True):
            del lines[j]
            i -= 1
    i += 1
s = "\n".join(lines)
for t in set(re.findall(r"\b(temp_\w+)\b", s)):
    if len(re.findall(rf"\b{t}\b", s)) == 1:
        s = re.sub(rf"\n[ \t]*[\w ]+\*? ?{t};", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
