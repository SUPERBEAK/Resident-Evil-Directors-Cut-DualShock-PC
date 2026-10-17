"""fieldrename.py - rename struct fields everywhere, guided by the compiler, verified by a full build.

Run from dualshock/ with the project venv python:
    python tools/fieldrename.py <batch file>      # lines: `Struct old_name new_name   # comment`
    python tools/fieldrename.py --fast <batch>    # verify with an incremental build (only for renames in .c
                                                  # files: header changes need the clean build)

The same placeholder name (unk14) exists in many structs, so a text search can't tell the uses apart.
The compiler can: for every pair the definition is renamed (spec line in config/structs, config/locals or
config/enemy_work, and/or the typedef block `} Struct;` in include/*.h or src/), the generated headers are
rewritten, and `make -k` reports "structure has no member named `old'" at each use; those lines get the new
name. Repeated until it compiles (rounds: one old name at a time per round, so a line can't mix up two
structs' fields), then a clean build must match all 13 executables; otherwise everything is restored.
"""
import glob
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
PY = sys.executable
SPEC_DIRS = [os.path.join("config", d) for d in ("structs", "locals", "enemy_work")]
# (parallel make can glue two messages together: take the last src/ or include/ path before `:line:`)
NO_MEMBER = re.compile(r"(?P<file>(?:src|include)[/\\](?:(?!src[/\\])[\w./\\])+\.[ch]):(?P<line>\d+): (?:structure|union) has no member named `(?P<name>\w+)'")


def make(cmd):
    r = subprocess.run(["sh", BUILDENV, cmd], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def src_files():
    out = []
    for top in ("src", "include"):
        for d, _, fs in os.walk(top):
            out += [os.path.join(d, f) for f in fs if f.endswith((".c", ".h"))]
    return out


def rename_definitions(struct, old, new, texts):
    """Rename the field in the spec(s) of struct and in typedef blocks `} struct;`. Returns how many."""
    n = 0
    for d in SPEC_DIRS:
        for p in glob.glob(os.path.join(d, "*.txt")):
            t = texts.setdefault(p, open(p).read())
            if not re.search(rf"^struct {re.escape(struct)}\s*$", t, re.M):
                continue
            t2, k = re.subn(rf"^(0x[0-9A-Fa-f]+\s+[^#\n]*?)\b{re.escape(old)}\b", rf"\g<1>{new}", t, flags=re.M)
            if k:
                texts[p], n = t2, n + k
    for p in src_files():
        t = texts.setdefault(p, open(p).read())
        for m in re.finditer(rf"\}}\s*{re.escape(struct)}\s*;", t):
            start = t.rfind("typedef struct", 0, m.start())
            if start < 0:
                continue
            block = t[start:m.end()]
            block2, k = re.subn(rf"\b{re.escape(old)}\b", new, block)
            if k:
                t = t[:start] + block2 + t[m.end():]
                texts[p], n = t, n + k
    return n


def main():
    fast = "--fast" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--fast"]
    if len(args) != 1:
        sys.exit(__doc__)
    pairs = []
    for line in open(args[0]):
        line = line.split("#")[0].split()
        if len(line) == 3:
            pairs.append(tuple(line))
    texts = {}
    for p in src_files():
        texts[p] = open(p).read()
    backup = {}

    def save(p, t):
        if p not in backup:
            backup[p] = open(p).read()
        open(p, "w", newline="\n").write(t)

    for struct, old, new in pairs:
        n = rename_definitions(struct, old, new, texts)
        print(f"{struct}.{old} -> {new}: {n} definition(s)")
        if n == 0 and rename_definitions(struct, new, new, {}) > 0:
            print("  (already renamed - resuming)")
            continue
        if n == 0:
            print("  NOT FOUND - stopping")
            for p, t in backup.items():
                open(p, "w", newline="\n").write(t)
            sys.exit(1)
    for p, t in texts.items():
        if os.path.exists(p) and open(p).read() != t:
            save(p, t)
    # the generated headers
    for tool in ("structview", "localview"):
        subprocess.run([PY, os.path.join("tools", f"{tool}.py"), "header"], capture_output=True)

    old_to_new = defaultdict(set)
    for _, old, new in pairs:
        old_to_new[old].add(new)
    for rnd in range(30):
        rc, log = make("make -k -j8")
        errs = [(m.group("file"), int(m.group("line")), m.group("name")) for m in NO_MEMBER.finditer(log)]
        if not errs:
            break
        fixed = 0
        by_file = defaultdict(set)
        for f, ln, name in errs:
            by_file[os.path.normpath(f)].add((ln, name))
        for f, items in by_file.items():
            lines = open(f).read().split("\n")
            for ln, name in sorted(items):
                news = old_to_new.get(name)
                if not news:
                    continue
                if len(news) > 1:
                    print(f"  {f}:{ln}: `{name}` has several new names {sorted(news)} - fix by hand")
                    continue
                new = next(iter(news))
                line2 = re.sub(rf"\b{re.escape(name)}\b", new, lines[ln - 1])
                if line2 != lines[ln - 1]:
                    lines[ln - 1] = line2
                    fixed += 1
            save(f, "\n".join(lines))
        print(f"round {rnd + 1}: {len(errs)} use errors, {fixed} lines fixed")
        if fixed == 0:
            print("no progress; remaining errors:")
            print("\n".join(f"  {f}:{ln}: {n}" for f, ln, n in errs[:30]))
            break
    rc, log = make("make -j8" if fast else "make clean && make -j8")
    if rc == 0 and "All 13 executables match" in log:
        print(f"OK: all 13 executables still match ({len(backup)} files changed)")
        return
    print("FAILED - restoring:\n" + "\n".join(l for l in log.splitlines() if "rror" in l or "MISMATCH" in l)[-3000:])
    for p, t in backup.items():
        open(p, "w", newline="\n").write(t)
    for tool in ("structview", "localview"):
        subprocess.run([PY, os.path.join("tools", f"{tool}.py"), "header"], capture_output=True)
    rc, log = make("make clean && make -j8")
    print("restored;", "build matches" if "All 13 executables match" in log else "BUILD STILL BROKEN")
    sys.exit(1)


if __name__ == "__main__":
    main()
