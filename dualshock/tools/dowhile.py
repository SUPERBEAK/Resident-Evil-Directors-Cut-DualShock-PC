"""dowhile.py <in.c> <out.c>: m2c writes Capcom's `do { body } while (--x != -1);` as
    loop_N:
        body
        if ((x = x - 1) == -1) { goto block_M; }
        goto loop_N;
    }
    block_M:
Rewrite it back to the do/while (stage7 func_801198C4, stage6 func_8011E670). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"\n(?P<lab>loop_\d+):\n(?P<body>.*?)\n(?P<ind>[ \t]*)if \(\((?P<x>[^=\n]+?) = (?P=x) - 1\) == -1\) \{\n"
                 r"[ \t]*goto (?P<blk>block_\d+);\n(?P=ind)\}\n(?P=ind)goto (?P=lab);\n(?P<close>[ \t]*\}\n)(?P=blk):\n", re.S)
m = pat.search(s)
if m and m.group("body").count(m.group("lab")) == 0 and s.count(m.group("blk")) == 2:
    ind = m.group("ind")
    new = (f"\n{ind}do {{\n{m.group('body')}\n{ind}}} while (--{m.group('x')} != -1);\n{m.group('close')}")
    s = s[:m.start()] + new + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
