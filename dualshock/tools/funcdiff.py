"""funcdiff.py - compare built functions against the original executable.

Run from dualshock/ with the project venv python:
    python tools/funcdiff.py [--target slus_007.47] [func_name ...]

Without names: lists every function whose bytes differ from the original (in address
order; a size change shifts everything after it, so fix the FIRST one first).
With names: shows a side-by-side disassembly (original | built) for those functions.

Functions come from the built ELF's symbol table (st_size from glabel/endlabel or the
compiler). The original bytes are read from the extracted executable at the same VRAM.
"""

import argparse
import re
import os
import struct
import subprocess
import sys

TOOLS = os.path.join("..", "tools", "external")
BIN = os.path.join(TOOLS, "mips-toolchain", "bin")
TARGETS = {
    "slus_007.47": "../extracted/RE1-Dualshock/SLUS_007.47",
    **{o: f"../extracted/RE1-Dualshock/PSXUSA/PROG2/{o.upper()}.EXE" for o in
       ["logo", "prologue", "title", "select", "ending"] + [f"stage{i}" for i in range(1, 8)]},
}


def load_exe(path):
    d = open(path, "rb").read()
    t_addr, t_size = struct.unpack_from("<II", d, 0x18)
    return t_addr, d[0x800:0x800 + t_size]


def elf_funcs(elf):
    out = subprocess.run([os.path.join(BIN, "mips64-elf-nm"), "-S", "--defined-only", elf],
                         capture_output=True, text=True, check=True).stdout
    funcs = {}
    for line in out.splitlines():
        parts = line.split()
        # Only real functions (jtbl_/D_ labels in .rodata also show up as 't' in nm).
        if (len(parts) == 4 and parts[2] in "Tt" and not parts[3].endswith(".NON_MATCHING")
                and not parts[3].startswith(("jtbl_", "D_", ".L"))):
            funcs[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    return funcs


def disasm(raw, vram):
    tmp = os.path.join("build", "_funcdiff.bin")
    open(tmp, "wb").write(raw)
    out = subprocess.run([os.path.join(BIN, "mips64-elf-objdump"), "-D", "-b", "binary",
                          "-m", "mips:3000", "-EL", "-M", "reg-names=32",
                          f"--adjust-vma=0x{vram:X}", tmp],
                         capture_output=True, text=True, check=True).stdout
    lines = [l.split("\t", 2) for l in out.splitlines() if re.match(r"^\s*[0-9a-f]+:\t", l)]
    return [f"{l[2] if len(l) > 2 else ''}".rstrip() for l in lines]


def orig_addr_size(name, target, built_size):
    """Original VRAM/size of a splat-named function (func_XXXXXXXX), else None."""
    m = re.fullmatch(r"func_([0-9A-F]{8})", name)
    if not m:
        return None
    size = built_size
    for root, _, files in os.walk(os.path.join("asm", "main" if target == "slus_007.47" else target)):
        if name + ".s" in files:
            # Not necessarily the first line: migrated jump tables (.rodata) can come first.
            sm = re.search(rf"^nonmatching {name},\s*(0x[0-9A-Fa-f]+)",
                           open(os.path.join(root, name + ".s")).read(), re.M)
            if sm:
                size = int(sm.group(1), 16)
            break
    return int(m.group(1), 16), size


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="slus_007.47", choices=sorted(TARGETS))
    ap.add_argument("funcs", nargs="*")
    args = ap.parse_args()

    base, orig = load_exe(TARGETS[args.target])
    built_path = os.path.join("build", "SLUS_007.47" if args.target == "slus_007.47"
                              else f"{args.target.upper()}.EXE")
    _, built = load_exe(built_path)
    funcs = elf_funcs(os.path.join("build", f"{args.target}.elf"))

    if not args.funcs:
        ordered = sorted(funcs.items(), key=lambda kv: kv[1][0])
        # 1) First function that moved: the one before it changed size.
        prev = None
        for name, (addr, size) in ordered:
            oa = orig_addr_size(name, args.target, size)
            if oa and oa[0] != addr:
                print(f"SHIFT: {name} is at 0x{addr:08X} instead of 0x{oa[0]:08X} "
                      f"({addr - oa[0]:+d} bytes); size changed in or before: {prev}")
                break
            prev = name
        # 2) Byte differences at the same address.
        bad = []
        for name, (addr, size) in ordered:
            o = addr - base
            if orig[o:o + size] != built[o:o + size]:
                bad.append((addr, size, name))
        print(f"{len(bad)} of {len(funcs)} functions differ")
        for addr, size, name in bad[:20]:
            print(f"  0x{addr:08X} {size:#6x} {name}")
        return 1 if bad else 0

    for name in args.funcs:
        if name not in funcs:
            sys.exit(f"{name}: not in built ELF")
        addr, size = funcs[name]
        oaddr, osize = orig_addr_size(name, args.target, size) or (addr, size)
        a = disasm(orig[oaddr - base:oaddr - base + osize], oaddr)
        b = disasm(built[addr - base:addr - base + size], addr)
        print(f"=== {name}: original 0x{oaddr:08X} size {osize:#x} | built 0x{addr:08X} size {size:#x}")
        for i in range(max(len(a), len(b))):
            l, r = (a[i] if i < len(a) else ""), (b[i] if i < len(b) else "")
            print(f"{'  ' if l == r else '!='} {l:<40} | {r}")


if __name__ == "__main__":
    sys.exit(main())
