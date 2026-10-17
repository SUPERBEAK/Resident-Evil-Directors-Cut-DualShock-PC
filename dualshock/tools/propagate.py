"""propagate.py - reuse matched C for identical copies of a function in other overlays.

Run from dualshock/ with the project venv python:
    python tools/propagate.py [--targets stage1,stage2,...] [--jobs 8]

The STAGE overlays share a lot of code (1,434 functions, ~577 distinct shapes). For every
function still pending in a target, this looks for a function with the same "shape" (asm
with names/addresses normalized away) that is already `match` in another target. The
matched C is then renamed - symbol references are paired up in order between the two asm
listings (jal func_X, %hi/%lo(D_Y), ...) - and verified at the copy's own address with
refine.py's linked distance check. Hits are saved as `match` in build/auto/<target>/.
Run tools/integrate.py --target <t> afterwards.
"""

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import autodecomp as ad  # noqa: E402
import compiler_matrix as cm  # noqa: E402
import refine  # noqa: E402

SYM = re.compile(r"\b(?:func|D|jtbl)_[0-9A-F]{8}\b|\.L[0-9A-F]{8}\b")
INS = re.compile(r"/\* [0-9A-F]+ [0-9A-F]{8} [0-9A-F]{8} \*/\s+(.*)")


def listing(func_dir, name):
    """(normalized instruction text list, ordered symbol references) of a function's asm."""
    ins, refs = [], []
    for line in open(os.path.join(func_dir, name + ".s")):
        m = INS.search(line)
        if not m:
            continue
        txt = re.sub(r"\s+", " ", m.group(1).strip())
        for s in SYM.findall(txt):
            if not s.startswith(".L"):
                refs.append(s)
        ins.append(SYM.sub("SYM", txt))
    return ins, refs


def shape(func_dir, name):
    ins, _ = listing(func_dir, name)
    return hashlib.sha1("\n".join(ins).encode()).hexdigest()


def rename(code, src_refs, dst_refs):
    if len(src_refs) != len(dst_refs):
        return None
    mapping = {}
    for a, b in zip(src_refs, dst_refs):
        if mapping.setdefault(a, b) != b:
            return None                       # inconsistent pairing
    out = code
    # two-phase replace so a->b, b->c chains don't collide
    for i, a in enumerate(mapping):
        out = re.sub(rf"\b{a}\b", f"@@{i}@@", out)
    for i, (a, b) in enumerate(mapping.items()):
        out = out.replace(f"@@{i}@@", b)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--targets", default=",".join(cm.OVERLAYS))
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    targets = args.targets.split(",")

    # index matched functions by shape
    donors = {}
    info = {}
    for t in targets:
        exe, base, func_dir, cfile = cm.target_info(t)
        res = json.load(open(os.path.join("build", "auto", t, "results.json")))
        info[t] = (func_dir, res)
        for n, s in res.items():
            if s == "match":
                donors.setdefault(shape(func_dir, n), (t, n))

    jobs = []
    for t in targets:
        func_dir, res = info[t]
        for n, s in res.items():
            if s != "match":
                d = donors.get(shape(func_dir, n))
                if d:
                    jobs.append((t, n, d))
    print(f"{len(donors)} matched shapes; {len(jobs)} pending functions have a matched twin")

    ctxs = {t: refine.Ctx(t) for t in {j[0] for j in jobs}}

    def work(job):
        t, n, (dt, dn) = job
        src_dir = info[dt][0]
        code = open(os.path.join("build", "auto", dt, "c", f"{dn}.c")).read()
        _, src_refs = listing(src_dir, dn)
        _, dst_refs = listing(info[t][0], n)
        new = rename(code, src_refs + [dn], dst_refs + [n])
        if new is None:
            return t, n, None, None
        d = refine.distance(ctxs[t], n, new, "prop")
        return t, n, d, new

    hits = 0
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        for t, n, d, new in ex.map(work, jobs):
            if d == 0:
                hits += 1
                info[t][1][n] = "match"
                ad.write_retry(os.path.join("build", "auto", t, "c", f"{n}.c"), new)
    for t in targets:
        json.dump(info[t][1], open(os.path.join("build", "auto", t, "results.json"), "w"),
                  indent=1, sort_keys=True)
    print(f"propagated: {hits} new matches")


if __name__ == "__main__":
    main()
