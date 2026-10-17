"""rettype.py <in.c> <out.c> func_X=type [func_Y=type ...]: set the declared return type of a
callee in a draft (`s32 func_80126B70();` -> `u8 func_80126B70();`). m2c guesses `s32`; when the
real function returns a byte, `p->x87 += func(...)` keeps the result in `v0` instead of copying
it first (stage4 func_8011FEA0). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
for spec in sys.argv[3:]:
    name, typ = spec.split("=", 1)
    s = re.sub(rf"^(\s*)(?:extern )?(?:[su](?:8|16|32)|M2C_UNK|void|\? ?) ?(?:\*\s*)?{name}\(([^;]*)\);",
               lambda m: f"{m.group(1)}{typ} {name}({m.group(2)});", s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
