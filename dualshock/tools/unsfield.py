"""unsfield.py <in.c> <out.c> [k]: the reverse of sfield.py - turn `M2C_SFIELD(...)` accesses of a
main-program draft back into `M2C_FIELD(...)` (plain pointer access).

Earlier refine/trybatch passes left single M2C_SFIELD accesses in many drafts; when the original
keeps that load after a store to a global (or a store before a load), the struct flag is wrong
(main func_80039580: the second read of arg0->x4 must stay after the D_800C515C store).
Scratchpad accesses `M2C_SFIELD(0x1F800000, ...)` are left alone. With k only the k-th
M2C_SFIELD is changed (for trybatch k = 0, 1, ...); without k all of them."""
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[3]) if len(sys.argv) > 3 else None
head, sep, body = s.partition("{")
hits = [m for m in re.finditer(r"\bM2C_SFIELD\((?!0x1F800000)", body)]
if k is not None:
    hits = hits[k:k + 1]
for m in reversed(hits):
    body = body[:m.start()] + "M2C_FIELD(" + body[m.end():]
open(sys.argv[2], "w", newline="\n").write(head + sep + body)
