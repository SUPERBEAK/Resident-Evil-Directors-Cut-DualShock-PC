"""psyq_match.py - find Sony PsyQ library code in a PS-X EXE using ghidra_psx_ldr signatures.

Usage (project venv python):
    python tools/psyq_match.py <exe>
        [--versions 330,370,...] [--min-bytes 12] [--report out.md] [--symbols out.txt]

Method:
  1. Every object (.OBJ inside a .LIB) of every PsyQ version is searched in the executable's
     text, 4-byte aligned. A signature's '??' bytes are relocated values (wildcards).
  2. A candidate is only kept if it matches exactly ONE place in the executable, has at
     least --min-bytes fixed (non-wildcard) bytes (tiny generic functions match everywhere)
     and starts at a plausible function boundary (previous word 0, or a jr/j right before
     the delay slot) - this rejects matches on the tail end of an unrelated function.
  3. Candidates are assigned greedily, longest (most fixed bytes) first; a candidate whose
     byte range overlaps an already assigned one is dropped.
  4. Identical matches from several versions (same LIB/OBJ at the same address) are merged:
     each assigned object records ALL versions whose signature matches there.
  5. Per library the versions consistent with all of its objects are reported (intersection);
     if empty the library is mixed and the most common version is shown.
With --symbols the global labels of all assigned objects are written in splat
symbol_addrs format.
"""

import argparse
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

SIG_ROOT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "external", "ghidra_settings", "ghidra",
    "ghidra_12.1.3_PUBLIC", "Extensions", "ghidra_psx_ldr", "data", "psyq")


def load_exe(path):
    data = open(path, "rb").read()
    if data[:8] != b"PS-X EXE":
        sys.exit(f"{path}: not a PS-X EXE")
    t_addr, t_size = struct.unpack_from("<II", data, 0x18)
    return t_addr, data[0x800:0x800 + t_size]


def sig_regex(toks):
    return re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)]))
                               for t in toks), re.DOTALL)


def is_global(label):
    return not re.match(r"^(loc|text|data|bss|rdata|sdata|sbss|jpt|jtbl)_[0-9A-Fa-f]+$", label)


def at_function_boundary(text, off):
    """True if a function can start at `off`: start of text, previous word is 0 (nop or
    padding), or the word before the delay slot is a jump (jr <any reg> / j)."""
    if off < 8:
        return True
    prev, prev2 = struct.unpack_from("<II", text, off - 4)[0], struct.unpack_from("<I", text, off - 8)[0]
    is_jr = (prev2 & 0xFC1FFFFF) == 0x00000008
    is_j = (prev2 >> 26) == 2
    return prev == 0 or is_jr or is_j


def vkey(v):
    return int(v.ljust(4, "0"))


def vname(v):  # '3610' -> '3.6.10', '400' -> '4.0'
    return {"3610": "3.6.10", "3611": "3.6.11"}.get(v, f"{v[0]}.{v[1]}" + (f".{v[2:]}" if v[2:] != "0" else ""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("--versions", help="comma list, default: all available")
    ap.add_argument("--min-bytes", type=int, default=12)
    ap.add_argument("--report")
    ap.add_argument("--symbols")
    args = ap.parse_args()

    versions = args.versions.split(",") if args.versions else sorted(
        (d for d in os.listdir(SIG_ROOT) if d.isdigit()), key=vkey)
    base, text = load_exe(args.exe)

    # 1+2: collect unique, big-enough candidates; key = (lib, obj, offset, sig length)
    cands = {}            # key -> {"fixed": n, "labels": [...], "versions": set()}
    rejected = Counter()  # reason -> count
    for ver in versions:
        vdir = os.path.join(SIG_ROOT, ver)
        for fn in sorted(os.listdir(vdir)):
            if not fn.endswith(".json"):
                continue
            lib = fn[:-5]
            for obj in json.load(open(os.path.join(vdir, fn))):
                if "sig" not in obj:          # data-only object (xbss list)
                    continue
                toks = obj["sig"].split()
                fixed = sum(t != "??" for t in toks)
                offs = [m.start() for m in sig_regex(toks).finditer(text) if m.start() % 4 == 0]
                if not offs:
                    continue
                if len(offs) > 1:
                    rejected["matches more than once"] += 1
                    continue
                if fixed < args.min_bytes:
                    rejected[f"fewer than {args.min_bytes} fixed bytes"] += 1
                    continue
                if not at_function_boundary(text, offs[0]):
                    rejected["not at a function boundary"] += 1
                    continue
                key = (lib, obj["name"], offs[0], len(toks))
                c = cands.setdefault(key, {"fixed": fixed, "labels": obj["labels"], "versions": set()})
                c["versions"].add(ver)

    # 3: greedy, longest first, no overlaps
    taken = []  # (start, end)
    assigned = []
    for key, c in sorted(cands.items(), key=lambda kv: (-kv[1]["fixed"], kv[0][2])):
        lib, obj, off, length = key
        if any(off < e and s < off + length for s, e in taken):
            continue
        taken.append((off, off + length))
        assigned.append((lib, obj, off, length, c))

    # 5: per-library version consistency
    bylib = defaultdict(list)
    for a in assigned:
        bylib[a[0]].append(a)
    out = [f"# PsyQ signature match: {os.path.basename(args.exe)}", "",
           f"Text base 0x{base:08X}, size 0x{len(text):X}. Versions scanned: "
           f"{', '.join(vname(v) for v in versions)}. Min fixed bytes: {args.min_bytes}.", "",
           "| Library | Objects | Bytes | Consistent versions (all objects) | Most common versions |",
           "|---|---|---|---|---|"]
    total = 0
    for lib in sorted(bylib):
        objs = bylib[lib]
        size = sum(a[3] for a in objs)
        total += size
        inter = set.intersection(*(a[4]["versions"] for a in objs))
        cnt = Counter(v for a in objs for v in a[4]["versions"])
        top = max(cnt.values())
        common = [v for v in versions if cnt.get(v) == top]
        out.append(f"| {lib} | {len(objs)} | {size:,} | "
                   f"{', '.join(vname(v) for v in versions if v in inter) or '(mixed)'} | "
                   f"{', '.join(vname(v) for v in common)} ({top}/{len(objs)}) |")
    out += ["", f"Total identified Sony code: {total:,} bytes "
                f"({100 * total / len(text):.1f}% of text).", "",
            "Rejected candidates: " + ", ".join(f"{n} {r}" for r, n in rejected.items()), "",
            "## Objects", "", "| Address | Size | Library | Object | Matching versions |", "|---|---|---|---|---|"]
    for lib, obj, off, length, c in sorted(assigned, key=lambda a: a[2]):
        out.append(f"| 0x{base + off:08X} | {length} | {lib} | {obj} | "
                   f"{', '.join(vname(v) for v in sorted(c['versions'], key=vkey))} |")
    report = "\n".join(out)
    print("\n".join(out[:out.index('## Objects')]))
    if args.report:
        open(args.report, "w", encoding="utf-8").write(report + "\n")

    if args.symbols:
        syms = {}
        for lib, obj, off, length, c in assigned:
            for lab in c["labels"]:
                if is_global(lab["name"]):
                    syms.setdefault(base + off + lab["offset"], (lab["name"], lib, obj))
        # Sony libs reuse names (BIOS stub vs wrapper, file-local helpers like memclr in
        # several objects); splat needs unique names, so every duplicate gets _<OBJ>.
        counts = Counter(n for n, _, _ in syms.values())
        with open(args.symbols, "w", encoding="utf-8") as f:
            for addr in sorted(syms):
                name, lib, obj = syms[addr]
                if counts[name] > 1:
                    name = f"{name}_{obj.rsplit('.', 1)[0]}"
                # No ':' in the provenance note: splat parses key:value pairs as attributes.
                f.write(f"{name} = 0x{addr:08X}; // type:func psyq {lib}/{obj}\n")
        print(f"Wrote {len(syms)} symbols to {args.symbols}")


if __name__ == "__main__":
    main()
