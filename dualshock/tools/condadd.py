"""condadd.py <in.c> <out.c> [--mul|--tern]: Capcom's conditional increments
    p->x86 += (f() == 0) ? 4 : 0;          (stage5 func_8011BEDC)
    p->x16C += (rand() & 0xFFF) * !(p->x16E & 1);   (stage6 func_80113B28)
are rewritten by GCC's fold into `c ? A + K : A + 0` with A (and the lvalue address) in SAVE_EXPRs,
which live in s0/s1/s2 across the call. m2c shows that as
    temp_s0 = A;  [temp_sY = X;]  if (c) { var = temp_s0 + K_or_temp_sY; } else { var = temp_s0; }
    LHS = var;                       (or `*temp_s2 = var;` with temp_s2 = &A set first)
Rewrite back: constant K -> `A += (c) ? K : 0;`, a value X -> `A += X * (c);` (--tern:
`A += (c) ? X : 0;`, --mul with constant K: `A += K * (c);`). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
flags = sys.argv[3:]


def uses(name):
    return len(re.findall(rf"\b{name}\b", s))


def assign(name, before):
    """(match, rhs) of the last `name = rhs;` line before position `before`."""
    found = None
    for m in re.finditer(rf"\n[ \t]*{name} = ([^;\n]+);(?=\n)", s[:before + 1]):
        found = m
    return found


def drop_decl(name):
    global s
    if uses(name) == 1:
        s = re.sub(rf"\n[ \t]*(?:register )?[\w ]+\*?\s*\b{name};(?=\n)", "", s, count=1)


pat = re.compile(
    r"\n(?P<i>[ \t]*)if \((?P<c>.*)\) \{\n(?P=i)    (?P<v>\w+) = (?P<a>[\w]+) (?P<op>[+-]) (?P<x>[\w]+);\n"
    r"(?P=i)\} else \{\n(?P=i)    (?P=v) = (?P=a);\n(?P=i)\}\n(?P=i)(?P<lhs>[^=\n;]+?) = (?P=v);\n")
pat2 = re.compile(
    r"\n(?P<i>[ \t]*)if \((?P<c>.*)\) \{\n(?P=i)    (?P<v>\w+) = (?P<a>[\w]+);\n"
    r"(?P=i)\} else \{\n(?P=i)    (?P=v) = (?P=a) (?P<op>[+-]) (?P<x>[\w]+);\n(?P=i)\}\n(?P=i)(?P<lhs>[^=\n;]+?) = (?P=v);\n")

for p, neg in ((pat, False), (pat2, True)):
    while True:
        m = p.search(s)
        if not m:
            break
        a, x, v, lhs, c, ind = m.group("a"), m.group("x"), m.group("v"), m.group("lhs"), m.group("c"), m.group("i")
        if not a.startswith("temp_") or uses(v) != 4:
            break
        aa = assign(a, m.start())
        if not aa or uses(a) != 4:
            break
        load = re.sub(r"^\((?:[us](?:8|16|32))\) ", "", aa.group(1))
        c = re.sub(r"( & (?:0x[0-9A-F]+|\d+)) & 0xFF(?:FF)?\b", r"\1", c)   # m2c's extra mask
        cond = f"!({c})" if neg else (c if re.fullmatch(r"!\([^()]*(?:\([^()]*\)[^()]*)*\)", c) else f"({c})")
        removed = [aa]
        if re.fullmatch(r"0x[0-9A-F]+|\d+", x):
            val, const = x, True
        else:
            xa = assign(x, m.start())
            if not xa or uses(x) != 3:
                break
            val, const = xa.group(1), False
            removed.append(xa)
        target = lhs
        if lhs.startswith("*"):
            pa = assign(lhs[1:], m.start())
            if not pa or uses(lhs[1:]) != 3:
                break
            target = load.replace("M2C_FIELD(", "M2C_PFIELD(", 1) if load.startswith("M2C_FIELD(") else load
            removed.append(pa)
        if const:
            expr = f"{val} * {cond}" if "--mul" in flags else f"{cond} ? {val} : 0"
        else:
            expr = f"{cond} ? ({val}) : 0" if "--tern" in flags else f"({val}) * {cond}"
        new = f"\n{ind}{target} {m.group('op')}= {expr};\n"
        s = s[:m.start()] + new + s[m.end():]
        for r in sorted(removed, key=lambda r: r.start(), reverse=True):
            s = s[:r.start()] + s[r.end():]
        for name in (a, x, v, lhs[1:] if lhs.startswith("*") else ""):
            if name:
                drop_decl(name)

# one-line form: `t = X; X = (c) ? t - K : t;` -> `X -= (c) ? K : 0;` (stage5 func_80117128)
tern = re.compile(r"\n(?P<i>[ \t]*)(?P<t>temp_s\w+) = (?P<x>[^;\n]+);\n(?P=i)(?P=x) = \((?P<c>.*)\) \? (?P=t) (?P<op>[+-]) "
                  r"(?P<k>0x[0-9A-F]+|\d+) : (?P=t);\n")
while True:
    m = tern.search(s)
    if not m or uses(m.group("t")) != 4:
        break
    s = s[:m.start()] + f"\n{m.group('i')}{m.group('x')} {m.group('op')}= ({m.group('c')}) ? {m.group('k')} : 0;\n" + s[m.end():]
    drop_decl(m.group("t"))
# two constants: `t = X; X = (c) ? t - K1 : t - K2;` -> `X -= (c) ? K1 : K2;` (stage4 func_80124690)
tern2 = re.compile(r"\n(?P<i>[ \t]*)(?P<t>temp_s\w+) = (?P<x>[^;\n]+);\n(?P=i)(?P=x) = \((?P<c>.*)\) \? (?P=t) (?P<op>[+-]) "
                   r"(?P<k1>0x[0-9A-F]+|\d+) : (?P=t) (?P=op) (?P<k2>0x[0-9A-F]+|\d+);\n")
while True:
    m = tern2.search(s)
    if not m or uses(m.group("t")) != 4:
        break
    s = s[:m.start()] + (f"\n{m.group('i')}{m.group('x')} {m.group('op')}= ({m.group('c')}) ? {m.group('k1')} : "
                         f"{m.group('k2')};\n") + s[m.end():]
    drop_decl(m.group("t"))
open(sys.argv[2], "w", newline="\n").write(s)
