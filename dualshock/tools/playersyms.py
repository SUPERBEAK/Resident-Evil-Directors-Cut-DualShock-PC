"""playersyms.py - the remaining separate symbols inside the player object -> g_PlayerEntity fields.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python:
    python tools/playersyms.py <C file> [<C file> ...]     # src/main/door.c, src/shared/zombie.c, ...
    python tools/playersyms.py --all

The player is the Entity at 0x800C5124 (g_PlayerEntity). During matching its fields got separate
symbols (D_800C51AE = +0x8A = hit_state), and tools/playerfields.py converted only the uses whose
declared type equals the field's type. This tool does the rest, per use of a symbol (or of a
function's alias `extern u8 D_800C51AE_u8 __asm__("D_800C51AE");`) that starts an Entity field:
  same type                    -> g_PlayerEntity.hit_state
  same size, other type        -> read: ((u8) g_PlayerEntity.hit_state), written: plain field
  &symbol                      -> &g_PlayerEntity.hit_state (cast to the old pointer type if needed)
  s32 at 0x84 / 16 bits at 0x86 -> ENTITY_STATE_WORD(&g_PlayerEntity) / ENTITY_ACTION_S16/U16(...)
Then every program that compiles the file is rebuilt; in a function whose bytes changed the uses
become the exact form `(*(u8 *)&g_PlayerEntity.hit_state)`, and if it still differs (the main program
is -O2: a struct field's address can be kept in a register where a symbol was loaded directly) the
function keeps its old text. Aliases no longer used are removed; declarations of the symbols in a
program's globals.h are removed when no file of that program uses them any more.
The player's own data at 0x15C.. becomes g_PlayerWork.<field> (struct PlayerWork in re1_types.h).
Offsets without a field (pads) are left alone.
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import declclean as dc  # noqa: E402
import enemywork as ew  # noqa: E402
import entityfields as ef  # noqa: E402
import integrate as ig  # noqa: E402

BASE, END = 0x800C5124, 0x800C5124 + 0x18C
VAR = "g_PlayerEntity"
DECL = f'extern Entity {VAR} __asm__("g_PlayerEntity");'
SCALAR = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}
ALIAS_RE = re.compile(r'^\s+extern\s+([\w ]+?)\s*(\**)\s*(\w+)\s*__asm__\("(D_800C5[12][0-9A-F]{2})"\);\s*$')
PLAIN_RE = re.compile(r"^\s*extern\s+([\w ]+?)\s*(\**)\s*(D_800C5[12][0-9A-F]{2})\s*;\s*$")
LVAL_AFTER = re.compile(r"^\s*(?:=(?!=)|\+\+|--|[-+*/%&|^]=|<<=|>>=)")
LVAL_BEFORE = re.compile(r"(?:\+\+|--)\s*$")


def in_object(sym):
    return BASE <= int(sym[2:], 16) < END


def norm(base, stars):
    return " ".join(base.split()) + (" " + stars if stars else "")


def size(ty):
    return 4 if "*" in ty else SCALAR.get(ty)


def program_decls(prog):
    """{symbol: type} from src/<prog>/globals.h."""
    p = os.path.join("src", prog, "globals.h")
    out = {}
    if os.path.isfile(p):
        for l in open(p).read().split("\n"):
            m = PLAIN_RE.match(l)
            if m and in_object(m.group(3)):
                out[m.group(3)] = norm(m.group(1), m.group(2))
    return out


FIELDS = None


WORK_BASE = 0x15C
WORK_FIELDS = None


def work_fields():
    """{offset: (name, type)} of struct PlayerWork (entity offsets 0x15C..; g_PlayerWork)."""
    h = open(os.path.join("include", "re1_types.h")).read()
    body = h[h.index("typedef struct PlayerWork {"):h.index("} PlayerWork;")]
    return {int(m.group(1), 16): (m.group(3), m.group(2)) for m in
            re.finditer(r"/\* 0x([0-9A-Fa-f]+) \*/\s*([us](?:8|16|32))\s+(\w+)\s*;", body)
            if not m.group(3).startswith("pad")}


def field_at(off):
    """(C expression of the field, type) for the field starting at entity offset off, or None.
    0x15C.. is the player's own data (g_PlayerWork), the rest Entity fields (g_PlayerEntity)."""
    if off >= WORK_BASE:
        f = WORK_FIELDS.get(off)
        return (f"g_PlayerWork.{f[0]}", f[1]) if f else None
    for (o, ty), name in FIELDS.items():
        if o == off and not re.match(r"(pad|sub)[0-9A-Fa-f]+$", name):
            return f"{VAR}.{name}", ty
    return None


def expr_for(sym_off, ty, lval, addr, exact):
    """Replacement text for one use of a symbol of type ty at entity offset sym_off, or None."""
    if sym_off == 0x84 and ty in ("s32", "u32") and not addr:
        return "ENTITY_STATE_WORD(&g_PlayerEntity)" if ty == "s32" else None
    if sym_off == 0x86 and ty in ("s16", "u16") and not addr:
        return f"ENTITY_ACTION_{ty.upper()}(&g_PlayerEntity)"
    f = field_at(sym_off)
    if f is None or size(ty) is None:
        return None
    plain, fty = f
    if addr:
        return f"&{plain}" if ty == fty else f"({ty} *)&{plain}"
    if ty == fty:
        return plain
    if exact or size(ty) != size(fty):
        return f"(*({ty} *)&{plain})"
    return plain if lval else f"(({ty}) {plain})"


def convert_function(lines, file_types, exact):
    """New lines of one function (header line first)."""
    aliases, drop = {}, set()
    for i, l in enumerate(lines):
        m = ALIAS_RE.match(l)
        if m and in_object(m.group(4)):
            aliases[m.group(3)] = (m.group(4), norm(m.group(1), m.group(2)), i)
    local_plain = {}
    for i, l in enumerate(lines):
        m = PLAIN_RE.match(l)
        if m and i > 0 and in_object(m.group(3)):
            local_plain[m.group(3)] = (norm(m.group(1), m.group(2)), i)
    names = {n: (sym, ty) for n, (sym, ty, _) in aliases.items()}
    for sym, ty in file_types.items():
        names.setdefault(sym, (sym, ty))
    for sym, (ty, _) in local_plain.items():
        names[sym] = (sym, ty)
    if not names:
        return lines, 0
    rx = re.compile(r"(?<![\w.])(" + "|".join(map(re.escape, sorted(names, key=len, reverse=True))) + r")(?![\w\[])")
    out, n = [], 0
    for i, l in enumerate(lines):
        if i == 0 or i in {v[2] for v in aliases.values()} or i in {v[1] for v in local_plain.values()}:
            out.append(l)
            continue
        code = ig.code_only(l)
        res, pos = [], 0
        for m in rx.finditer(code):
            sym, ty = names[m.group(1)]
            before = l[max(0, m.start() - 3):m.start()]
            addr = before.rstrip().endswith("&") and not before.rstrip().endswith("&&")
            lval = LVAL_AFTER.match(l[m.end():m.end() + 4]) or LVAL_BEFORE.search(before)
            new = expr_for(int(sym[2:], 16) - BASE, ty, lval, addr, exact)
            if new is None:
                continue
            start = m.start()
            if addr:                                   # replace the `&` too
                start = l.rindex("&", 0, m.start())
            res.append(l[pos:start])
            res.append(new)
            pos = m.end()
            n += 1
        res.append(l[pos:])
        out.append("".join(res))
    # drop aliases / local plain declarations nobody uses any more
    body = "\n".join(out)
    keep = []
    for i, l in enumerate(out):
        m = ALIAS_RE.match(l) or (PLAIN_RE.match(l) if i > 0 else None)
        if m and i > 0:
            name = m.group(3)
            rest = "\n".join(x for j, x in enumerate(out) if j != i)
            if not re.search(r"(?<![\w.])" + re.escape(name) + r"(?!\w)", ig.code_only(rest)):
                continue
        keep.append(l)
    return keep, n


def programs(path):
    return ew.programs_of(path)


def file_types(path):
    """{symbol: type} visible at file scope: the file's own declarations, else its program's globals.h."""
    progs = programs(path)
    types = dict(program_decls(progs[0][0])) if progs else {}
    for l in open(path).read().split("\n"):
        if l.startswith("extern"):
            m = PLAIN_RE.match(l)
            if m and in_object(m.group(3)):
                types[m.group(3)] = norm(m.group(1), m.group(2))
    return types


def process(path):
    original = open(path).read()
    types = file_types(path)
    lines = original.split("\n")
    spans = ig.function_spans(original)
    names = [n for _, _, n in spans]
    level = {n: "cast" for n in names}

    def assemble():
        out, pos, total = [], 0, 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            body = lines[s - 1:e]
            if level[n] != "orig":
                body, c = convert_function(body, types, level[n] == "exact")
                total += c
            out += body
            pos = e
        text = "\n".join(out + lines[pos:])
        progs = programs(path)
        has_decl = DECL in text or any(DECL in open(os.path.join("src", p, "globals.h")).read()
                                       for p, _ in progs if os.path.isfile(os.path.join("src", p, "globals.h")))
        if total and not has_decl:
            inc = max(i for i, l in enumerate(text.split("\n")[:200]) if l.startswith("#include"))
            tl = text.split("\n")
            text = "\n".join(tl[:inc + 1] + [DECL] + tl[inc + 1:])
        return text, total

    text, total = assemble()
    if total == 0:
        print(f"{path}: nothing to convert")
        return True
    for attempt in range(20):
        text, total = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        if errs:
            spans_now = ig.function_spans(text)
            in_funcs = {n for f, ln, _ in errs for s, e, n in spans_now
                        if f.replace(os.sep, "/").endswith(path.replace(os.sep, "/").split("src/")[-1]) and s <= int(ln) <= e}
            in_funcs -= {n for n in names if level[n] == "orig"}
            if in_funcs:
                for n in in_funcs:
                    level[n] = "orig"
                print(f"  compile errors in {len(in_funcs)} function(s), keeping their old text")
                continue
            print("compile errors outside functions, restoring:", *[f"  {f}:{ln}: {m}" for f, ln, m in errs], sep="\n")
            ig.write_file(os.path.join("build", "playersyms_failed.c"), text)
            break
        ok_all, bad = res
        if ok_all:
            kept = [n for n in names if level[n] == "orig"]
            print(f"OK: {path}: {total} uses -> {VAR}; exact forms in "
                  f"{sum(1 for n in names if level[n] == 'exact')} function(s); old text kept in {len(kept)}"
                  + (f": {' '.join(kept[:10])}" if kept else ""))
            return True
        if not bad:
            print("build failed for another reason, restoring")
            break
        for n in bad:
            level[n] = "exact" if level[n] == "cast" else "orig"
        print(f"  {len(bad)} function(s) changed: {' '.join(sorted(bad)[:8])}")
    ig.write_file(path, original)
    for prog, f in programs(path):
        ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))
    return False


def drop_unused_decls():
    """Remove `extern T D_800C5xxx;` lines from globals.h files when no file of that program uses them."""
    for h in sorted(glob.glob(os.path.join("src", "*", "globals.h"))):
        prog = h.split(os.sep)[1]
        texts = [open(f).read() for f in glob.glob(os.path.join("src", prog, "*.c"))]
        for f in glob.glob(os.path.join("src", prog, "*.c")):
            m = re.search(r'#include "\.\./shared/(\w+)\.c"', open(f).read())
            if m:
                texts.append(open(os.path.join("src", "shared", m.group(1) + ".c")).read())
        code = ig.code_only("\n".join(texts))
        t = open(h).read()
        out = []
        for l in t.split("\n"):
            m = PLAIN_RE.match(l)
            if m and in_object(m.group(3)) and not re.search(r"(?<![\w.])" + m.group(3) + r"(?!\w)", code):
                continue
            out.append(l)
        if len(out) != len(t.split("\n")):
            print(f"{h}: {len(t.split(chr(10))) - len(out)} unused player-symbol declarations removed")
            ig.write_file(h, "\n".join(out))


def main():
    global FIELDS, WORK_FIELDS
    FIELDS = ef.entity_fields()
    WORK_FIELDS = work_fields()
    args = sys.argv[1:]
    if args == ["--all"]:
        args = sorted(f for f in glob.glob(os.path.join("src", "*", "*.c"))
                      if re.search(r"D_800C5[12][0-9A-F]{2}", open(f).read())
                      and '#include "../shared/' not in open(f).read())
    elif not args:
        sys.exit(__doc__)
    for p in args:
        process(p)
    drop_unused_decls()


if __name__ == "__main__":
    main()
