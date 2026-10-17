"""port_inventory.py - what in Capcom's code ties it to PS1 hardware?

Run from dualshock/ with the project venv python:  python tools/port_inventory.py [--list]

Scans the splat assembly of all Capcom code (main executable `capcom` + the 12 overlays)
and reports, per target:
  * GTE (geometry coprocessor, cop2) instructions inside Capcom code,
  * scratchpad (0x1F800000-0x1F8003FF) and hardware register (0x1F801000+) accesses,
  * calls into Sony library functions (its --list made include/psyq_protos.h),
  * the range of fixed RAM addresses used outside the executable images.
"""

import argparse
import collections
import os
import re

INS = re.compile(r"/\* [0-9A-F]+ ([0-9A-F]{8}) [0-9A-F]{8} \*/\s+(\S+)\s*(.*)")
GTE_OPS = {"mtc2", "mfc2", "ctc2", "cfc2", "lwc2", "swc2", "rtps", "rtpt", "nclip", "op", "dpcs",
           "intpl", "mvmva", "ncds", "cdp", "ncdt", "nccs", "cc", "ncs", "nct", "sqr", "dcpl",
           "dpct", "avsz3", "avsz4", "ncct", "gpf", "gpl", "cop2"}


def psyq_names():
    names = set()
    for line in open(os.path.join("config", "symbol_addrs_psyq.txt")):
        m = re.match(r"(\w+) = 0x", line)
        if m:
            names.add(m.group(1))
    return names


def scan(paths, sony):
    r = {"funcs": 0, "insns": 0, "gte": collections.Counter(), "gte_funcs": set(),
         "scratch": set(), "hwreg": collections.Counter(), "sony_calls": collections.Counter(),
         "fixed": [], "fixed_funcs": set()}
    for p in paths:
        func = None
        for line in open(p):
            g = re.match(r"glabel (\w+)", line.strip())
            if g:
                func = g.group(1)
                r["funcs"] += 1
                continue
            m = INS.search(line)
            if not m:
                continue
            r["insns"] += 1
            op, args = m.group(2), m.group(3)
            if op in GTE_OPS:
                r["gte"][op] += 1
                r["gte_funcs"].add(func)
            for v in re.findall(r"0x(1F80[0-9A-F]{4})", args):
                a = int(v, 16)
                if a < 0x1F800400:
                    r["scratch"].add(func)
                elif a >= 0x1F801000:
                    r["hwreg"][hex(a)] += 1
            for v in re.findall(r"\(?0x(80[0-9A-F]{6})\b", args):
                a = int(v, 16)
                if a >= 0x800B2360:          # beyond the main executable's data
                    r["fixed"].append(a)
                    r["fixed_funcs"].add(func)
            if op in ("jal", "j"):
                tgt = args.strip()
                if tgt in sony:
                    r["sony_calls"][tgt] += 1
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="list Sony functions called")
    args = ap.parse_args()
    sony = psyq_names()
    targets = {"main (capcom)": [os.path.join(dp, f) for dp, _, fs in os.walk("asm/main/nonmatchings")
                                 for f in fs if f.endswith(".s")]}
    for o in ["logo", "prologue", "title", "select", "ending"] + [f"stage{i}" for i in range(1, 8)]:
        targets[o] = [os.path.join("asm", o, f"{o}.s")]

    all_sony = collections.Counter()
    print(f"{'target':14s} {'funcs':>5s} {'insns':>6s} {'GTE ins':>7s} {'GTE fn':>6s} "
          f"{'scratch fn':>10s} {'HW regs':>7s} {'Sony fn':>7s}  fixed RAM range")
    for name, paths in targets.items():
        r = scan(paths, sony)
        all_sony.update(r["sony_calls"])
        fr = f"0x{min(r['fixed']):08X}-0x{max(r['fixed']):08X} ({len(r['fixed_funcs'])} fn)" if r["fixed"] else "-"
        print(f"{name:14s} {r['funcs']:5d} {r['insns']:6d} {sum(r['gte'].values()):7d} "
              f"{len(r['gte_funcs']):6d} {len(r['scratch']):10d} {sum(r['hwreg'].values()):7d} "
              f"{len(r['sony_calls']):7d}  {fr}")
        if r["hwreg"]:
            print(f"{'':14s} HW registers: {dict(r['hwreg'])}")
    print(f"\nDistinct Sony library functions called from Capcom code: {len(all_sony)}")
    if args.list:
        for n, c in sorted(all_sony.items(), key=lambda kv: (-kv[1], kv[0])):
            print(f"  {c:4d}  {n}")


if __name__ == "__main__":
    main()
