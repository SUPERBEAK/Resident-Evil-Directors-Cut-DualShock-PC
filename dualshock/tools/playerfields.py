"""playerfields.py - turn the separate symbols inside the player object into g_PlayerEntity fields.

Run from dualshock/ with the project venv python:
    python tools/playerfields.py --target main        (or stage1, ...)

The player object is an Entity at 0x800C5124. During matching its fields got separate symbols
(`D_800C51AB` is +0x87 = action_state). This tool, per C file:
  1. declares `extern Entity g_PlayerEntity __asm__("g_PlayerEntity");` at file scope (a C name for
     the existing linker symbol at the object's start - nothing changes for splat or the linker),
  2. in every function replaces a symbol inside the object by `g_PlayerEntity.<field>` when the
     Entity field at that offset has exactly the type the symbol is declared with there (the
     file-scope declaration, or the function's own alias `extern T D_X_s8 __asm__("D_X");` - an
     alias whose uses all become fields is removed),
  3. removes file-scope declarations that are no longer used,
  4. rebuilds; functions whose bytes change get their old text back, until all 13 match.
Literal-address uses (`*(s32 *)0x800C5158`) are left alone (a symbol compiles differently at -O2).
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402
import declclean as dc  # noqa: E402
import entityfields as ef  # noqa: E402

BASE, END = 0x800C5124, 0x800C5124 + 0x18C
VAR = "g_PlayerEntity"
DECL = f'extern Entity {VAR} __asm__("D_{BASE:08X}");'
SYM = re.compile(r"^D_(800C5[12][0-9A-F]{2})$")


def in_object(sym):
    m = SYM.match(sym)
    return m and BASE <= int(m.group(1), 16) < END


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", required=True)
    ap.add_argument("--loose", action="store_true",
                    help="also use a field whose signedness differs (u8/s8, ...); kept only where it matches")
    a = ap.parse_args()
    t = ig.Target(a.target)
    original = open(t.cfile).read()
    rerun = DECL in original          # a second (e.g. --loose) pass over an already converted file
    fields = ef.entity_fields()

    # file-scope types of the object's symbols
    filescope = {}
    for m in re.finditer(r"^extern ([\w ]+?)\s*(\*?)\s*(D_800C5[12][0-9A-F]{2})\s*;\s*$", original, re.M):
        if in_object(m.group(3)) and not m.group(2):
            filescope[m.group(3)] = m.group(1).strip()
    lines = original.split("\n")
    spans = ig.function_spans(original)
    alias_re = re.compile(r'^\s+extern\s+([us](?:8|16|32))\s+(\w+)\s*__asm__\("(D_800C5[12][0-9A-F]{2})"\);\s*$')

    def field_for(sym, ty):
        off = int(sym[2:], 16) - BASE
        f = fields.get((off, ty))
        if f is None and a.loose and ty[0] in "us":
            f = fields.get((off, ("u" if ty[0] == "s" else "s") + ty[1:]))
        return f

    new_funcs, count = {}, 0
    for s, e, n in spans:
        body = lines[s - 1:e]
        repl = {}                       # C name -> replacement expression
        drop = set()                    # alias declaration lines to remove
        for i, l in enumerate(body):
            m = alias_re.match(l)
            if m and in_object(m.group(3)):
                f = field_for(m.group(3), m.group(1))
                if f:
                    repl[m.group(2)] = f"{VAR}.{f}"
                    drop.add(i)
        for sym, ty in filescope.items():
            f = field_for(sym, ty)
            if f:
                repl.setdefault(sym, f"{VAR}.{f}")
        out = []
        for i, l in enumerate(body):
            if i in drop:
                continue
            if i > 0:
                for old, new in repl.items():
                    if old in l:
                        before = l
                        # (outside string literals: other aliases' __asm__("D_X") must stay)
                        l = ig.sub_code(r"(?<![\w.])" + re.escape(old) + r"(?![\w\[])", new, l)
                        count += before != l
            out.append(l)
        new_funcs[n] = out

    def assemble(keep_old):
        out, pos = [], 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            out += lines[s - 1:e] if n in keep_old else new_funcs[n]
            pos = e
        text = "\n".join(out + lines[pos:])
        # file-scope: add the object, drop declarations nobody uses any more
        block_end = text.index("\n\n", text.index(dc.BLOCK_BEGIN)) if dc.BLOCK_BEGIN in text else None
        if block_end is None:
            sys.exit("run tools/declclean.py on this file first")
        if not rerun:
            text = text[:block_end] + "\n" + DECL + text[block_end:]
        for sym in filescope:
            if not re.search(r"(?<![\w.])" + sym + r"\b", text.split(DECL, 1)[1]):
                text = re.sub(r"^extern [\w ]+?\s*" + sym + r"\s*;\s*\n", "", text, count=1, flags=re.M)
        return text

    print(f"{a.target}: {count} lines use {VAR} fields after conversion")
    keep_old = set()
    for _ in range(60):
        ig.write_file(t.cfile, assemble(keep_old))
        ok, log = ig.build(t)
        if ok:
            print(f"OK: {a.target} matches; {len(keep_old)} function(s) kept the separate symbols")
            return
        errs = re.findall(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): (?!warning)(.*)", log)
        if errs:
            text_now = open(t.cfile).read()
            spans_now = ig.function_spans(text_now)
            in_funcs = {n for ln, _ in errs for s, e, n in spans_now if s <= int(ln) <= e} - keep_old
            outside = [ln for ln, _ in errs if not any(s <= int(ln) <= e for s, e, _ in spans_now)]
            if in_funcs and not outside:
                print(f"  compile errors in {len(in_funcs)} function(s), keeping their old text")
                keep_old |= in_funcs
                continue
            ig.write_file(os.path.join("build", "playerfields_failed.c"), text_now)
            print("compile errors (file kept as build/playerfields_failed.c), restoring:")
            for ln, msg in errs[:10]:
                print(f"  line {ln}: {msg}")
            break
        bad = dc.bad_functions(t, [n for _, _, n in spans if n not in keep_old])
        if not bad:
            print("build failed for another reason, restoring:\n" + log[-2000:])
            break
        print(f"  {len(bad)} function(s) changed bytes, keeping their old text: {' '.join(bad[:6])}")
        keep_old |= set(bad)
    ig.write_file(t.cfile, original)
    ig.build(t)
    sys.exit(1)


if __name__ == "__main__":
    main()
