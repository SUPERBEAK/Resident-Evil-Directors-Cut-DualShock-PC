"""rawfield.py - hand raw byte-offset accesses to the struct tools (localview.py / structview.py).

Run from dualshock/ with the project venv python:
    python tools/rawfield.py to [files...]    # raw byte-offset accesses -> OFFSET_DEREF(base, T *, off)
    python tools/rawfield.py back [files...]  # every OFFSET_DEREF left -> raw form again
    python tools/rawfield.py count            # how many raw byte-offset accesses are left

The struct tools name fields of M2C_FIELD/PFIELD/SFIELD macros (and verify every rewrite with a build).
Since the raw field macros are gone (2026-10-16), the accesses still written by hand as
    *(T *)((u8 *)X + 0x34)      *(T *)(bytes + 0x34)  (bytes: a u8/s8/char/void pointer)    ((u8 *)X)[0x34]
are invisible to them. `to` rewrites exactly these into OFFSET_DEREF(X, T *, 0x34), which is defined as
`(*(T *)((s8 *)(X) + (0x34)))` - the same byte arithmetic, so the code doesn't change. Then run the
struct tools' `apply` with the specs that know the bases; whatever no spec claims goes back with `back`
(no OFFSET_DEREF is used in src/ otherwise, so every OFFSET_DEREF found is one of ours).
"""
import json
import os
import re
import sys

# `to` records the original text of every rewrite here, `back` puts it back
JOURNAL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "temp", "rawfield_journal.json")

SKIP = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', re.S)
TYPE = r"(?:(?:volatile|const|unsigned|signed|struct)\s+)*\w+\s*\*+"          # a pointer type `T *`
BYTE = r"(?:(?:unsigned|signed)\s+)?(?:u8|s8|char)"                          # a byte type
NUM = r"0x[0-9A-Fa-f]+|\d+"
OPERAND = r"[A-Za-z_]\w*(?:(?:->|\.)\w+)*"                                   # p, p->x, s.a->b
# declarations of byte pointers: `u8 *a, *b = ..;`, parameters `(u8 *arg0, ...)`
# (also aliases of globals: `extern u8 *g_RoomData_l __asm__("g_RoomData");`)
DECL = re.compile(rf"\b({BYTE}|void)\s+((?:\*\s*\w+\s*(?:=[^,;()]*)?,\s*)*\*\s*\w+)\s*(?=[;,)=]|__asm__)")
# element sizes for `((T *)X)[k]` = the bytes at k * size
ELEM = {"u8": 1, "s8": 1, "char": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4}


def files(args):
    if args:
        return args
    out = []
    for d, _, fs in os.walk("src"):
        out += [os.path.join(d, f) for f in sorted(fs) if f.endswith((".c", ".h"))]
    return out


def byte_vars(code):
    """{name: element type} of the byte/void pointer variables declared in code (`x[k]` of a `s8 *` / `char *`
    reads a SIGNED byte - the index form must keep the type)."""
    names = {}
    for m in DECL.finditer(code):
        for n in re.findall(r"\*\s*(\w+)", m.group(2)):
            names[n] = " ".join(m.group(1).split())
    return names


def functions(code):
    """[(start, end)] of top-level `{ ... }` bodies with their header line (crude, enough for C here)."""
    spans, depth, start = [], 0, None
    for i, c in enumerate(code):
        if c == "{":
            if depth == 0:                    # the body plus its header (from the previous `;` / `}`)
                start = max(code.rfind(";", 0, i), code.rfind("}", 0, i)) + 1
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                spans.append((start, i + 1))
    return spans


def to_pfield(code, journal=None):
    """Rewrite one file's code. Comments and strings are kept as they are. journal (a list) gets
    (macro text, original text) per rewrite, in file order, for `back`."""
    filevars = byte_vars("\n".join(l for l in code.split("\n") if l.startswith(("extern", "static"))))
    out, pos, n = [], 0, 0
    for s, e in functions(code):
        out.append(code[pos:s])
        body = code[s:e]
        bvars = {**filevars, **byte_vars(SKIP.sub("", body))}
        body, k = rewrite_body(body, bvars, journal)
        out.append(body)
        n += k
        pos = e
    out.append(code[pos:])
    return "".join(out), n


def rewrite_body(body, bvars, journal=None):
    n = 0
    found = []

    def pf(x, t, off, orig=None):
        nonlocal n
        n += 1
        mac = f"OFFSET_DEREF({x}, {t.strip()}, {off})"
        found.append((mac, orig))
        return mac
    pieces, pos = [], 0
    for m in SKIP.finditer(body):
        pieces.append(("c", body[pos:m.start()]))
        pieces.append(("s", m.group(0)))
        pos = m.end()
    pieces.append(("c", body[pos:]))
    res = []
    for kind, text in pieces:
        if kind == "c":
            # *(T *)((u8 *)X + off)
            text = re.sub(rf"\*\s*\(\s*(?P<t>{TYPE})\s*\)\s*\(\s*\(\s*{BYTE}\s*\*\s*\)\s*(?P<x>{OPERAND})\s*\+\s*(?P<off>{NUM})\s*\)",
                          lambda m: pf(m.group("x"), m.group("t"), m.group("off"), m.group(0)), text)
            # *(T *)(bytes + off), bytes a byte/void pointer variable
            text = re.sub(rf"\*\s*\(\s*(?P<t>{TYPE})\s*\)\s*\(\s*(?P<x>\w+)\s*\+\s*(?P<off>{NUM})\s*\)",
                          lambda m: pf(m.group("x"), m.group("t"), m.group("off"), m.group(0)) if m.group("x") in bvars else m.group(0), text)
            # ((T *)X)[k], T a byte / 16-bit / 32-bit integer: the bytes at k * sizeof(T)
            def elem(m):
                t = " ".join(m.group("t").split())
                size = ELEM.get(t.split()[-1])
                return pf(m.group("x"), t + " *", hex(int(m.group("off"), 0) * size), m.group(0)) if size else m.group(0)
            text = re.sub(rf"\(\s*\(\s*(?P<t>(?:(?:unsigned|signed)\s+)?\w+)\s*\*\s*\)\s*(?P<x>{OPERAND})\s*\)\s*\[\s*(?P<off>{NUM})\s*\]",
                          elem, text)
            # bytes[k], bytes a byte pointer variable (`obj[1] = 3` is a field too)
            # (not a declaration `char s[6];`: there a type name stands right before the name)
            def index(m, text=text):
                prev = re.search(r"(\w+)\s*$", text[:m.start()])
                if m.group("x") not in bvars or (prev and prev.group(1) not in ("return", "case", "sizeof", "else", "do")):
                    return m.group(0)
                if bvars[m.group("x")] == "void":
                    return m.group(0)
                return pf(m.group("x"), bvars[m.group("x")] + " *", m.group("off"), m.group(0))
            text = re.sub(rf"(?<![\w.>\]\)])(?P<x>\w+)\s*\[\s*(?P<off>{NUM})\s*\]", index, text)
        res.append(text)
    if journal is not None:
        # (in pass order; `back` pairs equal macro texts first-come first-served - any pairing gives
        # the same code, the build after `back` checks it)
        journal.extend(found)
    return "".join(res), n


def back(code, journal=None):
    """OFFSET_DEREF(X, T *, off) -> the original text recorded by `to` (first unused entry with the same
    macro text; all of them are the same code), else *(T *)((u8 *)X + off)."""
    pending = list(journal or [])

    def raw(m):
        for i, (mac, orig) in enumerate(pending):
            if mac == m.group(0) and orig:
                del pending[i]
                return orig
        x, t, off = m.group(1), m.group(2).strip(), m.group(3)
        return f"(*({t})((u8 *){x} + {off}))"             # parenthesised: `X[1]++` must stay (X[1])++
    return re.subn(rf"OFFSET_DEREF\(({OPERAND}), ({TYPE}), ({NUM})\)", raw, code)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    total = 0
    journals = json.load(open(JOURNAL)) if cmd == "back" and os.path.isfile(JOURNAL) else {}
    for p in files(args):
        code = open(p).read()
        key = p.replace(os.sep, "/")
        if cmd == "to":
            journals[key] = []
            new, n = to_pfield(code, journals[key])
        elif cmd == "back":
            new, n = back(code, journals.get(key))
        elif cmd == "count":
            _, n = to_pfield(code)
            new = code
        else:
            sys.exit(__doc__)
        if n and cmd != "count":
            open(p, "w", newline="\n").write(new)
        total += n
    if cmd == "to":
        json.dump(journals, open(JOURNAL, "w"), indent=0)
    print(f"{cmd}: {total}")


if __name__ == "__main__":
    main()
