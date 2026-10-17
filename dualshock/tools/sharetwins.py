"""sharetwins.py - compile the enemy code that several stages contain from ONE shared source file.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python, after the stages were
split into per-enemy files (tools/splitc.py --target stageN):
    python tools/sharetwins.py

Capcom linked the same enemy source into every stage overlay that uses that enemy (zombie code is in
six stages). For every src/stageN/<enemy>.c that exists in two or more stages:
  - src/shared/<enemy>.c gets the code once (each stage's text is tried; the one most stages
    match wins); its INCLUDE_RODATA folders become
    `OVERLAY_ASM "/<enemy>"`,
  - each stage's <enemy>.c becomes a small wrapper: its own includes, globals.h, the prototypes it saw,
    `#define OVERLAY_ASM "asm/stageN/nonmatchings"`, `#include "../shared/<enemy>.c"` and its own rodata
    padding line (tools/splitc.py --pad) if it had one,
  - all programs are rebuilt; a stage whose bytes change with the shared text keeps its own copy
    (reported), and a shared file used by fewer than two stages is undone.
"""
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import declclean as dc  # noqa: E402
import integrate as ig  # noqa: E402

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
PAD_TAG = "/* rodata padding up to the next file's original start (tools/splitc.py --pad) */"
WRAP_RE = re.compile(r'#define OVERLAY_ASM "asm/(stage\d)/nonmatchings"\n#include "\.\./shared/(\w+)\.c"\n')


def full_text(p):
    """The complete text of src/stageN/<e>.c; a wrapper is expanded with its shared file (so the tool
    can run again on a tree where enemies are already shared)."""
    t = open(p).read()
    m = WRAP_RE.search(t)
    if not m:
        return t
    st, e = m.groups()
    shared = open(os.path.join("src", "shared", e + ".c")).read()
    body = shared.split("*/\n\n", 1)[1]                  # drop the shared file's own top comment
    body = body.replace(f'INCLUDE_RODATA(OVERLAY_ASM "/{e}"', f'INCLUDE_RODATA("asm/{st}/nonmatchings/{e}"')
    rest = t[m.end():].strip("\n")
    return t[:m.start()].rstrip("\n") + "\n\n" + body.rstrip("\n") + ("\n\n" + rest if rest else "") + "\n"


def globals_decls(stage):
    """{name: declaration} of src/<stage>/globals.h: (type, array suffix) for plain variables, the
    line itself for anything else (function pointers, inline struct types: those can't be aliased)."""
    out = {}
    for line in open(os.path.join("src", stage, "globals.h")).read().split("\n"):
        m = dc.FILE_PLAIN.match(line)
        if m:
            out[m.group(2)] = dc.norm(m.group(1), m.group(3))
            continue
        m = dc.EXTERN_ANY.match(line) or dc.FUNCPTR.match(line)
        if m:
            out[m.group(1)] = " ".join(line.split())
    return out


def localize(body, canon_stage, stages):
    """Each stage's globals.h has the types of ITS first declarations, so a global can have another type
    in another stage (D_800C51AE: u8 in stage1, s8 in stage2). In the shared text, every global whose
    declaration differs between `stages` gets a private alias with the canonical stage's type in each
    function that uses it (`extern u8 D_800C51AE_u8 __asm__("D_800C51AE");`, the form declclean uses)."""
    canon = globals_decls(canon_stage)
    others = [globals_decls(s) for s in stages if s != canon_stage]
    differ = sorted(s for s, d in canon.items() if isinstance(d, tuple) and any(o.get(s) != d for o in others))
    if not differ:
        return body, []
    text = "\n".join(body)
    lines = body[:]
    for s, e, _ in reversed(ig.function_spans(text)):
        k = s - 1
        while not lines[k].rstrip().endswith("{"):        # a signature continued on the next line(s)
            k += 1
        func = lines[k + 1:e]
        code = "\n".join(func)
        decls = []
        for sym in differ:
            if not re.search(r"(?<![\w.])" + re.escape(sym) + r"(?!\w)", ig.code_only(code)):
                continue
            ty, arr = canon[sym]
            a = dc.alias_name(sym, ty, arr)
            stars = ty.count("*")
            base = " ".join(w for w in ty.split() if w != "*")
            decls.append(f'    extern {base} {"*" * stars}{a}{arr} __asm__("{sym}");')
            func = [l for l in func if not (dc.VAR_PLAIN.match(l) and dc.VAR_PLAIN.match(l).group(3) == sym)]
            func = [ig.sub_code(r"(?<![\w.])" + re.escape(sym) + r"(?!\w)", a, l) for l in func]
        if decls:
            lines[k + 1:e] = decls + func
    return lines, differ


def split_header(text):
    """(header lines, body lines, pad lines) of a file written by tools/splitc.py."""
    lines = text.rstrip("\n").split("\n")
    i = lines.index('#include "globals.h"') + 1
    while i < len(lines) and (lines[i] == "" or lines[i].startswith("/* Types used") or
                              lines[i].startswith("/* Functions defined") or
                              lines[i].startswith("   functions saw")):
        if lines[i].startswith("/* Types used"):
            i += 1
            depth = 0
            while i < len(lines):                       # the copied typedef blocks
                depth += lines[i].count("{") - lines[i].count("}")
                if not lines[i].startswith(("typedef", " ", "\t", "}")) and depth <= 0:
                    break
                i += 1
            continue
        if lines[i].startswith("/* Functions defined"):
            i += 2
            while i < len(lines) and lines[i].strip().endswith(");") and not lines[i].startswith(" "):
                i += 1
            continue
        i += 1
    header, body = lines[:i], lines[i:]
    pad = []
    if PAD_TAG in body:
        k = body.index(PAD_TAG)
        pad, body = body[k:k + 2], body[:k]
    while body and body[-1] == "":
        body.pop()
    return header, body, pad


def build_status():
    """{program: True/False} after `make -k` (a program that fails to compile or match is False)."""
    r = subprocess.run(["sh", BUILDENV, "make clean && make -k -j8"], capture_output=True, text=True)
    log = r.stdout + r.stderr
    ok = {m.lower().split(".")[0]: True for m in re.findall(r"^OK\s+build/(\S+)", log, re.M)}
    for m in re.findall(r"^MISMATCH\s+build/(\S+)", log, re.M):
        ok[m.lower().split(".")[0]] = False
    # compile failures: make's own "*** [... build/src/stageN/x.o] Error" lines (compiler messages from
    # parallel jobs interleave and can't be parsed reliably)
    for st in re.findall(r"\*\*\* \[[^\]]*build/src/(stage\d)/\w+\.o\] Error", log):
        ok[st] = False
    return ok


def stage_of(p):
    return p.replace(os.sep, "/").split("/")[1]


def try_shared(e, paths, originals, canon):
    """Write src/shared/<e>.c from `canon` and wrappers for all `paths`; return {stage: matched}."""
    _, body, _ = split_header(originals[canon])
    st0 = stage_of(canon)
    body, _ = localize(body, st0, [stage_of(p) for p in paths])
    body = [re.sub(r'INCLUDE_RODATA\("asm/' + st0 + r'/nonmatchings/' + e + '"',
                   f'INCLUDE_RODATA(OVERLAY_ASM "/{e}"', l) for l in body]
    open(os.path.join("src", "shared", e + ".c"), "w", newline="\n").write(
        f"/* {e}.c - {e} code shared by stage overlays (tools/sharetwins.py). Capcom linked one source into\n"
        f"   every stage that has this enemy; each such stage compiles this file through its own\n"
        f"   src/stageN/{e}.c wrapper, which provides that stage's declarations and OVERLAY_ASM. */\n\n"
        + "\n".join(body) + "\n")
    for p in paths:
        header, _, pad = split_header(originals[p])
        wrapper = header + ["", f'#define OVERLAY_ASM "asm/{stage_of(p)}/nonmatchings"',
                            f'#include "../shared/{e}.c"'] + ([""] + pad if pad else [])
        text = re.sub(r"\n{3,}", "\n\n", "\n".join(wrapper))       # no runs of empty lines
        open(p, "w", newline="\n").write(text + "\n")
    status = build_status()
    return {stage_of(p): status.get(stage_of(p), False) for p in paths}


def main():
    enemies = {}
    for p in sorted(glob.glob(os.path.join("src", "stage*", "*.c"))):
        name = os.path.basename(p)[:-2]
        if name != "entry":
            enemies.setdefault(name, []).append(p)
    enemies = {e: ps for e, ps in enemies.items() if len(ps) >= 2}
    os.makedirs(os.path.join("src", "shared"), exist_ok=True)
    for e, paths in enemies.items():
        originals = {p: full_text(p) for p in paths}
        best = None
        for canon in paths:                    # try each stage's text as the shared one
            res = try_shared(e, paths, originals, canon)
            good = [p for p in paths if res[stage_of(p)]]
            print(f"  {e} from {stage_of(canon)}: matches in {[stage_of(p) for p in good]}")
            if best is None or len(good) > len(best[1]):
                best = (canon, good)
            if len(good) == len(paths):
                break
        canon, good = best
        for p in paths:                        # start from the originals again
            open(p, "w", newline="\n").write(originals[p])
        if len(good) >= 2:
            res = try_shared(e, good, originals, canon)
            if not all(res.values()):
                for p in good:
                    open(p, "w", newline="\n").write(originals[p])
                os.remove(os.path.join("src", "shared", e + ".c"))
                print(f"{e}: not shared (the combination did not match)")
            else:
                print(f"{e}: shared by {[stage_of(p) for p in good]} (text of {stage_of(canon)}); own copies: "
                      f"{[stage_of(p) for p in paths if p not in good]}")
        else:
            os.remove(os.path.join("src", "shared", e + ".c"))
            print(f"{e}: not shared (no two stages match one text)")
    final = build_status()
    print("final:", "all 13 match" if all(final.values()) and len(final) == 13 else f"PROBLEM {final}")


if __name__ == "__main__":
    main()
