"""structview.py - give a struct to data reached through one pointer/global, from a spec, keeping the byte match.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python:
    python tools/structview.py header                       # regenerate include/re1_views.h from all specs
    python tools/structview.py apply config/structs/x.txt   # header + rewrite every file that uses the base

Spec (config/structs/<name>.txt):
    struct InteractiveScreen                                   # the struct type
    view ISCREEN = ((InteractiveScreen *)g_InteractiveScreenStatePtr)   # short macro for the pointer
    base g_InteractiveScreenStatePtr                           # expression in the code (1+ lines per view;
                                                               # several view/base groups may share one struct)
    define                                                     # write the typedef from the lines below
    0x04 s32 cursor_col   # comment                            # offset, type, name
  or, for a struct already in a header:   fields Entity [max 0x16C]
The header include/re1_views.h (included by re1_types.h) gets per spec: the typedef (with `define`), the
view macro and VIEW_AS(f, T) / VIEW_FIELD(f) / VIEW_FIELD_AS(f, T) (MEMBER_AS / FIELD_OF(_AS) there).
Rewrites, per use of the base at an offset where a field starts (as tools/entityas.py does for
g_CurrentEntity): readable `VIEW->f` (a cast when read with another type, plain when written) ->
exact `VIEW_AS(f, T)` (member form) / `VIEW_FIELD(_AS)(...)` (pointer form) -> the old text, per
function, verified in every program that compiles the file. `(u8 *)BASE + off` -> `&VIEW->f`.
M2C_FIELD is the member form in the -O0 overlays and pointer arithmetic in the -O2 main program.
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enemywork as ew  # noqa: E402
import integrate as ig  # noqa: E402

HEADER = os.path.join("include", "re1_views.h")
SIZES = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}
FIELD_LINE = re.compile(r"\s*(0x[0-9A-Fa-f]+)\s+(.+?)\s+(\w+)(\[\w+\])?\s*(?:#\s*(.*))?$")


def size(ty, arr=""):
    n = int(arr[1:-1], 0) if arr else 1
    base = 4 if "*" in ty else SIZES.get(ty)
    return None if base is None else base * n


class Spec:
    def __init__(self, path):
        self.path = path.replace(os.sep, "/")
        self.views, self.fields, self.define, self.max = [], [], False, None   # views: [view, def, bases]
        self.struct = self.from_type = None
        for raw in open(path):
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            key = line.split()[0]
            if key == "struct":
                self.struct = line.split()[1]
            elif key == "view":
                m = re.match(r"view\s+(\w+)\s*=\s*(.+)$", line)
                self.views.append([m.group(1), m.group(2).split("  #")[0].strip(), []])
            elif key == "base":                          # belongs to the view above it
                self.views[-1][2].append(line.split(None, 1)[1].split("  #")[0].strip())
            elif key == "define":
                self.define = True
            elif key == "fields":
                parts = line.split()
                self.from_type = parts[1]
                if len(parts) >= 4 and parts[2] == "max":
                    self.max = int(parts[3], 0)
            else:
                m = FIELD_LINE.match(line)
                if m:
                    self.fields.append((int(m.group(1), 16), " ".join(m.group(2).split()), m.group(3),
                                        m.group(4) or "", (m.group(5) or "").strip()))
        if self.from_type:
            self.fields = header_fields(self.from_type)
        self.fields.sort()

    def field_at(self, off):
        if self.max is not None and off >= self.max:
            return None
        for o, ty, name, arr, _ in self.fields:
            if o == off and not arr and not re.match(r"(pad|sub)[0-9A-Fa-f]+$", name):
                return name, ty
        return None


def header_fields(typename):
    """[(offset, type, name, arr, '')] of `typedef struct X {...} X;` in include/*.h."""
    for h in glob.glob(os.path.join("include", "*.h")):
        t = open(h).read()
        m = re.search(r"typedef struct " + typename + r" \{(.*?)\} " + typename + ";", t, re.S)
        if m:
            return [(int(f.group(1), 16), " ".join(f.group(2).split()), f.group(3), f.group(4) or "", "")
                    for f in re.finditer(r"/\* 0x([0-9A-Fa-f]+) \*/\s*([\w ]+?\**)\s*(\w+)(\[[^\]]+\])?\s*;", m.group(1))]
    sys.exit(f"struct {typename} not found in include/")


def block(spec):
    out = [f"/* {spec.struct} via {', '.join(v for v, _, _ in spec.views)} ({spec.path}) */"]
    if spec.define:
        rows, pos = [], 0
        for off, ty, name, arr, c in spec.fields:
            if off > pos:
                rows.append((pos, f"u8 pad{pos:02X}[{off - pos}];", ""))
            stars = ty.count("*")
            base = " ".join(w for w in ty.split() if w != "*")
            rows.append((off, f"{base} {'*' * stars}{name}{arr};", c))
            pos = off + size(ty, arr)
        w = max(len(d) for _, d, _ in rows)
        out.append(f"typedef struct {spec.struct} {{")
        out += [f"    /* 0x{o:02X} */ {d.ljust(w)}{'  /* ' + c + ' */' if c else ''}".rstrip() for o, d, c in rows]
        out.append(f"}} {spec.struct};")
    s = spec.struct
    for v, vdef, _ in spec.views:
        out += [f"#define {v} {vdef}",
                f"#define {v}_AS(f, T) MEMBER_AS({v}, {s}, f, T)",
                f"#define {v}_FIELD(f) FIELD_OF({v}, {s}, f)",
                f"#define {v}_FIELD_AS(f, T) FIELD_OF_AS({v}, {s}, f, T)"]
    return "\n".join(out)


def write_header():
    specs = [Spec(p) for p in sorted(glob.glob(os.path.join("config", "structs", "*.txt")))]
    body = "\n\n".join(block(s) for s in specs)
    t = open(HEADER).read()
    head = t[:t.index("#define RE1_VIEWS_H") + len("#define RE1_VIEWS_H")]
    ig.write_file(HEADER, head + "\n\n" + body + "\n\n#endif /* RE1_VIEWS_H */\n")
    return specs


def uses(text, spec, main):
    out = []
    for v, _, bases in spec.views:
        for b in bases:
            mac = re.compile(r"M2C_(FIELD|PFIELD|SFIELD)\(" + re.escape(b) + r", ([\w ]+?\*+), (0x[0-9A-Fa-f]+|\d+)\)")
            for m in mac.finditer(text):
                kind = m.group(1)
                form = "S" if kind == "SFIELD" or (kind == "FIELD" and not main) else "P"
                out.append((m.start(), m.end(), form, " ".join(m.group(2).split())[:-1].rstrip(), int(m.group(3), 0), v))
            addr = re.compile(r"\(u8 \*\)\(?" + re.escape(b) + r"\)? \+ (0x[0-9A-Fa-f]+|\d+)\b(?!\s*[-+\[])")
            for m in addr.finditer(text):
                out.append((m.start(), m.end(), "D", None, int(m.group(1), 0), v))
    return sorted(out)


def rewrite(text, spec, main, level):
    out, pos, n = [], 0, 0
    for s, e, form, ty, off, v in uses(text, spec, main):
        if s < pos:
            continue
        f = spec.field_at(off)
        if f is None:
            continue
        name, fty = f
        if form == "D":
            if off == 0:
                continue
            new = f"&{v}->{name}"
        else:
            if size(ty) is None or size(fty) is None:
                continue
            same = ty == fty
            before = text[max(0, s - 3):s].rstrip()
            lval = (ew.LVAL_AFTER.match(text[e:e + 4]) or ew.LVAL_BEFORE.search(text[max(0, s - 3):s])) \
                and not re.search(r"(^|[^\w)\]])\*\s*$", text[max(0, s - 3):s])   # `*X = ...` writes through X
            addr = before.endswith("&") and not before.endswith("&&")
            if form == "P" and not main:
                new = f"{v}_FIELD({name})" if same else f"{v}_FIELD_AS({name}, {ty})"
            elif level == "exact" or addr or size(ty) != size(fty):
                if form == "S":
                    new = f"{v}->{name}" if same else f"{v}_AS({name}, {ty})"
                else:
                    new = f"{v}_FIELD({name})" if same else f"{v}_FIELD_AS({name}, {ty})"
            elif same or lval:
                new = f"{v}->{name}"
            else:
                new = f"(({ty}) {v}->{name})"
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

    def assemble():
        out, pos, total = [], 0, 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            body = "\n".join(lines[s - 1:e])
            if level[n] != "orig":
                body, c = rewrite(body, spec, main, level[n])
                total += c
            out += body.split("\n")
            pos = e
        return "\n".join(out + lines[pos:]), total

    text, total = assemble()
    if total == 0:
        print(f"{path}: nothing to convert")
        return True
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
                print(f"  compile errors in {len(bad)} function(s), keeping their old text")
                continue
            print("compile errors outside functions, restoring:", *[f"  {f}:{ln}: {m}" for f, ln, m in errs], sep="\n")
            break
        ok_all, bad = res
        if ok_all:
            kept = [n for n in names if level[n] == "orig"]
            print(f"OK: {path}: {total} uses via {spec.struct}; exact forms in "
                  f"{sum(1 for n in names if level[n] == 'exact')} function(s); old text kept in {len(kept)}"
                  + (f": {' '.join(kept[:8])}" if kept else ""))
            return True
        if not bad:
            print("build failed for another reason, restoring")
            break
        for n in bad:
            level[n] = "exact" if level[n] == "cast" else "orig"
        print(f"  {len(bad)} function(s) changed: {' '.join(sorted(bad)[:6])}")
    ig.write_file(path, original)
    for prog, f in ew.programs_of(path):
        ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))
    return False


def main():
    if sys.argv[1:] == ["header"]:
        write_header()
        return
    if len(sys.argv) != 3 or sys.argv[1] != "apply":
        sys.exit(__doc__)
    write_header()
    spec = Spec(sys.argv[2])
    files = sorted(f for f in glob.glob(os.path.join("src", "*", "*.c"))
                   if any(b in open(f).read() for _, _, bs in spec.views for b in bs) and '#include "../shared/' not in open(f).read())
    for f in files:
        process(f, spec)


if __name__ == "__main__":
    main()
