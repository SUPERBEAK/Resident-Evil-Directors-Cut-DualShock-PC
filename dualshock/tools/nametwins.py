"""nametwins.py - name a function and all its twins (the same code in other overlays) in one go.

Run from dualshock/ with the project venv python:
    python tools/nametwins.py --file names.txt [--dry-run]

names.txt lines:  `<program> <current name> = <new name>; // evidence`
e.g.              `stage1 func_80106048 = enemy_update_common; // ...`
Every function with the same fingerprint as the given one (tools/twins.py) in any overlay gets the
same name; each overlay is renamed with `tools/rename.py --target <overlay>` (verified by a full
split + rebuild there). Names stay per overlay (each overlay links on its own), so the same name in
stage1..stage7 is fine. The expanded list is kept as config/names_overlays.txt (appended).
"""
import argparse
import collections
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import twins  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    funcs = twins.collect()
    by_fp = collections.defaultdict(list)
    for key, fp in funcs.items():
        by_fp[fp].append(key)
    per_target = collections.defaultdict(list)
    log = []
    for line in open(a.file):
        line = line.strip()
        if not line or line.startswith(("#", "//")):
            continue
        head, _, comment = line.partition("//")
        prog_cur, _, new = head.partition("=")
        prog, cur = prog_cur.split()
        new = new.strip().rstrip(";").strip()
        if (prog, cur) not in funcs:
            print(f"SKIPPED {prog} {cur}: no such function (already renamed?)")
            continue
        group = [k for k in by_fp[funcs[(prog, cur)]] if k[0] != "main"]
        # Only spread a name when the code is UNIQUE in every program of the group. Tiny generic
        # functions (empty, one-call thunks, `table[x]()` dispatchers) share a fingerprint with
        # unrelated functions; spreading those named title_nop `zombie_no_action` (2026-10-16).
        per_prog = collections.Counter(p for p, _ in group)
        if any(c > 1 for c in per_prog.values()):
            group = [(prog, cur)]
        for p, f in group:
            per_target[p].append((f, new, comment.strip()))
            log.append(f"{p} {f} = {new}; // {comment.strip()}" + ("" if (p, f) == (prog, cur) else f" (twin of {prog} {cur})"))
        print(f"{prog} {cur} -> {new}: {len(group)} function(s) in {', '.join(sorted({p for p, _ in group}))}")
    if a.dry_run:
        return
    ok_all = True
    for prog, items in sorted(per_target.items()):
        batch = os.path.join("..", "temp", f"nametwins_{prog}.txt")
        with open(batch, "w", newline="\n") as f:
            seen = set()
            for cur, new, _ in items:
                if cur not in seen:
                    seen.add(cur)
                    f.write(f"{cur} = {new};\n")
        r = subprocess.run([sys.executable, os.path.join("tools", "rename.py"), "--target", prog, "--file", batch],
                           capture_output=True, text=True)
        tail = (r.stdout + r.stderr).strip().splitlines()
        print(f"{prog}: " + " | ".join(l for l in tail if "renamed," in l or "SKIPPED" in l or "OK" in l or "FAILED" in l or "restored" in l))
        ok_all &= r.returncode == 0
    with open(os.path.join("config", "names_overlays.txt"), "a", newline="\n") as f:
        f.write("\n".join(log) + "\n")
    print("done" if ok_all else "SOME TARGETS FAILED - see above")


if __name__ == "__main__":
    main()
