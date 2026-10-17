"""integrate_retry.py - retry reverted functions one at a time.

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/integrate_retry.py --target stage3

integrate.py integrates all `match` drafts of a target together; when the build mismatches, its
culprit search sometimes reverts innocent functions too (status `match,integrate-fail`). This
retries each of those alone (`integrate.py --only f`); real failures stay marked.
"""

import argparse
import json
import os
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", required=True)
    args = ap.parse_args()
    rp = os.path.join("build", "auto", args.target, "results.json")
    cfile = (os.path.join("src", "main", "capcom.c") if args.target == "main"
             else os.path.join("src", args.target, f"{args.target}.c"))
    src = open(cfile).read()
    # reverted ones, and plain `match` drafts left out when a whole batch was restored
    names = sorted(n for n, s in json.load(open(rp)).items()
                   if s == "match,integrate-fail" or (s == "match" and f", {n});" in src))
    ok = 0
    for n in names:
        r = json.load(open(rp))
        r[n] = "match"
        json.dump(r, open(rp, "w"), indent=1, sort_keys=True)
        if f", {n});" not in open(cfile).read():       # already in the C file (stale status)
            print(f"{n}: already integrated (status corrected)", flush=True)
            continue
        out = subprocess.run([sys.executable, "tools/integrate.py", "--target", args.target, "--only", n],
                             capture_output=True, text=True).stdout
        done = "OK: 1 functions integrated" in out
        ok += done
        print(f"{n}: {'integrated' if done else 'still fails'}", flush=True)
    print(f"{args.target}: {ok} of {len(names)} reverted functions integrated on retry")


if __name__ == "__main__":
    main()
