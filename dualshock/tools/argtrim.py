"""argtrim.py <in.c> <out.c> [k]: drop m2c's stale extra call arguments.

m2c passes whatever a0-a3 still hold when it cannot see the callee's signature, e.g.
`func_8004AFA8(0x800C3008, p->xDB, temp_a2)` where func_8004AFA8 takes two parameters (main
func_800258F0). For every call to a function already defined in C (src/*/*.c) with fewer
parameters than the call passes, the extra trailing arguments are removed. With k only the k-th
such call is changed (for trybatch k = 0, 1, ...). For trybatch."""
import glob
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[3]) if len(sys.argv) > 3 else None

nparams = {}
for path in glob.glob("src/*/*.c"):
    for m in re.finditer(r"^[A-Za-z_][\w \*]*?\b(func_[0-9A-F]{8})\s*\(([^;{)]*)\)\s*\{", open(path).read(), re.M):
        p = m.group(2).strip()
        nparams[m.group(1)] = 0 if p in ("", "void") else p.count(",") + 1


def split_args(text):
    """Split a call's argument text at top-level commas."""
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


head, sep, body = s.partition("{")
edits = []
for m in re.finditer(r"\b(func_[0-9A-F]{8})\(", body):
    name = m.group(1)
    if name not in nparams:
        continue
    i, depth = m.end(), 1
    while i < len(body) and depth:
        depth += {"(": 1, ")": -1}.get(body[i], 0)
        i += 1
    args = split_args(body[m.end():i - 1])
    if len(args) > nparams[name]:
        edits.append((m.end(), i - 1, ",".join(args[:nparams[name]]).strip()))
if k is not None:
    edits = edits[k:k + 1]
for a, b, new in reversed(edits):
    body = body[:a] + new + body[b:]
open(sys.argv[2], "w", newline="\n").write(head + sep + body)
