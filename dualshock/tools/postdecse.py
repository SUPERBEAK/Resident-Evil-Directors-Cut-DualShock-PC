"""postdecse.py <in.c> <out.c>: a byte post-decrement tested for zero, `if (!(F(B, s8 *, O)-- & 0xFF))`
in m2c's output. The original decrements as unsigned (`addiu a0,v1,255`) and tests the old value
with `andi 0xff`; with a u8 field GCC folds `x-- == 0` into `--x == 255`. What matches is a
statement expression: `if (!({ (s32) M2C_FIELD(B, u8 *, O)--; }))` (stage4 func_80125794).
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"if \((?P<neg>!?)\((?P<m>M2C_P?FIELD)\((?P<b>(?:[^(),]|\([^()]*\))+?), [su]8 \*, (?P<o>0x[0-9A-Fa-f]+|\d+)\)-- & 0xFF\)\) \{")


def rep(m):
    e = f"({{ (s32) {m.group('m')}({m.group('b')}, u8 *, {m.group('o')})--; }})"
    return f"if ({m.group('neg')}{e}) {{"


s = pat.sub(rep, s)
# m2c's `(u32) F(B, s8 *, O)-- == C` (old value compared signed, sll/sra 24) ->
# `({ (s32) F(B, s8 *, O)--; }) == C` (stage4 func_80127668).
s = re.sub(r"if \(\(u32\) (?P<f>M2C_P?FIELD\((?:[^()]|\([^()]*\))+?, s8 \*, (?:0x[0-9A-Fa-f]+|\d+)\))-- (?P<op>==|!=) (?P<c>-?(?:0x[0-9A-Fa-f]+|\d+))\) \{",
           lambda m: f"if (({{ (s32) {m.group('f')}--; }}) {m.group('op')} {m.group('c')}) {{", s)
open(sys.argv[2], "w", newline="\n").write(s)
