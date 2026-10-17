"""gotoloop.py <in.c> <out.c> [--do]: m2c writes a loop whose exit test sits at the bottom as
    loop_28:
        BODY
        if (COND) {
            goto loop_28;
        }
GCC -O0 output `bne COND, skip; j exit; skip: j top` is Capcom's
    for (;;) { BODY; if (!COND) break; }
(stage3 func_8012137C). With --do the rewrite is `do { BODY } while (COND);` instead (a single
`bne` back to the top). The comparison is flipped when it is a plain one. For trybatch."""
import re
import sys

FLIP = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}


def negate(c):
    m = re.fullmatch(r"\((.*)\) (==|!=|<=|>=|<|>) (-?\w+)", c)
    if m and m.group(1).count("(") == m.group(1).count(")"):
        return f"({m.group(1)}) {FLIP[m.group(2)]} {m.group(3)}"
    return f"!({c})"


s = open(sys.argv[1]).read()
do_form = "--do" in sys.argv[3:]
pat = re.compile(r"\n(?P<i>[ \t]*)(?P<l>loop_\d+):\n(?P<body>(?:(?P=i)[^\n]*\n|\n)*?)(?P=i)if \((?P<c>[^\n]*)\) \{\n(?P=i)    goto (?P=l);\n(?P=i)\}\n")


def rep(m):
    i, body = m.group("i"), m.group("body")
    if m.group("l") + ";" in body or re.search(rf"\bgoto {m.group('l')};", s[m.end():]):
        return m.group(0)
    body = "".join(("    " + l if l else l) + "\n" for l in body.rstrip("\n").split("\n"))
    if do_form:
        return f"\n{i}do {{\n{body}{i}}} while ({m.group('c')});\n"
    return f"\n{i}for (;;) {{\n{body}{i}    if ({negate(m.group('c'))}) {{\n{i}        break;\n{i}    }}\n{i}}}\n"


s = pat.sub(rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
