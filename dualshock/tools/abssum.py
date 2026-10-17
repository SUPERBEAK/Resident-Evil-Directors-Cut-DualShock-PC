"""abssum.py <in.c> <out.c>: Capcom's distance `*(s32 *)0x1F800000 = abs(px - x34) + abs(pz - x3C);`
- m2c evaluates the second abs into a temp first:
    var_a0 = __builtin_abs(B);
    X = __builtin_abs(A) + var_a0;
Rewrite to one expression in source order, and declare D_800C5158 (the player x) as s32 - a
narrower extern makes GCC load it early (stage3 func_8011536C, func_8010E708). For trybatch."""
import re
import sys

ABS = r"__builtin_abs\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)"
s = open(sys.argv[1]).read()
pat = re.compile(r"\n(?P<i>[ \t]*)(?P<v>(?:var|temp)_\w+) = (?P<b>" + ABS + r");\n(?P=i)(?P<x>[^=\n]+) = "
                 r"\(?(?P<a>" + ABS + r")\)? \+ (?P=v);\n")
m = pat.search(s)
if m:
    v = m.group("v")
    s = s.replace(m.group(0), f"\n{m.group('i')}{m.group('x')} = {m.group('a')} + {m.group('b')};\n", 1)
    if len(re.findall(rf"\b{v}\b", s)) == 1:
        s = re.sub(rf"\n[ \t]*[\w ]+ {v};", "", s, count=1)
s = s.replace("extern s16 D_800C5158;", "extern s32 D_800C5158;")
open(sys.argv[2], "w", newline="\n").write(s)
