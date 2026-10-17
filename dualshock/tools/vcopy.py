"""vcopy.py <in.c> <out.c>: m2c's four word copies between two constant addresses
    M2C_FIELD((void *)0xA, s32 *, 0) = (s32) M2C_FIELD((void *)0xB, s32 *, 0);  ... 4, 8, 0xC
are one 16-byte struct copy in Capcom's source: `*(VECTOR *)0xA = *(VECTOR *)0xB;` (the original
loads all four words into t0-t3 first). Same as refine's `structcopy`, standalone so it can run in
a chain (stage7 func_8011A31C). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
line = r"[ \t]*M2C_FIELD\(\(void \*\)(0x[0-9A-F]+), s32 \*, {o}\) = \(s32\) M2C_FIELD\(\(void \*\)(0x[0-9A-F]+), s32 \*, {o}\);\n"
pat = re.compile(r"\n([ \t]*)M2C_FIELD\(\(void \*\)(?P<d>0x[0-9A-F]+), s32 \*, 0\) = \(s32\) M2C_FIELD\(\(void \*\)(?P<s>0x[0-9A-F]+), s32 \*, 0\);\n"
                 r"[ \t]*M2C_FIELD\(\(void \*\)(?P=d), s32 \*, 4\) = \(s32\) M2C_FIELD\(\(void \*\)(?P=s), s32 \*, 4\);\n"
                 r"[ \t]*M2C_FIELD\(\(void \*\)(?P=d), s32 \*, 8\) = \(s32\) M2C_FIELD\(\(void \*\)(?P=s), s32 \*, 8\);\n"
                 r"[ \t]*M2C_FIELD\(\(void \*\)(?P=d), s32 \*, 0xC\) = \(s32\) M2C_FIELD\(\(void \*\)(?P=s), s32 \*, 0xC\);\n")
s = pat.sub(lambda m: f"\n{m.group(1)}*(VECTOR *){m.group('d')} = *(VECTOR *){m.group('s')};\n", s)
open(sys.argv[2], "w", newline="\n").write(s)
