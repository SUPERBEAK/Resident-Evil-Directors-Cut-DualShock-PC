"""twinhunt.py [--min 0.85] [--targets stage1,...]: for every pending overlay function, find matched
functions in other overlays whose normalized asm is similar (difflib ratio >= --min), port their C
with port.py's symbol pairing and measure the distance with refine.py. Prints the best candidates;
with --apply, stores improvements in build/auto/<target>/ (status match or diff:N).
propagate.py only handles exact twins; this catches copies that differ slightly
(stage5 func_8011D6BC <- stage3 func_80126040)."""
import argparse
import difflib
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refine  # noqa: E402

INS = re.compile(r"/\* [0-9A-F]+ [0-9A-F]{8} [0-9A-F]{8} \*/\s+(.*)")
NORM = re.compile(r"\b(?:func|D|jtbl)_[0-9A-F]{8}\b|\.L[0-9A-F]{8}\b|0x[0-9A-Fa-f]+|-?\d+")


def shape(target, func):
    p = os.path.join("asm", target, "nonmatchings", target, f"{func}.s")
    if not os.path.exists(p):
        return None
    out = []
    for line in open(p):
        m = INS.search(line)
        if m:
            out.append(NORM.sub("#", m.group(1)).split()[0] + " " + NORM.sub("#", m.group(1)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=float, default=0.85)
    ap.add_argument("--targets", default="stage1,stage2,stage3,stage4,stage5,stage6,stage7")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()
    targets = args.targets.split(",")
    status = {t: json.load(open(f"build/auto/{t}/results.json")) for t in targets}
    src_asm = {}
    for t in targets:
        text = "".join(open(p).read() for p in glob.glob(f"src/{t}/*.c"))
        src_asm[t] = set(re.findall(r"INCLUDE_ASM\([^,]+, (func_[0-9A-F]+)\)", text))
    matched = [(t, f) for t in targets for f, s in status[t].items() if s == "match"]
    mshape = {}
    for t, f in matched:
        sh = shape(t, f)
        if sh:
            mshape[(t, f)] = sh
    for t in targets:
        ctx = None
        for f in sorted(src_asm[t]):
            st = status[t].get(f, "")
            cur = int(st[5:]) if st.startswith("diff:") else 10 ** 6
            sh = shape(t, f)
            if not sh:
                continue
            cands = []
            for (mt, mf), msh in mshape.items():
                if mt == t or abs(len(msh) - len(sh)) > max(4, len(sh) // 20):
                    continue
                r = difflib.SequenceMatcher(None, sh, msh, autojunk=False).ratio()
                if r >= args.min:
                    cands.append((r, mt, mf))
            cands.sort(reverse=True)
            for r, mt, mf in cands[:2]:
                out = os.path.join("..", "temp", "dc", "hand", f"twin_{t}_{f}.c")
                import subprocess
                subprocess.run([sys.executable, os.path.join("tools", "port.py"), mt, mf, t, f, out],
                               capture_output=True)
                if ctx is None:
                    ctx = refine.Ctx(t)
                code = open(out).read()
                d = refine.distance(ctx, f, code, "twin")
                print(f"{t} {f} ({st}) <- {mt} {mf} ratio {r:.2f}: distance {d}", flush=True)
                if args.apply and d is not None and d < cur:
                    os.replace(out, os.path.join("build", "auto", t, "c", f"{f}.c"))
                    status[t][f] = "match" if d == 0 else f"diff:{d}"
                    json.dump(status[t], open(f"build/auto/{t}/results.json", "w"), indent=1)
                    cur = d
                if d == 0:
                    break


if __name__ == "__main__":
    main()
