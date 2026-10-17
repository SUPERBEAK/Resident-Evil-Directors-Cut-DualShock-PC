"""predectest.py <in.c> <out.c>: Capcom's `if (--p->x5 == 0)` on a byte. m2c writes the decrement as a
statement and then tests a stale/unrelated variable:
    M2C_FIELD(B, s8 *, 5) = M2C_FIELD(B, u8 *, 5) + 0xFF;
    if (!(temp & 0xFF)) {
Rewrite as `if (--M2C_FIELD(B, u8 *, 5) == 0) {` (stage4 func_80128A48, func_8012B418).
Also `- 1` and the `if (temp & 0xFF)` (!= 0) form. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
F = r"M2C_P?FIELD\((?P<b>(?:[^(),]|\([^()]*\))+?), [su]8 \*, (?P<o>0x[0-9A-F]+|\d+)\)"
pat = re.compile(r"\n(?P<i>[ \t]*)" + F + r" = M2C_P?FIELD\((?P=b), [su]8 \*, (?P=o)\) (?:\+ 0xFF|- 1);\n"
                 r"(?P=i)if \((?P<neg>!?)\((?P<t>\w+) & 0xFF\)\) \{")


def rep(m):
    op = "==" if m.group("neg") else "!="
    return f"\n{m.group('i')}if (--M2C_FIELD({m.group('b')}, u8 *, {m.group('o')}) {op} 0) {{"


s = pat.sub(rep, s)
for t in set(re.findall(r"\n[ \t]*u8 (temp_\w+);", s)):
    if len(re.findall(rf"\b{t}\b", s)) == 1:
        s = re.sub(rf"\n[ \t]*u8 {t};", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
