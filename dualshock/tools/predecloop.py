"""predecloop.py <in.c> <out.c>: rewrite m2c's counted loops on a memory counter (scratchpad
words like 0x1F80001C) into the pre-decrement form Capcom wrote.

m2c prints `while (--*(s32 *)X != -1) { BODY }` as

    t1 = *(s32 *)X - 1;
    *(s32 *)X = t1;
    if (t1 != -1) {
        do {
            BODY
            t2 = *(s32 *)X - 1;
            *(s32 *)X = t2;
        } while (t2 != -1);
    }

and `do { BODY } while (--*(s32 *)X != -1);` as the inner do-while alone. Both are rewritten.
A preceding `*(s8 *)X = (s32) n; t1 = n - 1; *(s32 *)X = t1;` (m2c's view of `*(s32 *)X = n;`
followed by the first decrement) becomes `*(s32 *)X = n;`. Written as plain statements GCC's flow
pass deletes the first store (main func_80012ED0, func_800241B4). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
LV = r"\*\((?:s32|u32|s16|u16|s8|u8) \*\)(?:0x[0-9A-Fa-f]+|\w+)"
# 1) if (t1 != -1) { do { BODY t2 = X - 1; X = t2; } while (t2 != -1); }  preceded by  t1 = X - 1; X = t1;
pat1 = re.compile(
    r"(?:(?P<pind>[ \t]*)\*\(s8 \*\)(?P<px>0x[0-9A-Fa-f]+) = \(s32\) (?P<n>\w+);\n(?P<mid>(?:.*\n){0,2}?))?"
    r"(?P<ind>[ \t]*)(?P<t1>\w+) = (?P<src>" + LV + r"|\w+) - 1;\n"
    r"(?P=ind)(?P<x>" + LV + r") = (?P=t1);\n"
    r"(?P=ind)if \((?P=t1) != -1\) \{\n"
    r"(?P=ind)    do \{\n"
    r"(?P<body>(?:.*\n)*?)"
    r"(?P=ind)        (?P<t2>\w+) = (?P=x) - 1;\n"
    r"(?P=ind)        (?P=x) = (?P=t2);\n"
    r"(?P=ind)    \} while \((?P=t2) != -1\);\n"
    r"(?P=ind)\}\n")


def rep1(m):
    ind = m.group("ind")
    if m.group("src") != m.group("x"):
        # first decrement of a value just stored: only m2c's `*(s8 *)X = (s32) n;` form
        if m.group("n") != m.group("src") or not m.group("x").endswith(m.group("px")):
            return m.group(0)
        pre = f"{m.group('pind')}{m.group('x')} = {m.group('n')};\n{m.group('mid')}"
    else:
        pre = m.group(0)[:m.start("ind") - m.start()]  # optional prefix matched: keep it as is
    body = "".join(l[4:] if l.startswith(ind + "        ") else l
                   for l in m.group("body").splitlines(True))
    return f"{pre}{ind}while (--{m.group('x')} != -1) {{\n{body}{ind}}}\n"


s = pat1.sub(rep1, s)

# 2) do { BODY t = X - 1; X = t; } while (t != -1);
pat2 = re.compile(
    r"(?P<ind>[ \t]*)do \{\n"
    r"(?P<body>(?:.*\n)*?)"
    r"(?P=ind)    (?P<t>\w+) = (?P<x>" + LV + r") - 1;\n"
    r"(?P=ind)    (?P=x) = (?P=t);\n"
    r"(?P=ind)\} while \((?P=t) != -1\);\n")
s = pat2.sub(lambda m: f"{m.group('ind')}do {{\n{m.group('body')}{m.group('ind')}}} while (--{m.group('x')} != -1);\n", s)

# drop declarations of temporaries that are no longer used
head, sep, body = s.partition("{")
for d in re.findall(r"^[ \t]+\w[\w ]*?[ \t\*]+(\w+);\n", body, re.M):
    if len(re.findall(r"\b" + d + r"\b", body)) == 1:
        body = re.sub(r"^[ \t]+\w[\w ]*?[ \t\*]+" + d + r";\n", "", body, count=1, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(head + sep + body)
