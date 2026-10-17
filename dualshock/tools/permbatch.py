"""permbatch.py - run decomp-permuter on many drafts, one after another, with a time limit each.

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/permbatch.py --target main [--only f ...] [--max-dist 3] [--seconds 120] [--jobs 6]

Without --only: the pending drafts with status diff:N, N <= --max-dist (closest first). Per
function: permsetup.py setup, then the permuter (stops at score 0 or after --seconds), then
every zero-score output is checked byte for byte (permsetup.check) and the first exact one is
exported as a `match` draft. Integrate afterwards with tools/integrate.py --target <target>.
The permuter's own preprocessing calls `cpp`: our GCC 2.6.3 cpp is put on PATH for it.
"""

import argparse
import glob
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compiler_matrix as cm  # noqa: E402
import permsetup  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--max-dist", type=int, default=3)
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--jobs", type=int, default=6)
    a = ap.parse_args()
    exe, base, fd, cfile = cm.target_info(a.target)
    fd = fd.replace(os.sep, "/")
    src = open(cfile).read()
    res = json.load(open(os.path.join("build", "auto", a.target, "results.json")))
    if a.only:
        names = a.only
    else:
        c = [(int(s.split(":")[1].split(",")[0]), n) for n, s in res.items()
             if s.startswith("diff:") and int(s.split(":")[1].split(",")[0]) <= a.max_dist
             and f'INCLUDE_ASM("{fd}", {n});' in src]
        names = [n for _, n in sorted(c)]
    env = dict(os.environ)
    env["PATH"] = os.path.join(cm.TOOLS, "old-gcc-build", "gcc-2.6.3-psx") + os.pathsep + env["PATH"]
    found = []
    for i, n in enumerate(names):
        try:
            d = permsetup.setup(a.target, n, None)
        except SystemExit as e:
            print(f"[{i + 1}/{len(names)}] {n}: setup failed ({e})")
            continue
        for old in glob.glob(os.path.join(d, "output-*")):
            import shutil
            shutil.rmtree(old, ignore_errors=True)
        p = subprocess.Popen([cm.PY, os.path.join(permsetup.PERMUTER, "permuter.py"), d, "-j", str(a.jobs),
                              "--stop-on-zero", "--quiet"], env=env,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            p.wait(timeout=a.seconds)
        except subprocess.TimeoutExpired:
            # kill the whole tree: the permuter's multiprocessing workers survive a plain kill
            # of the parent and keep using the CPU (270 orphans piled up, 2026-10-13)
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            p.wait()
        zero = sorted(glob.glob(os.path.join(d, "output-0-*", "source.c")))
        ok = None
        for z in zero:
            if permsetup.check(a.target, n, z) == 0:
                ok = z
                break
        if ok:
            permsetup.export(a.target, n, ok)
            found.append(n)
        best = sorted(glob.glob(os.path.join(d, "output-*")),
                      key=lambda p: int(os.path.basename(p).split("-")[1]))
        print(f"[{i + 1}/{len(names)}] {n}: {'MATCH' if ok else 'best ' + (os.path.basename(best[0]) if best else '-')}",
              flush=True)
    print(f"{a.target}: {len(found)} matches: {' '.join(found)}")


if __name__ == "__main__":
    main()
