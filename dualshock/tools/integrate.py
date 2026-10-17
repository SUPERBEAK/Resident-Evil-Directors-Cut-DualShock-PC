"""integrate.py - move auto-decompiled functions into a target's C file, verified by the build.

Run from dualshock/ with the project venv python:
    python tools/integrate.py [--target main|stage1|...] [--status match] [--only func_A ...]

Takes m2c output from build/auto/<target>/c/<f>.c (made by tools/autodecomp.py) for functions
whose status in build/auto/<target>/results.json is `match` (or the given --only list), and:
  1. turns each output into a self-contained definition: m2c's file-scope declarations become
     block-scope declarations at the top of the function body (GCC 2.6.3 only warns about
     conflicting block-scope types, and every function keeps the types it matched with);
     a declaration of a function that is already *defined* earlier in the file is dropped
     (redeclaring after a conflicting definition is an error),
  2. replaces the INCLUDE_ASM line with it,
  3. builds the target and checks it byte-for-byte. On a compile error the function at the
     error line is reverted; on a mismatch the first differing integrated function is
     reverted. Repeats until the build matches again (or restores the file on failure).
"""

import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compiler_matrix as cm  # noqa: E402  (target_info)

ROOT = os.path.abspath("..")
BUILDENV = os.path.join(ROOT, "tools", "buildenv.sh")
NM = os.path.join(ROOT, "tools", "external", "mips-toolchain", "bin", "mips64-elf-nm")
# A function definition line at column 0 (any name: functions get real names in the readability pass).
DEF_RE = re.compile(r"^(?!typedef\b|struct\b|union\b|enum\b)[A-Za-z_][\w \*]*?\b(\w+)\s*\(.*\)\s*\{\s*$")


class Target:
    def __init__(self, name):
        # `main/door` = program main, C file src/main/door.c (the main program is split into topic
        # files since 2026-10-16); a plain program name keeps its single (first) C file
        name, _, cfile = name.partition("/")
        self.orig, self.base, func_dir, self.cfile = cm.target_info(name)
        if cfile:
            self.cfile = os.path.join("src", name, cfile + ".c")
            func_dir = os.path.join("asm", name, "nonmatchings", cfile)
        self.folder = func_dir.replace(os.sep, "/")
        self.make = "slus_007.47" if name == "main" else name
        self.exe = os.path.join("build", "SLUS_007.47" if name == "main" else name.upper() + ".EXE")
        self.auto = os.path.join("build", "auto", name)

    def inc(self, name):
        return f'INCLUDE_ASM("{self.folder}", {name});'


FILE_PROTOS = set()
FILE_VOID = set()     # functions the file defines as `void` (a draft that saw a value-returning
                      # prototype keeps it through an alias: v0 "used" changes -O2 register allocation)

# Name searches and renames must skip string literals: a hand-made draft declares its globals as
# block-scope aliases, `extern void *D_X_l __asm__("D_X");`, and the "D_X" inside the string is
# not a use. Treating it as one added a second, conflicting declaration and then renamed the
# string itself (`__asm__("D_X_Entityp")`, an undefined symbol) - stage4 func_80125EAC, 2026-10-15.
STR_RE = re.compile(r'"(?:\\.|[^"\\\n])*"' r"|'(?:\\.|[^'\\\n])*'")
ASM_ALIAS_RE = re.compile(r'__asm__\s*\(\s*"(\w+)"\s*\)\s*;\s*$')


def code_only(text):
    """text with the contents of string/char literals blanked (same length)."""
    return STR_RE.sub(lambda m: m.group(0)[0] + " " * (len(m.group(0)) - 2) + m.group(0)[-1], text)


def sub_code(pattern, repl, text):
    """re.sub applied outside string/char literals only."""
    out, pos = [], 0
    for m in STR_RE.finditer(text):
        out.append(re.sub(pattern, repl, text[pos:m.start()]))
        out.append(m.group(0))
        pos = m.end()
    out.append(re.sub(pattern, repl, text[pos:]))
    return "".join(out)


def declared_names(lines):
    """Names declared by declaration lines (file-scope draft declarations or the function's own
    block-scope ones): the declared identifier, and for an `__asm__("SYM")` alias also SYM."""
    names = set()
    for d in lines:
        s = d.strip()
        if not s.endswith(";"):
            continue
        am = ASM_ALIAS_RE.search(s)
        if am:
            names.add(am.group(1))
            s = s[:am.start()].rstrip()
        else:
            s = s[:-1].rstrip()
        m = (re.search(r"\b(\w+)\s*\((?:[^()]|\([^()]*\))*\)$", s)      # function: name(params)
             or re.search(r"\(\s*\*\s*(\w+)\s*\)\s*\(.*\)$", s)            # pointer to function
             or re.search(r"\b(\w+)\s*(?:\[[^\]]*\])*$", s))               # variable / array
        if m:
            names.add(m.group(1))
    return names


def convert(name, code, defined, file_types=None):
    decls, body, in_body = [], [], False
    renames = {}
    # Top-level typedefs / #defines of the draft stay at file scope, in front of the function
    # (a struct type used in the signature cannot be declared inside the body).
    prelude, depth, in_define = [], 0, False
    for line in code.splitlines():
        if not in_body and (depth > 0 or in_define or line.startswith(("typedef", "#define"))):
            prelude.append(line)
            depth += line.count("{") - line.count("}")
            in_define = line.rstrip().endswith("\\") and (in_define or line.startswith("#define"))
            continue
        if not in_body:
            m = DEF_RE.match(line)
            if m and m.group(1) == name:
                in_body = True
                body.append(line)
                continue
            s = line.strip()
            if not s or s.startswith("/*"):
                continue
            fm = re.match(r"^.*?\b(\w+)\s*\(.*\);", s)
            if "/* extern */" in s and fm and (fm.group(1) in defined or fm.group(1) in FILE_PROTOS):
                pm = re.match(rf"^(.*?\b){fm.group(1)}\s*\((.*)\);", s)
                value_vs_void = pm and fm.group(1) in FILE_VOID and pm.group(1).strip() != "void"
                if pm and (pm.group(2).strip() not in ("", "void") or value_vs_void):
                    # a prototyped declaration the draft matched with (Capcom's caller saw these
                    # parameter types, e.g. s32 where the definition has s16): keep it as a
                    # private alias of the same symbol, calls renamed
                    alias = f"{fm.group(1)}_p"
                    decls.append(f"    {pm.group(1)}{alias}({pm.group(2)}) __asm__(\"{fm.group(1)}\");")
                    renames[fm.group(1)] = alias
                continue                      # callee already defined above: use that
            vm = re.match(r"^extern [\w ]+\b(func_[0-9A-F]{8});$", s)
            if vm and vm.group(1) in defined:
                continue                      # function used as an address (`&func_X`): defined above
            decls.append("    " + s.replace("/* extern */", "").rstrip())
        else:
            body.append(line)
    if not body:
        return None
    for old_name, alias in renames.items():
        body[1:] = [sub_code(r"\b" + old_name + r"(\s*\()", alias + r"\1", b) for b in body[1:]]
    # Drafts made with `autodecomp.py --psyq` rely on Sony prototypes (m2c then doesn't
    # declare Sony functions itself): copy the ones this function calls into its body.
    # (Hand-made drafts declare what they use inside the body - those count as declared too.)
    declared = declared_names(decls) | declared_names(body[1:])
    text = code_only("\n".join(body))
    for fname, proto in PSYQ_PROTOS.items():
        if fname not in declared and re.search(rf"(?<![\w.>]){fname}\s*\(", text):
            decls.append("    " + proto)
    # Typed Capcom globals (re1_externs.h) used by drafts made with that context.
    for vname, decl in RE1_EXTERNS.items():
        if vname not in declared and re.search(rf"(?<![\w.]){vname}\b", text):
            decls.append("    " + decl)
    # A global the file already declares with another type: GCC 2.6.3 keeps the earlier
    # type ("type mismatch with previous external decl" is only a warning), so the code
    # changes. Capcom's source files declared shared globals differently; here the function
    # gets a private alias of the same symbol with its own type (an asm label).
    if file_types is not None:
        for i, d in enumerate(decls):
            m = VAR_DECL_RE.match(d.strip())
            if not m:
                continue
            ty, vname, arr = m.group(1), m.group(2), m.group(3)
            key = norm_decl(ty, arr)
            others = file_types.get(vname, set())
            if others and key not in others:
                alias = f"{vname}_{re.sub(r'[^A-Za-z0-9]', '', ty.replace('*', 'p'))}"
                decls[i] = f'    extern {ty} {alias}{arr} __asm__("{vname}");'
                body[1:] = [sub_code(rf"(?<![\w.]){vname}\b", alias, b) for b in body[1:]]
            else:
                file_types.setdefault(vname, set()).add(key)
    return "\n".join(prelude + ([""] if prelude else []) + [body[0]] + decls + body[1:])


VAR_DECL_RE = re.compile(r"^extern\s+([^;()]*?)\s*\b(D_[0-9A-F]{8}\w*)\s*((?:\[[^\]]*\])?)\s*;$")


def norm_decl(ty, arr):
    return " ".join(ty.replace("*", " * ").split()) + ("[]" if arr else "")


def extern_types(text):
    """name -> set of normalized types of every `extern T D_X;` declaration in the file."""
    types = {}
    for line in text.splitlines():
        m = VAR_DECL_RE.match(line.strip())
        if m:
            types.setdefault(m.group(2), set()).add(norm_decl(m.group(1), m.group(3)))
    return types


def load_re1_externs():
    """name -> `extern T name;` line, from include/re1_externs.h."""
    text = re.sub(r"/\*.*?\*/", "", open(os.path.join("include", "re1_externs.h")).read(), flags=re.S)
    return {m.group(1): " ".join(m.group(0).split())
            for m in re.finditer(r"^extern [^;(]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", text, re.M)}


RE1_EXTERNS = load_re1_externs()


def load_psyq_protos():
    """name -> one-line prototype, from include/psyq_protos.h."""
    text = re.sub(r"/\*.*?\*/", "", open(os.path.join("include", "psyq_protos.h")).read(), flags=re.S)
    protos = {}
    for stmt in re.findall(r"^[A-Za-z][^;#{}]*?\([^;{}]*\);", text, re.M):
        one = " ".join(stmt.split())
        m = re.search(r"(\w+)\s*\((?:[^()]|\([^()]*\))*\);$", one)
        if m:
            protos[m.group(1)] = one
    return protos


PSYQ_PROTOS = load_psyq_protos()


def function_spans(text):
    """[(start_line, end_line, name)] of C function definitions (1-based)."""
    spans, lines = [], text.splitlines()
    i = 0
    while i < len(lines):
        m = DEF_RE.match(lines[i])
        if not m and re.match(r"^[A-Za-z_][\w \*]*?\b\w+\s*\([^;{]*,\s*$", lines[i]):
            # a signature continued on the next line(s) (main func_8005CE44)
            joined, j = lines[i], i
            while j + 1 < len(lines) and j - i < 5 and not joined.rstrip().endswith(("{", ";")):
                j += 1
                joined += " " + lines[j].strip()
            m = DEF_RE.match(joined)
        if m:
            j = i
            while j < len(lines) and lines[j] != "}":
                j += 1
            spans.append((i + 1, j + 1, m.group(1)))
            i = j
        i += 1
    return spans


RODATA_PAD = ('\n/* Capcom\'s source file aligned this jump table to 8 within its own .rodata: the 4 zero\n'
              '   bytes after it (see docs: rodata of the stage overlays). */\n'
              '__asm__(".section .rodata\\n\\t.word 0\\n\\t.text");\n')


def needs_rodata_pad(asm_path):
    """The original jump table of this (overlay) function ends with a zero word inside its own
    `jtbl` block: Capcom's file padded the table to 8 bytes. Our overlay tables are `.align 2`
    (Makefile alignfix_for), so the draft must emit that word itself (stage1 func_8011CF24,
    stage3 func_8011A74C)."""
    try:
        asm = open(asm_path).read()
    except OSError:
        return False
    # only the function's LAST table: a pad after an earlier table sits between two tables and
    # must be emitted inside the body, before the next switch (title func_800E07BC)
    blks = re.findall(r"dlabel jtbl_\w+\n(.*?)enddlabel", asm, re.S)
    if blks:
        words = re.findall(r"\.word (\S+)", blks[-1])
        if words and words[-1] == "0x00000000":
            return True
    return False


NARROW_RE = re.compile(r"\b(?:s8|u8|s16|u16|char|short)\b")


def alias_old_decls(text, name):
    """`name` is now defined with narrow prototyped parameters (`s8 f(s16 a, u16 b) {`). Older
    functions that declare it themselves (`s8 f();`, `s8 f(s16, s16);`) would make GCC 2.6.3
    call a local `f.NNN` (link error) or, without the declaration, change their argument code.
    Give each such caller a private alias of the same symbol with its own declaration and rename
    its calls - their code stays as it was (2026-10-13, stage1 func_80118418).
    Done for every definition, not only narrow ones: a caller's own `M2C_UNK f(M2C_UNK, u16);`
    also conflicts with `void f(s32, s32)` and its code must not change (2026-10-14, main
    func_8003726C, func_8002B8A4)."""
    dm = re.search(rf"^\S[^\n]*?\b{name}\s*\(([^)]*)\)\s*\{{\s*$", text, re.M)
    if not dm:
        return text, 0
    lines = text.split("\n")
    n = 0
    for start, end, fn in function_spans(text):
        if fn == name:
            continue
        for k in range(start, end):
            m = re.match(rf"^(\s+)((?:extern\s+)?[A-Za-z_][\w \*]*?)\s*\b{name}\s*\(([^;{{}}]*)\);\s*$", lines[k])
            if not m or re.search(r"\breturn\b|=", m.group(2)):
                continue
            n += 1
            alias = f"{name}_a{n}"
            lines[k] = f'{m.group(1)}{m.group(2).strip()} {alias}({m.group(3)}) __asm__("{name}");'
            for q in range(k + 1, end):
                lines[q] = re.sub(rf"\b{name}(\s*\()", alias + r"\1", lines[q])
            break
    return "\n".join(lines), n


def write_file(path, text):
    """Write with retries: Windows editors/antivirus lock files for a moment."""
    import time
    for i in range(20):
        try:
            open(path, "w", newline="\n").write(text)
            return
        except PermissionError:
            time.sleep(0.5 * (i + 1))
    open(path, "w", newline="\n").write(text)


def build(t):
    # Always compile for real: delete the target's objects first. (A C file rewritten within
    # the file system's timestamp resolution looked unchanged to make, so a stale object was
    # linked and a broken file passed as "matches" - 2026-10-13.)
    stem = os.path.splitext(os.path.relpath(t.cfile, "src"))[0]
    for ext in (".i", ".s", ".ms.s", ".o"):
        try:
            os.remove(os.path.join("build", "src", stem + ext))
        except OSError:
            pass
    # Retry transient Windows file locks (antivirus scanning freshly written files).
    for _ in range(5):
        r = subprocess.run(["sh", BUILDENV, f"make check-{t.make}"], capture_output=True, text=True)
        log = r.stdout + r.stderr
        if "Device or resource busy" not in log and "Permission denied" not in log:
            break
    return r.returncode == 0, log


def defined_names(text):
    return {n for _, _, n in function_spans(text)}


def first_bad(t, names):
    """First (by address) of `names` whose built bytes differ from the original."""
    nm = subprocess.run([NM, "-S", "--defined-only", os.path.join("build", f"{t.make}.elf")],
                        capture_output=True, text=True).stdout
    syms = {p[3]: (int(p[0], 16), int(p[1], 16)) for p in (l.split() for l in nm.splitlines())
            if len(p) == 4}
    orig = open(t.orig, "rb").read()
    built = open(t.exe, "rb").read()
    for n in sorted(names, key=lambda n: syms.get(n, (0,))[0]):
        if n not in syms:
            return n
        a, s = syms[n]
        o = a - t.base + 0x800
        if orig[o:o + s] != built[o:o + s]:
            return n
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--status", default="match")
    ap.add_argument("--only", nargs="*")
    args = ap.parse_args()
    t = Target(args.target)
    res_path = os.path.join(t.auto, "results.json")
    results = json.load(open(res_path))
    text = open(t.cfile).read()
    names = args.only or sorted(n for n, s in results.items() if s == args.status)
    names = [n for n in names if t.inc(n) in text]
    print(f"{args.target}: integrating {len(names)} functions")
    backup = text

    # address order, so "defined earlier" is known while converting
    order = re.findall(r'INCLUDE_ASM\("' + re.escape(t.folder) + r'", (\w+)\);'
                       r'|^\S[^\n]*?\b(func_[0-9A-F]{8}|main)\s*\([^;\n]*\{', text, re.M)
    defined, todo, codes = set(), set(names), {}
    file_types = extern_types(text)
    # functions with a prototype at file scope (a draft's own prototype of them conflicts too)
    FILE_PROTOS.clear()
    FILE_PROTOS.update(re.findall(r"^[A-Za-z_][\w \*]*?\b(func_[0-9A-F]{8})\s*\([^;{]*\);", text, re.M))
    FILE_VOID.clear()
    FILE_VOID.update(re.findall(r"^(?:static\s+)?void\s+(func_[0-9A-F]{8})\s*\([^;]*\)\s*\{", text, re.M))
    for inc_name, def_name in order:
        if def_name:
            defined.add(def_name)
        elif inc_name in todo:
            c = convert(inc_name, open(os.path.join(t.auto, "c", f"{inc_name}.c")).read(), defined,
                        file_types)
            if c and args.target != "main":
                need = needs_rodata_pad(os.path.join(t.folder, inc_name + ".s"))
                has = '__asm__(".section .rodata' in c
                if need and not has:
                    c = c.rstrip("\n") + "\n" + RODATA_PAD
                    print(f"  {inc_name}: added the jump-table pad word after it")
                elif has and not need:      # copied from a twin whose file padded (propagate.py)
                    c2 = c.replace(RODATA_PAD.strip("\n"), "").rstrip("\n") + "\n"
                    if c2.strip() != c.strip():     # (a pad *inside* the body is a leading pad - kept)
                        c = c2
                        print(f"  {inc_name}: removed a jump-table pad word its original doesn't have")
            if c:
                codes[inc_name] = c
                defined.add(inc_name)
    for n, c in codes.items():
        text = text.replace(t.inc(n), c, 1)
    for n in codes:
        text, k = alias_old_decls(text, n)
        if k:
            print(f"  {n}: {k} older caller declaration(s) turned into private aliases")
    active = set(codes)
    repaired = set()
    err_re = re.compile(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): (?!warning)")
    for _ in range(400):
        write_file(t.cfile, text)
        ok, log = build(t)
        if ok:
            print(f"OK: {len(active)} functions integrated, build matches")
            json.dump(results, open(res_path, "w"), indent=1, sort_keys=True)   # (reverted ones marked)
            return
        errs = [int(m.group(1)) for m in err_re.finditer(log)]
        culprit = None
        # A newly defined function conflicts with an older function's block-scope declaration
        # of it (`M2C_UNK f();` vs `void f(void) {`): retype that declaration (unprototyped,
        # definition's return type) - the full build verifies the old function still matches.
        cm_ = re.search(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): conflicting types for `(\w+)'", log)
        if cm_ and (cm_.group(2) in active or cm_.group(2) in defined_names(text)):
            name, line_no = cm_.group(2), int(cm_.group(1))
            lines = text.split("\n")
            dm = re.match(rf"^(\s+).*?\b{name}\s*\(.*\);\s*$", lines[line_no - 1])
            ret = re.search(rf"^([A-Za-z_][\w \*]*?)\s*\b{name}\s*\([^;\n]*\)\s*\{{\s*$", text, re.M)
            if dm and ret and f"repair:{line_no}" not in repaired:
                repaired.add(f"repair:{line_no}")
                params = re.search(rf"\b{name}\s*\(([^)]*)\)\s*\{{\s*$", text, re.M)
                if params and params.group(1).strip() not in ("", "void"):
                    # a prototyped definition with (narrow) parameters: an unprototyped
                    # redeclaration is incompatible and GCC 2.6.3 then calls a *local*
                    # `name.NNN` - drop the declaration, the definition's prototype is in scope
                    del lines[line_no - 1]
                    print(f"  dropped old declaration of {name} (line {line_no})")
                else:
                    lines[line_no - 1] = f"{dm.group(1)}{ret.group(1).strip()} {name}();"
                    print(f"  retyped old declaration of {name} (line {line_no})")
                text = "\n".join(lines)
                continue
        # An older function declares a newly defined function as a variable
        # (`extern M2C_UNK func_X;`, m2c's form when only its address is used): make that a
        # function declaration - the address is the same.
        rk = re.search(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): `(\w+)' redeclared as different kind of symbol", log)
        if rk and rk.group(2) in active and f"redecl:{rk.group(1)}" not in repaired:
            line_no, name = int(rk.group(1)), rk.group(2)
            lines = text.split("\n")
            dm = re.match(rf"^(\s+)extern [\w ]+\b{name};\s*$", lines[line_no - 1])
            if dm:
                repaired.add(f"redecl:{line_no}")
                lines[line_no - 1] = f"{dm.group(1)}void {name}();"
                text = "\n".join(lines)
                print(f"  retyped old variable declaration of {name} (line {line_no})")
                continue
        # A new function calls an older `T f(void)` with arguments (Capcom's callers often had
        # no prototype in scope): define f K&R-style `T f()` - same code, any call allowed.
        ta = re.search(re.escape(t.cfile.replace(os.sep, "/")) + r":\d+: too many arguments to function `(\w+)'", log)
        if ta and f"kr:{ta.group(1)}" not in repaired:
            name = ta.group(1)
            new_text = re.sub(rf"^(\S[^\n]*\b{name})\(void\)(\s*\{{)$", r"\1()\2", text, count=1, flags=re.M)
            if new_text != text:
                repaired.add(f"kr:{name}")
                text = new_text
                print(f"  made the definition of {name} K&R-style (called with arguments)")
                continue
        if errs:
            spans = function_spans(text)
            for err_line in errs:
                for s, e, n in spans:
                    if s <= err_line <= e and n in active:
                        culprit = n
                        break
                if culprit:
                    break
            if culprit is None:
                print("compile error outside integrated functions:")
                lines = text.split("\n")
                for ln in sorted(set(errs))[:10]:
                    msg = re.search(re.escape(t.cfile.replace(os.sep, "/")) + rf":{ln}: (?!warning)(.*)", log)
                    print(f"  line {ln}: {msg.group(1) if msg else '?'}\n      {lines[ln - 1].strip()}")
                break
        elif "MISMATCH" in log:
            culprit = first_bad(t, active)
        else:
            print("build failed:\n", log[-3000:])
            break
        if culprit is None:
            print("could not attribute the failure")
            break
        why = "mismatch"
        if errs:
            em = re.search(re.escape(t.cfile.replace(os.sep, "/")) + r":\d+: (?!warning)(.*)", log)
            why = em.group(1) if em else "compile error"
        print(f"  reverting {culprit} ({why})")
        text = text.replace(codes[culprit], t.inc(culprit), 1)
        active.discard(culprit)
        results[culprit] = results.get(culprit, "") + ",integrate-fail"
    else:
        print("too many attempts")
    print("restoring the original file")
    write_file(t.cfile, backup)
    json.dump(results, open(res_path, "w"), indent=1, sort_keys=True)
    sys.exit(1)


if __name__ == "__main__":
    main()
