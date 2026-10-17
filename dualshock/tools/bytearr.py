"""bytearr.py <in.c> <out.c> [--s8|--s16|--u16]: m2c sometimes types a table it can't identify as
words: `*(s32 *)((u8 *)&D_X + (i))` (an `lbu` in the original became `lw`), next to correct
`*(u8 *)((u8 *)&D_X + (i))` reads. Rewrite every such access of a table whose declaration is
M2C_UNK as `D_X[i]` and declare it `extern u8 D_X[];` (--s8: s8) - stage6 func_80113F14,
stage4 func_8011409C. Halfword tables (--s16/--u16) have the index written as `((i) * 2)`:
`-*(s32 *)((u8 *)&D_X + ((i) * 2))` -> `-D_X[i]` (stage5 func_801143A4). Tables with other uses
(`&D_X` passed around) are left alone. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
flags = sys.argv[3:]
elem = next((f[2:] for f in flags if f in ("--s8", "--s16", "--u16")), "u8")
half = elem in ("s16", "u16")


def close(text, i):
    """Index just past the parenthesis matching the one at text[i]."""
    depth = 0
    for k in range(i, len(text)):
        depth += {"(": 1, ")": -1}.get(text[k], 0)
        if depth == 0:
            return k + 1
    return -1


def accesses(text, name):
    """[(start, end, index_text)] for `[(T) ]*(T *)((u8 *)&name + (I))`."""
    out = []
    for m in re.finditer(r"(?:\((?:s32|u32|s16|u16)\) )?\*\((?:s32|u32|u8|s8|s16|u16) \*\)\(\(u8 \*\)&" + name + r" \+ \(", text):
        i = m.end() - 1                       # the '(' of the index
        j = close(text, i)
        if j < 0 or j >= len(text) or text[j] != ")":
            return None
        out.append((m.start(), j + 1, text[i + 1:j - 1]))
    return out


def index(i):
    if not half:
        return i
    i = i.strip()
    while i.startswith("(") and close(i, 0) == len(i):
        i = i[1:-1].strip()
    if i.endswith(" * 2"):
        i = i[:-4].strip()
        if i.startswith("(") and close(i, 0) == len(i):
            i = i[1:-1]
        return i
    return None


for name in sorted(set(re.findall(r"extern M2C_UNK (D_[0-9A-F]{8});", s))):
    acc = accesses(s, name)
    if not acc or any(index(a[2]) is None for a in acc):
        continue
    t = s
    for a, b, i in reversed(acc):
        t = t[:a] + f"{name}[{index(i)}]" + t[b:]
    if len(re.findall(rf"\b{name}\b", t)) != 1 + len(acc):   # declaration + the rewritten uses
        continue
    s = t.replace(f"extern M2C_UNK {name};", f"extern {elem} {name}[];")
open(sys.argv[2], "w", newline="\n").write(s)
