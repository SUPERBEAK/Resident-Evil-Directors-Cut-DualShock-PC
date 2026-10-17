"""rodatasplit.py <func> [--target main]: move the rodata blocks at the top of a function's .s
(strings/tables splat put before `.section .text`) into their own D_xxxxxxxx.s files and print
the INCLUDE_RODATA lines that must precede the C function. Needed when the function's C code
references those symbols as externs: once the INCLUDE_ASM is replaced, the function's .s (which
defined them) is no longer assembled (main func_800195E8: undefined reference to D_80010128).
The jump tables (jtbl_*) are left alone - the compiler emits its own."""
import argparse
import os
import re

ap = argparse.ArgumentParser()
ap.add_argument("func")
ap.add_argument("--target", default="main")
a = ap.parse_args()
d = f"asm/{a.target}/nonmatchings/capcom" if a.target == "main" else None
path = os.path.join(d, a.func + ".s")
text = open(path).read()
head = text.split(".section .text", 1)[0]
lines = []
for m in re.finditer(r"nonmatching (D_[0-9A-F]{8})\n(.*?)enddlabel \1\n", head, re.S):
    name = m.group(1)
    out = os.path.join(d, name + ".s")
    if not os.path.exists(out):
        open(out, "w", newline="\n").write(f".section .rodata\n\nnonmatching {name}\n{m.group(2)}enddlabel {name}\n")
    lines.append(f'INCLUDE_RODATA("asm/{a.target}/nonmatchings/capcom", {name});')
print("\n".join(lines))
