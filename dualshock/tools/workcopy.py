"""workcopy.py - private copies of dualshock/ for parallel work, and merging their changes back.

Run from dualshock/ with the project venv python:
    python tools/workcopy.py create <name>   # ../work_<name>/: copy of the sources, split + built
    python tools/workcopy.py diff <name>     # ../temp/work_<name>.patch (changes since create)
    python tools/workcopy.py apply <name>    # apply that patch to dualshock/, split, clean build;
                                             # restores dualshock/ if anything fails to match

A work copy sits next to dualshock/ (same depth), so its relative paths to ../extracted and
../tools work unchanged; it can be built and checked on its own with the usual
commands. Only src/, include/ and config/ are compared and merged (the rest is regenerated).
The baseline of a copy is saved in work_<name>/.baseline/ at creation, so the patch contains only
the copy's own changes and applies cleanly to a dualshock/ that changed elsewhere meanwhile.
"""
import difflib
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
TRACKED = ("src", "include", "config")
SKIP = {"build", "asm", "assets", "__pycache__", ".splache", ".baseline"}


def make(cwd, cmd):
    r = subprocess.run(["sh", BUILDENV, cmd], cwd=cwd, capture_output=True, text=True)
    return r.returncode == 0 and ("All 13 executables match" in r.stdout + r.stderr or "split" in cmd), r.stdout + r.stderr


def files_under(base):
    out = {}
    for top in TRACKED:
        for d, _, fs in os.walk(os.path.join(base, top)):
            for f in fs:
                p = os.path.join(d, f)
                out[os.path.relpath(p, base).replace(os.sep, "/")] = p
    return out


def create(name):
    dest = os.path.join(ROOT, f"work_{name}")
    if os.path.exists(dest):
        sys.exit(f"{dest} already exists")
    shutil.copytree(".", dest, ignore=lambda d, names: [n for n in names if n in SKIP])
    os.makedirs(os.path.join(dest, "build"))
    for top in TRACKED:
        shutil.copytree(top, os.path.join(dest, ".baseline", top))
    ok, log = make(dest, "make split")
    ok2, log2 = make(dest, "make -j8")
    print(f"created {dest}:", "builds, all 13 match" if ok2 else "BUILD FAILED\n" + log2[-2000:])


def diff(name):
    dest = os.path.join(ROOT, f"work_{name}")
    base, cur = files_under(os.path.join(dest, ".baseline")), files_under(dest)
    chunks = []
    for rel in sorted(set(base) | set(cur)):
        a = open(base[rel], encoding="utf-8", errors="surrogateescape").read().splitlines(True) if rel in base else []
        b = open(cur[rel], encoding="utf-8", errors="surrogateescape").read().splitlines(True) if rel in cur else []
        if a == b:
            continue
        chunks.append(f"diff --git a/dualshock/{rel} b/dualshock/{rel}\n")
        if not a:
            chunks.append("new file mode 100644\n")
        if not b:
            chunks.append("deleted file mode 100644\n")
        d = difflib.unified_diff(a, b, "/dev/null" if not a else f"a/dualshock/{rel}",
                                 "/dev/null" if not b else f"b/dualshock/{rel}", n=3)
        chunks.extend(l if l.endswith("\n") else l + "\n\\ No newline at end of file\n" for l in d)
    out = os.path.join(ROOT, "temp", f"work_{name}.patch")
    open(out, "w", encoding="utf-8", errors="surrogateescape", newline="\n").write("".join(chunks))
    files = sum(1 for c in chunks if c.startswith("diff --git"))
    print(f"{out}: {files} file(s) changed")
    return out


def apply(name):
    patch = diff(name)
    chk = subprocess.run(["git", "apply", "--check", patch], cwd=ROOT, capture_output=True, text=True)
    if chk.returncode:
        sys.exit("patch does not apply cleanly:\n" + chk.stderr[-3000:])
    backup = {rel: open(p, "rb").read() for rel, p in files_under(".").items()}
    subprocess.run(["git", "apply", patch], cwd=ROOT, check=True)
    ok, log = make(".", "make split")
    ok, log = make(".", "make clean && make -j8")
    if ok:
        print("applied; all 13 executables match")
        return
    print("FAILED after applying - restoring dualshock/:\n" + "\n".join(
        l for l in log.splitlines() if "rror" in l or "MISMATCH" in l)[-3000:])
    for rel in files_under("."):
        if rel not in backup:
            os.remove(rel)
    for rel, data in backup.items():
        open(rel, "wb").write(data)
    make(".", "make split")
    ok, _ = make(".", "make clean && make -j8")
    print("restored;", "build matches" if ok else "BUILD STILL BROKEN")
    sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("create", "diff", "apply"):
        sys.exit(__doc__)
    {"create": create, "diff": diff, "apply": apply}[sys.argv[1]](sys.argv[2])
