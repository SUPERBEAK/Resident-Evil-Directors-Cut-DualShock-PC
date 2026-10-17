"""foldarms.py <in.c> <out.c>: GCC's fold turns `X = (c ? K1 : K2) - A;` into
`c ? K1 - A : K2 - A` with A in a SAVE_EXPR (kept in s0 across calls). m2c shows
    temp_s0 = A;  if (c) { v = E(K1, temp_s0); } else { v = E(K2, temp_s0); }  X = v;
where both arms are the same expression except for one constant. Rewrite as
`X = E((c) ? K1 : K2, A);` (stage5 func_8011AE34: `D_800C5198 = ((c ? 0x400 : 0xC00) - t) & 0xFFF`).
Also without a temp. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
TOK = re.compile(r"0x[0-9A-F]+|\d+|\w+|\S")


def uses(name):
    return len(re.findall(rf"\b{name}\b", s))


pat = re.compile(r"\n(?P<i>[ \t]*)if \((?P<c>.*)\) \{\n(?P=i)    (?P<v>\w+) = (?P<a>.+);\n(?P=i)\} else \{\n"
                 r"(?P=i)    (?P=v) = (?P<b>.+);\n(?P=i)\}\n(?P=i)(?P<lhs>[^=\n;]+?) = (?P=v);\n")
pos = 0
while True:
    m = pat.search(s, pos)
    if not m:
        break
    pos = m.end()
    v = m.group("v")
    ta, tb = TOK.findall(m.group("a")), TOK.findall(m.group("b"))
    if len(ta) != len(tb) or uses(v) != 4:
        continue
    diff = [k for k in range(len(ta)) if ta[k] != tb[k]]
    if len(diff) != 1 or not re.fullmatch(r"0x[0-9A-F]+|\d+", ta[diff[0]]):
        continue
    k = diff[0]
    expr_toks = ta[:k] + [f"(({m.group('c')}) ? {ta[k]} : {tb[k]})"] + ta[k + 1:]
    expr = ""
    for t in expr_toks:                       # re-join with C-ish spacing
        if expr and (t[0].isalnum() or t[0] in "(_") and (expr[-1].isalnum() or expr[-1] in ")_"):
            expr += " "
        elif t in "+-*/&|<>=?:" or (expr and expr[-1] in "+-*/&|<>=?:,"):
            expr += " "
        expr += t
    expr = re.sub(r"\s+", " ", expr).replace("( ", "(").replace(" )", ")").replace(" ,", ",")
    new = f"\n{m.group('i')}{m.group('lhs')} = {expr};\n"
    s = s[:m.start()] + new + s[m.end():]
    s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{v};(?=\n)", "", s, count=1)
    # inline a saved temp used only in this expression
    for t in set(re.findall(r"\btemp_s\d\w*\b", expr)):
        a = None
        for am in re.finditer(rf"\n[ \t]*{t} = ([^;\n]+);(?=\n)", s):
            a = am
        if a and uses(t) == 3:
            s = s[:a.start()] + s[a.end():]
            s = re.sub(rf"\b{t}\b(?!;)", lambda _: a.group(1), s, count=1)
            s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{t};(?=\n)", "", s, count=1)
    pos = 0
open(sys.argv[2], "w", newline="\n").write(s)
