"""switch1.py <in.c> <out.c>: a one-case `switch (E) { case K: body; break; }` compiles (at -O0) to
`beq -> body; j end; body; j end`, which m2c shows as `if (E != K) { } else { body }`. Neither
`if (E == K)` nor the empty-then form produce the second jump; the switch does
(stage3 func_8011E104). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"\n([ \t]*)if \((?P<e>[^\n]*?) != (?P<k>0x[0-9A-F]+|\d+)U?\) \{\n[ \t]*\n\1\} else \{\n")
while True:
    m = pat.search(s)
    if not m:
        break
    ind = m.group(1)
    close = s.find(f"\n{ind}}}", m.end() - 1)      # the else's closing brace at the same indent
    if close < 0:
        break
    body = s[m.end():close + 1]
    new = f"\n{ind}switch ({m.group('e')}) {{\n{ind}case {m.group('k')}:\n{body}{ind}    break;\n{ind}}}"
    s = s[:m.start()] + new + s[close + len(ind) + 2:]
open(sys.argv[2], "w", newline="\n").write(s)
