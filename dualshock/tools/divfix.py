"""divfix.py <in.c> <out.c> [--s16]: m2c writes a signed division by 2 as
`((s32) (X + ((u32) (X >> 0x1F) >> 0x1F)) >> 1)`, often with X a temp holding a struct field.
Rewrite it as `(X / 2)` (with --s16: `(s16) (X / 2)`, the form when the result is passed to a
call taking a short - stage7 func_8011CF54) and inline the temp when that was its only use.
For trybatch: run plain and with --s16."""
import re
import sys

s = open(sys.argv[1]).read()
cast = "(s16) " if "--s16" in sys.argv[3:] else ""
pat = re.compile(r"\(\(s32\) \((?P<x>[^()]+|\w+\([^()]*\)) \+ \((?:\(u32\) )?\((?P=x) >> 0x1F\) >> 0x1F\)\) >> 1\)")


def count(name):
    return len(re.findall(rf"\b{re.escape(name)}\b", s))


while True:
    m = pat.search(s)
    if not m:
        break
    x = m.group("x")
    rep = f"{cast}({x} / 2)"
    if re.fullmatch(r"temp_\w+", x) and count(x) == 4:     # declaration, assignment, two uses
        a = re.search(rf"\n[ \t]*{x} = ([^;\n]+);\n", s)
        if a:
            rep = f"{cast}({a.group(1)} / 2)"
            s = s[:m.start()] + rep + s[m.end():]
            s = s.replace(a.group(0), "\n", 1)
            s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{x};(?=\n)", "", s, count=1)
            continue
    s = s[:m.start()] + rep + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
