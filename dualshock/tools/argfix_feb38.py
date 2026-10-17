"""argfix_feb38.py <in.c> <out.c>: m2c drops the second argument of the stage engine call
`D_801FEB38` (loaded with `lbu a1, OFF(entity)` just before the call). Known pairs:
0x800C3008 -> entity byte 0xDB, 0x800C86BC -> entity byte 0x163 (stage3 func_8011C180,
func_8011FA24). Also merges m2c's split `D_801FEC18 = D_801FEB38; D_801FEB38(...)`, and accepts
a `(void *)` cast on the address (stage5 func_801167CC). For trybatch."""
import re
import sys

ARG2 = {"0x800C3008": "0xDB", "0x800C86BC": "0x163"}
s = open(sys.argv[1]).read()
s = re.sub(r"\n(\s*)D_801FEC18 = D_801FEB38;\n\s*D_801FEB38\(", r"\n\1(D_801FEC18 = D_801FEB38)(", s)


def rep(m):
    a = m.group(2).rstrip("U")
    return f"(D_801FEC18 = D_801FEB38)({m.group(1) or ''}{m.group(2)}, M2C_FIELD(D_800CAB14, u8 *, {ARG2[a]}))"


s = re.sub(r"\(D_801FEC18 = D_801FEB38\)\((\(void \*\))?(0x800C3008U?|0x800C86BCU?)\)", rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
