"""tableprop.py - name overlay functions through dispatch tables of already-named functions.

Run from dualshock/ with the project venv python:
    python tools/tableprop.py [--apply]

For every NAMED stage-overlay function F that calls through a table indexed by an entity field,
`D_X[g_CurrentEntity->action_behavior]()`, the RE1 PC decomp's function F (same name - names only,
no code is used) is looked up; if it dispatches through a table on the SAME field,
`((void(*)())NAME[ENTITY->action_behavior])()`, the two tables are paired by position. A pairing is
used only when it is consistent (one of our functions never gets two different PC names). Without
--apply the proposals are printed; with --apply they are renamed per overlay (tools/rename.py,
verified by a full build) and the search repeats until nothing new is found.
"""
import argparse
import collections
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402

# Optional reference: a clone of the RE1 PC decompilation; set RE1_PC_DECOMP to its folder,
# default ../reference/resident-evil-pc-decomp.
PCDIR = os.path.join(os.environ.get("RE1_PC_DECOMP", os.path.join("..", "reference", "resident-evil-pc-decomp")),
                     "src", "game")
STAGES = [f"stage{i}" for i in range(1, 8)]
OUR_DISPATCH = re.compile(r"(D_[0-9A-F]{8})\[\s*\(?\s*g_CurrentEntity->(\w+)[^\]]*\]\s*\(\s*\)")
PC_DISPATCH = re.compile(r"(\w+)\s*\[\s*\(?\s*ENTITY\s*->\s*(\w+)[^\]]*\]\s*\)?\s*\(\s*\)")


def pc_sources():
    out = {}
    for p in glob.glob(os.path.join(PCDIR, "**", "*.cpp"), recursive=True):
        out[p] = open(p, encoding="utf-8", errors="replace").read()
    return out


def pc_function_body(srcs, name):
    for t in srcs.values():
        m = re.search(r"\n[\w\s\*]*\b" + re.escape(name) + r"\s*\(\s*(?:void)?\s*\)\s*\{", t)
        if m:
            i, depth = m.end(), 1
            while i < len(t) and depth:
                depth += {"{": 1, "}": -1}.get(t[i], 0)
                i += 1
            return t[m.end():i]
    return None


def pc_table(srcs, name):
    for t in srcs.values():
        m = re.search(r"\b" + re.escape(name) + r"\s*\[\s*\w*\s*\]\s*\)?\s*(?:\([^)]*\))?\s*=\s*\{(.*?)\};", t, re.S)
        if m:
            body = re.sub(r"//[^\n]*", "", m.group(1))
            ents = []
            for item in [x.strip() for x in body.split(",") if x.strip()]:
                if item in ("NULL", "0", "nullptr"):
                    ents.append(None)
                else:
                    f = re.search(r"(\w+)\s*\)?\s*$", item)
                    ents.append(f.group(1) if f else None)
            return ents
    return None


def our_table(prog, label):
    text = "".join(open(p).read() for p in glob.glob(f"asm/{prog}/data/*.s"))
    blk = re.search(r"dlabel " + re.escape(label) + r"\n(.*?)enddlabel", text, re.S)
    return [None if w == "0x00000000" else w for w in re.findall(r"\.word (\S+)", blk.group(1))] if blk else []


def proposals(srcs):
    props = collections.defaultdict(dict)       # prog -> {our func: pc name}
    notes = []
    for prog in STAGES:
        txt = open(f"src/{prog}/{prog}.c").read()
        lines = txt.split("\n")
        for s, e, fname in ig.function_spans(txt):
            if fname.startswith("func_"):
                continue
            ours = {(field): tbl for tbl, field in OUR_DISPATCH.findall("\n".join(lines[s:e]))}
            if not ours:
                continue
            body = pc_function_body(srcs, fname)
            if body is None:
                continue
            theirs = {field: tbl for tbl, field in PC_DISPATCH.findall(body)}
            for field, tbl in ours.items():
                if field not in theirs:
                    continue
                o, t = our_table(prog, tbl), pc_table(srcs, theirs[field])
                if not o or not t:
                    continue
                pair, bad = {}, False
                for a, b in zip(o, t):
                    if a and b:
                        if a in pair and pair[a] != b:
                            bad = True
                        pair.setdefault(a, b)
                if bad:
                    notes.append(f"# {prog} {fname}: {tbl} vs {theirs[field]} inconsistent - skipped")
                    continue
                for a, b in pair.items():
                    if a.startswith("func_"):
                        props[prog][a] = (b, f"{theirs[field]} via {fname} (.{field})")
    return props, notes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    srcs = pc_sources()
    for round_no in range(1, 10):
        props, notes = proposals(srcs)
        for n in notes:
            print(n)
        total = sum(len(v) for v in props.values())
        print(f"round {round_no}: {total} proposals")
        if not total or not a.apply:
            for prog, d in props.items():
                for f, (n, why) in sorted(d.items()):
                    print(f"{prog} {f} = {n}; // {why}")
            return
        for prog, d in props.items():
            names = collections.Counter()
            lines = []
            for f, (n, why) in sorted(d.items()):
                names[n] += 1
                nn = n if names[n] == 1 else f"{n}_{names[n]}"
                lines.append(f"{f} = {nn}; // {why}")
            batch = os.path.join("..", "temp", f"tableprop_{prog}.txt")
            open(batch, "w", newline="\n").write("\n".join(lines) + "\n")
            r = subprocess.run([sys.executable, os.path.join("tools", "rename.py"), "--target", prog, "--file", batch],
                               capture_output=True, text=True)
            res = [l for l in (r.stdout + r.stderr).splitlines() if "renamed," in l or "OK" in l or "FAILED" in l]
            print(f"  {prog}: {' | '.join(res)}")
            with open(os.path.join("config", "names_overlays.txt"), "a", newline="\n") as fo:
                fo.write("\n".join(f"{prog} {l}" for l in lines) + "\n")


if __name__ == "__main__":
    main()
