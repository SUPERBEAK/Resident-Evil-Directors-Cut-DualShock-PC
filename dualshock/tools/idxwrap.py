"""idxwrap.py <in.c> <out.c>: wrap array indexes of the form `[EXPR - K]` / `[EXPR + K]` in a
statement expression: `tbl[({ EXPR - K; })]`.

At -O2, GCC's pointer_int_sum() and expand_expr(EXPAND_SUM) apply the distributive law and fold
`tbl[x - 1]` into `(tbl - 4)[x]`, so the `addiu -1` before the `sll` disappears. The statement
expression hides the PLUS/MINUS from both (main func_8005A010). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()


def wrap(m):
    inner = m.group(1)
    # only plain "EXPR +/- constant" indexes, balanced parentheses, no statement expression yet
    if "({" in inner or inner.count("(") != inner.count(")"):
        return m.group(0)
    if not re.search(r"\s[-+]\s(?:0x[0-9A-Fa-f]+|\d+)U?\)?$", inner):
        return m.group(0)
    body = inner[1:-1] if inner.startswith("(") and inner.endswith(")") else inner
    return "[({ " + body + "; })]"


s = re.sub(r"\[((?:[^\[\]]|\[[^\[\]]*\])+)\]", wrap, s)
open(sys.argv[2], "w", newline="\n").write(s)
