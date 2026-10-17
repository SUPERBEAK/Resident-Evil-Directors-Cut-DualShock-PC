"""constmove.py <in.c> <out.c> [--mul]: GCC's fold moves a constant term inward:
Capcom's `A + K + B` compiles as `A + (B + K)` and `A + K - B` as `A - (B - K)`, which is how m2c
prints them; writing m2c's form back gives `(A + K) + B` (the constant pulled out again).
Rewrite m2c's `A + (B + K)` -> `A + K + B` and `A - (B - K)` -> `A + K - B` (stage4
func_80116054, stage5 func_80120488/func_80120840, func_80117818). With --mul, a `B << n`
next to the constant becomes `B * 2^n` (func_80117818). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()


def close(text, i):
    depth = 0
    for k in range(i, len(text)):
        depth += {"(": 1, ")": -1}.get(text[k], 0)
        if depth == 0:
            return k + 1
    return -1


def split_top(t, op):
    """Split `t` at its last top-level ` op ` (outside parentheses)."""
    depth, pos = 0, -1
    for k, c in enumerate(t):
        depth += {"(": 1, ")": -1}.get(c, 0)
        if depth == 0 and t.startswith(f" {op} ", k):
            pos = k
    return (t[:pos], t[pos + 3:]) if pos >= 0 else None


out, i = [], 0
pat = re.compile(r" ([+-]) \(")
while True:
    m = pat.search(s, i)
    if not m:
        out.append(s[i:])
        break
    j = close(s, m.end() - 1)
    inner = s[m.end():j - 1] if j > 0 else ""
    parts = split_top(inner, "+" if m.group(1) == "+" else "-")
    if j > 0 and parts and re.fullmatch(r"0x[0-9A-F]+|\d+", parts[1].strip()):
        b, k = parts[0].strip(), parts[1].strip()
        if "--mul" in sys.argv[3:]:
            b = re.sub(r"^\((.*)\) << (\d+)$", lambda mm: f"({mm.group(1)}) * {1 << int(mm.group(2))}", b)
            b = re.sub(r"^\(\((.*)\) << (\d+)\)$", lambda mm: f"(({mm.group(1)}) * {1 << int(mm.group(2))})", b)
        out.append(s[i:m.start()] + f" + {k} {m.group(1)} {b}")
        i = j
    else:
        out.append(s[i:m.end()])
        i = m.end()
s = "".join(out)
open(sys.argv[2], "w", newline="\n").write(s)
