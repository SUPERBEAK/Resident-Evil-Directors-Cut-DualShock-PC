"""splitplan.py - turn a wish list of topic-file starts into a split plan that keeps the byte match.

Run from dualshock/ with the project venv python:
    python tools/splitplan.py wish.txt plan.txt

wish.txt: `<first function> <file name>` per line, in address order. A file can only start where its
rodata will land exactly as in the original: each C file's .rodata section is aligned (and its
`.align 3` jump tables padded) relative to the FILE's start, so a file whose rodata contains an
8-aligned jump table must have its first rodata item at an 8-aligned address. Files without 8-aligned
rodata (or without rodata) can start anywhere. Invalid starts are moved to the nearest function that
makes both neighbouring files valid; files that would become empty are merged (reported).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402
import splitc  # noqa: E402


def main():
    wish = [l.split() for l in open(sys.argv[1]) if l.strip() and not l.startswith("#")]
    txt = open(os.path.join("src", "main", "capcom.c")).read()
    lines = txt.split("\n")
    spans = ig.function_spans(txt)
    names = [n for _, _, n in spans]
    info = []
    for k, (s, e, n) in enumerate(spans):
        t, r = splitc.func_offsets(n)
        asm = open(os.path.join(splitc.ASMDIR, n + ".s")).read()
        jt = len(re.findall(r"^dlabel jtbl_", asm, re.M))
        prev_end = spans[k - 1][1] if k else 0
        markers = sum(1 for l in lines[prev_end:s] if "#JTALIGN2" in l)
        info.append((r, jt > markers))          # (first rodata offset, has an 8-aligned table)

    def valid(i0, i1):
        ro = [info[i][0] for i in range(i0, i1) if info[i][0] is not None]
        if not ro or not any(info[i][1] for i in range(i0, i1)):
            return True
        return (min(ro) - 0x800) % 8 == 0

    starts = [names.index(f) for f, _ in wish]
    files = [f for _, f in wish]
    starts[0] = 0
    for k in range(1, len(starts)):
        end_next = starts[k + 1] if k + 1 < len(starts) else len(names)
        best = None
        for d in sorted(range(-40, 41), key=abs):
            c = starts[k] + d
            if c <= starts[k - 1] or c >= end_next:
                continue
            if valid(starts[k - 1], c) and valid(c, end_next):
                best = c
                break
        if best is None:
            print(f"{files[k]}: no valid start near {names[starts[k]]} - merged into {files[k - 1]}")
            starts[k] = None
            continue
        if best != starts[k]:
            print(f"{files[k]}: {names[starts[k]]} -> {names[best]} ({best - starts[k]:+d})")
        starts[k] = best
        # (later files are checked against this new start)
    plan = [(s, f) for s, f in zip(starts, files) if s is not None]
    # final check over the whole plan
    bad = [f for k, (s, f) in enumerate(plan) if not valid(s, plan[k + 1][0] if k + 1 < len(plan) else len(names))]
    if bad:
        print("STILL INVALID:", bad)
    with open(sys.argv[2], "w", newline="\n") as fo:
        for s, f in plan:
            fo.write(f"{names[s]} {f}\n")
    print(f"{len(plan)} files -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
