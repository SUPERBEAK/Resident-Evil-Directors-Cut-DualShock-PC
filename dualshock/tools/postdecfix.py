"""postdecfix.py <in.c> <out.c>: struct-typed drafts write Capcom's signed post-decrement test as
`D_800CAB14->unk0C4-- == 0`; the matching form is `(u32) M2C_FIELD(D_800CAB14, s16 *, 0xC4)-- == 0`
(see refine.py `postdec`). For tools/trybatch.py."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"^(\s*)if \(D_800CAB14->unk([0-9A-F]{3})(--|\+\+) (==|!=) 0\)",
           lambda m: f"{m.group(1)}if ((u32) M2C_FIELD(D_800CAB14, s16 *, 0x{m.group(2)}){m.group(3)} {m.group(4)} 0)",
           s, flags=re.M)
s = re.sub(r"\(D_800CAB14->unk([0-9A-F]{3})(--|\+\+) (==|!=) 0\)",
           lambda m: f"((u32) M2C_FIELD(D_800CAB14, s16 *, 0x{m.group(1)}){m.group(2)} {m.group(3)} 0)", s)
open(sys.argv[2], "w", newline="\n").write(s)
