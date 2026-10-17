"""autodecomp.py - bulk first-pass decompilation with m2c + automatic match checking.

Run from dualshock/ with the project venv python:
    python tools/autodecomp.py [--target main|stage1|...] [--jobs 8] [--only func_A ...]
                               [--flags "-O2 -G0"] [--gcc 2.6.3] [--psyq]

For every function still pulled in by INCLUDE_ASM in the target's C file:
  1. m2c (--valid-syntax) turns asm/<t>/nonmatchings/<t>/<f>.s into C (the target's shared
     rodata files - every non-function .s in that folder - are passed along for jump tables),
  2. the C is compiled ALONE with the real toolchain (gcc-<ver>-psx -> maspsx -> as),
  3. its .text is compared with the original bytes (address fields of relocated words are
     ignored, like tools/compiler_matrix.py).
Results: build/auto/<target>/results.json (status per function) and the m2c output in
build/auto/<target>/c/<f>.c. Status: match / diff:N / size:BUILT/ORIG / m2c-fail / cc-fail.
Defaults per target come from TARGET_FLAGS (main: -O2 -G0; overlays: see below).
"""

import argparse
import concurrent.futures as cf
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compiler_matrix as cm  # noqa: E402  (build/text_and_syms/compare/target_info)

M2C = os.path.join(cm.VENV_BIN, "m2c" + cm.EXE)
M2C_EXTRA = []                   # extra m2c flags (--m2c)
TARGET_FLAGS = {"main": "-O2 -G0"}
DEFAULT_OVERLAY_FLAGS = "-O0 -G0"   # overlays: every function keeps $fp; -O0 matches (M3)


def out_dir(target):
    return os.path.join("build", "auto", target)


def pending_functions(cfile, folder):
    text = open(cfile).read()
    return re.findall(rf'INCLUDE_ASM\("{re.escape(folder)}", (\w+)\);', text)


def orig_size(func_dir, name):
    m = re.search(rf"^nonmatching {name},\s*(0x[0-9A-Fa-f]+)",
                  open(os.path.join(func_dir, name + ".s")).read(), re.M)
    return int(m.group(1), 16) if m else None


def func_addr(func_dir, name):
    m = re.search(rf"glabel {name}\n\s*/\* [0-9A-F]+ ([0-9A-F]{{8}})",
                  open(os.path.join(func_dir, name + ".s")).read())
    return int(m.group(1), 16)


def write_retry(path, text):
    """Write a file, retrying briefly on transient Windows locks (antivirus scans)."""
    import time
    for i in range(20):
        try:
            with open(path, "w", newline="\n") as f:
                f.write(text)
            return
        except PermissionError:
            time.sleep(0.25)
    raise PermissionError(path)


def add_return(name, code):
    """-O0: an explicit `return;` at the end of a void function emits `j <epilogue>`."""
    m = re.search(rf"^void {name}\(.*\)\s*\{{\s*$", code, re.M)
    if not m:
        return None
    i = code.rindex("\n}")
    if code[:i].rstrip().endswith("return;"):
        return None
    return code[:i] + "\n    return;" + code[i:]


def void_deref_fix(line):
    """`*(void *)ADDR` -> an access whose width fits the address' alignment (odd: byte,
    2 mod 4: halfword, else word; refine tries other widths). A word access at an odd address
    crashed GCC 2.6.3's optimizer (func_800171EC, 2026-10-13); other `*(void *)` -> 32-bit."""
    def fix(m):
        a = int(m.group(1), 16)
        t = "u8" if a & 1 else "s16" if a & 2 else "s32"
        return f"*({t} *)0x{m.group(1)}"
    line = re.sub(r"\*\(void \*\)0x([0-9A-Fa-f]+)", fix, line)
    return align_fix(line.replace("*(void *)", "*(s32 *)"))


def align_fix(text):
    """Constant-address accesses wider than their address' alignment (m2c writes some, e.g.
    `*(s32 *)0x1F80038E`) -> the widest access the alignment allows. They crash cc1 at -O1+."""
    def fix(m):
        t, a = m.group(1), int(m.group(2), 16)
        size = {"8": 1, "16": 2, "32": 4}[t[1:]]
        if a % size == 0:
            return m.group(0)
        nt = ("u8" if a & 1 else t[0] + "16")
        return f"*({nt} *)0x{m.group(2)}"
    return re.sub(r"\*\(([su](?:8|16|32)) \*\)0x([0-9A-Fa-f]+)\b", fix, text)


def kr_decls(name, code):
    """Fix-ups for m2c output that doesn't compile:
    * unprototyped (K&R) extern function declarations with `s32` instead of `void` returns
      (m2c guesses prototypes per call site: 'too few arguments', 'void value not ignored');
      old C code often called functions without prototypes anyway,
    * functions m2c declared as variables (`extern M2C_UNK func_X;`, 'called object is not
      a function') become function declarations,
    * dereferences of untyped constant addresses `*(void *)0x1F800018` (scratchpad) become
      32-bit accesses (the byte comparison catches it if the original used another size)."""
    out, changed = [], False
    # Function-pointer table dispatch (`table[p->state](...)`), which m2c writes as
    # `*((IDX * 4) + &D_X)(...)` or `*(&D_X + (IDX * 4))(...)`: index a real array of
    # unprototyped function pointers instead.
    tables = set()

    def _tab(m, idx, tab):
        tables.add(tab)
        return f"{tab}[{idx}]("
    # (tables may also be mislabeled func_X by splat, e.g. &func_8008F89C)
    code2 = re.sub(r"\*\(\((.+?) \* 4\) \+ &((?:D|func)_[0-9A-F]{8})\)\(", lambda m: _tab(m, m.group(1), m.group(2)), code)
    code2 = re.sub(r"\*\(&((?:D|func)_[0-9A-F]{8}) \+ \((.+?) \* 4\)\)\(", lambda m: _tab(m, m.group(2), m.group(1)), code2)
    # (without `&`: D_X is a pointer that holds the table's address)
    code3 = re.sub(r"\*\(\((.+?) \* 4\) \+ (D_[0-9A-F]{8})\)\(", r"((s32 (**)())\2)[\1](", code2)
    if code3 != code2:
        changed, code, code2 = True, code3, code3
    if tables:
        changed = True
        code = code2
        for tab in tables:
            code = re.sub(rf"^extern [^;\n]*\b{tab};\s*$", f"extern s32 (*{tab}[])();", code, flags=re.M)
            code = re.sub(rf"^[^\n]*\b{tab}\([^\n]*\);\s*/\* extern \*/.*$", f"extern s32 (*{tab}[])();",
                          code, flags=re.M)
            code = re.sub(rf"&{tab}\b(?!\[)", tab, code)       # leftover `&D_X` args -> the table
            # `D_X[i](D_X)`: the "argument" is only the table address left in $a0 by the
            # address computation (m2c reads it as an argument); the call has none.
            # (-O0: the target then goes through $s0 - verified byte-identical, 2026-10-12)
            code = re.sub(rf"\b{tab}\[([^\]]+)\]\({tab}\)", rf"{tab}[\1]()", code)
    for line in code.splitlines():
        fv = re.match(r"^extern M2C_UNK (func_[0-9A-F]{8});\s*$", line)
        if fv:
            line, changed = f"s32 {fv.group(1)}();", True
        # Function pointers (e.g. the overlays' engine-call table at 0x801FE9xx): unprototyped.
        fp = re.match(r"^extern (.+?)\(\*(\w+)\)\((.+)\);\s*$", line)
        if fp and fp.group(3).strip() not in ("", "void"):
            line, changed = f"extern {fp.group(1)}(*{fp.group(2)})();", True
        if "*(void *)" in line and "/* extern */" not in line:
            line, changed = void_deref_fix(line), True
        if "/* extern */" in line:
            m = re.match(r"^(.*?)\b(\w+)\((.*)\);(\s*/\* extern \*/.*)$", line)
            if m:
                ret = m.group(1).strip() or "s32"
                if ret == "void":
                    ret = "s32"
                new = f"{ret} {m.group(2)}();{m.group(4)}"
                changed |= new != line
                line = new
        out.append(line)
    return "\n".join(out) + "\n" if changed else None


def errfix(name, code, a, rounds=10):
    """Error-driven repair of a draft that doesn't compile. Uses the compiler's messages:
      `X' undeclared            -> declare m2c's stack/saved-register placeholder as a local
      invalid type argument of `unary *' -> give that dereference an explicit (s32 *) cast
    Returns the repaired code (or None if nothing could be changed)."""
    func_dir, shared, context, orig, base, flags, gcc, odir = a
    cur = align_fix(code)
    changed = cur != code
    for i in range(rounds):
        work = os.path.join(odir, "work", name, f"errfix{i}")
        os.makedirs(work, exist_ok=True)
        pre = cm.preamble_for(cur)
        src = os.path.join(work, f"{name}.c")
        write_retry(src, pre + cur)
        try:
            cm.build(src, gcc, flags, work)
            return cur if changed else None
        except SystemExit as e:
            msg = str(e)
        off = pre.count("\n")                       # preamble lines before the draft
        lines = cur.split("\n")
        fixed = False
        und = sorted(set(re.findall(r"`(\w+)' undeclared", msg)))
        if und:
            m = re.search(rf"^[^\n]*\b{name}\([^\n]*\{{\n", cur, re.M)
            if m:
                decl = "".join(f"    {'s32 ' + v + '[0x40]' if re.search(rf'\b{v}\[', cur) else 's32 ' + v};\n"
                               for v in und)
                cur = cur[:m.end()] + decl + cur[m.end():]
                fixed = True
        else:
            for ln in sorted({int(x) for x in re.findall(r"\.c:(\d+): invalid type argument of `unary \*'", msg)},
                             reverse=True):
                k = ln - 1 - off
                if 0 <= k < len(lines):
                    # `**(T *)(X)`: X holds a pointer -> `**(T **)(X)`
                    new = re.sub(r"\*\*\(((?:[us](?:8|16|32))) \*\)", r"**(\1 **)", lines[k], count=1)
                    if new == lines[k]:
                        new = re.sub(r"(?<![\w)\]])\*\((?!\s*\(?\s*(?:[us](?:8|16|32)|void|M2C_UNK\w*)\s*\*)",
                                     "*(s32 *)(", lines[k], count=1)
                    if new == lines[k]:
                        # `*0xADDR`: a bare constant used as a pointer
                        new = re.sub(r"(?<![\w)\]])\*0x([0-9A-Fa-f]+)\b", r"*(s32 *)0x\1", lines[k])
                    if new == lines[k]:
                        # `*var` on an integer variable (m2c typed a pointer as u32/s32)
                        new = re.sub(r"(?<![\w)\]])\*(?=[A-Za-z_]\w*\b(?!\s*\())", "*(s32 *)", lines[k], count=1)
                    if new != lines[k]:
                        lines[k], fixed = new, True
            # pointer subtraction between unrelated types (`*p - p`, `x - M2C_FIELD(q, T **, o)`):
            # compute with integers on that line
            ptr_locals = set(re.findall(r"^\s+[\w ]+?\*+\s*(\w+);", "\n".join(lines), re.M))
            for ln in sorted({int(x) for x in re.findall(r"\.c:(\d+): invalid operands to binary [-+]", msg)}):
                k = ln - 1 - off
                if not 0 <= k < len(lines):
                    continue
                new = re.sub(r"(M2C_[SP]?FIELD\([^,]+, )\w+ \*\*(, )", r"\1s32 *\2", lines[k])
                new = re.sub(r"(?<=[-+] )(?!\(s32\) )\b(" + "|".join(map(re.escape, ptr_locals)) + r")\b", r"(s32) \1",
                             new) if ptr_locals else new
                if new != lines[k]:
                    lines[k], fixed = new, True
            # a stack struct (m2c typed it from the Sony prototypes) used as a number on this
            # line: m2c means its first word -> `(*(s32 *)&spXX)` there (the struct stays)
            for ln in sorted({int(x) for x in re.findall(r"\.c:(\d+): (?:aggregate value used where an integer "
                                                          r"was expected|incompatible types in assignment)", msg)}):
                k = ln - 1 - off
                if not 0 <= k < len(lines):
                    continue
                for v in set(re.findall(r"\b(sp[0-9A-F]+)\b", lines[k])):
                    if any(re.match(rf"^\s+(?!s8\b|u8\b|s16\b|u16\b|s32\b|u32\b|void\b|M2C)[A-Za-z_]\w* {v};(?:\s*/\*.*\*/)?$", l)
                           for l in lines):
                        new = re.sub(rf"(?<![&\w.]){v}\b(?!\s*[.\[])", f"(*(s32 *)&{v})", lines[k])
                        if new != lines[k]:
                            lines[k], fixed = new, True
            # `*(void *)ADDR` used as a value -> a 32-bit access (refine tries other widths)
            for ln in sorted({int(x) for x in re.findall(r"\.c:(\d+): (?:invalid use of void expression|"
                                                          r"void value not ignored as it ought to be)", msg)},
                             reverse=True):
                k = ln - 1 - off
                if not 0 <= k < len(lines):
                    continue
                if "*(void *)" in lines[k]:
                    lines[k], fixed = void_deref_fix(lines[k]), True
                    continue
                # `*((E * N) + p)` with p a void pointer: an N-byte element (refine tries others)
                m = re.search(r"(?<![\w)\]])\*\(\((.+?) \* ([124])\) \+ ", lines[k])
                t = {"1": "s8", "2": "s16", "4": "s32"}[m.group(2)] if m else "s32"
                new = re.sub(r"(?<![\w)\]])\*\((?!\s*\(?\s*(?:[us](?:8|16|32)|void|M2C_UNK\w*)\s*\*)",
                             f"*({t} *)(", lines[k], count=1)
                if new != lines[k]:
                    lines[k], fixed = new, True
            if fixed:
                cur = "\n".join(lines)
        if not fixed:
            return cur if changed else None
        changed = True
    return cur if changed else None


RANK = ["match", "diff", "size", "cc-fail", "m2c-fail"]


def rank(status):
    return RANK.index(status.split(":")[0])


def better_or_equal(a, b):
    """Is status `a` at least as good as status `b`? (match > diff:N (smaller N) > size > cc-fail)"""
    ra, rb = rank(a), rank(b)
    if ra != rb:
        return ra < rb
    if a.startswith("diff:") and b.startswith("diff:"):
        return int(a.split(":")[1]) <= int(b.split(":")[1])
    return True


def check(name, code, a, tag):
    """Compile `code` alone and compare with the original. Returns a status string."""
    func_dir, shared, context, orig, base, flags, gcc, odir = a
    work = os.path.join(odir, "work", name, tag)
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, f"{name}.c")
    # Includes + only those Sony prototypes / typed globals the draft doesn't declare itself.
    write_retry(src, cm.preamble_for(code) + code)
    try:
        o = cm.build(src, gcc, flags, work)
    except SystemExit as e:
        return "cc-fail", "\n/* " + str(e)[:2000].replace("*/", "* /") + " */"
    text, syms, relocs = cm.text_and_syms(o)
    if name not in syms:
        return "cc-fail", ""
    off, size = syms[name]
    want = orig_size(func_dir, name)
    if size != want:
        return f"size:{size:#x}/{want:#x}", ""
    n = cm.compare(text, off, size, relocs, orig, func_addr(func_dir, name), base=base)
    return ("match" if n == 0 else f"diff:{n}"), ""


def run_one(name, a):
    """Decompile one function; its scratch folder is deleted afterwards (see refine.py)."""
    try:
        return _run_one(name, a)
    finally:
        shutil.rmtree(os.path.join(a[7], "work", name), ignore_errors=True)


def _run_one(name, a):
    func_dir, shared, context = a[0], a[1], a[2]
    cmd = [M2C, "-t", "mips-gcc-c", "--valid-syntax", *M2C_EXTRA]
    if context:
        cmd += ["--context", context]
    cmd += [os.path.join(func_dir, name + ".s"), *shared]
    r = subprocess.run(cmd, capture_output=True, text=True, env=cm.ENV)
    code = r.stdout
    if r.returncode or "Decompilation failure" in code or not code.strip():
        return name, "m2c-fail", code + r.stderr
    # Variants: base m2c output; K&R declarations if it doesn't compile; plus an explicit
    # `return;` for every variant that compiles. Keep the first match, else the best status.
    status, err = check(name, code, a, "base")
    if status == "match":
        return name, status, code
    best = (status, code, err)
    pool = [("base", code, status)]
    if status == "cc-fail":
        v = kr_decls(name, code)
        if v:
            st, e = check(name, v, a, "kr")
            if st == "match":
                return name, st, v
            pool.append(("kr", v, st))
            if rank(st) < rank(best[0]):
                best = (st, v, e)
        else:
            v, st = code, status          # (no K&R changes: repair the base output itself)
        if True:
            if st == "cc-fail":
                v2 = errfix(name, v, a)
                if v2:
                    st, e = check(name, v2, a, "errfix")
                    if st == "match":
                        return name, st, v2
                    pool.append(("errfix", v2, st))
                    if rank(st) < rank(best[0]):
                        best = (st, v2, e)
    # m2c emits *byte* offsets on arrays of unknown type (`&D_X + off`), but M2C_UNK is s32
    # in valid-syntax mode: declare those as bytes (M2C_UNK8) as an extra variant.
    for tag, src, st in list(pool):
        v = re.sub(r"^extern M2C_UNK (\w+);", r"extern M2C_UNK8 \1;", src, flags=re.M)
        if v != src:
            st2, e2 = check(name, v, a, tag + "+unk8")
            if st2 == "match":
                return name, st2, v
            pool.append((tag + "+unk8", v, st2))
            if rank(st2) < rank(best[0]):
                best = (st2, v, e2)
    for tag, src, st in pool:
        if st == "cc-fail":
            continue
        v = add_return(name, src)
        if v:
            st2, e2 = check(name, v, a, tag + "+return")
            if st2 == "match":
                return name, st2, v
            if rank(st2) < rank(best[0]):
                best = (st2, v, e2)
    return name, best[0], best[1] + best[2]


def make_psyq_context():
    """m2c can't parse preprocessor directives: flatten basic typedefs + psyq_protos.h with
    our cpp -P into build/m2c_context.h. Drafts made with it are compiled with
    `#include "psyq_protos.h"`; integrate.py copies the prototypes into function bodies."""
    src = os.path.join("build", "m2c_context_src.h")
    out = os.path.join("build", "m2c_context.h")
    with open(src, "w", newline="\n") as f:
        f.write("typedef signed char s8;\ntypedef unsigned char u8;\ntypedef short s16;\n"
                "typedef unsigned short u16;\ntypedef int s32;\ntypedef unsigned int u32;\n"
                "typedef long long s64;\ntypedef unsigned long long u64;\n"
                '#include "psyq_protos.h"\n#include "re1_externs.h"\n')
    cpp = os.path.join(cm.TOOLS, "old-gcc-build", "gcc-2.6.3-psx", "cpp" + cm.EXE)
    cm.run([cpp, "-P", "-undef", "-nostdinc", "-Iinclude", "-D__GNUC__=2", src, out])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--psyq", action="store_true",
                    help="give m2c the Sony prototypes (include/psyq_protos.h) as context")
    ap.add_argument("--flags")
    ap.add_argument("--gcc", default="2.6.3")
    ap.add_argument("--m2c", default="", help="extra m2c flags, e.g. \"--reg-vars s0,s1 --no-andor\" "
                    "(a stored draft is only replaced by a better one)")
    args = ap.parse_args()
    exe, base, func_dir, cfile = cm.target_info(args.target)
    flags = args.flags or TARGET_FLAGS.get(args.target, DEFAULT_OVERLAY_FLAGS)
    odir = out_dir(args.target)
    os.makedirs(os.path.join(odir, "c"), exist_ok=True)
    shared = sorted(os.path.join(func_dir, f) for f in os.listdir(func_dir)
                    if f.endswith(".s") and not f.startswith("func_") and f != "main.s")
    names = args.only or pending_functions(cfile, func_dir.replace(os.sep, "/"))
    orig = open(exe, "rb").read()
    res_path = os.path.join(odir, "results.json")
    results = json.load(open(res_path)) if os.path.exists(res_path) else {}
    context = make_psyq_context() if args.psyq else None
    M2C_EXTRA[:] = args.m2c.split()
    pack = (func_dir, shared, context, orig, base, flags, args.gcc, odir)
    done = 0
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        futs = [ex.submit(run_one, n, pack) for n in names]
        for fut in cf.as_completed(futs):
            name, status, code = fut.result()
            old = results.get(name, "").split(",")[0]
            if old and better_or_equal(old, status):
                # keep a previous draft that was at least as good (e.g. improved by refine.py)
                continue
            results[name] = status
            write_retry(os.path.join(odir, "c", f"{name}.c"), code)
            done += 1
            if done % 100 == 0:
                print(f"  {done}/{len(names)}", flush=True)
    json.dump(results, open(res_path, "w"), indent=1, sort_keys=True)
    summary = {}
    for n in names:
        k = results[n].split(":")[0]
        summary[k] = summary.get(k, 0) + 1
    print(f"{args.target} ({flags}, gcc {args.gcc}):", summary)


if __name__ == "__main__":
    main()
