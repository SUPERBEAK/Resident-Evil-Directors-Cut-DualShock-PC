"""pctablematch.py - match the RE1 PC decomp's function tables to our overlays' tables by shape.

Run from dualshock/ with the project venv python:
    python tools/pctablematch.py [--min 4] [--out ../temp/pctables.tsv]

Reads the PC decomp's `static void* const NAME[N] = { (void*)func, NULL, ... }` tables (names only -
no code is used) and every function-pointer table in our overlays' data (splat dlabel blocks of
`.word` entries). A table's SHAPE is its length, where the NULLs are and which entries repeat (each
entry -> index of its first occurrence). Equal shapes (length >= --min, at least 2 distinct entries)
are candidate matches; each match proposes a name per our function. Candidates must be checked
against the code before use (tools/nametwins.py applies names).
"""
import argparse
import glob
import os
import re

# Optional reference: a clone of the RE1 PC decompilation (the resident-evil-pc-decomp project);
# set RE1_PC_DECOMP to its folder, default ../reference/resident-evil-pc-decomp.
PC = os.path.join(os.environ.get("RE1_PC_DECOMP", os.path.join("..", "reference", "resident-evil-pc-decomp")), "src")
OVERLAYS = [f"stage{i}" for i in range(1, 8)]


def shape(entries):
    first = {}
    out = []
    for e in entries:
        if e is None:
            out.append(-1)
        else:
            first.setdefault(e, len(first))
            out.append(first[e])
    return tuple(out)


def pc_tables():
    tabs = []
    for p in glob.glob(os.path.join(PC, "**", "*.cpp"), recursive=True):
        t = open(p, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"void\s*\*\s*(?:const\s+)?(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{(.*?)\};", t, re.S):
            body = re.sub(r"//[^\n]*", "", m.group(2))
            ents = []
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if item in ("NULL", "0", "nullptr"):
                    ents.append(None)
                    continue
                f = re.search(r"(\w+)\s*\)?\s*$", item)
                ents.append(f.group(1) if f else None)
            if len(ents) >= 2:
                tabs.append((os.path.basename(p), m.group(1), ents))
    return tabs


def our_tables(prog):
    text = "".join(open(p).read() for p in glob.glob(f"asm/{prog}/data/*.s"))
    tabs = []
    for name, body in re.findall(r"dlabel (\w+)\n(.*?)enddlabel", text, re.S):
        words = re.findall(r"\.word (\S+)", body)
        if len(words) < 2:
            continue
        ents = []
        ok = True
        for w in words:
            if w in ("0x00000000", "0"):
                ents.append(None)
            elif re.match(r"^[A-Za-z_]\w*$", w) and not w.startswith(("D_", "jtbl_", ".L")):
                ents.append(w)
            else:
                ok = False
                break
        if ok and any(ents):
            tabs.append((name, ents))
    return tabs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=int, default=4)
    ap.add_argument("--out", default=os.path.join("..", "temp", "pctables.tsv"))
    a = ap.parse_args()
    pcs = [(f, n, e, shape(e)) for f, n, e in pc_tables()]
    lines = []
    for prog in OVERLAYS:
        for tname, ents in our_tables(prog):
            sh = shape(ents)
            if len(ents) < a.min or len(set(x for x in sh if x >= 0)) < 2:
                continue
            for f, pn, pe, psh in pcs:
                if psh == sh:
                    for ours, theirs in zip(ents, pe):
                        if ours and theirs:
                            lines.append(f"{prog}\t{tname}\t{f}:{pn}\t{ours}\t{theirs}")
    open(a.out, "w", newline="\n").write("\n".join(lines) + "\n")
    tables = {(l.split('\t')[0], l.split('\t')[1], l.split('\t')[2]) for l in lines}
    print(f"{len(pcs)} PC tables; {len(tables)} (overlay table, PC table) matches; {len(lines)} name proposals -> {a.out}")


if __name__ == "__main__":
    main()
