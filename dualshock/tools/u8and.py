"""u8and.py <in.c> <out.c>: m2c writes byte flag updates as `F(p, s8 *, o) = (s8) (F(p, s8 *, o) & 0xFE);`
- an s8 load makes GCC build the mask as `li a0,-2; and`, the original has `andi 0xfe` (u8 load).
Rewrite both sides as u8 (also `| K`). For trybatch (stage2 func_8011920C)."""
import re
import sys

s = open(sys.argv[1]).read()
B = r"(?P<b>(?:[^(),]|\([^()]*\))+?)"
s = re.sub(r"(?P<m1>M2C_P?FIELD)\(" + B + r", s8 \*, (?P<o>0x[0-9A-Fa-f]+|\d+)\) = \(s8\) \((?P<m2>M2C_P?FIELD)\((?P=b), [su]8 \*, (?P=o)\) (?P<op>[&|^]) (?P<k>0x[0-9A-Fa-f]+|\d+)\);",
           lambda m: f"{m.group('m1')}({m.group('b')}, u8 *, {m.group('o')}) = (u8) ({m.group('m2')}({m.group('b')}, u8 *, {m.group('o')}) {m.group('op')} {m.group('k')});", s)
open(sys.argv[2], "w", newline="\n").write(s)
