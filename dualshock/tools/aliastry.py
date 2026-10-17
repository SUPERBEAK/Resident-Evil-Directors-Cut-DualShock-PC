"""aliastry.py - drop per-function __asm__ aliases wherever the real name compiles to the same bytes.

Run from dualshock/ with the project venv python:
    python tools/aliastry.py src/main/door.c [more files]      # data and function aliases
    python tools/aliastry.py --data-only src/main/door.c        # only aliases of globals

During matching, functions declared the globals and functions they use as private aliases
(`extern u8 *g_RoomData_l __asm__("g_RoomData");`, `s32 sound_play_l() __asm__("sound_play");`), usually
because the shared declaration's type differed from what m2c wanted. Often the shared declaration compiles
to the very same code anyway (a value only stored or passed on, a call with promoted arguments).
Candidates per function: aliases of data with a file-scope declaration (the program's globals.h / this
file), and aliases of functions defined in other files and not prototyped at file scope here (an implicit
declaration of a function defined later in this file would conflict there, far from the function at fault).
Round 1 removes all candidates of every function; a function whose build fails or whose bytes change then
tries its candidates ONE AT A TIME (all functions advance in parallel, one build per round) and keeps each
removal that compiles to the same bytes. Every round rebuilds every program that compiles the file.
tools/funcaliasclean.py / declclean.py handle the exact-type cases; this one simply tries.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enemywork as ew  # noqa: E402
import integrate as ig  # noqa: E402

DATA = re.compile(r'^[ \t]*extern\s+[^;()]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*__asm__\("(\w+)"\);[ \t]*\n', re.M)
FUNC = re.compile(r'^[ \t]*(?:extern\s+)?[A-Za-z_][\w \*]*?\b(\w+)\s*\([^()]*\)\s*__asm__\("(\w+)"\);[ \t]*\n', re.M)
EXTERN = re.compile(r"^extern\s+[^;()]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", re.M)


def declared_globals(path):
    """Data names with a file-scope declaration visible in this file (its program's globals.h + the file)."""
    names = set()
    prog = path.replace(os.sep, "/").split("/")[1]
    for h in (os.path.join("src", prog, "globals.h"), os.path.join("src", "main", "globals.h"), path):
        if os.path.isfile(h):
            names |= set(EXTERN.findall(open(h).read()))
    return names


def candidates(body, known, data_only, local_funcs):
    """[(declaration text, alias, real name)] that may be dropped."""
    out = []
    for pat, kind in ((DATA, "data"), (FUNC, "func")):
        if kind == "func" and data_only:
            continue
        for m in pat.finditer(body):
            alias, sym = m.group(1), m.group(2)
            if alias == sym or (kind == "data" and sym not in known) or (kind == "func" and sym in local_funcs):
                continue
            out.append((m.group(0), alias, sym))
    return out


def rewrite(body, drop):
    for decl, alias, sym in drop:
        body = body.replace(decl, "", 1)
    for decl, alias, sym in drop:
        body = re.sub(rf"\b{re.escape(alias)}\b", sym, body)
    return body


def process(path, data_only):
    original = open(path).read()
    lines = original.split("\n")
    spans = ig.function_spans(original)
    names = [n for _, _, n in spans]
    known = declared_globals(path)
    local_funcs = set(names) | set(re.findall(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^()]*\)\s*;", original, re.M))
    bodies = {n: "\n".join(lines[s - 1:e]) for s, e, n in spans}
    cands = {n: candidates(bodies[n], known, data_only, local_funcs) for n in names}
    # mode: "all" (round 1), "one" (one at a time: accepted + cands[n][idx]), "done"
    mode = {n: ("all" if cands[n] else "done") for n in names}
    accepted = {n: [] for n in names}
    idx = {n: 0 for n in names}
    if not any(cands.values()):
        print(f"{path}: nothing to try")
        return 0

    def trial(n):
        if mode[n] == "all":
            return cands[n]
        if mode[n] == "one":
            return accepted[n] + [cands[n][idx[n]]]
        return accepted[n]

    def assemble():
        out, pos = [], 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            out += rewrite(bodies[n], trial(n)).split("\n")
            pos = e
        return "\n".join(out + lines[pos:])

    def advance(n, ok):
        if mode[n] == "all":
            if ok:
                accepted[n], mode[n] = list(cands[n]), "done"
            else:
                mode[n] = "one"
            return
        if mode[n] == "one":
            if ok:
                accepted[n].append(cands[n][idx[n]])
            idx[n] += 1
            if idx[n] >= len(cands[n]):
                mode[n] = "done"

    for rnd in range(200):
        if all(mode[n] == "done" for n in names):
            break
        text = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        if errs:
            spans_now = ig.function_spans(text)
            tail = path.replace(os.sep, "/").split("src/")[-1]
            bad = {n for f, ln, _ in errs for s, e, n in spans_now
                   if f.replace(os.sep, "/").endswith(tail) and s <= int(ln) <= e}
            trying = {n for n in names if mode[n] != "done"}
            if not bad & trying:
                bad = trying            # an error we can't place: everyone being tried fails this round
            # (the bytes of the others were not checked: they try the same again next round)
            for n in bad & trying:
                advance(n, False)
            continue
        ok_all, bad = res
        for n in names:
            if mode[n] != "done":
                advance(n, ok_all or n not in bad)
        if not ok_all and not bad:
            print(f"{path}: build failed for another reason, restoring")
            ig.write_file(path, original)
            for prog, f in ew.programs_of(path):
                ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))
            return 0
    # final: the accepted removals; verify once more. Functions that fail now (a removal can interact with
    # another function's: e.g. GCC keeps an implicit declaration for the rest of the file) get their old
    # text back, until everything matches.
    for _ in range(20):
        text = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        if not errs and res[0]:
            break
        if errs:
            spans_now = ig.function_spans(text)
            tail = path.replace(os.sep, "/").split("src/")[-1]
            fail = {n for f, ln, _ in errs for s, e, n in spans_now
                    if f.replace(os.sep, "/").endswith(tail) and s <= int(ln) <= e}
        else:
            fail = set(res[1])
        fail = {n for n in fail if accepted[n]}
        if not fail:
            break
        for n in fail:
            accepted[n] = []
    if errs or not res[0]:
        print(f"{path}: final check failed, restoring:", errs[:4] if errs else sorted(res[1])[:8])
        ig.write_file(path, original)
        for prog, f in ew.programs_of(path):
            ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))
        return 0
    total = sum(len(accepted[n]) for n in names)
    print(f"OK: {path}: {total} of {sum(len(c) for c in cands.values())} aliases removed")
    return total


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        sys.exit(__doc__)
    total = sum(process(p, "--data-only" in sys.argv) for p in args)
    print(f"total: {total} aliases removed")


if __name__ == "__main__":
    main()
