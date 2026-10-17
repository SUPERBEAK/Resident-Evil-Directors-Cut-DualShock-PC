"""declclean.py - move the per-function `extern` declarations of globals to file scope.

Run from dualshock/ with the project venv python:
    python tools/declclean.py --target main        (or logo, stage1, ...)

During matching every function declared the globals it uses inside its own body, often as a
private alias (`extern s16 D_X_l __asm__("D_X");`) because Capcom's source files declared shared
globals with different types. This tool, per C file:
  1. picks each global's canonical type: the type of its FIRST plain block-scope declaration in the
     file (GCC 2.6.3 keeps the first type for all later plain declarations - "type mismatch with
     previous external decl" is only a warning - so that is the type every plain use compiled with),
     or, for a global that is only ever aliased, its most common alias type,
  2. declares it once at file scope (a block after the #include lines), sorted by address,
  3. removes the plain declarations and the aliases with the canonical type from the functions and
     renames those aliases back to the real name; aliases with another type stay,
  4. rebuilds; functions whose bytes changed get their old text back, until all 13 match.
Types that use a typedef defined inside the C file (e.g. `Rect8_800514E8`) are left alone.
"""
import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402

VAR_ALIAS = re.compile(r'^(\s+)extern\s+([^;()]*?)\s*\b(\w+)\s*((?:\[[^\]]*\])*)\s*__asm__\("(\w+)"\);\s*$')
VAR_PLAIN = re.compile(r'^(\s+)extern\s+([^;()]*?)\s*\b(\w+)\s*((?:\[[^\]]*\])*)\s*;\s*$')
FILE_PLAIN = re.compile(r'^extern\s+([^;()]*?)\s*\b(\w+)\s*((?:\[[^\]]*\])*)\s*;\s*$')
# any variable declaration `extern <anything> NAME[...];` (also inline struct types; not prototypes)
EXTERN_ANY = re.compile(r'^\s*extern\b.*?\b(\w+)\s*((?:\[[^\]]*\])*)\s*;\s*$')
# function-pointer variables: `extern void (**D_X)(void);`, `extern s32 (*D_X[])();` (left alone)
FUNCPTR = re.compile(r"^\s*extern\b[^;]*\(\s*\*+\s*(\w+)\s*((?:\[[^\]]*\])*)\s*\)\s*\([^;]*\)\s*;\s*$")
BLOCK_BEGIN = "/* Globals used in this file (tools/declclean.py; per-function aliases with other types remain) */"


def norm(ty, arr):
    return " ".join(ty.replace("*", " * ").split()), "".join(arr.split())   # full suffix: [][6] stays


def decl_text(ty, name, arr):
    stars = ty.count("*")
    base = " ".join(w for w in ty.split() if w != "*")
    return f"extern {base} {'*' * stars}{name}{arr};"


def known_types():
    """typedef/struct names declared in include/*.h (usable at file scope before any function)."""
    names = {"void", "char", "short", "int", "long", "unsigned", "signed", "const", "volatile", "struct"}
    for f in os.listdir("include"):
        if f.endswith(".h"):
            t = open(os.path.join("include", f)).read()
            names |= set(re.findall(r"\btypedef\b[^;]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", t, re.S))
            names |= set(re.findall(r"}\s*(\w+)\s*;", t))
    return names


def analyse(text, types_ok):
    lines = text.split("\n")
    spans = ig.function_spans(text)
    defined = {n for _, _, n in spans}     # functions declared like variables (address uses) stay
    plain = collections.defaultdict(collections.Counter)
    alias_count = collections.defaultdict(collections.Counter)
    filescope, complex_syms = {}, set()
    for line in lines[:spans[0][0] - 1] if spans else []:
        m = FILE_PLAIN.match(line)
        if m:
            filescope.setdefault(m.group(2), norm(m.group(1), m.group(3)))
            continue
        m = EXTERN_ANY.match(line) or FUNCPTR.match(line)
        if m:
            complex_syms.add(m.group(1))
    for s, e, _ in spans:
        for k in range(s, e):
            m = VAR_ALIAS.match(lines[k])
            if m:
                alias_count[m.group(5)][norm(m.group(2), m.group(4))] += 1
                continue
            m = VAR_PLAIN.match(lines[k])
            if m:
                plain[m.group(3)][norm(m.group(2), m.group(4))] += 1
                continue
            m = EXTERN_ANY.match(lines[k]) or FUNCPTR.match(lines[k])
            if m and "__asm__" not in lines[k]:
                complex_syms.add(m.group(1))     # e.g. an inline `struct { ... } *X;`: leave X alone
    canon = {}
    for sym in set(plain) | set(alias_count) | set(filescope):
        if sym in complex_syms:
            continue
        c = (filescope.get(sym) or (plain[sym].most_common(1)[0][0] if plain[sym] else None)
             or alias_count[sym].most_common(1)[0][0])
        if (set(re.findall(r"\w+", c[0])) <= types_ok and re.match(r"^\w+$", sym)
                and sym not in defined and not sym.startswith("func_")):
            canon[sym] = c
    return lines, spans, canon, set(filescope)


def alias_name(sym, ty, arr):
    """`D_X` + a suffix from its type: s8 -> D_X_s8, u8 * -> D_X_u8p, u8 [] -> D_X_u8a."""
    words = [w for w in ty.split() if w != "*"]
    return f"{sym}_{''.join(words)}{'p' * ty.count('*')}{'a' if arr else ''}"


def transform_function(lines, s, e, canon, full=True):
    """New text of lines[s:e] (0-based end-exclusive slice of one function).
    full=True: drop declarations with the canonical type (the file-scope one replaces them) and
    rename canonical-type aliases to the real name. Always: a plain declaration with ANOTHER type
    becomes a private alias of that type (GCC 2.6.3 compiles each function with its own declared
    type; next to a file-scope declaration a different plain type would be an error)."""
    body, renames = [], {}
    for k in range(s, e):
        line = lines[k]
        m = VAR_ALIAS.match(line)
        if m and full and canon.get(m.group(5)) == norm(m.group(2), m.group(4)):
            renames[m.group(3)] = m.group(5)
            continue
        m = VAR_PLAIN.match(line)
        if m and m.group(3) in canon and k > s:
            sym, ty = m.group(3), norm(m.group(2), m.group(4))
            if ty == canon[sym]:
                if full:
                    continue
            else:
                a = alias_name(sym, *ty)
                renames[sym] = a
                stars = ty[0].count("*")
                base = " ".join(w for w in ty[0].split() if w != "*")
                line = f'{m.group(1)}extern {base} {"*" * stars}{a}{ty[1]} __asm__("{sym}");'
        body.append(line)
    out = []
    for line in body:          # (renames skip string literals and can't match inside D_X_s8)
        for old, new in renames.items():
            line = ig.sub_code(r"(?<![\w.])" + re.escape(old) + r"(?!\w)", new, line)
        out.append(line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    a = ap.parse_args()
    t = ig.Target(a.target)
    original = open(t.cfile).read()
    if BLOCK_BEGIN in original:
        sys.exit("already done for this file")
    lines, spans, canon, filescope = analyse(original, known_types())
    print(f"{a.target}: {len(canon)} globals get a file-scope declaration")

    def addr(sym):
        m = re.search(r"([0-9A-F]{8})", sym)
        return (0, int(m.group(1), 16)) if m else (1, sym)

    block = [BLOCK_BEGIN] + [decl_text(ty, sym, arr) for sym, (ty, arr) in
                             sorted(canon.items(), key=lambda kv: addr(kv[0])) if sym not in filescope]
    # insert after the last #include line at the top
    inc = max(i for i, l in enumerate(lines[:200]) if l.startswith("#include"))
    new_funcs = {n: transform_function(lines, s - 1, e, canon) for s, e, n in spans}
    old_funcs = {n: transform_function(lines, s - 1, e, canon, full=False) for s, e, n in spans}

    def assemble(keep_old):
        out, pos = lines[:inc + 1] + [""] + block, inc + 1
        for s, e, n in spans:
            out += lines[pos:s - 1]
            out += old_funcs[n] if n in keep_old else new_funcs[n]
            pos = e
        return "\n".join(out + lines[pos:])

    keep_old = set()
    for attempt in range(30):
        ig.write_file(t.cfile, assemble(keep_old))
        ok, log = ig.build(t)
        if ok:
            print(f"OK: {a.target} builds and matches; {len(keep_old)} function(s) kept their old declarations")
            return
        errs = re.findall(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): (?!warning)(.*)", log)
        if errs:
            ig.write_file(os.path.join("build", "declclean_failed.c"), open(t.cfile).read())
            print("compile errors (file kept as build/declclean_failed.c), restoring:")
            for ln, msg in errs[:10]:
                print(f"  line {ln}: {msg}")
            break
        bad = bad_functions(t, [n for _, _, n in spans if n not in keep_old])
        if not bad:
            print("build failed for another reason, restoring:\n" + log[-2000:])
            break
        print(f"  {len(bad)} function(s) changed bytes, keeping their old text: {' '.join(bad[:8])}{' ...' if len(bad) > 8 else ''}")
        keep_old |= set(bad)
    ig.write_file(t.cfile, original)
    ig.build(t)
    sys.exit(1)


def original_sizes(t):
    """{function: size} from splat's `nonmatching NAME, 0xSIZE` lines in asm/<target>/."""
    sizes = {}
    folder = os.path.dirname(t.folder)
    for d, _, fs in os.walk(folder):
        for f in fs:
            if f.endswith(".s"):
                for m in re.finditer(r"^nonmatching (\w+), (0x[0-9A-Fa-f]+)", open(os.path.join(d, f)).read(), re.M):
                    sizes[m.group(1)] = int(m.group(2), 16)
    return sizes


def bad_functions(t, names):
    """The functions of `names` that changed. If any function's SIZE changed, only those are
    returned: everything after a size change moves, so a byte comparison would flag all of it.
    Otherwise all functions whose bytes differ from the original."""
    import subprocess
    nm = subprocess.run([ig.NM, "-S", "--defined-only", os.path.join("build", f"{t.make}.elf")],
                        capture_output=True, text=True).stdout
    syms = {p[3]: (int(p[0], 16), int(p[1], 16)) for p in (l.split() for l in nm.splitlines()) if len(p) == 4}
    sizes = original_sizes(t)
    resized = [n for n in names if n in syms and n in sizes and syms[n][1] != sizes[n]]
    if resized:
        return resized
    orig, built = open(t.orig, "rb").read(), open(t.exe, "rb").read()
    out = []
    for n in names:
        if n in syms:
            a, s = syms[n]
            o = a - t.base + 0x800
            if orig[o:o + s] != built[o:o + s]:
                out.append(n)
    return out


if __name__ == "__main__":
    main()
