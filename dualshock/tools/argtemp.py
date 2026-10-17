"""argtemp.py <in.c> <out.c>: when the original reuses an argument register ($a0/$a1) for a
temporary, m2c invents a parameter and uses it as a temp: `void f(u32 arg1) { ...
arg1 = g(...) >> 0xA; x86 += arg1 & 1; }`. Inline the value into its single use and drop the
parameter (stage5 func_801114E4, func_801116C4). Only for a parameter whose first appearance in
the body is that assignment. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
sig = re.search(r"^(\w[\w \*]*?\b(func_[0-9A-F]{8}))\(([^()]*)\) \{$", s, re.M)
if sig:
    params = [p.strip() for p in sig.group(3).split(",") if p.strip() and p.strip() != "void"]
    for p in params:
        name = p.split()[-1].lstrip("*")
        body = s[sig.end():]
        uses = [m.start() for m in re.finditer(rf"\b{name}\b", body)]
        a = re.search(rf"\n([ \t]*){name} = ([^;\n]+);\n", body)
        if not a or len(uses) != 2 or uses[0] != a.start() + 1 + len(a.group(1)):
            continue
        expr = a.group(2)
        body = body.replace(a.group(0), "\n", 1)
        body = re.sub(rf"\b{name}\b", lambda _: f"({expr})", body, count=1)
        rest = [q for q in params if q != p]
        params = rest
        s = s[:sig.start()] + f"{sig.group(1)}({', '.join(rest) if rest else 'void'}) {{" + body
        sig = re.search(r"^(\w[\w \*]*?\b(func_[0-9A-F]{8}))\(([^()]*)\) \{$", s, re.M)
open(sys.argv[2], "w", newline="\n").write(s)
