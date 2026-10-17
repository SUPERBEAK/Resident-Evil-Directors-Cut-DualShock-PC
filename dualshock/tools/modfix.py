"""modfix.py <in.c> <out.c>: m2c expands Capcom's modulo `X = (E) % K;` (with --expand-div GCC
emits the division sequence) into `t = E; X = (T) (t - ((t / K) * K));`. Rewrite it back
(stage5 func_8011401C, func_80115610). Also the temp-less form `X = (T) (E - ((E / K) * K))`.
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
m = None
while True:
    m = re.search(r"\n(\s*)(temp_\w+) = ([^;\n]+);\n\s*([^=\n]+?) = (?:\((?:u8|s8|u16|s16|s32|u32)\) )?\(\2 - \(\(\2 / (\w+)\) \* \5\)\);", s)
    if not m or len(re.findall(rf"\b{m.group(2)}\b", s)) != 5:
        break
    s = s.replace(m.group(0), f"\n{m.group(1)}{m.group(4)} = ({m.group(3)}) % {m.group(5)};", 1)
    s = re.sub(rf"\n[ \t]*\w+ {m.group(2)};", "", s, count=1)
s = re.sub(r"\(([\w>.\-]+(?:\([^()]*\))?) - \(\(\1 / (\w+)\) \* \2\)\)", r"(\1 % \2)", s)

# Divisors whose magic needs the add-back step (7): m2c keeps the dividend and the high half
# as temps, `t = E; h = E / 7; ... (t - (((u32) (h + ((u32) (t - h) >> 1)) >> 2) * 7)) ...`.
# -> `(E % 7)` (stage3 func_80124F64).
while True:
    m = re.search(r"\n[ \t]*(temp_\w+) = ([^;\n]+);\n[ \t]*(temp_\w+) = \2 / (\w+);\n", s)
    if not m:
        break
    t, e, h, k = m.group(1), m.group(2), m.group(3), m.group(4)
    use = f"({t} - (((u32) ({h} + ((u32) ({t} - {h}) >> 1)) >> "
    i = s.find(use, m.end())
    j = s.find(f") * {k}))", i)
    if i < 0 or j < 0 or not re.fullmatch(r"\d+", s[i + len(use):j]):
        break
    s = s[:i] + f"({e} % {k})" + s[j + len(f") * {k}))"):]
    s = s.replace(m.group(0), "\n", 1)
    for v in (t, h):
        if len(re.findall(rf"\b{v}\b", s)) == 1:
            s = re.sub(rf"\n[ \t]*\w+ {v};", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
