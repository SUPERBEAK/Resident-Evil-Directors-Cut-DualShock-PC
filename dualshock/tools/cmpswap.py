"""cmpswap.py <in.c> <out.c> [--all]: m2c writes `if ((u32) (K - (call() & 1) * N) < (u32) p->x38)`
with the call on the left, but Capcom wrote `p->x38 > (...)`: GCC evaluates the left operand
first, so the original loads the base pointer BEFORE the call and keeps it in an s-register
(`lui s0,%hi(D_800CAB14)` ahead of the `jalr`; stage4 func_80116CF4, func_80117650).
Swap the operands of a top-level comparison in an `if`/`while` condition (or one `&&`/`||` term)
when only the left side contains a call, mirroring the operator.
Without --all only the first candidate is changed; with --all every one. For trybatch."""
import re
import sys

MIRROR = {"<": ">", ">": "<", "<=": ">=", ">=": "<=", "==": "==", "!=": "!="}


def close_paren(s, i):
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    return -1


def top_split(expr, ops):
    """Split expr at depth-0 occurrences of any operator in ops -> list of (text, op)."""
    parts, depth, start, i = [], 0, 0, 0
    while i < len(expr):
        c = expr[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif depth == 0:
            for op in ops:
                if expr.startswith(f" {op} ", i):
                    parts.append((expr[start:i], op))
                    i += len(op) + 2
                    start = i
                    break
            else:
                i += 1
                continue
            continue
        i += 1
    parts.append((expr[start:], None))
    return parts


def has_call(t):
    return "(D_801FEC18 = " in t or re.search(r"\bfunc_[0-9A-F]{8}\(", t) is not None


def swap_term(term):
    """'(A op B)' or 'A op B' -> swapped text, or None."""
    t = term.strip()
    wrapped = t.startswith("(") and close_paren(t, 0) == len(t) - 1
    inner = t[1:-1] if wrapped else t
    parts = top_split(inner, ["<=", ">=", "==", "!=", "<", ">"])
    if len(parts) != 2:
        return None
    (a, op), (b, _) = parts
    if not has_call(a) or has_call(b) or "=" in re.sub(r"[<>!=]=|==", "", b):
        return None
    if re.fullmatch(r"\(?-?(?:0x[0-9A-Fa-f]+|\d+)U?\)?|NULL", b.strip()):
        return None                          # `call() == 0` stays as it is
    new = f"{b.strip()} {MIRROR[op]} {a.strip()}"
    lead = term[:len(term) - len(term.lstrip())]
    return lead + (f"({new})" if wrapped else new)


s = open(sys.argv[1]).read()
do_all = "--all" in sys.argv[3:]
pos = 0
done = False
while not done:
    m = re.compile(r"\b(?:if|while) \(").search(s, pos)
    if not m:
        break
    o = m.end() - 1
    c = close_paren(s, o)
    if c < 0:
        break
    cond = s[o + 1:c]
    terms = top_split(cond, ["&&", "||"])
    out, changed = [], False
    for text, op in terms:
        new = None if (changed and not do_all) else swap_term(text)
        if new is not None:
            changed = True
        out.append((new if new is not None else text) + (f" {op} " if op else ""))
    if changed:
        s = s[:o + 1] + "".join(out) + s[c:]
        if not do_all:
            done = True
    pos = o + 1
open(sys.argv[2], "w", newline="\n").write(s)
