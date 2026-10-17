"""arrinit.py <in.c> <out.c> [--pad]: a local byte array with an initialiser,
`u8 sp18[8] = { 1, 2, 1, 1, 2, 1, 1, 2 };` - GCC copies the initialiser from the function's own
rodata with lwl/lwr pairs, which m2c can't decode:
    M2C_FIELD(&sp18, M2C_UNK *, 3) = M2C_UNALIGNED32(M2C_ERROR(/* ...lwr... */));
The destination (`&spXX`) and size (highest offset + 1) come from those lines; the bytes from the
function's own `.rodata` blocks in the .s, in order (the trailing zero word of a block is the
8-byte alignment pad, not data). Declares `u8 spXX[N] = {...};`, drops the broken lines and
rewrites `*(&spXX + i)` as `spXX[i]` (stage4 func_8011B904, func_8011B65C). --pad adds `s32`
padding locals before it so it lands at its offset. For trybatch; derives the .s from the draft
path build/auto/<target>/c/<func>.c."""
import glob
import os
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
p = os.path.abspath(src).replace("\\", "/")
m = re.search(r"build/auto/(\w+)/c/(func_[0-9A-F]{8})\.c$", p)
out = s
if m and "M2C_UNALIGNED32" in s:
    asm = glob.glob(f"asm/{m.group(1)}/nonmatchings/**/{m.group(2)}.s", recursive=True)
    if asm:
        a = open(asm[0]).read()
        blocks = []
        for body in re.findall(r"dlabel D_[0-9A-F]{8}\n(.*?)enddlabel", a.split(".section .text")[0], re.S):
            words = re.findall(r"\.word 0x([0-9A-F]{8})", body)
            if words and ".word ." not in body:
                blocks.append(b"".join(int(w, 16).to_bytes(4, "little") for w in words))
        dests = []
        for v in re.findall(r"M2C_FIELD\(&(sp[0-9A-F]+), M2C_UNK \*, \d+\) = M2C_UNALIGNED32", s):
            if v not in dests:
                dests.append(v)
        # the array's real stack offset: `addiu $x, $fp, OFF` (m2c sometimes names it after another
        # local, stage4 func_80121378: array at 0x18, m2c says sp10)
        fp_offs = []
        for o in re.findall(r"addiu\s+\$\w+, \$fp, 0x([0-9A-F]+)", a):
            if int(o, 16) not in fp_offs:
                fp_offs.append(int(o, 16))
        if blocks and len(dests) <= len(blocks):
            for k, (d, data) in enumerate(zip(dests, blocks)):
                offs = [int(x) for x in re.findall(rf"M2C_FIELD\(&{d}, M2C_UNK \*, (\d+)\) = M2C_UNALIGNED32", s)]
                n = max(offs) + 1
                data = data[:n]
                v = f"sp{fp_offs[k]:X}" if k < len(fp_offs) else d
                out = re.sub(rf"\n[ \t]*M2C_FIELD\(&{d}, M2C_UNK \*, \d+\) = M2C_UNALIGNED32\([^\n]*", "", out)
                still = re.sub(rf"\*\(&{d} \+ ", "", out)
                if v == d or len(re.findall(rf"\b{d}\b", still)) <= 1:      # no other use of m2c's name
                    out = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{d};", "", out, count=1)
                out = re.sub(rf"\*\(&{d} \+ (\((?:[^()]|\([^()]*\))*\)|[^)]+)\)", lambda mm: f"{v}[{mm.group(1)}]", out)
                decl = f"    u8 {v}[{n}] = {{ {', '.join(str(b) for b in data)} }};\n"
                if "--pad" in sys.argv[3:]:
                    off = int(v[2:], 16)
                    decl = "".join(f"    s32 pad{q:X};\n" for q in range(0x10, off, 4)) + decl
                    out = re.sub(r"\n    s32 unused(?:\[\d+\])?;", "", out)
                out = re.sub(r"(\n(?:void|s32|u32|s16|u16|u8|s8) func_[0-9A-F]{8}\([^)]*\) \{\n)",
                             lambda mm: mm.group(1) + decl, out, count=1)
                out = re.sub(rf"\*\(&{v} \+ (\((?:[^()]|\([^()]*\))*\)|[^)]+)\)", lambda mm: f"{v}[{mm.group(1)}]", out)
            out = re.sub(r"\n[ \t]*(?:\w+ = )?M2C_ERROR\([^\n]*", "", out)
            for t in set(re.findall(r"\n[ \t]*\w+ (temp_\w+);", out)):
                if len(re.findall(rf"\b{t}\b", out)) == 1:
                    out = re.sub(rf"\n[ \t]*\w+ {t};", "", out, count=1)
open(dst, "w", newline="\n").write(out)
