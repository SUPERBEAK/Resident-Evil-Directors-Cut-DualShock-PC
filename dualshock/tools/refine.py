"""refine.py - hill-climbing search that mutates non-matching C drafts until they match.

Run from dualshock/ with the project venv python:
    python tools/refine.py --target stage1 [--status size,diff] [--only f ...] [--evals 300] [--jobs 8]

For each function (default: status size/diff in build/auto/<target>/results.json) it starts
from the best m2c variant in build/auto/<target>/c/<f>.c and repeatedly tries single
mutations, keeping one whenever the compiled code gets closer to the original:
  * a field access M2C_FIELD -> explicit struct form (M2C_SFIELD) or pointer form (M2C_PFIELD)
  * add / remove a trailing `return;` (-O0 emits `j <epilogue>` for an explicit return)
  * remove one cast like `(u8) `
  * flip signedness of one type (s8/u8, s16/u16, s32/u32) in a field access or declaration
  * inline a single-use `temp_*` variable (at -O0 every local is a stack slot)
  * merge `a = b; b(...)` into `(a = b)(...)`
Distance: each candidate is compiled with the real toolchain, LINKED at the function's
original address (overlay/main symbols from the built ELF + splat's auto symbol files, rodata
at its original address), and its words are diffed against the original bytes.
A distance of 0 = byte-identical; the draft is saved and marked `match` (integrate.py then
verifies it in the full build).
"""

import argparse
import concurrent.futures as cf
import difflib
import json
import os
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import autodecomp as ad  # noqa: E402
import compiler_matrix as cm  # noqa: E402

BIN = cm.BIN


def make_name(target):
    return "slus_007.47" if target == "main" else target


def symbol_script(target, out):
    """Linker script assigning every known symbol its original address."""
    mk = make_name(target)
    nm = cm.run([os.path.join(BIN, "mips64-elf-nm"), "--defined-only", os.path.join("build", f"{mk}.elf")])
    syms = {}
    for line in nm.splitlines():
        p = line.split()
        if len(p) == 3 and re.match(r"^[A-Za-z_]\w*$", p[2]) and not p[2].endswith("_START") \
                and not p[2].endswith("_END") and not p[2].endswith("_SIZE") and not p[2].endswith("_VRAM"):
            syms[p[2]] = int(p[0], 16)
    for f in (f"{mk}_undefined_syms_auto.txt", f"{mk}_undefined_funcs_auto.txt"):
        for line in open(os.path.join("build", f)):
            m = re.match(r"(\w+) = (0x[0-9A-Fa-f]+);", line)
            if m:
                syms.setdefault(m.group(1), int(m.group(2), 16))
    with open(out, "w") as f:
        for n, a in sorted(syms.items()):
            f.write(f"PROVIDE({n} = 0x{a:08X});\n")
    return out


def rodata_addr(func_dir, name):
    text = open(os.path.join(func_dir, name + ".s")).read()
    if ".section .rodata" not in text.split("glabel")[0]:
        return None
    m = re.search(r"/\* [0-9A-F]+ ([0-9A-F]{8}) ", text)
    return int(m.group(1), 16) if m else None


class Ctx:
    def __init__(self, target):
        self.target = target
        self.exe, self.base, self.func_dir, self.cfile = cm.target_info(target)
        self.orig = open(self.exe, "rb").read()
        self.flags = ad.TARGET_FLAGS.get(target, ad.DEFAULT_OVERLAY_FLAGS)
        self.dir = os.path.join("build", "refine", target)
        os.makedirs(self.dir, exist_ok=True)
        self.syms = symbol_script(target, os.path.join(self.dir, "syms.ld"))


def distance(ctx, name, code, tag):
    """Edit distance (in words) between the linked candidate and the original; None = no compile."""
    work = os.path.join(ctx.dir, "work", name, tag)
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, f"{name}.c")
    # Includes + only those Sony prototypes / typed globals the draft doesn't declare itself.
    ad.write_retry(src, cm.preamble_for(code) + code)
    try:
        o = cm.build(src, os.environ.get("RE_GCC", "2.6.3"), ctx.flags, work)
    except SystemExit:
        return None
    addr = ad.func_addr(ctx.func_dir, name)
    ro = rodata_addr(ctx.func_dir, name)
    ld = os.path.join(work, "place.ld")
    with open(ld, "w") as f:
        f.write("SECTIONS {\n"
                f"  .text 0x{addr:08X} : {{ *(.text) }}\n"
                + (f"  .rodata 0x{ro:08X} : {{ *(.rodata) *(.rdata) }}\n" if ro else
                   "  .rodata : { *(.rodata) *(.rdata) }\n")
                + "  .data : { *(.data) *(.sdata) }\n  .bss : { *(.bss) *(.sbss) *(COMMON) }\n"
                  "  /DISCARD/ : { *(*) }\n}\n")
    elf = os.path.join(work, "m.elf")
    r = subprocess.run([os.path.join(BIN, "mips64-elf-ld"), "-EL", "-m", "elf32ebmip", "-nostdlib",
                        "--no-check-sections", "--unresolved-symbols=ignore-all", "-T", ctx.syms,
                        "-T", ld, "-o", elf, o], capture_output=True, text=True, env=cm.ENV)
    if r.returncode:
        return None
    got = cm.read_elf(elf)[0][".text"][1]       # in-process instead of objcopy (faster)
    osz = ad.orig_size(ctx.func_dir, name)
    o0 = addr - ctx.base + 0x800
    want = ctx.orig[o0:o0 + osz]
    a = [struct.unpack_from("<I", want, i)[0] for i in range(0, len(want), 4)]
    b = [struct.unpack_from("<I", got, i)[0] for i in range(0, len(got) - len(got) % 4, 4)]
    exact = word_distance(a, b)
    if exact == 0:
        return 0
    # Score with branch/jump targets masked: a fix that changes the code size somewhere moves every
    # later branch target, and counting those made real improvements look worse (2026-10-13).
    # Only byte-identical code scores 0.
    return max(1, word_distance([mask_branch(w) for w in a], [mask_branch(w) for w in b]))


def mask_branch(w):
    """The instruction word without its branch/jump target (BEQ/BNE/BLEZ/BGTZ/REGIMM offsets,
    J targets; JAL kept: the callee matters)."""
    op = w >> 26
    if op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
        return w & 0xFFFF0000
    if op == 2:
        return w & 0xFC000000
    return w


def word_distance(a, b):
    d = 0
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if op != "equal":
            d += max(i2 - i1, j2 - j1)
    return d


# ---------------------------------------------------------------- mutations
def inline_all(code):
    """Repeatedly inline temp_/var_ variables that are declared, assigned once, used once."""
    c2 = code
    for _ in range(40):
        changed = False
        for v in sorted(set(re.findall(r"\b((?:temp|var)_\w+)\b", c2))):
            uses = list(re.finditer(rf"\b{v}\b", c2))
            assign = re.search(rf"^(\s*){v} = (.+);\n", c2, re.M)
            decl = re.search(rf"^\s*[\w\s\*]+\b{v};\n", c2, re.M)
            if assign and decl and len(uses) == 3:
                expr = assign.group(2)
                c3 = c2[:assign.start()] + c2[assign.end():]
                c3 = re.sub(rf"^\s*[\w\s\*]+\b{v};\n", "", c3, count=1, flags=re.M)
                c2 = re.sub(rf"\b{v}\b", f"({expr})", c3, count=1)
                changed = True
                break
        if not changed:
            break
    return c2


def forward_subst(code):
    """`v = EXPR;` immediately followed by `LHS = v;` -> `LHS = EXPR;` (v removed if unused)."""
    c2 = code
    for _ in range(40):
        m = re.search(r"^(\s*)((?:temp|var)_\w+) = (.+);\n\1(.+?) = \2;\n", c2, re.M)
        if not m:
            break
        v = m.group(2)
        c2 = c2[:m.start()] + f"{m.group(1)}{m.group(4)} = {m.group(3)};\n" + c2[m.end():]
        if len(re.findall(rf"\b{v}\b", c2)) == 1:          # only the declaration left
            c2 = re.sub(rf"^\s*[\w\s\*]+\b{v};\n", "", c2, count=1, flags=re.M)
    return c2


def call_arg_spans(text, only_tables=False):
    """Spans (start, end) of the *inside* of non-empty argument lists of calls to func_X / D_X
    (incl. `(a = D_X)(...)`), found with a nesting-aware scan. Declarations are skipped."""
    spans = []
    pat = r"\(\s*(?:D|func)_[0-9A-F]{8} = D_[0-9A-F]{8}\s*\)\(" if only_tables else \
        r"(?:\(\s*(?:D|func)_[0-9A-F]{8} = (?:D|func)_[0-9A-F]{8}\s*\)|(?<![\w*])(?:D|func)_[0-9A-F]{8})\("
    if only_tables:
        pat = r"(?:" + pat + r"|(?<![\w*])D_[0-9A-F]{8}\()"
    for m in re.finditer(pat, text):
        line_start = text.rfind("\n", 0, m.start()) + 1
        line = text[line_start:text.find("\n", m.start())]
        if "/* extern */" in line or line.lstrip().startswith("extern"):
            continue
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
        if depth == 0 and text[m.end():i - 1].strip():
            spans.append((m.end(), i - 1))
    return spans


def all_calls(text):
    """[(start, end, [args])] for every call to func_X / D_X / `(a = D_X)` in the body (start/end
    = the inside of the parentheses; args split at top-level commas). Declarations skipped."""
    out = []
    pat = r"(?:\(\s*(?:D|func)_[0-9A-F]{8} = (?:D|func)_[0-9A-F]{8}\s*\)|(?<![\w*])(?:D|func)_[0-9A-F]{8}" \
          r"|\(\(\w+ \(\*\*\)\(\)\)0x[0-9A-F]+\)\[[^\]]+\]|(?<![\w*])D_[0-9A-F]{8}\[[^\]]+\])\("
    for m in re.finditer(pat, text):
        line_start = text.rfind("\n", 0, m.start()) + 1
        line = text[line_start:text.find("\n", m.start())]
        if "/* extern */" in line or line.lstrip().startswith("extern"):
            continue
        i, depth, args, cur = m.end(), 1, [], m.end()
        while i < len(text) and depth:
            ch = text[i]
            depth += {"(": 1, ")": -1}.get(ch, 0)
            if ch == "," and depth == 1:
                args.append(text[cur:i].strip())
                cur = i + 1
            i += 1
        if depth == 0:
            last = text[cur:i - 1].strip()
            if last or args:
                args.append(last)
            out.append((m.end(), i - 1, args))
    return out


def asm_arg_consts(func_dir, name):
    """Constants the original loads into $a0-$a3 ({reg: [values]}) - candidates for call
    arguments m2c lost (it drops arguments that are constants set before a table call)."""
    consts = {k: [] for k in range(4)}
    try:
        text = open(os.path.join(func_dir, name + ".s")).read()
    except OSError:
        return consts
    for m in re.finditer(r"\b(?:addiu|ori)\s+\$a([0-3]), \$zero, (-?0x[0-9A-Fa-f]+|-?\d+)", text):
        v = int(m.group(2), 0)
        if v not in consts[int(m.group(1))]:
            consts[int(m.group(1))].append(v)
    for m in re.finditer(r"\bmove\s+\$a([0-3]), \$zero|\baddu\s+\$a([0-3]), \$zero, \$zero", text):
        r = int(m.group(1) or m.group(2))
        if 0 not in consts[r]:
            consts[r].append(0)
    for m in re.finditer(r"\blui\s+\$a([0-3]), \((0x[0-9A-Fa-f]+) >> 16\)", text):
        v = int(m.group(2), 16)
        if v not in consts[int(m.group(1))]:
            consts[int(m.group(1))].append(v)
    return consts


ARG_CONSTS = {}                  # name -> asm_arg_consts (set by _refine_one)
ASM_TYPES = {}                   # name -> asm_sym_types (set by _refine_one)
DEAD_INITS = {}                  # name -> asm_dead_inits (set by _refine_one)


def asm_dead_inits(func_dir, name):
    """Initialisers of locals the original stores right at the start (-O0, into a `$fp`
    slot): `lw r, %lo(D_X)(..)` [+ `lw r2, OFF(r)`] then `sw r/r2, N($fp)` within the first
    ~16 instructions -> ["D_X", "M2C_FIELD(D_X, void **, OFF)"]. m2c drops such a local when
    the function never reads it, which leaves our frame smaller."""
    try:
        text = open(os.path.join(func_dir, name + ".s")).read()
    except OSError:
        return []
    ins = re.findall(r"\*/[ \t]+(\w+)[ \t]*([^\n]*)", text)[:20]
    regs, out = {}, []
    for op, args in ins:
        a = [x.strip() for x in args.split(",")]
        if op == "lw" and len(a) == 2:
            m1 = re.match(r"%lo\((D_[0-9A-F]{8})\)\(", a[1])
            m2 = re.match(r"(0x[0-9A-F]+|\d+)\((\$\w+)\)", a[1])
            if m1:
                regs[a[0]] = m1.group(1)
            elif m2 and m2.group(2) in regs and not regs[m2.group(2)].startswith("M2C") \
                    and not regs[m2.group(2)].lstrip("-").isdigit():
                regs[a[0]] = f"M2C_FIELD({regs[m2.group(2)]}, void **, 0x{int(m2.group(1), 0):X})"
        elif op in ("addiu", "ori") and len(a) == 3 and a[1] == "$zero":
            regs[a[0]] = str(int(a[2], 0))                  # a constant (`li`)
        elif op in ("sw", "sh", "sb") and len(a) == 2 and a[0] in regs and a[1].endswith("($fp)"):
            t = {"sw": "void *", "sh": "s16", "sb": "u8"}[op]
            if regs[a[0]].lstrip("-").isdigit():
                t = {"sw": "s32", "sh": "s16", "sb": "u8"}[op]
            if (t, regs[a[0]]) not in out:
                out.append((t, regs[a[0]]))
    return out

_LOAD_T = {"lb": "s8", "lbu": "u8", "lh": "s16", "lhu": "u16", "lw": "s32"}
_STORE_T = {"sb": "u8", "sh": "u16", "sw": "s32"}


def asm_sym_types(func_dir, name):
    """{symbol: C type} from the original's accesses `lbu $x, %lo(D_X)($y)` etc. (loads give the
    signedness; a store only the width). The symbols' real access types, which m2c guesses."""
    types = {}
    try:
        text = open(os.path.join(func_dir, name + ".s")).read()
    except OSError:
        return types
    for m in re.finditer(r"\b(lbu|lb|lhu|lh|lw|sb|sh|sw)\s+\$\w+, %lo\((D_[0-9A-F]{8})\)", text):
        op, sym = m.group(1), m.group(2)
        if op in _LOAD_T:
            types[sym] = _LOAD_T[op]
        elif sym not in types:
            types[sym] = _STORE_T[op]
    return types


def _close(text, i):
    """Index just past the parenthesis matching the one at text[i]."""
    depth = 0
    for j in range(i, len(text)):
        depth += {"(": 1, ")": -1}.get(text[j], 0)
        if depth == 0:
            return j + 1
    return None


def _brace_close(text, i):
    """Index of the `}` matching the `{` at text[i] (plain counting; drafts have no braces in strings)."""
    depth = 0
    for j in range(i, len(text)):
        c = text[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return j
    return None


def _top_split(expr, sep=" + "):
    """Split at top-level `sep` (outside parentheses/brackets)."""
    parts, depth, cur, i = [], 0, 0, 0
    while i < len(expr):
        ch = expr[i]
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif depth == 0 and expr.startswith(sep, i):
            parts.append(expr[cur:i])
            cur = i + len(sep)
            i += len(sep)
            continue
        i += 1
    parts.append(expr[cur:])
    return parts


def deref_sums(code):
    """[(start, end, sym, index)] for dereferences `*(A + B)` where one side is `&D_X` or
    `&(*(T *)0xADDR)` and the other side is the index expression."""
    out = []
    # `*(A + B)` and the cast form `*(T *)(A + B)` (single dereference only)
    for m in re.finditer(r"(?<![\w)\]*])\*(?:\((?:[us](?:8|16|32)|M2C_UNK8?) \*\))?\(", code):
        end = _close(code, m.end() - 1)
        if not end:
            continue
        parts = _top_split(code[m.end():end - 1])
        if len(parts) != 2:
            continue
        for a_, b_ in (parts, parts[::-1]):
            a_ = a_.strip()
            sm = re.fullmatch(r"&(D_[0-9A-F]{8})|&\(\*\(\w+ \*\)(0x[0-9A-F]+)\)", a_)
            if sm:
                out.append((m.start(), end, sm.group(1) or a_, b_.strip()))
                break
    return out


def kr_all(code):
    """Unprototyped declarations for every extern function and function pointer."""
    out = []
    for line in code.splitlines(keepends=True):
        fp = re.match(r"^extern (.+?)\(\*(\w+)\)\((.+)\);(\s*)$", line)
        if fp and fp.group(3).strip() not in ("", "void"):
            line = f"extern {fp.group(1)}(*{fp.group(2)})();{fp.group(4)}"
        fd = re.match(r"^(.*?)\b(\w+)\((.+)\);(\s*/\* extern \*/.*)$", line.rstrip("\n"))
        if fd and fd.group(3).strip() not in ("", "void"):
            ret = fd.group(1).strip() or "s32"
            line = f"{'s32' if ret == 'void' else ret} {fd.group(2)}();{fd.group(4)}\n"
        out.append(line)
    return "".join(out)


def simplify_casts(code):
    """m2c's `(u16) -(s32) X` style chains -> `-X`."""
    return re.sub(r"\((?:u8|s8|u16|s16)\) -\((?:s32|u32)\) ", "-", code)


def mutations(name, code, o0):
    out = []
    head, _, body = code.partition(f"{name}(")
    # 1. field access form, per occurrence
    for m in re.finditer(r"M2C_(?:FIELD|SFIELD|PFIELD)\(", code):
        cur = m.group(0)
        for alt in ("M2C_SFIELD(", "M2C_PFIELD("):
            if alt != cur and not (cur == "M2C_FIELD(" and alt == ("M2C_SFIELD(" if o0 else "M2C_PFIELD(")):
                out.append(("field", code[:m.start()] + alt + code[m.end():]))
    # 2. trailing return
    i = code.rindex("\n}")
    before = code[:i].rstrip()
    if before.endswith("return;"):
        out.append(("-return", before[:-len("return;")].rstrip() + code[i:]))
    elif re.search(rf"^void {name}\(", code, re.M):
        out.append(("+return", code[:i] + "\n    return;" + code[i:]))
    # 3. remove one cast
    for m in re.finditer(r"\((?:u8|s8|u16|s16|u32|s32)\) ", code):
        if m.start() > len(head):
            out.append(("cast", code[:m.start()] + code[m.end():]))
    # 4. flip signedness of one type
    flip = {"s8": "u8", "u8": "s8", "s16": "u16", "u16": "s16", "s32": "u32", "u32": "s32"}
    for m in re.finditer(r"\b(s8|u8|s16|u16|s32|u32)\b", code):
        out.append(("sign", code[:m.start()] + flip[m.group(1)] + code[m.end():]))
    # 5. inline a single-use temp
    for v in sorted(set(re.findall(r"\b(temp_\w+)\b", code))):
        uses = [m for m in re.finditer(rf"\b{v}\b", code)]
        assign = re.search(rf"^(\s*){v} = (.+);\n", code, re.M)
        decl = re.search(rf"^\s*[\w\s\*]+\b{v};\n", code, re.M)
        if assign and decl and len(uses) == 3:      # declaration, assignment, one use
            expr = assign.group(2)
            c2 = code[:assign.start()] + code[assign.end():]
            c2 = re.sub(rf"^\s*[\w\s\*]+\b{v};\n", "", c2, count=1, flags=re.M)
            c2 = re.sub(rf"\b{v}\b", f"({expr})", c2, count=1)
            out.append(("inline", c2))
    # 6. call through assignment: `X = Y;` ... `Y(` -> `(X = Y)(` (statement removed)
    for m in re.finditer(r"^[ \t]*((?:D|func)_[0-9A-F]{8}) = ((?:D|func)_[0-9A-F]{8});\n", code, re.M):
        rest = code[m.end():]
        c = re.search(rf"\b{m.group(2)}\(", rest)
        if c:
            c2 = code[:m.start()] + rest[:c.start()] + f"({m.group(1)} = {m.group(2)})(" + rest[c.end():]
            out.append(("callassign", c2))
            # ... with the call's field arguments in the address-first form (-O0: `addiu; move;
            # lh 0(..)`), which only pays off together with the call-through-assignment
            k = len(code[:m.start()] + rest[:c.start()] + f"({m.group(1)} = {m.group(2)})(")
            e = _close(c2, k - 1)
            if e:
                args = c2[k:e - 1]
                args2 = re.sub(r"\bM2C_(?:S)?FIELD\(", "M2C_PFIELD(", args)
                if args2 != args:
                    out.append(("callassign", c2[:k] + args2 + c2[e - 1:]))
    # 7. drop the argument list of one call (m2c guesses args from leftover registers)
    off = len(head) + len(name) + 1
    for s, e in call_arg_spans(body):
        out.append(("noargs", kr_all(code[:off + s] + code[off + e:])))
    # 7b. K&R (unprototyped) declarations for all called functions
    k = kr_all(code)
    if k != code:
        out.append(("kr", k))
    # 8. inline every single-use temp at once (one alone may not reduce the -O0 stack usage)
    c2 = inline_all(code)
    if c2 != code:
        out.append(("inline-all", c2))
    # 8b. compound: drop args of all table calls (D_ function pointers) + forward-substitute
    #     + inline everything - escapes the local minimum where each step alone doesn't help
    c3 = code
    for s, e in reversed(call_arg_spans(code, only_tables=True)):
        c3 = c3[:s] + c3[e:]
    c3 = kr_all(inline_all(forward_subst(simplify_casts(c3))))
    if c3 != code:
        out.append(("compound", c3))
    c4 = inline_all(forward_subst(simplify_casts(code)))
    if c4 != code and c4 != c3:
        out.append(("subst", c4))
    # 9. increments / decrements
    for m in re.finditer(r"^(\s*)(.+?) = (?:\((?:u8|s8|u16|s16|u32|s32)\) )?\(?\2 ([+-]) 1\)?;$", code, re.M):
        op = "++" if m.group(3) == "+" else "--"
        out.append(("incr", code[:m.start()] + f"{m.group(1)}{m.group(2)}{op};" + code[m.end():]))
    for m in re.finditer(r"^(\s*)(.+?) ([+-])= 1;$", code, re.M):
        op = "++" if m.group(3) == "+" else "--"
        out.append(("incr", code[:m.start()] + f"{m.group(1)}{m.group(2)}{op};" + code[m.end():]))
    # 9a. fixed-address access: Capcom used macros like ((T *)0x800C3000) for many globals; the
    #     original then shows a numeric address (`addu $at,$reg,$at` order, lui/ori la).
    for m in re.finditer(r"^extern ([\w ]+?)\s*(\*?)\s*\bD_([0-9A-F]{8})(\[\])?;\s*$", code, re.M):
        typ, star, addr, arr = m.group(1).strip(), m.group(2), m.group(3), m.group(4)
        if typ == "M2C_UNK" and not star:
            typ = "s8"            # m2c's arithmetic on unknown-typed symbols is in bytes
        sym = f"D_{addr}"
        if arr:
            repl = f"(({typ} {star}*)0x{addr})"
        else:
            repl = f"(*({typ} {star}*)0x{addr})"
        body_only = code[m.end():]
        if sym not in body_only:
            continue
        c2 = code[:m.start()] + re.sub(rf"(?<![\w]){sym}\b", repl, body_only)
        c2 = c2.replace(f"&{repl}", f"&{repl}")
        out.append(("fixaddr", c2))
    # 9d. commute one binary operation `A op B` -> `B op A` (at -O2 operand order changes the
    #     register allocation: `addu v1,v1,v0` vs `addu v0,v0,v1`)
    operand = r"(?:\(\w+(?: \*)?\) )?[\w.\->\[\]]+"
    for m in re.finditer(rf"(?<![\w)\]])({operand}) ([+*&|^]) ({operand})(?![\w(\[])", code):
        a_, op, b_ = m.group(1), m.group(2), m.group(3)
        if a_ != b_ and not a_.endswith("*"):
            out.append(("commute", code[:m.start()] + f"{b_} {op} {a_}" + code[m.end():]))
    # 9e. swap two adjacent simple statements (scheduling / register order at -O2)
    stm = list(re.finditer(r"^(    +)([^\n{}]+;)\n(?=\1[^\n{}]+;\n)", code, re.M))
    for m in stm:
        nxt = re.match(rf"{m.group(1)}([^\n{{}}]+;)\n", code[m.end():])
        if nxt and "return" not in m.group(2) + nxt.group(1):
            s2 = m.end() + nxt.end()
            out.append(("swap", code[:m.start()] + f"{m.group(1)}{nxt.group(1)}\n{m.group(1)}{m.group(2)}\n"
                        + code[s2:]))
    # 9c. m2c's byte-offset array access on an unknown-typed symbol `*(&D_X + (E * N))` (wrongly
    #     scaled when D_X is M2C_UNK = s32) -> real indexing with an N-byte element type
    elem = {"4": ["s32", "u32"], "2": ["s16", "u16"], "": ["s8", "u8"]}
    for m in re.finditer(r"\*\(&((?:D|func)_[0-9A-F]{8}) \+ (?:\((.+?) \* (4|2)\)|(\w+))\)", code):
        sym, e = m.group(1), m.group(2) or m.group(4)
        for t in elem[m.group(3) or ""]:
            out.append(("index", code[:m.start()] + f"(({t} *)&{sym})[{e}]" + code[m.end():]))
    # 9b. compound assignment `X = (cast) (X op E);` -> `X op= E;` (-O0 evaluates X once)
    for m in re.finditer(r"^(\s*)(.+?) = (?:\((?:u8|s8|u16|s16|u32|s32)\) )?\(?\2 (\+|-|&|\||\^|<<|>>|\*) (.+?)\)?;$",
                         code, re.M):
        if m.group(4).strip() not in ("1",) or m.group(3) not in "+-":
            out.append(("opassign", code[:m.start()] + f"{m.group(1)}{m.group(2)} {m.group(3)}= {m.group(4)};"
                        + code[m.end():]))
    # 9i. `t = E; X = (cast) t; if (... (cast) t ...)` -> `if (... (X = E) ...)` (an assignment
    #     inside the condition; m2c's temp costs a stack slot and a reload at -O0)
    for m in re.finditer(r"^(\s*)(temp_\w+) = ([^;\n]+);\n\1([^\n=;]+?) = (?:\(\w+\) )?\2;\n\1if \((.*?)"
                         r"(?:\((?:u8|s8|u16|s16|u32|s32)\) )?\b\2\b(.*)\) \{\n", code, re.M):
        tv = m.group(2)
        if re.search(rf"\b{tv}\b", code[m.end():]):
            continue
        ind = m.group(1)
        out.append(("assigncond", code[:m.start()] + f"{ind}if ({m.group(5)}({m.group(4).strip()} = {m.group(3)}){m.group(6)}) {{\n"
                    + code[m.end():]))
    # 9h. `X = D_Y;` then m2c used D_Y again where the original used the local X (-O0: stack
    #     loads `lw v0,16(s8)` instead of `lui/lw`): replace D_Y by X until either is reassigned
    for m in re.finditer(r"^(\s*)((?:sp|temp_|var_)\w*) = (D_[0-9A-F]{8});\n", code, re.M):
        x, g = m.group(2), m.group(3)
        rest = code[m.end():]
        stop = re.search(rf"^\s*(?:{x}|{g}) =[^=]", rest, re.M)
        seg, tail = (rest[:stop.start()], rest[stop.start():]) if stop else (rest, "")
        new_seg = re.sub(rf"(?<![\w&]){g}\b", x, seg)
        if new_seg != seg:
            out.append(("localuse", code[:m.end()] + new_seg + tail))
    # 9g. a local pointer stepped by `p += N;` (N > 1) is a pointer to N-byte records stepped with
    #     `p++` (-O0: `addiu; move` - the ++ idiom); `*p` keeps its old type through a cast
    for m in re.finditer(r"^    ((?:s8|u8|s16|u16|s32|u32|void|M2C_UNK\w*)) \*(\w+);\n", code, re.M):
        typ, p = m.group(1), m.group(2)
        steps = list(re.finditer(rf"^(\s*){p} ([+-])= (0x[0-9A-Fa-f]+|\d+);$", code, re.M))
        if not steps or len({int(s_.group(3), 0) for s_ in steps}) != 1:
            continue
        n = int(steps[0].group(3), 0)
        if n < 2:
            continue
        t2 = "u8" if typ in ("void",) or typ.startswith("M2C") else typ
        rest = code[m.end():]
        rest = re.sub(rf"^(\s*){p} \+= (?:0x[0-9A-Fa-f]+|\d+);$", rf"\1{p}++;", rest, flags=re.M)
        rest = re.sub(rf"^(\s*){p} -= (?:0x[0-9A-Fa-f]+|\d+);$", rf"\1{p}--;", rest, flags=re.M)
        # `p = p + (E * N)` -> `p = p + E` (record pointer arithmetic scales by N itself)
        rest = re.sub(rf"\b{p} = {p} \+ \((.+?) \* (?:0x{n:X}|{n})\);", rf"{p} = {p} + (\1);", rest)
        rest = re.sub(rf"(?<![\w)\]])\*{p}\b(?!\s*\+\+)", f"*({t2} *){p}", rest)
        rest = re.sub(rf"\b{p} = (M2C_[SP]?FIELD\([^,]+, )\w+ \*\*", rf"{p} = \1void **", rest)
        out.append(("structinc", code[:m.start()] + f"    struct {{ u8 b[0x{n:X}]; }} *{p};\n" + rest))
    # 9f. the reverse: `X op= E;` -> `X = X op E;` (and `X = E op X;` for + - only as written)
    for m in re.finditer(r"^(\s*)([^\n=;]+?) (\+|-|&|\||\^|<<|>>|\*)= ([^\n;]+);$", code, re.M):
        out.append(("opexpand", code[:m.start()] + f"{m.group(1)}{m.group(2)} = {m.group(2)} {m.group(3)} {m.group(4)};"
                    + code[m.end():]))
    # 10. move one local declaration to the front of the declaration block (-O0 stack order)
    bm = re.search(rf"^\S[^\n]*\b{name}\([^\n]*\{{\n((?:    [\w\s\*\[\]]+;\n)+)", code, re.M)
    if bm:
        decls = bm.group(1).splitlines(keepends=True)
        for k in range(1, len(decls)):
            new = [decls[k]] + decls[:k] + decls[k + 1:]
            out.append(("declorder", code[:bm.start(1)] + "".join(new) + code[bm.end(1):]))
    # 11. access width per type token (m2c sometimes guesses a byte where it's a halfword)
    widen = {"u8": ["u16", "s16"], "s8": ["s16", "u16"], "u16": ["u8", "u32", "s32"],
             "s16": ["s8", "s32", "u32"], "s32": ["s16", "u16"], "u32": ["u16", "s16"]}
    for m in re.finditer(r"M2C_[SP]?FIELD\([^,]+, (u8|s8|u16|s16|u32|s32) \*", code):
        for alt in widen[m.group(1)]:
            out.append(("width", code[:m.start(1)] + alt + code[m.end(1):]))
    # 12. a variable that is used but never assigned (m2c lost a stack variable): replace it
    #     by each other local
    locals_ = re.findall(rf"^    [\w\s\*]+?\b(\w+);\n", code.split(f"{name}(", 1)[1], re.M)
    for v in locals_:
        if not re.search(rf"\b{v}\s*(?:=[^=]|\+\+|--|[-+*/&|^]=)", code) and \
                len(re.findall(rf"\b{v}\b", code)) > 1:
            for other in locals_:
                if other != v and other != "return":
                    c2 = re.sub(rf"^    [\w\s\*]+?\b{v};\n", "", code, count=1, flags=re.M)
                    c2 = re.sub(rf"\b{v}\b", other, c2)
                    out.append(("uninit", c2))
    # 12b. pointed-to type of one pointer declaration / cast (wrong element size gives a wrong
    #      stride: `addiu v1,v1,16` instead of 4, `sll 3` instead of `sll 1`)
    ptr_alts = ["s8", "u8", "s16", "u16", "s32"]
    for m in re.finditer(r"\b(M2C_UNK|M2C_UNK8|void|s8|u8|s16|u16|s32|u32)( ?\*)", code):
        line_start = code.rfind("\n", 0, m.start()) + 1
        if "/* extern */" in code[line_start:code.find("\n", m.start())]:
            continue
        for alt in ptr_alts:
            if alt != m.group(1):
                out.append(("ptrtype", code[:m.start(1)] + alt + code[m.end(1):]))
    # 12c. type of one `extern M2C_UNK X;` variable
    for m in re.finditer(r"^extern (M2C_UNK|M2C_UNK8) (\w+);", code, re.M):
        for alt in ["s8", "u8", "s16", "u16", "s32", "u32"]:
            out.append(("unktype", code[:m.start(1)] + alt + code[m.end(1):]))
    # 12d. chained assignment `Y = X = E;` (-O0: an extra `move` before the second store)
    for m in re.finditer(r"^(\s*)(.+?) = (.+);\n\1(.+?) = (.+);\n", code, re.M):
        x, e, y, e2 = m.group(2), m.group(3), m.group(4), m.group(5)
        if e2 == x or e2 == e:
            out.append(("chain", code[:m.start()] + f"{m.group(1)}{y} = {x} = {e};\n" + code[m.end():]))
            out.append(("chain2", code[:m.start()] + f"{m.group(1)}{x} = {y} = {e};\n" + code[m.end():]))
    # 12e. drop one jump statement (an extra `j <label>` in our build)
    for m in re.finditer(r"^\s*(?:break|return|continue);\n", code, re.M):
        if code[m.end():].strip() != "}":         # not the function's very last statement
            out.append(("dropjump", code[:m.start()] + code[m.end():]))
    # 13. drop a redundant mask like `& 0xFFFF` / `& 0xFF`
    for m in re.finditer(r" & 0x(?:FFFF|FF)\b", code):
        out.append(("mask", code[:m.start()] + code[m.end():]))
    # --- added 2026-10-12 (near-miss analysis of the main program) ---
    # 14. flip the signedness of every `(t *)` cast of one type at once (several stores of the
    #     same constant share one `li`: flipping only one changes nothing)
    for t in sorted(set(re.findall(r"\((s8|u8|s16|u16|s32|u32) \*\)", code))):
        out.append(("signall", code.replace(f"({t} *)", f"({flip[t]} *)")))
    # 15. a fixed address that the original reached through a symbol (`lui/addiu %lo` instead
    #     of `lui/ori`): `(*(T *)0x800XXXXX)` -> `D_800XXXXX`, `((T *)0x800XXXXX)` -> `((T *)&D_X)`
    for m in re.finditer(r"\(\*\(([\w ]+?) ?\*\)0x(80[0-9A-F]{6})\)", code):
        typ, addr = m.group(1).strip(), m.group(2)
        sym = f"D_{addr}"
        if re.search(rf"\b{sym}\b", code):
            continue
        c2 = code.replace(m.group(0), sym)
        out.append(("symaddr", f"extern {typ} {sym};\n" + c2))
    # 15b. ... or an array element `D_X[0]` (`extern T D_X[];`): -O2 loads the array's address
    #      once per block (`lui/addiu`) - the symbol form loads `%lo` per access, the constant
    #      form uses `lui/ori` (func_8002F544, 2026-10-13)
    for typ, addr in sorted(set(re.findall(r"\(\*\(((?:[us](?:8|16|32))) \*\)0x(80[0-9A-F]{6})\)", code))):
        if re.search(rf"\bD_{addr}\b", code):
            continue
        c2 = code.replace(f"(*({typ} *)0x{addr})", f"D_{addr}[0]")
        out.append(("symarr", f"extern {typ} D_{addr}[];\n" + c2))
    for m in re.finditer(r"^extern ((?:[us](?:8|16|32))) (D_[0-9A-F]{8});\s*$", code, re.M):
        body_part = code[m.end():]
        if len(re.findall(rf"(?<![\w]){m.group(2)}\b", body_part)) >= 2:
            # `&D_X` -> `D_X` (the array decays to its address; `&D_X + 5` -> `D_X + 5`),
            # plain `D_X` -> `D_X[0]` (func_800153B4: the address stays in a saved register)
            nb = re.sub(rf"&{m.group(2)}\b", f"@@{m.group(2)}", body_part)
            nb = re.sub(rf"(?<![\w@]){m.group(2)}\b", f"{m.group(2)}[0]", nb)
            nb = nb.replace(f"@@{m.group(2)}", m.group(2))
            out.append(("symarr", code[:m.start()] + f"extern {m.group(1)} {m.group(2)}[];" + nb))
    # 16. fixaddr for function-pointer tables (`extern s32 (*D_X[])();`, called as `D_X[i]()`)
    for m in re.finditer(r"^extern (\w+) \(\*D_([0-9A-F]{8})\[\]\)\(\);\s*$", code, re.M):
        typ, addr = m.group(1), m.group(2)
        body_only = code[m.end():]
        if f"D_{addr}[" in body_only:
            out.append(("fixaddr-fp", code[:m.start()] + re.sub(rf"\bD_{addr}\[", f"(({typ} (**)())0x{addr})[",
                                                               body_only)))
    # 17. pointer additions m2c scaled twice: `p + N` / `p += N` with N a multiple of the
    #     element size -> N / size (try 2 and 4)
    ptrs = set(re.findall(r"^\s+(?:s8|u8|s16|u16|s32|u32|void|M2C_UNK\w*) \*(\w+);", code, re.M))
    for m in re.finditer(r"\b(\w+) (\+|-|\+=|-=) (0x[0-9A-Fa-f]+|\d+)\b", code):
        if m.group(1) not in ptrs:
            continue
        n = int(m.group(3), 0)
        for k in (2, 4):
            if n and n % k == 0:
                out.append(("ptradd", code[:m.start(3)] + str(n // k) + code[m.end(3):]))
    # 19. return type of a function-pointer extern (`u16 (*D_X)()` masks the result with
    #     0xFFFF before a test; the stages' engine-call pointers mostly return int)
    for m in re.finditer(r"^extern (\w+) \(\*(D_[0-9A-F]{8})\)\(\);", code, re.M):
        for alt in ("s32", "s16", "u16", "s8", "u8", "void"):
            if alt != m.group(1):
                out.append(("fpret", code[:m.start(1)] + alt + code[m.end(1):]))
    # 19b. every function-pointer extern returns s32 (one step: changing one alone often
    #      doesn't help when several calls feed the same expression)
    fp_all = re.sub(r"^extern (?:u8|s8|u16|s16|u32|M2C_UNK) \(\*(D_[0-9A-F]{8})\)\(\);",
                    r"extern s32 (*\1)();", code, flags=re.M)
    if fp_all != code:
        out.append(("fpret-all", fp_all))
    # 19d. the reverse, `X * 2^N` -> `X << N` (-O0: the shift needs no `move`; func_8011A9E0)
    for m in re.finditer(r" \* (0x[0-9A-Fa-f]+|\d+)\b", code):
        n = int(m.group(1), 0)
        if n > 1 and n & (n - 1) == 0:
            out.append(("mul2shl", code[:m.start()] + f" << {n.bit_length() - 1}" + code[m.end():]))
    # 19c. `X << N` -> `X * 2^N` (-O0: a multiplication becomes a shift plus a `move`), alone
    #      and together with 19b
    for m in re.finditer(r" << (0x[0-9A-Fa-f]+|\d+)\b", code):
        n = int(m.group(1), 0)
        if 0 < n < 31:
            rep = f" * 0x{1 << n:X}"
            out.append(("shl2mul", code[:m.start()] + rep + code[m.end():]))
            if fp_all != code:
                c2 = fp_all
                mm = re.search(re.escape(code[m.start() - 30 if m.start() >= 30 else 0:m.end()]), c2)
                if mm:
                    k = mm.end() - len(m.group(0))
                    out.append(("shl2mul", c2[:k] + rep + c2[mm.end():]))
    # 26. call arguments m2c got wrong: drop the last one, or append one (0 or a constant the
    #     original loads into that argument register)
    consts = ARG_CONSTS.get(name, {k: [0] for k in range(4)})
    calls = all_calls(code)
    for s, e, args in calls:
        if s <= len(head):
            continue
        if args:
            out.append(("argfix", kr_all(code[:s] + ", ".join(args[:-1]) + code[e:])))
        if len(args) < 4:
            for v in (consts.get(len(args)) or [0])[:12]:
                val = f"0x{v:X}" if v > 9 else str(v)
                out.append(("argfix", kr_all(code[:s] + ", ".join(args + [val]) + code[e:])))
    # 27. a stack struct/array split into scalars by m2c (`&sp10` passed, sp12/sp14 set but
    #     "unused" -> GCC drops their stores): merge sp10.. into one array `T sp10[n]`
    sizes = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}
    decl_re = r"^    (s8|u8|s16|u16|s32|u32) sp([0-9A-F]+);\n"
    sp_decls = {int(m.group(2), 16): m.group(1) for m in re.finditer(decl_re, code, re.M)}
    for m in re.finditer(r"&sp([0-9A-F]+)\b", code):
        b = int(m.group(1), 16)
        t = sp_decls.get(b)
        if not t:
            continue
        sz = sizes[t]
        members = [o for o in sorted(sp_decls) if o >= b and (o - b) % sz == 0 and sp_decls[o] == t
                   and o - b < 0x20]
        if len(members) < 2:
            continue
        n = (members[-1] - b) // sz + 1
        for extra in (0, 1):
            c2 = re.sub(decl_re, lambda d: "" if int(d.group(2), 16) in members[1:] else d.group(0), code,
                        flags=re.M)
            c2 = re.sub(rf"^    {t} sp{m.group(1)};\n", f"    {t} sp{m.group(1)}[{n + extra}];\n", c2, count=1,
                        flags=re.M)
            for o in members:
                c2 = re.sub(rf"(?<![\w&])sp{o:X}\b(?!\[)", f"sp{m.group(1)}[{(o - b) // sz}]", c2)
            c2 = re.sub(rf"\([^()]*\)\s*&sp{m.group(1)}\b|&sp{m.group(1)}\b",
                        lambda x: x.group(0).replace(f"&sp{m.group(1)}", f"sp{m.group(1)}"), c2)
            out.append(("stackarr", c2))
        break
    # 28. `p += 1;` right before a statement that reads `*p`: m2c moved the increment out of
    #     a delay slot - the original reads first (`*p++`)
    for m in re.finditer(r"^(\s*)(var_\w+|temp_\w+) \+= 1;\n(\s*)([^\n]*?)\*\2\b(?!\+\+)", code, re.M):
        c2 = code[:m.start()] + m.group(3) + m.group(4) + f"*{m.group(2)}++" + code[m.end():]
        out.append(("postinc", c2))
    # 29. `A = B - 1;` followed by a test of a never-assigned temp: m2c lost the value of a
    #     pre-decrement in a condition (`if (--p->x == 0)`); same for + 1 / ++
    for m in re.finditer(r"^(\s*)([^\n=]+?) = (?:\((?:u8|s8|u16|s16|u32|s32)\) )?\(?(.+?) ([+-]) 1\)?;\n((?:[^\n]*\n){0,2}?)"
                         r"([^\n]*?)(?:\((?:u8|s8|u16|s16|u32|s32)\) )?\b(temp_\w+)\b", code, re.M):
        tv = m.group(7)
        if re.search(rf"\b{tv}\s*=[^=]", code):
            continue                                  # the temp is assigned somewhere: not lost
        op = "--" if m.group(4) == "-" else "++"
        tail = code[m.end():]
        c2 = code[:m.start()] + m.group(5) + m.group(6) + f"{op}{m.group(3)}" + tail
        out.append(("predec", c2))
    #     ... post-decrement: `t = X; X = (cast) (t - 1); if ((cast) t == 0)` -> `if (X-- == 0)`
    for m in re.finditer(r"^(\s*)(temp_\w+) = ([^\n;]+);\n\1([^\n=;]+?) = (?:\((?:u8|s8|u16|s16|u32|s32)\) )?\(?\2 ([+-]) 1\)?;\n"
                         r"([^\n]*?)(?:\(((?:u8|s8|u16|s16|u32|s32))\) )?\b\2\b", code, re.M):
        tv = m.group(2)
        rest = code[m.end():]
        if re.search(rf"\b{tv}\b", rest):
            continue                                  # the temp is used again later
        op = "--" if m.group(5) == "-" else "++"
        forms = {m.group(3).strip(), m.group(4).strip()}
        if m.group(7):                                # the test's cast = the field's real type
            forms |= {re.sub(r"\b(?:u8|s8|u16|s16|u32|s32) \*", f"{m.group(7)} *", f) for f in list(forms)}
        for lv in sorted(forms):
            out.append(("postdec", code[:m.start()] + m.group(1) + m.group(6) + f"{lv}{op}" + rest))
        # Capcom's form (func_8010C994, 2026-10-13): `if ((u32) p->x-- == 0)` on a SIGNED field.
        # The sign-changing cast stops GCC folding `x-- == 0` into `--x == -1`; the original
        # shows `lhu; addiu -1; sh; sll/sra` of the old value
        for lv in sorted(forms):
            lv2 = re.sub(r"\bu(8|16) \*", r"s\1 *", lv)
            for cast in ("(u32) ", "(u16) "):
                out.append(("postdec", code[:m.start()] + m.group(1) + m.group(6) + f"{cast}{lv2}{op}" + rest))
    #     ... m2c's assignment form `(X = X - 1) == 0` / `(X = X + 1)` is a pre-decrement `--X == 0`
    #         (func_8010B314: -O0 loads the base three times for `--p->x`)
    for m in re.finditer(r"\((M2C_[SP]?FIELD\([^()]*?\)) = M2C_[SP]?FIELD\(([^()]*?)\) ([+-]) 1\) (==|!=) ", code):
        lv = m.group(1)
        op = "--" if m.group(3) == "-" else "++"
        lv_plain = re.sub(r"^M2C_[SP]?FIELD", "M2C_FIELD", lv)
        out.append(("predec", code[:m.start()] + f"{op}{lv_plain} {m.group(4)} " + code[m.end():]))
    # 40. an empty trailing `default: return;` (with or without a fallthrough comment) at the end
    #     of a switch that ends the function: -O0 emits an extra `j <epilogue>` for it
    dm2 = re.search(r"\n(\s*/\* fallthrough \*/\n)?\s*default:\n\s*return;\n(\s*\}\n\}\s*)$", code)
    if dm2:
        out.append(("dropdefault", code[:dm2.start()] + "\n" + dm2.group(2)))
        # ... or the last case ends with `break;` (its `j` stays, out-of-range values skip it;
        #     ending func_800E4950)
        ind = re.search(r"\n([ \t]*)\S[^\n]*$", code[:dm2.start()])
        bi = (ind.group(1) if ind else "        ")
        out.append(("dropdefault", code[:dm2.start()] + f"\n{bi}break;\n" + dm2.group(2)))
    # 41. `if ((v = E) < 0) { v = -v; }` is Capcom's abs(): -O0 only inlines it (bgez/move/negu,
    #     no stack slot) when written `__builtin_abs(E)` (func_801092D8); also inlined into v's
    #     single later use
    for m in re.finditer(r"^(\s*)if \(\((var_\w+) = (.+)\) < 0\) \{\n\s*\2 = -\2;\n\s*\}\n", code, re.M):
        ind, v, e = m.group(1), m.group(2), m.group(3)
        rest = code[m.end():]
        out.append(("absb", code[:m.start()] + f"{ind}{v} = __builtin_abs({e});\n" + rest))
        if len(re.findall(rf"\b{v}\b", rest)) == 1 and not re.search(rf"\b{v}\s*=[^=]", rest):
            out.append(("absb", code[:m.start()] + re.sub(rf"\b{v}\b", lambda _: f"__builtin_abs({e})", rest)))
    #     ... m2c's other form `v = E; if (temp_x < 0) { v = -v; }` (temp_x never set; func_801081D4)
    for m in re.finditer(r"^(\s*)(var_\w+) = ([^\n;]+);\n\s*if \((?:temp_\w+|\2) < 0\) \{\n\s*\2 = -\2;\n\s*\}\n",
                         code, re.M):
        ind, v, e = m.group(1), m.group(2), m.group(3)
        rest = code[m.end():]
        out.append(("absb", code[:m.start()] + f"{ind}{v} = __builtin_abs({e});\n" + rest))
        if len(re.findall(rf"\b{v}\b", rest)) == 1 and not re.search(rf"\b{v}\s*=[^=]", rest):
            out.append(("absb", code[:m.start()] + re.sub(rf"\b{v}\b", lambda _: f"__builtin_abs({e})", rest)))
    # 42. the `break;` right before a switch's closing brace: -O0 emits a `j` for it even in the
    #     last case (func_801092D8)
    for m in re.finditer(r"\n([ \t]*)break;\n([ \t]*)\}\n", code):
        if len(m.group(1)) > len(m.group(2)):
            out.append(("dropbreak", code[:m.start()] + f"\n{m.group(2)}}}\n" + code[m.end():]))
    #     ... and a `return;` in the last case of a switch that ends the function (stage5 func_80114D4C)
    mr2 = re.search(r"\n([ \t]+)return;\n([ \t]+)\}\n\}\s*$", code)
    if mr2 and len(mr2.group(1)) > len(mr2.group(2)):
        out.append(("dropbreak", code[:mr2.start()] + f"\n{mr2.group(2)}}}\n}}\n"))
    # 51. if/else with the branches swapped: `if (C) {A} else {B}` -> `if (!(C)) {B} else {A}`
    #     (GCC places the then-branch first; stage1 func_80113448: Capcom wrote the short
    #     branch first with a negated || condition)
    for m in re.finditer(r"^([ \t]*)if \(", code, re.M):
        cp = _close(code, m.end() - 1)
        if not cp or code[cp:cp + 3] != " {\n":
            continue
        ob = cp + 1
        cb = _brace_close(code, ob)
        if cb is None or not code.startswith(" else {\n", cb + 1):
            continue
        eb = cb + len(" else ") + 1
        ce = _brace_close(code, eb)
        if ce is None:
            continue
        cond = code[m.end():cp - 1]
        a_body, b_body = code[ob + 2:cb], code[eb + 2:ce]
        if "else" in code[ce + 1:ce + 7]:
            continue
        ind = m.group(1)
        out.append(("swapif", code[:m.start()] + f"{ind}if (!({cond})) {{\n{b_body}{ind}}} else {{\n{a_body}{ind}}}"
                    + code[ce + 1:]))
        # 52. ... or the then-branch leaves (`break;` / `return;`) and the else-body follows
        #     unguarded: its `j` goes straight to the end (stage2 func_801182F4)
        a_txt = a_body.rstrip(" \t")
        b_out = re.sub(r"^    ", "", b_body.rstrip(" \t"), flags=re.M)
        rest_ = code[ce + 1:].lstrip("\n")
        for ex in ("break;", "return;"):
            out.append(("earlyexit", code[:m.start()] + f"{ind}if ({cond}) {{\n{a_txt}{ind}    {ex}\n{ind}}}\n"
                        + b_out + rest_))
    # 50. a `break;` before a case label where the draft falls through (the original has the
    #     `j <after switch>`; stage3 func_8011F058, func_80131804)
    for m in re.finditer(r"\n([ \t]*)(case [^\n:]+:|default:)\n", code):
        before = code[:m.start()].rstrip()
        if before.endswith(("break;", "return;", "{", "*/")) or re.search(r"return [^\n;]*;$", before):
            continue
        ind = m.group(1) + "    "
        out.append(("addbreak", code[:m.start()] + f"\n{ind}break;" + code[m.start():]))
    # 49. `= -(E)` written `= (E) * -1` (-O0: `move; negu` instead of `negu`; func_8010BCF0,
    #     func_8011A8B4), each occurrence and all at once
    negs = []
    for m in re.finditer(r"= (?:\((?:s32|s16|u16|u8|s8)\) )?-\(", code):
        k = _close(code, m.end() - 1)
        if k:
            negs.append((m.start(), k, code[m.end() - 1:k]))
    for a_, k, e in negs:
        out.append(("negmul", code[:a_] + f"= {e} * -1" + code[k:]))
    if len(negs) > 1:
        c2 = code
        for a_, k, e in reversed(negs):
            c2 = c2[:a_] + f"= {e} * -1" + c2[k:]
        out.append(("negmul", c2))
    # 53. m2c's split call-table idiom anywhere in a statement (`D_801FEC18 = D_X;` + `... D_X(..)`)
    #     -> `(D_801FEC18 = D_X)(..)` (tools/callmerge.py; stage7 func_80119ED0, func_8011B2E0)
    cm2 = re.sub(r"\n[ \t]*D_801FEC18 = (D_801FE[0-9A-F]{3});\n([^\n]*?)\b\1\(",
                 lambda m: f"\n{m.group(2)}(D_801FEC18 = {m.group(1)})(", code)
    if cm2 != code:
        out.append(("callmerge", cm2))
    # 48. word-by-word block copies -> VECTOR/MATRIX struct assignment (see structcopy_fix)
    sc = structcopy_fix(code)
    if sc:
        out.append(("structcopy", sc))
    # 47. m2c sometimes uses a temp on the line *before* the line that assigns it (an
    #     uninitialised read in the draft); the assigned expression belongs in that use
    #     (func_80045564: `p->x74 = (p->x74 + f())` + `p = *(void **)0x800CAB14;`)
    for m in re.finditer(r"^([^\n]*)\n([ \t]*)(temp_\w+) = ([^\n;]+);\n", code, re.M):
        tv, prev = m.group(3), m.group(1)
        if not re.search(rf"\b{tv}\b", prev) or re.search(rf"\b{tv}\s*=[^=]", code[:m.start()]):
            continue
        new_prev = re.sub(rf"\b{tv}\b", lambda _: f"({m.group(4)})", prev)
        rest = code[m.end():]
        keep = f"{m.group(2)}{tv} = {m.group(4)};\n" if re.search(rf"\b{tv}\b", rest) else ""
        out.append(("uninit", code[:m.start()] + new_prev + "\n" + keep + rest))
    # 46. one global accessed both ways in one function (-O2): array form `D_X[0]` (address kept in
    #     a register) in one switch case, plain `%lo` accesses in another (func_8005130C). Per case
    #     block, the `D_X[0]` accesses become a scalar alias of the same symbol.
    seg_starts = [m.start() for m in re.finditer(r"^[ \t]*(?:case [^\n:]+|default):\n", code, re.M)]
    if seg_starts:
        bounds = list(zip(seg_starts, seg_starts[1:] + [len(code)]))
        for dm in re.finditer(r"^extern ([\w ]+?) (D_[0-9A-F]{8})\[\];$", code, re.M):
            sym, typ = dm.group(2), dm.group(1)
            for s0, e0 in bounds:
                seg = code[s0:e0]
                if f"{sym}[0]" not in seg:
                    continue
                c2 = code[:s0] + seg.replace(f"{sym}[0]", f"{sym}_s") + code[e0:]
                c2 = c2.replace(dm.group(0), dm.group(0) + f'\nextern {typ} {sym}_s __asm__("{sym}");', 1)
                out.append(("symsplit", c2))
    # 44. `if (C) { v = A; } else { v = B; }` -> `v = (C) ? A : B;` with v still used afterwards
    #     (-O0: `li; j; li; sw` into the one slot; func_8011F11C)
    for m in re.finditer(r"^(\s*)if \((.+)\) \{\n\s*(var_\w+) = ([^\n;]+);\n\s*\} else \{\n\s*\3 = ([^\n;]+);\n"
                         r"\s*\}\n", code, re.M):
        out.append(("ternary2", code[:m.start()] + f"{m.group(1)}{m.group(3)} = ({m.group(2)}) ? {m.group(4)} : "
                    f"{m.group(5)};\n" + code[m.end():]))
    # 45. m2c's `if (A) { if (B) { goto L; } } else { L: BODY }` is one condition
    #     `if (!(A) || (B)) { BODY }`: GCC's jump for a trailing && inside || ends in an explicit
    #     `j <body>` (func_80115274)
    for m in re.finditer(r"^([ \t]*)if \((.+)\) \{\n[ \t]*if \((.+)\) \{\n[ \t]*goto (block_\d+);\n[ \t]*\}\n"
                         r"\1\} else \{\n\4:\n((?:.*\n)*?)\1\}\n", code, re.M):
        if len(re.findall(rf"\b{m.group(4)}\b", code)) != 2:
            continue
        ind = m.group(1)
        out.append(("gotoor", code[:m.start()] + f"{ind}if (!({m.group(2)}) || ({m.group(3)})) {{\n{m.group(5)}{ind}}}\n"
                    + code[m.end():]))
    #     ... m2c's other shape: an EMPTY inner if and a stray label in the else
    #     (`if (A) { if (B) { } } else { lbl: BODY }`; func_80112928, func_8011C6F4)
    for m in re.finditer(r"^([ \t]*)if \((.+)\) \{\n[ \t]*if \((.+)\) \{\n\s*\}\n\1\} else \{\n(?:\w+:\n)?((?:.*\n)*?)\1\}\n",
                         code, re.M):
        ind = m.group(1)
        out.append(("gotoor", code[:m.start()] + f"{ind}if (!({m.group(2)}) || ({m.group(3)})) {{\n{m.group(4)}{ind}}}\n"
                    + code[m.end():]))
    # 43. a trailing `return;` that sat inside the last (nested) if: its `j <epilogue>` is then only
    #     reached from that block, and the failing tests jump past it (func_8011F618)
    mr = re.search(r"((?:\n[ \t]*\}[ \t]*)+)\n[ \t]*return;\n\}\s*$", code)
    if mr:
        closes = mr.group(1).split("\n")[1:]
        head_ = code[:mr.start()]
        for k in range(1, len(closes) + 1):
            ind = re.match(r"[ \t]*", closes[-k]).group(0) + "    "
            inner, outer = closes[:len(closes) - k], closes[len(closes) - k:]
            out.append(("movereturn", head_ + "".join("\n" + c for c in inner) + f"\n{ind}return;"
                        + "".join("\n" + c for c in outer) + "\n}\n"))
    #     ... an existing `X-- == 0` test gets the sign-changing cast (see above)
    for m in re.finditer(r"(?:\((?:u8|s8|u16|s16|u32|s32)\) )?(M2C_[SP]?FIELD\([^()]*?, )(?:u|s)(8|16)( \*, \w+\))(--|\+\+) (==|!=) 0\b",
                         code):
        for cast in ("(u32) ", "(u16) "):
            out.append(("postdec", code[:m.start()] + f"{cast}{m.group(1)}s{m.group(2)}{m.group(3)}{m.group(4)} {m.group(5)} 0"
                        + code[m.end():]))
    #     ... and the `X--;` / `X++;` statement form
    for m in re.finditer(r"^(\s*)([^\n;]+?)(--|\+\+);\n((?:[^\n]*\n){0,2}?)([^\n]*?)(?:\((?:u8|s8|u16|s16|u32|s32)\) )?"
                         r"\b(temp_\w+)\b", code, re.M):
        tv = m.group(6)
        if re.search(rf"\b{tv}\s*=[^=]", code):
            continue
        c2 = code[:m.start()] + m.group(4) + m.group(5) + f"{m.group(3)}{m.group(2).strip()}" + code[m.end():]
        out.append(("predec", c2))
    # 32. `loop_N: if (C) { BODY goto loop_N; }` -> `while (C) { BODY }` (-O0: `bnez body; j end`)
    #     (the `if` may carry odd indentation from an earlier rewrite: match the brace by the
    #     indentation of the label line; stage3 func_8012A5A8)
    for m in re.finditer(r"^([ \t]*)(loop_\w+):\n[ \t]*if \((.+)\) \{\n((?:.*\n)*?)[ \t]*goto \2;\n(?:\1    |\1)\}\n",
                         code, re.M):
        if len(re.findall(rf"\b{m.group(2)}\b", code)) != 2:
            continue                                  # other gotos to the label
        ind = m.group(1) if m.group(1) else "    "
        out.append(("gotowhile", code[:m.start()] + f"{ind}while ({m.group(3)}) {{\n{m.group(4)}{ind}}}\n"
                    + code[m.end():]))
    # 21b. `X += 0xFF;` / `X += 0xFFFF;` is a decrement of a u8/u16
    for m in re.finditer(r"^(\s*)(.+?) \+= (0xFF|0xFFFF);$", code, re.M):
        out.append(("decr", code[:m.start()] + f"{m.group(1)}{m.group(2)}--;" + code[m.end():]))
    # 30. `if (C) { v = A; } else { v = B; } X = v;` -> `X = (C) ? A : B;` (m2c's form of a
    #     conditional expression; the temp costs a stack slot at -O0)
    for m in re.finditer(r"^(\s*)if \((.+)\) \{\n\s*(var_\w+) = ([^\n;]+);\n\s*\} else \{\n\s*\3 = ([^\n;]+);\n"
                         r"\s*\}\n\s*([^\n=]+?) = \3;\n", code, re.M):
        v = m.group(3)
        rest = code[m.end():]
        if re.search(rf"\b{v}\b", rest):
            continue
        ind, cond, a_, b_, x = m.group(1), m.group(2), m.group(4), m.group(5), m.group(6).strip()
        for c_, t_, f_ in ((cond, a_, b_),):
            out.append(("ternary", code[:m.start()] + f"{ind}{x} = ({c_}) ? {t_} : {f_};\n" + rest))
            out.append(("ternary", code[:m.start()] + f"{ind}{x} = {c_} ? {t_} : {f_};\n" + rest))
    # 26b. a numeric argument replaced by another constant the original loads into that $aN
    for s, e, args in calls:
        if s <= len(head):
            continue
        for k, a_ in enumerate(args[:4]):
            if not re.fullmatch(r"(?:\([^()]*\) ?)?(-?0x[0-9A-Fa-f]+|-?\d+)U?", a_):
                continue
            cur = int(re.search(r"-?0x[0-9A-Fa-f]+|-?\d+", a_).group(0), 0)
            for v in (consts.get(k) or [])[:12]:
                if v != cur:
                    new_args = args[:k] + [f"0x{v:X}" if v > 9 else str(v)] + args[k + 1:]
                    out.append(("argval", code[:s] + ", ".join(new_args) + code[e:]))
    # 25. copy propagation: a local assigned exactly once from a plain variable (`sp10 = arg0;`,
    #     m2c's copy of an argument's stack home) -> use that variable, drop the copy
    for m in re.finditer(r"^\s*(\w+) = (\w+);\n", code, re.M):
        x, y = m.group(1), m.group(2)
        if x == y or not re.match(r"^(?:sp|temp_|var_|saved_)\w*$", x) or re.match(r"^\d|0x", y):
            continue
        rest = code[:m.start()] + code[m.end():]
        if re.search(rf"\b{x}\s*(?:=[^=]|\+\+|--|[-+*/&|^]=)", rest) or re.search(rf"&{x}\b", rest):
            continue                                  # assigned again / address taken
        c2 = re.sub(rf"^    [\w\s\*]+?\b{x};\n", "", rest, count=1, flags=re.M)
        c2 = re.sub(rf"\b{x}\b", y, c2)
        if c2 != rest:
            out.append(("copyprop", c2))
    # 31. an unused local that the original initialises at the start (from the asm)
    if o0:
        hm2 = re.search(rf"^\S[^\n]*\b{name}\([^\n]*\{{\n((?:    [\w\s\*\[\]]+;\n)*)", code, re.M)
        if hm2:
            for typ, init in DEAD_INITS.get(name, []):
                if f"= {init};" in code:
                    continue
                out.append(("deadinit", code[:hm2.end()] + f"    {typ} unused_init = {init};\n" + code[hm2.end():]))
                out.append(("deadinit", code[:hm2.start(1)] + f"    {typ} unused_init = {init};\n" + code[hm2.start(1):]))
    # 24. an unused local (-O0 gives every declared local a stack slot: the original's frame
    #     is 8/16 bytes bigger when Capcom left a variable unused) - kept by drop_unused_locals
    hm = re.search(rf"^\S[^\n]*\b{name}\([^\n]*\{{\n", code, re.M)
    if hm and "unused" not in code:
        if o0:
            out.append(("padlocal", code[:hm.end()] + "    s32 unused;\n" + code[hm.end():]))
            out.append(("padlocal", code[:hm.end()] + "    s32 unused;\n    s32 unused2;\n" + code[hm.end():]))
            out.append(("padlocal", code[:hm.end()] + "    s32 unused[4];\n" + code[hm.end():]))
            # ... also after the existing locals (they keep their slots; func_8011E810)
            hl = re.match(r"((?:    [\w\s\*\[\]\{\};]+;\n)*)", code[hm.end():])
            if hl and hl.group(1):
                k = hm.end() + hl.end(1)
                for pad in ("    s32 unused;\n", "    s32 unused[2];\n", "    s32 unused[4];\n",
                            "    s32 unused[6];\n", "    s32 unused[8];\n"):
                    out.append(("padlocal", code[:k] + pad + code[k:]))
        else:   # -O2 removes unused scalars, but an unused array still gets frame space
            for n in (2, 4, 6, 8, 12):
                out.append(("padlocal", code[:hm.end()] + f"    s32 unused[{n}];\n" + code[hm.end():]))
    # 20. width of a plain extern global (`sb` where the original stores a halfword)
    for m in re.finditer(r"^extern (s8|u8|s16|u16|s32|u32) (D_[0-9A-F]{8});", code, re.M):
        for alt in widen[m.group(1)] + [flip[m.group(1)]]:
            out.append(("exttype", code[:m.start(1)] + alt + code[m.end(1):]))
    # 21. `X = (u8) (X + 0xFF);` / `(u16) (X + 0xFFFF)` is a decrement (`X--`, -O0 idiom)
    for m in re.finditer(r"^(\s*)(.+?) = \((?:u8|s8)\) \(\2 \+ 0xFF\);$|^(\s*)(.+?) = \((?:u16|s16)\) \(\4 \+ 0xFFFF\);$",
                         code, re.M):
        ind, lv = (m.group(1), m.group(2)) if m.group(2) else (m.group(3), m.group(4))
        out.append(("decr", code[:m.start()] + f"{ind}{lv}--;" + code[m.end():]))
    #     ... also when m2c gave the left side another field type (`M2C_FIELD(p, u16 *, 0x87) =
    #     (u8) (M2C_FIELD(p, u8 *, 0x87) + 0xFF)`): the right side's field is the real one
    for m in re.finditer(r"^(\s*)M2C_[SP]?FIELD\(([^,]+), \w+ \*, (\w+)\) = \((?:u8|s8|u16|s16)\) "
                         r"\((M2C_[SP]?FIELD\(\2, \w+ \*, \3\)) ([+-]) (0xFF|0xFFFF|1)\);$", code, re.M):
        op = "--" if (m.group(5) == "+") != (m.group(6) == "1") else "++"
        out.append(("decr", code[:m.start()] + f"{m.group(1)}{m.group(4)}{op};" + code[m.end():]))
    # 22. a global reached through a local pointer `T *p = &D_X;` (-O2: `la` once, then
    #     `0($reg)` for every access - neither the symbol form (`%lo` per access) nor the
    #     fixed-address form (`lui/ori`) gives that)
    dm = re.search(rf"^\S[^\n]*\b{name}\([^\n]*\{{\n((?:    [\w\s\*\[\]]+;\n)*)", code, re.M)
    if dm:
        ins = dm.end()
        for m in sorted(set(re.findall(r"\(\*\(((?:[us](?:8|16|32))) \*\)0x(80[0-9A-F]{6})\)", code))):
            typ, addr = m
            pat = f"(*({typ} *)0x{addr})"
            if code.count(pat) >= 2 and not re.search(rf"\bD_{addr}\b", code):
                c2 = code[:ins] + f"    {typ} *ptr_{addr} = &D_{addr};\n" + code[ins:]
                c2 = f"extern {typ} D_{addr};\n" + c2.replace(pat, f"(*ptr_{addr})")
                out.append(("symptr", c2))
        for m in re.finditer(r"^extern ((?:[us](?:8|16|32))) (D_([0-9A-F]{8}));\s*$", code, re.M):
            typ, sym, addr = m.group(1), m.group(2), m.group(3)
            body_part = code[ins:]
            if len(re.findall(rf"(?<![\w&]){sym}\b", body_part)) >= 2:
                c2 = code[:ins] + f"    {typ} *ptr_{addr} = &{sym};\n" + \
                    re.sub(rf"(?<![\w&]){sym}\b", f"(*ptr_{addr})", body_part)
                out.append(("symptr", c2))
    # 22b. a fixed address kept in a local pointer (`T *ptr = (T *)0x800C3000;`): -O2 then keeps
    #      it in a saved register across loops/calls (`lui/ori` once, before the loop)
    if dm:
        ins2 = dm.end()
        for typ, addr in sorted(set(re.findall(r"\*\(((?:[us](?:8|16|32))) \*\)0x(80[0-9A-F]{6})\b", code))):
            pat = f"*({typ} *)0x{addr}"
            decl = f"    {typ} *ptr_{addr} = ({typ} *)0x{addr};\n"
            c2 = code[:ins2] + decl + code[ins2:].replace(pat, f"*ptr_{addr}")
            out.append(("constptr", c2))
            if re.search(rf"\bD_{addr}\b", code):
                c3 = re.sub(rf"(?<![\w&])D_{addr}\b(?!\s*\[)", f"(*ptr_{addr})", c2[len(code[:ins2]) + len(decl):])
                out.append(("constptr", code[:ins2] + decl + c3))
    # 23. `X = A + B;` -> `X = A; X = (u8 *) X + B;` (a pointer that is set, then advanced:
    #     -O2 adds into the pointer's register); when A is a field read, also as a pointer
    #     load (`void **`: m2c often types the base as the halfword it guessed)
    for m in re.finditer(r"^(\s*)(temp_\w+|var_\w+) = (.+?) \+ (.+);$", code, re.M):
        ind, x, a_, b_ = m.groups()
        if a_.count("(") != a_.count(")") or b_.count("(") != b_.count(")"):
            continue
        variants = [a_]
        fm = re.match(r"^(M2C_[SP]?FIELD\(.+?, )(?:s8|u8|s16|u16|s32|u32) \*(, [^,()]+\))$", a_)
        if fm:
            variants.append(f"{fm.group(1)}void **{fm.group(2)}")
        for a2 in variants:
            out.append(("splitadd", code[:m.start()] + f"{ind}{x} = {a2};\n{ind}{x} = (u8 *) {x} + {b_};"
                        + code[m.end():]))
    # 18b. any `*(INDEX + &SYM)` / `*(&SYM + INDEX)` (SYM = D_X or `(*(T *)0xADDR)`): m2c's
    #      INDEX is in bytes, C scales it again by the element size -> keep it as a byte offset
    for st, en, sym, idx in deref_sums(code):
        if sym.startswith("D_"):
            bases = [f"(u8 *)&{sym}", f"(u8 *)0x{sym[2:]}"]       # symbol / Capcom's fixed address
        else:
            bases = [f"(u8 *){re.match(r'.*?(0x[0-9A-F]+)', sym).group(1)}"]
        for base in bases:
            for t in ("s32", "u32", "s16", "u16", "s8", "u8"):
                out.append(("byteoff2", code[:st] + f"*({t} *)({base} + ({idx}))" + code[en:]))
    # 18c. all indexed accesses at once, each with the type the original uses for that symbol
    #      (one at a time is a local minimum when they share an index computation)
    atypes = ASM_TYPES.get(name, {})

    def symkey(sym):             # `D_X` or `&(*(T *)0xADDR)` -> the asm's symbol name D_ADDR
        if sym.startswith("D_"):
            return sym
        am = re.search(r"0x([0-9A-F]+)", sym)
        return f"D_{int(am.group(1), 16):08X}" if am else sym
    sums = [(st, en, symkey(sym), idx) for st, en, sym, idx in deref_sums(code) if symkey(sym) in atypes]
    if sums:
        for numeric in (False, True):
            c2 = code
            for st, en, sym, idx in sorted(sums, reverse=True):
                base = f"(u8 *)0x{sym[2:]}" if numeric else f"(u8 *)&{sym}"
                c2 = c2[:st] + f"*({atypes[sym]} *)({base} + ({idx}))" + c2[en:]
            if not numeric:      # symbols must be declared
                for sym in sorted({s_[2] for s_ in sums}):
                    if not re.search(rf"^extern [^\n]*\b{sym}\b", c2, re.M):
                        c2 = f"extern u8 {sym}[];\n" + c2
            out.append(("byteoff-asm", c2))
    # 37. swap the operands of a parenthesised `(A + B)` with complex operands (register order of
    #     the `addu` at -O2; the simple `commute` mutation only handles plain operands)
    for m in re.finditer(r"\(", code[len(head):]):
        st = len(head) + m.start()
        en = _close(code, st)
        if not en:
            continue
        parts = _top_split(code[st + 1:en - 1])
        if len(parts) == 2 and all(p.strip() for p in parts) and ("(" in parts[0] or "(" in parts[1]):
            out.append(("commute2", code[:st + 1] + f"{parts[1].strip()} + {parts[0].strip()}" + code[en - 1:]))
    # 36. a store of an expression computed in full width first (`{ s32 t = EXPR; *(s16 *)P = t; }`):
    #     a direct `*(s16 *)P = EXPR;` lets GCC narrow the reads in EXPR (`lhu` instead of `lh`/`lw`)
    for m in re.finditer(r"^(\s*)(\*\((?:s32|u32|s16|u16|s8|u8) \*\)\(.+?\)|M2C_[SP]?FIELD\([^,]+, (?:s32|u32|s16|u16|s8|u8) \*, \w+\)) = "
                         r"([^;\n]*[-+*&|^][^;\n]*);$", code, re.M):
        ind, lhs, rhs = m.groups()
        if lhs.count("(") != lhs.count(")"):
            continue
        cur = re.search(r"(s32|u32|s16|u16|s8|u8) \*", lhs).group(1)
        for t in dict.fromkeys([cur, "s16", "u16", "s8", "u8"]):
            lhs2 = re.sub(r"(s32|u32|s16|u16|s8|u8) \*", f"{t} *", lhs, count=1)
            out.append(("widetmp", code[:m.start()] + f"{ind}{{ s32 wt = {rhs}; {lhs2} = wt; }}" + code[m.end():]))
    # 38. a sign test written as a mask: `(x & 0x8000)` -> `((s16) x < 0)`, `(x & 0x80)` -> `((s8) x < 0)`
    #     (-O2: `sll 16; bgez` instead of `andi 0x8000; beqz`)
    for m in re.finditer(r"\(([\w.\->\[\]]+) & 0x(8000|80)\)", code):
        t = "s16" if m.group(2) == "8000" else "s8"
        out.append(("signtest", code[:m.start()] + f"(({t}) {m.group(1)} < 0)" + code[m.end():]))
    # 39. one store to a word global through a narrower pointer (`*(s16 *)&D_X = 2;`): Capcom
    #     wrote the same variable with different widths (`sh` among `sw` stores)
    for m in re.finditer(r"^(\s*)(D_[0-9A-F]{8}) = ([^;\n]+);$", code, re.M):
        if re.search(rf"^extern (?:s32|u32|s16|u16) {m.group(2)};", code, re.M):
            for t in ("s16", "u8"):
                out.append(("partstore", code[:m.start()] + f"{m.group(1)}*({t} *)&{m.group(2)} = {m.group(3)};"
                            + code[m.end():]))
    # 33. a non-void function whose last statement isn't a return: m2c lost `return N;`
    rm = re.search(rf"^(?!void\b)\S[^\n]*\b{name}\(", code, re.M)
    if rm:
        i = code.rindex("\n}")
        last = code[:i].rstrip().splitlines()[-1].strip() if code[:i].strip() else ""
        if not last.startswith("return"):
            for v in ("1", "0"):
                out.append(("retconst", code[:i] + f"\n    return {v};" + code[i:]))
    # 35. `if (C) { return R; } REST` (top level) -> `if (!C) { REST [return N;] } return R;`
    #     (-O2 lays out the rest of the function as the branch target instead)
    em = re.search(r"^    if \((.+)\) \{\n        return( [^;\n]*)?;\n    \}\n", code, re.M)
    if em:
        end = code.rindex("\n}")
        rest = code[em.end():end].rstrip("\n")
        cond = em.group(1)
        inv = {" < ": " >= ", " >= ": " < ", " > ": " <= ", " <= ": " > ", " == ": " != ", " != ": " == "}
        parts = [(op, cond.split(op)) for op in inv if cond.count(op) == 1 and "&&" not in cond and "||" not in cond]
        ncond = f"{parts[0][1][0]}{inv[parts[0][0]]}{parts[0][1][1]}" if parts else f"!({cond})"
        body = "\n".join("    " + l if l.strip() else l for l in rest.splitlines())
        tails = [""]
        if rm and not rest.strip().splitlines()[-1].strip().startswith("return"):
            tails = ["\n        return 1;", "\n        return 0;"]
        for tl in tails:
            out.append(("invertret", code[:em.start()] + f"    if ({ncond}) {{\n{body}{tl}\n    }}\n"
                        f"    return{em.group(2) or ''};" + code[end:]))
    #     ... and the reverse: `if (C) { BODY return A; } return B; }` -> `if (!C) { return B; } BODY return A;`
    im = re.search(r"^    if \((.+)\) \{\n((?:        .*\n|\n)*?)        return( [^;\n]*)?;\n    \}\n((?:    (?!return).*\n)*?)"
                   r"    return( [^;\n]*)?;\n\}", code, re.M)
    if im:
        cond = im.group(1)
        inv = {" < ": " >= ", " >= ": " < ", " > ": " <= ", " <= ": " > ", " == ": " != ", " != ": " == "}
        parts = [(op, cond.split(op)) for op in inv if cond.count(op) == 1 and "&&" not in cond and "||" not in cond]
        ncond = f"{parts[0][1][0]}{inv[parts[0][0]]}{parts[0][1][1]}" if parts else f"!({cond})"
        body1 = "".join((l[4:] if l.startswith("        ") else l) + "\n" for l in im.group(2).splitlines())
        body2 = "".join(("    " + l if l.strip() else l) + "\n" for l in im.group(4).splitlines())
        out.append(("invertret", code[:im.start()] + f"    if ({ncond}) {{\n{body2}        return{im.group(5) or ''};\n    }}\n"
                    f"{body1}    return{im.group(3) or ''};\n}}" + code[im.end():]))
    # 34. a byte field read at the neighbouring offset (m2c misplaces some delay-slot loads)
    for m in re.finditer(r"(M2C_[SP]?FIELD\([^,()]+, [us]8 \*, )(0x[0-9A-F]+|\d+)\)", code):
        off = int(m.group(2), 0)
        for d in (1, -1):
            if off + d >= 0:
                out.append(("fieldoff", code[:m.start()] + f"{m.group(1)}0x{off + d:X})" + code[m.end():]))
    # 18e. a byte-offset base through the symbol that Capcom wrote as a number (`addu at,reg,at`)
    sym_bases = list(re.finditer(r"\(u8 \*\)&(D_([0-9A-F]{8}))\b", code))
    for m in sym_bases:
        out.append(("sym2num", code[:m.start()] + f"(u8 *)0x{m.group(2)}" + code[m.end():]))
    if len(sym_bases) > 1:
        out.append(("sym2num", re.sub(r"\(u8 \*\)&D_([0-9A-F]{8})\b", r"(u8 *)0x\1", code)))
    # 18d. every untyped extern (`M2C_UNK D_X`) gets the type the original accesses it with
    c3 = re.sub(r"^extern M2C_UNK8? (D_[0-9A-F]{8});",
                lambda m: f"extern {atypes[m.group(1)]} {m.group(1)};" if m.group(1) in atypes else m.group(0),
                code, flags=re.M)
    if c3 != code:
        out.append(("asmtype", c3))
    # 18. byte-offset access on a symbol `*(&D_X + (E * N))` (m2c's M2C_UNK arithmetic is in
    #     bytes): keep the byte offset, try each access type
    for m in re.finditer(r"\*\(&((?:D|func)_[0-9A-F]{8}) \+ \((.+?) \* (\d+)\)\)", code):
        for t in ("s32", "u32", "s16", "u16", "s8", "u8"):
            out.append(("byteoff", code[:m.start()] + f"*({t} *)((u8 *)&{m.group(1)} + ({m.group(2)} * {m.group(3)}))"
                        + code[m.end():]))
    # every candidate: drop local declarations that are no longer used (-O0 still gives
    # unused locals a stack slot, so leftovers would keep the frame size wrong), and make
    # function-pointer declarations unprototyped (call tables are called with any arity)
    # Targeted mutations first (refine takes the first improvement and starts over, so the
    # order decides how fast a draft converges); the broad ones (ptrtype, sign, swap, ...)
    # come after, in their original order.
    first = ["callmerge", "structcopy", "absb", "negmul", "addbreak", "swapif", "earlyexit", "dropbreak", "movereturn", "mul2shl", "ternary2", "gotoor", "symsplit", "uninit", "byteoff-asm", "asmtype", "sym2num", "retconst", "invertret", "widetmp", "structinc", "symarr", "signtest", "partstore", "dropdefault", "localuse", "callassign", "assigncond", "ternary", "deadinit", "gotowhile", "predec", "postdec", "decr", "postinc", "copyprop", "padlocal", "stackarr", "argfix", "argval", "byteoff2",
             "symptr", "splitadd", "shl2mul", "fpret-all", "signall", "fixaddr-fp", "exttype", "fpret"]
    rank = {k: i for i, k in enumerate(first)}
    out = sorted(out, key=lambda kc: rank.get(kc[0], len(first)))     # (stable: keeps the rest's order)
    return [(k, fp_unproto(drop_unused_locals(name, c))) for k, c in out]


COPY_TYPES = {4: "VECTOR", 8: "MATRIX"}


def structcopy_fix(code):
    """m2c's word-by-word copy of a 16/32-byte block -> one VECTOR/MATRIX struct assignment
    (-O0 copies structs with `lw t2,t3,t4,t1` / `sw` groups; func_80108F04, 2026-10-13).
    Form A: `temp = SRC + OFF;` + `M2C_FIELD(DST, s32 *, 0) = M2C_PFIELD(SRC, s32 *, OFF);`
            + `M2C_FIELD(DST, s32 *, k) = M2C_FIELD(temp, s32 *, k);` ...
    Form B: `M2C_FIELD(DST, s32 *, k) = M2C_FIELD(SRC, s32 *, k);` for k = 0, 4, ..."""
    lines = code.split("\n")
    out, i, changed = [], 0, False
    while i < len(lines):
        l = lines[i]
        ma = re.match(r"^(\s*)(temp_\w+) = (\w+) \+ (0x[0-9A-F]+|\d+);$", l)
        if ma and i + 1 < len(lines):
            ind, tv, src, off = ma.groups()
            m0 = re.match(rf"^\s*M2C_FIELD\((.+?), s32 \*, 0\) = (?:\(s32\) )?M2C_PFIELD\({src}, s32 \*, {off}\);$",
                          lines[i + 1])
            if m0:
                dst = m0.group(1)
                k, j = 1, i + 2
                while j < len(lines) and re.match(
                        rf"^\s*M2C_FIELD\({re.escape(dst)}, s32 \*, (?:0x)?{format(4 * k, 'X')}\) = (?:\(s32\) )?"
                        rf"M2C_FIELD\({tv}, s32 \*, (?:0x)?{format(4 * k, 'X')}\);$", lines[j]):
                    k, j = k + 1, j + 1
                if k in COPY_TYPES:
                    out.append(f"{ind}*({COPY_TYPES[k]} *){dst} = *({COPY_TYPES[k]} *)((u8 *){src} + {off});")
                    i, changed = j, True
                    continue
        mb = re.match(r"^(\s*)M2C_FIELD\((.+?), s32 \*, 0\) = (?:\(s32\) )?M2C_FIELD\((\w+), s32 \*, 0\);$", l)
        if mb:
            ind, dst, src = mb.groups()
            k, j = 1, i + 1
            while j < len(lines) and re.match(
                    rf"^\s*M2C_FIELD\({re.escape(dst)}, s32 \*, (?:0x)?{format(4 * k, 'X')}\) = (?:\(s32\) )?"
                    rf"M2C_FIELD\({src}, s32 \*, (?:0x)?{format(4 * k, 'X')}\);$", lines[j]):
                k, j = k + 1, j + 1
            if k in COPY_TYPES:
                out.append(f"{ind}*({COPY_TYPES[k]} *){dst} = *({COPY_TYPES[k]} *){src};")
                i, changed = j, True
                continue
        out.append(l)
        i += 1
    return "\n".join(out) if changed else None


def fp_unproto(code):
    return re.sub(r"^extern (.+?)\(\*(\w+)\)\((?!\s*(?:void)?\s*\);).+\);",
                  lambda m: f"extern {m.group(1)}(*{m.group(2)})();", code, flags=re.M)


def drop_unused_locals(name, code):
    bm = re.search(rf"^\S[^\n]*\b{name}\([^\n]*\{{\n((?:    [\w\s\*\[\]]+;\n)+)", code, re.M)
    if not bm:
        return code
    keep = []
    for line in bm.group(1).splitlines(keepends=True):
        v = re.search(r"(\w+)(?:\[[^\]]*\])?;\s*$", line)
        if v and len(re.findall(rf"\b{v.group(1)}\b", code)) == 1 and not v.group(1).startswith("unused"):
            continue
        keep.append(line)
    return code[:bm.start(1)] + "".join(keep) + code[bm.end(1):]


def refine_one(ctx, name, evals):
    """Refine one function; its scratch folder is deleted afterwards (thousands of small
    files otherwise keep Windows Defender and the indexer busy long after a run)."""
    try:
        return _refine_one(ctx, name, evals)
    finally:
        shutil.rmtree(os.path.join(ctx.dir, "work", name), ignore_errors=True)


def _refine_one(ctx, name, evals):
    path = os.path.join("build", "auto", ctx.target, "c", f"{name}.c")
    code = open(path).read()
    code = code.split("\n/* failed:")[0]
    o0 = "-O0" in ctx.flags
    ARG_CONSTS[name] = asm_arg_consts(ctx.func_dir, name)
    ASM_TYPES[name] = asm_sym_types(ctx.func_dir, name)
    DEAD_INITS[name] = asm_dead_inits(ctx.func_dir, name)
    best = distance(ctx, name, code, "start")
    if best is None:
        return name, None, None, code
    n = 0
    improved = True
    best_code = code
    cur = best                       # distance of `code` (may equal best after a sideways move)
    seen = {code}
    sideways = 0
    while best and improved and n < evals:
        improved = False
        plateau = None
        for kind, cand in mutations(name, code, o0):
            n += 1
            d = distance(ctx, name, cand, f"e{n % 4}")
            if d is not None and d < cur:
                cur, code, improved = d, cand, True
                seen.add(code)
                if d < best:
                    best, best_code = d, cand
                break
            if d is not None and d == cur and plateau is None and cand not in seen:
                plateau = cand
            if n >= evals:
                break
        # Nothing better: take one sideways step to an equally good, unseen candidate (some
        # fixes need two mutations, e.g. shl2mul + fpret, where the first alone doesn't help)
        if not improved and plateau is not None and sideways < 4 and n < evals:
            sideways += 1
            code = plateau
            seen.add(code)
            improved = True
    return name, best, n, best_code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    ap.add_argument("--status", default="size,diff")
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--evals", type=int, default=300)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    ctx = Ctx(args.target)
    res_path = os.path.join("build", "auto", args.target, "results.json")
    results = json.load(open(res_path))
    wanted = tuple(args.status.split(","))
    names = list(dict.fromkeys(args.only or [n for n, s in results.items() if s.split(":")[0] in wanted]))
    print(f"{args.target}: refining {len(names)} functions")
    matched = 0
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        for name, d, n, code in ex.map(lambda nm_: refine_one(ctx, nm_, args.evals), names):
            if d == 0:
                matched += 1
                results[name] = "match"
                ad.write_retry(os.path.join("build", "auto", args.target, "c", f"{name}.c"), code)
                print(f"  MATCH {name} ({n} evals)")
            elif d is not None and n:
                results[name] = f"diff:{d}"
                ad.write_retry(os.path.join("build", "auto", args.target, "c", f"{name}.c"), code)
    json.dump(results, open(res_path, "w"), indent=1, sort_keys=True)
    print(f"{args.target}: {matched} new matches")


if __name__ == "__main__":
    main()
