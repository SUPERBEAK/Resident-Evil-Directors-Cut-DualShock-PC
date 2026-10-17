"""permharvest.py [--target main] [--top 3]: collect decomp-permuter near-misses.

permbatch.py only exports exact matches; a run that ends at a low score leaves its best sources in
build/permuter/<target>/<func>/output-<score>-<n>/. For every still-pending function with such
outputs, the --top lowest-scoring ones are turned into drafts (permsetup.draft_text) and measured
with refine.distance; one that beats the stored draft replaces it (results.json diff:N / match).
A permuter score is not our distance - main func_8003FC6C: score 5 = distance 1, stored draft 5.
Drafts whose stored version has its own `typedef` are skipped (permuter exports drop typedefs).
Run from dualshock/ through buildenv.sh."""
import argparse
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import permsetup  # noqa: E402
import refine  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--top", type=int, default=3)
    a = ap.parse_args()
    ctx = refine.Ctx(a.target)
    rp = f"build/auto/{a.target}/results.json"
    pending = set()
    for cf in glob.glob(f"src/{a.target}/*.c"):
        pending |= set(re.findall(r'INCLUDE_ASM\("[^"]+", (\w+)\)', open(cf).read()))
    better = 0
    for d in sorted(glob.glob(f"build/permuter/{a.target}/func_*")):
        name = os.path.basename(d)
        if name not in pending:
            continue
        draft = f"build/auto/{a.target}/c/{name}.c"
        st = json.load(open(rp)).get(name, "")
        if not st.startswith("diff:") or not os.path.exists(draft) or "typedef" in open(draft).read():
            continue
        cur = int(st[5:].split(",")[0])
        outs = sorted(glob.glob(os.path.join(d, "output-*", "source.c")),
                      key=lambda p: int(os.path.basename(os.path.dirname(p)).split("-")[1]))
        best = None
        for src in outs[:a.top]:
            try:
                text = permsetup.draft_text(name, src)
            except SystemExit:
                continue
            dist = refine.distance(ctx, name, text, "harvest")
            if dist is not None and dist < cur and (best is None or dist < best[0]):
                best = (dist, text, src)
        if best:
            better += 1
            print(f"{name}: diff:{cur} -> {best[0]} ({os.path.relpath(best[2])})", flush=True)
            open(draft, "w", newline="\n").write(best[1])
            rr = json.load(open(rp))
            rr[name] = "match" if best[0] == 0 else f"diff:{best[0]}"
            json.dump(rr, open(rp, "w"), indent=1, sort_keys=True)
    print(f"{better} improved", flush=True)


if __name__ == "__main__":
    main()
