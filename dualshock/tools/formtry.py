"""formtry.py - rewrite code to a more readable form wherever it compiles to the same bytes (per-site trials).

Run from dualshock/ with the project venv python:
    python tools/formtry.py RULES.py src/main/effect.c [more files]
    python tools/formtry.py --whole RULES.py src/main/effect.c    # per function only, no per-site retries

RULES.py is a python file defining
    RULES = [(regex, replacement), ...]     # re.sub style; applied inside function bodies only; a callable
                                             # replacement gets the match and returns the new text or None
    SKIP_LINE = r"..."                       # optional: lines matching this regex are never rewritten
    FUNCS = ["name", ...]                    # optional: only these functions
Every match of every rule in a function is a "site" (earlier rules win where matches overlap; a rule whose
replacement leaves the text unchanged protects it from the later rules). Round 1
rewrites all sites of every function; a function whose build fails or whose bytes change then tries its
sites ONE AT A TIME (all functions advance in parallel, one build per round) and keeps each rewrite that
compiles to the same bytes; a final check drops the rewrites of any function that still fails.
Same scheme as tools/aliastry.py (which tries removing per-function aliases).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enemywork as ew  # noqa: E402
import integrate as ig  # noqa: E402


def load_rules(path):
    ns = {}
    exec(open(path).read(), ns)
    rules = [(re.compile(p, re.M), r) for p, r in ns["RULES"]]
    skip = re.compile(ns["SKIP_LINE"]) if ns.get("SKIP_LINE") else None
    return rules, skip, set(ns.get("FUNCS") or [])


def sites(body, rules, skip):
    """[(start, end, replacement)] non-overlapping, in text order."""
    taken = []
    out = []
    for pat, repl in rules:
        for m in pat.finditer(body):
            s, e = m.span()
            if any(s < te and ts < e for ts, te in taken):
                continue
            if skip:
                ls = body.rfind("\n", 0, s) + 1
                le = body.find("\n", e)
                if skip.search(body[ls:le if le >= 0 else len(body)]):
                    continue
            new = repl(m) if callable(repl) else m.expand(repl)
            if new is None:
                continue
            taken.append((s, e))            # an unchanged match protects its text from later rules
            if new == m.group(0):
                continue
            out.append((s, e, new))
    return sorted(out)


def apply_sites(body, chosen):
    for s, e, new in sorted(chosen, reverse=True):
        body = body[:s] + new + body[e:]
    return body


def process(path, rules, skip, only, whole):
    original = open(path).read()
    lines = original.split("\n")
    spans = ig.function_spans(original)
    names = [n for _, _, n in spans]
    bodies = {n: "\n".join(lines[s - 1:e]) for s, e, n in spans}
    cands = {n: (sites(bodies[n], rules, skip) if (not only or n in only) else []) for n in names}
    if not any(cands.values()):
        print(f"{path}: nothing to try")
        return 0, 0
    mode = {n: ("all" if cands[n] else "done") for n in names}
    accepted = {n: [] for n in names}
    idx = {n: 0 for n in names}

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
            out += apply_sites(bodies[n], trial(n)).split("\n")
            pos = e
        return "\n".join(out + lines[pos:])

    def advance(n, ok):
        if mode[n] == "all":
            if ok:
                accepted[n], mode[n] = list(cands[n]), "done"
            elif whole or len(cands[n]) == 1:
                mode[n] = "done"
            else:
                mode[n] = "one"
            return
        if mode[n] == "one":
            if ok:
                accepted[n].append(cands[n][idx[n]])
            idx[n] += 1
            if idx[n] >= len(cands[n]):
                mode[n] = "done"

    def restore():
        ig.write_file(path, original)
        for prog, f in ew.programs_of(path):
            ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))

    tail = path.replace(os.sep, "/").split("src/")[-1]
    for rnd in range(2000):
        if all(mode[n] == "done" for n in names):
            break
        text = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        trying = {n for n in names if mode[n] != "done"}
        if errs:
            spans_now = ig.function_spans(text)
            bad = {n for f, ln, _ in errs for s, e, n in spans_now
                   if f.replace(os.sep, "/").endswith(tail) and s <= int(ln) <= e}
            if not bad & trying:
                bad = trying
            for n in bad & trying:
                advance(n, False)
            continue
        ok_all, bad = res
        if not ok_all and not bad:
            print(f"{path}: build failed for another reason, restoring")
            restore()
            return 0, 0
        for n in trying:
            advance(n, ok_all or n not in bad)
    for _ in range(30):
        text = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        if not errs and res[0]:
            break
        if errs:
            spans_now = ig.function_spans(text)
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
        restore()
        return 0, 0
    done = sum(len(accepted[n]) for n in names)
    total = sum(len(c) for c in cands.values())
    print(f"OK: {path}: {done} of {total} sites rewritten")
    for n in names:
        if cands[n] and len(accepted[n]) < len(cands[n]):
            print(f"   kept {len(cands[n]) - len(accepted[n])} of {len(cands[n])} in {n}")
    return done, total


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2:
        sys.exit(__doc__)
    rules, skip, only = load_rules(args[0])
    whole = "--whole" in sys.argv
    done = total = 0
    for p in args[1:]:
        d, t = process(p, rules, skip, only, whole)
        done += d
        total += t
    print(f"total: {done} of {total} sites rewritten")


if __name__ == "__main__":
    main()
