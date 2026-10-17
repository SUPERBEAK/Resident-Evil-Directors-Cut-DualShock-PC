"""temptype.py <in.c> <out.c> <k> [s32|u32]: widen the k-th narrow local temporary (u8/s8/u16/s16
`temp_*` / `var_*` declaration, in declaration order) of a draft to s32 (or u32).

At -O2 a narrow temp adds `andi`/`sll+sra` truncations or changes signedness of a compare
(`sltu` vs `slt`); m2c types temps from their first use, which is often narrower than Capcom's
int variable (main func_80031A74, func_80059D9C, func_80059EBC). For trybatch with k = 0, 1, ..."""
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[3])
wide = sys.argv[4] if len(sys.argv) > 4 else "s32"
decls = list(re.finditer(r"^(    )(u8|s8|u16|s16)( (?:temp|var)_\w+;)$", s, re.M))
if k < len(decls):
    m = decls[k]
    s = s[:m.start()] + m.group(1) + wide + m.group(3) + s[m.end():]
open(sys.argv[2], "w", newline="\n").write(s)
