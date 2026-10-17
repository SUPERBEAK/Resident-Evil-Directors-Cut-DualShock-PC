"""rdt_msg.py <ROOM files...> (run from the project root): print the text messages (RDT section 11) of RE1 rooms.
Section pointers start at RDT +0x48 (section 6 = init script at +0x60); section 11 = messages at +0x74.
The byte -> character table is parsed from the RDT format page of the Just Solve the File Format Problem wiki (see PAGE below);
bytes without a character there are shown as {XX} (control codes: line break, end, colour, ...)."""
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
lines = [l.strip() for l in text.split("\n")]
start = next(i for i, l in enumerate(lines) if l.startswith("Conversion table"))
cells = [l for l in lines[start:start + 400] if l != ""]
CHARS = {}
k = 0
while k < len(cells):
    m = re.fullmatch(r"0x([0-9a-fA-F])0", cells[k])
    if m and k + 16 < len(cells):
        row = int(m.group(1), 16) * 16
        for c in range(16):
            v = cells[k + 1 + c]
            if v not in ("?",):
                CHARS[row + c] = " " if v.lower() in ("space", "spc", "\xa0") else v
        k += 17
    else:
        k += 1


def decode(b):
    out = []
    for x in b:
        out.append(CHARS.get(x, f"{{{x:02X}}}"))
    return "".join(out)


for f in sys.argv[1:]:
    d = open(f, "rb").read()
    base = struct.unpack_from("<I", d, 0x74)[0]
    if base == 0 or base >= len(d):
        print(f, "- no messages")
        continue
    first = struct.unpack_from("<H", d, base)[0]
    offs = [struct.unpack_from("<H", d, base + 2 * i)[0] for i in range(first // 2)]
    print(f"== {f}: {len(offs)} messages")
    for i, o in enumerate(offs):
        end = offs[i + 1] if i + 1 < len(offs) else min(len(d), base + o + 400)
        print(f"  [{i}] {decode(d[base + o:base + end])}")
