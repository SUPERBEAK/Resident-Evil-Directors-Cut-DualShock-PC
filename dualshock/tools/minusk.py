"""minusk.py <in.c> <out.c>: m2c prints `A = (T) ((A - K) - B);` for code that computes A - K first
and then subtracts B; written that way GCC re-associates it into `A - (B + K)`. Capcom's form is
`A += -K - B;` (stage4 func_80125794: `x16C.n += -0x18 - x78`). Also `(A + K) - B` -> `A += K - B`.
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
F = r"(?:M2C_\w*FIELD\((?:[^()]|\([^()]*\))*\)|[\w.\->\[\]]+)"
pat = re.compile(r"^(?P<i>[ \t]*)(?P<a>" + F + r") = (?:\([\w ]+\) )?\(\((?P<a2>" + F + r") (?P<op>[-+]) (?P<k>0x[0-9A-Fa-f]+|\d+)\) - (?P<b>[^;]+)\);$", re.M)


def norm(t):
    return re.sub(r"\bM2C_[SP]?FIELD\(", "M2C_FIELD(", re.sub(r"\b[su](?:8|16|32) \*", "T *", t))


def rep(m):
    if norm(m.group("a")) != norm(m.group("a2")):
        return m.group(0)
    k = ("-" if m.group("op") == "-" else "") + m.group("k")
    return f"{m.group('i')}{m.group('a')} += {k} - {m.group('b')};"


s = pat.sub(rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
