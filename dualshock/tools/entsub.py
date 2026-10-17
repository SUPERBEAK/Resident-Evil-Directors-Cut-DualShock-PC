"""entsub.py <in.c> <out.c> [--all]: drafts typed with the Entity struct write the work sub-struct as
`D_800CAB14->sub16C[3] = 3;`, which compiles to `sb v1,367(v0)`; Capcom reached it through its
address (`addiu v0,v1,0x16C; move v1,v0; sb ..,3(v1)`). Rewrite to
`M2C_FIELD(((u8 *)D_800CAB14 + 0x16C), u8 *, 3)` - stores only, or with --all every access
(stage3 func_80120350). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
if "--all" in sys.argv[3:]:
    s = re.sub(r"D_800CAB14->sub16C\[(0x[0-9A-Fa-f]+|\d+)\]", r"M2C_FIELD(((u8 *)D_800CAB14 + 0x16C), u8 *, \1)", s)
else:
    s = re.sub(r"D_800CAB14->sub16C\[(0x[0-9A-Fa-f]+|\d+)\] = ", r"M2C_FIELD(((u8 *)D_800CAB14 + 0x16C), u8 *, \1) = ", s)
open(sys.argv[2], "w", newline="\n").write(s)
