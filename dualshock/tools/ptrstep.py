"""ptrstep.py <in.c> <out.c> [--all]: `p += 0x7C;` on a local pointer, where the original steps a
pointer to a 0x7C-byte record with `p++` - GCC's post-increment adds a `move` (`lw v1; addiu v0,v1,124;
move v1,v0; sw v1`). Rewrite as `((struct { u8 b[0x7C]; } *)p)++;` (a GCC cast-as-lvalue; stage2
func_801163E0). Without --all only the first statement changes. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
count = 0 if "--all" in sys.argv[3:] else 1
s = re.sub(r"^([ \t]*)(sp[0-9A-F]+|var_\w+|temp_\w+) \+= (0x[0-9A-Fa-f]+|\d+);$",
           lambda m: f"{m.group(1)}((struct {{ u8 b[{m.group(3)}]; }} *){m.group(2)})++;", s, count=count, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
