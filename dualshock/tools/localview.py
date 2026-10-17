"""localview.py - name the fields reached through LOCAL pointer variables (sp18, temp_v0, ...), from a spec.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python:
    python tools/localview.py header                      # regenerate include/re1_locals.h from all specs
    python tools/localview.py apply config/locals/x.txt   # header + rewrite every file

Spec (config/locals/<name>.txt):
    struct Joint                       # the struct type (defined below with `define`, or `fields Entity`)
    macro JOINT                        # JOINT(p) = ((Joint *)(p)), written to include/re1_locals.h
    source joints|, 0x98\\)            # regex: a variable qualifies if EVERY assignment to it in the
                                       # function matches this (or is the variable itself +/- something)
    stride 0x7C                        # optional: offsets past one element are later elements [k]
    max 0x16C                          # optional: ignore offsets from here on
    define
    0x00 u8 flags   # comment          # offset, type (s8..u32, pointers, SVECTOR, VECTOR, MATRIX), name
A use M2C_FIELD(sp18, s32 *, 0x58) of such a variable becomes JOINT(sp18)->world.t[0] (nested PS1 types
resolve to member paths); a cast when read with another type. Exact forms (the original's code where the
readable one compiles differently): MEMBER_AT(sp18, Joint, ->world.t[0], s32) (member form) and
FIELD_AT(sp18, Joint, [1].flags, u8) (pointer form; also used for later elements). `sp18 + 0x44` (an
address) becomes &JOINT(sp18)->world. Variables keep their declared types (their other uses, e.g.
pointer arithmetic, stay as they are). Per function: readable -> exact -> old text, verified in every
program that compiles the file.
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enemywork as ew  # noqa: E402
import integrate as ig  # noqa: E402
import structview as sv  # noqa: E402

HEADER = os.path.join("include", "re1_locals.h")
SCALAR = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}
# PS1 types: (size, [(offset, type, member path)])
NESTED = {
    "SVECTOR": (8, [(0, "s16", ".vx"), (2, "s16", ".vy"), (4, "s16", ".vz"), (6, "s16", ".pad")]),
    "VECTOR": (16, [(0, "s32", ".vx"), (4, "s32", ".vy"), (8, "s32", ".vz"), (12, "s32", ".pad")]),
    "MATRIX": (32, [((i * 3 + j) * 2, "s16", f".m[{i}][{j}]") for i in range(3) for j in range(3)]
               + [(0x14, "s32", ".t[0]"), (0x18, "s32", ".t[1]"), (0x1C, "s32", ".t[2]")]),
}
MAC = re.compile(r"M2C_(FIELD|PFIELD|SFIELD)\((\w+), ([\w ]+?\*+), (0x[0-9A-Fa-f]+|\d+)\)")


def tsize(ty, arr=""):
    n = int(arr[1:-1], 0) if arr else 1
    if "*" in ty:
        return 4 * n
    if ty in NESTED:
        return NESTED[ty][0] * n
    return SCALAR[ty] * n if ty in SCALAR else None


class Spec:
    def __init__(self, path):
        self.path = path.replace(os.sep, "/")
        self.struct = self.macro = self.source = None
        self.stride, self.max, self.define, self.fields = None, None, False, []
        self.params = {}                                 # {function: {argN, ...}} listed by hand
        self.exprs = []                                  # `expr <regex>`: base expressions that qualify
        self.noaddr = False
        self.files, self.excludes = None, []
        for raw in open(path):
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            key, _, rest = line.partition(" ")
            rest = rest.split("  #")[0].strip()
            if key == "struct":
                self.struct = rest
            elif key == "macro":
                self.macro = rest
            elif key == "source":
                self.source = re.compile(rest)
            elif key == "stride":
                self.stride = int(rest, 0)
            elif key == "max":
                self.max = int(rest, 0)
            elif key == "define":
                self.define = True
            elif key == "file":                          # only files whose path matches this regex
                self.files = re.compile(rest)
            elif key == "exclude":                       # never files whose path matches this regex (e.g. the
                self.excludes.append(re.compile(rest))   # yawn: its entities' aim_target holds a JOINT pointer)
            elif key == "noaddr":                        # leave `p + N` alone (script records: N is "skip
                self.noaddr = True                       # to the next operand", not a field's address)
            elif key == "expr":                          # `expr ->aim_target\)?$`: any such base expression
                self.exprs.append(re.compile(rest))
            elif key == "param":                         # `param joint_severed_limb_step arg0`
                fn, arg = rest.split(None, 1)            # arg may be an expression: *(void **)0x800CF640
                self.params.setdefault(fn, set()).add(arg.strip())
            elif key == "fields":
                self.fields = [(o, t, n, a, "") for o, t, n, a, _ in sv.header_fields(rest)]
            else:
                m = sv.FIELD_LINE.match(line)
                if m:
                    self.fields.append((int(m.group(1), 16), " ".join(m.group(2).split()), m.group(3),
                                        m.group(4) or "", (m.group(5) or "").strip()))
        self.fields.sort()

    def path_at(self, off, ty):
        """(member path from a pointer, field type) for an access of type ty at off, or None."""
        k = 0
        if self.stride and off >= self.stride:
            k, off = divmod(off, self.stride)
        if self.max is not None and off >= self.max:
            return None
        for o, fty, name, arr, _ in self.fields:
            if name.startswith("pad"):
                continue
            size = tsize(fty)
            if size is None or not (o <= off < o + size * (int(arr[1:-1], 0) if arr else 1)):
                continue
            if arr:                                      # an array field: element index + the rest
                i, rem = divmod(off - o, size)
                name, o = f"{name}[{i}]", off - rem
            head = f"->{name}" if k == 0 else f"[{k}].{name}"
            if off == o and (fty == ty or fty not in NESTED or ty == fty):
                if fty in NESTED and ty != fty:
                    break                       # a scalar at the start of a nested type: use the member
                return head, fty
            if fty in NESTED:
                for so, sty, sp in NESTED[fty][1]:
                    if o + so == off:
                        return head + sp, sty
            return None
        return None


def block(spec, paths=None):
    out = [f"/* {spec.struct} through local pointers: {spec.macro}(p) ({', '.join(paths or [spec.path])}) */"]
    if spec.define:
        rows, pos = [], 0
        for off, ty, name, arr, c in spec.fields:
            if off > pos:
                rows.append((pos, f"u8 pad{pos:02X}[{off - pos}];", ""))
            stars = ty.count("*")
            base = " ".join(w for w in ty.split() if w != "*")
            rows.append((off, f"{base} {'*' * stars}{name}{arr};", c))
            pos = off + tsize(ty, arr)
        w = max(len(d) for _, d, _ in rows)
        out.append(f"typedef struct {spec.struct} {{")
        out += [f"    /* 0x{o:02X} */ {d.ljust(w)}{'  /* ' + c + ' */' if c else ''}".rstrip() for o, d, c in rows]
        out.append(f"}} {spec.struct};")
    out.append(f"#define {spec.macro}(p) (({spec.struct} *)(p))")
    return "\n".join(out)


def write_header():
    specs = [Spec(p) for p in sorted(glob.glob(os.path.join("config", "locals", "*.txt")))]
    # several specs may share one macro (more bases for the same struct): one block per macro, at the place
    # of its first spec, with the struct from the spec that defines it
    groups, order = {}, []
    for sp in specs:
        if sp.macro not in groups:
            groups[sp.macro] = [sp, []]
            order.append(sp.macro)
        g = groups[sp.macro]
        if g[0].struct != sp.struct:
            sys.exit(f"{sp.path}: macro {sp.macro} is {g[0].struct} in {g[0].path}")
        if sp.define and g[0] is not sp:
            if g[0].define:
                sys.exit(f"{sp.path}: struct {sp.struct} is also defined by {g[0].path}")
            g[0] = sp
        g[1].append(sp.path)
    body = "\n\n".join(block(groups[m][0], groups[m][1]) for m in order)
    ig.write_file(HEADER, "/* re1_locals.h - structs reached through local pointer variables, with a cast macro each.\n"
                  "   GENERATED by tools/localview.py from config/locals/*.txt - edit the spec, not this file. */\n"
                  "#ifndef RE1_LOCALS_H\n#define RE1_LOCALS_H\n\n" + body + "\n\n#endif /* RE1_LOCALS_H */\n")
    return specs


def find_macros(text):
    """[(start, end, kind, base expression, accessed type, offset)] of every M2C field macro (balanced parse,
    so the base may be any expression)."""
    out = []
    for m in re.finditer(r"M2C_(FIELD|PFIELD|SFIELD)\(", text):
        i, depth = m.end(), 0
        while i < len(text):
            ch = text[i]
            if ch in "([":
                depth += 1
            elif ch in ")]":
                if depth == 0:
                    break
                depth -= 1
            elif ch == "," and depth == 0:
                break
            i += 1
        if i >= len(text) or text[i] != ",":
            continue
        tail = re.match(r",\s*([\w ]+?\*+),\s*(0x[0-9A-Fa-f]+|\d+)\)", text[i:i + 60])
        if not tail:
            continue
        out.append((m.start(), i + tail.end(), m.group(1), text[m.end():i].strip(),
                    " ".join(tail.group(1).split())[:-1].rstrip(), int(tail.group(2), 0)))
    return out


OFFSET_BASE = re.compile(r"^\(?\s*(\w+)\s*[-+]\s*[^,]+?\)?$")      # (V + 0x6C8), (V - 0x7C), (V + (n * 0x7C))


def base_var(base):
    """V for a base `(V +/- ...)` (balanced), else None."""
    m = OFFSET_BASE.match(base)
    if not m:
        return None
    inner = base[1:-1] if base.startswith("(") and base.endswith(")") else base
    depth = 0
    for ch in inner:                                     # the outer parentheses must belong together
        depth += ch == "("
        depth -= ch == ")"
        if depth < 0:
            return None
    return m.group(1)


def offset_base(base, vars_, spec):
    """True for an array element address built from a qualifying variable: `(V + 0x7C)`, `(V - (n * 0x7C))`,
    `((n * 0x7C) + V + 0xF8)` - only for specs with a stride, and every constant / multiplier must be a
    multiple of it (an entity's `(e + 0x16C)` is the enemy work area, NOT another entity)."""
    if not spec.stride:
        return False
    s = base.strip()
    names = set(re.findall(r"\b[A-Za-z_]\w*\b", s))
    hits = [n for n in names if n in vars_]
    if len(hits) != 1:
        return False
    v = re.escape(hits[0])
    if re.search(r"[A-Za-z_]\w*\(\s*" + v + r"\b", s) or re.search(r"\b" + v + r"\s*\)?\s*(->|\.|\[)", s):
        return False                   # inside a call / a member of it: `JOINT(arg0)->anim_slot` is not arg0 + N
    rest = re.sub(r"\b" + re.escape(hits[0]) + r"\b", "", s)
    rest = re.sub(r"\b[A-Za-z_]\w*\([^()]*\)(->\w+|\.\w+)*", "X", rest)          # MACRO(e)->field values
    rest = re.sub(r"\*\(\w+ \*\)0x[0-9A-Fa-f]+|\(\w+\)|\b(?!0x)[A-Za-z_]\w*(->\w+|\.\w+)*", "X", rest)
    for m in re.finditer(r"(0x[0-9A-Fa-f]+|\b\d+\b)", rest):
        if int(m.group(1), 0) % spec.stride:
            return False
    return bool(re.fullmatch(r"[\s()+\-*X0-9A-Fa-fx]*", rest))


def blocks(text):
    """[(open, close, header)] of the brace blocks in text (header = the text before the `{`)."""
    out, stack = [], []
    for i, ch in enumerate(text):
        if ch == "{":
            j = max(text.rfind(";", 0, i), text.rfind("}", 0, i), text.rfind("{", 0, i))
            stack.append((i, text[j + 1:i].strip()))
        elif ch == "}" and stack:
            o, h = stack.pop()
            out.append((o, i, h))
    return out


def flow_ok(text, var, pos, spec, blks):
    """Per-use typing for a variable whose assignments in the function mix several kinds (m2c reuses one
    name per stack slot): the use at pos qualifies when the nearest assignment A before it matches the
    spec's source, A's block contains the use, no other assignment to var lies between them, and no loop
    contains the use without also containing A (a later assignment could come round the loop)."""
    if spec.source is None:
        return False
    assigns = [(m.start(), m.group(1)) for m in
               re.finditer(r"(?<![\w.>])" + re.escape(var) + r"\s*=(?!=)\s*([^;]+);", text) if m.start() < pos]
    if not assigns:
        return False
    a_pos, rhs = assigns[-1]
    if not spec.source.search(rhs):
        return False
    inner_a = min((b for b in blks if b[0] < a_pos < b[1]), key=lambda b: b[1] - b[0], default=None)
    if inner_a is None or not (inner_a[0] < pos < inner_a[1]):
        return False
    all_assigns = [m.start() for m in re.finditer(r"(?<![\w.>])" + re.escape(var) + r"\s*=(?!=)", text)]
    for o, c, h in blks:                                 # a loop around the use that reassigns var
        if o < pos < c and not (o < a_pos < c) and re.search(r"\b(while|for|do)\b", h) \
                and any(o < p < c for p in all_assigns):
            return False
    return True


def qualifies(base, vars_, spec):
    if base in vars_:
        return True
    if any(r.search(base) for r in spec.exprs):
        return True
    return offset_base(base, vars_, spec)


def qualifying_vars(body, spec, func=None):
    """Variables of this function that point to spec.struct (every assignment matches spec.source),
    plus the parameters the spec lists for this function (`param`)."""
    out = set(spec.params.get(func, ()))
    if spec.source is None:
        return out
    cands = set()
    for _, _, _, base, _, _ in find_macros(body):
        cands.add(base_var(base) or base)
        cands |= set(re.findall(r"\b(?:sp[0-9A-F]+|temp_\w+|var_\w+|arg\d)\b", base))
    for var in cands:
        if not re.match(r"^\w+$", var):
            continue
        if var.startswith(("g_", "D_")) or var[0].isdigit():
            continue
        assigns = re.findall(r"(?<![\w.>])" + re.escape(var) + r"\s*=(?!=)\s*([^;]+);", body)
        if not assigns:
            continue
        if all(spec.source.search(a) or re.match(r"\(?\s*(\(\w+ \*+\))?\s*" + re.escape(var) + r"\b", a.strip())
               for a in assigns):
            out.add(var)
    return out


def rewrite(text, spec, main, level, vars_):
    out, pos, n = [], 0, 0
    W, M = spec.struct, spec.macro
    found = []
    blks = None
    for s, e, kind, base, ty, off in find_macros(text):
        if "M2C_" in base:
            continue                                    # (a nested macro base is converted on a later run)
        if not qualifies(base, vars_, spec):
            v = base if re.match(r"^\w+$", base) else base_var(base)
            if not v:                                   # `((n * 0xF8) + sp18)`: the one local variable in it
                locs = set(re.findall(r"\b(?:sp[0-9A-F]+|temp_\w+|var_\w+|arg\d|[a-z]\w{0,3})\b", base)) - {"s32", "u8", "s8", "u16", "s16", "u32", "void"}
                v = locs.pop() if len(locs) == 1 else None
            if not v or v.startswith(("g_", "D_")):
                continue
            blks = blks if blks is not None else blocks(text)
            if not flow_ok(text, v, s, spec, blks):
                continue
            if v != base and not offset_base(base, {v}, spec):
                continue
        form = "S" if kind == "SFIELD" or (kind == "FIELD" and not main) else "P"
        found.append((s, e, form, ty, off, base))      # the macros parenthesise their argument
    for v in ([] if spec.noaddr else vars_):
        for m in re.finditer(r"(?<![\w.>*&])" + re.escape(v) + r" \+ (0x[0-9A-Fa-f]+|\d+)\b(?!\s*[-+*\[])", text):
            before = text[max(0, m.start() - 12):m.start()]
            if re.search(r"\)\s*$", before) or before.rstrip().endswith(("*", "(s32)", "(u32)", "+", "-")):
                continue                              # a cast/deref in front, or part of a longer sum
                                                      # (`n * 0x7C + v + 0x44`: a typed address would scale)
            found.append((m.start(), m.end(), "D", None, int(m.group(1), 0), v))
    for s, e, form, ty, off, v in sorted(found):
        if s < pos:
            continue
        if form == "D":                               # an address: the field starting there (first element)
            if (spec.stride and off >= spec.stride) or (spec.max is not None and off >= spec.max):
                continue
            hit = [name for o, fty, name, arr, _ in spec.fields if o == off and not arr and not name.startswith("pad")]
            if not hit:
                continue
            new = f"&{M}({v})->{hit[0]}"
        else:
            p = spec.path_at(off, ty)
            if p is None or tsize(ty) is None:
                continue
            path, fty = p
            same = ty == fty
            before = text[max(0, s - 3):s].rstrip()
            lval = (ew.LVAL_AFTER.match(text[e:e + 4]) or ew.LVAL_BEFORE.search(text[max(0, s - 3):s])) \
                and not re.search(r"(^|[^\w)\]])\*\s*$", text[max(0, s - 3):s])   # `*X = ...` writes through X
            addr = before.endswith("&") and not before.endswith("&&")
            simple = path.startswith("->")
            if form == "P" and not main or not simple or level == "exact" or addr or tsize(ty) != tsize(fty):
                if form == "S" and same and simple and level != "exact":   # (a plain member compiles like
                    # M2C_SFIELD almost always - yawn_update's `->index == 3` didn't: the exact level uses MEMBER_AT)
                    new = f"{M}({v}){path}"
                else:
                    new = f"{'MEMBER_AT' if form == 'S' else 'FIELD_AT'}({v}, {W}, {path}, {ty})"
            elif same or lval:
                new = f"{M}({v}){path}"
            else:
                new = f"(({ty}) {M}({v}){path})"
        out.append(text[pos:s])
        out.append(new)
        pos = e
        n += 1
    out.append(text[pos:])
    return "".join(out), n


def process(path, spec):
    main = path.replace(os.sep, "/").startswith("src/main/")
    original = open(path).read()
    lines = original.split("\n")
    spans = ig.function_spans(original)
    names = [n for _, _, n in spans]
    level = {n: "cast" for n in names}
    fvars = {n: qualifying_vars("\n".join(lines[s - 1:e]), spec, n) for s, e, n in spans}

    def assemble():
        out, pos, total = [], 0, 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            body = "\n".join(lines[s - 1:e])
            if level[n] != "orig":
                body, c = rewrite(body, spec, main, level[n], fvars[n])
                total += c
            out += body.split("\n")
            pos = e
        return "\n".join(out + lines[pos:]), total

    text, total = assemble()
    if total == 0:
        return
    for _ in range(20):
        text, total = assemble()
        ig.write_file(path, text)
        res, errs = ew.check(path, names)
        if errs:
            spans_now = ig.function_spans(text)
            tail = path.replace(os.sep, "/").split("src/")[-1]
            bad = {n for f, ln, _ in errs for s, e, n in spans_now
                   if f.replace(os.sep, "/").endswith(tail) and s <= int(ln) <= e} - {n for n in names if level[n] == "orig"}
            if bad:
                for n in bad:
                    level[n] = "orig"
                print(f"  compile errors in {len(bad)} function(s), keeping their old text: {' '.join(sorted(bad)[:4])}")
                continue
            print("compile errors outside functions, restoring:", *[f"  {f}:{ln}: {m}" for f, ln, m in errs], sep="\n")
            break
        ok_all, bad = res
        if ok_all:
            kept = [n for n in names if level[n] == "orig" and fvars[n]]
            print(f"OK: {path}: {total} uses via {spec.macro}(); exact forms in "
                  f"{sum(1 for n in names if level[n] == 'exact')} function(s); old text kept in {len(kept)}"
                  + (f": {' '.join(kept[:8])}" if kept else ""))
            return
        if not bad:
            print("build failed for another reason, restoring")
            break
        for n in bad:
            level[n] = "exact" if level[n] == "cast" else "orig"
        print(f"  {len(bad)} function(s) changed: {' '.join(sorted(bad)[:6])}")
    ig.write_file(path, original)
    for prog, f in ew.programs_of(path):
        ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))


def main():
    if sys.argv[1:] == ["header"]:
        write_header()
        return
    if len(sys.argv) != 3 or sys.argv[1] != "apply":
        sys.exit(__doc__)
    write_header()
    spec = Spec(sys.argv[2])
    for f in sorted(glob.glob(os.path.join("src", "*", "*.c"))):
        p = f.replace(os.sep, "/")
        if (spec.files and not spec.files.search(p)) or any(r.search(p) for r in spec.excludes):
            continue
        if '#include "../shared/' not in open(f).read():
            process(f, spec)


if __name__ == "__main__":
    main()
