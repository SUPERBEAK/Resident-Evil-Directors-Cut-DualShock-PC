"""scratchmixbatch.py [--target main] [--max-dist 60] [--max-addrs 5]: run scratchmix.py's
literal/struct sweep on every pending draft that uses scratchpad addresses and store
improvements like trybatch.py does (draft + results.json status). Run from dualshock/ through
buildenv.sh. 2^n compiles per draft (n = distinct scratchpad addresses), hence the limits."""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

ap = argparse.ArgumentParser()
ap.add_argument("--target", default="main")
ap.add_argument("--max-dist", type=int, default=60)
ap.add_argument("--max-addrs", type=int, default=5)
a = ap.parse_args()

rp = f"build/auto/{a.target}/results.json"
pending = set()
for cf in glob.glob(f"src/{a.target}/*.c"):
    pending |= set(re.findall(r'INCLUDE_ASM\("[^"]+", (\w+)\)', open(cf).read()))
pat = re.compile(r"\*\([A-Za-z_]\w*(?:\s*\*)*?\s*\*\)0x1F80([0-9A-F]{4})\b")
os.makedirs("build/trybatch", exist_ok=True)
rr = json.load(open(rp))
jobs = []
for n, st in sorted(rr.items()):
    if n not in pending or not st.startswith("diff:"):
        continue
    d = int(st[5:].split(",")[0])
    f = f"build/auto/{a.target}/c/{n}.c"
    if d > a.max_dist or not os.path.exists(f):
        continue
    addrs = set(pat.findall(open(f).read().split("{", 1)[-1]))
    if 0 < len(addrs) <= a.max_addrs:
        jobs.append((d, n, f))
print(f"{len(jobs)} drafts", flush=True)
better = 0
for d, n, f in sorted(jobs):
    out = f"build/trybatch/{a.target}_{n}_mix.c"
    r = subprocess.run([sys.executable, "tools/scratchmix.py", n, f, out, "--target", a.target],
                       capture_output=True, text=True).stdout
    m = re.search(r"best distance (\d+)", r)
    if not m:
        continue
    nd = int(m.group(1))
    if nd < d:
        better += 1
        print(f"{n}: diff:{d} -> {nd}", flush=True)
        open(f, "w", newline="\n").write(open(out).read())
        rr = json.load(open(rp))
        rr[n] = "match" if nd == 0 else f"diff:{nd}"
        json.dump(rr, open(rp, "w"), indent=1, sort_keys=True)
print(f"{better} improved", flush=True)
