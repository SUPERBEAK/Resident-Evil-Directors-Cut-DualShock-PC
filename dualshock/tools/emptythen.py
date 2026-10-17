"""emptythen.py <in.c> <out.c>: m2c writes `if (c) { /* empty */ } else { body }`. Rewrite as
`if (!c) { body }`, negating each comparison of a top-level `&&` chain (De Morgan) so the
result reads `if ((a == x) || (b == y))` like Capcom's source (stage3 func_8012B1EC).
For trybatch."""
import re
import sys

NEG = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}


def split_top(text, sep):
    parts, depth, last, i = [], 0, 0, 0
    while i < len(text):
        c = text[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif depth == 0 and text.startswith(sep, i):
            parts.append(text[last:i])
            i += len(sep)
            last = i
            continue
        i += 1
    parts.append(text[last:])
    return parts


def strip_parens(t):
    t = t.strip()
    while t.startswith("(") and t.endswith(")") and len(split_top(t[1:-1], ")")) == 1:
        inner = t[1:-1]
        depth = 0
        ok = True
        for c in inner:
            depth += {"(": 1, ")": -1}.get(c, 0)
            if depth < 0:
                ok = False
                break
        if not ok:
            break
        t = inner.strip()
    return t


def negate(term):
    t = strip_parens(term)
    if len(split_top(t, " && ")) > 1 or len(split_top(t, " || ")) > 1:
        return f"!({t})"
    for op in ("==", "!=", ">=", "<=", "<", ">"):
        parts = split_top(t, f" {op} ")
        if len(parts) == 2:
            return f"({parts[0]} {NEG[op]} {parts[1]})"
    if t.startswith("!"):
        return t[1:]
    return f"!{t}" if re.fullmatch(r"[\w>-]+", t) else f"!({t})"


def neg_cond(cond):
    ands = split_top(strip_parens(cond), " && ")
    if len(ands) > 1:
        return " || ".join(negate(a) for a in ands)
    return negate(cond)


s = open(sys.argv[1]).read()
pat = re.compile(r"\n([ \t]*)if \((.*)\) \{\n[ \t]*\n\1\} else \{\n")
while True:
    m = pat.search(s)
    if not m:
        break
    s = s[:m.start()] + f"\n{m.group(1)}if ({neg_cond(m.group(2))}) {{\n" + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
