"""tempcompound.py <in.c> <out.c>: m2c splits a compound assignment `F op= e;` into
`temp = F; F = (T) (temp op e);` and then sometimes passes the stale temp to a later call
(stage3 func_80126040). Rewrite to `F op= e;`, drop the temp and replace any leftover use of it
in a call argument list by nothing (the arguments were garbage registers). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()


def norm(t):
    return re.sub(r"\bM2C_[SP]?FIELD\(", "M2C_FIELD(", re.sub(r"\b[su](?:8|16|32) \*", "T *", t)).replace(" ", "")


F = r"(?:M2C_\w*FIELD\((?:[^()]|\([^()]*\))*\)|D_[0-9A-F]{8})"
pat = re.compile(r"\n(?P<i>[ \t]*)(?P<t>temp_\w+) = (?:\([\w ]+\) )?(?P<f>" + F + r");\n(?P=i)(?P<f2>" + F +
                 r") = (?:\([\w ]+\) )?\((?P=t) (?P<op>[-+|&^]) (?P<e>.+)\);\n")
while True:
    m = pat.search(s)
    if not m or norm(m.group("f")) != norm(m.group("f2")):
        break
    t = m.group("t")
    s = s.replace(m.group(0), f"\n{m.group('i')}{m.group('f2')} {m.group('op')}= {m.group('e')};\n", 1)
    s = re.sub(rf"\n[ \t]*[\w ]+\*? ?{t};", "", s, count=1)
    # stale uses as call arguments: `f(temp_a0, (s32) temp_a1)` -> `f()`
    s = re.sub(rf"\((?:\([\w ]+\) )?{t}(?:, (?:\([\w ]+\) )?temp_\w+)*\)", "()", s)
    s = re.sub(rf"(?:, )?(?:\([\w ]+\) )?\b{t}\b", "", s)
open(sys.argv[2], "w", newline="\n").write(s)
