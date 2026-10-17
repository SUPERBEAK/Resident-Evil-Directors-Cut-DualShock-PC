"""movesweep.py: hill-climb the statement order of drafts by moving single statements.

    python tools/movesweep.py one <func> <in.c> <out.c> [--target main]
    python tools/movesweep.py batch [--target main] [--min-dist 1] [--max-dist 30] [--jobs 8]

A block is a run of consecutive simple statements (plain `...;` lines at the same indentation:
no declarations, no control flow - the same test as swapadj.py). Every move of one statement to
another position inside its block is compiled and measured (refine.distance); the best
improvement is kept and the search repeats until nothing improves. At -O2 Capcom's statement
order decides scheduling and register allocation and m2c's order often differs (main
func_8004ED90: three byte stores had to follow the two u16 copies; func_8005ED40: `v.vy = 0`
after v.vz). swapadj.py only swaps neighbours; this tries every single move, every swap of two
statements, every sub-run wrapped in `do { ... } while (0);` (a scheduling barrier the permuter
found twice) and every other access width (s8/u8/s16/u16/s32/u32) of each `*(T *)` dereference
and `M2C_FIELD(x, T *, off)` - m2c often guesses a width wrong (main func_8003C99C sh/sb,
func_80043E04 sw/sh, func_8005AF84 lh/lw). --no-wrap / --no-widths turn those parts off.

batch: every pending draft with status diff:N (min-dist <= N <= max-dist), in parallel
(multiprocessing, --jobs workers); improvements are written back to the draft and to
results.json (match / diff:N) like trybatch.py. Run from dualshock/ through buildenv.sh. Start it
from a shell with BelowNormal priority so the workers inherit it."""
import argparse
import glob
import json
import multiprocessing as mp
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refine  # noqa: E402

CTRL = re.compile(r"^\s*(if|else|for|while|do|switch|case|default|return|goto|break|continue|\}|\{)\b|^\s*\}|:\s*$")
DECL = re.compile(r"^\s*(?:extern\s+|register\s+|static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?"
                  r"(?:[us](?:8|16|32)|int|char|short|long|void|f32|M2C_UNK\w*|[A-Z]\w*)\b[\s\*]+\w[\w\s\*]*"
                  r"(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*$")
MAX_BLOCK = 12
WRAP = True  # also try do { ... } while (0) around sub-runs (--no-wrap turns it off)
WIDTHS = True  # also try other access widths (--no-widths turns it off)
WTYPES = ["s8", "u8", "s16", "u16", "s32", "u32"]
# (M2C_UNK = m2c's unknown 32-bit type: main func_8001ED3C had six of them that were u8/s16)
WPAT = re.compile(r"(?<=\*\()([su](?:8|16|32)|M2C_UNK)(?= \*\))|(?<=, )([su](?:8|16|32)|M2C_UNK)(?= \*, )")
# (unlike swapadj.py's DECL, a type must be followed by a name: `D_800B257D = x;` is a statement)


def simple(l):
    return l.strip().endswith(";") and not CTRL.match(l) and not DECL.match(l) and "__asm__" not in l


def blocks(lines):
    """[(start, end)] line ranges (end exclusive) of runs of >= 2 simple statements."""
    start = next((i for i, l in enumerate(lines) if re.match(r"^\w.*\)\s*\{\s*$", l)), None)
    out = []
    if start is None:
        return out
    i = start + 1
    while i < len(lines):
        if simple(lines[i]):
            ind = len(lines[i]) - len(lines[i].lstrip())
            j = i
            while j < len(lines) and simple(lines[j]) and len(lines[j]) - len(lines[j].lstrip()) == ind:
                j += 1
            if 2 <= j - i <= MAX_BLOCK:
                out.append((i, j))
            i = j
        else:
            i += 1
    return out


def variants(text):
    lines = text.split("\n")
    for a, b in blocks(lines):
        for i in range(a, b):
            for j in range(a, b):
                if j == i or j == i + 1:  # moving next to itself = no change / same as swap from other side
                    continue
                new = lines[:]
                l = new.pop(i)
                new.insert(j if j < i else j - 1, l)
                yield "\n".join(new)
        # swaps of two non-adjacent statements (two moves whose middle step can be worse:
        # main func_80050B84 needed the 0x20 and 0x26 stores of 0x28 exchanged)
        for i in range(a, b):
            for j in range(i + 2, b):
                new = lines[:]
                new[i], new[j] = new[j], new[i]
                yield "\n".join(new)
    if WIDTHS:
        start = text.index("{") if "{" in text else 0
        for m in WPAT.finditer(text, start):
            for t in WTYPES:
                if t != m.group(0):
                    yield text[:m.start()] + t + text[m.end():]
    if not WRAP:
        return
    # a sub-run wrapped in `do { ... } while (0);` - a scheduling barrier the permuter found in
    # main func_80019D90 and func_8005E0E4
    for a, b in blocks(lines):
        ind = lines[a][:len(lines[a]) - len(lines[a].lstrip())]
        for i in range(a, b):
            for j in range(i + 1, b + 1):
                new = lines[:i] + [ind + "do {"] + ["    " + l for l in lines[i:j]] + \
                    [ind + "} while (0);"] + lines[j:]
                yield "\n".join(new)


def joinblocks(text):
    """Drop blank lines between two simple statements of the same indentation, so a statement
    can move past them (main func_8001D100: `ply = (u8 *)0x800C3024;` stood alone)."""
    lines = text.split("\n")
    out = []
    for i, l in enumerate(lines):
        if not l.strip() and out and 0 < i < len(lines) - 1 and simple(out[-1]) and simple(lines[i + 1]) \
                and len(out[-1]) - len(out[-1].lstrip()) == len(lines[i + 1]) - len(lines[i + 1].lstrip()):
            continue
        out.append(l)
    return "\n".join(out)


def climb(ctx, name, text, d0, tag):
    joined = joinblocks(text)
    if joined != text:
        dj = refine.distance(ctx, name, joined, tag)
        if dj is not None and dj <= d0:
            text, d0 = joined, dj
    best, bd = text, d0
    seen = {text}
    while bd:
        cand = None
        for v in variants(best):
            if v in seen:
                continue
            seen.add(v)
            d = refine.distance(ctx, name, v, tag)
            if d is not None and d < bd and (cand is None or d < cand[1]):
                cand = (v, d)
                if d == 0:
                    break
        if cand is None:
            break
        best, bd = cand
    return best, bd


def work(job):
    global WRAP, WIDTHS
    target, name, path, d0, WRAP, WIDTHS = job  # (spawned workers do not see main()'s globals)
    ctx = refine.Ctx(target)
    text = open(path).read()
    best, bd = climb(ctx, name, text, d0, f"mv{os.getpid()}")
    return name, path, d0, bd, (best if bd < d0 else None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["one", "batch"])
    ap.add_argument("rest", nargs="*")
    ap.add_argument("--target", default="main")
    ap.add_argument("--min-dist", type=int, default=1)
    ap.add_argument("--max-dist", type=int, default=30)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--no-wrap", action="store_true")
    ap.add_argument("--no-widths", action="store_true")
    a = ap.parse_args()
    global WRAP, WIDTHS
    WRAP = not a.no_wrap
    WIDTHS = not a.no_widths
    if a.mode == "one":
        name, src, out = a.rest
        ctx = refine.Ctx(a.target)
        text = open(src).read()
        d0 = refine.distance(ctx, name, text, "mv")
        best, bd = climb(ctx, name, text, d0, "mv")
        open(out, "w", newline="\n").write(best)
        print(f"{name}: {d0} -> {bd}")
        return
    rp = f"build/auto/{a.target}/results.json"
    pending = set()
    for cf in glob.glob(f"src/{a.target}/*.c"):
        pending |= set(re.findall(r'INCLUDE_ASM\("[^"]+", (\w+)\)', open(cf).read()))
    jobs = []
    for n, st in sorted(json.load(open(rp)).items()):
        if n not in pending or not st.startswith("diff:"):
            continue
        d = int(st[5:].split(",")[0])
        f = f"build/auto/{a.target}/c/{n}.c"
        if a.min_dist <= d <= a.max_dist and os.path.exists(f) and blocks(open(f).read().split("\n")):
            jobs.append((a.target, n, f, d, WRAP, WIDTHS))
    jobs.sort(key=lambda j: j[3])
    print(f"{len(jobs)} drafts", flush=True)
    better = 0
    with mp.Pool(a.jobs) as pool:
        for name, path, d0, bd, best in pool.imap_unordered(work, jobs):
            if best is None:
                continue
            rr = json.load(open(rp))
            now = rr.get(name, "")
            # something else (the permuter, a hand edit) may have stored a better draft meanwhile
            if now == "match" or (now.startswith("diff:") and int(now[5:].split(",")[0]) <= bd):
                continue
            better += 1
            print(f"{name}: diff:{d0} -> {bd}", flush=True)
            open(path, "w", newline="\n").write(best)
            rr[name] = "match" if bd == 0 else f"diff:{bd}"
            json.dump(rr, open(rp, "w"), indent=1, sort_keys=True)
    print(f"{better} improved", flush=True)


if __name__ == "__main__":
    main()
