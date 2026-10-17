"""rdt_zones.py [types...] [-v] (run from the project root): list the rooms whose init script creates room-action
zones of the given g_RoomCheckActions types (default 5 6). Known types: 1 door, 2 message, 5 / 6 staircase
(room_action_stairs_zone_x / _z), 12 ladder (set_ladder_zone), 17 slope (slope_height_update).
SCD 0x0D room_action_set record (18 bytes): +1 slot, +2 x, +4 z, +6 w, +8 d, +0xA type, +0xB flags,
+0xC arg0, +0xE arg1, +0x10 arg2. Opcode lengths from the RDT format page (like rdt_em_set.py)."""
import glob
import html
import os
import re
import struct
import sys

TYPES = {int(a, 0) for a in sys.argv[1:] if not a.startswith("-")} or {5, 6}

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


def walk(data, start):
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
    hits, bad = [], None
    for p, op in walk(d, init):
        if op is None:
            bad = f"(stopped at unknown opcode {d[p]:02X})"
            break
        if op == 0x0D and d[p + 0x0A] in TYPES:
            x, z, w, dd = struct.unpack_from("<hhhh", d, p + 2)
            a0, a1, a2 = struct.unpack_from("<HHh", d, p + 0x0C)
            hits.append(f"type {d[p + 0xA]} flags {d[p + 0xB]:02X} zone ({x},{z}) {w}x{dd} args {a0:X} {a1:X} {a2}")
    if hits or ("-v" in sys.argv and bad):
        print(os.path.basename(f), "|", "; ".join(hits), bad or "")
