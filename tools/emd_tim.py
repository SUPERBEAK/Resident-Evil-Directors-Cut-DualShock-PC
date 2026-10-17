"""emd_tim.py <dir> <EMD names...> (run from the project root): Find the TIM texture(s) inside RE1 EMD model files and save them as PNG (one-off identification of NPC models)."""
import os
import struct
import sys
import zlib


def png(path, w, h, rgb):
    raw = b"".join(b"\x00" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def c15(v):
    return ((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31)


def tims(data):
    for off in range(0, len(data) - 20, 4):
        if struct.unpack_from("<I", data, off)[0] != 0x10:
            continue
        flags = struct.unpack_from("<I", data, off + 4)[0]
        if flags not in (8, 9):
            continue
        clen, _, _, cw, ch = struct.unpack_from("<IHHHH", data, off + 8)
        if not (0 < cw <= 256 and 0 < ch <= 64 and clen == 12 + cw * ch * 2):
            continue
        clut = [c15(v) for v in struct.unpack_from(f"<{cw}H", data, off + 20)]   # first palette
        p = off + 8 + clen
        ilen, _, _, iw, ih = struct.unpack_from("<IHHHH", data, p)
        if ilen != 12 + iw * ih * 2:
            continue
        pix = data[p + 12:p + ilen]
        w = iw * (4 if flags == 8 else 2)
        rgb = []
        for b in pix:
            idx = [b & 15, b >> 4] if flags == 8 else [b]
            for i in idx:
                rgb += clut[i] if i < len(clut) else (0, 0, 0)
        yield off, w, ih, rgb


for name in sys.argv[2:]:
    d = open(os.path.join(sys.argv[1], name + ".EMD"), "rb").read()
    for k, (off, w, h, rgb) in enumerate(tims(d)):
        out = os.path.join("temp", "emd_png", f"{name}_{k}.png")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        png(out, w, h, rgb)
        print(name, hex(off), w, h, out)
