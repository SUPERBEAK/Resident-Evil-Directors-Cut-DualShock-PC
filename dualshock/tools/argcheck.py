"""argcheck.py - find calls whose argument count differs from the callee's definition (MIPS register
pass-through idioms that only work with the original calling convention). Heuristic, prints candidates."""
import os
import re
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else "src"   # run from dualshock/
DEF = re.compile(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^;{]*)\)\s*\{", re.M)
CALL = re.compile(r"\b(\w+)\s*\(")


def count_params(p):
    p = p.strip()
    if p in ("", "void"):
        return 0
    if "..." in p:
        return -1
    return p.count(",") + 1


def strip(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    text = re.sub(r'"(\\.|[^"\\])*"', '""', text)
    return text


files = {}
for d, _, fs in os.walk(SRC):
    for f in fs:
        if f.endswith(".c"):
            files[os.path.join(d, f)] = strip(open(os.path.join(d, f), encoding="latin-1").read())

defs = {}
for p, t in files.items():
    for m in DEF.finditer(t):
        name = m.group(1)
        if name in ("if", "while", "for", "switch", "return", "sizeof"):
            continue
        defs.setdefault(name, (count_params(m.group(2)), p))

# asm alias map: local name -> symbol
alias = {}
for p, t in files.items():
    for m in re.finditer(r"\b(\w+)\s*\([^;]*\)\s*__asm__\(\"(\w+)\"\)", t):
        alias[m.group(1)] = m.group(2)


def args_of(t, i):
    """i = index just after '('; returns arg count or None."""
    depth, n, start, any_tok = 1, 0, i, False
    while i < len(t) and depth:
        c = t[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 1:
            n += 1
        elif not c.isspace() and depth >= 1:
            any_tok = True
        i += 1
    return (n + 1) if any_tok else 0


seen = set()
for p, t in files.items():
    for m in CALL.finditer(t):
        name = m.group(1)
        sym = alias.get(name, name)
        if sym not in defs:
            continue
        # skip the definition / declarations themselves: look at what follows the ')'
        nargs = args_of(t, m.end())
        # find matching ')'
        depth, j = 1, m.end()
        while j < len(t) and depth:
            depth += (t[j] == "(") - (t[j] == ")")
            j += 1
        after = t[j:j + 40].lstrip()
        if after.startswith(("{", "__asm__")) or after.startswith(";") and re.search(r"(\b(s32|u32|void|int|s16|u16|u8|s8|long|char|short)\b[\s\*]*)" + re.escape(name) + r"\s*\($", t[max(0, m.start() - 60):m.end()]):
            continue
        want, dp = defs[sym]
        if want < 0 or nargs == want:
            continue
        line = t.count("\n", 0, m.start()) + 1
        key = (p, line, name)
        if key in seen:
            continue
        seen.add(key)
        print(f"{p}:{line}: {name}{'->' + sym if sym != name else ''} called with {nargs}, defined with {want} ({os.path.basename(dp)})")
