"""tempdec.py <in.c> <out.c>: m2c splits a post-decrement/increment that is used inside the
following condition into temps:
    temp_p = BASE;                                  (optional pointer temp)
    temp_v = M2C_FIELD(temp_p, T, OFF);
    M2C_FIELD(temp_p, T, OFF) = (temp_v + 0xFF);    (or `- 1`, `+ 1`, `+ 0xFFFF`)
    if (... temp_v ...) {
Fold it back to `if (... M2C_FIELD(BASE, T, OFF)-- ...)` (stage7 func_801214C0:
`if (!(p->x18-- & 7))`). A trailing `& 0xFF` that m2c adds on a u8 test is dropped. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
DEC = {"+ 0xFF": "--", "+ 0xFFFF": "--", "- 1": "--", "+ 1": "++", "+ -1": "--"}


def count(name):
    return len(re.findall(rf"\b{name}\b", s))


pat = re.compile(
    r"\n(?P<i>[ \t]*)(?:(?P<p>temp_\w+) = (?P<base>[^;\n]+);\n[ \t]*)?"
    r"(?P<v>temp_\w+) = M2C_FIELD\((?P<b2>[^,\n]+), (?P<t>[\w ]+\*), (?P<off>0x[0-9A-F]+|\d+)\);\n"
    r"[ \t]*M2C_FIELD\((?P=b2), (?P=t), (?P=off)\) = \(?(?:\([\w ]+\) )?\(?(?P=v) (?P<op>[+-] (?:0xFFFF|0xFF|-?1))\)?\)?;\n"
    r"(?P<if>[ \t]*if \((?P<cond>.*)\) \{)\n")
changed = True
while changed:
    changed = False
    for m in pat.finditer(s):
        v, p, op = m.group("v"), m.group("p"), m.group("op")
        if op not in DEC or count(v) != 4 or (p and (m.group("b2") != p or count(p) != 4)):
            continue
        base = m.group("base") if p else m.group("b2")
        if p and not base.startswith("("):
            base = f"({base})"
        field = f"M2C_FIELD({base}, {m.group('t')}, {m.group('off')}){DEC[op]}"
        cond = re.sub(rf"\b{v}\b", lambda _: field, m.group("cond"), count=1)
        cond = cond.replace(f"{DEC[op]} & 7 & 0xFF", f"{DEC[op]} & 7")
        cond = re.sub(rf"({re.escape(DEC[op])} & (?:0x[0-9A-F]+|\d+)) & 0xFF\b", r"\1", cond)
        new = "\n" + m.group("if").replace(m.group("cond"), cond, 1) + "\n"
        s = s[:m.start()] + new + s[m.end():]
        for t in (v, p):
            if t and count(t) == 1:
                s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{t};(?=\n)", "", s, count=1)
        changed = True
        break
open(sys.argv[2], "w", newline="\n").write(s)
