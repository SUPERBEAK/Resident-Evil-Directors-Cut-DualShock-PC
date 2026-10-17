"""scratch.py <in.c> <out.c> [k]: write scratchpad accesses `*(T *)0x1F80XXXX` as struct members
of the scratchpad, `M2C_SFIELD(0x1F800000, T *, 0xXXXX)`.

Capcom's main program reads the PSX scratchpad (0x1F800000) as a struct: the original code has
`lui v0,0x1f80; lw 0x3D4(v0)` per access, while a plain literal `*(s32 *)0x1F8003D4` used twice
gets its address CSE'd into a register (`lui; ori; lw 0(reg)`) - main func_80013B14.
With k, only the k-th distinct address is rewritten (trybatch k = 0, 1, ...); without k all."""
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[3]) if len(sys.argv) > 3 else None
pat = re.compile(r"\*\(([A-Za-z_]\w*(?:\s*\*)*?)\s*\*\)0x1F80([0-9A-F]{4})\b")
if "{" in s:
    head, body = s.split("{", 1)
    addrs = sorted(set(m.group(2) for m in pat.finditer(body)))
    if k is not None:
        addrs = addrs[k:k + 1]
    body = pat.sub(lambda m: f"M2C_SFIELD(0x1F800000, {m.group(1)} *, 0x{m.group(2)})"
                   if m.group(2) in addrs else m.group(0), body)
    s = head + "{" + body
open(sys.argv[2], "w", newline="\n").write(s)
