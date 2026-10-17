"""litbase.py <in.c> <out.c> [k]: write array accesses `D_XXXXXXXX[i]` of a draft through the
literal address instead of the symbol: `((T *)0xXXXXXXXX)[i]` (T = the extern's element type).

Capcom's main program addresses many tables through literal pointers. The machine code tells:
a literal base gives `lui at; addu at, <index>, at` (index first), a symbol base gives
`addu at, at, <index>` (main func_8004A5C8, the 0x800C3024 table). With k, only the k-th distinct
array is rewritten (for trybatch k = 0, 1, ...); without k all of them."""
import re
import sys

s = open(sys.argv[1]).read()
k = int(sys.argv[3]) if len(sys.argv) > 3 else None
arrays = re.findall(r"^extern ([\w ]+?) \*?(D_([0-9A-F]{8}))\[\w*\];", s, re.M)
arrays = [a for a in arrays if not a[0].startswith(("s32 (", "void ("))]
if k is not None:
    arrays = arrays[k:k + 1]
for typ, name, addr in arrays:
    if f"{name}[" not in s.split("{", 1)[-1]:
        continue
    head, body = s.split("{", 1)
    body = re.sub(rf"\b{name}\[", f"(({typ} *)0x{addr})[", body)
    s = head + "{" + body
# m2c's other shapes for scalar-declared symbols: ((T *)&D_X)[i], *(&D_X + i), *(i + &D_X)
scalars = re.findall(r"^extern ([\w ]+?) (D_([0-9A-F]{8}));", s, re.M)
if k is not None:
    scalars = [] if arrays else scalars[k:k + 1]
for typ, name, addr in scalars:
    if "{" not in s:
        break
    head, body = s.split("{", 1)
    body = re.sub(rf"\(\((\w+ \*)\)&{name}\)\[", rf"((\1)0x{addr})[", body)
    body = re.sub(rf"\*\(&{name} \+ ", f"*(({typ} *)0x{addr} + ", body)
    body = re.sub(rf"\+ &{name}\)", f"+ ({typ} *)0x{addr})", body)
    s = head + "{" + body
open(sys.argv[2], "w", newline="\n").write(s)
