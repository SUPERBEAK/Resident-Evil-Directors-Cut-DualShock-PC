"""entityfields.py - replace m2c field macros on the current object by real Entity fields.

Run from dualshock/ with the project venv python (overlays; -O0 builds):
    python tools/entityfields.py --target stage1

`M2C_FIELD(g_CurrentEntity, u8 *, 0x87)` becomes `g_CurrentEntity->action_state` when the Entity
struct (include/re1_types.h) has a field at that offset with exactly that type. Per C file:
  1. the file-scope `extern void *g_CurrentEntity;` becomes `extern Entity *g_CurrentEntity;`;
     pointer arithmetic on it gets a `(u8 *)` cast (`(u8 *)g_CurrentEntity + 0x16C` - same bytes,
     without the cast C would scale by sizeof(Entity)); aliases declared `Entity *` become the
     real name (same type now),
  2. M2C_FIELD / M2C_SFIELD on it (or on such an alias) with a matching field are converted.
     M2C_PFIELD is left alone (it is pointer arithmetic, chosen on purpose during matching: at -O0
     it compiles differently from a struct member),
  3. rebuild; functions whose bytes change get their step-2 conversions undone, until all match.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402
import declclean as dc  # noqa: E402  (bad_functions)

NAME = "g_CurrentEntity"


def entity_fields():
    """{(offset, type): field name} from the Entity struct (scalar fields only)."""
    h = open(os.path.join("include", "re1_types.h")).read()
    body = h[h.index("typedef struct Entity {"):h.index("} Entity;")]
    out = {}
    for m in re.finditer(r"/\* 0x([0-9A-Fa-f]+) \*/\s*([us](?:8|16|32))\s+(\w+)\s*;", body):
        out[(int(m.group(1), 16), m.group(2))] = m.group(3)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", required=True)
    ap.add_argument("--loose", action="store_true",
                    help="also convert accesses whose signedness differs from the field (u8/s8, u16/s16, u32/s32); kept only where the function still matches")
    a = ap.parse_args()
    t = ig.Target(a.target)
    original = open(t.cfile).read()
    fields = entity_fields()

    # 1. file-level changes (always kept)
    text = original
    if f"extern void *{NAME};" not in text and f"extern Entity *{NAME};" not in text:  # (either is fine)
        sys.exit(f"no file-scope `extern void *{NAME};` in {t.cfile}")
    text = text.replace(f"extern void *{NAME};", f"extern Entity *{NAME};", 1)
    # aliases with type Entity * -> the real name, per function (the same alias name can be declared
    # with another type in other functions: main g_CurrentEntity_l is u8 * / s32 elsewhere)
    alias_re = re.compile(r'^\s+extern\s+Entity\s*\*\s*(\w+)\s*__asm__\("' + NAME + r'"\);\s*$')
    tl = text.split("\n")
    out_lines, pos, aliases = [], 0, set()
    for s0, e0, _ in ig.function_spans(text):
        out_lines += tl[pos:s0 - 1]
        body = tl[s0 - 1:e0]
        mine = {m.group(1) for m in (alias_re.match(l) for l in body) if m}
        if mine:
            aliases |= mine
            body = [l for l in body if not alias_re.match(l)]
            for al in mine:
                body = [ig.sub_code(r"(?<![\w.])" + re.escape(al) + r"(?!\w)", NAME, l) for l in body]
        out_lines += body
        pos = e0
    text = "\n".join(out_lines + tl[pos:])
    # pointer arithmetic: `g_CurrentEntity + x`, `g_CurrentEntity - x`, `x + g_CurrentEntity`
    text = ig.sub_code(r"(?<![\w.>)])" + NAME + r"(?=\s*[-+](?![-+=>]))", f"(u8 *){NAME}", text)
    text = ig.sub_code(r"(?<=[^+-]\+\s)" + NAME + r"(?![\w-])", f"(u8 *){NAME}", text)

    # 2. macro conversions, per function
    macro = re.compile(r"M2C_S?FIELD\(" + NAME + r", ([us](?:8|16|32)) \*, (0x[0-9A-Fa-f]+|\d+)\)")
    lines = text.split("\n")
    spans = ig.function_spans(text)
    new_funcs, count = {}, 0

    def conv(m):
        nonlocal n_here
        off = int(m.group(2), 0)
        f = fields.get((off, m.group(1)))
        if f is None and a.loose:
            ty = m.group(1)
            f = fields.get((off, ("u" if ty[0] == "s" else "s") + ty[1:]))
        if f is None:
            return m.group(0)
        n_here += 1
        return f"{NAME}->{f}"
    for s, e, n in spans:
        n_here = 0
        new_funcs[n] = [ig.sub_code(macro.pattern, conv, l) if "M2C_" in l else l for l in lines[s - 1:e]]
        count += n_here
    print(f"{a.target}: {count} field macros convertible; {len(aliases)} Entity* alias name(s) merged")

    def assemble(keep_old):
        out, pos = [], 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            out += lines[s - 1:e] if n in keep_old else new_funcs[n]
            pos = e
        return "\n".join(out + lines[pos:])

    keep_old = set()
    for _ in range(60):
        ig.write_file(t.cfile, assemble(keep_old))
        ok, log = ig.build(t)
        if ok:
            print(f"OK: {a.target} matches; {len(keep_old)} function(s) kept their macros")
            return
        errs = re.findall(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): (?!warning)(.*)", log)
        if errs:
            in_funcs = {n for ln, _ in errs for s, e, n in spans if s <= int(ln) <= e} - keep_old
            outside = [ln for ln, _ in errs if not any(s <= int(ln) <= e for s, e, _ in spans)]
            if in_funcs and not outside:
                print(f"  compile errors in {len(in_funcs)} function(s), keeping their macros: "
                      f"{' '.join(sorted(in_funcs)[:8])}")
                keep_old |= in_funcs
                continue
            ig.write_file(os.path.join("build", "entityfields_failed.c"), open(t.cfile).read())
            print("compile errors (file kept as build/entityfields_failed.c), restoring:")
            for ln, msg in errs[:10]:
                print(f"  line {ln}: {msg}")
            break
        bad = dc.bad_functions(t, [n for _, _, n in spans if n not in keep_old])
        if not bad:
            print("build failed for another reason, restoring:\n" + log[-2000:])
            break
        print(f"  {len(bad)} function(s) changed bytes, keeping their macros: {' '.join(bad[:8])}")
        keep_old |= set(bad)
    ig.write_file(t.cfile, original)
    ig.build(t)
    sys.exit(1)


if __name__ == "__main__":
    main()
