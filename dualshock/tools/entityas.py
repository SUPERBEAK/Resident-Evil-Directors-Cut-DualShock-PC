"""entityas.py - the remaining g_CurrentEntity field macros -> named Entity fields (any type).

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python:
    python tools/entityas.py <C file> [...]      # src/main/door.c, src/shared/zombie.c, ...
    python tools/entityas.py --all

tools/entityfields.py named the accesses whose type equals the field's. The rest read a field with
another type (s16 for a u16 field, void * for an s32 field, ...) or use the pointer-arithmetic form.
Per use of g_CurrentEntity at an offset where an Entity field starts (below 0x16C, the enemy area):
  readable first   g_CurrentEntity->action_ticks_counter, a cast when read with another type
                   ((s16) g_CurrentEntity->action_ticks_counter), plain when written
  exact fallback   ENTITY_AS(g_CurrentEntity, action_ticks_counter, s16)       (member form)
                   ENTITY_FIELD(_AS)(g_CurrentEntity, action_ticks_counter[, s16]) (pointer form)
  addresses        (u8 *)g_CurrentEntity + 0x34  ->  &g_CurrentEntity->pos_x
Which macro is which form: M2C_SFIELD = member, M2C_PFIELD = pointer arithmetic, M2C_FIELD = member
in the -O0 overlays and pointer arithmetic in the -O2 main program (Makefile cppdefs_for). At -O0 the
pointer form compiles differently from a member, so there it goes straight to ENTITY_FIELD(_AS); at
-O2 both compile alike and the readable form is tried first. Every program that compiles the file is
rebuilt; functions whose bytes change fall back (readable -> exact -> old text).
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enemywork as ew  # noqa: E402
import entityfields as ef  # noqa: E402
import integrate as ig  # noqa: E402

E = "g_CurrentEntity"
MAC_RE = re.compile(r"M2C_(FIELD|PFIELD|SFIELD)\(\(?" + E + r"\)?, ([\w ]+?\*+), (0x[0-9A-Fa-f]+|\d+)\)")
ADDR_RE = re.compile(r"\(u8 \*\)" + E + r" \+ (0x[0-9A-Fa-f]+)\b(?!\s*[-+\[])")
SIZES = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}
FIELDS = None


def size(ty):
    return 4 if "*" in ty else SIZES.get(ty)


def field_at(off):
    for (o, ty), name in FIELDS.items():
        if o == off and not re.match(r"(pad|sub)[0-9A-Fa-f]+$", name):
            return name, ty
    return None


def uses(text, main):
    out = []
    for m in MAC_RE.finditer(text):
        off = int(m.group(3), 0)
        if off >= 0x16C:
            continue
        kind = m.group(1)
        form = "S" if kind == "SFIELD" or (kind == "FIELD" and not main) else "P"
        out.append((m.start(), m.end(), form, " ".join(m.group(2).split())[:-1].rstrip(), off))
    for m in ADDR_RE.finditer(text):
        off = int(m.group(1), 16)
        if 0 < off < 0x16C:
            out.append((m.start(), m.end(), "D", None, off))
    return sorted(out)


def rewrite(text, main, level):
    out, pos, n = [], 0, 0
    for s, e, form, ty, off in uses(text, main):
        f = field_at(off)
        if f is None:
            continue
        name, fty = f
        if form == "D":
            new = f"&{E}->{name}"
        else:
            if size(ty) is None:
                continue
            same = ty == fty
            lval = (ew.LVAL_AFTER.match(text[e:e + 4]) or ew.LVAL_BEFORE.search(text[max(0, s - 3):s])) \
                and not re.search(r"(^|[^\w)\]])\*\s*$", text[max(0, s - 3):s])   # `*X = ...` writes through X
            addr = text[max(0, s - 3):s].rstrip().endswith("&") and not text[max(0, s - 3):s].rstrip().endswith("&&")
            if form == "P" and not main:            # -O0 pointer form: never a plain member
                new = f"ENTITY_FIELD({E}, {name})" if same else f"ENTITY_FIELD_AS({E}, {name}, {ty})"
            elif level == "exact" or addr or size(ty) != size(fty):
                if form == "S":
                    new = f"{E}->{name}" if same else f"ENTITY_AS({E}, {name}, {ty})"
                else:
                    new = f"ENTITY_FIELD({E}, {name})" if same else f"ENTITY_FIELD_AS({E}, {name}, {ty})"
            elif same or lval:
                new = f"{E}->{name}"
            else:
                new = f"(({ty}) {E}->{name})"
        out.append(text[pos:s])
        out.append(new)
        pos = e
        n += 1
    out.append(text[pos:])
    return "".join(out), n


def process(path):
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
                body, c = rewrite(body, main, level[n])
                total += c
            out += body.split("\n")
            pos = e
        return "\n".join(out + lines[pos:]), total

    text, total = assemble()
    if total == 0:
        print(f"{path}: nothing to convert")
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
                print(f"  compile errors in {len(bad)} function(s), keeping their old text")
                continue
            print("compile errors outside functions, restoring:", *[f"  {f}:{ln}: {m}" for f, ln, m in errs], sep="\n")
            break
        ok_all, bad = res
        if ok_all:
            kept = [n for n in names if level[n] == "orig"]
            print(f"OK: {path}: {total} uses named; exact forms in {sum(1 for n in names if level[n] == 'exact')} "
                  f"function(s); old text kept in {len(kept)}" + (f": {' '.join(kept[:8])}" if kept else ""))
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
    global FIELDS
    FIELDS = ef.entity_fields()
    args = sys.argv[1:]
    if args == ["--all"]:
        args = sorted(f for f in glob.glob(os.path.join("src", "*", "*.c"))
                      if E in open(f).read() and '#include "../shared/' not in open(f).read())
    elif not args:
        sys.exit(__doc__)
    for p in args:
        process(p)


if __name__ == "__main__":
    main()
