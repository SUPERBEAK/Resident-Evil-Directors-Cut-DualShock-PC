"""callmerge.py <in.c> <out.c>: m2c splits the stage engine's call-table idiom
`(D_801FEC18 = D_X)(args)` into `D_801FEC18 = D_X;` + a call `D_X(args)` - also when the call is
inside a condition or an assignment. Merge them back (stage7 func_80119ED0).
Inside a condition m2c uses a comma expression instead: `(D_801FEC18 = D_X, (D_X(args) == 0))`
-> `((D_801FEC18 = D_X)(args) == 0)` (stage4 func_80128818, func_80113010). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
# a call-table function passed as the argument of another one (title func_800E0064):
# `D_801FEC18 = X; D_801FEC18 = Y; X((T *) Y);` -> `(D_801FEC18 = X)(D_801FEC18 = Y);`
s = re.sub(r"\n([ \t]*)D_801FEC18 = (D_801FE[0-9A-F]{3});\n[ \t]*D_801FEC18 = (D_801FE[0-9A-F]{3});\n[ \t]*\2\((?:\([^()]*\) )?\3\);",
           lambda m: f"\n{m.group(1)}(D_801FEC18 = {m.group(2)})(D_801FEC18 = {m.group(3)});", s)
s = re.sub(r"\n[ \t]*D_801FEC18 = (D_801FE[0-9A-F]{3});\n([^\n]*?)\b\1\(",
           lambda m: f"\n{m.group(2)}(D_801FEC18 = {m.group(1)})(", s)

COMMA = re.compile(r"\(D_801FEC18 = (D_801FE[0-9A-F]{3}), \(\1\(")
while True:
    m = COMMA.search(s)
    if not m:
        break
    depth, k = 0, m.start()
    while k < len(s):                       # find the comma expression's closing parenthesis
        depth += {"(": 1, ")": -1}.get(s[k], 0)
        if depth == 0:
            break
        k += 1
    if k >= len(s) or s[k - 1] != ")":
        break
    # drop the inner group's `)` (just before the outer one) and rewrite the head
    s = s[:k - 1] + s[k:]
    s = s[:m.start()] + f"((D_801FEC18 = {m.group(1)})(" + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
