"""Dead declarations: file-scope externs / typedefs / #defines that nothing else uses (run from dualshock/)."""
import os
import re

COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
texts = {}
for top in ("src", "include"):
    for d, _, fs in os.walk(top):
        for f in fs:
            if f.endswith((".c", ".h")):
                p = os.path.join(d, f).replace(os.sep, "/")
                texts[p] = COMMENT.sub("", open(p, encoding="latin-1").read())


def uses(name, scope):
    return sum(len(re.findall(rf"\b{re.escape(name)}\b", texts[p])) for p in scope)


def scope_of(p):
    # a stage header is seen by its stage's files + shared/ files compiled into it; main headers by everything
    parts = p.split("/")
    if p.startswith("src/") and parts[1] not in ("main",):
        return [q for q in texts if q.startswith(f"src/{parts[1]}/") or q.startswith("src/shared/")]
    return list(texts)


report = {}
for p, t in texts.items():
    if p.endswith(("re1_views.h", "re1_locals.h", "re1_addrs.h")):
        continue
    sc = scope_of(p)
    dead = []
    # file-scope extern variables / prototypes (lines starting at column 0)
    for m in re.finditer(r"^extern\s+[^;(]*?\b(\w+)\s*(?:\[[^\]]*\])*\s*(?:__asm__\([^)]*\))?\s*;", t, re.M):
        if uses(m.group(1), sc) <= 1:
            dead.append(("extern", m.group(1)))
    for m in re.finditer(r"^typedef\b[^;{]*?\b(\w+)\s*;", t, re.M):
        if uses(m.group(1), sc) <= 1:
            dead.append(("typedef", m.group(1)))
    for m in re.finditer(r"^typedef\s+(?:struct|union)\s*\w*\s*\{(?:[^{}]|\{[^{}]*\})*\}\s*(\w+)\s*;", t, re.M):
        if uses(m.group(1), sc) <= 1:
            dead.append(("typedef", m.group(1)))
    for m in re.finditer(r"^#define\s+(\w+)", t, re.M):
        n = m.group(1)
        if n.endswith("_H") or n.startswith("__"):
            continue
        if uses(n, sc) <= 1:
            dead.append(("define", n))
    if dead:
        report[p] = dead

tot = 0
for p in sorted(report):
    d = report[p]
    tot += len(d)
    kinds = {}
    for k, n in d:
        kinds.setdefault(k, []).append(n)
    print(f"{p}: " + "; ".join(f"{k} x{len(v)}: {' '.join(v[:12])}{' ...' if len(v) > 12 else ''}" for k, v in kinds.items()))
print("total", tot)
