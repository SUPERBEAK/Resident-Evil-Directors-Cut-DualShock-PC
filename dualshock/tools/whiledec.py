"""whiledec.py <in.c> <out.c>: m2c writes Capcom's `while (--x != -1) { body }` as
    loop_N:
        if ((x = x - 1) != -1) { body  goto loop_N; }
(GCC puts a label nop before the test, which the goto form doesn't reproduce) -
stage3 func_8011F7B0. Also `!= K` tests with other constants. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"\n(?P<lab>loop_\d+):\n(?P<i>[ \t]*)if \(\((?P<x>[^=\n]+?) = (?P=x) - 1\) != (?P<k>-?\w+)\) \{\n"
                 r"(?P<body>(?:(?P=i)    [^\n]*\n|\n)*?)(?P=i)    goto (?P=lab);\n(?P=i)\}\n")
while True:
    m = pat.search(s)
    if not m or s.count(m.group("lab")) != 2:
        break
    i = m.group("i")
    s = s[:m.start()] + f"\n{i}while (--{m.group('x')} != {m.group('k')}) {{\n{m.group('body')}{i}}}\n" + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
