"""twins.py - find functions that are the same code in several programs (mostly the 7 stage overlays).

Run from dualshock/ with the project venv python (after `make split`):
    python tools/twins.py                 # summary
    python tools/twins.py --groups out.tsv  # every group: fingerprint id, program, function

Each function's assembly (asm/<program>/nonmatchings/<file>/<func>.s, written by splat) is reduced
to a fingerprint: mnemonics, registers and constants are kept; branch labels, symbol names
(func_/D_/jtbl_/named) and absolute addresses are replaced by placeholders. Functions with the
same fingerprint do the same thing (only the addresses they touch differ), so a name found for one
applies to all of them.
"""
import argparse
import collections
import glob
import os
import re

PROGRAMS = ["main", "logo", "prologue", "title", "select", "ending",
            "stage1", "stage2", "stage3", "stage4", "stage5", "stage6", "stage7"]
INSN = re.compile(r"^\s*/\* [0-9A-F]+ [0-9A-F]+ [0-9A-F]+ \*/\s+(\S+)\s*(.*)$")


def fingerprint(path):
    text = open(path).read()
    text = text.split(".section .text", 1)[-1]
    out = []
    for line in text.splitlines():
        m = INSN.match(line)
        if not m:
            continue
        ops = m.group(2)
        ops = re.sub(r"%(hi|lo|gp_rel)\([^)]*\)", r"%\1(SYM)", ops)
        ops = re.sub(r"\.L[0-9A-Fa-f_]+", "LBL", ops)
        ops = re.sub(r"\b(func|D|jtbl|jtgt)_[0-9A-F]{8}\b", "SYM", ops)
        if m.group(1) in ("jal", "j"):
            ops = "SYM"
        out.append(m.group(1) + " " + ops)
    return "\n".join(out)


def collect():
    funcs = {}
    for prog in PROGRAMS:
        for p in glob.glob(f"asm/{prog}/nonmatchings/*/*.s"):
            name = os.path.basename(p)[:-2]
            if name.startswith(("D_", "jtbl_")):
                continue
            fp = fingerprint(p)
            if fp:
                funcs[(prog, name)] = fp
    return funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--groups")
    a = ap.parse_args()
    funcs = collect()
    groups = collections.defaultdict(list)
    for key, fp in funcs.items():
        groups[fp].append(key)
    multi = [g for g in groups.values() if len(g) > 1]
    overlay_funcs = [k for k in funcs if k[0] != "main"]
    covered = {k for g in multi for k in g if k[0] != "main"}
    print(f"{len(funcs)} functions, {len(groups)} distinct; {len(multi)} groups with twins")
    print(f"overlay functions: {len(overlay_funcs)}; with a twin: {len(covered)}; "
          f"distinct overlay fingerprints: {len({funcs[k] for k in overlay_funcs})}")
    with_main = [g for g in multi if any(k[0] == "main" for k in g) and any(k[0] != "main" for k in g)]
    print(f"groups shared between main and an overlay: {len(with_main)}")
    if a.groups:
        with open(a.groups, "w", newline="\n") as f:
            for i, g in enumerate(sorted(groups.values(), key=lambda g: (-len(g), g))):
                for prog, name in sorted(g):
                    f.write(f"{i}\t{prog}\t{name}\t{len(g)}\n")
        print("groups written to", a.groups)


if __name__ == "__main__":
    main()
