"""rename.py - give functions / globals real names, verified by a full rebuild.

Run from dualshock/ with the project venv python:
    python tools/rename.py func_8003BAF8=Pad_Update D_800CAB14=g_CurrentEntity ...
    python tools/rename.py --file names.txt            # lines `OLD = NEW; // comment`
    python tools/rename.py --target stage1 func_80105400=Stage1_Init ...   # overlay-local symbol

For every pair:
  - the symbol file gets `NEW = 0xADDR; // type:func` (no type for data; config/symbol_addrs.txt for the main
    program - the overlays read it too - or config/symbol_addrs_<target>.txt with --target);
    an existing entry of OLD is renamed instead,
    a name that is only a fixed-address name (config/addrs.txt: LIT / LITP / ADDR_) gets no symbol entry,
  - OLD is replaced in src/, include/, the header specs config/structs, config/locals, config/enemy_work,
    config/addrs.txt (and in the other symbol files): plain uses, the per-function
    alias variants (`OLD_l`, `OLD_a1`, ...), `__asm__("OLD")` strings and comments.
Then `make split` regenerates asm/ with the new names, the programs are rebuilt from scratch and
all 13 must still match the originals; otherwise every changed file is restored.
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
IDENT = re.compile(r"^[A-Za-z_]\w*$")
ADDR_NAME = re.compile(r"^(?:func|D|jtbl)_([0-9A-F]{8})$")


def read_pairs(args):
    pairs = []
    for a in args.pairs:
        old, new = a.split("=", 1)
        pairs.append((old.strip(), new.strip()))
    if args.file:
        for line in open(args.file):
            line = line.split("//")[0].strip().rstrip(";")
            if line and not line.startswith("#"):
                old, new = line.split("=", 1)
                pairs.append((old.strip(), new.strip()))
    return pairs


def source_files():
    out = []
    for top in ("src", "include"):
        for d, _, files in os.walk(top):
            out += [os.path.join(d, f) for f in files if f.endswith((".c", ".h"))]
    # the specs the generated headers (include/re1_views.h, re1_locals.h) are made from
    for top in (os.path.join("config", "structs"), os.path.join("config", "locals"),
                os.path.join("config", "enemy_work")):
        out += [os.path.join(top, f) for f in os.listdir(top) if f.endswith(".txt")]
    # the fixed-address names (LIT / LITP / ADDR_, include/re1_addrs.h is generated from it)
    out.append(os.path.join("config", "addrs.txt"))
    return out


def fixed_address_of(old):
    """Address of a name that is only a fixed-address name (config/addrs.txt), or None."""
    m = re.search(rf"^0x([0-9A-Fa-f]+)\s+{re.escape(old)}\b", open(os.path.join("config", "addrs.txt")).read(), re.M)
    return int(m.group(1), 16) if m else None


def symbol_files():
    return [os.path.join("config", f) for f in os.listdir("config")
            if f.startswith("symbol_addrs") and f.endswith(".txt")]


def address_of(old, sym_files):
    m = ADDR_NAME.match(old)
    if m:
        return int(m.group(1), 16)
    for p in sym_files:
        m = re.search(rf"^{re.escape(old)}\s*=\s*0x([0-9A-Fa-f]+)", open(p).read(), re.M)
        if m:
            return int(m.group(1), 16)
    return None


def run_make(cmd):
    r = subprocess.run(["sh", BUILDENV, cmd], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pairs", nargs="*", help="OLD=NEW")
    ap.add_argument("--file")
    ap.add_argument("--target", default="main")
    args = ap.parse_args()
    pairs = read_pairs(args)
    if not pairs:
        sys.exit(__doc__)

    srcs, syms = source_files(), symbol_files()
    own = os.path.join("config", "symbol_addrs.txt" if args.target == "main"
                       else f"symbol_addrs_{args.target}.txt")
    texts = {p: open(p).read() for p in srcs + syms}
    backup = dict(texts)

    def in_scope(p):
        # main symbols are visible everywhere; an overlay symbol only in that overlay's own files
        # (+ headers and the main/psyq symbol files it links with) - the same name may then be used
        # for the twin function in another overlay (each overlay links separately)
        if args.target == "main":
            return True
        q = p.replace(os.sep, "/")
        return (q.startswith(f"src/{args.target}/") or q.startswith("include/")
                or q.endswith(("symbol_addrs.txt", "symbol_addrs_psyq.txt", f"symbol_addrs_{args.target}.txt")))
    everything = "\n".join(v for p, v in texts.items() if in_scope(p))

    skipped = 0
    for old, new in pairs:
        # a bad pair is skipped (with a message), the rest of a batch still goes ahead
        problem = None
        if not IDENT.match(new):
            problem = f"not a C identifier: {new}"
        elif re.search(rf"\b{re.escape(new)}\b", everything):
            problem = f"{new} is already used somewhere"
        elif not re.search(rf"\b{re.escape(old)}\b", everything):
            problem = f"{old} not found (already renamed?)"
        addr = None if problem else address_of(old, syms)
        addr_only = False
        if not problem and addr is None:
            addr = fixed_address_of(old)
            addr_only = addr is not None
        if not problem and addr is None:
            problem = f"address of {old} unknown"
        if problem:
            print(f"SKIPPED {old} -> {new}: {problem}")
            skipped += 1
            continue
        kind = "func" if old.startswith("func_") or "type:func" in everything.split(old + " =", 1)[-1][:80] else "data"
        # (also ADDR_OLD: the fixed-address form, include/re1_addrs.h)
        pat = re.compile(rf"(?:(?<=\bADDR_)|(?<![\w])){re.escape(old)}(?=\b|_)")
        # 1. symbol files: rename an existing entry everywhere it is listed
        listed = False
        for p in syms:
            if args.target != "main" and p != own:
                continue
            # (also OLD_suffix symbols: the sources rename those too, e.g. g_ImageFade_W with g_ImageFade)
            new_text, n = re.subn(rf"^{re.escape(old)}((?:_\w+)?\s*=)", rf"{new}\1", texts[p], flags=re.M)
            if n:
                texts[p], listed = new_text, listed or p == own
        if not listed and not addr_only:
            # splat knows the types func/jtbl/label only; a data symbol has no type
            typ = " // type:func" if kind == "func" else ""
            texts[own] = texts[own].rstrip("\n") + f"\n{new} = 0x{addr:08X};{typ}\n"
        # 2. sources (main symbols: every program; overlay symbols: that overlay only)
        for p in srcs:
            # (headers and specs too: a generated view macro may name an overlay symbol, 2026-10-16)
            q = p.replace(os.sep, "/")
            if args.target != "main" and not q.startswith((f"src/{args.target}/", "include/", "config/")):
                continue
            texts[p] = pat.sub(new, texts[p])
        everything = "\n".join(v for p, v in texts.items() if in_scope(p))
        print(f"{old} -> {new} (0x{addr:08X}, {kind})")

    changed = [p for p in texts if texts[p] != backup[p]]
    for p in changed:
        open(p, "w", newline="\n").write(texts[p])
    # splat writes NEW.s but never deletes OLD.s: stale per-function files made tools/twins.py see
    # every renamed function twice (2026-10-16) - remove them before re-splitting
    import glob
    for old, new in pairs:
        for stale in glob.glob(os.path.join("asm", "*", "nonmatchings", "*", old + ".s")):
            os.remove(stale)
    print(f"{len(pairs) - skipped} renamed, {skipped} skipped; {len(changed)} files changed; "
          "re-splitting and rebuilding...")
    rc, log = run_make("make split")
    if rc == 0:
        rc, log = run_make("make clean && make -j8")
    if rc == 0 and "All 13 executables match" in log:
        print("OK: all 13 executables still match")
        return
    print("FAILED - restoring:\n" + "\n".join(l for l in log.splitlines()
                                             if "rror" in l or "MISMATCH" in l or "undefined" in l)[-3000:])
    for p in changed:
        open(p, "w", newline="\n").write(backup[p])
    run_make("make split")
    rc, log = run_make("make clean && make -j8")
    print("restored;", "build matches" if "All 13 executables match" in log else "BUILD STILL BROKEN")
    sys.exit(1)


if __name__ == "__main__":
    main()
