"""rectable.py <in.c> <out.c>: tables of fixed-size records. m2c writes every field of
`D_X[i].field` as its own symbol and byte arithmetic:
    *(u8 *)((u8 *)&D_8012C324 + ((i * 0xA)))     *(s16 *)((u8 *)&D_8012C326 + ((i * 0xA)))
Group the M2C_UNK symbols that are used with the same record size and lie within one record of
the lowest one, declare `extern struct { u8 f0; u8 pad1; s16 f2; ... } D_BASE[];` and rewrite
each access as `D_BASE[i].fNN` (stage7 func_801198C4: 10-byte records). The index expression
must be the same text in every access of a group. For trybatch."""
import collections
import re
import sys

s = open(sys.argv[1]).read()
SIZES = {"u8": 1, "s8": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4}


def close(text, i):
    depth = 0
    for k in range(i, len(text)):
        depth += {"(": 1, ")": -1}.get(text[k], 0)
        if depth == 0:
            return k + 1
    return -1



def scan_and_apply():
    global s
    acc = []          # (start, end, type, sym, index, size)
    for m in re.finditer(r"\*\((u8|s8|u16|s16|u32|s32) \*\)\(\(u8 \*\)&(D_[0-9A-F]{8}) \+ \(", s):
        i = m.end() - 1
        j = close(s, i)
        if j < 0 or s[j] != ")":
            continue
        inner = s[i + 1:j - 1].strip()
        while inner.startswith("(") and close(inner, 0) == len(inner):
            inner = inner[1:-1].strip()
        mm = re.fullmatch(r"\(?(.+?)\)? \* (0x[0-9A-F]+|\d+)", inner)
        ms = re.fullmatch(r"\(?(.+?)\)? << (\d+)", inner)            # power-of-2 records (ending func_800E527C)
        if mm:
            acc.append((m.start(), j + 1, m.group(1), m.group(2), mm.group(1).strip(), int(mm.group(2), 0)))
        elif ms:
            acc.append((m.start(), j + 1, m.group(1), m.group(2), ms.group(1).strip(), 1 << int(ms.group(2))))

    unk = set(re.findall(r"extern M2C_UNK (D_[0-9A-F]{8});", s))
    by_size = collections.defaultdict(list)
    for a in acc:
        if a[3] in unk:
            by_size[(a[5], a[4])].append(a)

    for (size, idx), lst in by_size.items():
        syms = sorted({a[3] for a in lst}, key=lambda x: int(x[2:], 16))
        base = int(syms[0][2:], 16)
        if int(syms[-1][2:], 16) - base >= size:
            continue
        if len(syms) == 1 and len({a[2] for a in lst}) == 1 and SIZES[lst[0][2]] == size:
            continue                    # a plain array, not records: m2c's form already compiles right
        fields = {}
        ok = True
        for a in lst:
            off = int(a[3][2:], 16) - base
            if fields.get(off, a[2]) != a[2] or off % SIZES[a[2]]:
                ok = False
            fields[off] = a[2]
        if not ok:
            continue
        # every symbol of the group must be used only through these accesses
        if any(len(re.findall(rf"\b{x}\b", s)) != 1 + sum(1 for a in lst if a[3] == x) for x in syms):
            continue
        members, pos = [], 0
        for off in sorted(fields):
            if off < pos:
                ok = False
                break
            if off > pos:
                members.append(f"u8 pad{pos}[{off - pos}];")
            members.append(f"{fields[off]} f{off:X};")
            pos = off + SIZES[fields[off]]
        if not ok:
            continue
        if pos < size:
            members.append(f"u8 pad{pos}[{size - pos}];")
        name = syms[0]
        for a in sorted(lst, reverse=True):
            off = int(a[3][2:], 16) - base
            s = s[:a[0]] + f"{name}[{idx}].f{off:X}" + s[a[1]:]
        for x in syms:
            s = re.sub(rf"extern M2C_UNK {x};\n", "", s, count=1)
        s = f"extern struct {{ {' '.join(members)} }} {name}[];\n" + s
        return True                     # positions are stale now: rescan
    return False


while scan_and_apply():
    pass
open(sys.argv[2], "w", newline="\n").write(s)
