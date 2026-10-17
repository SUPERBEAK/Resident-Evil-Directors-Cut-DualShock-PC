"""splitc.py - split one program's C file into several topic files, keeping the byte match.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python, after `make split`:
    python tools/splitc.py [--target main|stage1|...] plan.txt
    make split && make;  python tools/splitc.py [--target ...] --pad;  rebuild until no padding changes

plan.txt: one line per new file, in address order: `<first function> <file name without .c>`;
the first line must name the first function of the C file (main: src/main/capcom.c, an overlay:
src/<overlay>/<overlay>.c). For each new file the tool:
  - includes the old file's prelude (globals block, file-scope types) through a shared
    src/<program>/globals.h - every function saw all of it before, and still does,
  - adds file-scope prototypes of the functions defined in EARLIER files that it references (plus the
    file-scope types those prototypes use) - exactly what a function saw before (a definition earlier
    in the same file); functions defined later stay unseen, as before,
  - moves the INCLUDE_RODATA pieces with their functions (path asm/<program>/nonmatchings/<file>),
and rewrites the program's yaml: one `.rodata` and one `c` subsegment per file at the original offsets
(code: the first function's file offset; rodata: the first rodata item of the file's functions, from
splat's per-function .s files); config/rodata_pieces.txt folders are updated.
--pad: splat's linker script uses SUBALIGN(2), so each file's .rodata is packed right after the
previous one; where the original had alignment padding between two files' data, `.space N` is appended
to the earlier file. Main-program rule (tools/splitplan.py): a file with an 8-aligned jump table must
start its rodata 8-aligned. Overlay tables are 4-aligned (Makefile alignfix), so any boundary works.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402

RODATA_LINE = re.compile(r"^\s*/\* ([0-9A-F]+) ([0-9A-F]{8}) ")
PAD_TAG = "/* rodata padding up to the next file's original start (tools/splitc.py --pad) */"


class Prog:
    def __init__(self, target):
        self.target = target
        self.base = "capcom" if target == "main" else target         # the original single C file
        self.src = os.path.join("src", target, self.base + ".c")
        self.asmdir = os.path.join("asm", target, "nonmatchings", self.base)
        self.yaml = "slus_007.47.yaml" if target == "main" else f"{target}.yaml"
        self.map = os.path.join("build", ("slus_007.47" if target == "main" else target) + ".map")
        self.vram = int(re.search(r"vram: 0x([0-9A-Fa-f]+)", open(self.yaml).read()).group(1), 16)


P = None
ASMDIR = os.path.join("asm", "main", "nonmatchings", "capcom")     # (splitplan.py uses it for main)


def func_offsets(name):
    """(text file offset, first rodata file offset or None) from splat's <name>.s."""
    text = open(os.path.join(ASMDIR, name + ".s")).read()
    ro, tx = text.split(".section .text", 1) if ".section .text" in text else ("", text)
    t = re.search(r"^\s*/\* ([0-9A-F]+) [0-9A-F]{8} [0-9A-F]{8} \*/", tx, re.M)
    r = [int(m.group(1), 16) for m in (RODATA_LINE.match(l) for l in ro.splitlines()) if m]
    return int(t.group(1), 16), (min(r) if r else None)


def piece_offset(name):
    p = os.path.join(ASMDIR, name + ".s")
    if not os.path.isfile(p):
        return None
    r = [int(m.group(1), 16) for m in (RODATA_LINE.match(l) for l in open(p).read().splitlines()) if m]
    if r:
        return min(r)
    # pieces written by tools/rodata_pieces.py have no offset comments: 0x800 + addr - load address
    return int(name[2:], 16) - P.vram + 0x800


def split(plan_path):
    plan = [l.split() for l in open(plan_path) if l.strip() and not l.startswith("#")]
    text = open(P.src).read()
    lines = text.split("\n")
    spans = ig.function_spans(text)
    names = [n for _, _, n in spans]
    if plan[0][0] != names[0]:
        sys.exit(f"the plan must start with the first function, {names[0]}")
    starts = [names.index(f) for f, _ in plan]
    prelude = lines[:spans[0][0] - 1]
    chunks = []
    for k, (first, fname) in enumerate(plan):
        i0 = starts[k]
        i1 = starts[k + 1] if k + 1 < len(plan) else len(spans)
        # text between the previous chunk's last function and this chunk's first function belongs here
        a = spans[i0 - 1][1] if k else spans[0][0] - 1
        b = spans[i1 - 1][1] if k + 1 < len(plan) else len(lines)
        chunks.append((fname, i0, i1, lines[a:b]))
    defs = {}
    for s, e, n in spans:
        k, head = s - 1, lines[s - 1].rstrip()
        while not head.endswith("{"):          # a signature continued on the next line(s)
            k += 1
            head += " " + lines[k].strip()
        defs[n] = head[:-1].rstrip() + ";"
    # file-scope typedefs between functions (a prototype for a later file may need them)
    typedefs, inside = [], set()
    for s, e, _ in spans:
        inside.update(range(s - 1, e))
    i = len(prelude)
    while i < len(lines):
        if i not in inside and lines[i].startswith("typedef"):
            j, depth, block = i, 0, []
            while j < len(lines):
                block.append(lines[j])
                depth += lines[j].count("{") - lines[j].count("}")
                if depth <= 0 and lines[j].rstrip().endswith(";"):
                    break
                j += 1
            name = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*;\s*$", block[-1]).group(1)
            typedefs.append((i, name, block))
            i = j + 1
        else:
            i += 1
    includes = [l for l in prelude if l.startswith("#include")]
    shared = [l for l in prelude if not l.startswith("#include")]
    guard = f"{P.target.upper()}_GLOBALS_H"
    with open(os.path.join("src", P.target, "globals.h"), "w", newline="\n") as h:
        h.write(f"/* globals.h - declarations shared by all {P.target} C files (was the top of {P.base}.c;\n"
                "   tools/splitc.py). Per-function aliases with other types stay in the functions. */\n"
                f"#ifndef {guard}\n#define {guard}\n" + "\n".join(shared).strip("\n") + "\n\n#endif\n")
    old_inc = f'INCLUDE_RODATA("asm/{P.target}/nonmatchings/{P.base}"'
    yaml_ro, yaml_c, pieces_moved = [], [], {}
    for k, (fname, i0, i1, body) in enumerate(chunks):
        used = set(re.findall(r"\b[A-Za-z_]\w*\b", "\n".join(body)))
        protos = [defs[n] for n in names[:i0] if n in used]     # only what this file calls/references
        out = includes + ['#include "globals.h"']
        first_line = spans[i0][0] - 1
        need = [t for t in typedefs if t[0] < first_line and any(re.search(rf"\b{t[1]}\b", p) for p in protos)]
        if need:
            out += ["", "/* Types used by the prototypes below (defined in an earlier file) */"]
            for _, _, block in need:
                out += block
        if protos:
            out += ["", f"/* Functions defined in the files before this one (tools/splitc.py) - what these",
                    "   functions saw when everything was one file. Functions defined later stay unseen. */"]
            out += protos
        out += [""]
        body = [l.replace(old_inc, f'INCLUDE_RODATA("asm/{P.target}/nonmatchings/{fname}"') for l in body]
        for l in body:
            m = re.search(r'INCLUDE_RODATA\("[^"]+", (\w+)\)', l)
            if m:
                pieces_moved[m.group(1)] = fname
        offs = [func_offsets(n) for n in names[i0:i1]]
        ro = [r for _, r in offs if r is not None]
        ro += [o for o in (piece_offset(p) for p, f in pieces_moved.items() if f == fname) if o is not None]
        out += body
        open(os.path.join("src", P.target, fname + ".c"), "w", newline="\n").write("\n".join(out).rstrip("\n") + "\n")
        yaml_c.append((offs[0][0], fname))
        if ro:
            yaml_ro.append((min(ro), fname))
    os.remove(P.src)
    y = open(P.yaml).read()
    ro_lines = "".join(f"      - [0x{o:X}, .rodata, {f}]\n" for o, f in sorted(yaml_ro))
    c_lines = "".join(f"      - [0x{o:X}, c, {f}]\n" for o, f in sorted(yaml_c))
    y, n1 = re.subn(r"      - \[0x[0-9A-F]+, \.rodata, " + P.base + r"\][^\n]*\n", lambda m: ro_lines, y, count=1)
    y, n2 = re.subn(r"      - \[0x[0-9A-F]+, c, " + P.base + r"\][^\n]*\n", lambda m: c_lines, y, count=1)
    assert n1 == 1 and n2 == 1, "yaml subsegments not found"
    open(P.yaml, "w", newline="\n").write(y)
    cfg_path = os.path.join("config", "rodata_pieces.txt")  # gone since every piece is C (2026-10-16)
    if os.path.exists(cfg_path):
        cfg = open(cfg_path).read()
        for p, f in pieces_moved.items():
            cfg = cfg.replace(f"asm/{P.target}/nonmatchings/{P.base} {p} ", f"asm/{P.target}/nonmatchings/{f} {p} ")
        open(cfg_path, "w", newline="\n").write(cfg)
    print(f"{len(chunks)} files; rodata subsegments {len(yaml_ro)}; pieces moved {len(pieces_moved)}")


def pad():
    y = open(P.yaml).read()
    sub = re.findall(r"- \[0x([0-9A-F]+), (\.?\w+), (\w+)\]", y)
    starts, after = [], None
    for k, (o, kind, name) in enumerate(sub):
        if kind == ".rodata":
            starts.append((int(o, 16), name))
            after = int(sub[k + 1][0], 16) if k + 1 < len(sub) else None
    starts.append((after, None))                 # the next subsegment ends the last file's rodata
    m = open(P.map).read()
    built = {f: (int(a, 16) - P.vram + 0x800, int(sz, 16)) for a, sz, f in
             re.findall(r"^ \.rodata\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) build/src/" + P.target + r"/(\w+)\.o", m, re.M)}
    changed = 0
    for (o, f), (o_next, _) in zip(starts, starts[1:]):
        b_off, b_size = built[f]
        if b_off != o:
            print(f"{f}: built rodata at 0x{b_off:X}, original 0x{o:X} - fix earlier files first")
        need = o_next - (o + b_size)
        path = os.path.join("src", P.target, f + ".c")
        t = open(path).read()
        cur = re.search(re.escape(PAD_TAG) + r'\n__asm__\("\.section \.rodata\\n\\t\.space (\d+)\\n\\t\.text"\);\n', t)
        have = int(cur.group(1)) if cur else 0
        if need == 0:
            continue
        total = have + need
        if cur:
            t = t[:cur.start()] + t[cur.end():]
        if total > 0:
            t = t.rstrip("\n") + "\n\n" + PAD_TAG + f'\n__asm__(".section .rodata\\n\\t.space {total}\\n\\t.text");\n'
        open(path, "w", newline="\n").write(t)
        print(f"{f}: rodata padding {have} -> {total} bytes")
        changed += 1
    print("padding changes:", changed)


def main():
    global P, ASMDIR
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--pad", action="store_true")
    ap.add_argument("plan", nargs="?")
    a = ap.parse_args()
    P = Prog(a.target)
    ASMDIR = P.asmdir
    if a.pad:
        pad()
    else:
        split(a.plan)


if __name__ == "__main__":
    main()
