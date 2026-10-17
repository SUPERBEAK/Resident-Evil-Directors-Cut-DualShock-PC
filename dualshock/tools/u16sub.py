"""u16sub.py <in.c> <out.c>: `F(b, u16 *, o) = (u16) (F(b, u16 *, o) - K);` compiles to
`li a1, 0x10000-K; addu` for an unsigned short; Capcom's field was signed: `F(b, s16 *, o) -= K;`
gives the original `addiu -K` (stage5 func_8011AE34). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"(M2C_P?FIELD)\(((?:[^(),]|\([^()]*\))+?), u16 \*, (0x[0-9A-F]+|\d+)\) = \(u16\) \(\1\(\2, u16 \*, \3\) - (0x[0-9A-F]+|\d+)\);",
           r"\1(\2, s16 *, \3) -= \4;", s)
open(sys.argv[2], "w", newline="\n").write(s)
