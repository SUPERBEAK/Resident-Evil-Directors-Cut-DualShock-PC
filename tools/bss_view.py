"""bss_view.py (run from the project root): decode the camera backgrounds of RE1 rooms into PNGs + one contact
sheet per room.

    python bss_view.py STAGE2/ROOM207 STAGE2/ROOM20E STAGE1/ROOM110 ...

A ROOMsrr.BSS file holds one 320x240 MDEC picture (PS1 "bs" v3 frame) per camera cut, each in its own 0x8000
bytes (load_room_bg_image: lba + cut * 16 sectors). Stages 6 / 7 (and Advanced 12 / 13) use stage 1 / 2's
BSS files. Decoding with jPSXdec (tools/external/jpsxdec, needs Java).
Output: temp/rooms/<ROOM>/cutN.png and temp/rooms/<ROOM>.png (all cuts, labelled).
"""
import glob
import os
import subprocess
import sys

from PIL import Image, ImageDraw

JAR = os.path.abspath("tools/external/jpsxdec/jpsxdec.jar")
SRC = "extracted/RE1-Dualshock/PSXUSA"
OUT = os.path.abspath("temp/rooms")

for room in sys.argv[1:]:
    name = os.path.basename(room)
    data = open(os.path.join(SRC, room + ".BSS"), "rb").read()
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    pngs = []
    for cut in range(len(data) // 0x8000):
        chunk = data[cut * 0x8000:(cut + 1) * 0x8000]
        if chunk[2:4] != b"\x00\x38":
            continue
        bs = os.path.join(d, f"cut{cut}.bs")
        open(bs, "wb").write(chunk)
        # (jPSXdec writes a static frame into the current directory, ignoring -dir)
        subprocess.run(["java", "-jar", JAR, "-f", bs, "-static", "bs", "-dim", "320x240", "-fmt", "png"],
                       capture_output=True, text=True, cwd=d)
        out = sorted(glob.glob(os.path.join(d, f"cut{cut}*.png")))
        if out:
            pngs.append((cut, out[0]))
    cols = 3
    rows = (len(pngs) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 330, rows * 260), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (cut, p) in enumerate(pngs):
        x, y = (i % cols) * 330 + 5, (i // cols) * 260 + 18
        sheet.paste(Image.open(p).convert("RGB"), (x, y))
        draw.text((x, y - 15), f"{name} cut {cut}", fill=(255, 255, 255))
    sheet.save(os.path.join(OUT, name + ".png"))
    print(name, len(pngs), "cuts ->", os.path.join(OUT, name + ".png"))
