"""rdt_em_set.py (run from the project root): Scan every RE1 room file (RDT) for EM_SET (entity spawn, opcode 0x1B) records in the init script.
Opcode lengths come from the RDT format page of the Just Solve the File Format Problem wiki (see PAGE below). One-off investigation:
which room spawns a hunter (model 6) with behaviour 0/1 next to an NPC (Rebecca)?"""
import glob
import html
import os
import re
import struct
import sys

# The RDT format description: the "RDT (Resident Evil 1997)" page of the Just Solve the File Format Problem
# wiki (fileformat.archiveteam.org), saved as HTML. Set RDT_FORMAT_PAGE to it, default reference/rdt_format.htm.
PAGE = os.environ.get("RDT_FORMAT_PAGE", os.path.join("reference", "rdt_format.htm"))
if not os.path.exists(PAGE):
    sys.exit(f"RDT format page not found: {PAGE} (save the wiki page as HTML there or set RDT_FORMAT_PAGE)")
text = html.unescape(re.sub(r"<[^>]*>", "", open(PAGE, encoding="utf-8", errors="replace").read()))
lines = [l.strip() for l in text.split("\n") if l.strip()]
LEN = {}
for i, l in enumerate(lines[:-2]):
    if re.fullmatch(r"0x[0-9a-fA-F]{2}", l) and re.fullmatch(r"\d+", lines[i + 2]):
        LEN.setdefault(int(l, 16), int(lines[i + 2]))
print("opcode lengths known:", len(LEN), " ".join(f"{k:02X}:{v}" for k, v in sorted(LEN.items())), file=sys.stderr)


def walk(data, start):
    """Yield (offset, opcode) for the init script at `start` (u16 length, then bytecode)."""
    n = struct.unpack_from("<H", data, start)[0]
    p, end = start + 2, start + n
    while p < end:
        op = data[p]
        if op not in LEN or LEN[op] == 0:
            yield p, None
            return
        yield p, op
        p += LEN[op]


for f in sorted(glob.glob("extracted/RE1-Dualshock/PSXUSA/STAGE*/ROOM*.RDT")):
    d = open(f, "rb").read()
    init = struct.unpack_from("<I", d, 0x60)[0]
    spawns, bad = [], None
    for p, op in walk(d, init):
        if op is None:
            bad = f"unknown opcode {d[p]:02X} at +{p - init:X}"
            break
        if op == 0x1B:
            r = d[p:p + 22]
            spawns.append(f"model {r[1]:02X} beh {r[2]:02X} kill {r[3]:02X} slot {r[0x12]:02X}")
    if any(s.startswith("model 06") for s in spawns) or "-v" in sys.argv:
        print(os.path.basename(f), "|", "; ".join(spawns), "|", bad or "")
