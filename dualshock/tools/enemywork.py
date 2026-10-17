"""enemywork.py - name the enemy-specific part of an entity (Entity bytes 0x16C onward) per enemy.

Run from dualshock/ (or a tools/workcopy.py copy) with the project venv python:
    python tools/enemywork.py survey <enemy file>          # offsets, types, counts, example uses
    python tools/enemywork.py apply <enemy file> <spec>    # struct + macros, rewrite, verify

<enemy file>: src/shared/<enemy>.c (built by every stage that wraps it) or src/stageN/<enemy>.c.
<spec> (config/enemy_work/<enemy>.txt):
    struct ZombieWork ZOMBIE            # struct name, macro prefix
    0x06 s16 move_timer   comment...    # offset from 0x16C, type, field name
    skip plant42_body_*                 # optional: functions (patterns) of another entity kind in the
                                        # same file, whose 0x16C.. means something else - left alone
    only plant42_body_*                 # optional: the opposite (a second spec for that entity kind;
                                        # apply both specs to the file, each adds its own struct)
Each enemy uses Entity+0x16C differently, so each enemy file gets its own struct. Capcom's code
reached those bytes in two ways that GCC 2.6.3 -O0 compiles differently (see include/re1_types.h):
  A  M2C_FIELD(((u8 *)g_CurrentEntity + 0x16C), s16 *, 6)  ->  ZOMBIE(g_CurrentEntity)->move_timer
  B  M2C_PFIELD(g_CurrentEntity, s16 *, 0x172)               ->  ZOMBIE_FIELD(g_CurrentEntity, move_timer)
  C  M2C_FIELD(g_CurrentEntity, s16 *, 0x172)                ->  ZOMBIE_MEMBER(g_CurrentEntity, move_timer)
  D  (u8 *)g_CurrentEntity + 0x172 (an address)             ->  &ZOMBIE(g_CurrentEntity)->move_timer
A use with another type than the field's (s16 field read as u16) is written as a cast when read,
`(u16) ZOMBIE(e)->move_timer`, and plainly when written (a store of the same size is the same
instruction). The file is rebuilt in every program that compiles it; in a function whose bytes
changed, those uses become the exact form `ZOMBIE_AS(e, move_timer, u16)` / `ZOMBIE_FIELD_AS(...)`,
and if it still differs the function keeps its original macros.
"""
import collections
import fnmatch
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import declclean as dc  # noqa: E402
import integrate as ig  # noqa: E402

BASE = r"g_CurrentEntity"
# A: M2C_FIELD on the base pointer entity+0x16C (M2C_FIELD = the struct form at -O0)
A_RE = re.compile(r"M2C_FIELD\(\(\(u8 \*\)" + BASE + r" \+ 0x16C\), ([\w ]+?\*+), (0x[0-9A-Fa-f]+|\d+)\)")
# B: M2C_PFIELD on the entity with the full offset
B_RE = re.compile(r"M2C_PFIELD\(" + BASE + r", ([\w ]+?\*+), (0x[0-9A-Fa-f]+)\)")
# C: M2C_FIELD (struct form) on the entity with the full offset
C_RE = re.compile(r"M2C_FIELD\(" + BASE + r", ([\w ]+?\*+), (0x[0-9A-Fa-f]+)\)")
# D: a field's address, `(u8 *)g_CurrentEntity + 0x179` (not followed by more pointer arithmetic)
D_RE = re.compile(r"\(u8 \*\)" + BASE + r" \+ (0x1[6-8][0-9A-Fa-f])\b(?!\s*[-+\[])")
# writes: `X = ...` (not ==), compound assignments, ++/--
LVAL_AFTER = re.compile(r"^\s*(?:=(?!=)|\+\+|--|[-+*/%&|^]=|<<=|>>=)")
LVAL_BEFORE = re.compile(r"(?:\+\+|--|&)\s*$")


def uses(text):
    """[(start, end, form, type, offset from 0x16C)] of the enemy-area accesses in text
    (form D, a field's address, has type None)."""
    out = []
    for m in A_RE.finditer(text):
        out.append((m.start(), m.end(), "A", " ".join(m.group(1).split()), int(m.group(2), 0)))
    for rx, form in ((B_RE, "B"), (C_RE, "C")):
        for m in rx.finditer(text):
            o = int(m.group(2), 16)
            if 0x16C <= o < 0x18C:
                out.append((m.start(), m.end(), form, " ".join(m.group(1).split()), o - 0x16C))
    for m in D_RE.finditer(text):
        o = int(m.group(1), 16)
        if 0x16C < o < 0x18C:
            out.append((m.start(), m.end(), "D", None, o - 0x16C))
    return sorted(out)


def survey(path):
    text = open(path).read()
    acc = collections.defaultdict(collections.Counter)
    examples = collections.defaultdict(list)
    for s, e, form, ty, off in uses(text):
        acc[off][(ty, form)] += 1
        line = text[text.rfind("\n", 0, s) + 1:text.find("\n", e)].strip()
        if len(examples[off]) < 3 and line not in examples[off]:
            examples[off].append(line)
    for off in sorted(acc):
        kinds = ", ".join(f"{ty or 'address'} {form}:{n}" for (ty, form), n in acc[off].most_common())
        print(f"+0x{off:02X} (entity 0x{0x16C + off:X}): {kinds}")
        for ex in examples[off]:
            print("      " + ex[:150])


def read_spec(path):
    lines = [l.split("#")[0].rstrip() for l in open(path)]
    lines = [l for l in lines if l.strip()]
    _, struct, prefix = lines[0].split()
    fields, skip = [], []
    for l in lines[1:]:
        if l.split()[0] in ("skip", "only"):       # `skip plant42_body_*`: functions of another entity;
            skip += [("" if l.split()[0] == "skip" else "!") + p for p in l.split()[1:]]   # `only`: just these
            continue
        m = re.match(r"\s*(0x[0-9A-Fa-f]+)\s+(.+?)\s+(\w+)(\[\w+\])?\s*$", l)
        off, ty, name, arr = int(m.group(1), 16), " ".join(m.group(2).split()), m.group(3), m.group(4) or ""
        fields.append((off, ty, name, arr))
    comments = {}
    for l in open(path):
        m = re.match(r"\s*0x[0-9A-Fa-f]+\s+.+?\s+(\w+)(?:\[\w+\])?\s*#\s*(.+)$", l)
        if m:
            comments[m.group(1)] = m.group(2).strip()
    return struct, prefix, sorted(fields), comments, skip


SIZES = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4}


def size_of(ty, arr=""):
    n = int(arr[1:-1], 0) if arr else 1
    return (4 if "*" in ty else SIZES[ty]) * n


def struct_text(struct, prefix, fields, comments):
    out = [f"/* {struct}: the {prefix.lower()}'s own use of entity bytes 0x16C.. (tools/enemywork.py,",
           f"   config/enemy_work). {prefix}(e)->f, {prefix}_FIELD(e, f) and {prefix}_MEMBER(e, f) reach the same",
           "   bytes; Capcom's code used all three and GCC 2.6.3 compiles them differently (re1_types.h). */",
           f"typedef struct {struct} {{"]
    rows, pos = [], 0
    for off, ty, name, arr in fields:
        if off > pos:
            rows.append((pos, f"u8 pad{pos:02X}[{off - pos}];", ""))
        stars = ty.count("*")
        base = " ".join(w for w in ty.split() if w != "*")
        rows.append((off, f"{base} {'*' * stars}{name}{arr};", comments.get(name, "")))
        pos = off + size_of(ty, arr)
    width = max(len(d) for _, d, _ in rows)
    for off, decl, c in rows:                            # comments in one column
        out.append(f"    /* +0x{off:02X} */ {decl.ljust(width)}{'  /* ' + c + ' */' if c else ''}".rstrip())
    out.append(f"}} {struct};")
    out.append(f"#define {prefix}(e) ENEMY_WORK(e, {struct})")
    out.append(f"#define {prefix}_AS(e, f, T) ENEMY_WORK_AS(e, {struct}, f, T)")
    out.append(f"#define {prefix}_FIELD(e, f) ENEMY_FIELD(e, {struct}, f)")
    out.append(f"#define {prefix}_FIELD_AS(e, f, T) ENEMY_FIELD_AS(e, {struct}, f, T)")
    out.append(f"#define {prefix}_MEMBER(e, f) ENEMY_MEMBER(e, {struct}, f)")
    out.append(f"#define {prefix}_MEMBER_AS(e, f, T) ENEMY_MEMBER_AS(e, {struct}, f, T)")
    return out


def drop_unused_macros(text, prefix):
    """text without the `#define PREFIX_AS / _FIELD / _FIELD_AS / _MEMBER / _MEMBER_AS` lines whose macro
    is not used anywhere in the file (PREFIX(e) itself always stays). Removing a macro definition
    cannot change the generated code."""
    out = []
    for line in text.split("\n"):
        m = re.match(r"#define (" + re.escape(prefix) + r"_(?:AS|FIELD|FIELD_AS|MEMBER|MEMBER_AS))\(", line)
        if m and len(re.findall(r"\b" + m.group(1) + r"\(", text)) == 1:
            continue
        out.append(line)
    return "\n".join(out)


def rewrite(text, prefix, fields, exact):
    """text with the enemy-area uses replaced; `exact`: use the _AS forms for type mismatches.
    Returns (new text, number of uses replaced)."""
    by_off = {off: (ty, name, arr) for off, ty, name, arr in fields}
    out, pos, n = [], 0, 0
    for s, e, form, ty, off in uses(text):
        if off not in by_off or by_off[off][2]:          # unknown offset / array field: leave it
            continue
        fty, name, _ = by_off[off]
        if form == "D":
            new = f"&{prefix}(g_CurrentEntity)->{name}"
        else:
            lval = LVAL_AFTER.match(text[e:e + 4]) or LVAL_BEFORE.search(text[max(0, s - 3):s])
            t = ty[:-1].rstrip()                          # the accessed type (`s16 *` -> s16)
            same = " ".join(t.split()) == fty
            same_size = size_of(t) == size_of(fty) if (t in SIZES or "*" in t) else False
            macro = {"A": prefix, "B": prefix + "_FIELD", "C": prefix + "_MEMBER"}[form]
            plain = (f"{prefix}(g_CurrentEntity)->{name}" if form == "A"
                     else f"{macro}(g_CurrentEntity, {name})")
            exact_form = f"{macro}_AS(g_CurrentEntity, {name}, {t})"
            if same:
                new = plain
            elif exact or not same_size or text[max(0, s - 3):s].rstrip().endswith("&"):
                new = exact_form
            elif lval:
                new = plain
            else:
                new = f"(({t}) {plain})"
        out.append(text[pos:s])
        out.append(new)
        pos = e
        n += 1
    out.append(text[pos:])
    return "".join(out), n


def programs_of(path):
    """[(program, wrapper or file)] that compile `path`."""
    p = path.replace(os.sep, "/")
    if p.startswith("src/shared/"):
        e = os.path.basename(p)[:-2]
        res = []
        for w in sorted(os.listdir("src")):
            f = os.path.join("src", w, e + ".c")
            if w.startswith("stage") and os.path.isfile(f) and f'#include "../shared/{e}.c"' in open(f).read():
                res.append((w, f))
        return res
    return [(p.split("/")[1], path)]


def check(path, names):
    """{function: True} for the functions of `names` whose bytes changed in any program building path."""
    bad, ok_all = set(), True
    for prog, f in programs_of(path):
        t = ig.Target(prog + "/" + os.path.basename(f)[:-2])
        ok, log = ig.build(t)
        if ok:
            continue
        ok_all = False
        errs = re.findall(r"(\S+\.c):(\d+): (?!warning)(.*)", log)
        if errs:
            return None, errs[:10]
        bad |= set(dc.bad_functions(t, names))
    return (ok_all, bad), None


def apply(path, spec):
    struct, prefix, fields, comments, skip = read_spec(spec)
    original = open(path).read()
    current = original
    if f"typedef struct {struct} " in current:     # again (new spec / new forms): rebuild the struct block
        m = re.search(r"/\* " + struct + r":.*?\n(?:#define " + prefix + r"\w*\([^\n]*\n)+\n?", current, re.S)
        if not m:
            sys.exit(f"{path}: can't find the whole {struct} block to replace")
        current = current[:m.start()] + current[m.end():]
    lines = current.split("\n")
    spans = ig.function_spans(current)
    first = spans[0][0] - 1                                  # insert the struct before the first function
    head = "\n".join(lines[:first])
    funcs = [(n, "\n".join(lines[s - 1:e])) for s, e, n in spans]
    gaps = ["\n".join(lines[(spans[k - 1][1] if k else first):s - 1]) for k, (s, e, n) in enumerate(spans)]
    tail = "\n".join(lines[spans[-1][1]:])
    names = [n for n, _ in funcs]
    # cast -> exact -> orig; `skip` functions (another entity's code in the same file) stay as they are
    only = [p[1:] for p in skip if p.startswith("!")]
    skip = [p for p in skip if not p.startswith("!")]
    level = {n: "orig" if any(fnmatch.fnmatch(n, p) for p in skip)
             or (only and not any(fnmatch.fnmatch(n, p) for p in only)) else "cast" for n in names}

    def assemble():
        parts, total = [head.rstrip("\n"), "", *struct_text(struct, prefix, fields, comments), ""], 0
        for k, (n, body) in enumerate(funcs):
            if level[n] != "orig":
                body, c = rewrite(body, prefix, fields, level[n] == "exact")
                total += c
            gap = gaps[k] if k else ""
            parts.append(gap + ("\n" if k else "") + body if k else body)
        return "\n".join(parts) + "\n" + tail, total

    for attempt in range(20):
        text, total = assemble()
        ig.write_file(path, text)
        res, errs = check(path, names)
        if errs:
            print("compile errors, restoring:", *[f"  {f}:{ln}: {m}" for f, ln, m in errs], sep="\n")
            ig.write_file(os.path.join("build", "enemywork_failed.c"), text)
            break
        ok_all, bad = res
        if ok_all:
            # only the view macros the file uses stay defined (2026-10-16: ~46 unused _AS variants removed)
            text = drop_unused_macros(text, prefix)
            ig.write_file(path, text)
            kept = [n for n in names if level[n] == "orig"]
            exact = [n for n in names if level[n] == "exact"]
            left = len(uses(text))
            print(f"OK: {path}: {total} uses named, {left} left as macros; exact forms in {len(exact)} "
                  f"function(s), original text kept in {len(kept)}: {' '.join(kept)}")
            return
        if not bad:
            print("build failed for another reason, restoring")
            break
        for n in bad:
            level[n] = "exact" if level[n] == "cast" else "orig"
        print(f"  {len(bad)} function(s) changed: {' '.join(sorted(bad)[:8])}")
    ig.write_file(path, original)
    for prog, f in programs_of(path):
        ig.build(ig.Target(prog + "/" + os.path.basename(f)[:-2]))
    sys.exit(1)


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "survey":
        survey(sys.argv[2])
    elif len(sys.argv) == 4 and sys.argv[1] == "apply":
        apply(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
