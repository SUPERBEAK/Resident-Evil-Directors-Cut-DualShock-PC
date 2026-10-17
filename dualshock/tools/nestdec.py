"""nestdec.py <in.c> <out.c> [--all]: `--p->q->x` on a field reached through a pointer field.
GCC -O0 expands the pre-decrement the long way (target address expanded once and dropped, then
`x = x - 1`), leaving a dead `lw v0,0xB8(v1)` in front - m2c drops that and writes
`M2C_FIELD(M2C_FIELD(D, void **, 0xB8), s16 *, 0x70) = M2C_FIELD(M2C_SFIELD(D, void **, 0xB8), s16 *, 0x70) - 1;`.
Rewrite as `--M2C_FIELD(M2C_FIELD(D, void **, 0xB8), s16 *, 0x70);` (also `+ 1` -> `++`)
(stage4 func_80127C0C). Without --all only the first statement changes. For trybatch."""
import re
import sys


def norm(t):
    return re.sub(r"\bM2C_[SP]?FIELD\(", "M2C_FIELD(", re.sub(r"\(\w+\) ", "", t)).replace(" ", "")


s = open(sys.argv[1]).read()
do_all = "--all" in sys.argv[3:]
out, done = [], False
for line in s.split("\n"):
    m = re.fullmatch(r"(\s*)(M2C_\w*FIELD\(M2C_\w*FIELD\(.+\)) = (?:\(\w+\) )?\(?(M2C_\w*FIELD\(M2C_\w*FIELD\(.+\)) ([-+]) 1\)?;", line)
    if m and not (done and not do_all) and norm(m.group(2)) == norm(m.group(3)):
        line = f"{m.group(1)}{'--' if m.group(4) == '-' else '++'}{m.group(2)};"
        done = True
    out.append(line)
open(sys.argv[2], "w", newline="\n").write("\n".join(out))
