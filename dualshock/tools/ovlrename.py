"""ovlrename.py - rename overlay symbols in every overlay at once (incl. src/shared/), verified by a full rebuild.

Run from dualshock/ with the project venv python:
    python tools/ovlrename.py OLD=NEW ...
    python tools/ovlrename.py --file renames.txt        # lines `OLD = NEW; // why (evidence)`

tools/rename.py --target stageN only edits src/stageN/; a function in src/shared/<enemy>.c is listed in
the symbol file of every stage that compiles it, so this tool renames a symbol in
  - every config/symbol_addrs_<overlay>.txt that lists it (also `OLD_suffix =` per-function variants),
  - every overlay source (src/shared, src/stage1..7, title, select, ending, logo, prologue), the overlay
    headers (include/overlay_api.h, include/stage_common.h), config/enemy_work/*.txt,
    config/names_overlays.txt and config/globals_overlays.txt (the name records: `= OLD;` -> `= NEW;`,
    and the comment of the `--file` line is appended as `[renamed from OLD: why]`),
matching OLD as a whole word plus alias variants `OLD_xyz` that are not symbols of their own.
A pair is SKIPPED if OLD also appears in a file this tool must not touch (main program sources,
include/re1_*.h, config/structs, config/addrs.txt, config/symbol_addrs.txt) or if NEW is already used.
Mentions in config/locals/*.txt (tools/localview.py specs) are not edited but printed as FOLLOW-UP.
Then `make split`, `make clean && make -j8`: all 13 must match, else everything is restored.
"""
import argparse
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
IDENT = re.compile(r"^[A-Za-z_]\w*$")
OVL_DIRS = ["src/shared"] + ["src/stage%d" % i for i in range(1, 8)] + \
           ["src/title", "src/select", "src/ending", "src/logo", "src/prologue"]
OVL_HEADERS = ["include/overlay_api.h", "include/stage_common.h"]


def run_make(cmd):
    r = subprocess.run(["sh", BUILDENV, cmd], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def read_pairs(args):
    pairs = []
    for a in args.pairs:
        old, new = a.split("=", 1)
        pairs.append((old.strip(), new.strip(), ""))
    if args.file:
        for line in open(args.file, encoding="utf-8"):
            code, _, why = line.partition("//")
            code = code.strip().rstrip(";")
            if code and not code.startswith("#"):
                old, new = code.split("=", 1)
                pairs.append((old.strip(), new.strip(), why.strip()))
    return pairs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pairs", nargs="*", help="OLD=NEW")
    ap.add_argument("--file")
    ap.add_argument("--no-build", action="store_true", help="edit only (no split / build check)")
    args = ap.parse_args()
    pairs = read_pairs(args)
    if not pairs:
        sys.exit(__doc__)

    srcs = []
    for d in OVL_DIRS:
        srcs += sorted(glob.glob(d + "/*.c") + glob.glob(d + "/*.h"))
    srcs += [h for h in OVL_HEADERS if os.path.exists(h)]
    srcs += sorted(glob.glob("config/enemy_work/*.txt"))
    records = ["config/names_overlays.txt", "config/globals_overlays.txt"]
    syms = sorted(p for p in glob.glob("config/symbol_addrs_*.txt") if not p.endswith("psyq.txt"))
    forbidden = sorted(glob.glob("src/main/*") + glob.glob("include/re1_*.h") + glob.glob("include/psyq_*.h")
                       + glob.glob("config/structs/*")
                       + ["config/addrs.txt", "config/symbol_addrs.txt", "config/symbol_addrs_psyq.txt"])
    srcs = [p.replace("\\", "/") for p in srcs]
    texts = {p: open(p, encoding="utf-8").read() for p in srcs + syms + records}
    backup = dict(texts)
    forb_text = "\n".join(open(p, encoding="utf-8", errors="replace").read() for p in forbidden)
    # config/locals (tool specs for tools/localview.py, owned by the main-program side): a mention there
    # does not affect the build - the rename goes ahead and the line to update is printed
    soft = {p: open(p, encoding="utf-8").read() for p in sorted(glob.glob("config/locals/*.txt"))}
    followups = []
    all_syms = set()
    for p in syms + ["config/symbol_addrs.txt"]:
        all_syms |= set(re.findall(r"^(\w+)\s*=", open(p, encoding="utf-8").read(), re.M))

    done, skipped = [], 0
    for old, new, why in pairs:
        everything = "\n".join(texts[p] for p in srcs + syms)
        problem = None
        if not IDENT.match(new):
            problem = "not a C identifier: " + new
        elif re.search(r"\b%s\b" % re.escape(new), everything) or new in all_syms:
            problem = new + " is already used"
        elif not re.search(r"\b%s\b" % re.escape(old), everything):
            problem = old + " not found"
        elif re.search(r"\b%s\b" % re.escape(old), forb_text):
            problem = old + " is also used outside the overlays (main sources / shared headers / main specs)"
        if problem:
            print("SKIPPED %s -> %s: %s" % (old, new, problem))
            skipped += 1
            continue
        # OLD_xyz that are symbols of their own are not alias variants
        own = sorted(s for s in all_syms if s.startswith(old + "_"))
        pat = re.compile(r"(?<![\w])%s(?=\b|_)" % re.escape(old))

        def rep(m, s=None):
            return new

        def sub_text(t):
            out = []
            pos = 0
            for m in pat.finditer(t):
                tail = re.match(r"\w*", t[m.end():]).group(0)
                if tail and (old + tail) in own:
                    continue
                out.append(t[pos:m.start()])
                out.append(new)
                pos = m.end()
            out.append(t[pos:])
            return "".join(out)

        for p in srcs + syms:
            texts[p] = sub_text(texts[p])
        for p in records:
            t = texts[p]
            note = " [renamed from %s%s]" % (old, ": " + why if why else "")

            def recrep(m):
                return m.group(1) + new + ";" + m.group(2) + note
            t = re.sub(r"(=\s*)%s;([^\n]*)" % re.escape(old), recrep, t)
            texts[p] = t
        for p, st in soft.items():
            for ln in st.splitlines():
                if re.search(r"\b%s\b" % re.escape(old), ln):
                    followups.append("%s: %s  (%s -> %s)" % (p, ln.strip(), old, new))
        all_syms |= {new}
        all_syms.discard(old)   # (a later pair of the batch may reuse the old name)
        done.append((old, new))
        print("%s -> %s" % (old, new))

    changed = [p for p in texts if texts[p] != backup[p]]
    for p in changed:
        open(p, "w", encoding="utf-8", newline="\n").write(texts[p])
    for old, new in done:
        for stale in glob.glob(os.path.join("asm", "*", "nonmatchings", "*", old + ".s")):
            os.remove(stale)
    print("%d renamed, %d skipped; %d files changed" % (len(done), skipped, len(changed)))
    for f in followups:
        print("FOLLOW-UP (not edited):", f)
    if args.no_build or not done:
        return
    print("re-splitting and rebuilding...")
    rc, log = run_make("make split")
    if rc == 0:
        rc, log = run_make("make clean && make -j8")
    if rc == 0 and "All 13 executables match" in log:
        print("OK: all 13 executables still match")
        return
    print("FAILED - restoring:\n" + "\n".join(l for l in log.splitlines()
                                             if "rror" in l or "MISMATCH" in l or "undefined" in l)[-3000:])
    for p in changed:
        open(p, "w", encoding="utf-8", newline="\n").write(backup[p])
    run_make("make split")
    rc, log = run_make("make clean && make -j8")
    print("restored;", "build matches" if "All 13 executables match" in log else "BUILD STILL BROKEN")
    sys.exit(1)


if __name__ == "__main__":
    main()
